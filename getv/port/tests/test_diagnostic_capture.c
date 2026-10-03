/* ROM-free regression for F3 local diagnostic capture.
 *
 * Includes the production implementation directly and replaces every game-facing provider with
 * bounded synthetic state. The test writes only into the host temp directory and deletes its
 * capture tree before exiting.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <process.h>
#define TEST_MKDIR(p) _mkdir(p)
#define TEST_RMDIR(p) _rmdir(p)
#define TEST_PID() ((long)_getpid())
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#define TEST_MKDIR(p) mkdir((p), 0700)
#define TEST_RMDIR(p) rmdir(p)
#define TEST_PID() ((long)getpid())
#endif

#include "../src/ge_diagnostic_capture.c"

static char fake_user_dir[512];
int g_ControlsLockedFlag;

static int failures;

static void check(const char *what, int ok)
{
    if (ok) {
        printf("  ok    %s\n", what);
    } else {
        printf("  FAIL  %s\n", what);
        failures++;
    }
}

static int make_tree(const char *path)
{
    char buf[1024];
    size_t i, n = strlen(path);
    if (n >= sizeof buf) return -1;
    memcpy(buf, path, n + 1);
    for (i = 1; i < n; i++) {
        if (buf[i] == '/' || buf[i] == '\\') {
            char save = buf[i];
            buf[i] = '\0';
            if (strlen(buf) > 0) (void)TEST_MKDIR(buf);
            buf[i] = save;
        }
    }
    return TEST_MKDIR(buf) == 0 || ge_diag_path_exists(buf) ? 0 : -1;
}

int gePortUserDataDir(const char *org, const char *app, char *out, size_t outsz)
{
    (void)org;
    (void)app;
    if (snprintf(out, outsz, "%s", fake_user_dir) >= (int)outsz) return -1;
    return 0;
}

int gePortMakeDirTree(const char *path, unsigned mode)
{
    (void)mode;
    return make_tree(path);
}

int bossGetStageNum(void) { return 11; }
int lvlGetSelectedDifficulty(void) { return 1; }
int checkGamePaused(void) { return 0; }
int gePortObjectiveCount(void) { return 2; }
int gePortObjectiveStatus(int index, int *out_status)
{
    if (!out_status || index < 0 || index > 1) return 0;
    *out_status = index == 0 ? 1 : 2;
    return 1;
}
unsigned long gePlayerTick(void) { return 1234; }

int gePlayerStateGet(int slot, GePlayerState *out)
{
    if (!out || slot < 0 || slot >= GE_MAX_SLOTS) return 0;
    memset(out, 0, sizeof *out);
    if (slot != 0) return 0;
    out->present = 1;
    out->fields = GE_ST_POSITION | GE_ST_ROOM | GE_ST_HEALTH | GE_ST_WEAPON;
    out->x = 10.0f; out->y = 20.0f; out->z = 30.0f;
    out->room = 7;
    out->health = 0.75f;
    out->armour = 0.25f;
    out->weapon = 4;
    out->ammo_clip = 7;
    out->ammo_reserve = 28;
    return 1;
}

size_t geEventRecentCopy(GeEventRecord *out, size_t capacity)
{
    if (!out || capacity < 2) return 0;
    memset(out, 0, sizeof(*out) * 2);
    out[0].sequence = 41; out[0].frame = 75; out[0].type = GE_EV_ROOM_CHANGE;
    out[0].a = 0; out[0].b = 7; out[0].c = 6;
    out[1].sequence = 42; out[1].frame = 76; out[1].type = GE_EV_GUARD_NEAR;
    out[1].a = 0; out[1].b = 12; out[1].c = 300;
    return 2;
}

const char *geEventName(GeEventType type)
{
    if (type == GE_EV_ROOM_CHANGE) return "room_change";
    if (type == GE_EV_GUARD_NEAR) return "guard_near";
    return "other";
}

int geInputPreset(void) { return GE_PRESET_MODERN; }
const char *gePresetName(int preset) { return preset == GE_PRESET_MODERN ? "modern" : "n64"; }
const char *geActionName(int act)
{
    static const char *const names[GE_ACT_MAX] = {
        "fire", "aim", "use", "reload", "crouch", "weapon_next", "weapon_prev", "pause"
    };
    return act >= 0 && act < GE_ACT_MAX ? names[act] : "?";
}
const char *geActionEnvSuffix(int act)
{
    static const char *const names[GE_ACT_MAX] = {
        "FIRE", "AIM", "USE", "RELOAD", "CROUCH", "WEAPON_NEXT", "WEAPON_PREV", "PAUSE"
    };
    return act >= 0 && act < GE_ACT_MAX ? names[act] : "?";
}
const char *geSourceName(int src)
{
    return src == GE_SRC_RT ? "rt" : (src == GE_SRC_LT ? "lt" : "none");
}
int geBindSrc(int player, int act)
{
    (void)player;
    return act == GE_ACT_AIM ? GE_SRC_LT : GE_SRC_RT;
}
const char *gePresetKeys(int preset, int act)
{
    (void)preset; (void)act;
    return "synthetic-key";
}
const char *geAxisName(int axis)
{
    static const char *const names[GE_AXIS_MAX] = {
        "forward", "backward", "strafe_left", "strafe_right",
        "look_up", "look_down", "look_left", "look_right"
    };
    return axis >= 0 && axis < GE_AXIS_MAX ? names[axis] : "?";
}
const char *geAxisEnvSuffix(int axis)
{
    static const char *const names[GE_AXIS_MAX] = {
        "FORWARD", "BACKWARD", "STRAFE_LEFT", "STRAFE_RIGHT",
        "LOOK_UP", "LOOK_DOWN", "LOOK_LEFT", "LOOK_RIGHT"
    };
    return axis >= 0 && axis < GE_AXIS_MAX ? names[axis] : "?";
}
const char *gePresetAxisKeys(int preset, int axis)
{
    (void)preset; (void)axis;
    return "synthetic-axis";
}

static int read_all(const char *path, char *out, size_t cap)
{
    FILE *f = fopen(path, "rb");
    size_t n;
    if (!f || cap == 0) {
        if (f) fclose(f);
        return 0;
    }
    n = fread(out, 1, cap - 1, f);
    out[n] = '\0';
    fclose(f);
    return 1;
}

int main(void)
{
    const char *tmp;
    char capture_dir[1024];
    char path[1200];
    char text[16384];
    FILE *shot;

#ifdef _WIN32
    tmp = getenv("TEMP");
    if (!tmp || !*tmp) tmp = ".";
#else
    tmp = getenv("TMPDIR");
    if (!tmp || !*tmp) tmp = "/tmp";
#endif
    snprintf(fake_user_dir, sizeof fake_user_dir, "%s/ge_diag_test_%ld", tmp, TEST_PID());

    printf("F3 diagnostic capture\n\n");
    gePortDiagnosticRequest();
    check("request becomes active", ge_diag.active == 1);
    check("capture lives under user-data directory",
          strncmp(ge_diag.directory, fake_user_dir, strlen(fake_user_dir)) == 0);
    snprintf(capture_dir, sizeof capture_dir, "%s", ge_diag.directory);

    shot = fopen(ge_diag.screenshot, "wb");
    if (shot) {
        fwrite("BM", 1, 2, shot);
        fclose(shot);
    }
    gePortDiagnosticScreenshotComplete(shot != NULL, 1280, 720);
    gePortDiagnosticFinalizeFrame(77);
    check("finalize clears pending capture", ge_diag.active == 0);

    snprintf(path, sizeof path, "%s/session.json", capture_dir);
    check("session.json written", read_all(path, text, sizeof text));
    check("session records local-only policy", strstr(text, "local capture; not uploaded") != NULL);
    check("session records exact render frame", strstr(text, "\"render_frame\": 77") != NULL);
    check("session records screenshot dimensions", strstr(text, "\"width\":1280,\"height\":720") != NULL);
    check("session records resolved bindings", strstr(text, "\"preset\": \"modern\"") != NULL);
    check("session does not invent a build commit", strstr(text, "\"build_commit\": null") != NULL);

    snprintf(path, sizeof path, "%s/state.json", capture_dir);
    check("state.json written", read_all(path, text, sizeof text));
    check("state records player position", strstr(text, "\"position\":[10,20,30]") != NULL);
    check("state records objective result", strstr(text, "\"status\":[1,2]") != NULL);

    snprintf(path, sizeof path, "%s/events.jsonl", capture_dir);
    check("events.jsonl written", read_all(path, text, sizeof text));
    check("events include bounded typed history", strstr(text, "\"type\":\"guard_near\"") != NULL);

    snprintf(path, sizeof path, "%s/report.md", capture_dir);
    check("report.md written", read_all(path, text, sizeof text));
    check("report explicitly says nothing uploaded", strstr(text, "Nothing was uploaded") != NULL);

    snprintf(path, sizeof path, "%s/screenshot.bmp", capture_dir); remove(path);
    snprintf(path, sizeof path, "%s/session.json", capture_dir); remove(path);
    snprintf(path, sizeof path, "%s/state.json", capture_dir); remove(path);
    snprintf(path, sizeof path, "%s/events.jsonl", capture_dir); remove(path);
    snprintf(path, sizeof path, "%s/report.md", capture_dir); remove(path);
    TEST_RMDIR(capture_dir);
    snprintf(path, sizeof path, "%s/diagnostics", fake_user_dir); TEST_RMDIR(path);
    TEST_RMDIR(fake_user_dir);

    printf("\n%s -- %d failure(s)\n", failures ? "FAILED" : "PASSED", failures);
    return failures ? 1 : 0;
}
