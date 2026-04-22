#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

static NSString * const WineLauncherExecutablePathKey = @"LauncherExecutablePath";

typedef NS_ENUM(NSInteger, WineLauncherOpenMode) {
    WineLauncherOpenModeDefault = 0,
    WineLauncherOpenModeNewInstance,
    WineLauncherOpenModeChangeExecutable,
    WineLauncherOpenModeCancel,
};

@interface WineBundleLauncher : NSObject <NSApplicationDelegate, NSMenuItemValidation>
@property (nonatomic, copy) NSString *runtimeName;
@property (nonatomic, retain) NSURL *runtimeURL;
@property (nonatomic, retain) NSURL *wineExecutableURL;
@property (nonatomic, retain) NSURL *icdURL;
@property (nonatomic, retain) NSTask *wineTask;
@property (nonatomic, retain) NSMenuItem *launchItem;
@property (nonatomic, retain) NSMenuItem *chooseItem;
@property (nonatomic, retain) NSMenuItem *revealItem;
@property (nonatomic, retain) NSMenuItem *alternateOpenItem;
@property (nonatomic, retain) NSMenu *dockMenu;
@end

@implementation WineBundleLauncher

- (BOOL)isRuntimeAvailableSilently
{
    BOOL isDirectory = NO;
    NSFileManager *fileManager = [NSFileManager defaultManager];

    if (![fileManager fileExistsAtPath:self.runtimeURL.path isDirectory:&isDirectory] || !isDirectory) return NO;
    if (![fileManager isExecutableFileAtPath:self.wineExecutableURL.path]) return NO;
    return YES;
}

- (instancetype)init
{
    self = [super init];
    if (!self) return nil;

    NSBundle *bundle = [NSBundle mainBundle];
    NSString *runtimeName = [bundle objectForInfoDictionaryKey:@"WineRuntimeName"];
    NSString *resourcePath = [bundle resourcePath];

    if (![runtimeName isKindOfClass:[NSString class]] || !runtimeName.length || !resourcePath.length)
        return self;

    self.runtimeName = runtimeName;
    self.runtimeURL = [NSURL fileURLWithPath:[resourcePath stringByAppendingPathComponent:runtimeName] isDirectory:YES];
    self.wineExecutableURL = [self.runtimeURL URLByAppendingPathComponent:@"bin/wine"];
    self.icdURL = [self.runtimeURL URLByAppendingPathComponent:@"share/vulkan/icd.d/MoltenVK_icd.json"];
    return self;
}

- (void)dealloc
{
    [_runtimeName release];
    [_runtimeURL release];
    [_wineExecutableURL release];
    [_icdURL release];
    [_wineTask release];
    [_launchItem release];
    [_chooseItem release];
    [_revealItem release];
    [_alternateOpenItem release];
    [_dockMenu release];
    [super dealloc];
}

- (NSString *)appName
{
    NSString *name = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleName"];
    return name.length ? name : @"Wine";
}

- (NSString *)configuredExecutablePath
{
    NSString *path = [[NSUserDefaults standardUserDefaults] stringForKey:WineLauncherExecutablePathKey];
    return path.length ? path : nil;
}

- (NSURL *)configuredExecutableURLIfValid
{
    NSString *path = [self configuredExecutablePath];
    BOOL isDirectory = NO;

    if (!path.length) return nil;
    if (![[NSFileManager defaultManager] fileExistsAtPath:path isDirectory:&isDirectory] || isDirectory) return nil;

    return [NSURL fileURLWithPath:path];
}

- (BOOL)hasRunningWineTask
{
    return self.wineTask && self.wineTask.isRunning;
}

- (void)showAlertWithMessage:(NSString *)message informativeText:(NSString *)informativeText
{
    NSAlert *alert = [[[NSAlert alloc] init] autorelease];
    alert.messageText = message;
    alert.informativeText = informativeText ?: @"";
    [alert addButtonWithTitle:@"OK"];
    [alert runModal];
}

