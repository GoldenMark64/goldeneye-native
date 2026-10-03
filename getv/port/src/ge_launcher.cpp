/* The launcher window.
 *
 * What this is. A window that opens before the game, collects settings, and then starts the
 * game with them. It is not new engine capability: the mod surface is already about 275
 * GETV_* environment gates plus goldeneye.cfg, and this is a user interface over that
 * surface. Every control below resolves to an environment variable that already existed and
 * already worked from a shell.
 *
 * ---------------------------------------------------------------- why it re-execs
 *
 * The one measured fact that decides this file's shape: **76 of those gates are read once
 * into a static on first use** -- the `static int x = -1; if (x == -1) x = getenv(...)`
 * pattern, counted across getv/port. A setting changed after the game has started therefore
 * does nothing, silently, for most of the surface. An in-process launcher that toggled
 * options and then continued into SDL_main() would appear to work and would be wrong for
 * every gate that had already been touched.
 *
 * So the launcher sets the environment and execv()s the binary again with --launcher
 * removed. The game then starts in a pristine process where nothing has read anything yet,
 * which is the only arrangement where every gate is guaranteed to take effect. It also makes
 * "change a setting and relaunch" exactly as correct as starting from a shell, because it
 * *is* starting from a shell.
 *
 * A second reason, less obvious and just as decisive: the launcher creates its own SDL
 * window, GL context and ImGui context. The game's renderer creates its own later, and
 * gfx_sdl2.c assumes it is the one initialising SDL video. Handing a used SDL over to it
 * would be a source of subtle, platform-specific breakage for no benefit.
 *
 * ---------------------------------------------------------------- what it does not do
 *
 * It does not write goldeneye.cfg. The file stays the user's, edited by hand or by
 * --write-config, and the launcher composes a run on top of whatever it says. Every control
 * starts from the value already in the environment, which is the value the config layer just
 * resolved -- so the launcher opens showing the current configuration rather than a set of
 * defaults that disagree with it.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "ge_launcher_policy.h"
#include "ge_developer_tools.h"
#include <time.h>
/* The headers ge_lua.c uses to walk the mods directory, and for the same reason: the launcher
 * has to discover exactly what the loader would.
 *
 * <sys/types.h> and the explicit <sys/stat.h> are the Mac's fix -- MinGW pulls `struct stat`
 * in transitively and Clang does not, so this compiled here and failed there. <dirent.h>
 * stays UNCONDITIONAL rather than moving into the non-Windows branch: mod_scan() calls
 * opendir/readdir on every platform, MinGW ships the header, and putting it behind #else
 * trades a Clang break for a MinGW one. Build-tests one platform, ships three -- in both
 * directions. */
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include "ge_actions.h"
#include "ge_bindings.h"
#include "ge_config.h"

/* The real errno. getv/port/include/ge_win_compat.h undefines errno on Windows so that
 * PR/os.h's struct fields of that name can parse; MSVCRT exposes the value through
 * _errno(). Spelled once here rather than at each use. */
#if defined(_WIN32)
#define ge_errno (*_errno())
#else
#define ge_errno errno
#endif


#if defined(GE_WITH_IMGUI)

#include <SDL2/SDL.h>
#if defined(_WIN32)
#include <windows.h>
#include <process.h>   /* _execv */
#else
#include <unistd.h>
#endif

/* Same icon as the game window, from the same pixels. See ge_icon_apply.c.
 *
 * Declared outside the _WIN32 split, not inside its #else. ge_icon_apply.c builds on every
 * desktop platform and the call site below is guarded by GE_PLATFORM_DESKTOP, so keeping the
 * declaration on the non-Windows branch alone left Windows compiling the call with nothing in
 * scope -- "gePortSetWindowIcon was not declared in this scope; did you mean SDL_SetWindowIcon".
 * The platform split above is about which system headers to pull in, which this is not. */
#if defined(__APPLE__) && defined(GE_PLATFORM_MAC)
#include "../mac/ge_renderer_choice.h"
#else
static bool geAppRendererActive(void) { return false; }
#endif

extern "C" void gePortSetWindowIcon(SDL_Window *w);

#ifdef RAPI_METAL
/* The launcher's own window uses its own standalone Metal context
 * (ge_launcher_metal.mm), NOT gfx_metal.mm's game-window state -- that does not exist yet
 * when the launcher runs (main()/SDL_main() calls gePortLauncherRun() before gfx_init()). */
#include "ge_launcher_metal.h"
#endif

/* Windows needs an extension loader before any GL header. opengl32.dll exports GL 1.1 and
 * nothing later, so glGetVertexAttribiv and everything else past 1997 resolves only through
 * GLEW -- without this the link fails on __imp_glGetVertexAttribiv, which reads like a
 * missing DLL import and is really a missing loader. gfx_opengl.c does exactly the same on
 * __MINGW32__; macOS and Linux need no equivalent. */
#if defined(_WIN32)
#define GLEW_STATIC
#include <GL/glew.h>
#endif

/* No GL on this target (RAPI_METAL). imgui.h/imgui_impl_sdl2.h stay unconditional -- the
 * UI-drawing helper functions throughout this file (ge_load_fonts, ge_apply_style, the page
 * renderers) use plain ImGui:: calls with no renderer dependency, and imgui_impl_sdl2.h is the
 * SDL PLATFORM backend (events/input), shared across GL and Metal. Only the OpenGL2 RENDERER
 * backend and the raw GL headers are GL-specific -- see gePortLauncherRun()'s own #ifdef
 * RAPI_METAL / #else split for why its body does not need them under RAPI_METAL either. */
