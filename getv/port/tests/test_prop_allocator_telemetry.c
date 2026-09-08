#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ge_prop_allocator_telemetry.c"

static int failures;

static void check_i(const char *what, long long got, long long want)
{
    if (got == want) {
        printf("  ok    %-62s %lld\n", what, got);
    } else {
        printf("  FAIL  %-62s got %lld want %lld\n", what, got, want);
        failures++;
    }
}

static void check_contains(const char *what, const char *text, const char *needle)
{
    if (strstr(text, needle) != NULL) {
        printf("  ok    %s\n", what);
    } else {
        printf("  FAIL  %s\n        missing: %s\n", what, needle);
        failures++;
    }
}

static void test_reset_state(void)
{
    memset(&ge_prop_telemetry, 0, sizeof ge_prop_telemetry);
    ge_prop_telemetry.configured = 1;
    ge_prop_telemetry.enabled = 1;
    ge_prop_copy_token(ge_prop_telemetry.run_id, "run-reset", "invalid");
    gePortPropAllocatorReset(4, 3);

    check_i("reset advances the pool epoch", ge_prop_telemetry.pool_epoch, 1);
    check_i("reset records allocator capacity", ge_prop_telemetry.capacity, 4);
    check_i("reset starts with no allocated slots", ge_prop_telemetry.current_allocated, 0);
    check_i("reset starts with every slot free", ge_prop_telemetry.free_slots, 4);
    check_i("reset initializes lifetime minimum free", ge_prop_telemetry.minimum_free_slots, 4);
    check_i("reset records the separate onscreen array capacity",
            ge_prop_telemetry.onscreen_capacity, 3);
}

static void test_allocate_free_and_high_water(void)
{
    gePortPropAllocatorAllocated(0);
    gePortPropAllocatorAllocated(1);
    gePortPropAllocatorFreed(1);
    gePortPropAllocatorAllocated(2);

    check_i("three successful allocator calls are counted",
            ge_prop_telemetry.allocation_successes, 3);
    check_i("one free call is counted", ge_prop_telemetry.free_calls, 1);
    check_i("two slots remain allocated", ge_prop_telemetry.current_allocated, 2);
    check_i("high water does not fall after a free", ge_prop_telemetry.high_water_allocated, 2);
    check_i("lifetime minimum free does not rise after a free",
            ge_prop_telemetry.minimum_free_slots, 2);
    check_i("slot accounting remains consistent", ge_prop_telemetry.slot_tracking_consistent, 1);
}

static void test_stage_ready_and_post_ready_counts(void)
{
    FILE *capture = tmpfile();
    char line[GE_PROP_OUTPUT_MAX];

    check_i("temporary output stream opens", capture != NULL, 1);
    if (capture == NULL) { return; }
    ge_prop_telemetry_output = capture;
    gePortPropAllocatorStageReady("us", 34, 0, 1, 0, 0);
    gePortPropAllocatorAllocated(3);
    gePortPropAllocatorFreed(3);
    gePortPropAllocatorFrame(2);
    gePortPropAllocatorRunFinal(34, 0, 1, 7, 9);

    rewind(capture);
    check_i("stage-ready record can be read", fgets(line, sizeof line, capture) != NULL, 1);
    check_contains("stage-ready record uses the versioned schema", line,
                   "\"schema\":\"goldeneye-native.prop-allocator\",\"version\":1");
    check_contains("stage-ready record carries the caller run token", line,
                   "\"phase\":\"stage-ready\",\"runId\":\"run-reset\"");
    check_contains("stage-ready record separates build and stage identity", line,
                   "\"buildVariant\":\"us\",\"stageId\":34");
    check_contains("stage-ready captures the post-setup occupancy", line,
                   "\"stageReadyAllocated\":2,\"postStageReadyHighWaterAllocated\":2");

    check_i("run-final record can be read", fgets(line, sizeof line, capture) != NULL, 1);
    check_contains("run-final record advances sequence deterministically", line,
                   "\"phase\":\"run-final\",\"runId\":\"run-reset\",\"sequence\":2");
    check_contains("run-final keeps raw post-ready allocator counters separate", line,
                   "\"postStageReadyAllocationCalls\":1,"
                   "\"postStageReadyAllocationSuccesses\":1,"
                   "\"postStageReadyAllocationFailures\":0,"
                   "\"postStageReadyFreeCalls\":1");
    check_contains("run-final records separate onscreen current and high water", line,
                   "\"onscreen\":{\"arrayCapacity\":3,\"currentCount\":2,"
                   "\"highWaterCount\":2}");
    check_contains("run-final reports but does not interpret invariants", line,
                   "\"invariants\":{\"accountingConsistent\":true,"
                   "\"slotTrackingConsistent\":true,\"onscreenWithinCapacity\":true}");
    check_i("only the two lifecycle records are emitted", fgets(line, sizeof line, capture) == NULL,
            1);
    fclose(capture);
    ge_prop_telemetry_output = NULL;
}