- (void)installMenu
{
    NSMenu *mainMenu = [[[NSMenu alloc] initWithTitle:@"Main Menu"] autorelease];
    NSMenu *appMenu = [[[NSMenu alloc] initWithTitle:[self appName]] autorelease];
    NSMenuItem *appMenuItem = [[[NSMenuItem alloc] initWithTitle:[self appName] action:nil keyEquivalent:@""] autorelease];
    NSString *appName = [self appName];

    self.chooseItem = [[[NSMenuItem alloc] initWithTitle:@"Choose Executable…" action:@selector(chooseExecutable:) keyEquivalent:@"o"] autorelease];
    [self.chooseItem setTarget:self];
    [appMenu addItem:self.chooseItem];

    self.launchItem = [[[NSMenuItem alloc] initWithTitle:@"Launch Configured Executable" action:@selector(launchConfiguredExecutable:) keyEquivalent:@"r"] autorelease];
    [self.launchItem setTarget:self];
    [appMenu addItem:self.launchItem];

    self.revealItem = [[[NSMenuItem alloc] initWithTitle:@"Reveal Configured Executable" action:@selector(revealConfiguredExecutable:) keyEquivalent:@""] autorelease];
    [self.revealItem setTarget:self];
    [appMenu addItem:self.revealItem];

    self.alternateOpenItem = [[[NSMenuItem alloc] initWithTitle:@"Alternative Open Mode…" action:@selector(showAlternativeOpenMode:) keyEquivalent:@""] autorelease];
    [self.alternateOpenItem setTarget:self];
    [appMenu addItem:self.alternateOpenItem];

    [appMenu addItem:[NSMenuItem separatorItem]];

    NSMenuItem *hideItem = [appMenu addItemWithTitle:[NSString stringWithFormat:@"Hide %@", appName]
                                              action:@selector(hide:)
                                       keyEquivalent:@"h"];
    [hideItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand];

    NSMenuItem *hideOthers = [appMenu addItemWithTitle:@"Hide Others"
                                                action:@selector(hideOtherApplications:)
                                         keyEquivalent:@"h"];
    [hideOthers setKeyEquivalentModifierMask:NSEventModifierFlagCommand | NSEventModifierFlagOption];

    [appMenu addItemWithTitle:@"Show All" action:@selector(unhideAllApplications:) keyEquivalent:@""];
    [appMenu addItem:[NSMenuItem separatorItem]];

    NSMenuItem *quitItem = [appMenu addItemWithTitle:[NSString stringWithFormat:@"Quit %@", appName]
                                              action:@selector(terminate:)
                                       keyEquivalent:@"q"];
    [quitItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand];

    [appMenuItem setSubmenu:appMenu];
    [mainMenu addItem:appMenuItem];
    [NSApp setMainMenu:mainMenu];
}

- (void)installDockMenu
{
    NSMenu *dockMenu = [[[NSMenu alloc] initWithTitle:[self appName]] autorelease];
    NSMenuItem *newInstanceItem = [[[NSMenuItem alloc] initWithTitle:@"New Instance"
                                                              action:@selector(newInstanceFromDock:)
                                                       keyEquivalent:@""] autorelease];
    NSMenuItem *changeExecutableItem = [[[NSMenuItem alloc] initWithTitle:@"Change Executable…"
                                                                   action:@selector(changeExecutableFromDock:)
                                                            keyEquivalent:@""] autorelease];
    NSMenuItem *revealItem = [[[NSMenuItem alloc] initWithTitle:@"Reveal Configured Executable"
                                                         action:@selector(revealConfiguredExecutable:)
                                                  keyEquivalent:@""] autorelease];

    [newInstanceItem setTarget:self];
    [changeExecutableItem setTarget:self];
    [revealItem setTarget:self];

    [dockMenu addItem:newInstanceItem];
    [dockMenu addItem:changeExecutableItem];
    [dockMenu addItem:[NSMenuItem separatorItem]];
    [dockMenu addItem:revealItem];

    self.dockMenu = dockMenu;
}

- (void)persistExecutableURL:(NSURL *)url
{
    if (!url) return;
    [[NSUserDefaults standardUserDefaults] setObject:url.path forKey:WineLauncherExecutablePathKey];
    [[NSUserDefaults standardUserDefaults] synchronize];
}

- (BOOL)shouldUseAlternateOpenModeOnStartup
{
    return ([NSEvent modifierFlags] & NSEventModifierFlagOption) != 0;
}

