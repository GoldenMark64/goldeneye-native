/* macOS app bootstrap. Opens the custom launcher directly; AppKit is error recovery only.
 * This executable contains no game data and never replaces a user's game config. */
#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>

static BOOL validRenderer(id value) {
    return [value isKindOfClass:NSString.class] &&
        ([value isEqual:@"gl"] || [value isEqual:@"metal"]);
}

@interface GERendererSession : NSObject
@property NSURL *directory;
@property NSDictionary<NSString *, NSURL *> *binaries;
@property NSUserDefaults *preferences;
@property NSMutableDictionary<NSString *, NSString *> *environment;
@property NSMutableArray<NSString *> *arguments;
@property NSString *selection;
@property BOOL metalAvailable;
- (instancetype)initWithDirectory:(NSURL *)directory owner:(NSString *)owner arguments:(NSArray *)arguments
                     environment:(NSDictionary *)environment preferences:(NSUserDefaults *)preferences
                  metalAvailable:(BOOL)metalAvailable;
- (NSString *)unavailable:(NSString *)renderer;
- (NSTask *)taskForRenderer:(NSString *)renderer;
- (NSTask *)launchRenderer:(NSString *)renderer handler:(void (^)(NSTask *))handler error:(NSError **)error;
@end

@implementation GERendererSession
- (instancetype)initWithDirectory:(NSURL *)directory owner:(NSString *)owner arguments:(NSArray *)arguments
                     environment:(NSDictionary *)environment preferences:(NSUserDefaults *)preferences
                  metalAvailable:(BOOL)metalAvailable {
    if (!(self = [super init])) return nil;
    _preferences = preferences;
    _environment = [environment mutableCopy];
    _arguments = [NSMutableArray array];
    _metalAvailable = metalAvailable;
    // Both generated apps use the same pair. No PATH search or stale fallback binary.
    NSURL *gl = directory;
    NSURL *metal = directory;
    if ([owner isEqual:@"metal"]) {
        gl = [[directory URLByDeletingLastPathComponent] URLByAppendingPathComponent:@"build-mac"];
    } else {
        metal = [[directory URLByDeletingLastPathComponent] URLByAppendingPathComponent:@"build-mac-metal"];
    }
    _binaries = @{@"gl": [gl URLByAppendingPathComponent:@"goldeneye"],
                  @"metal": [metal URLByAppendingPathComponent:@"goldeneye-metal"]};
    // Use OpenGL's existing adjacent config for both renderers, even if its binary is missing.
    BOOL isDirectory = NO;
    _directory = ([NSFileManager.defaultManager fileExistsAtPath:gl.path isDirectory:&isDirectory] && isDirectory) ? gl : directory;
    id saved = [preferences objectForKey:@"renderer"];
    _selection = !saved ? @"gl" : validRenderer(saved) ? saved : nil;
    for (NSString *arg in arguments) {
        if ([arg hasPrefix:@"--app-renderer="]) {
            NSString *value = [arg substringFromIndex:15];
            _selection = validRenderer(value) ? value : nil;
        } else if (![arg hasPrefix:@"-psn_"]) {
            [_arguments addObject:arg];
        }
    }
    _environment[@"GETV_MAC_APP_CONFIG_DIR"] = _directory.path;
    _environment[@"GETV_MAC_RENDERER_APP"] = @"1";
    _environment[@"GETV_MAC_APP_RENDERER"] = _selection ?: @"invalid";
    _environment[@"GETV_MAC_APP_GL"] = _binaries[@"gl"].path;
    _environment[@"GETV_MAC_APP_METAL"] = _binaries[@"metal"].path;
    _environment[@"GETV_MAC_APP_METAL_AVAILABLE"] = metalAvailable ? @"1" : @"0";
    return self;
}
- (NSString *)unavailable:(NSString *)renderer {
    NSURL *binary = self.binaries[renderer];
    BOOL directory = NO;
    if (!binary || ![NSFileManager.defaultManager fileExistsAtPath:binary.path isDirectory:&directory] ||
        directory || ![NSFileManager.defaultManager isExecutableFileAtPath:binary.path])
        return @"Not installed. Install this renderer, then reopen the app.";
    if ([renderer isEqual:@"metal"] && !self.metalAvailable)
        return @"Metal is unavailable on this Mac. Use OpenGL to continue.";
    return nil;
}
- (NSTask *)taskForRenderer:(NSString *)renderer {
    NSTask *task = [NSTask new];
    task.executableURL = self.binaries[renderer];
    task.currentDirectoryURL = self.directory;
    task.environment = self.environment;
    task.arguments = [@[@"--launcher"] arrayByAddingObjectsFromArray:self.arguments];
    return task;
}
- (NSTask *)launchRenderer:(NSString *)renderer handler:(void (^)(NSTask *))handler error:(NSError **)error {
    NSString *reason = [self unavailable:renderer];
    if (reason) {
        if (error) *error = [NSError errorWithDomain:@"GoldenEyeRenderer" code:1 userInfo:@{NSLocalizedDescriptionKey: reason}];
        return nil;
    }
    NSTask *task = [self taskForRenderer:renderer];
    task.terminationHandler = handler;
    if (![task launchAndReturnError:error]) return nil;
    return task;
}
@end