static void test_exhaustion_and_invariant_failures(void)
{
    memset(&ge_prop_telemetry, 0, sizeof ge_prop_telemetry);
    ge_prop_telemetry.configured = 1;
    ge_prop_telemetry.enabled = 1;
    ge_prop_copy_token(ge_prop_telemetry.run_id, "run-exhaustion", "invalid");
    gePortPropAllocatorReset(2, 2);
    gePortPropAllocatorAllocated(0);
    gePortPropAllocatorAllocated(1);
    gePortPropAllocatorAllocationFailed();

    check_i("exhaustion counts the failed allocator call", ge_prop_telemetry.allocation_calls, 3);
    check_i("exhaustion counts one failure", ge_prop_telemetry.allocation_failures, 1);
    check_i("exhaustion reaches zero free slots", ge_prop_telemetry.free_slots, 0);
    check_i("exhaustion records the full-pool high water", ge_prop_telemetry.high_water_allocated, 2);
    check_i("exhaustion records zero as lifetime minimum free",
            ge_prop_telemetry.minimum_free_slots, 0);
    check_i("a failure at zero free preserves accounting consistency",
            ge_prop_telemetry.accounting_consistent, 1);

    gePortPropAllocatorAllocated(1);
    check_i("a duplicate allocation is detected", ge_prop_telemetry.slot_tracking_consistent, 0);
    check_i("a duplicate allocation invalidates aggregate accounting",
            ge_prop_telemetry.accounting_consistent, 0);

    gePortPropAllocatorFrame(2);
    check_i("onscreen count equal to array length is outside the terminator-safe range",
            ge_prop_telemetry.onscreen_within_capacity, 0);
}

static void test_tokens_and_disabled_behavior(void)
{
    FILE *capture = tmpfile();
    long length;

    check_i("token accepts the editor correlation alphabet",
            ge_prop_token_is_valid("run-allocator_001:v1"), 1);
    check_i("token rejects a leading punctuation character", ge_prop_token_is_valid("-run"), 0);
    check_i("token rejects path separators", ge_prop_token_is_valid("../private/report"), 0);
    check_i("token rejects whitespace", ge_prop_token_is_valid("run allocator"), 0);
    check_i("token rejects an empty value", ge_prop_token_is_valid(""), 0);

    unsetenv("GETV_PROP_TELEMETRY");
    unsetenv("GETV_PROP_TELEMETRY_RUN_ID");
    memset(&ge_prop_telemetry, 0, sizeof ge_prop_telemetry);
    ge_prop_telemetry_output = capture;
    gePortPropAllocatorReset(600, 500);
    gePortPropAllocatorAllocated(0);
    gePortPropAllocatorStageReady("us", 34, 0, 1, 0, 0);
    gePortPropAllocatorFrame(9);
    gePortPropAllocatorRunFinal(34, 0, 1, 1, 1);
    fflush(capture);
    fseek(capture, 0, SEEK_END);
    length = ftell(capture);
    check_i("disabled production hooks emit no telemetry", length, 0);
    check_i("disabled production hooks keep the sequence untouched",
            ge_prop_telemetry.sequence, 0);

    setenv("GETV_PROP_TELEMETRY", "1", 1);
    setenv("GETV_PROP_TELEMETRY_RUN_ID", "../private/report", 1);
    memset(&ge_prop_telemetry, 0, sizeof ge_prop_telemetry);
    gePortPropAllocatorReset(600, 500);
    check_i("malformed correlation input leaves telemetry disabled",
            ge_prop_telemetry.enabled, 0);
    check_i("malformed correlation input is checked only once",
            ge_prop_telemetry.configured, 1);

    setenv("GETV_PROP_TELEMETRY_RUN_ID", "run-valid:001", 1);
    memset(&ge_prop_telemetry, 0, sizeof ge_prop_telemetry);
    gePortPropAllocatorReset(600, 500);
    check_i("valid opt-in enables production accounting", ge_prop_telemetry.enabled, 1);
    check_i("valid opt-in initializes the first pool epoch", ge_prop_telemetry.pool_epoch, 1);
    check_contains("valid opt-in preserves the opaque run token",
                   ge_prop_telemetry.run_id, "run-valid:001");

    unsetenv("GETV_PROP_TELEMETRY");
    unsetenv("GETV_PROP_TELEMETRY_RUN_ID");
    fclose(capture);
    ge_prop_telemetry_output = NULL;
}

int main(void)
{
    printf("PropRecord allocator telemetry\n\n");
    test_reset_state();
    test_allocate_free_and_high_water();
    test_stage_ready_and_post_ready_counts();
    test_exhaustion_and_invariant_failures();
    test_tokens_and_disabled_behavior();
    printf("\n%s -- %d failure(s)\n", failures ? "FAILED" : "PASSED", failures);
    return failures ? 1 : 0;
}