#ifndef RAPI_METAL
#if defined(USE_GLES)
#include <SDL2/SDL_opengles2.h>
#else
#include <SDL2/SDL_opengl.h>
#endif
#endif /* RAPI_METAL */

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#ifndef RAPI_METAL
#include "imgui_impl_opengl2.h"
#endif

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#include <TargetConditionals.h>
#endif

namespace {

/* Defined further down with the exec helpers, used well above it to resolve a relative mods
 * path against the binary's own directory. Declared at the top of the namespace rather than
 * immediately before its first use: the mid-file form compiled under MinGW and Clang
 * rejected it. */
bool self_path(char *out, size_t n);

/* ---------------------------------------------------------------- the model
 *
 * Plain values, seeded from the environment, written back to the environment on launch.
 * Nothing here holds a default of its own: a default that disagreed with ge_config.c's would
 * be a second source of truth, and the first symptom would be the launcher quietly undoing a
 * setting from the config file. */

int   env_int(const char *k, int fallback)
{
    const char *v = getenv(k);
    if (v == NULL || *v == '\0') return fallback;
    return atoi(v);
}

bool  env_bool(const char *k, bool fallback)
{
    const char *v = getenv(k);
    if (v == NULL || *v == '\0') return fallback;
    return (*v != '0');
}

void  env_str(const char *k, char *dst, size_t n, const char *fallback)
{
    const char *v = getenv(k);
    snprintf(dst, n, "%s", (v && *v) ? v : fallback);
}

void  put_int(const char *k, int v) { char b[32]; snprintf(b, sizeof b, "%d", v); setenv(k, b, 1); }
void  put_str(const char *k, const char *v)
{
    if (v && *v) setenv(k, v, 1);
    else         unsetenv(k);
}

/* The stage list. Ids and names are from the project's own stage reference table, the
 * ground truth for which stages are solo, multiplayer-only or have no data at all. Only
 * loadable stages are offered: eleven ids can never load (nine cut, plus CITADEL, whose
 * background exists but whose setup file does not), and offering them would be offering a
 * hang. MP-only stages are marked because selecting one solo loads geometry with no setup,
 * which looks like a rendering bug and is not one. */
/* Ordered as the campaign is played, not by stage id. The id order is an artefact of the
 * ROM and means nothing to a player: "Bunker 1, Silo, Statue, Control..." is not a sequence
 * anyone recognises, while "01 Dam, 02 Facility, 03 Runway..." is the game people remember.
 * The mission number and the theatre it belongs to come from the game's own briefings, and
 * they are what make the list scannable -- the id is an implementation detail and is not
 * shown at all. `mission` is 0 for the multiplayer-only stages, which have no campaign slot
 * and are grouped separately in the UI for that reason. */
struct Stage { int id; const char *name; const char *place; int mission; bool mp_only; };
const Stage kStages[] = {
    { 33, "Dam",        "Arkangelsk",     1,  false },
    { 34, "Facility",   "Arkangelsk",     2,  false },
    { 35, "Runway",     "Arkangelsk",     3,  false },
    { 36, "Surface",    "Severnaya",      4,  false },
    {  9, "Bunker 1",   "Severnaya",      5,  false },
    { 20, "Silo",       "Kirghizstan",    6,  false },
    { 26, "Frigate",    "Monte Carlo",    7,  false },
    { 43, "Surface 2",  "Severnaya",      8,  false },
    { 27, "Bunker 2",   "Severnaya",      9,  false },
    { 22, "Statue",     "St Petersburg",  10, false },
    { 24, "Archives",   "St Petersburg",  11, false },
    { 29, "Streets",    "St Petersburg",  12, false },
    { 30, "Depot",      "St Petersburg",  13, false },
    { 25, "Train",      "St Petersburg",  14, false },
    { 37, "Jungle",     "Cuba",           15, false },
    { 23, "Control",    "Cuba",           16, false },
    { 39, "Caverns",    "Cuba",           17, false },
    { 41, "Cradle",     "Cuba",           18, false },
    { 28, "Aztec",      "Bonus",          19, false },
    { 32, "Egypt",      "Bonus",          20, false },
    { 31, "Complex",    "Multiplayer",    0,  true  },
    { 38, "Temple",     "Multiplayer",    0,  true  },
    { 45, "Basement",   "Multiplayer",    0,  true  },
    { 46, "Stack",      "Multiplayer",    0,  true  },
    { 48, "Library",    "Multiplayer",    0,  true  },
    { 50, "Caves",      "Multiplayer",    0,  true  },
};
const int kStageCount = (int)(sizeof kStages / sizeof kStages[0]);

/* Mirrors GE_CHEATS in ge_config.c. `live` there means the cheat has a real cheatIsActive()
 * consumer, so setting the flag is enough; the others need in-game activation because their
 * effect lives in the turn-on switch, which needs a player context that does not exist at
 * startup. That distinction is surfaced in the UI rather than hidden, because a checkbox that
 * silently does nothing is worse than one that says it will not apply yet. */
/* `name` is the token GETV_CHEATS is built from and must not change; `label` is what the
 * launcher shows. They were the same string until the UI grew up, and "10x_health" in a
 * settings list reads as a debug symbol rather than as a cheat anyone recognises. */
struct Cheat { const char *name; const char *label; bool live; };
const Cheat kCheats[] = {
    { "invincibility", "Invincibility",   false },
    { "all_guns",      "All guns",        false },
    { "max_ammo",      "Max ammo",        false },
    { "infinite_ammo", "Infinite ammo",   true  },
    { "dk_mode",       "DK mode",         true  },
    { "paintball",     "Paintball mode",  true  },
    { "no_radar",      "No radar",        true  },
    { "enemy_rockets", "Enemy rockets",   true  },
    { "invisibility",  "Invisibility",    false },
    { "tiny_bond",     "Tiny Bond",       false },
    { "golden_gun",    "Golden gun",      false },
    { "magnum",        "Magnum",          false },
    { "laser",         "Laser",           false },
    { "turbo_mode",    "Turbo mode",      false },
    { "10x_health",    "10x health",      false },
    { "2x_armor",      "2x armour",       false },
    { "extra_weapons", "Extra weapons",   false },
    { "fast_animation","Fast animation",  false },
};
const int kCheatCount = (int)(sizeof kCheats / sizeof kCheats[0]);

const char *kRulesets[] = { "classic", "hardcore", "survival", "chaos", "horde" };
const int   kRulesetCount = 5;

/* Matches GE_LUA_MAX_MODS in ge_lua.c. Listing more here than the loader will accept would
 * offer mods that silently never load. */
#define GE_MAX_MODS 32

/* The bindable actions and the pad sources, taken from ge_actions.h.
 *
 * These used to be two hand-written string tables here, with a comment explaining that
 * the launcher "never links the input layer". That was not true -- ge_launcher.cpp is
 * compiled into the same binary as ge_bindings.c -- and the duplication was already
 * stale in practice: the six actions and eleven sources listed here had to be edited in
 * lockstep with port_os.c, ge_config.c and two launcher UIs, with nothing to catch a
 * list that had only been updated in three of the four.
 *
 * Generating them from GE_ACTION_LIST / GE_SOURCE_LIST removes the possibility. The
 * DEFAULT is likewise no longer a repeated string: it comes from gePresetSource(), so
 * the UI shows what the selected preset will actually do rather than what somebody
 * wrote down once.
 */
struct BindAction { const char *label; const char *key; };
const BindAction kActions[] = {
#define M(id, lo, up) { NULL, up },
    GE_ACTION_LIST(M)
#undef M
};
const int kActionCount = (int)(sizeof kActions / sizeof kActions[0]);

/* Display names. A switch rather than a table so a missing case is a compile-time
 * -Wswitch rather than a blank row, and so the ordinal coupling that made the old
 * parallel arrays fragile does not come back through the side door. */
static const char *ActionLabel(int a)
{
    switch (a) {
        case GE_ACT_FIRE:        return "Fire";
        case GE_ACT_AIM:         return "Aim";
        case GE_ACT_USE:         return "Use / interact";
        case GE_ACT_RELOAD:      return "Reload";
        case GE_ACT_CROUCH:      return "Crouch";
        case GE_ACT_WEAPON_NEXT: return "Next weapon";
        case GE_ACT_WEAPON_PREV: return "Prev weapon";
        case GE_ACT_PAUSE:       return "Pause";
        default:                 return "?";
    }
}

static const char *AxisLabel(int x)
{
    switch (x) {
        case GE_AXIS_FORWARD:      return "Forward";
        case GE_AXIS_BACKWARD:     return "Back";
        case GE_AXIS_STRAFE_LEFT:  return "Strafe left";
        case GE_AXIS_STRAFE_RIGHT: return "Strafe right";
        case GE_AXIS_LOOK_UP:      return "Look up";
        case GE_AXIS_LOOK_DOWN:    return "Look down";
        case GE_AXIS_LOOK_LEFT:    return "Look left";
        case GE_AXIS_LOOK_RIGHT:   return "Look right";
        default:                   return "?";
    }
}

/* Positional, matching GE_SOURCE_LIST. Names are what the player types in
 * goldeneye.cfg, so they are shown verbatim rather than prettified -- "lt" here and
 * "lt" in the file is the whole point. */
const char *kSources[] = {
#define M(id, lo) lo.‹­¦ëm®éÜj×¢¸ Šv¥jšk£¦j×­¢G§r‹§·Ÿ9ÓhÛÛœİÚ\ˆ
œİ]HHK›[ÙÛÛ–ÚWHÈ‘SP“Qˆˆ‘TĞP“QÂˆ›Ø]İÈH^ÕÚY
×Ù”ÛX[L‹Œ‹İ]KK™ŠNÂˆ^Ê×Ù”ÛX[L‹Œ‹[U™XÌŠ
È]ÈHİÈHMH
È
ˆYˆHËŒŠKˆK›[ÙÛÛ–ÚWHÈÑÛÛˆÑ[Kİ]KK™ŠNÂ‚ˆ[QİZN”Ù]İ\œÛÜ”ØÜ™Y[”ÜÊ[U™XÌŠH
È
È‹ŒŠJNÂˆ[QİZN‘[[^J[U™XÌŠ
JNÂˆB‚ˆ[QİZN‘[[^J[U™XÌŠL
JNÂˆ[
‘\ØX›Y[ÙÈ\™H™XÛÜ™YH˜[YKÛÈH[ÙYYÈH›Û\ˆ‚ˆ›]\ˆ\È[˜X›YHY˜][˜]\ˆ[ˆÚ[[HYÛ›Ü™YˆŠNÂˆBˆB‚ˆ[ÙHYˆ
YÙHOHÊHÂˆİ]XÈÛÛœİÚ\ˆ
››İXÙHHˆÂˆÙXİ[ÛŠ”‘PÓÔ‘“ÔTĞQÑHŠNÂˆ[QİZNÚXÚØ›Ş
”™XÛÜ™[[Y]H›Üˆ\È][˜Ú‹	›Kœ™XÛÜ™İ[[Y]JNÂˆ[
“YX\İ\™HØØİ\YYØš™XİÛİËXZÈ\ØYÙH[™[ØØ][Ûˆ˜Z[\™\Ëˆ‚ˆ”Ø]™\ÈHØØ[”ÓÓ“™\Ü[™H™XYX›Hİ[[X\Kˆ›İ[™È\È\ØYYˆŠNÂˆÛÛœİÚ\ˆ
™\˜][ÛœÖ×HHÈŒËŒœ˜[Y\È
ŒHZ[]H]Œ”ÊH‹ˆŒLœ˜[Y\È
ŒÈZ[]\È]Œ”ÊH‹ŒNœ˜[Y\È
HZ[]\È]Œ”ÊHˆNÂˆ[\˜][ÛˆHKœ™XÛÜ™Ùœ˜[Y\ÈOHÍŒÈˆKœ™XÛÜ™Ùœ˜[Y\ÈOHNÈˆˆNÂˆ[QİZN™YÚ[‘\ØX›Y
[Kœ™XÛÜ™İ[[Y]JNÂˆ[QİZN”Ù]™^][UÚY
ÍŒ
NÂˆYˆ
[QİZNÛÛX›Ê”™XÛÜ™[™È[™İ‹	™\˜][Û‹\˜][ÛœËÊJBˆKœ™XÛÜ™Ùœ˜[Y\ÈH\˜][ÛˆOHÈÍŒˆ\˜][ÛˆOHˆÈNˆLÂˆ[QİZN‘[™\ØX›Y

NÂˆ[
•HØ[YHÛÜÙ\È]]ÛX]XØ[H]Hœ˜[YH[Z]ˆÙ^X›Ø\™[™[İ\ÙHİ^H‚ˆ˜Xİ]™KˆÛÜÚ[™ÈX\›HX]™\È[ˆ[˜ÛÛ\]H™\ÜˆÚÛÜÙHHZ\ÜÚ[Ûˆ™Y›Ü™Hİ\[™ËˆŠNÂˆYˆ
Š“ÔSˆ‘TÔ•È‹[U™XÌŠNÌŠK˜[ÙJJBˆ›İXÙHH]™[Ü\—ÛÜ[—Ü™\ÜÊ˜[ÙJHÈˆˆˆÛİ[›İÜ[ˆH™\ÜÈ›Û\‹ˆÂˆ[QİZN”Ø[YS[™J
NÂˆYˆ
Š“UTÕÕSSPT–H‹[U™XÌŠŒÌŠK˜[ÙJJBˆ›İXÙHH]™[Ü\—ÛÜ[—Ü™\ÜÊYJHÈˆˆˆ“›Èİ[[X\H›İ[™ÜˆHš[HÛİ[›İ™HÜ[™YˆÂˆYˆ

››İXÙJH[
›İXÙJNÂ‚ˆÙXİ[ÛŠ‘P•QÑÒS‘ÈŠNÂˆ[QİZNÚXÚØ›Ş
”ÚİÈH]™[Ü\ˆİ™\›^H[ˆØ[YH‹	›K™]—Ûİ™\›^JNÂˆ[QİZN”Ù]™^][UÚY
ŒŒ
NÂˆ[QİZN’[œ]^
ÛÛœÛÛHİÙ^H‹K˜ÛÛœÛÛWÚÙ^KÚ^™[ÙˆK˜ÛÛœÛÛWÚÙ^JNÂˆ[
‘Y˜][ˆÜ˜]™KØ˜XÚÜ][İKˆÛÜÙHHÛÛœÛÛHÈ™]\›ˆÙ^X›Ø\™[™[İ\ÙHÈHØ[YKˆŠNÂˆÛÛœİÚ\ˆ
›]™[Ö×HHÈ“Ù™ˆ‹‘]šXÙHXYÛ›ÜİXÜÈ‹‘]Z[Y[œ]XYÛ›ÜİXÜÈˆNÂˆ[QİZN”Ù]™^][UÚY

NÂˆ[QİZNÛÛX›Ê’[œ]ÙÙÚ[™È‹	›Kš[œ]ÙXYË]™[ËÊNÂˆ[QİZNÚXÚØ›Ş
“ÙÈœ˜[YHXÚ[™È‹	›KœXÙWİ˜XÙJNÂˆ[
‘XYÈÙÜÈÛÈÈİ[™\™İ]]Ù\\˜]Hœ›ÛHØ]™Y[[Y]H™\ÜËˆ‚ˆ‘]Z[YÙÙÚ[™ÈØ[ˆY™™Xİ\™›Ü›X[˜ÙKˆ\ÙHÚÚXÙ\È\HÈ\È][˜ÚˆŠNÂ‚ˆÙXİ[ÛŠ’SŠNÂˆYˆ
Š‘ÕRQHÈTHÓˆÒUPˆ‹[U™XÌŠÌŠK˜[ÙJJBˆ›İXÙHHÑÓÜ[•T“
šÎ‹ËÙÚ]X‹˜ÛÛKÜÙX‹\]›Û‹ÙÛÛ[™^YK[˜]]™KØ›Ø‹ÛXZ[‹ÙØÜËÑU‘SÔT—ÕÓÓË›YŠHOHˆÈˆˆˆÛİ[›İÜ[ˆHœ›İÜÙ\‹ˆÂˆB‚ˆ[QİZN‘[™\ØX›Y

NÂˆ[QİZN‘[™Ú[

NÂˆ[QİZN”Üİ[PÛÛÜŠ
NÂ‚ˆÊˆKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKH›Ûİ\ˆ
‹ÂˆOY™Xİš[Y
[U™XÌŠH›Ûİ\’
K[U™XÌŠË
KÔ[™[
NÂˆOY[™J[U™XÌŠH›Ûİ\’
K[U™XÌŠËH›Ûİ\’
KÓ[™KKŒŠNÂ‚ˆÂˆÊˆÚ]\ÈXİX[HX›İ]È™H][˜ÚY[ˆÛ™H[™KˆH][˜Ú\ˆÛÛ\ÜÙ\ÈBˆ
ˆ[ˆİ]Ùˆš]™HYÙ\ÎÈÚ]İ]Hİ[[X\HHÛ›HØ^HÈÚXÚÈ]\ÈÈÛÂˆ
ˆ˜XÚÈ›İYÚ]™\HYÙKˆ
‹ÂˆÚ\ˆİ[VÌM—NÂˆ[ˆHÂˆYˆ
KœXÚ×ÜİYÙJHÂˆYˆ
ÔİYÙ\ÖÛKœİYÙWÚYK›Z\ÜÚ[Ûˆˆ
Bˆˆ
ÏHÛœš[Šİ[H
È‹Ú^™[Ùˆİ[HH‹“RTÔÒSÓˆ	L™	\È‹ˆÔİYÙ\ÖÛKœİYÙWÚYK›Z\ÜÚ[Û‹ÔİYÙ\ÖÛKœİYÙWÚYK›˜[YJNÂˆ[ÙBˆˆ
ÏHÛœš[Šİ[H
È‹Ú^™[Ùˆİ[HH‹T‘SH	\È‹ˆÔİYÙ\ÖÛKœİYÙWÚYK›˜[YJNÂˆH[ÙHÂˆˆ
ÏHÛœš[Šİ[H
È‹Ú^™[Ùˆİ[HH‹•UHĞÔ‘QSˆŠNÂˆBˆˆ
ÏHÛœš[Šİ[H
È‹Ú^™[Ùˆİ[HH‹ˆÈ	\È‹ˆKœ›Ùš[HÈ‘ÓÓS‘VQJÈˆˆTÑHĞSQHŠNÂˆˆ
ÏHÛœš[Šİ[H
È‹Ú^™[Ùˆİ[HH‹ˆÈ	\È‹ˆKœœ×Øİ\İÛHÈÕTÕÓHĞSQTVHˆˆÔ[\Ù]ÖÛKœ[\Ù]JNÂˆYˆ
KšÜ™JHˆ
ÏHÛœš[Šİ[H
È‹Ú^™[Ùˆİ[HH‹ˆÈÔ‘HŠNÂˆÂˆ[˜ÈHÂˆ›Üˆ
[HHÈHĞÚX]Ûİ[ÈJÊÊHYˆ
K˜ÚX]ÛÛ–ÚWJH˜ÊÊÎÂˆYˆ
˜ÊHÛœš[Šİ[H
È‹Ú^™[Ùˆİ[HH‹ˆÈ	YÒPU	\È‹ˆ˜Ë˜ÈOHHÈˆˆˆ”ÈŠNÂˆBˆ›Üˆ
Ú\ˆ
œHHİ[NÈ
œNÈJÊÊH
œHH
Ú\ŠHİ\\Š
[œÚYÛ™YÚ\ŠH
œJNÂˆ^Ê×Ù”ÛX[LËŒ‹[U™XÌŠÍH›Ûİ\’
ÈÌJKÑ[Kİ[KK™ŠNÂˆB‚ˆ[QİZN”Ù]İ\œÛÜ”ØÜ™Y[”ÜÊ[U™XÌŠÈHÍNH›Ûİ\’
ÈŒJJNÂˆYˆ
Š”URU‹[U™XÌŠLŒÍŠK˜[ÙJJHÈ[›š[™ÈH˜[ÙNÈBˆ[QİZN”Ù]İ\œÛÜ”ØÜ™Y[”ÜÊ[U™XÌŠÈHŒŒ‹H›Ûİ\’
ÈŒJJNÂˆÚYˆYš[™Y
×ĞTW×ÊH	‰ˆYš[™Y
ÑWÔU“Ô“WÓPPÊBˆ[QİZN™YÚ[‘\ØX›Y
ÙP\™[™\™\Xİ]™J
H	‰ˆYÙP\™[™\™\]˜Z[X›JÙP\™[™\™\”Ù[XİY

JJNÂˆÙ[™Y‚ˆYˆ
ŠKœXÚ×ÜİYÙHÈ”ÕT•RTÔÒSÓˆˆˆ”ÕT•ĞSQH‹[U™XÌŠNÍŠKYJJHÂˆYˆ
]™[Ü\—Ü™\\™WÜ™XÛÜ™[™ÊJJHÈ][˜ÚHYNÈ[›š[™ÈH˜[ÙNÈBˆ[ÙHÑÔÚİÔÚ[\SY\ÜØYÙP›Ş
ÑÓQTÔĞQÑP“ÖÑT”“Ô‹Ø[››İİ\™XÛÜ™[™È‹ˆ•H™\ÜÈ\™XİÜHÛİ[›İ™H™\\™Yˆ\›ˆ™XÛÜ™[™ÈÙ™ˆÜˆÚXÚÈ›Û\ˆXØÙ\ÜËˆ‹Ú[ŠNÂˆB‚ˆÚYˆYš[™Y
×ĞTW×ÊH	‰ˆYš[™Y
ÑWÔU“Ô“WÓPPÊBˆ[QİZN‘[™\ØX›Y

NÂˆÙ[™Y‚ˆ[QİZN‘[™

NÂˆB‚ˆ[QİZN”™[™\Š
NÂˆÚY™YˆTWÓQUSˆÙS][˜Ú\“Y][™[™\[™™\Ù[
[QİZN‘Ù]˜]Ñ]J
JNÂˆÙ[ÙBˆÂˆÊˆHšY]ÜÜ\ÈHUĞP“HÚ^™K›İ[Ë‘\Ü^TÚ^™K‚ˆ
‚ˆ
ˆÚ]ÑÕÒS‘Õ×ĞSÕ×ÒQÒHÜÙHÛÈY™™\ˆHH\Ü^IÜÈØØ[H˜XİÜ‚ˆ
ˆ[Ë‘\Ü^TÚ^™H\ÈÙÚXØ[
Ú]ÑÑÙ]Ú[™İÔÚ^™H™\ÜÊHÚ[HHœ˜[YXY™™\‚ˆ
ˆ\È\ÚXØ[ˆ\ÈXXÚ[™IÜÈ[™[[œÈ]ML	KÛÈHLLŒÎÚ[™İÈ\ÈBˆ
ˆMLMÌ˜]ØX›K[™šY]ÜÜ[™ÈÈHÙÚXØ[Ú^™H™]ÈHÚÛHRH[Âˆ
ˆH›İÛK[Y‹ÌÈÙˆHY™™\ˆKHÚXÚ™XYÈ\È›ÛÛYY[ˆ[™İ]Ù™ˆ]ˆ
ˆHšYÚ‹›İ\ÈHØØ[[™ÈYË[™\È[š\ÚX›H]L	HØØ[Kˆ
‹Âˆ[ËÂˆÑÑÓÑÙ]˜]ØX›TÚ^™JÚ[‹	™Ë	™
NÂˆÛšY]ÜÜ
Ë
NÂˆÛÛX\ÛÛÜŠŒÌY‹ŒÍY‹ŒÙ‹KŒŠNÈÊˆĞ™È
‹ÂˆÛÛX\ŠÓĞÓÓÔ—Ğ•Q‘‘T—Ğ’U
NÂˆ[QİZWÒ[\Ü[‘Ó—Ô™[™\‘˜]Ñ]J[QİZN‘Ù]˜]Ñ]J
JNÂˆBˆÙ[™Y‚‚ˆÚY™YˆTWÓQUSˆÊˆÑU—ÓUSÒT—ÔÒÕ[™H^[XÛİ[[™È[ˆÙˆÑU—ÓUSÒT—Ô“Ğ‘H\™H›İˆ
ˆ[\[Y[YÛˆ\È˜XÚÙ[™
Û™XY^[È\È›ÈY][\]Z]˜[[Ú\™Y\\™Bˆ
ˆY]KHÙYHÙWÚ[YİZK˜Ü	ÜÈY[XØ[Ø\›ÜˆÑU—ÒSQÕRWÔ“Ğ‘JKˆ›Ø™WÙœ˜[Y\Èİ[ˆ
ˆÛÜÙ\ÈHÚ[™İÈY\ˆˆœ˜[Y\ËÚXÚ\ÈH[ˆÙˆHYXÚ[š\ÛH]]ÛX]Yˆ
ˆ\İ[™È™YYÈ[Üİ
›İš[™ÈHÛÜ[œÈ[™^]Ë]™[ˆÚ]İ]H^[ˆ
ˆÚXÚÊNÈ]\İØ[››İÛÛ™š\›HÚ]Ø\È˜]ÛˆØ\È›Û‹Y[\Kˆ
‹ÂˆYˆ
›Ø™WÙœ˜[Y\Èˆ	‰ˆ
ÊÜ›Ø™WÜÙY[ˆH›Ø™WÙœ˜[Y\ÊHÂˆYˆ
Ù][Š‘ÑU—ÓUSÒT—ÔÒÕŠHÙ][Š‘ÑU—ÓUSÒT—Ô“Ğ‘HŠJHÂˆš[Š–ÙÙ]—VÛ][˜Ú\—HÑU—ÓUSÒT—ÔÒÕÔ“Ğ‘IÜÈ^[ÚXÚÜÈ\™H›İ‚ˆš[\[Y[YÛˆHY][˜XÚÙ[™KHÛÜÚ[™ÈY\ˆ	Yœ˜[Y\ÈÛ›Wˆ‹ˆ›Ø™WÙœ˜[Y\ÊNÂˆ™›\Ú
İİ]
NÂˆBˆ[›š[™ÈH˜[ÙNÂˆBˆÙ[ÙBˆÊˆÑU—ÓUSÒT—ÔÒÕO]˜›\ˆÚ]ÑU—ÓUSÒT—Ô“Ğ‘OOœ˜[Y\ÏˆÜš]HH˜]ØX›Bˆ
ˆÈHš[HÛˆH›Ø™Hœ˜[YKˆ[ˆ^\›˜[ØÜ™Y[œÚİÛÛØ[››İİÙÜ˜\\Âˆ
ˆÚ[™İÈ™[XX›HKHHØ\\š[™È›ØÙ\ÜÈ\ÈK][˜]Ø\™HÚ\™H\ÈÛ™H\È›İÛÂˆ
ˆH™Xİ[™ÛH]Ü˜XœÈ\ÈHš\X[\ÙYÛ™H[™H[XYÙHÛÛY\È˜XÚÈÜ›ÜY‚ˆ
ˆ™XY[™ÈHœ˜[YXY™™\ˆÙH\İ™]È\È›ÈİXÚ[XšYİZ]Kˆ
‹ÂˆYˆ
›Ø™WÙœ˜[Y\Èˆ	‰ˆ›Ø™WÜÙY[ˆ
ÈHH›Ø™WÙœ˜[Y\ÊHÂˆÛÛœİÚ\ˆ
œÚİHÙ][Š‘ÑU—ÓUSÒT—ÔÒÕŠNÂˆYˆ
ÚİOH•S	‰ˆ
œÚİOH	×	ÊHÂˆ[ËÂˆÑÑÓÑÙ]˜]ØX›TÚ^™JÚ[‹	™Ë	™
NÂˆ[œÚYÛ™YÚ\ˆ
œH
[œÚYÛ™YÚ\ˆ
ŠHX[ØÊ
Ú^™Wİ
HÈ
ˆ
Ú^™Wİ
H
ˆ
NÂˆ’SH
™ˆH
OH•S
HÈ›Ü[ŠÚİØˆŠHˆ•SÂˆYˆ
OH•S	‰ˆˆOH•S
HÂˆÛ™XY^[ÊËÓÔ‘ĞKÓÕS”ÒQÓ‘QĞ–UK
NÂˆÛÛœİ[YH
H
È
ˆÊH	H
H	HÂˆÛÛœİ[]H
È
ˆÈ
ÈY
H
ˆÂˆ[œÚYÛ™YÚ\ˆ–ÍMNÂˆÛÛœİ[œÚYÛ™YÚ\ˆ™\›ÖÌ×HHÈNÂˆY[\Ù]
‹Ú^™[ÙˆŠNÂˆ–ÌHH	Ğ‰ÎÈ–ÌWHH	ÓIÎÂˆ
Š[
ŠH	š–Ì—HHM
È]Âˆ
Š[
ŠH	š–ÌLHHMÂˆ
Š[
ŠH	š–ÌMHHÂˆ
Š[
ŠH	š–ÌNHHÎÂˆ
Š[
ŠH	š–ÌŒ—HHÈÊˆÜÚ]]™Nˆ›İÛK]\X]Ú[™ÈÛ™XY^[È
‹Âˆ
ŠÚÜ
ŠH	š–Ì—HHNÂˆ
ŠÚÜ
ŠH	š–ÌHHÂˆ
Š[
ŠH	š–ÌÍHH]ÂˆÜš]J‹KMŠNÂˆ›Üˆ
[HHÈHÈJÊÊHÂˆ›Üˆ
[HÈÎÈ
ÊÊHÂˆÛÛœİ[œÚYÛ™YÚ\ˆ
œHH
È

Ú^™Wİ
HH
ˆÈ
È
H
ˆÂˆÛÛœİ[œÚYÛ™YÚ\ˆ™Ü–Ì×HHÈVÌ—KVÌWKVÌHNÂˆÜš]J™Ü‹KËŠNÂˆBˆYˆ
Y
HÜš]J™\›ËK
Ú^™Wİ
HYŠNÂˆBˆš[Š–ÙÙ]—VÛ][˜Ú\—HÚİˆ	\È
	Y	Y
Wˆ‹ÚİË
NÂˆBˆYˆ
ŠH˜ÛÜÙJŠNÂˆYˆ

Hœ™YJ
NÂˆBˆBˆÊˆÑU—ÓUSÒT—Ô“Ğ‘OOœ˜[Y\Ïˆ˜]È]X[Hœ˜[Y\ËÛİ[İÈX[H^[ÈY™™\‚ˆ
ˆœ›ÛHHÛX\ˆÛÛİ\‹™\Ü[™ÛÜÙKˆHÚ[™İÈ›ØÚÜÈÛˆH[X[‹ÛÈÚ]İ]ˆ
ˆ\ÈH][˜Ú\ˆ™[™\œÈˆÛİ[Û›H™H\ÜÙ\YˆÛİ[[™È^[È\İ[™İZ\Ú\Âˆ
ˆH˜]ÛˆRHœ›ÛH[ˆ[\HÚ[™İÈ]Y\™[H˜Z[YÈ\œ›Ü‹ÚXÚ\ÈHXİX[ˆ
ˆ˜Z[\™H[ÙHÛÜØ]Ú[™È\™Kˆ
‹ÂˆYˆ
›Ø™WÙœ˜[Y\Èˆ	‰ˆ
ÊÜ›Ø™WÜÙY[ˆH›Ø™WÙœ˜[Y\ÊHÂˆ[İËÚKÚ[™ÙYHÂˆÑÑÓÑÙ]˜]ØX›TÚ^™JÚ[‹	İË	Ú
NÂˆÂˆ[œÚYÛ™YÚ\ˆ
œH
[œÚYÛ™YÚ\ˆ
ŠHX[ØÊ
Ú^™Wİ
HİÈ
ˆ
Ú^™Wİ
HÚ
ˆ
NÂˆYˆ
OH•S
HÂˆÛ™XY^[ÊİËÚÓÔ‘ĞKÓÕS”ÒQÓ‘QĞ–UK
NÂˆ›Üˆ
HHÈHÚÈJÊÊHÂˆ›Üˆ
HÈİÎÈ
ÊÊHÂˆÛÛœİ[œÚYÛ™YÚ\ˆ
œHH
È

Ú^™Wİ
HH
ˆİÈ
È
H
ˆÂˆÊˆHÛX\ˆÛÛİ\‹Œ‹ÌŒËÌŒH\ÈX›İ]MKÌNÌŒÈ
‹ÂˆYˆ
VÌHˆÌVÌWHˆÌVÌ—HˆÎ
HÈÚ[™ÙY
ÊÎÈBˆBˆBˆœ™YJ
NÂˆBˆBˆš[Š–ÙÙ]—VÛ][˜Ú\—H›Ø™Nˆ	Y	Y˜]ØX›K	Y^[ÈX›İ™HHÛX\ˆ‚ˆ˜ÛÛİ\ˆY\ˆ	Yœ˜[Y\×ˆ‹İËÚÚ[™ÙY›Ø™WÙœ˜[Y\ÊNÂˆ[›š[™ÈH˜[ÙNÂˆB‚ˆÑÑÓÔİØ\Ú[™İÊÚ[ŠNÂˆÙ[™Y‚ˆB‚ˆÚY™YˆTWÓQUSˆÙS][˜Ú\“Y][\İ›ŞJ
NÂˆÙ[ÙBˆ[QİZWÒ[\Ü[‘Ó—ÔÚ]İÛŠ
NÂˆÙ[™Y‚ˆ[QİZWÒ[\Ñ—ÔÚ]İÛŠ
NÂˆ[QİZN‘\İ›ŞPÛÛ^

NÂˆÚY›™YˆTWÓQUSˆÑÑÓÑ[]PÛÛ^
İ
NÂˆÙ[™Y‚ˆÑÑ\İ›ŞUÚ[™İÊÚ[ŠNÂˆÑÔ]Z]

NÂ‚ˆYˆ
[][˜Ú
HÂˆš[Š–ÙÙ]—VÛ][˜Ú\—HÛÜÙYÚ]İ]İ\[™ÈHØ[YWˆŠNÂˆ™]\›ˆNÈÊˆØ[\ˆ^]È
‹ÂˆB‚ˆ[Ù[ÜİÜ™JJNÂˆÊˆ\œÚ\İHÛÛ›ÛÈYÙHÛˆUSÒ›İÛ›HÚ[ˆĞU‘HÓÓ•“ÓÈ\È™\ÜÙY‚ˆ
‚ˆ
ˆH^XÚ]]ÛˆØ\È›İ[›İYÚˆ]™\Hİ\ˆYÙH[ˆ\È][˜Ú\ˆZÙ\Âˆ
ˆY™™XİH™Z[™È[™YÈH™[][˜ÚYØ[YH\È[š\›Û›Y[˜\šXX›\ËÛÂˆ
ˆ˜Ú[™ÙHHš[™[™Ë™\ÜÈ][˜Ú^HˆÛÚÜÈ^XİHZÙH]ÛÜšÙYKH[™[‚ˆ
ˆH™[X\\ÈÛÛ™HH™^[YHHØ[YHİ\ÈÛÛÚ]›İ[™È]š[™ÈØZYˆ
ˆÛËˆ™\]Z\š[™ÈHÙXÛÛ™Ù\\˜]HÛXÚÈÈXZÙHH™Xš[™\›X[™[\ÈH˜\[™ˆ
ˆH^Y\ˆÚÈ˜[È[È]ÛÛ˜ÛY\È]Ø]š[™È\Èœ›ÚÙ[ˆ˜]\ˆ[ˆ]^Bˆ
ˆZ\ÜÙYH]Û‹‚ˆ
‚ˆ
ˆ][˜Ú[™È\È[ˆ^XÚ][X™\˜]HXİÚXÚ\ÈÚ]XYHH]Û‹[Û›Bˆ
ˆ\ÚYÛˆY™[œÚX›H[ˆHš\œİXÙNÈ\ÈÚ[\H]XÚ\ÈHÜš]HÈHXİˆ
ˆH^Y\ˆ[™XYH\™›Ü›\ËˆHÜš]H]Ù[ˆ\ÈØ\™Y[KHÛÛ[Y[ËÜ™\š[™Âˆ
ˆ[™]™\Hİ\ˆÙ][™È[ˆHš[Hİ\š]™HKHÛÈÚ[™È][Ü™HÙ[ˆ\ÈÚX\‚ˆ
ˆH]Ûˆİ^\Ë›ÜˆØ]š[™ÈÚ]İ]][˜Ú[™Ëˆ
‹ÂˆÛÛ›Û×ÜØ]™Wİ×ØÛÛ™šYÊJNÂˆš[Š–ÙÙ]—VÛ][˜Ú\—Hİ\[™Îˆ›Ùš[OI\È[\Ù]I\É\É\×ˆ‹ˆKœ›Ùš[HÈ™ÛÛ[™^YJÈˆˆ˜˜\ÙKYØ[YH‹ˆÔ[\Ù]ÖÛKœ[\Ù]KˆKšÜ™HÈˆÜ™Hˆˆˆ‹ˆKœXÚ×ÜİYÙHÈˆˆˆˆ
]HØÜ™Y[ŠHŠNÂˆ™[][˜Ú

NÂˆ™]\›ˆÂŸB‚ˆÙ[ÙHÊˆQÑWÕÒUÒSQÕRH
‹Â‚‹ÊˆÚ]İ][QİZH\™H\È›İ[™ÈÈ˜]ÈHÚ[™İÈÚ]ˆ\ÚÚ[™È›ÜˆH][˜Ú\ˆØ^\ÈÛÂˆ
ˆÛ˜ÙH[™İ\ÈHØ[YH›Ü›X[K˜]\ˆ[ˆ˜Z[[™ÎˆK[][˜Ú\ˆ]\İ™]™\ˆ™HHØ^HÂˆ
ˆXZÙHHÛÜšÚ[™Èš[˜\H™Y\ÙHÈ[‹ˆ
‹Â™^\›ˆÈˆ[ÙTÜ][˜Ú\”[Š[\™ØËÚ\ˆ
Š˜\™İŠBÂˆ[NÂˆ›Üˆ
HHNÈH\™ØÎÈJÊÊHÂˆYˆ
İ˜Û\
\™İ–ÚWK‹K[][˜Ú\ˆŠHOH
HÂˆš[Š–ÙÙ]—VÛ][˜Ú\—H\Èš[˜\HØ\ÈZ[Ú]İ][QİZK—ˆ‚ˆ–ÙÙ]—VÛ][˜Ú\—H[ˆÛÛËÙ™]ÚÚ[YİZKœÚ[ˆ‹ÙÙ]‹ØZ[ÛXXËœÚ[ˆŠNÂˆœ™XZÎÂˆBˆBˆ™]\›ˆÂŸB‚ˆÙ[™YˆÊˆÑWÕÒUÒSQÕRH
‹Â