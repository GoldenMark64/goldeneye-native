#include "ge_diagnostic_capture.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <process.h>
#define GE_DIAG_GETPID() ((long)_getpid())
#else
#include <unistd.h>
#define GE_DIAG_GETPID() ((long)getpid())
#endif

#include "port_paths.h"
#include "ge_player_api.h"
#include "ge_event.h"
#include "ge_actions.h"
#include "ge_bindings.h"

#define GE_DIAG_PATH_MAX 1024
#define GE_DIAG_OBJECTIVES_MAX 32

typedef struct GeDiagnosticCapture {
    int active;
    unsigned long sequence;
    char directory[GE_DIAG_PATH_MAX];
    char screenshot[GE_DIAG_PATH_MAX];
    int screenshot_status;
    int screenshot_width;
    int screenshot_height;
    char created_utc[32];
} GeDiagnosticCapture;

static GeDiagnosticCapture ge_diag;
static unsigned long ge_diag_next_sequence;

extern int bossGetStageNum(void);
extern int lvlGetSelectedDifficulty(void);
extern int checkGamePaused(void);
extern int g_ControlsLockedFlag;
extern int gePortObjectiveCount(void);
extern int gePortObjectiveStatus(int index, int *out_status);

static const char *ge_diag_platform(void)
{
#if defined(_WIN32)
    return "windows";
#elif defined(__APPLE__)
    return "apple";
#elif defined(__linux__)
    return "linux";
#else
    return "unknown";
#endif
}

static const char *ge_diag_arch(void)
{
#if defined(__x86_64__) || defined(_M_X64)
    return "x86_64";
#elif defined(__aarch64__) || defined(_M_ARM64)
    return "arm64";
#elif defined(__i386__) || defined(_M_IX86)
    return "x86";
#else
    return "unknown";
#endif
}

static const char *ge_diag_renderer(void)
{
#if defined(RAPI_METAL)
    return "metal";
#elif defined(RAPI_GL)
    return "opengl";
#else
    return "unknown";
#endif
}

static int ge_diag_path_exists(const char *path)
{
    struct stat st;
    return path != NULL && stat(path, &st) == 0;
}

static int ge_diag_join(char *out, size_t out_size, const char *dir, const char *name)
{
    int n;
    if (out == NULL || out_size == 0 || dir == NULL || name == NULL) { return 0; }
    n = snprintf(out, out_size, "%s/%s", dir, name);
    return n >= 0 && (size_t)n < out_size;
}

static void ge_diag_json_string(FILE *out, const char *value)
{
    const unsigned char *p = (const unsigned char *)(value != NULL ? value : "");
    fputc('"', out);
    while (*p) {
        unsigned char ch = *p++;
        switch (ch) {
        case '"': fputs("\\\"", out); break;
        case '\\': fputs("\\\\", out); break;
        case '\n': fputs("\\n", out); break;
        case '\r': fputs("\\r", out); break;
        case '\t': fputs("\\t", out); break;
        default:
            if (ch < 0x20) {
                fprintf(out, "\\u%04x", (unsigned)ch);
            } else {
                fputc((int)ch, out);
            }
            break;
        }
    }
    fputc('"', out);
}

