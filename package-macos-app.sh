#!/usr/bin/env bash
set -euo pipefail

if [[ "$#" -lt 2 || "$#" -gt 3 ]]; then
  echo "Usage: $0 <source-dir> <output-dir> [app-name]" >&2
  exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SRC_DIR="$(cd "$1" && pwd)"
OUT_DIR="$2"
APP_NAME="${3:-$(basename "$SRC_DIR")}"
APP_DIR="$OUT_DIR/${APP_NAME}.app"
CONTENTS_DIR="$APP_DIR/Contents"
MACOS_DIR="$CONTENTS_DIR/MacOS"
RESOURCES_DIR="$CONTENTS_DIR/Resources"
RUNTIME_NAME="$(basename "$SRC_DIR")"
RUNTIME_DIR="$RESOURCES_DIR/$RUNTIME_NAME"
ICD_DIR="$RUNTIME_DIR/share/vulkan/icd.d"
PLIST_PATH="$CONTENTS_DIR/Info.plist"
LAUNCHER_PATH="$MACOS_DIR/$APP_NAME"
ICD_PATH="$ICD_DIR/MoltenVK_icd.json"
BUILT_LAUNCHER="$SCRIPT_DIR/loader/wine_bundle_launcher"
ICON_SOURCE="$SCRIPT_DIR/programs/winecfg/logo.ico"
ICONSET_DIR="$OUT_DIR/${APP_NAME}.iconset"
ICON_BASENAME="AppIcon"
ICON_PATH="$RESOURCES_DIR/${ICON_BASENAME}.icns"

mkdir -p "$OUT_DIR"
rm -rf "$APP_DIR"
mkdir -p "$MACOS_DIR" "$RESOURCES_DIR" "$RUNTIME_DIR"

cp -a "$SRC_DIR/." "$RUNTIME_DIR/"
mkdir -p "$ICD_DIR"

if [[ ! -x "$BUILT_LAUNCHER" ]]; then
  echo "Missing built launcher: $BUILT_LAUNCHER" >&2
  echo "Build it first with: make -C \"$SCRIPT_DIR/loader\" wine_bundle_launcher" >&2
  exit 1
fi

cp -a "$BUILT_LAUNCHER" "$LAUNCHER_PATH"
chmod 755 "$LAUNCHER_PATH"

if [[ -f "$ICON_SOURCE" ]]; then
  rm -rf "$ICONSET_DIR"
  mkdir -p "$ICONSET_DIR"

  TMP_PNG="$OUT_DIR/${APP_NAME}.png"
  sips -s format png "$ICON_SOURCE" --out "$TMP_PNG" >/dev/null

  for size in 16 32 128 256 512; do
    sips -z "$size" "$size" "$TMP_PNG" --out "$ICONSET_DIR/icon_${size}x${size}.png" >/dev/null
    retina_size=$((size * 2))
    sips -z "$retina_size" "$retina_size" "$TMP_PNG" --out "$ICONSET_DIR/icon_${size}x${size}@2x.png" >/dev/null
  done

  iconutil -c icns "$ICONSET_DIR" -o "$ICON_PATH"
  rm -rf "$ICONSET_DIR" "$TMP_PNG"
fi

cat > "$PLIST_PATH" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleDevelopmentRegion</key>
  <string>en</string>
  <key>CFBundleExecutable</key>
  <string>$APP_NAME</string>
  <key>CFBundleIdentifier</key>
  <string>org.mcpelauncher.gdk.$(printf '%s' "$APP_NAME" | tr '[:upper:]' '[:lower:]' | tr -cs 'a-z0-9' '-')</string>
  <key>CFBundleInfoDictionaryVersion</key>
  <string>6.0</string>
  <key>CFBundleIconFile</key>
  <string>$ICON_BASENAME</string>
  <key>CFBundleName</key>
  <string>$APP_NAME</string>
  <key>CFBundlePackageType</key>
  <string>APPL</string>
  <key>CFBundleShortVersionString</key>
  <string>1.0</string>
  <key>CFBundleVersion</key>
  <string>1</string>
  <key>LSMinimumSystemVersion</key>
  <string>11.0</string>
  <key>WineRuntimeName</key>
  <string>$RUNTIME_NAME</string>
</dict>
</plist>
EOF

cat > "$ICD_PATH" <<'EOF'
{
    "file_format_version" : "1.0.0",
    "ICD": {
        "library_path": "../../../lib/libMoltenVK.dylib",
        "api_version" : "1.4.0",
        "is_portability_driver" : true
    }
}
EOF

echo "Created app bundle: $APP_DIR"
