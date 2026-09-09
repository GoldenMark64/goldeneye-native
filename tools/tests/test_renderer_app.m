/* ROM-free native contract tests. Runs production selection and NSTask handoff. */
#define GE_RENDERER_APP_TEST
#import "../../getv/port/mac/ge_renderer_app.m"
#include <sys/stat.h>
#define GE_PLATFORM_MAC 1
#import "../../getv/port/mac/ge_renderer_choice.h"
static int checks = 0;
#define CHECK(...) do { checks++; if (!(__VA_ARGS__)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #__VA_ARGS__); exit(1); } } while (0)
int main(int argc, char **argv) {
 @autoreleasepool {
    if (argc == 3) {
        NSUserDefaults *read = [[NSUserDefaults alloc] initWithSuiteName:[NSString stringWithUTF8String:argv[1]]];
        return [[read objectForKey:@"renderer"] isEqual:[NSString stringWithUTF8String:argv[2]]] ? 0 : 1;
    }
    NSString *suite = [@"org.goldeneyenative.test." stringByAppendingString:NSUUID.UUID.UUIDString];
    NSUserDefaults *prefs = [[NSUserDefaults alloc] initWithSuiteName:suite];
    NSURL *root = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:[@"renderer spaces " stringByAppendingString:NSUUID.UUID.UUIDString]]];
    root = root.URLByResolvingSymlinksInPath;
    NSFileManager *fm = NSFileManager.defaultManager;
    NSURL *gl = [root URLByAppendingPathComponent:@"build-mac"];
    NSURL *metal = [root URLByAppendingPathComponent:@"build-mac-metal"];
    for (NSURL *dir in @[gl, metal]) CHECK([fm createDirectoryAtURL:dir withIntermediateDirectories:YES attributes:nil error:nil]);
    NSString *fixture = @"#!/bin/sh\nprintf '%s\\n' \"$PWD\" \"$GETV_TEST_MARKER\" \"$GETV_MAC_APP_CONFIG_DIR\" \"$@\" > \"$GETV_TEST_OUTPUT\"\nexit 0\n";
    for (NSURL *bin in @[[gl URLByAppendingPathComponent:@"goldeneye"], [metal URLByAppendingPathComponent:@"goldeneye-metal"]]) {
        CHECK([fixture writeToURL:bin atomically:YES encoding:NSUTF8StringEncoding error:nil]);
        CHECK(chmod(bin.fileSystemRepresentation, 0700) == 0);
    }
    NSURL *output = [root URLByAppendingPathComponent:@"output"];
    NSDictionary *env = @{@"GETV_TEST_MARKER": @"preserved", @"GETV_TEST_OUTPUT": output.path};
    GERendererSession *(^session)(NSURL *, NSString *, NSArray *, BOOL) = ^(NSURL *dir, NSString *owner, NSArray *args, BOOL available) {
        return [[GERendererSession alloc] initWithDirectory:dir owner:owner arguments:args environment:env preferences:prefs metalAvailable:available];
    };
    GERendererSession *s = session(gl, @"gl", @[], YES);
    CHECK([s.selection isEqual:@"gl"]);
    CHECK(![s unavailable:@"gl"] && ![s unavailable:@"metal"]);
    [prefs setObject:@"metal" forKey:@"renderer"];
    CHECK([session(gl, @"gl", @[], YES).selection isEqual:@"metal"]);
    s = session(gl, @"gl", @[@"--app-renderer=gl", @"--config=config with spaces.cfg"], YES);
    CHECK([s.selection isEqual:@"gl"]);
    CHECK([s.arguments isEqual:@[@"--config=config with spaces.cfg"]]);
    CHECK([[prefs objectForKey:@"renderer"] isEqual:@"metal"]);
    CHECK(session(gl, @"gl", @[@"--app-renderer=bad"], YES).selection == nil);
    [prefs setObject:@42 forKey:@"renderer"];
    CHECK(session(gl, @"gl", @[], YES).selection == nil);
    CHECK([[prefs objectForKey:@"renderer"] isEqual:@42]);
    CHECK([session(gl, @"gl", @[], NO) unavailable:@"metal"] != nil);
    CHECK([session(metal, @"metal", @[], YES).directory.path isEqual:gl.path]);
    // Real subprocess handoff with spaces, explicit config and inherited environment.
    NSError *error = nil;
    NSTask *task = [s launchRenderer:@"metal" handler:nil error:&error];
    CHECK(task != nil && error == nil);
    [task waitUntilExit];
    CHECK(task.terminationStatus == 0);
    NSString *actual = [NSString stringWithContentsOfURL:output encoding:NSUTF8StringEncoding error:nil];
    char *physical = realpath(gl.fileSystemRepresentation, NULL);
    CHECK(physical != NULL);
    CHECK([actual isEqual:[NSString stringWithFormat:@"%@\npreserved\n%@\n--launcher\n--config=config with spaces.cfg\n", [NSString stringWithUTF8String:physical], gl.path]]);
    free(physical);
    CHECK(geAppRememberRenderer(1, (__bridge CFStringRef)suite));
    [prefs synchronize];
    NSTask *reopen = [NSTask new];
    reopen.executableURL = [NSURL fileURLWithPath:[NSString stringWithUTF8String:argv[0]]];
    reopen.arguments = @[suite, @"metal"];
    CHECK([reopen launchAndReturnError:&error]);
    [reopen waitUntilExit];
    CHECK(reopen.terminationStatus == 0);
    NSUserDefaults *reopened = [[NSUserDefaults alloc] initWithSuiteName:suite];
    CHECK([[reopened objectForKey:@"renderer"] isEqual:@"metal"]);
    task = [s launchRenderer:@"gl" handler:nil error:&error];
    CHECK(task != nil); [task waitUntilExit];
    CHECK([[prefs objectForKey:@"renderer"] isEqual:@"metal"]);
    // Failed executable launch must not change the preference or silently retry.
    NSURL *glBinary = s.binaries[@"gl"];
    CHECK([@"invalid executable" writeToURL:glBinary atomically:YES encoding:NSUTF8StringEncoding error:nil]);
    CHECK(chmod(glBinary.fileSystemRepresentation, 0700) == 0);
    CHECK([s launchRenderer:@"gl" handler:nil error:&error] == nil);
    CHECK([[prefs objectForKey:@"renderer"] isEqual:@"metal"]);
    CHECK([fm removeItemAtURL:s.binaries[@"metal"] error:nil]);
    CHECK([s unavailable:@"metal"] != nil);
    CHECK([s launchRenderer:@"metal" handler:nil error:&error] == nil);
    // Replacing a build folder preserves relative discovery; no embedded absolute path.
    NSURL *moved = [root URLByAppendingPathComponent:@"moved gl folder"];
    CHECK([fm moveItemAtURL:gl toURL:moved error:nil]);
    CHECK([session(moved, @"gl", @[], YES).binaries[@"gl"].path isEqual:[moved.path stringByAppendingPathComponent:@"goldeneye"]]);
    [prefs removePersistentDomainForName:suite];
    CHECK([fm removeItemAtURL:root error:nil]);
    printf("PASS %d native renderer app checks\n", checks);
 }
 return 0;
}
