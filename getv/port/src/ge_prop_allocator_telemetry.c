#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "ge_prop_allocator_telemetry.h"

#define GE_PROP_TELEMETRY_SCHEMA "goldeneye-native.prop-allocator"
#define GE_PROP_TELEMETRY_VERSION 1u
#define GE_PROP_MAX_TRACKED_SLOTS 600u
#define GE_PROP_TOKEN_MAX         128u
#define GE_PROP_OUTPUT_MAX        2048u

typedef struct GePropAllocatorIdentity {
    const char *phase;
    const char *build_variant;
    int stage_id;
    int difficulty;
    int player_count;
    uint64_t game_tick;
    uint64_t render_frame;
} GePropAllocatorIdentity;

typedef struct GePropAllocatorTelemetryState {
    int configured;
    int enabled;
    char run_id[GE_PROP_TOKEN_MAX + 1u];
    char build_variant[GE_PROP_TOKEN_MAX + 1u];
    uint64_t sequence;
    uint64_t pool_epoch;

    unsigned int capacity;
    unsigned int current_allocated;
    unsigned int free_slots;
    unsigned int high_water_allocated;
    unsigned int minimum_free_slots;
    unsigned int stage_ready_allocated;
    unsigned int post_stage_ready_high_water_allocated;
    uint64_t allocation_calls;
    uint64_t allocation_successes;
    uint64_t allocation_failures;
    uint64_t free_calls;
    uint64_t post_stage_ready_allocation_calls;
    uint64_t post_stage_ready_allocation_successes;
    uint64_t post_stage_ready_allocation_failures;
    uint64_t post_stage_ready_free_calls;

    unsigned int onscreen_capacity;
    unsigned int onscreen_current;
    unsigned int onscreen_high_water;

    int stage_ready;
    int accounting_consistent;
    int slot_tracking_consistent;
    int onscreen_within_capacity;
    unsigned char allocated_slots[GE_PROP_MAX_TRACKED_SLOTS];
} GePropAllocatorTelemetryState;

static GePropAllocatorTelemetryState ge_prop_telemetry;
/* Tests replace this with a private temporary stream. Production always leaves it NULL. */
static FILE *ge_prop_telemetry_output;

/* Separate report streams contain only telemetry, never the surrounding runtime log. */
static FILE *ge_prop_report;
static FILE *ge_prop_summary;

static FILE *ge_prop_create_report(const char *path)
{
    int fd;
    FILE *stream;
#ifdef _WIN32
    wchar_t wide_path[4096];
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1,
                             wide_path, (int)(sizeof wide_path / sizeof wide_path[0]))) return NULL;
    fd = _wopen(wide_path, _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY, _S_IREAD | _S_IWRITE);
    if (fd < 0) return NULL;
    stream = _fdopen(fd, "wb");
    if (!stream) _close(fd);
#else
    fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0) return NULL;
    stream = fdopen(fd, "w");
    if (!stream) close(fd);
#endif
    return stream;
}

static void ge_prop_open_reports(void)
{
    const char *path = getenv("GETV_PROP_TELEMETRY_FILE");
    char summary[4096];
    int n;
    if (!path || !*path) return;
    ge_prop_report = ge_prop_create_report(path);
    if (!ge_prop_report) {
        fputs("[getv][prop-allocator] cannot create report; stdout telemetry remains active\n", stderr);
        return;
    }
    n = snprintf(summary, sizeof summary, "%s.txt", path);
    if (n < 0 || (size_t)n >= sizeof summary ||
        !(ge_prop_summary = ge_prop_create_report(summary))) {
        fputs("[getv][prop-allocator] cannot create summary; JSONL report remains active\n", stderr);
        return;
    }
    fputs("Prop allocator recording started.\n"
          "Incomplete until a timed run-final record is written.\n", ge_prop_summary);
    fflush(ge_prop_summary);
}

static int ge_prop_accounting_valid(void)
{
    return
        ge_prop_telemetry.accounting_consistent &&
        ge_prop_telemetry.current_allocated <= ge_prop_telemetry.capacity &&
        ge_prop_telemetry.free_slots ==
            ge_prop_telemetry.capacity - ge_prop_telemetry.current_allocated &&
        ge_prop_telemetry.allocation_successes >= ge_prop_telemetry.free_calls &&
        ge_prop_telemetry.allocation_successes - ge_prop_telemetry.free_calls ==
            ge_prop_telemetry.current_allocated;
}

