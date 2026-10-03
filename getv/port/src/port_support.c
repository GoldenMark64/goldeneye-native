/* GoldenEye tvOS port - the host services Fast3D expects.
 *
 * sm64ex supplies these from its own platform/config/filesystem layers.
 * GoldenEye's decomp has none of that, so the port provides the minimum Fast3D
 * actually touches. Kept deliberately small: everything here is host plumbing, not
 * game behaviour.
 */
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#include <SDL.h>

#include "../platform.h"
#include "../configfile.h"
#include "../fs/fs.h"
#include "../pc_main.h"
#include "../fast3d/gfx_window_manager_api.h"   /* WAPI_WIN_CENTERPOS */

/* dyn.c owns two alternating transient graphics buffers. Texture bytes built with dynAllocate()
 * are mutable even when their pointer repeats on a later frame, so Fast3D must not treat their
 * address as immutable texture identity. */
extern unsigned char *g_VtxBuffers[3];

bool gePortTextureSourceIsTransient(const void *ptr)
{
    uintptr_t p;
    uintptr_t begin;
    uintptr_t end;

    if (ptr == NULL || g_VtxBuffers[0] == NULL || g_VtxBuffers[2] == NULL) {
        return false;
    }

    p = (uintptr_t)ptr;
    begin = (uintptr_t)g_VtxBuffers[0];
    end = (uintptr_t)g_VtxBuffers[2];
    return p >= begin && p < end;
}

/* ---- platform ---------------------------------------------------------- */

void sys_fatal(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    printf("[getv] FATAL: ");
    vprintf(fmt, ap);
    va_end(ap);
    putchar('\n');
    /* stdout to `devicectl --console` is block-buffered, so without this flush the
     * message explaining the crash is exactly the part that gets lost. This cost
     * real time on Perfect Dark, where a SIGABRT presented as a clean exit 0. */
    fflush(stdout);
    abort();
}

/* The argument is microseconds, not seconds. sm64ex's only caller is
 * sync_framerate_with_timer() in gfx_sdl2.c, which passes
 * `remain / perf_freq * 1000000.0`. Treating it as seconds multiplies every wait by a
 * million: a 16 ms frame pace becomes SDL_Delay(16000000), about four and a half hours.
 * Frame 0 runs long enough to skip the sleep entirely, so the port appears healthy
 * until frame 1 renders quickly and hangs inside gfx_end_frame(). */
void sys_sleep(double us)
{
    if (us > 0.0) {
        SDL_Delay((Uint32)(us / 1000.0));
    }
}

/* ---- host timebase ------------------------------------------------------ */

/* The N64 count register, synthesised from the host clock, at its real 46.875 MHz
 * rate. Called only by osGetCount() under GETV_REALCLOCK=1 -- see the long note there
 * for why that is opt-in. It lives HERE rather than in port_os.c purely because
 * port_os.c includes <PR/os.h>, which cannot coexist with <string.h> (and therefore
 * not with <SDL.h>) in either order.
 *
 * Reduce against an origin before scaling. arm64 macOS's performance counter is in the
 * nanosecond domain and is already far past 2^32 at boot, so scaling the absolute value
 * loses all precision; measuring from the first call keeps the product small and exact.
 * The u32 truncation that remains is the same wrap the hardware register had. */
unsigned int gePortHostN64Count(void)
{
    static Uint64 origin = 0;
    static double freq   = 0.0;
    Uint64 now;

    if (freq == 0.0) {
        freq   = (double)SDL_GetPerformanceFrequency();
        origin = SDL_GetPerformanceCounter();
        if (freq <= 0.0) { freq = 1.0; }
    }
    now = SDL_GetPerformanceCounter();
    return (unsigned int)(unsigned long long)(((double)(now - origin) / freq) * 46875000.0);
}

/* ---- config ------------------------------------------------------------ */

/* tvOS has exactly one display mode, 1920x1080, and no windowing. These values are
 * therefore fixed rather than read from a config file.
 *
 * macOS is the one platform on this port that has a window, and that is the reason the
 * Mac target exists -- a window can be looked at, screenshotted and played. So it
 * starts windowed and resizable rather than seizing the display. Override with
 * GETV_WINDOW=WxH, or GETV_FULLSCREEN=1. */
#ifdef GE_PLATFORM_DESKTOP
ConfigWindow configWindow = {
    .x = (unsigned)WAPI_WIN_CENTERPOS, .y = (unsigned)WAPI_WIN_CENTERPOS,
    .w = 1280, .h = 960,          /* 4:3, the aspect the game was authored for */
    .vsync = true,
    .reset = false,
    .fullscreen = false,
    .exiting_fullscreen = false,
    .settings_changed = false,
};

/* Called from gePortMacWindowConfig() below, before gfx_init(). Deliberately env-driven
 * rather than a config file: this port has no settings UI and every other knob on it is
 * a GETV_* variable. */
