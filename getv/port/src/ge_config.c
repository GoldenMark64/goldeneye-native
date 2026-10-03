/* ge_config.c - the user configuration layer.
 *
 * Why this exists, and why it looks like this
 * -------------------------------------------
 * This port already has around 100 `GETV_*` environment gates. They were never
 * designed as user settings; they are A/B development switches, and the project's
 * reproducibility rules (PORTING_PLAYBOOK.md Â§2.10-Â§2.12, every `level_sweep.sh` arm)
 * depend on them continuing to behave exactly as they do today. A public release still
 * needs a config file.
 *
 * The whole design is one line: setenv(key, value, OVERWRITE=0).
 *
 * POSIX setenv's third argument is "overwrite". Passing 0 means "set this only if it is * not already set". So:
 *
 * config file  -> setenv(..., 0) loses to anything already in the environment
 * CLI flag     -> setenv(..., 1) overwrites, so it beats the environment
 *
 * which is the required precedence, CLI > env > config file > default, with no changes
 * to any consumer. Every existing `getenv("GETV_...")` call site in port_render.c,
 * port_input.c, port_audio.c, port_save.c, port_support.c, gfx_sdl2.c, gfx_opengl.c,
 * gfx_pc.c and front.c keeps working unmodified. A harness that exports
 * GETV_EXIT_FRAME=61 gets 61 no matter what a user's goldeneye.cfg says.
 *
 * The consequence is that this file must run before anything reads a gate. It is called
 * from the first statement of main() in port/mac/ge_mac_main.c, before SDL_main(), and
 * therefore before gfx_init(), before osGetCount()'s first call, and before front.c
 * ever runs. That ordering is the only invariant here.
 *
 * Where the file is looked for (first hit wins, all others ignored)
 * -----------------------------------------------------------------
 *   1. $GETV_CONFIG                                    (explicit override)
 *   2. --config=<path>                                 (explicit override)
 *   3. <dir of argv[0]>/goldeneye.ini                  (beside the binary)
 *      falling back to legacy goldeneye.cfg
 *   4. platform user-data directory/goldeneye.ini
 *      falling back to legacy goldeneye.cfg
 *
 * argv[0] is used rather than _NSGetExecutablePath() deliberately: this file is globbed
 * into the port layer too (build_sim.sh / build.sh compile port/src/*.c), and
 * <mach-o/dyld.h> is a macOS-only header. argv[0] is portable C.
 *
 * File format
 * -----------
 *   # comment              ; also a comment
 * key = value            (whitespace around either side is trimmed)
 * GETV_ANYTHING = value  (raw escape hatch - sets that gate directly)
 *
 * No sections, no quoting, no line continuation. A config file that users hand-edit
 * should be hard to get subtly wrong; an unknown key is reported on stdout rather than
 * ignored.
 */

#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "ge_actions.h"
#include "ge_config.h"
/* The user-data directory and mkdir -p, factored out of the
 * two places below that open-coded "$HOME/Library/Application Support". The macOS
 * paths this file produces are unchanged, character for character. */
#include "port_paths.h"
int ge_config_loaded  = 0;
int ge_config_controls = -1;

/* port/configfile.h's globals. Declared by hand rather than #included: configfile.h
 * drags in the Fast3D config surface, and this is all we touch. It is defined in
 * port/src/port_support.c; this file only assigns to it. */
extern unsigned int configFiltering;   /* 0 = nearest, 1 = bilinear, 2 = three-point */
extern unsigned int configWidescreen;  /* 0 = retail 4:3 pillarbox, 1 = fill real window */
extern unsigned char ge_crosshair_r, ge_crosshair_g, ge_crosshair_b;
extern float ge_crosshair_scale;

/* Rare's own leftover position readout. src/game/debugmenu_handler.c:1018 - a
 * three-line exported setter for `g_DebugManPos` (a plain s32 in BSS at :266),
 * whose only other writers are the debug menu's own toggle (:628) and this setter.
 * Setting it once here therefore sticks. bondview2.c:10367 then draws room id,
 * collision X/Y/Z and a compass heading every frame.
 *
 * This works in a stock build: the flag is gated on LEFTOVERDEBUG, which
 * build_mac.sh:87 already defines unconditionally. It does not need DEBUGMENU and
 * therefore does not repurpose START or change codegen. */
extern void set_debug_testingmanpos_flag(int flag);