static void ge_prop_write_summary(const GePropAllocatorIdentity *identity)
{
    if (!ge_prop_summary) return;
    fprintf(ge_prop_summary,
        "\n%s | run %s | pool %llu | stage %d | frame %llu\n"
        "Slots occupied: %u / %u; free: %u; peak occupied: %u\n"
        "Allocations since stage ready: %llu; frees: %llu; allocation failures: %llu\n"
        "On-screen count: %u; peak: %u; array capacity: %u\n"
        "Accounting: %s; slot tracking: %s; on-screen bounds: %s\n",
        identity->phase, ge_prop_telemetry.run_id,
        (unsigned long long)ge_prop_telemetry.pool_epoch, identity->stage_id,
        (unsigned long long)identity->render_frame,
        ge_prop_telemetry.current_allocated, ge_prop_telemetry.capacity,
        ge_prop_telemetry.free_slots, ge_prop_telemetry.high_water_allocated,
        (unsigned long long)ge_prop_telemetry.post_stage_ready_allocation_successes,
        (unsigned long long)ge_prop_telemetry.post_stage_ready_free_calls,
        (unsigned long long)ge_prop_telemetry.post_stage_ready_allocation_failures,
        ge_prop_telemetry.onscreen_current, ge_prop_telemetry.onscreen_high_water,
        ge_prop_telemetry.onscreen_capacity,
        ge_prop_accounting_valid() ? "consistent" : "FAILED",
        ge_prop_telemetry.slot_tracking_consistent ? "consistent" : "FAILED",
        ge_prop_telemetry.onscreen_within_capacity ? "within capacity" : "FAILED");
    if (strcmp(identity->phase, "run-final") == 0)
        fputs("\nTimed recording complete. These observations describe this run only;\n"
              "they do not establish a safe mission size or test other resource pools.\n",
              ge_prop_summary);
    if (fflush(ge_prop_summary) != 0)
        fputs("[getv][prop-allocator] summary write failed\n", stderr);
}

static int ge_prop_token_is_valid(const char *text)
{
    size_t i;

    if (text == NULL || text[0] == '\0') { return 0; }
    for (i = 0; text[i] != '\0'; i++) {
        const unsigned char c = (unsigned char)text[i];
        if (i >= GE_PROP_TOKEN_MAX) { return 0; }
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || (i > 0 && (c == '.' || c == '_' || c == ':' || c == '-')))) {
            return 0;
        }
    }
    return 1;
}

static void ge_prop_copy_token(char *destination, const char *source, const char *fallback)
{
    const char *value = ge_prop_token_is_valid(source) ? source : fallback;
    snprintf(destination, GE_PROP_TOKEN_MAX + 1u, "%s", value);
}

static void ge_prop_configure(void)
{
    const char *enabled;
    const char *run_id;

    if (ge_prop_telemetry.configured) { return; }
    ge_prop_telemetry.configured = 1;
    enabled = getenv("GETV_PROP_TELEMETRY");
    if (enabled == NULL || strcmp(enabled, "1") != 0) { return; }

    run_id = getenv("GETV_PROP_TELEMETRY_RUN_ID");
    if (!ge_prop_token_is_valid(run_id)) {
        fputs("[getv][prop-allocator] disabled: GETV_PROP_TELEMETRY_RUN_ID must be a "
              "1..128 character path-free token\n", stderr);
        return;
    }

    ge_prop_copy_token(ge_prop_telemetry.run_id, run_id, "invalid");
    ge_prop_copy_token(ge_prop_telemetry.build_variant, "unknown", "unknown");
    ge_prop_telemetry.enabled = 1;
    ge_prop_open_reports();
}

static const char *ge_prop_platform(void)
{
#if defined(__APPLE__) && defined(GE_PLATFORM_MAC)
    return "macos";
#elif defined(__APPLE__)
    return "apple-mobile";
#elif defined(_WIN32)
    return "windows";
#elif defined(__linux__)
    return "linux";
#else
    return "unknown";
#endif
}

static const char *ge_prop_architecture(void)
{
#if defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64)
    return "arm64";
#elif defined(__x86_64__) || defined(_M_X64)
    return "x86_64";
#elif defined(__i386__) || defined(_M_IX86)
    return "x86";
#else
    return "unknown";
#endif
}