void gePortMacWindowConfig(void)
{
    const char *w = getenv("GETV_WINDOW");
    const char *f = getenv("GETV_FULLSCREEN");
    if (w != NULL && *w != '\0') {
        unsigned ww = 0, hh = 0;
        if (sscanf(w, "%ux%u", &ww, &hh) == 2 && ww >= 320 && hh >= 240) {
            configWindow.w = ww;
            configWindow.h = hh;
        }
    }
    if (f != NULL && *f == '1') {
        configWindow.fullscreen = true;
    }

    /* Clamp the default to a window that fits. 1280x960 is bigger than the usable area
     * of a 13" laptop panel once the menu bar and Dock are subtracted, and an oversized
     * SDL window on macOS comes up with its title bar under the menu bar and its bottom
     * off-screen, leaving a window that cannot be moved. Shrink by whole 4:3 steps so
     * the aspect the game was authored for is preserved rather than letterboxed.
     *
     * Only the default is clamped. An explicit GETV_WINDOW is honoured as given:
     * measurement runs deliberately ask for sizes larger than the panel, and `drawn` is
     * resolution-sensitive on Mac (659 at <=1280x960, 672/673 at >=1600x1200), so
     * silently resizing a requested size would corrupt a comparison. */
    if ((w == NULL || *w == '\0') && !configWindow.fullscreen) {
        SDL_Rect usable;
        /* Idempotent: gfx_sdl_init() calls SDL_Init(SDL_INIT_VIDEO) again and SDL
         * reference-counts subsystems, so this does not disturb the normal path. */
        if (SDL_InitSubSystem(SDL_INIT_VIDEO) == 0 &&
            SDL_GetDisplayUsableBounds(0, &usable) == 0 &&
            usable.w > 0 && usable.h > 0) {
            static const unsigned steps[][2] = {
                { 1280, 960 }, { 1024, 768 }, { 800, 600 }, { 640, 480 }
            };
            size_t i;
            for (i = 0; i < sizeof(steps) / sizeof(steps[0]); i++) {
                /* Leave room for the title bar; SDL's usable bounds do not subtract it. */
                if (steps[i][0] <= (unsigned)usable.w &&
                    steps[i][1] + 28u <= (unsigned)usable.h) {
                    break;
                }
            }
            if (i >= sizeof(steps) / sizeof(steps[0])) { i = sizeof(steps) / sizeof(steps[0]) - 1; }
            if (steps[i][0] != configWindow.w || steps[i][1] != configWindow.h) {
                printf("[getv] window: %ux%u would not fit usable %dx%d -- using %ux%u\n",
                       configWindow.w, configWindow.h, usable.w, usable.h,
                       steps[i][0], steps[i][1]);
            }
            configWindow.w = steps[i][0];
            configWindow.h = steps[i][1];
        }
    }

    printf("[getv] window: %ux%u %s, resizable; fullscreen toggle = F11 / Cmd-F / Alt-Enter\n",
           configWindow.w, configWindow.h,
           configWindow.fullscreen ? "fullscreen" : "windowed");
    fflush(stdout);
}
#else
ConfigWindow configWindow = {
    .x = 0, .y = 0, .w = 1920, .h = 1080,
    .vsync = true,
    .reset = false,
    .fullscreen = true,
    .exiting_fullscreen = false,
    .settings_changed = false,
};
#endif

/* 0 = nearest, 1 = bilinear, 2 = three-point. Three-point is what the N64 actually
 * did and what looked right on Perfect Dark.
 *
 * GETV_FILTERING=0|1|2 overrides the compiled-in default below, read once via a GCC/clang
 * constructor rather than a lazy-static check at the read sites -- gfx_opengl.c and gfx_pc.c
 * both read this global directly with no accessor to hang a check off, and both run before
 * any explicit port-init call this file could otherwise piggyback on. Constructors run before
 * main() on every platform this project targets, so "before the first read" is guaranteed
 * without needing to find or disturb an existing init sequence. */
unsigned int configFiltering = 2;

__attribute__((constructor))
static void ge_filtering_env_init(void)
{
    const char *e = getenv("GETV_FILTERING");
    if (e && *e >= '0' && *e <= '2' && e[1] == '\0') {
        configFiltering = (unsigned int) (*e - '0');
    }
}

/* 1 = the rendered scene fills the real window at its real aspect; 0 = retail behaviour,
 * pillarboxed/letterboxed to the game's native 4:3 (gfx_pc.c's ge_scale()/ge_offset_*()
 * picking the smaller of the two axis scales). Defaults on: the pillarbox was never a
 * deliberate user-facing choice, just what happens when nothing corrects for a non-4:3
 * window, and it is the thing GETV_WIDESCREEN=0 is for undoing on request.
 *
 * Same env-var-via-constructor approach as configFiltering above, for the same reason:
 * gfx_pc.c reads this global directly at multiple call sites with no accessor to hang a
 * lazy check off. */
unsigned int configWidescreen = 1;

__attribute__((constructor))
static void ge_widescreen_env_init(void)
{
    const char *e = getenv("GETV_WIDESCREEN");
    if (e && *e >= '0' && *e <= '1' && e[1] == '\0') {
        configWidescreen = (unsigned int) (*e - '0');
    }
}

/* F9 (gfx_sdl2.c's onkeydown) toggles vsync live during gameplay, the same F-row convention
 * as the existing F11 fullscreen toggle right there. This was originally aimed at an in-game
 * Watch settings page instead; options.h's WATCH_NUMBER_SCREENS carries an explicit "do not
 * change this value until player struct is fully shiftable" from the decomp itself, since the
 * per-page selector rectangles are sized off it directly inside struct player, so that route
 * was dropped in favour of this one, which touches no vendor struct at all.
 *
 * configWindow.vsync is not a one-shot cache like configWidescreen above: gfx_sdl2.c already
 * re-checks configWindow.settings_changed every frame, and F11 and window-resize both go
 * through that same flag, so this is a thin wrapper over an apply mechanism that already
 * existed rather than a new one. The getter exists so the keybinding can toggle from the real
 * current state instead of tracking a second, driftable copy. */
void gePortSetVsync(int on)
{
    configWindow.vsync = on ? true : false;
    configWindow.settings_changed = true;
    printf("[getv][video] vsync: %s (F9 to toggle)\n", on ? "on" : "off");
    fflush(stdout);
}

int gePortGetVsync(void)
{
    return configWindow.vsync ? 1 : 0;
}

/* GETV_CROSSHAIR_COLOR=RRGGBB -- gunfire.c's gunDrawSight() passes this straight through as
 * the RDP primitive colour it multiplies crosshairimage's decoded texels by, in place of the
 * retail 0xFF,0xFF,0xFF ("show the texture's own colour unmodified"). Defaults to white,
 * byte-for-byte the current behaviour, so an unset variable changes nothing.
 *
 * How cleanly it recolours depends on the baked N64 asset under the tint, which has not been
 * confirmed here: a white or grey source recolours cleanly under a multiply, a source with its
 * own baked hue only partially. This ships the mechanism the 1997 code already exposed at that
 * call site rather than blocking on resolving it, and the launcher's colour picker makes the
 * real answer visible immediately, which is the more direct check anyway. */
unsigned char ge_crosshair_r = 0xFF;
unsigned char ge_crosshair_g = 0xFF;
unsigned char ge_crosshair_b = 0xFF;

__attribute__((constructor))
static void ge_crosshair_color_env_init(void)
{
    const char *e = getenv("GETV_CROSSHAIR_COLOR");
    if (e && *e) {
        unsigned int r, g, b;
        if (sscanf(e, "%2x%2x%2x", &r, &g, &b) == 3) {
            ge_crosshair_r = (unsigned char) r;
            ge_crosshair_g = (unsigned char) g;
            ge_crosshair_b = (unsigned char) b;
        }
    }
}