/* ------------------------------------------------------------------------- *
 * Named cheats - the game's own cheat system, exposed by name.
 *
 * These are not GameShark codes, and the difference matters.
 *
 * A GameShark code is a raw N64 RDRAM address. This port has no RDRAM; it has native
 * pointers at ASLR'd locations, so the roughly 1,900 published GoldenEye codes cannot be
 * applied here the way a recompilation applies them. Mapping them back to symbols
 * resolves only 1.3% against this decomp (there is no .map file, and ge007.ld fixes only
 * three addresses), so the address route is a dead end.
 *
 * The well-known codes do not need it. Twenty-four of them cluster one byte apart
 * starting at 0x80069652, and
 *
 * gameshark_address - 0x80069650  == the CHEAT_ID enum ordinal
 *
 * exactly, gaps included: 0x80069650 is the retail base of `g_CheatPlayerTextRelated[]`
 * (src/game/cheat.c:26). This checks out against src/bondconstants.h:1249 on nine values
 * - Invincibility=2, AllGuns=3, LineMode=7, 2xHealth=8, Invisibility=0xA,
 * InfiniteAmmo=0xB, DKMode=0xC, TinyBond=0xE, Paintball=0xF - and every skipped address
 * lands on an enum member the published code lists do not name (CHEAT_MAXAMMO,
 * CHEAT_DEBUG_UNK5, CHEAT_DEACTIVATE_INVINCIBILITY, CHEAT_2X_ARMOR,
 * CHEAT_EXTRA_WEAPONS), which a coincidence would not reproduce.
 *
 * So the useful cheats on that list are the game's own cheat flags, and they can be set
 * by name. That is layout-independent, ASLR-proof, survives every relink and recompile,
 * and stays correct under mods that move the array - none of which an address list can
 * do.
 *
 * Why a direct array write and not cheatButtonTurnOnCheatForPlayers(): that function
 * (cheat.c:952) reads g_CheatInfo, calls getPlayerCount() and set_cur_player(), and
 * dispatches a per-cheat switch. None of that is safe from main(), which runs before any
 * player exists. It is also unnecessary, because the consumers do not read a cached copy
 * - they call cheatIsActive() live, per use (explosion.c:2025 for paintball; chr.c:2188,
 * 2208, 2825 and chr_b.c:41 for DK mode). cheatIsActive() (cheat.c:1677) is nothing but
 * `(g_CheatPlayerTextRelated[cheat] >> get_cur_playernum()) & 1`. Setting the bits
 * directly at startup therefore reaches every one of those call sites, and for anything
 * spawned after startup it is more complete than the turn-on path, whose
 * cheatButtonSetDkMode() only rescales guards that already exist.
 *
 * What this does not do. A flag write alone is not enough for most cheats: `line_mode`
 * set this way produces a frame byte-identical to baseline. The rule comes from
 * enumerating every live consumer in the tree - `grep -rE "cheatIsActive\(CHEAT_" src` -
 * which returns exactly five:
 *
 * CHEAT_DK_MODE chr.c:2188,2208,2825 chr_b.c:41,43
 * CHEAT_INFINITE_AMMO lv.c  (x2)
 * CHEAT_PAINTBALL explosion.c:2025
 * CHEAT_NO_RADAR_MP radar.c
 * CHEAT_ENEMY_ROCKETS prop.c
 *
 * (CHEAT_MARQUIS and CHEAT_ENEMYSHIELDS also appear in that grep. Both are inside
 * commented-out Perfect Dark leftovers in chrai.c:2981 and :3517, and neither exists in
 * the CHEAT_IDS enum at all. They are not cheats and are not exposed.)
 *
 * Those five, plus CHEAT_EXTRA_MP_CHARS (whose entire switch arm is one assignment this
 * file can make itself), are marked `live = 1` and take effect from the config file
 * immediately. Every other cheat's effect lives in the turn-on switch
 * (cheat.c:1084-1445) - granting weapons, multiplying health - which needs a player
 * context that does not exist at main() time. For those, the flag is set, which is real
 * and which the game's own UI honours, and the log says so. Nothing is silently
 * half-applied.
 *
 * cheatDisableAllCheats() (cheat.c:1625) is called from lvlUnloadStageTextData()
 * (lv.c:1745), i.e. on stage unload, and clears every cheat carrying CHEAT_MASK_TOGGLE.
 * Config cheats therefore apply to the session you boot into and are cleared when you
 * leave the stage. That is the retail lifetime, not a bug.
 *
 * The ordinals below are transcribed from src/bondconstants.h:1249-1284, which is the
 * only source of truth. They are hard-coded rather than #included because this file is
 * port-layer code and must not pull a game header (and therefore <ultra64.h>) into the
 * port build. If that enum gains or loses a member, every ordinal after it shifts and
 * this table silently applies the wrong cheat. Re-check the table against the enum after
 * any decomp bump. */

extern unsigned char g_CheatPlayerTextRelated[];  /* cheat.c:26, u8[CHEAT_INVALID+1] */
extern int num_chars_selectable_mp;               /* front.c:573, s32, initialised to 8 */

#define GE_CHEAT_MAX_ID 34   /* CHEAT_2X_LASER - the last gameplay cheat we expose */

static const struct { const char *name; unsigned char id; unsigned char live; }
GE_CHEATS[] = {
    /* name id live = has a real cheatIsActive() consumer, so a
 flag write alone is enough (see above) */
    { "extra_mp_chars",          1, 1 },   /* handled specially -> roster, below */
    { "invincibility",           2, 0 },
    { "all_guns",                3, 0 },
    { "max_ammo",                4, 0 },
    { "line_mode",               7, 0 },   /* no live consumer; front.c:1015 only */
    { "2x_health",               8, 0 },
    { "2x_armor",                9, 0 },
    { "invisibility",           10, 0 },
    { "infinite_ammo",          11, 1 },   /* lv.c */
    { "dk_mode",                12, 1 },   /* chr.c, chr_b.c */
    { "extra_weapons",          13, 0 },
    { "tiny_bond",              14, 0 },
    { "paintball",              15, 1 },   /* explosion.c:2025 */
    { "10x_health",             16, 0 },
    { "magnum",                 17, 0 },
    { "laser",                  18, 0 },
    { "golden_gun",             19, 0 },
    { "silver_pp7",             20, 0 },
    { "gold_pp7",               21, 0 },
    { "bond_phase",             22, 0 },
    { "no_radar",               23, 1 },   /* radar.c */
    { "turbo_mode",             24, 0 },
    { "debug_pos",              25, 0 },
    { "fast_animation",         26, 0 },
    { "slow_animation",         27, 0 },
    { "enemy_rockets",          28, 1 },   /* prop.c */
    { "2x_rocket_launcher",     29, 0 },
    { "2x_grenade_launcher",    30, 0 },
    { "2x_rcp90",               31, 0 },
    { "2x_throwing_knife",      32, 0 },
    { "2x_hunting_knife",       33, 0 },
    { "2x_laser",               34, 0 },
};
#define GE_CHEAT_COUNT ((int)(sizeof GE_CHEATS / sizeof GE_CHEATS[0]))

/* All four player bits, mirroring what cheatButtonHandleCheatsTurnedOn() writes for a
 * CHEAT_MASK_GLOBAL cheat: `(1 << player_count) - 1`. Using 0xF makes the cheat active
 * whatever get_cur_playernum() turns out to be, in SP and in 1-4P MP alike. */
#define GE_CHEAT_ALL_PLAYERS 0x0F


#define GE_CFG_BASENAME "goldeneye.ini"
#define GE_CFG_LEGACY_BASENAME "goldeneye.cfg"
static int g_errors = 0;
static char g_cfgpath[1024] = "";

/* ------------------------------------------------------------------ helpers */

static void ge_err(const char *fmt, const char *a, const char *b)
{
 printf("[getv][config] ERROR: ");
 printf(fmt, a, b);
 printf("\n");
 g_errors++;
}