static void ge_diag_write_safe_settings(FILE *out)
{
    static const char *const keys[] = {
        "GETV_STAGE", "GETV_PROFILE_PLUS", "GETV_BASE_GAME", "GETV_PRESET",
        "GETV_WINDOW", "GETV_FULLSCREEN", "GETV_FPS", "GETV_REALCLOCK",
        "GETV_TICKFIELDS", "GETV_SUPERSAMPLE", "GETV_FILTERING", "GETV_POINT_FILTER",
        "GETV_WIDESCREEN", "GETV_FOV", "GETV_MSAA", "GETV_ANISO",
        "GETV_MIPMAPS", "GETV_HD_TEXTURES", "GETV_PARALLAX", "GETV_FXAA",
        "GETV_CRT", "GETV_CROSSHAIR_SCALE", "GETV_CROSSHAIR_COLOR", "GETV_GAMEPAD",
        "GETV_DEADZONE", "GETV_INVERTLOOK", "GETV_CONTROLS", "GETV_INPUT_PRESET",
        "GETV_CROUCH_MODE", "GETV_AIM_MODE", "GETV_MOUSE", "GETV_MOUSE_MODE",
        "GETV_MOUSE_SENS", "GETV_MOUSE_INVERT", "GETV_KEYBOARD", "GETV_BLOOD",
        "GETV_BLOOD_LIMIT", "GETV_GIBS", "GETV_COOP", "GETV_COOP_FRIENDLYFIRE",
        "GETV_RULESET", "GETV_HORDE", "GETV_AUDIO_DEBUG"
    };
    size_t i;

    fputs("{\n", out);
    for (i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        const char *value = getenv(keys[i]);
        fputs("    ", out);
        ge_diag_json_string(out, keys[i]);
        fputs(": ", out);
        if (value != NULL) {
            ge_diag_json_string(out, value);
        } else {
            fputs("null", out);
        }
        fputs(i + 1 < sizeof(keys) / sizeof(keys[0]) ? ",\n" : "\n", out);
    }
    fputs("  }", out);
}

static void ge_diag_write_bindings(FILE *out)
{
    int preset = geInputPreset();
    int player;
    int act;
    int axis;

    fputs("{\n    \"preset\": ", out);
    ge_diag_json_string(out, gePresetName(preset));
    fputs(",\n    \"pad\": [", out);
    for (player = 0; player < GE_MAX_SLOTS; player++) {
        if (player != 0) { fputc(',', out); }
        fputs("\n      {", out);
        for (act = 0; act < GE_ACT_MAX; act++) {
            if (act != 0) { fputc(',', out); }
            fputs("\n        ", out);
            ge_diag_json_string(out, geActionName(act));
            fputs(": ", out);
            ge_diag_json_string(out, geSourceName(geBindSrc(player, act)));
        }
        fputs("\n      }", out);
    }
    fputs("\n    ],\n    \"keyboard\": {", out);
    for (act = 0; act < GE_ACT_MAX; act++) {
        char key[64];
        const char *value;
        snprintf(key, sizeof key, "GETV_KEY_%s", geActionEnvSuffix(act));
        value = getenv(key);
        if (value == NULL) { value = gePresetKeys(preset, act); }
        if (act != 0) { fputc(',', out); }
        fputs("\n      ", out);
        ge_diag_json_string(out, geActionName(act));
        fputs(": ", out);
        ge_diag_json_string(out, value);
    }
    fputs("\n    },\n    \"axes\": {", out);
    for (axis = 0; axis < GE_AXIS_MAX; axis++) {
        char key[64];
        const char *value;
        snprintf(key, sizeof key, "GETV_KEY_%s", geAxisEnvSuffix(axis));
        value = getenv(key);
        if (value == NULL) { value = gePresetAxisKeys(preset, axis); }
        if (axis != 0) { fputc(',', out); }
        fputs("\n      ", out);
        ge_diag_json_string(out, geAxisName(axis));
        fputs(": ", out);
        ge_diag_json_string(out, value);
    }
    fputs("\n    }\n  }", out);
}

