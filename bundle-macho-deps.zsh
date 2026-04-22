#!/bin/zsh
set -euo pipefail

if [[ "$#" -lt 3 ]]; then
  echo "Usage:"
  echo "  $0 <bundle-root> <frameworks-dir> <direct-lib> [more direct libs...]"
  exit 1
fi

BUNDLE_ROOT="$1"
FRAMEWORKS_DIR="$2"
shift 2
DIRECT_DEPS=("$@")

mkdir -p "$FRAMEWORKS_DIR"

typeset -A SEEN
typeset -A COPIED_FROM

is_macho() {
  local f="$1"
  file "$f" | grep -Eq 'Mach-O'
}

is_system_lib() {
  local dep="$1"
  [[ "$dep" == /System/* || "$dep" == /usr/lib/* ]]
}

is_brew_lib() {
  local dep="$1"
  [[ "$dep" == /opt/homebrew/* || "$dep" == /usr/local/* ]]
}

resolve_realpath() {
  python3 - "$1" <<'PY'
import os, sys
print(os.path.realpath(sys.argv[1]))
PY
}

relpath_between() {
  python3 - "$1" "$2" <<'PY'
import os, sys
print(os.path.relpath(sys.argv[2], sys.argv[1]))
PY
}

list_deps() {
  local f="$1"
  otool -L "$f" 2>/dev/null | tail -n +2 | awk '{print $1}'
}

copy_one_lib() {
  local src="$1"
  local real src_base dst

  real="$(resolve_realpath "$src")"
  src_base="$(basename "$src")"
  dst="$FRAMEWORKS_DIR/$src_base"

  if [[ -n "${SEEN[$real]-}" ]]; then
    return 0
  fi
  SEEN[$real]=1

  echo "Copy: $real -> $dst"
  cp -f "$real" "$dst"
  chmod u+w "$dst" || true

  COPIED_FROM[$src_base]="$real"

  recurse_copy_brew_deps "$real"
}

recurse_copy_brew_deps() {
  local f="$1"
  local dep
  while IFS= read -r dep; do
    [[ -z "$dep" ]] && continue
    is_system_lib "$dep" && continue
    is_brew_lib "$dep" || continue
    copy_one_lib "$dep"
  done < <(list_deps "$f")
}

set_id_to_rpath() {
  local f="$1"
  local base
  base="$(basename "$f")"
  echo "Set install id: $f -> @rpath/$base"
  install_name_tool -id "@rpath/$base" "$f" 2>/dev/null || true
}

patch_refs_to_frameworks() {
  local f="$1"
  local dep dep_base

  while IFS= read -r dep; do
    [[ -z "$dep" ]] && continue
    dep_base="$(basename "$dep")"

    if [[ -f "$FRAMEWORKS_DIR/$dep_base" ]]; then
      echo "Rewrite dep in $f:"
      echo "  from: $dep"
      echo "  to:   @rpath/$dep_base"

      chmod u+w "$f" || true
      install_name_tool -change "$dep" "@rpath/$dep_base" "$f"
    fi
  done < <(list_deps "$f")
}

add_loader_rpath_for_file() {
  local f="$1"
  local file_dir fw_dir rel rpath

  file_dir="$(cd "$(dirname "$f")" && pwd)"
  fw_dir="$(cd "$FRAMEWORKS_DIR" && pwd)"
  rel="$(relpath_between "$file_dir" "$fw_dir")"
  rpath="@loader_path/$rel"

  echo "Add rpath to $f => $rpath"
  install_name_tool -add_rpath "$rpath" "$f" 2>/dev/null || true
}

collect_macho_files() {
  find "$BUNDLE_ROOT" -type f | while IFS= read -r f; do
    if is_macho "$f"; then
      echo "$f"
    fi
  done
}

echo "Seeding direct dependencies..."
for dep in "${DIRECT_DEPS[@]}"; do
  copy_one_lib "$dep"
done

echo "Setting install names on copied libs..."
find "$FRAMEWORKS_DIR" -type f | while IFS= read -r f; do
  if is_macho "$f"; then
    set_id_to_rpath "$f"
  fi
done

echo "Patching all Mach-O files inside bundle root..."
collect_macho_files | while IFS= read -r f; do
  chmod u+w "$f" || true
  patch_refs_to_frameworks "$f"
done

echo "Patching copied libs against each other..."
find "$FRAMEWORKS_DIR" -type f | while IFS= read -r f; do
  if is_macho "$f"; then
    patch_refs_to_frameworks "$f"
  fi
done

echo "Adding per-file relative rpaths..."
collect_macho_files | while IFS= read -r f; do
  add_loader_rpath_for_file "$f"
done

find "$FRAMEWORKS_DIR" -type f | while IFS= read -r f; do
  if is_macho "$f"; then
    add_loader_rpath_for_file "$f"
  fi
done

echo
echo "Done. Final copied libs:"
find "$FRAMEWORKS_DIR" -maxdepth 1 -type f -print | sort