#ifndef GE_RENDERER_APP_TEST
@interface GEAppDelegate : NSObject <NSApplicationDelegate>
@property GERendererSession *session;
@property NSTask *child;
- (void)openLauncher;
- (void)recoverWithMessage:(NSString *)message;
@end

@implementation GEAppDelegate
- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    NSURL *directory = [NSBundle.mainBundle.bundleURL URLByDeletingLastPathComponent];
    self.session = [[GERendererSession alloc] initWithDirectory:directory
        owner:[NSBundle.mainBundle objectForInfoDictionaryKey:@"GERendererBuild"]
        arguments:[NSProcessInfo.processInfo.arguments subarrayWithRange:NSMakeRange(1, NSProcessInfo.processInfo.arguments.count - 1)]
        environment:NSProcessInfo.processInfo.environment
        preferences:[[NSUserDefaults alloc] initWithSuiteName:@"org.goldeneyenative.renderer"]
        metalAvailable:MTLCreateSystemDefaultDevice() != nil];
    NSMenu *menu = [NSMenu new];
    NSMenuItem *app = [NSMenuItem new];
    [menu addItem:app];
    app.submenu = [NSMenu new];
    [app.submenu addItemWithTitle:@"Quit GoldenEye" action:@selector(terminate:) keyEquivalent:@"q"];
    NSApp.mainMenu = menu;
    [self openLauncher];
}
- (void)openLauncher {
    // Use OpenGL for the settings UI so selecting Metal never makes recovery depend on it.
    // A Metal-only installation can still open its settings; missing OpenGL is shown there.
    NSString *launcher = ![self.session unavailable:@"gl"] ? @"gl" : @"metal";
    __weak GEAppDelegate *weakSelf = self;
    void (^handler)(NSTask *) = ^(NSTask *finished) {
        dispatch_async(dispatch_get_main_queue(), ^{
            GEAppDelegate *owner = weakSelf;
            owner.child = nil;
            if (finished.terminationStatus == 0) [NSApp terminate:nil];
            else [owner recoverWithMessage:@"The game could not start or exited with an error. Reopen the custom launcher with OpenGL selected to try again. Your saved renderer will not change unless you change it in Video settings."];
        });
    };
    NSError *error = nil;
    self.child = [self.session launchRenderer:launcher handler:handler error:&error];
    if (!self.child) [self recoverWithMessage:[NSString stringWithFormat:@"Could not open the custom launcher: %@", error.localizedDescription]];
}
- (void)recoverWithMessage:(NSString *)message {
    NSAlert *alert = [NSAlert new];
    alert.messageText = @"GoldenEye could not start";
    alert.informativeText = message;
    BOOL canRecover = ![self.session unavailable:@"gl"];
    if (canRecover) [alert addButtonWithTitle:@"Open launcher with OpenGL"];
    [alert addButtonWithTitle:@"Quit"];
    [NSApp activateIgnoringOtherApps:YES];
    if ([alert runModal] == NSAlertFirstButtonReturn && canRecover) {
        self.session.environment[@"GETV_MAC_APP_RENDERER"] = @"gl";
        [self openLauncher]; // One user action, one attempt. No automatic retry.
    } else [NSApp terminate:nil];
}
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender {
    // The child owns the visible custom launcher/game. Do not orphan its monitor.
    return self.child ? NSTerminateCancel : NSTerminateNow;
}
@end

int main(void) {
    @autoreleasepool {
        NSApplication *application = NSApplication.sharedApplication;
        [application setActivationPolicy:NSApplicationActivationPolicyAccessory];
        GEAppDelegate *delegate = [GEAppDelegate new];
        application.delegate = delegate;
        [application run];
    }
    return 0;
}
#endif