static int ge_diag_write_state(const char *path, unsigned long render_frame)
{
    FILE *out = fopen(path, "wb");
    int stage = bossGetStageNum();
    int difficulty = lvlGetSelectedDifficulty();
    int total = gePortObjectiveCount();
    int captured;
    int i;

    if (out == NULL) { return 0; }
    if (total < 0) { total = 0; }
    captured = total < GE_DIAG_OBJECTIVES_MAX ? total : GE_DIAG_OBJECTIVES_MAX;

    fprintf(out,
            "{\n"
            "  \"schema_version\": 1,\n"
            "  \"render_frame\": %lu,\n"
            "  \"game_tick\": %lu,\n"
            "  \"stage\": %d,\n"
            "  \"difficulty\": %d,\n"
            "  \"paused\": %s,\n"
            "  \"controls_locked\": %s,\n"
            "  \"players\": [\n",
            render_frame, gePlayerTick(), stage, difficulty,
            checkGamePaused() ? "true" : "false",
            g_ControlsLockedFlag ? "true" : "false");

    for (i = 0; i < GE_MAX_SLOTS; i++) {
        GePlayerState st;
        int available = gePlayerStateGet(i, &st);
        fprintf(out, "    {\"slot\":%d,\"available\":%s", i, available ? "true" : "false");
        if (available) {
            fprintf(out, ",\"present\":%s,\"fields\":%u",
                    st.present ? "true" : "false", st.fields);
            if (st.fields & GE_ST_POSITION) {
                fprintf(out, ",\"position\":[%.6g,%.6g,%.6g]",
                        (double)st.x, (double)st.y, (double)st.z);
            }
            if (st.fields & GE_ST_ROOM) {
                fprintf(out, ",\"room\":%d", st.room);
            }
            if (st.fields & GE_ST_ANGLE) {
                fprintf(out, ",\"angle\":%.6g", (double)st.angle);
            }
            if (st.fields & GE_ST_HEALTH) {
                fprintf(out, ",\"health\":%.6g,\"armour\":%.6g,\"dead\":%s",
                        (double)st.health, (double)st.armour, st.dead ? "true" : "false");
            }
            if (st.fields & GE_ST_WEAPON) {
                fprintf(out, ",\"weapon\":%d,\"ammo_clip\":%d,\"ammo_reserve\":%d",
                        st.weapon, st.ammo_clip, st.ammo_reserve);
            }
            if (st.fields & GE_ST_SCORE) {
                fprintf(out, ",\"kills\":%d,\"deaths\":%d,\"shots\":%d",
                        st.kills, st.deaths, st.shots);
            }
        }
        fprintf(out, "}%s\n", i + 1 < GE_MAX_SLOTS ? "," : "");
    }

    fprintf(out,
            "  ],\n"
            "  \"objectives\": {\"total\":%d,\"captured\":%d,\"truncated\":%s,\"status\":[",
            total, captured, total > captured ? "true" : "false");
    for (i = 0; i < captured; i++) {
        int status = 0;
        if (i != 0) { fputc(',', out); }
        if (gePortObjectiveStatus(i, &status)) {
            fprintf(out, "%d", status);
        } else {
            fputs("null", out);
        }
    }
    fputs("]}\n}\n", out);
    fclose(out);
    return 1;
}

static int ge_diag_write_events(const char *path)
{
    GeEventRecord records[GE_EVENT_HISTORY_MAX];
    size_t count = geEventRecentCopy(records, GE_EVENT_HISTORY_MAX);
    size_t i;
    FILE *out = fopen(path, "wb");

    if (out == NULL) { return 0; }
    for (i = 0; i < count; i++) {
        fprintf(out, "{\"sequence\":%llu,\"frame\":%d,\"type\":",
                records[i].sequence, records[i].frame);
        ge_diag_json_string(out, geEventName(records[i].type));
        fprintf(out, ",\"a\":%d,\"b\":%d,\"c\":%d}\n",
                records[i].a, records[i].b, records[i].c);
    }
    fclose(out);
    return 1;
}

