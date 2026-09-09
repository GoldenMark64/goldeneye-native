/* macOS app-only renderer handoff. Direct binary and other-platform launches ignore it. */
#ifndef GE_RENDERER_CHOICE_H
#define GE_RENDERER_CHOICE_H
#if defined(__APPLE__) && defined(GE_PLATFORM_MAC)
#include <CoreFoundation/CoreFoundation.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static inline int geAppRendererActive(void) {
    const char *value = getenv("GETV_MAC_RENDERER_APP");
    return value && strcmp(value, "1") == 0;
}
static inline int geAppRendererSelected(void) {
    const char *value = getenv("GETV_MAC_APP_RENDERER");
    if (value && strcmp(value, "gl") == 0) return 0;
    if (value && strcmp(value, "metal") == 0) return 1;
    return -1;
}
static inline const char *geAppRendererPath(int renderer) {
    return getenv(renderer == 0 ? "GETV_MAC_APP_GL" : "GETV_MAC_APP_METAL");
}
static inline int geAppRendererAvailable(int renderer) {
    if (renderer < 0 || renderer > 1) return 0;
    const char *path = geAppRendererPath(renderer);
    struct stat st;
    if (!path || stat(path, &st) != 0 || !S_ISREG(st.st_mode) || access(path, X_OK) != 0) return 0;
    if (renderer == 1) {
        const char *available = getenv("GETV_MAC_APP_METAL_AVAILABLE");
        if (!available || strcmp(available, "1") != 0) return 0;
    }
    return 1;
}
static inline int geAppRememberRenderer(int renderer, CFStringRef domain) {
    if (renderer < 0 || renderer > 1) return 0;
    CFPreferencesSetAppValue(CFSTR("renderer"), renderer ? CFSTR("metal") : CFSTR("gl"), domain);
    return CFPreferencesAppSynchronize(domain);
}
#endif
#endif
