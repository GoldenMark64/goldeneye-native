/* Launcher-owned timed recordings preserve live input. Plain launches do not
 * change caller-supplied automation gates. No settings are persisted to config. */
#ifndef GE_DEVELOPER_TOOLS_H
#define GE_DEVELOPER_TOOLS_H
#include <stdio.h>
#include <stdlib.h>

static int geDeveloperRecordingFrames(int frames)
{
    return frames == 3600 || frames == 10800 || frames == 18000 ? frames : 10800;
}

static void geDeveloperStartRecording(int frames, const char *run_id, const char *path)
{
    char limit[24];
    snprintf(limit, sizeof limit, "%d", geDeveloperRecordingFrames(frames));
    setenv("GETV_PROP_TELEMETRY", "1", 1);
    setenv("GETV_PROP_TELEMETRY_RUN_ID", run_id, 1);
    setenv("GETV_PROP_TELEMETRY_FILE", path, 1);
    setenv("GETV_EXIT_FRAME", limit, 1);
    setenv("GETV_KEYBOARD_IDLE", "0", 1);
}
#endif