static int ge_diag_write_session(const char *path, unsigned long render_frame,
                                 int state_ok, int events_ok, int report_ok)
{
    FILE *out = fopen(path, "wb");
    int stage = bossGetStageNum();
    int difficulty = lvlGetSelectedDifficulty();

    if (out == NULL) { return 0; }

    fputs("{\n  \"schema_version\": 1,\n  \"publication_status\": ", out);
    ge_diag_json_string(out, "local capture; not uploaded");
    fputs(",\n  \"created_utc\": ", out);
    ge_diag_json_string(out, ge_diag.created_utc);
    fprintf(out, ",\n  \"capture_sequence\": %lu,\n  \"platform\": ", ge_diag.sequence);
    ge_diag_json_string(out, ge_diag_platform());
    fputs(",\n  \"architecture\": ", out);
    ge_diag_json_string(out, ge_diag_arch());
    fputs(",\n  \"renderer\": ", out);
    ge_diag_json_string(out, ge_diag_renderer());
    fprintf(out,
            ",\n  \"build_commit\": null,"
            "\n  \"build_compatibility_id\": null,"
            "\n  \"render_frame\": %lu,"
            "\n  \"game_tick\": %lu,"
            "\n  \"stage\": %d,"
            "\n  \"difficulty\": %d,"
            "\n  \"screenshot\": {"
            "\"file\":\"screenshot.bmp\","
            "\"format\":\"native_bmp_pending_collector_normalization\","
            "\"captured\":%s,"
            "\"width\":%d,\"height\":%d},"
            "\n  \"artifacts\": {"
            "\"state.json\":%s,\"events.jsonl\":%s,\"report.md\":%s},"
            "\n  \"settings\": ",
            render_frame, gePlayerTick(), stage, difficulty,
            ge_diag.screenshot_status == 1 ? "true" : "false",
            ge_diag.screenshot_width, ge_diag.screenshot_height,
            state_ok ? "true" : "false", events_ok ? "true" : "false",
            report_ok ? "true" : "false");
    ge_diag_write_safe_settings(out);
    fputs(",\n  \"bindings\": ", out);
    ge_diag_write_bindings(out);
    fputs(",\n  \"notes\": "
          "\"Native capture v1 is local-only. Build identity is unavailable until the binary "
          "exports it. Run tools/collect_bug_report.py before publication; "
          "the collector is responsible for sanitization, PNG normalization, hashes, and review.\""
          "\n}\n", out);

    fclose(out);
    return 1;
}

static int ge_diag_write_report(const char *path, unsigned long render_frame)
{
    FILE *out = fopen(path, "wb");
    if (out == NULL) { return 0; }

    fprintf(out,
            "# GoldenEye-Native diagnostic capture\n\n"
            "**Local capture only. Nothing was uploaded.**\n\n"
            "- Capture sequence: %lu\n"
            "- Created UTC: %s\n"
            "- Render frame: %lu\n"
            "- Game tick: %lu\n"
            "- Stage: %d\n"
            "- Difficulty: %d\n"
            "- Renderer: %s\n"
            "- Screenshot: %s\n\n"
            "## What happened\n\n"
            "_Describe the visible problem._\n\n"
            "## What should have happened\n\n"
            "_Describe the expected behavior._\n\n"
            "## What I did just before it happened\n\n"
            "_Add the shortest reliable reproduction steps._\n\n"
            "## Captured files\n\n"
            "- session.json - tested-session identity and safe effective settings\n"
            "- state.json - bounded player/objective state for this frame\n"
            "- events.jsonl - bounded typed event history leading into this frame\n"
            "- screenshot.bmp - native renderer capture; normalize with the collector before sharing\n",
            ge_diag.sequence, ge_diag.created_utc, render_frame, gePlayerTick(),
            bossGetStageNum(), lvlGetSelectedDifficulty(), ge_diag_renderer(),
            ge_diag.screenshot_status == 1 ? "captured" : "not captured");
    fclose(out);
    return 1;
}