- (WineLauncherOpenMode)promptForAlternativeOpenMode
{
    NSAlert *alert = [[[NSAlert alloc] init] autorelease];
    [alert setMessageText:@"Alternative Open Mode"];
    [alert setInformativeText:@"Choose how this app launch should behave."];
    [alert addButtonWithTitle:@"New Instance"];
    [alert addButtonWithTitle:@"Change Executable"];
    [alert addButtonWithTitle:@"Cancel"];
    [NSApp activateIgnoringOtherApps:YES];

    switch ([alert runModal])
    {
        case NSAlertFirstButtonReturn:
            return WineLauncherOpenModeNewInstance;
        case NSAlertSecondButtonReturn:
            return WineLauncherOpenModeChangeExecutable;
        default:
            return WineLauncherOpenModeCancel;
    }
}

- (NSURL *)promptForExecutableWithReason:(NSString *)reason autoLaunch:(BOOL)autoLaunch
{
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    NSURL *currentURL = [self configuredExecutableURLIfValid];
    NSString *storedPath = [self configuredExecutablePath];

    if (reason.length)
    {
        NSAlert *alert = [[[NSAlert alloc] init] autorelease];
        [alert setMessageText:reason];
        [alert setInformativeText:@"Choose the Windows executable to launch with Wine."];
        [alert addButtonWithTitle:@"Choose Executable…"];
        [alert addButtonWithTitle:@"Cancel"];
        [NSApp activateIgnoringOtherApps:YES];
        if ([alert runModal] != NSAlertFirstButtonReturn) return nil;
    }

    panel.canChooseFiles = YES;
    panel.canChooseDirectories = NO;
    panel.allowsMultipleSelection = NO;
    panel.resolvesAliases = YES;
    panel.message = @"Select a Windows executable";
    panel.prompt = @"Choose";

    if (currentURL)
        panel.directoryURL = [currentURL URLByDeletingLastPathComponent];
    else if (storedPath.length)
        panel.directoryURL = [[NSURL fileURLWithPath:storedPath] URLByDeletingLastPathComponent];

    [NSApp activateIgnoringOtherApps:YES];
    if ([panel runModal] != NSModalResponseOK) return nil;

    NSURL *selectedURL = panel.URL;
    [self persistExecutableURL:selectedURL];
    if (autoLaunch) [self launchExecutableURL:selectedURL];
    return selectedURL;
}

- (BOOL)ensureRuntimeAvailable
{
    if (![self isRuntimeAvailableSilently])
    {
        NSString *message = @"Wine runtime not found";
        NSString *detail = @"The bundled Wine runtime is missing from the application package.";
        if (self.runtimeURL.path.length && [[NSFileManager defaultManager] fileExistsAtPath:self.runtimeURL.path])
        {
            message = @"Bundled Wine executable not found";
            detail = @"The application package does not contain an executable Wine binary.";
        }
        [self showAlertWithMessage:message informativeText:detail];
        return NO;
    }

    return YES;
}

- (void)spawnExecutableURL:(NSURL *)url trackTask:(BOOL)trackTask
{
    if (!url) return;
    if (trackTask && [self hasRunningWineTask]) return;
    if (![self ensureRuntimeAvailable]) return;

    NSMutableDictionary *environment = [[[NSProcessInfo processInfo] environment] mutableCopy];
    NSTask *task = [[[NSTask alloc] init] autorelease];

    if (self.icdURL.path.length)
        environment[@"VK_ICD_FILENAMES"] = self.icdURL.path;
    environment[@"WINE_D3D_CONFIG"] = @"renderer=vulkan";

    task.executableURL = self.wineExecutableURL;
    task.arguments = @[url.path];
    task.currentDirectoryURL = self.runtimeURL;
    task.environment = environment;

    __block WineBundleLauncher *blockSelf = self;
    task.terminationHandler = ^(NSTask *finishedTask) {
        dispatch_async(dispatch_get_main_queue(), ^{
            if (blockSelf.wineTask == finishedTask) blockSelf.wineTask = nil;
        });
    };

    @try
    {
        NSError *error = nil;
        if (![task launchAndReturnError:&error])
        {
            [self showAlertWithMessage:@"Failed to launch Wine target"
                       informativeText:error.localizedDescription ?: @"Unknown error"];
            [environment release];
            return;
        }
        if (trackTask) self.wineTask = task;
        [NSApp hide:nil];
    }
    @catch (NSException *exception)
    {
        [self showAlertWithMessage:@"Failed to launch Wine target"
                   informativeText:exception.reason ?: @"Unknown exception"];
    }

    [environment release];
}