static const char *ge_prop_renderer(void)
{
#if defined(RAPI_METAL)
    return "metal";
#elif defined(RAPI_GL)
    return "opengl";
#else
    return "unknown";
#endif
}

static const char *ge_prop_json_bool(int value)
{
    return value ? "true" : "false";
}

static int ge_prop_format(char *buffer, size_t size, uint64_t sequence,
                          const GePropAllocatorIdentity *identity)
{
    const int accounting = ge_prop_accounting_valid();
    int written;

    written = snprintf(
        buffer, size,
        "{\"schema\":\"%s\",\"version\":%u,\"phase\":\"%s\","
        "\"runId\":\"%s\",\"sequence\":%llu,\"poolEpoch\":%llu,"
        "\"identity\":{\"platform\":\"%s\",\"architecture\":\"%s\","
        "\"renderer\":\"%s\",\"buildVariant\":\"%s\",\"stageId\":%d,"
        "\"difficulty\":%d,\"playerCount\":%d,\"gameTick\":%llu,"
        "\"renderFrame\":%llu},"
        "\"allocator\":{\"capacity\":%u,\"currentAllocated\":%u,\"freeSlots\":%u,"
        "\"highWaterAllocated\":%u,\"minimumFreeSlots\":%u,"
        "\"stageReadyAllocated\":%u,\"postStageReadyHighWaterAllocated\":%u,"
        "\"allocationCalls\":%llu,\"allocationSuccesses\":%llu,"
        "\"allocationFailures\":%llu,\"freeCalls\":%llu,"
        "\"postStageReadyAllocationCalls\":%llu,"
        "\"postStageReadyAllocationSuccesses\":%llu,"
        "\"postStageReadyAllocationFailures\":%llu,"
        "\"postStageReadyFreeCalls\":%llu,\"observedExhaustion\":%s},"
        "\"onscreen\":{\"arrayCapacity\":%u,\"currentCount\":%u,"
        "\"highWaterCount\":%u},"
        "\"invariants\":{\"accountingConsistent\":%s,"
        "\"slotTrackingConsistent\":%s,\"onscreenWithinCapacity\":%s}}\n",
        GE_PROP_TELEMETRY_SCHEMA, GE_PROP_TELEMETRY_VERSION, identity->phase,
        ge_prop_telemetry.run_id, (unsigned long long)sequence,
        (unsigned long long)ge_prop_telemetry.pool_epoch,
        ge_prop_platform(), ge_prop_architecture(), ge_prop_renderer(), identity->build_variant,
        identity->stage_id, identity->difficulty, identity->player_count,
        (unsigned long long)identity->game_tick, (unsigned long long)identity->render_frame,
        ge_prop_telemetry.capacity, ge_prop_telemetry.current_allocated,
        ge_prop_telemetry.free_slots, ge_prop_telemetry.high_water_allocated,
        ge_prop_telemetry.minimum_free_slots, ge_prop_telemetry.stage_ready_allocated,
        ge_prop_telemetry.post_stage_ready_high_water_allocated,
        (unsigned long long)ge_prop_telemetry.allocation_calls,
        (unsigned long long)ge_prop_telemetry.allocation_successes,
        (unsigned long long)ge_prop_telemetry.allocation_failures,
        (unsigned long long)ge_prop_telemetry.free_calls,
        (unsigned long long)ge_prop_telemetry.post_stage_ready_allocation_calls,
        (unsigned long long)ge_prop_telemetry.post_stage_ready_allocation_successes,
        (unsigned long long)ge_prop_telemetry.post_stage_ready_allocation_failures,
        (unsigned long long)ge_prop_telemetry.post_stage_ready_free_calls,
        ge_prop_json_bool(ge_prop_telemetry.allocation_failures > 0),
        ge_prop_telemetry.onscreen_capacity, ge_prop_telemetry.onscreen_current,
        ge_prop_telemetry.onscreen_high_water, ge_prop_json_bool(accounting),
        ge_prop_json_bool(ge_prop_telemetry.slot_tracking_consistent),
        ge_prop_json_bool(ge_prop_telemetry.onscreen_within_capacity));
    return written >= 0 && (size_t)written < size ? written : -1;
}