static char *trim(char *s)
{
 char *e;
 while (*s != '\0' && isspace((unsigned char)*s)) { s++; }
 e = s + strlen(s);
 while (e > s && isspace((unsigned char)e[-1])) { e--; }
    *e = '\0';
 return s;
}

static void lower(char *s)
{
 for (; *s != '\0'; s++) { *s = (char)tolower((unsigned char)*s); }
}

/* The single choke point. `over` is setenv's overwrite flag and is the only thing that
 * distinguishes a CLI source from a file source. */
static void put(const char *name, const char *value, int over)
{
 setenv(name, value, over);
}

static int is_true(const char *v)
{
 return (strcmp(v, "1") == 0 || strcmp(v, "on") == 0 ||
 strcmp(v, "true") == 0 || strcmp(v, "yes") == 0);
}

static int is_false(const char *v)
{
 return (strcmp(v, "0") == 0 || strcmp(v, "off") == 0 ||
 strcmp(v, "false") == 0 || strcmp(v, "no") == 0);
}

/* ------------------------------------------------------- the day-0 key table */

/* Every key here is a friendly name for an existing gate, never a new mechanism.
 * Anything that cannot be expressed as "set this GETV_* variable" does not belong in
 * this table. */

static void key_resolution(const char *v, int over)
{
 unsigned w = 0, h = 0;
    /* Mirrors port_support.c:79's own parse and its own 320x240 floor exactly, so a
     * value this layer accepts is a value that layer will honour. Accepting
     * something it silently drops would be worse than rejecting it here. */
 if (strcmp(v, "fullscreen") == 0 || strcmp(v, "native") == 0) {
 put("GETV_FULLSCREEN", "1", over);
 return;
    }
 if (sscanf(v, "%ux%u", &w, &h) != 2 || w < 320 || h < 240) {
 ge_err("resolution=\"%s\" is not WIDTHxHEIGHT with width>=320 and height>=240 ""(e.g. 1280x960, 1920x1080, or \"fullscreen\")%s", v, "");
 return;
    }
    {
 char buf[64];
 snprintf(buf, sizeof buf, "%ux%u", w, h);
 put("GETV_WINDOW", buf, over);
    }
}

static void key_crosshair_color(const char *v, int over)
{
    unsigned r, g, b;
    /* Mirrors port_support.c's own sscanf("%2x%2x%2x", ...) exactly, same reasoning as
     * key_resolution above: a value this layer accepts is a value that layer parses too. */
    if (strlen(v) != 6 || sscanf(v, "%2x%2x%2x", &r, &g, &b) != 3) {
        ge_err("crosshair_color=\"%s\" is not RRGGBB hex (e.g. FF0000 for red, "
               "00FF00 for green, FFFFFF for the retail default)%s", v, "");
        return;
    }
    put("GETV_CROSSHAIR_COLOR", v, over);
}

static void key_gibs(const char *v, int over)
{
    if (is_false(v)) {
        put("GETV_GIBS", "off", over);
    } else if (is_true(v) || strcmp(v, "explosion") == 0 ||
               strcmp(v, "explosions") == 0) {
        put("GETV_GIBS", "explosions", over);
    } else if (strcmp(v, "high_damage") == 0 || strcmp(v, "high-damage") == 0 ||
               strcmp(v, "highdamage") == 0) {
        put("GETV_GIBS", "high_damage", over);
    } else if (strcmp(v, "always") == 0) {
        put("GETV_GIBS", "always", over);
    } else {
        ge_err("gibs=\"%s\" - expected off|explosions|high_damage|always%s", v, "");
    }
}

/* Mirrors port_support.c's own clamp exactly, same reasoning as key_crosshair_color above:
 * a value this layer accepts has to be one that layer will actually use. Silently taking a
 * number and then ignoring it is the failure this whole file is written against. */