- (void)launchExecutableURL:(NSURL *)url
{
    [self spawnExecutableURL:url trackTask:YES];
}

- (void)launchConfiguredExecutable:(id)sender
{
    NSURL *url = [self configuredExecutableURLIfValid];

    if (!url)
    {
        NSString *reason = [self configuredExecutablePath].length
            ? @"The configured executable could not be found."
            : @"No executable has been configured yet.";
        [self promptForExecutableWithReason:reason autoLaunch:YES];
        return;
    }

    [self launchExecutableURL:url];
}

- (void)chooseExecutable:(id)sender
{
    [self promptForExecutableWithReason:nil autoLaunch:NO];
}

- (void)revealConfiguredExecutable:(id)sender
{
    NSURL *url = [self configuredExecutableURLIfValid];
    if (!url)
    {
        [self showAlertWithMessage:@"No configured executable"
                   informativeText:@"Choose an executable first."];
        return;
    }

    [[NSWorkspace sharedWorkspace] activateFileViewerSelectingURLs:@[url]];
}

- (void)showAlternativeOpenMode:(id)sender
{
    NSURL *configuredURL = [self configuredExecutableURLIfValid];

    switch ([self promptForAlternativeOpenMode])
    {
        case WineLauncherOpenModeNewInstance:
            if (configuredURL) [self launchExecutableURL:configuredURL];
            else
            {
                NSString *reason = [self configuredExecutablePath].length
                    ? @"The configured executable could not be found."
                    : @"No executable has been configured yet.";
                [self promptForExecutableWithReason:reason autoLaunch:YES];
            }
            break;
        case WineLauncherOpenModeChangeExecutable:
            [self promptForExecutableWithReason:nil autoLaunch:YES];
            break;
        case WineLauncherOpenModeCancel:
        case WineLauncherOpenModeDefault:
            break;
    }
}

- (void)newInstanceFromDock:(id)sender
{
    NSURL *configuredURL = [self configuredExecutableURLIfValid];

    if (configuredURL) [self spawnExecutableURL:configuredURL trackTask:NO];
    else
    {
        NSString *reason = [self configuredExecutablePath].length
            ? @"The configured executable could not be found."
            : @"No executable has been configured yet.";
        [self promptForExecutableWithReason:reason autoLaunch:YES];
    }
}

- (void)changeExecutableFromDock:(id)sender
{
    [self promptForExecutableWithReason:nil autoLaunch:YES];
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem
{
    SEL action = menuItem.action;

    if (action == @selector(launchConfiguredExecutable:))
        return ![self hasRunningWineTask] && [self isRuntimeAvailableSilently];
    if (action == @selector(revealConfiguredExecutable:))
        return [self configuredExecutableURLIfValid] != nil;
    if (action == @selector(showAlternativeOpenMode:))
        return YES;
    if (action == @selector(newInstanceFromDock:))
        return [self isRuntimeAvailableSilently];
    if (action == @selector(changeExecutableFromDock:))
        return YES;
    if (action == @selector(chooseExecutable:))
        return YES;
    return YES;
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification
{
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [self installMenu];
    [self installDockMenu];
    [NSApp activateIgnoringOtherApps:YES];

    if ([self shouldUseAlternateOpenModeOnStartup])
    {
        [self showAlternativeOpenMode:nil];
        return;
    }

    NSURL *configuredURL = [self configuredExecutableURLIfValid];
    if (configuredURL)
        [self launchExecutableURL:configuredURL];
    else
    {
        NSString *reason = [self configuredExecutablePath].length
            ? @"The configured executable could not be found."
            : @"No executable has been configured yet.";
        [self promptForExecutableWithReason:reason autoLaunch:YES];
    }
}

- (NSMenu *)applicationDockMenu:(NSApplication *)sender
{
    return self.dockMenu;
}

- (BOOL)applicationShouldHandleReopen:(NSApplication *)sender hasVisibleWindows:(BOOL)flag
{
    [NSApp activateIgnoringOtherApps:YES];
    [self showAlternativeOpenMode:nil];
    return NO;
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
    return NSTerminateNow;
}

@end

int main(int argc, const char * argv[])
{
    @autoreleasepool
    {
        NSApplication *application = [NSApplication sharedApplication];
        WineBundleLauncher *delegate = [[[WineBundleLauncher alloc] init] autorelease];

        [application setDelegate:delegate];
        [application run];
    }

    return 0;
}