static void ge_prop_emit(const GePropAllocatorIdentity *identity)
{
    char output[GE_PROP_OUTPUT_MAX];
    const uint64_t sequence = ge_prop_telemetry.sequence + 1u;
    FILE *stream = ge_prop_telemetry_output != NULL ? ge_prop_telemetry_output : stdout;

    if (!ge_prop_telemetry.enabled) { return; }
    if (ge_prop_format(output, sizeof output, sequence, identity) < 0) {
        fputs("[getv][prop-allocator] fixed record exceeded its internal bound\n", stderr);
        ge_prop_telemetry.enabled = 0;
        return;
    }
    fputs(output, stream);
    fflush(stream);
    ge_prop_telemetry.sequence = sequence;
    if (ge_prop_report) {
        if (fputs(output, ge_prop_report) == EOF || fflush(ge_prop_report) != 0)
            fputs("[getv][prop-allocator] JSONL report write failed\n", stderr);
    }
    ge_prop_write_summary(identity);
}

void gePortPropAllocatorReset(unsigned int capacity, unsigned int onscreen_capacity)
{
    const int configured = ge_prop_telemetry.configured;
    const int enabled = ge_prop_telemetry.enabled;
    const uint64_t sequence = ge_prop_telemetry.sequence;
    const uint64_t pool_epoch = ge_prop_telemetry.pool_epoch + 1u;
    char run_id[GE_PROP_TOKEN_MAX + 1u];

    ge_prop_configure();
    if (!ge_prop_telemetry.enabled) { return; }

    memcpy(run_id, ge_prop_telemetry.run_id, sizeof run_id);
    memset(&ge_prop_telemetry, 0, sizeof ge_prop_telemetry);
    ge_prop_telemetry.configured = configured ? configured : 1;
    ge_prop_telemetry.enabled = enabled ? enabled : 1;
    memcpy(ge_prop_telemetry.run_id, run_id, sizeof ge_prop_telemetry.run_id);
    ge_prop_copy_token(ge_prop_telemetry.build_variant, "unknown", "unknown");
    ge_prop_telemetry.sequence = sequence;
    ge_prop_telemetry.pool_epoch = pool_epoch;
    ge_prop_telemetry.capacity = capacity;
    ge_prop_telemetry.free_slots = capacity;
    ge_prop_telemetry.minimum_free_slots = capacity;
    ge_prop_telemetry.onscreen_capacity = onscreen_capacity;
    ge_prop_telemetry.accounting_consistent = capacity > 0;
    ge_prop_telemetry.slot_tracking_consistent =
        capacity > 0 && capacity <= GE_PROP_MAX_TRACKED_SLOTS;
    ge_prop_telemetry.onscreen_within_capacity = onscreen_capacity > 0;
}

void gePortPropAllocatorAllocated(unsigned int slot_index)
{
    if (!ge_prop_telemetry.enabled) { return; }
    ge_prop_telemetry.allocation_calls++;
    ge_prop_telemetry.allocation_successes++;
    if (ge_prop_telemetry.stage_ready) {
        ge_prop_telemetry.post_stage_ready_allocation_calls++;
        ge_prop_telemetry.post_stage_ready_allocation_successes++;
    }
    if (slot_index >= ge_prop_telemetry.capacity ||
        slot_index >= GE_PROP_MAX_TRACKED_SLOTS ||
        ge_prop_telemetry.allocated_slots[slot_index]) {
        ge_prop_telemetry.slot_tracking_consistent = 0;
        ge_prop_telemetry.accounting_consistent = 0;
        return;
    }
    ge_prop_telemetry.allocated_slots[slot_index] = 1;
    if (ge_prop_telemetry.current_allocated >= ge_prop_telemetry.capacity) {
        ge_prop_telemetry.accounting_consistent = 0;
        return;
    }
    ge_prop_telemetry.current_allocated++;
    ge_prop_telemetry.free_slots =
        ge_prop_telemetry.capacity - ge_prop_telemetry.current_allocated;
    if (ge_prop_telemetry.current_allocated > ge_prop_telemetry.high_water_allocated) {
        ge_prop_telemetry.high_water_allocated = ge_prop_telemetry.current_allocated;
    }
    if (ge_prop_telemetry.free_slots < ge_prop_telemetry.minimum_free_slots) {
        ge_prop_telemetry.minimum_free_slots = ge_prop_telemetry.free_slots;
    }
    if (ge_prop_telemetry.stage_ready &&
        ge_prop_telemetry.current_allocated >
            ge_prop_telemetry.post_stage_ready_high_water_allocated) {
        ge_prop_telemetry.post_stage_ready_high_water_allocated =
            ge_prop_telemetry.current_allocated;
    }
}