/* GETV_CROSSHAIR_SCALE -- a multiplier on the sight's half-extent in gunDrawSight().
 *
 * Retail hard-codes 16.0f for both axes, then narrows x by 0.75 at 16:9 and, on PAL, scales
 * y by g_GunSightAspectRatio. Those are aspect corrections and they still apply; this is a
 * separate factor on top, so the reticle keeps its correct shape at any window and only
 * changes size.
 *
 * 1.0 is retail, exactly. It is the default, and an unset variable is byte-for-byte the
 * original call. The reason to want anything else is that a 32-pixel sight was sized for a
 * 320x240 field of view on a CRT across a room; at 1280x960 on a desk it covers noticeably
 * more of what you are aiming at than it did in 1997. GoldenEye+ asks for 0.6.
 *
 * Clamped rather than trusted. Below about a quarter the texture has too few texels left to
 * read as a shape, and above 2.0 it stops being a sight and starts being an obstruction. */
float ge_crosshair_scale = 1.0f;

__attribute__((constructor))
static void ge_crosshair_scale_env_init(void)
{
    const char *e = getenv("GETV_CROSSHAIR_SCALE");
    if (e && *e) {
        double v = atof(e);
        if (v >= 0.25 && v <= 2.0) {
            ge_crosshair_scale = (float) v;
        }
    }
}

/* GETV_PARALLAX -- whether a height map found in a texture pack displaces the diffuse UVs.
 *
 * The shader carries the parallax branch unconditionally and gates it at run time on
 * uHasHeight, which is only ever true for a texture whose pack supplied a `<hash>_h.png`
 * companion. So with no pack, or a pack with no height maps, this changes nothing either
 * way: there is no height data in the game's own assets and never was.
 *
 * It is a switch rather than an automatic because the two profiles want different answers to
 * the same installed pack. Somebody running 97 Console with an HD pack for the texture
 * resolution should not silently also get displacement the N64 never did; somebody running
 * GoldenEye+ should. Defaults on, since the only way to reach it at all is to have gone and
 * installed a pack that carries height maps. */
int gePortParallaxEnabled(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_PARALLAX");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 1;
    }
    return on;
}