static void key_crosshair_scale(const char *v, int over)
{
    double s = atof(v);

 "‹­¦ëm®éÜj×¢¸ Šv¥jšk£¦j×­¢G§r‹§·~¶Ó¸Yˆ
ˆH	ĞIÈ	‰ˆˆH	Ö‰ÊHÈˆH
Ú\ŠH
ˆH	ĞIÈ
È	ØIÊNÈBˆYˆ
HOHŠHÈ™]\›ˆLNÈBˆBˆ
ÏHÛ[ÂˆBˆÚ[H

œOH	È	È
œOH	×	ÊHÈ
ÊÎÈBˆYˆ

œOH	ÏIÊHÈ™]\›ˆLNÈBˆ
ÊÎÂˆÚ[H

œOH	È	È
œOH	×	ÊHÈ
ÊÎÈB‚ˆYˆ
İ]ØÛÛ[Y[YOH•S
HÈ
›İ]ØÛÛ[Y[YHÛÛ[Y[YÈBˆ™]\›ˆ
[
H
H[™JNÂŸB‚‹ÊˆÚ\™HÙPÛÛ™šYÔØ]™J
HÜš]\Ëˆ™]™\ˆ•SÈ[\HÚ[ˆ›Èš[HØ\ÈØØ]YS‘›Û™Bˆ
ˆÛİ[™HXÙYÚXÚHØ[\ˆ]\İ™X]\ÈœØ]š[™È\È[˜]˜Z[X›Hˆ˜]\ˆ[‚ˆ
ˆÜš][™ÈÈHÛÜšÚ[™È\™XİÜKˆ
‹Â˜ÛÛœİÚ\ˆ
™ÙPÛÛ™šYÔ]
›ÚY
BÂˆİ]XÈÚ\ˆ˜[˜XÚÖÌLNÂ‚ˆYˆ
×ØÙ™Ü]ÌHOH	×	ÊHÈ™]\›ˆ×ØÙ™Ü]ÈB‚ˆÊˆ›Èš[HØ\È™XY\È[ˆKHHš\œİ][˜ÚÜˆÛ™Hİ\YÚ]HÛÛ™šYÂˆ
ˆ[]YˆØ]š[™È]\İİ[ÛÜšË[™]]\İ[™Ú\™HH™^][˜ÚÚ[ˆ
ˆÛÚËÚXÚ\ÈHØ[YH\Ù\‹Y]H\™XİÜHØØ]J
H˜[È›İYÚËˆ
‹ÂˆYˆ
ÙTÜ\Ù\‘]Q\Š‘ÛÛ[™^YKS˜]]™H‹‘ÛÛ[™^YKS˜]]™H‹ˆ˜[˜XÚËÚ^™[Ùˆ˜[˜XÚÊHOH
HÂˆ™]\›ˆˆÂˆBˆYˆ
ÙTÜXZÙQ\•™YJ˜[˜XÚËÍÍÊHOH
HÂˆš[Š–ÙÙ]—VØÛÛ™šY×HZÙ\ˆ˜Z[Yˆ	\×ˆ‹˜[˜XÚÊNÂˆ™]\›ˆˆÂˆBˆYˆ
İ›[Š˜[˜XÚÊH
ÈH
Èİ›[ŠÑWĞÑ‘×ĞTÑSSQJH
ÈHˆÚ^™[Ùˆ˜[˜XÚÊHÂˆ™]\›ˆˆÂˆBˆİ˜Ø]
˜[˜XÚË‹ÈˆÑWĞÑ‘×ĞTÑSSQJNÂˆ™]\›ˆ˜[˜XÚÎÂŸB‚‹ÊˆY\™ÙHÛİ[Ù^Kİ˜[YHZ\œÈ[ÈHÛÛ™šYÈš[K‚ˆ
‚ˆ
ˆH•SÜˆ[\H˜[YHSUTÈHÙ^HKHH[™H\ÈÛÛ[Y[Yİ]˜]\ˆ[‚ˆ
ˆ™[[İ™YÛÈHÛÛ[Y[]Øİ[Y[È]İ\š]™\È[™H^Y\ˆØ[ˆÙYHÚ]Ø\Âˆ
ˆ\›™YÙ™‹ˆ™]\›œÈÛˆİXØÙ\ÜË‚ˆ
‚ˆ
ˆÜš][ˆ›İYÚH[\Ü˜\Hš[H[™™[˜[YYˆHÜ˜\ÚÜˆH[\ÚÈZYØ^H›İYÚˆ
ˆH\™Xİ™]Üš]HÛİ[X]™HH[˜Ø]YÛÛ™šYË[™H™^][˜ÚÛİ[ÛÛYH\ˆ
ˆÚ][ˆ]ÈÙ][™ÜÈZ\ÜÚ[™È[™›È[™XØ][ÛˆÚNÈ™[˜[YJ
H\È]ÛZXÈÛˆ]™\Bˆ
ˆ]›Ü›H\ÈÚ\ÈËÛÈHš[H\ÈZ]\ˆHÛÛ™HÜˆH™]ÈÛ™K‚ˆ
‹Âš[ÙPÛÛ™šYÔØ]™JÛÛœİÚ\ˆ
˜ÛÛœİ
šÙ^\ËÛÛœİÚ\ˆ
˜ÛÛœİ
˜[Y\Ë[Ûİ[
BÂˆÛÛœİÚ\ˆ
œ]HÙPÛÛ™šYÔ]

NÂˆÚ\ˆ\ÌLNÂˆ’SH
š[Âˆ’SH
›İ]ÂˆÚ\ˆ[™VÌŒNÂˆ[
Üš][Âˆ[NÂˆ[˜ÈHÂ‚ˆYˆ
]OH•S
œ]OH	×	ÊHÂˆš[Š–ÙÙ]—VØÛÛ™šY×H›İÚ\™HÈØ]™H×ˆŠNÂˆ™]\›ˆNÂˆBˆYˆ
Ûİ[
HÈÛİ[HÈB‚ˆÜš][ˆH
[
ŠHØ[ØÊ
Ú^™Wİ
H
Ûİ[ˆÈÛİ[ˆJKÚ^™[ÙŠ[
JNÂˆYˆ
Üš][ˆOH•S
HÈ™]\›ˆNÈB‚ˆYˆ
Ûœš[Š\Ú^™[Ùˆ\‰\Ë\‹]
HH
[
HÚ^™[Ùˆ\
HÂˆœ™YJÜš][ŠNÂˆ™]\›ˆNÂˆBˆİ]H›Ü[Š\ÈŠNÂˆYˆ
İ]OH•S
HÂˆš[Š–ÙÙ]—VØÛÛ™šY×HØ[››İÜš]H	\×ˆ‹\
NÂˆœ™YJÜš][ŠNÂˆ™]\›ˆNÂˆB‚ˆ[ˆH›Ü[Š]œˆŠNÂˆYˆ
[ˆOH•S
HÂˆÚ[H
™Ù]Ê[™KÚ^™[Ùˆ[™K[ŠHOH•S
HÂˆ[[™YHÂ‚ˆ›Üˆ
HHÈHÛİ[ÈJÊÊHÂˆ[ÛÛ[Y[YHÂˆ[Ù™Â‚ˆYˆ
Üš][–ÚWJHÈÛÛ[YNÈBˆYˆ
Ù^\ÖÚWHOH•S
HÈÛÛ[YNÈBˆÙ™ˆHÙ™×Û[™WÛX]Ú\Ê[™KÙ^\ÖÚWK	˜ÛÛ[Y[Y
NÂˆYˆ
Ù™ˆ
HÈÛÛ[YNÈB‚ˆÊˆ[˜Ú[™ÙY˜[YNˆÙY\H[™H]H›Üˆ]K‚ˆ
‚ˆ
ˆÚ]İ]\È]™\HØ]™H™]Ü›İHXXÚÙ^H]Ø\È[™YÛÈH]™Bˆ
ˆš\™HHØ[YH˜XÚÈ\Èš\™HHKHHÛÛ[[ˆ[YÛ›Y[[™ˆ
ˆ[H˜Z[[™ÈÈ›İXÛÛ™HKHÛˆH[™HÚÜÙHÙ][™ÈY›İÚ[™ÙYˆ
ˆ][ˆÛ›HH[™HÚÜÙH˜[YHXİX[HY™™\œÈ\ÈÛÜ™]Üš][™Ëˆ
‹ÂˆYˆ
XÛÛ[Y[Y	‰ˆ˜[Y\ÖÚWHOH•S	‰ˆ˜[Y\ÖÚWVÌHOH	×	ÊHÂˆÛÛœİÚ\ˆ
ˆH[™H
ÈÙ™ÂˆÚ^™WİˆHÂˆÚ[H
–Û—HOH	×	È	‰ˆ–Û—HOH	ÈÉÈ	‰ˆ–Û—HOH	ÎÉÈ	‰‚ˆ–Û—HOH	×‰È	‰ˆ–Û—HOH	×‰ÊHÈŠÊÎÈBˆÚ[H
ˆˆ	‰ˆ
–ÛˆHWHOH	È	È–ÛˆHWHOH	×	ÊJHÈ‹KNÈBˆYˆ
İ›[Š˜[Y\ÖÚWJHOHˆ	‰ˆİ›˜Û\
‹˜[Y\ÖÚWKŠHOH
HÂˆœ]Ê[™Kİ]
NÂˆÜš][–ÚWHHNÂˆ[™YHNÂˆœ™XZÎÂˆBˆB‚ˆÊˆÙY\HXY[™ÈÚ]\ÜXÙHÛÈ[ˆ[™[Y›ØÚÈİ^\È[™[Yˆ
‹ÂˆÂˆÛÛœİÚ\ˆ
›XYH[™NÂˆÚ^™Wİ›XYHÂˆÚ[H
XYÛ›XYHOH	È	ÈXYÛ›XYHOH	×	ÊHÈ›XY
ÊÎÈBˆÜš]J[™KK›XYİ]
NÂˆB‚ˆYˆ
˜[Y\ÖÚWHOH•S˜[Y\ÖÚWVÌHOH	×	ÊHÂˆÊˆÛÛ[Y[Yİ]˜]\ˆ[ˆ[]YˆHİ\œ›İ[™[™ÈÛÛ[Y[]ˆ
ˆ^Z[œÈHÙ^Hİ^\ÈYX[š[™Ù[[™H^Y\ˆØ[ˆÙYHÚ]ˆ
ˆØ\È\›™YÙ™ˆ[œİXYÙˆš[™[™ÈHÛKˆ
‹Âˆœš[Šİ]ˆÈ	\ÈWˆ‹Ù^\ÖÚWJNÂˆH[ÙHÂˆœš[Šİ]‰\ÈH	\×ˆ‹Ù^\ÖÚWK˜[Y\ÖÚWJNÂˆBˆÜš][–ÚWHHNÂˆ[™YHNÂˆœ™XZÎÂˆB‚ˆYˆ
Z[™Y
HÈœ]Ê[™Kİ]
NÈBˆBˆ˜ÛÜÙJ[ŠNÂˆB‚ˆÊˆ[][™ÈHš[HY›İ[™XYHY[[Û‹]™HÜˆÛÛ[Y[YÛÙ\È[ˆH›ØÚÈ]ˆ
ˆH[™ˆX™[Y™XØ]\ÙHH^Y\ˆÚÈÜ[œÈHš[HY\ˆ\Ú[™ÈH][˜Ú\‚ˆ
ˆÚİ[™HX›HÈ[]HÛ[˜ÙHÚXÚ[™\È^HÜ›İH[™ÚXÚH][˜Ú\‚ˆ
ˆYˆ
‹ÂˆÂˆ[[HHÂˆ›Üˆ
HHÈHÛİ[ÈJÊÊHÂˆYˆ
Üš][–ÚWHÙ^\ÖÚWHOH•S
HÈÛÛ[YNÈBˆYˆ
˜[Y\ÖÚWHOH•S˜[Y\ÖÚWVÌHOH	×	ÊHÈÛÛ[YNÈBˆYˆ
X[JHÂˆœ]Ê—ˆÈKKHÜš][ˆHH][˜Ú\ˆKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKH‚ˆ‹KKKKKKKKKKKKKWˆ‹İ]
NÂˆ[HHNÂˆBˆœš[Šİ]‰\ÈH	\×ˆ‹Ù^\ÖÚWK˜[Y\ÖÚWJNÂˆBˆB‚ˆYˆ
˜ÛÜÙJİ]
HOH
HÂˆš[Š–ÙÙ]—VØÛÛ™šY×HÜš]H˜Z[Yˆ	\×ˆ‹\
NÂˆ™[[İ™J\
NÂˆœ™YJÜš][ŠNÂˆ™]\›ˆNÂˆB‚ˆÊˆÚ[™İÜÉÈ™[˜[YJ
H˜Z[ÈYˆH\İ[˜][Ûˆ^\İË[›ZÙHÔÒVˆ™[[İš[™Èš\œİˆ
ˆÜ[œÈHÚ[™İÈÚ\™H™Z]\ˆš[H\È]]ÚXÚ\ÈÚHH[\Ü˜\H\Âˆ
ˆÙ\[[H™[˜[YHİXØÙYYÈKHH˜Z[Y™[˜[YHX]™\ÈH]H™XÛİ™\˜X›H]ˆ
ˆ]‹\[™Ø^\ÈÛË˜]\ˆ[ˆÜÚ[™È]ˆ
‹ÂˆÚYˆYš[™Y
ÕÒSŒÌŠBˆ™[[İ™J]
NÂˆÙ[™Y‚ˆYˆ
™[˜[YJ\]
HOH
HÂˆš[Š–ÙÙ]—VØÛÛ™šY×HØ[››İ™\XÙH	\ÈKH[İ\ˆÙ][™ÜÈ\™H[ˆ	\×ˆ‹]\
NÂˆ˜ÈHNÂˆH[ÙHÂˆš[Š–ÙÙ]—VØÛÛ™šY×HØ]™Y	\×ˆ‹]
NÂˆB‚ˆœ™YJÜš][ŠNÂˆ™›\Ú
İİ]
NÂˆ™]\›ˆ˜ÎÂŸB‚‹ÊˆKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKH[š]
‹Â‚‹Êˆ™]\›™YHÙPÛÛ™šYÒ[š]

HÚ[ˆH›YÈ\ÈÛ™H]ÈÚÛH›Øˆ[™H›ØÙ\ÜÈÚİ[ˆ
ˆİÜİXØÙ\ÜÙ[H
KZ[K]Üš]KXÛÛ™šYËK[\İXÚX]ÊKˆ\İ[˜İœ›ÛHHÜÚ]]™Bˆ
ˆ™]\›‹ÚXÚYX[œÈH˜][ÛÛ™šYÈ\œ›Ü‹ˆ
‹ÂˆÙYš[™HÑWĞÓÓ‘’Q×ÔÕÔ
LJB‚š[ÙPÛÛ™šYÒ[š]
[\™ØËÚ\ˆ
Š˜\™İŠBÂˆÛÛœİÚ\ˆ
˜ÛT]H•SÂˆÛÛœİÚ\ˆ
Üš]T]H•SÂˆ[ÕÜš]HHÒ[HÂˆ[NÂ‚ˆÊˆ™Y›Ü™H[][™È\È™XY™XØ]\ÙHY\ˆHš[H\È™Y[ˆ\œÙY\™H\È›ÈØ^HYˆ
ˆÈ[H˜[YHH[š\›Û›Y[İ\YYœ›ÛHÛ™H\Èš[H]\™Kˆ
‹ÂˆÙWÜ™\Ù]ÜÛ˜\Úİ

NÂ‚ˆÊˆ\ÜÈHHÛ›HH›YÜÈ]Ú[™ÙHÚ]\[œÈ™^ˆ›İ[™È\È\YYY]ˆ
ˆ™XØ]\ÙHKXÛÛ™šYÈ]\İ™HÛ›İÛˆ™Y›Ü™HHš[H\È™XY[™]™\Hİ\ˆ›YÈ]\İˆ
ˆ™H\YYY\ˆ]‚ˆ
‚ˆ
ˆK\™\Ù]\ÈÜİY\™H\ÈÙ[\È\œÙY]\‹™XØ]\ÙHH™\Ù]\ÈÈ™H\YYˆ
ˆ‘UÑQSˆHš[H[™\ÜÈÈ›ÜˆHÛÛ[X[™[™HÈÙY\™X][™È]ˆÛ›İÚ[™ÈX›İ]]ˆ
ˆÛ›HÚ[ˆ\ÜÈÈ™XXÚ\È]Ûİ[™HÛÈ]Kˆ
‹Âˆ›Üˆ
HHNÈH\™ØÎÈJÊÊHÂˆÛÛœİÚ\ˆ
˜HH\™İ–ÚWNÂˆYˆ
İ›˜Û\
K‹K\™\Ù]H‹JHOHİ›˜Û\
K‹K\›Ùš[OH‹L
HOH
HÂˆÛÛœİÚ\ˆ
œˆHİ˜ÚŠK	ÏIÊH
ÈNÂˆYˆ
İ˜Û\
‹™[š[˜ÙYŠHOHİ˜Û\
‹œ\ÈŠHOHˆİ˜Û\
‹™ÛÛ[™^YJÈŠHOHİ˜Û\
‹™ÙJÈŠHOH
HÈ×Ü™\Ù]Ü\ÈHNÈBˆ[ÙHÈ×Ü™\Ù]Ü\ÈHÈBˆBˆYˆ
İ›˜Û\
K‹KXÛÛ™šYÏH‹JHOH
HÈÛT]HH
ÈNÈBˆ[ÙHYˆ
İ˜Û\
K‹KXÛÛ™šYÈŠHOH	‰ˆH
ÈH\™ØÊHÈÛT]H\™İ–ÊÊÚWNÈBˆ[ÙHYˆ
İ›˜Û\
K‹K]Üš]KXÛÛ™šYÏH‹MJHOH
HÈÕÜš]HHNÈÜš]T]HH
ÈMNÈBˆ[ÙHYˆ
İ˜Û\
K‹K]Üš]KXÛÛ™šYÈŠHOH
HÈÕÜš]HHNÈBˆ[ÙHYˆ
İ˜Û\
K‹KZ[ŠHOHİ˜Û\
K‹ZŠHOH
HÈÒ[HNÈBˆ[ÙHYˆ
İ˜Û\
K‹K[\İXÚX]ÈŠHOH
HÈ\İØÚX]Ê
NÈ™]\›ˆÑWĞÓÓ‘’Q×ÔÕÔÈB‚ˆBˆYˆ
Ò[
HÈ\ØYÙJ
NÈ™]\›ˆÑWĞÓÓ‘’Q×ÔÕÔÈBˆÊˆÜš]WÙY˜][

H\Ù\ÈH^]XÛÙHÛÛ™[[Ûˆ
HİXØÙ\ÜÊK]HØ[\‚ˆ
ˆ™X]È›Û™\›È\ÈœİÜ‹ˆ™]\›š[™È]˜]ÈÛİ[XZÙHHİXØÙ\ÜÙ[ˆ
ˆK]Üš]KXÛÛ™šYÈ˜[›İYÚ[™›ÛİHØ[YHÚ[HH˜Z[YÛ™H^]YÛX[›Kˆ
ˆK™Kˆ^XİH˜XÚİØ\™ËˆX\›İÛÈH^XÚ]İÜÙ[[™[ˆ
‹ÂˆYˆ
ÕÜš]JHÈ™]\›ˆÜš]WÙY˜][
Üš]T]
HOHÈÑWĞÓÓ‘’Q×ÔÕÔˆNÈB‚ˆÊˆX\›Y\ˆZ[ÈÜ›İHHÛÛ™šYÈÈH\™XİÜH˜[YY‘ÛÛ[‘^YHˆÚ[HØ]™\ÈÙ[ˆ
ˆÈ‘ÛÛ[™^YKS˜]]™H‹ˆHÛÈ\™H[šYšYY›İË][ˆ^\İ[™È[œİ[\ÈH[™Yˆ
ˆÛÛ™šYÈ[™\ˆHÛ˜[YNÈYÜ]˜]\ˆ[ˆ™\Ù[[™ÈHœ™\ÚÛ™Kˆ›İ[™È\Âˆ
ˆÛÜYYÜˆ[]YKHHš[HÙY\ÈÛÜšÚ[™ÈÚ\™H]\Ëˆ
‹ÂˆYˆ
ÛT]OH•S
HÂˆÛÛœİÚ\ˆ
šÛYHHÙ][Š’ÓQHŠNÂˆYˆ
ÛYHOH•S	‰ˆ
šÛYHOH	×	ÊHÂˆİ]XÈÚ\ˆÛÌLNÂˆÚ\ˆ™]ÜÌLNÂˆÚ\ˆ™]ÛYØXŞVÌLNÂˆİXİİ]İÂˆÛœš[ŠÛÚ^™[ÙˆÛˆ‰\ËÓXœ˜\KĞ\XØ][Ûˆİ\ÜÑÛÛ[‘^YKÈˆÑWĞÑ‘×ÓQĞPÖWĞTÑSSQKÛYJNÂˆÛœš[Š™]ÜÚ^™[Ùˆ™]Üˆ‰\ËÓXœ˜\KĞ\XØ][Ûˆİ\ÜÑÛÛ[™^YKS˜]]™KÈˆÑWĞÑ‘×ĞTÑSSQKÛYJNÂˆÛœš[Š™]ÛYØXŞKÚ^™[Ùˆ™]ÛYØXŞKˆ‰\ËÓXœ˜\KĞ\XØ][Ûˆİ\ÜÑÛÛ[™^YKS˜]]™KÈˆÑWĞÑ‘×ÓQĞPÖWĞTÑSSQKÛYJNÂˆYˆ
İ]
Û	œİ
HOH	‰ˆİ]
™]Ü	œİ
HOH	‰ˆİ]
™]ÛYØXŞK	œİ
HOH
HÂˆš[Š–ÙÙ]—VØÛÛ™šY×H\Ú[™ÈH™K\™[˜[YHÛÛ™šYÎˆ	\×ˆ‹Û
NÂˆÛT]HÛÂˆBˆBˆB‚ˆÊˆ\ÜÈˆHHš[KÚ]İ™\Üš]OLÛÈH[š\›Û›Y[[Ø^\ÈÚ[œËˆ
‹ÂˆYˆ
ØØ]J\™ØÈˆÈ\™İ–ÌHˆ•SÛT]
JHÂˆ™XYÙš[J
NÂˆH[ÙHYˆ
ÛT]OH•S
HÂˆš[Š–ÙÙ]—VØÛÛ™šY×HKXÛÛ™šYÏI\È›İ›İ[™ˆ‹ÛT]
NÂˆ×Ù\œ›ÜœÊÊÎÂˆH[ÙHÂˆÊˆš\œİ[ˆ›ÈÛÛ™šYÈ[]Ú\™H[™›Û™H\ÚÙY›Ü‹ÛÈÜš]HH[\]H[™ˆ
ˆ[ˆ™XY]ˆ\È\È›İHÛÛ™[šY[˜ÙNÈ]\ÈİÈHÜ	ÜÈ[™YY˜][Âˆ
ˆ™XXÚH^Y\ˆ][‚ˆ
‚ˆ
ˆHY˜][]Û›H^\İÈ[œÚYHHš[HH\Ù\ˆ\È™]™\ˆÙ[™\˜]Y\È›İˆ
ˆHY˜][ˆ[™\ÛÛÚÈHX\ÈHØ\ÙH[ˆÚ[ˆH\Ù\ˆÚ]›ÈÛÛ™šYÈš[Bˆ
ˆÙ]È™]Z[	ÜÈY˜][[œİXY[™™]Z[ÛZ]ÈÔSÓ—ÒS•‘T•ÓÒËÚXÚˆ
ˆXZÙ\ÈİXÚË]\š]™H]ÚİÛˆ][˜]H[™[ˆ]HNLYÜ™YHÛ[\ˆ
ˆÚ][ˆX›İ]KHÙXÛÛ™ËˆHœ™\Ú[œİ[[ˆÜ[œÈÚ]HØ[Y\˜Hİ\š[™Âˆ
ˆ]H›ÛÜ‹‚ˆ
‚ˆ
ˆ˜Z[\™H\È[X™\˜][H›Û‹Y˜][[™™X\‹\Ú[[ˆH™XY[Û›HÓQH]\İˆ
ˆİ[›ÛİÛˆZ[Z[ˆY˜][Ëˆ
‹ÂˆYˆ
Üš]WÙY˜][
•S
HOH	‰‚ˆØØ]J\™ØÈˆÈ\™İ–ÌHˆ•S•S
JHÂˆš[Š–ÙÙ]—VØÛÛ™šY×Hš\œİ[ˆKHÜ›İHHY˜][ÛÛ™šYÎÈY]]È\İWˆŠNÂˆ™XYÙš[J
NÂˆBˆB‚ˆÊˆH™\Ù]Ú]È\™HÛˆ\œÜÙNˆY\ˆHš[K™Y›Ü™HHÛÛ[X[™[™KˆÙYBˆ
ˆÙWÜ™\Ù]Ø\J
H›ÜˆÚH]ÜÚ][Ûˆ\ÈHÚÛH[Kˆ
‹ÂˆÙWÜ™\Ù]Ø\J
NÂ‚ˆÊˆ\ÜÈÈHHÛÛ[X[™[™KÚ]İ™\Üš]OLHÛÈ]™X]ÈH[š\›Û›Y[ˆ
‹Âˆ›Üˆ
HHNÈH\™ØÎÈJÊÊHÂˆÚ\ˆ
˜HH\™İ–ÚWNÂˆÚ\ˆİ–ÍLL—K
™\NÂˆYˆ
İ›˜Û\
K‹KH‹ŠHOH
HÈÛÛ[YNÈBˆYˆ
İ›˜Û\
K‹KXÛÛ™šYÈ‹
HOHİ›˜Û\
K‹K]Üš]KXÛÛ™šYÈ‹M
HOHˆİ˜Û\
K‹KZ[ŠHOHİ˜Û\
K‹K[\İXÚX]ÈŠHOH
HÈÛÛ[YNÈBˆÊˆÛÛœİ[YYHÙWÛ][˜Ú\‹˜ÜÚXÚ[œÈY\ˆ\È™]\›œËˆÚÚ\Y˜]\‚ˆ
ˆ[ˆ[™YˆH][˜Ú\ˆ™YYÈHÛÛ™šYÈ^Y\ˆÈ]™Hš[š\ÚYš\œİÛÂˆ
ˆ]]™\HÛÛ›ÛÜ[œÈÚİÚ[™ÈH˜[YHHš[H[™[š\›Û›Y[™\ÛÛ™Yˆ
ˆËˆ\İ[™È]\™HÛ›HİÜÈ]™Z[™È™\ÜY\ÈHX[›Ü›YYKZÙ^O]˜[YKˆ
‹ÂˆYˆ
İ˜Û\
K‹K[][˜Ú\ˆŠHOH
HÈÛÛ[YNÈBˆÛœš[Šİ‹Ú^™[Ùˆİ‹‰\È‹H
ÈŠNÂˆ\HHİ˜ÚŠİ‹	ÏIÊNÂˆYˆ
\HOH•S
HÂˆYˆ
H
ÈH\™ØÈ	‰ˆ\™İ–ÚH
ÈWVÌHOH	ËIÊHÂˆÚ\ˆ›Ú[™YÍŒNÂˆÛœš[Š›Ú[™YÚ^™[Ùˆ›Ú[™Y‰\ÏI\È‹İ‹\™İ–ÊÊÚWJNÂˆÛœš[Šİ‹Ú^™[Ùˆİ‹‰\È‹›Ú[™Y
NÂˆ\HHİ˜ÚŠİ‹	ÏIÊNÂˆH[ÙHÂˆš[Š–ÙÙ]—VØÛÛ™šY×HYÛ›Üš[™È‰\×ˆ^XİYKZÙ^O]˜[YWˆ‹JNÂˆÛÛ[YNÂˆBˆBˆ
™\HH	×	ÎÂˆÂˆÚ\ˆ–ÍLL—NÂˆÛœš[Š‹Ú^™[Ùˆ‹‰\È‹\H
ÈJNÂˆYˆ
İ›˜Û\
İ‹‘ÑU—È‹JHOH	‰‚ˆİ˜Û\
İ‹œØ]™WÙ\ˆŠHOH	‰ˆİ˜Û\
İ‹œØ]™Y\ˆŠHOH
HÂˆİÙ\ŠŠNÂˆBˆYˆ
X\Jİ‹‹Ê›İ™\Üš]OJ‹ÌJJHÂˆš[Š–ÙÙ]—VØÛÛ™šY×HYÛ›Üš[™È[šÛ›İÛˆ›YÈKI\×ˆ‹İŠNÂˆBˆBˆB‚ˆÊˆÑU—ĞÒPUË\YY\İÛÈ]™X]È›İHš[H[™HÓK‚ˆ
‚ˆ
ˆÚX]È\™HHÛ™H\ÙˆHÛÛ™šYÈ]\È›İ^™\ÜÙY\ÈHÑU—ÈØ]NˆÙ^WØÚX]Âˆ
ˆÜš]\ÈHØ[YIÜÈÚX]›YÈ\œ˜^H\™XİK\™K]\œÙH[YKˆ]ÛÜšÜÈ›ÜˆBˆ
ˆÛÛ™šYÈš[K]]YX[œÈHÚX]Ø[››İİ\š]™H[ˆ^XÊ
HKH[™H][˜Ú\ˆ™[][˜Ú\Âˆ
ˆHš[˜\H™XÚ\Ù[H™XØ]\ÙH[ÜİØ]\È\™H™XYÛ˜ÙH[ÈHİ]XÈ[™Ø[››İ™HÚ[™ÙYˆ
ˆY\Ø\™Ëˆ\ÈØ]H\ÈİÈHÚX]Ù[Xİ[ÛˆÜ›ÜÜÙ\È]›İ[™\KˆØ[YHÛÛ[XK\Ù\\˜]Yˆ
ˆŞ[^\ÈHÚX]ØÙ^K[™]\ÈÚ[\H[™YÈHØ[YH\œÙ\‹ˆ
‹ÂˆÂˆÛÛœİÚ\ˆ
™[˜ÚX]ÈHÙ][Š‘ÑU—ĞÒPUÈŠNÂˆYˆ
[˜ÚX]ÈOH•S	‰ˆ
™[˜ÚX]ÈOH	×	ÊHÂˆÙ^WØÚX]Ê[˜ÚX]ËJNÂˆBˆB‚ˆÙPÛÛ™šYĞ\S][˜Ú\”›Ùš[J
NÂ‚ˆš[Š–ÙÙ]—VØÛÛ™šY×H	\É\ÈÚ[™İÏI\ÈœÏI\ÈÜÏI\ÈÛÛ›ÛÏIYš[\š[™ÏI]Wˆ‹ˆÙWØÛÛ™šY×ÛØYYÈ™š[Hˆˆ››ÈÛÛ™šYÈš[H‹ˆÙWØÛÛ™šY×ÛØYYÈ×ØÙ™Ü]ˆˆ‹ˆÙ][Š‘ÑU—ÕÒS‘ÕÈŠHÈÙ][Š‘ÑU—ÕÒS‘ÕÈŠHˆ™Y˜][‹ˆÙ][Š‘ÑU—Ñ”ÈŠHÈÙ][Š‘ÑU—Ñ”ÈŠHˆ™Y˜][‹ˆÙ][Š‘ÑU—ÔÕTT”ĞSTHŠHÈÙ][Š‘ÑU—ÔÕTT”ĞSTHŠHˆ™Y˜][‹ˆÙWØÛÛ™šY×ØÛÛ›ÛËÛÛ™šYÑš[\š[™ÊNÂˆYˆ
×Ù\œ›ÜœÈˆ
HÂˆš[Š–ÙÙ]—VØÛÛ™šY×H	YÙ][™ÊÊHÙ\™H™Z™XİYHÙYHX›İ™Kˆˆ•HØ[YHÚ[İ\Ú]HY˜][È›ÜˆÜÙK—ˆ‹×Ù\œ›ÜœÊNÂˆBˆ™›\Ú
İİ]
NÂˆ™]\›ˆÂŸB