void gePortPropAllocatorAllocationFailed(void)
{
    if (!ge_prop_telemetry.enabled) { return; }
    ge_prop_telemetry.allocation_calls++;
    ge_prop_telemetry.allocation_failures++;
    if (ge_prop_telemetry.stage_ready) {
        ge_prop_telemetry.post_stage_ready_allocation_calls++;
        ge_prop_telemetry.post_stage_ready_allocation_failures++;
    }
    if (ge_prop_telemetry.free_slots != 0) {
        ge_prop_telemetry.accounting_consistent = 0;
    }
}

void gePortPropAllocatorFreed(unsigned int slot_index)
{
    if (!ge_prop_telemetry.enabled) { return; }
    ge_prop_telemetry.free_calls++;
    if (ge_prop_telemetry.stage_ready) { ge_prop_telemetry.post_stage_ready_free_calls++; }
    if (slot_index >= ge_prop_telemetry.capacity ||
        slot_index >= GE_PROP_MAX_TRACKED_SLOTS ||
        !ge_prop_telemetry.allocated_slots[slot_index]) {
        ge_prop_telemetry.slot_tracking_consistent = 0;
        ge_prop_telemetry.accounting_consistent = 0;
        return;
    }
    ge_prop_telemetry.allocated_slots[slot_index] = 0;
    if (ge_prop_telemetry.current_allocated == 0) {
        ge_prop_telemetry.accounting_consistent = 0;
        return;
    }
    ge_prop_telemetry.current_allocated--;
    ge_prop_telemetry.free_slots =
        ge_prop_telemetry.capacity - ge_prop_telemetry.current_allocated;
}

void gePortPropAllocatorStageReady(const char *build_variant, int stage_id, int difficulty,
                                   int player_count, unsigned long long game_tick,
                                   unsigned long long render_frame)
{
    GePropAllocatorIdentity identity;

    if (!ge_prop_telemetry.enabled) { return; }
    ge_prop_copy_token(ge_prop_telemetry.build_variant, build_variant, "unknown");
    ge_prop_telemetry.stage_ready = 1;
    ge_prop_telemetry.stage_ready_allocated = ge_prop_telemetry.current_allocated;
    ge_prop_telemetry.post_stage_ready_high_water_allocated =
        ge_prop_telemetry.current_allocated;
    identity.phase = "stage-ready";
    identity.build_variant = ge_prop_telemetry.build_variant;
    identity.stage_id = stage_id;
    identity.difficulty = difficulty;
    identity.player_count = player_count;
    identity.game_tick = (uint64_t)game_tick;
    identity.render_frame = (uint64_t)render_frame;
    ge_prop_emit(&identity);
}

void gePortPropAllocatorFrame(int onscreen_count)
{
    if (!ge_prop_telemetry.enabled) { return; }
    if (onscreen_count < 0) {
        ge_prop_telemetry.onscreen_current = 0;
        ge_prop_telemetry.onscreen_within_capacity = 0;
        return;
    }
    ge_prop_telemetry.onscreen_current = (unsigned int)onscreen_count;
    if ((unsigned int)onscreen_count > ge_prop_telemetry.onscreen_high_water) {
        ge_prop_telemetry.onscreen_high_water = (unsigned int)onscreen_count;
    }
    if (ge_prop_telemetry.onscreen_capacity == 0 ||
        (unsigned int)onscreen_count >= ge_prop_telemetry.onscreen_capacity) {
        ge_prop_telemetry.onscreen_within_capacity = 0;
    }
}

void gePortPropAllocatorRunFinal(int stage_id, int difficulty, int player_count,
                                 unsigned long long game_tick,
                                 unsigned long long render_frame)
{
    GePropAllocatorIdentity identity;

    if (!ge_prop_telemetry.enabled) { return; }
    identity.phase = "run-final";
    identity.build_variant = ge_prop_telemetry.build_variant;
    identity.stage_id = stage_id;
    identity.difficulty = difficulty;
    identity.player_count = player_count;
    identity.game_tick = (uint64_t)game_tick;
    identity.render_frame = (uint64_t)render_frame;
    ge_prop_emit(&identity);
}