/* 1 = ge_upload_texture() (gfx_pc.c) checks GETV_TEXPACK for an override of every texture
 * before uploading the N64 decoder's own output; 0 = never checks, byte-for-byte the
 * current behaviour. Defaults OFF, unlike configFiltering/configWidescreen above -- both
 * of those were verified by tracing the exact call order and, for widescreen, by working
 * through the arithmetic that proves ge_scale() collapses to a single uniform factor. This
 * one has had no such ver‹­¦ëm®éÜj×¢¸ Šv¥jšk£¦j×­¢G§r‹§·]yßÞÂˆ\ˆH™\ÛÛ™YÂˆYˆ
ÙWÝ^XÚ×Ý˜XÙWÛÛŠ
JHÂˆš[Š–ÙÙ]—VÝ^XÚ×HXÚÈ\™XÝÜžNˆ	\È
^Y\‹\™[]]™JWˆ‹\ŠNÂˆ™›\Ú
ÝÝ]
NÂˆBˆ™]\›ˆ\ŽÂˆBˆBˆB‚ˆYˆ
ÙWÝ^XÚ×Ý˜XÙWÛÛŠ
JHÂˆš[Š–ÙÙ]—VÝ^XÚ×H›ÈXÚÈ\™XÝÜžH›Ý[™
šYY‰\×ˆYØZ[œÝÝÙ[™ÑU—ÑVQTŠHKH‚ˆ’^\™\ÈÚ[™HH›Ë[Üˆ‹\ŠNÂˆ™›\Ú
ÝÝ]
NÂˆBˆ\ˆH•SÂˆ™]\›ˆ\ŽÂŸB‚‹Êˆ˜\ÝÑ	ÜÈÝÛˆVT“SÑUH]\ÚÜÈ›Üˆ”×ÕVT‘QTˆ
™ÙžŠHÜXÚYšXØ[H[™^XÝÂˆ
ˆHØ[ÈÈ™XÝ\œÙH[È]È\ÈXÚÈ\È›ÈÝXÚÝXÝ\™K]\ÈÛ™H›]›Û\ˆÙ‚ˆ
ˆ\Ú‹^ˆš[\ËÛÈH˜\ÙX˜[Z[™È[žHÝX™\™XÝÜžHYÚ][X][Hš[™È›Ý[™Ëˆ
‹Â™œ×ÝØ[×Ü™\Ý[Ýœ×ÝØ[ÊÛÛœÝÚ\ˆ
˜˜\ÙKØ[×Ù›—ÝØ[Ù›‹›ÚY
\Ù\‹ÛÛœÝ›ÛÛ™XÝ\ŠBžÂˆÛÛœÝÚ\ˆ
œ›ÛÝHÙWÝ^XÚ×Ù\Š
NÂˆÚ\ˆ]ÌLŽNÂˆTˆ
™ÂˆÝXÝ\™[
™NÂˆœ×ÝØ[×Ü™\Ý[Ý™\Ý[H”×ÕÐS×ÔÕPÐÑTÔÎÂ‚ˆ
›ÚY
\™XÝ\ŽÈÊˆ›]\™XÝÜžK›Ý[™ÈÈ™XÝ\œÙH[È
‹ÂˆYˆ
›ÛÝOH•S
H™]\›ˆ”×ÕÐS×Ó“Õ“ÕS‘Â‚ˆYˆ
˜\ÙHOH•S	‰ˆ
˜˜\ÙHOH	×	È	‰ˆÝ˜Û\
˜\ÙK‹ˆŠHOH
HÂˆÛœš[Š]Ú^™[ÙŠ]
K‰\ËÉ\È‹›ÛÝ˜\ÙJNÂˆH[ÙHÂˆÛœš[Š]Ú^™[ÙŠ]
K‰\È‹›ÛÝ
NÂˆB‚ˆHÜ[™\Š]
NÂˆYˆ
OH•S
H™]\›ˆ”×ÕÐS×Ó“Õ“ÕS‘Â‚ˆÚ[H

HH™XY\Š
JHOH•S
HÂˆÚ\ˆ[ÌMLÍ—NÂˆYˆ
Ý˜Û\
KO™Û˜[YK‹ˆŠHOHÝ˜Û\
KO™Û˜[YK‹‹ˆŠHOH
HÛÛ[YNÂˆÛœš[Š[Ú^™[ÙŠ[
K‰\ËÉ\È‹]KO™Û˜[YJNÂˆYˆ
]Ø[Ù›Š\Ù\‹[
JHÂˆ™\Ý[H”×ÕÐS×ÒS•T”•TQÂˆœ™XZÎÂˆBˆBˆÛÜÙY\Š
NÂˆ™]\›ˆ™\Ý[ÂŸB‚›ÚY
™œ×ÛØYÙš[JÛÛœÝÚ\ˆ
œ]Z[Ý
›Ý]Ú^™JBžÂˆÛÛœÝÚ\ˆ
œ›ÛÝHÙWÝ^XÚ×Ù\Š
NÂˆÚ\ˆ]ÌLŽNÂˆ’SH
™ŽÂˆÛ™ÈÚ^™NÂˆ›ÚY
˜YŽÂ‚ˆYˆ
Ý]Ú^™JH
›Ý]Ú^™HHÂˆYˆ
›ÛÝOH•Sœ]OH•S
œ]OH	×	ÊH™]\›ˆ•SÂ‚ˆÊˆœ×ÝØ[Ê
HX›Ý™H[™È]ÈØ[˜XÚÈ[]È[™\ˆH™\ÛÛ™Y›ÛÝÈHØ[\‚ˆ
ˆ]\›œÈ\›Ý[™[™ØYÈÛ™HÙˆÜÙH
˜]\ˆ[ˆH˜\™H\Ú‹œ™ÈŠH\Âˆ
ˆ\ÜÚ[™È[ˆ[™XYK\›ÛÝY]˜XÚÈ[‹ÛÈ[ˆXœÛÛ]Hœ]\È\ÙY\ËZ\È[œÝXYˆ
ˆÙˆ™Z[™È›Ú[™YÛÈ›ÛÝHÙXÛÛ™[YKˆ
‹ÂˆYˆ
œ]ÌHOH	ËÉÈœ]ÌHOH	×	È
œ]ÌHOH	×	È	‰ˆœ]ÌWHOH	Î‰ÊJHÂˆÛœš[Š]Ú^™[ÙŠ]
K‰\È‹œ]
NÂˆH[ÙHÂˆÛœš[Š]Ú^™[ÙŠ]
K‰\ËÉ\È‹›ÛÝœ]
NÂˆB‚ˆˆH›Ü[Š]œ˜ˆŠNÂˆYˆ
ˆOH•S
H™]\›ˆ•SÂ‚ˆYˆ
œÙYZÊ‹ÑQR×ÑS‘
HOH
HÈ˜ÛÜÙJŠNÈ™]\›ˆ•SÈBˆÚ^™HH[
ŠNÂˆYˆ
Ú^™HœÙYZÊ‹ÑQR×ÔÑU
HOH
HÈ˜ÛÜÙJŠNÈ™]\›ˆ•SÈB‚ˆYˆHX[ØÊ
Ú^™WÝ
HÚ^™JNÂˆYˆ
YˆOH•S
HÈ˜ÛÜÙJŠNÈ™]\›ˆ•SÈB‚ˆYˆ
Ú^™Hˆ	‰ˆœ™XY
Y‹K
Ú^™WÝ
HÚ^™KŠHOH
Ú^™WÝ
HÚ^™JHÂˆœ™YJYŠNÂˆ˜ÛÜÙJŠNÂˆ™]\›ˆ•SÂˆBˆ˜ÛÜÙJŠNÂ‚ˆYˆ
Ý]Ú^™JH
›Ý]Ú^™HH
Z[Ý
HÚ^™NÂˆ™]\›ˆYŽÂŸB‚‹ÊˆKKKHÚ]ÝÛˆKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKH
‹Â‚‹Êˆ˜\ÝÑ	ÜÈÑ˜XÚÙ[™Ø[È\ÙHÚ[ˆHÔÈ\ÚÜÈH\È]Z]ˆ\™H\È›Âˆ
ˆØ[YHÝ]HÈX\ˆÝÛˆY]KHHXÛÛ\	ÜÈÝÛˆ›ÛÝ]\È›ÝÚ\™YKHÛÈBˆ
ˆ\›™\ÜÈ\ÝX]™\ÈÛX[›Kˆ
‹Â›ÚYØ[YWÙZ[š]
›ÚY
HÈB‚›ÚYØ[YWÙ^]
›ÚY
BžÂˆš[Š–ÙÙ]—HØ[YWÙ^]™\]Y\ÝYˆŠNÂˆ™›\Ú
ÝÝ]
NÂˆ^]

NÂŸB‚‹ÊˆKKKH[œ]KKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKH
‹Â‚‹ÊˆH\Hˆ\È›ÈÙ^X›Ø\™È˜\ÝÑ	ÜÈÑ˜XÚÙ[™™YÚ\Ý\œÈ\ÙH™YØ\™\ÜË‚ˆ
ˆ™]\›š[™È˜[ÙHYX[œÈ››Ý[™Y‹ÚXÚ\ÈÛÜœ™XÝ\™KˆØ[Y\Y[œ]ÛÙ\Âˆ
ˆ›ÝYÚÑ	ÜÈ›Þ\ÝXÚËÑØ[YPÛÛ›Û\ˆ]Ú[ˆHØ[YH\ÈÚ\™Y\ˆ
‹Â˜›ÛÛÙ^X›Ø\™ÛÛ—ÚÙ^WÙÝÛŠ[ØØ[˜ÛÙJHÈ
›ÚY
\ØØ[˜ÛÙNÈ™]\›ˆ˜[ÙNÈB˜›ÛÛÙ^X›Ø\™ÛÛ—ÚÙ^WÝ\
[ØØ[˜ÛÙJHÈ
›ÚY
\ØØ[˜ÛÙNÈ™]\›ˆ˜[ÙNÈB›ÚYÙ^X›Ø\™ÛÛ—Ø[ÚÙ^\×Ý\
›ÚY
HÈB‚‹ÊˆKKKH›ÛÝ˜XÚ[™È
[\Ü˜\žJHKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKH
‹Â‚‹ÊˆØ[Yœ›ÛH›ÜÜÒ[š]XZ[™XY]J
H™Y›Ü™HXXÚ[š]Ý\ÛÈH]šXÙHÛÛœÛÛBˆ
ˆÚÝÜÈ^XÝHÝÈ˜\ˆÝ\\Ù]Ëˆ™[[Ý™H[Û™ÈÚ]HØ[È[ˆ›ÜÜË˜ÈÛ˜ÙHBˆ
ˆ›ÛÝ]ÛÛ\]\Ëˆ
‹Â‹Êˆ\‹]YÈš[YÙ]›ÜˆÙTÜ›ÛÝX\šÊ
NÈÙYHH›ÝH\™KˆYÜÈ\™HÝš[™Âˆ
ˆ]\˜[ËÛÈÛÛ\\š[™ÈžHÚ[\ˆ\È›ÝÛÜœ™XÝ[™ÚX\ˆ
‹Â˜ÛÛœÝÚ\ˆ
™ÙWÛ\ÝÛX\šÈHŠ›Û™JHŽÂ[œÚYÛ™YÛ™ÈÙWÛX\š×ÜÙ\HHÂ‚ˆÙYš[™HÑWÓPT’×Ô‘TPUÈÂˆÙYš[™HÑWÓPT’×ÓPVLL‚‚œÝ]XÈ[ÙWÛX\š×ÜÚÝ[Üš[
ÛÛœÝÚ\ˆ
Ú]
BžÂˆÝ]XÈÛÛœÝÚ\ˆ
YÜÖÑÑWÓPT’×ÓPVNÂˆÝ]XÈ[]ÖÑÑWÓPT’×ÓPVNÂˆÝ]XÈ[YÜÈHÂˆ[NÂ‚ˆ›Üˆ
HHÈHYÜÎÈJÊÊHÂˆYˆ
YÜÖÚWHOHÚ]
HÂˆYˆ
]ÖÚWHÑWÓPT’×Ô‘TPUÊHÂˆ]ÖÚWJÊÎÂˆYˆ
]ÖÚWHOHÑWÓPT’×Ô‘TPUÊHÂˆš[Š–ÙÙ]—H
\\ˆ	É\ÉÈX\šÜÈÝ\™\ÜÙY
Wˆ‹Ú]
NÂˆBˆ™]\›ˆNÂˆBˆ™]\›ˆÂˆBˆBˆYˆ
YÜÈÑWÓPT’×ÓPV
HÂˆYÜÖÛYÜ×HHÚ]Âˆ]ÖÛYÜ×HHNÂˆYÜÊÊÎÂˆBˆ™]\›ˆNÂŸB‚›ÚYÙTÜ›ÛÝX\šÊÛÛœÝÚ\ˆ
Ú]
BžÂˆÊˆÚXÚÚ[™ÈHÝXˆØ[˜\šY\È\™H\ÈÚ]XZÙ\ÈHÝXˆÝ™\™›ÝÈš[™X›KˆBˆ
ˆ×Ô›ÜÈÝ™\œ[ˆÛÜœ\YHY[[ÜžK\ÛÛ˜[šÈX›H[™Û›HÝ\™˜XÙY™YBˆ
ˆÝXœÞ\Ý[\È]\‹\ÈHÚ[[Ú[JJNØ[œÚYHY[\[ØÐž]\Ò[˜[šËˆÚXÚÚ[™Âˆ
ˆÛˆ]™\žH›ÛÝX\šÈ˜[Y\È›ÝHÙ™™[™[™ÈÞ[X›Û[™HÝ\]Y]ˆ
‹Âˆ^\›ˆÛÛœÝÚ\ˆ
™ÙTÜÝXÚXÚÊ›ÚY
NÂˆÝ]XÈ[™\ÜYHÂˆÛÛœÝÚ\ˆ
˜˜YÂ‚ˆÊˆ˜]K[[Z]Yˆ[˜ÛÛ™][Û˜[X\šÜÈš[Ûˆ]™\žHœ˜[YHÛ˜ÙHH›ÛÝ]™XXÚ\Âˆ
ˆHœ˜[YHÛÜÚXÚ›ÙXÙY][KYÚYØXž]HÙÜÈœ›ÛH[œÈÙˆHÛÝ\HÙ‚ˆ
ˆZ[]\ËˆXXÚ\Ý[˜ÝYÈš[È]Èš\œÝÑWÓPT’×Ô‘TPUÈØØÝ\œ™[˜Ù\È[™\Âˆ
ˆ[ˆÝ\™\ÜÙYÙY\[™ÈHÛ™K\ÚÝ›ÛÝ˜XÙH[XÝÚ[HXZÚ[™ÈBˆ
ˆÝXYK\Ý]HÛÜÛÜÝ›Ý[™Ë‚ˆ
‚ˆ
ˆ]™\žHX\šÈ\ÈÝ[™XÛÜ™Y]™[ˆHÝ\™\ÜÙYÛ™Kˆ˜]K[[Z][™ÈÙY\ÈHÙÂˆ
ˆÛX[]ÛÝ[Ý\Ú\ÙHYHÚ\™HHÜ˜\Ú\[™YÛ˜ÙHHœ˜[YHÛÜ\Âˆ
ˆ[›š[™ËÚ[˜ÙHH\Ý[™Èš[YÛÝ[™HÙ]™\˜[]\˜][ÛœÈÝ[KˆBˆ
ˆÜ˜\Ú[™\ˆš[ÈÙWÛ\ÝÛX\šÈ[œÝXYÛÈÝ\™\ÜÚ[ÛˆÛÜÝÈ›Ý[™Âˆ
ˆXYÛ›ÜÝXØ[Kˆ
‹ÂˆÙWÛ\ÝÛX\šÈHÚ]ÂˆÙWÛX\š×ÜÙ\JÊÎÂ‚ˆYˆ
ÙWÛX\š×ÜÚÝ[Üš[
Ú]
JHÂˆš[Š–ÙÙ]—H›ÛÝOˆ	\×ˆ‹Ú]
NÂˆB‚ˆÊˆØ[YHYXH\ÈHÝXˆØ[˜\žK›ÜˆHY[[ÜžK\ÛÛ˜[šÈX›Nˆ™\ÜHš\œÝˆ
ˆ›ÛÝÝ\Y\ˆÚXÚ[žHÛÛ\È[™Ý\ˆ
‹ÂˆÂˆ^\›ˆ[ÙTÜY[\Ø[™J›ÚY
NÂˆÝ]XÈ[Y[\Ü™\ÜYHÂˆYˆ
[Y[\Ü™\ÜY	‰ˆYÙTÜY[\Ø[™J
JHÂˆY[\Ü™\ÜYHNÂˆš[Š–ÙÙ]—H
ŠŠˆÓÓP“HÓÔ”•TQ\š[™È	É\ÉÈ
X\šÈÉ[JWˆ‹ˆÚ]ÙWÛX\š×ÜÙ\JNÂˆBˆB‚ˆYˆ
\™\ÜY	‰ˆ
˜YHÙTÜÝXÚXÚÊ
JHOH•S
HÂˆ™\ÜYHNÈÊˆÛ˜ÙNˆY\ˆHš\œÝÝ™\œ[ˆ]™\žH]\ˆÚXÚÈ[ÛÈš\È
‹Âˆš[Š–ÙÙ]—H
ŠŠˆÕPˆÕ‘T‘“ÕÎˆ	\ÈÝ™\œ˜[ˆ]È	YXž]HÝÜ˜YÙK]XÝY]‚ˆ‰É\ÉËˆ]È™X[Ú^™H\È\™Ù\ˆKHÚ]™H]H™X[Yš[š][Û‹—ˆ‹ˆ˜YMˆ
ˆLÚ]
NÂˆBˆ™›\Ú
ÝÝ]
NÂŸB‚‹ÊˆKKKHÜÔÞ[˜Ôš[ˆKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKH
‹Â‚‹ÊˆX[˜IÜÈXYÈš[ˆÛˆH]Ù[ÈHÜÝÝ™\ˆHXYÈÜÈ\™H]ˆ
ˆÛÙ\ÈÈH]šXÙHÛÛœÛÛHZÙH]™\ž][™È[ÙKˆ[\[Y[Y›Üˆ™X[˜]\ˆ[‚ˆ
ˆÝX˜™Y™XØ]\ÙH]\ÈHXÛÛ\	ÜÈÕÓˆXYÛ›ÜÝXÈÚ[›™[KHHÝÙˆHØ[YIÜÂˆ
ˆ\œ›Üˆ]È™\Ü›ÝYÚ][™HÝXˆÚ[[H\ØØ\™È[Ùˆ[Kˆ
‹Â‹Êˆ›Û‹\Ý]XÈÛÈ›XÚÝÜÈHY™™\š[™ÈXÚ\Ú[ÛˆXÝX[HÛÚËHØ[YH™X\ÛÛš[™È\Âˆ
ˆÙWÙY\›ÛWÙ›\Ú\ÎˆHÝ]XÈÛÝ[[›[™H]Ø^H[™X]™H›È]šY[˜ÙHÚXÚ[ÙHH[ˆ\ÙYˆ
‹Âš[ÙWÛÙ×Ù›\ÚÙXXÚHÂ›ÚYÙWÛÙ×Ù›\ÚÛ›ÝÊ›ÚY
NÂ‚‹ÊˆH™›\Ú\™HØ\ÈHœ˜[YH˜]K[™Ø][™ÈHØ[\œÈÛÝ[]™H™Y[ˆHÜ›Û™Èš^‚ˆ
‚ˆ
ˆLMˆXÛÛ\Ø[Ú]\È[›™[›ÝYÚ\È[˜Ý[Ûˆ[™]™\žHÛ™HÙˆ[H›Ü˜ÙYH›\Ú‚ˆ
ˆYX\Ý\™YÛˆ\È›ÞH›\ÚYÝÝ][™HÛÜÝÈX›Ý]\ÈÚ[ˆÝÝ]\È™Y\™XÝYÈBˆ
ˆš[KÛÈHŒKŒ[™\ÈH˜Z[ˆ[ˆ[Z]ÈÛÜÝ›ÝYÚHÙXÛÛ™ÈÙˆHL\ÙXÛÛ™[‹ˆÚ]ˆ
ˆÝÝ]\ØØ\™Y[\™[HHØ[YH[ˆÙ\ÈLœÈYØZ[œÝN‚ˆ
‚ˆ
ˆHØš[Ý\È™\ÜÛœÙHKH]XXÚÚ]HXYÛ›ÜÝXÈ™Z[™]ÈÝÛˆ[ˆØ]HKH›ÝÜÈ]Ø^Bˆ
ˆ[™›Ü›X][ÛˆÈ^HÜYY[™HÛÛ[Y[X›Ý™HØ^\ÈÚH]\ÈH˜Y˜YH\™Nˆ\È\ÈBˆ
ˆXÛÛ\	ÜÈÕÓˆ\œ›ÜˆÚ[›™[[™HÝÙˆHØ[YIÜÈ˜Z[\™H]È™\Ü›ÝYÚ]ˆHÛÜÝˆ
ˆØ\È™]™\ˆHQTÔÐQÑTË]Ø\ÈH›\ÚˆÛÎˆÙY\]™\žH[™KÝÜÞ[˜Ú[™ÈY\ˆXXÚÛ™K‚ˆ
‚ˆ
ˆÝÝ]\ÈÚ]™[ˆH™X[Y™™\ˆ[™›\ÚY]^][œÝXYˆÝ]]Ý[\œš]™\ËÝ[[‚ˆ
ˆÜ™\‹[™H›Ü›X[[ˆ^\È›ÜˆH[™[ÙˆÜš]\È˜]\ˆ[ˆÚ^Y[ˆ[™™Y‚ˆ
‚ˆ
ˆH›\Ú^\ÝY›ÜˆH™X\ÛÛ‹ÛÈ]\ÈÝ[]˜Z[X›KˆH\™Ü˜\ÚØ[ˆÜÙHÚ]]™\ˆÚ]Âˆ
ˆ[ˆHY™™\‹ÚXÚ\È^XÝHÚ[ˆHXYÈÚ[›™[X]\œÈ[ÜÝKHÑU—ÓÑÑ“TÒLH™\ÝÜ™\Âˆ
ˆ\‹[[™H›\Ú[™È›ÜˆÚ\Ú[™ÈH[™ÈÜˆH˜][ˆY˜][[™È]Ñ‘ˆ\ÈHšYÚØ^H›Ý[™ˆ
ˆ™XØ]\ÙH[ˆ[œ™\›ÙXÚX›HÜ˜\Ú\È˜\™H[™HÞÛÝÙÝÛˆ\È]™\žHÚ[™ÛH[‹]HÚÚXÙBˆ
ˆ\ÈÈÝ^H]˜Z[X›HÜˆ\È™XÛÛY\ÈHš^]ÛÜÝÈÛÛY[Û™HH^H]\‹‚ˆ
‹ÂœÝ]XÈ›ÚYÙWÛÙ×ÜÙ]\
›ÚY
BžÂˆÝ]XÈ[Û™HHÂˆYˆ
Û™JHÈ™]\›ŽÈBˆÛ™HHNÂˆÂˆÛÛœÝÚ\ˆ
™HHÙ][Š‘ÑU—ÓÑÑ“TÒŠNÂˆÙWÛÙ×Ù›\ÚÙXXÚH
HOH•S	‰ˆ
™HOH	ÌIÊNÂˆBˆYˆ
YÙWÛÙ×Ù›\ÚÙXXÚ
HÂˆÊˆÐŽˆX›Ý]›ÜHÙˆ\È›Ú™XÝ	ÜÈÛ™Ù\ˆXYÛ›ÜÝXÈ[™\È\ˆÜš]Kˆ[ØØ]YžBˆ
ˆHÔ•˜]\ˆ[ˆHÝ]XÈÙˆÝ\œËÛÈ›Ý[™È\™H\ÈÈÝ]]™H^]

Kˆ
‹ÂˆÙ]˜YŠÝÝ]•SÒSÑ‘‹
ˆL
NÂˆÊˆY™™\™YÝ]]]\È™]™\ˆ›\ÚY\ÈÝ]]]Ø\È›ÝÛˆ]Ø^K[™H[ˆ]ˆ
ˆ[™ÈžH^]

H˜]\ˆ[ˆžH™]\›š[™Èœ›ÛHXZ[ˆ\ÈH›Ü›X[Ø\ÙH\™Bˆ
ˆ
ÑU—ÑVUÑ”SQJKˆ™YÚ\Ý\š[™ÈH›\Ú\ÈÚ]XZÙ\ÈšÙY\]™\žH[™HˆYKˆ
‹Âˆ]^]
ÙWÛÙ×Ù›\ÚÛ›ÝÊNÂˆBŸB‚›ÚYÙWÛÙ×Ù›\ÚÛ›ÝÊ›ÚY
BžÂˆ™›\Ú
ÝÝ]
NÂŸB‚‹ÊˆÑU—ÓÐQPÑHKHH\‹[[Ù[\ÜÙ]Ú]\‹Ù™ˆžHY˜][‚ˆ
‚ˆ
ˆ\Ý[˜Ýœ›ÛHHY™™\š[™ÈX›Ý™K[™ÛÜÙY\[™È\Ý[˜ÝˆY™™\š[™ÈXYHHÙÈÒPTÂˆ
ˆ\ÈXZÙ\È]ÒÔ•ˆ^HÛÛ™HY™™\™[›Ø›[\È[™™Z]\ˆ™\XÙ\ÈHÝ\ŽˆHÚX\ÙÂˆ
ˆÝ[\šY\ÈHÛ™H[™H[ÝHØ\™HX›Ý][™\ˆÚ^Y[ˆ[™™Y[ÝHÈ›Ý[™HÚÜÙÈ]ˆ
ˆÞ[˜ÙYY\ˆ]™\žH[™HÛÝ[Ý[ÛÜÝHœ˜[YH˜]K‚ˆ
‚ˆ
ˆÚ]]ÛÝ™\œÈ\ÈÛ™HØ]YÛÜžHKHXYÛ›ÜÝXÜÈ[Z]YÛ˜ÙH\ˆ[Ù[Üˆ\ˆ\ÜÙ]\È]ØYÎ‚ˆ
ˆ[Ù[ÛÛ‹ÝØ\^›ÝË[Ù[^[š]ËÙ^ÙÙ^ØØ[KÙÙÜËÙÙ›ÛÜQSTšYËˆ
ˆ™ÓØY[™™ÙÙˆÚ^Y[ˆØ[Ú]\Ë‚ˆ
‚ˆ
ˆQPTÕT‘Q›Ý\Ý[X]YˆHLYœ˜[YH˜Z[ˆ[ˆÛÙ\Èœ›ÛHKŒˆ[™\ÈÈKMˆKHˆ[™\Ëˆ
ˆX›Ý]ÎIKˆ[ˆX\›Y\ˆ˜YÙˆ\ÈÛÛ[Y[ÝY\ÜÙY˜X›Ý]KLÙˆŒKŒˆ™Y›Ü™H[ž[Û™Bˆ
ˆÛÝ[YÚXÚÛÝ[]™H™Y[ˆH™YY›ÛÝ™\œÝ][Y[Ú][™È[ˆH™YH\ÈØÝ[Y[][Û‹‚ˆ
ˆH™\ÝÙˆHÙÈ\ÈÙ[Z[™[H˜\šYYˆÜ[ËÛÜœË›ÛÝÝ\ËH[›È™XÛÜ™È[™Bˆ
ˆ\š[ÙXÈ[[YHÙ[œÝ\Ù\ËXXÚH[™[Ùˆ[™\Èœ›ÛHHY™™\™[XÙKÚ]›ÈÚ[™ÛBˆ
ˆØ]YÛÜžHYÛÜØ][™Ë‚ˆ
‚ˆ
ˆHœ˜[YK\˜]HY™™XÝ\È“ÕYX\Ý\˜X›HÛˆ\È›Þ[™›ÈšYÝ\™H\ÈÛZ[YY›Üˆ]ˆÚ]ˆ
ˆÝÝ][™XYHY™™\™Y\ÙH[™\ÈÛÜÝ[[ÜÝ›Ý[™Ë[™H[‹]Ë\[ˆÜ™XY\™H\Âˆ
ˆ\™Ù\ˆ[ˆ[žHØZ[ˆKHÛÈY[XØ[ÛÛ™šYÝ\˜][ÛœÈYX\Ý\™YŒÈ[™MHœËˆ\ÈØ]HXZÙ\Âˆ
ˆHÙÈÒÔ•ÚXÚ\ÈH™XYXš[]HÚ[ŽÈHÔQQØ[YHœ›ÛHHY™™\š[™ÈX›Ý™Kˆ™\Ù[[™Âˆ
ˆH›Ú\ÞH[H\ÈHÜYY\\ÈÝÈHXÙX›ÈÙ]ÈÛÛ[Z]Y‚ˆ
‚ˆ
ˆU[X™\˜][HÙ\È›ÝÛÝ™\ˆ\œ›Üˆ]ËˆÜÔÞ[˜Ôš[ˆ\ÈLMˆØ[Ú]\È[™[ÜÝÙˆ[Bˆ
ˆ\™HHXÛÛ\™\Ü[™È]ÛÛY][™ÈÙ[Ü›Û™ÎÈØ][™ÈÜÙHÚÛ\Ø[H\ÈÝÈH˜Z[\™Bˆ
ˆ™XÛÛY\È[š\ÚX›KˆÛ›HHÚ]\È]™\ÜÕPÐÑTÔÑ•S“ÕUS‘HÛÜšÈ\™HÜ˜\Y[™XXÚˆ
ˆÛ™HØ\ÈXÚÙYžH™XY[™È]˜]\ˆ[ˆžHX]Ú[™ÈH™Yš^‚ˆ
‹Âš[ÙTÜØY˜XÙJ›ÚY
BžÂˆÝ]XÈ[ÛˆHLNÂˆYˆ
Ûˆ
HÈÛÛœÝÚ\ˆ
™HHÙ][Š‘ÑU—ÓÐQPÑHŠNÈÛˆH
HOH•S	‰ˆ
™HOH	ÌIÊNÈBˆ™]\›ˆÛŽÂŸB‚›ÚYÜÔÞ[˜Ôš[ŠÛÛœÝÚ\ˆ
™›]‹‹ŠBžÂˆ˜WÛ\Ý\ÂˆÙWÛÙ×ÜÙ]\

NÂˆ˜WÜÝ\
\›]
NÂˆœš[Š›]\
NÂˆ˜WÙ[™
\
NÂˆYˆ
ÙWÛÙ×Ù›\ÚÙXXÚ
HÈ™›\Ú
ÝÝ]
NÈBŸB‚‹ÊˆKKH×Ú][WÙ[šY\ÈÛÜœ\[ÛˆØ[˜\žHKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKBˆ
ˆHØ\Ý[›È˜][È™XY[™È×Ú][WÙ[šY\ÖÚXYKšXY\ˆÚ][ˆY™\ÜÈÚÜÙBˆ
ˆÕÈÛÜ™\ÈÛÜœ™XÝ[™ÚÜÙHQÒÛÜ™ÛÈHÛX[[YÙ\‹ˆ]\ÈBˆ
ˆÚYÛ˜]\™HÙˆHÌ‹Xš][Ù™œÙ]Üš]H[™[™ÈÛˆH\\ˆ[ˆÙˆÚ]\È›ÝÂˆ
ˆHXš]Ú[\ˆKHYÈ˜[Z[HÌÈ
Ø\ÝX˜\ÙY^[Ý]ÛÛ˜XÝ
KˆØØ[›š[™ÈHÚÛBˆ
ˆX›H]H™]ÈÚXÚÜÚ[È\›œÈœÛÛY][™ÈÛÜœ\È]]™[X[Hˆ[ÈH˜[YYˆ
ˆØ[Ú]KHØ[YHØ^HH×Ó[Ù[][šY\ÈÝšYHYÈØ\È[›™Yˆ
‹Â›ÚYÙTÜÚXÚÒ][Q[šY\ÊÛÛœÝÚ\ˆ
Ú\™JBžÂˆÊˆØØ[Z\œ›ÜˆÙˆÚ“[Ù[š[T™XÛÜ™KHÜÜÝ\Ü˜È[X™\˜][HÙ\È›Ý[ˆ
ˆ[ˆHØ[YHXY\œËˆÛ›HHÛÈXY[™ÈÚ[\œÈX]\ˆ\™NÈH˜Z[[™Âˆ
ˆ›Ø]ËÙ›YÜÈYH™XÛÜ™È]È˜]\˜[Ì‹Xž]HXš]Ú^™Kˆ
‹ÂˆÝXÝÙWÚ][WÜ™XÈÈ›ÚY
šXY\ŽÈÚ\ˆ
™š[[˜[YNÈ›Ø]ØØ[KÝŽÂˆ[œÚYÛ™YÚ\ˆ\ÓX[K\ÒXYYKYŽÈNÂˆ^\›ˆÝXÝÙWÚ][WÜ™XÈ×Ú][WÙ[šY\Ö×NÂˆ[NÂˆ›Üˆ
HHÈHÈJÊÊHÂˆZ[—ÝH
Z[—Ý
H×Ú][WÙ[šY\ÖÚWKšXY\ŽÂˆZ[—ÝˆH
Z[—Ý
H×Ú][WÙ[šY\ÖÚWK™š[[˜[YNÂˆYˆ
OH	‰ˆ
ˆÌŠHOHJHÂˆÜÔÞ[˜Ôš[Š–ÙÙ]—HUSHS•–HÓÔ”•T	\ÎˆÉYKšXY\I\
YÚL	[
Wˆ‹ˆÚ\™KK
›ÚY
ŠH
[œÚYÛ™YÛ™ÊH
ˆÌŠJNÂˆ™]\›ŽÂˆBˆYˆ
ˆOH	‰ˆ
ˆˆÌŠHOHJHÂˆÜÔÞ[˜Ôš[Š–ÙÙ]—HUSHS•–HÓÔ”•T	\ÎˆÉYK™š[[˜[YOI\
YÚL	[
Wˆ‹ˆÚ\™KK
›ÚY
ŠH‹
[œÚYÛ™YÛ™ÊH
ˆˆÌŠJNÂˆ™]\›ŽÂˆBˆBŸB‚‹ÊˆZ[\ÙXÛÛ™ÈÙˆ™X[[YHÚ[˜ÙHHš\œÝØ[ˆ\ÙYžHHÛØÚÈXYÛ›ÜÝXÈ[‚ˆ
ˆœ˜[Y][Z[™Ë˜ËÚXÚ™YYÈH[YX˜\ÙH]\ÈYš[š][H›ÝHØ[YIÜÈÝÛ‹ˆ
‹Â[œÚYÛ™Y[ÙTÜÜÝZ[\Ê›ÚY
BžÂˆÝ]XÈZ[ÜšYÚ[ˆHÂˆÝ]XÈÝX›Hœ™\HHŒÂ‚ˆYˆ
œ™\HOHŒ
HÂˆœ™\HH
ÝX›JHÑÑÙ]\™›Ü›X[˜ÙQœ™\]Y[˜ÞJ
NÂˆÜšYÚ[ˆHÑÑÙ]\™›Ü›X[˜ÙPÛÝ[\Š
NÂˆYˆ
œ™\HHŒ
HÈœ™\HHKŒÈBˆBˆ™]\›ˆ
[œÚYÛ™Y[
H


ÝX›JH
ÑÑÙ]\™›Ü›X[˜ÙPÛÝ[\Š
HHÜšYÚ[ŠHÈœ™\JH
ˆLŒ
NÂŸB