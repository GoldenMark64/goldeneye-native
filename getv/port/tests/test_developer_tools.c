#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ge_developer_tools.h"
static int failures;
static void check(const char *label, int ok)
{
    printf("%s %s\n", ok ? "PASS" : "FAIL", label);
    failures += !ok;
}
int main(void)
{
    check("invalid recording duration uses the bounded default", geDeveloperRecordingFrames(-1) == 10800);
    check("one minute frame budget remains selectable", geDeveloperRecordingFrames(3600) == 3600);
    check("five minute frame budget remains selectable", geDeveloperRecordingFrames(18000) == 18000);
    setenv("GETV_KEYBOARD_IDLE", "1", 1);
    geDeveloperStartRecording(3600, "synthetic-run", "/synthetic/report.jsonl");
    check("recording enables the producer", strcmp(getenv("GETV_PROP_TELEMETRY"), "1") == 0);
    check("recording keeps manual keyboard and mouse active", strcmp(getenv("GETV_KEYBOARD_IDLE"), "0") == 0);
    check("recording uses the selected frame limit", strcmp(getenv("GETV_EXIT_FRAME"), "3600") == 0);
    check("recording supplies its correlation ID", strcmp(getenv("GETV_PROP_TELEMETRY_RUN_ID"), "synthetic-run") == 0);
    check("recording supplies a separate report destination", strcmp(getenv("GETV_PROP_TELEMETRY_FILE"), "/synthetic/report.jsonl") == 0);
    return failures ? 1 : 0;
}