void gePortDiagnosticRequest(void)
{
    char base[GE_DIAG_PATH_MAX];
    char stamp[32] = "unknown-time";
    time_t now;
    struct tm *utc;
    unsigned long seq;
    int attempt;

    if (ge_diag.active) {
        fputs("[getv][diag] capture already pending; F3 ignored\n", stderr);
        fflush(stderr);
        return;
    }

    if (gePortUserDataDir("Goldeneye-Native", "Goldeneye-Native", base, sizeof base) != 0) {
        fputs("[getv][diag] capture failed: user-data directory unavailable\n", stderr);
        fflush(stderr);
        return;
    }

    now = time(NULL);
    utc = gmtime(&now);
    if (utc != NULL) {
        (void)strftime(stamp, sizeof stamp, "%Y%m%dT%H%M%SZ", utc);
    }

    memset(&ge_diag, 0, sizeof ge_diag);
    for (attempt = 0; attempt < 100; attempt++) {
        seq = ++ge_diag_next_sequence;
        if (snprintf(ge_diag.directory, sizeof ge_diag.directory,
                     "%s/diagnostics/capture-%s-p%ld-%04lu",
                     base, stamp, GE_DIAG_GETPID(), seq) >= (int)sizeof ge_diag.directory) {
            fputs("[getv][diag] capture failed: path too long\n", stderr);
            return;
        }
        if (!ge_diag_path_exists(ge_diag.directory)) {
            ge_diag.sequence = seq;
            break;
        }
    }

    if (attempt == 100 || gePortMakeDirTree(ge_diag.directory, 0700) != 0) {
        fputs("[getv][diag] capture failed: cannot create capture directory\n", stderr);
        fflush(stderr);
        memset(&ge_diag, 0, sizeof ge_diag);
        return;
    }

    snprintf(ge_diag.created_utc, sizeof ge_diag.created_utc, "%s", stamp);
    if (!ge_diag_join(ge_diag.screenshot, sizeof ge_diag.screenshot,
                      ge_diag.directory, "screenshot.bmp")) {
        fputs("[getv][diag] capture failed: screenshot path too long\n", stderr);
        memset(&ge_diag, 0, sizeof ge_diag);
        return;
    }

    ge_diag.active = 1;
    ge_diag.screenshot_status = 0;

    fprintf(stderr, "[getv][diag] F3 capture requested -> %s\n"
                    "[getv][diag] local only; nothing uploaded\n",
            ge_diag.directory);
    fflush(stderr);
}

const char *gePortDiagnosticScreenshotPath(void)
{
    if (!ge_diag.active || ge_diag.screenshot_status != 0) { return NULL; }
    return ge_diag.screenshot;
}

void gePortDiagnosticScreenshotComplete(int success, int width, int height)
{
    if (!ge_diag.active || ge_diag.screenshot_status != 0) { return; }
    ge_diag.screenshot_status = success ? 1 : -1;
    ge_diag.screenshot_width = success ? width : 0;
    ge_diag.screenshot_height = success ? height : 0;
}

void gePortDiagnosticFinalizeFrame(unsigned long render_frame)
{
    char path[GE_DIAG_PATH_MAX];
    int state_ok = 0;
    int events_ok = 0;
    int session_ok = 0;
    int report_ok = 0;

    if (!ge_diag.active) { return; }

    if (ge_diag_join(path, sizeof path, ge_diag.directory, "state.json")) {
        state_ok = ge_diag_write_state(path, render_frame);
    }
    if (ge_diag_join(path, sizeof path, ge_diag.directory, "events.jsonl")) {
        events_ok = ge_diag_write_events(path);
    }
    if (ge_diag_join(path, sizeof path, ge_diag.directory, "report.md")) {
        report_ok = ge_diag_write_report(path, render_frame);
    }
    if (ge_diag_join(path, sizeof path, ge_diag.directory, "session.json")) {
        session_ok = ge_diag_write_session(path, render_frame, state_ok, events_ok, report_ok);
    }

    fprintf(stderr,
            "[getv][diag] F3 capture frame=%lu screenshot=%s state=%s events=%s "
            "session=%s report=%s\n"
            "[getv][diag] review locally: %s\n",
            render_frame,
            ge_diag.screenshot_status == 1 ? "ok" :
                (ge_diag.screenshot_status < 0 ? "failed" : "unavailable"),
            state_ok ? "ok" : "failed",
            events_ok ? "ok" : "failed",
            session_ok ? "ok" : "failed",
            report_ok ? "ok" : "failed",
            ge_diag.directory);
    fflush(stderr);

    memset(&ge_diag, 0, sizeof ge_diag);
}
