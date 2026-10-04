#include "ge_function_flight.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define GE_FUNFLIGHT_VERSION 1u
#define GE_FUNFLIGHT_SLOTS 262144u

#if defined(__GNUC__) || defined(__clang__)
#define GE_FUNFLIGHT_PACKED __attribute__((packed))
#else
#define GE_FUNFLIGHT_PACKED
#endif

struct GE_FUNFLIGHT_PACKED GeFunctionFlightHeader {
    char magic[8];                 /* "GEFF0001" */
    uint32_t version;
    uint32_t header_bytes;
    uint32_t event_bytes;
    uint32_t event_slots;
    uint64_t total_events;
    uint64_t total_batches;
    uint64_t start_monotonic_ns;
};

struct GE_FUNFLIGHT_PACKED GeFunctionFlightEvent {
    uint64_t serial;
    uint64_t monotonic_ns;
    uint64_t batch_id;
    uint64_t gpu_serial;
    uint64_t ptr0;
    uint64_t arg0;
    uint64_t arg1;
    uint64_t arg2;
    uint32_t function_id;
    uint32_t phase;
    uint32_t frame;
    int32_t cmd_progress;
    int32_t dl_depth;
    uint32_t opcode;
    char label[24];
};

struct GeFunctionFlightFile {
    struct GeFunctionFlightHeader header;
    struct GeFunctionFlightEvent events[GE_FUNFLIGHT_SLOTS];
};

static uint64_t ge_funflight_now_ns(void)
{
#if defined(CLOCK_MONOTONIC)
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return (uint64_t)ts.tv_sec * UINT64_C(1000000000) + (uint64_t)ts.tv_nsec;
    }
#endif
    return 0;
}

#if !defined(_WIN32)

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

static struct GeFunctionFlightFile *ge_funflight_file;
static int ge_funflight_fd = -1;
static int ge_funflight_state = -1;
static volatile int ge_funflight_frozen;
static uint64_t ge_funflight_batch;
static uint64_t ge_funflight_gpu_serial;
static uint32_t ge_funflight_frame;
static int32_t ge_funflight_cmd_progress;
static int32_t ge_funflight_dl_depth;
static uint32_t ge_funflight_opcode;
static uintptr_t ge_funflight_cmd_ptr;

static int ge_funflight_init(void)
{
    const char *path;
    size_t bytes;

    if (ge_funflight_state >= 0) {
        return ge_funflight_state;
    }

    path = getenv("GETV_FUNFLIGHT");
    if (path == NULL || *path == '\0') {
        ge_funflight_state = 0;
        return 0;
    }

    bytes = sizeof(struct GeFunctionFlightFile);
    ge_funflight_fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0600);
    if (ge_funflight_fd < 0) {
        fprintf(stderr, "[getv][funflight] open %s failed: %s\n", path, strerror(errno));
        ge_funflight_state = 0;
        return 0;
    }

    if (ftruncate(ge_funflight_fd, (off_t)bytes) != 0) {
        fprintf(stderr, "[getv][funflight] ftruncate %s failed: %s\n", path, strerror(errno));
        close(ge_funflight_fd);
        ge_funflight_fd = -1;
        ge_funflight_state = 0;
        return 0;
    }

    ge_funflight_file = (struct GeFunctionFlightFile *)mmap(
        NULL, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, ge_funflight_fd, 0);
    if (ge_funflight_file == MAP_FAILED) {
        fprintf(stderr, "[getv][funflight] mmap %s failed: %s\n", path, strerror(errno));
        ge_funflight_file = NULL;
        close(ge_funflight_fd);
        ge_funflight_fd = -1;
        ge_funflight_state = 0;
        return 0;
    }

    memset(ge_funflight_file, 0, bytes);
    memcpy(ge_funflight_file->header.magic, "GEFF0001", 8);
    ge_funflight_file->header.version = GE_FUNFLIGHT_VERSION;
    ge_funflight_file->header.header_bytes =
        (uint32_t)sizeof(struct GeFunctionFlightHeader);
    ge_funflight_file->header.event_bytes =
        (uint32_t)sizeof(struct GeFunctionFlightEvent);
    ge_funflight_file->header.event_slots = GE_FUNFLIGHT_SLOTS;
    ge_funflight_file->header.start_monotonic_ns = ge_funflight_now_ns();
    msync(ge_funflight_file, sizeof(struct GeFunctionFlightHeader), MS_ASYNC);

    printf("[getv][funflight] recording renderer function/submission events to %s "
           "(slots=%u)\n", path, GE_FUNFLIGHT_SLOTS);
    fflush(stdout);

    ge_funflight_state = 1;
    return 1;
}

int geFunctionFlightIsEnabled(void)
{
    return ge_funflight_init() && !ge_funflight_frozen;
}

void geFunctionFlightFreeze(void)
{
    size_t bytes;

    /* Do not initialise the recorder from the watchdog thread.  If the main
     * thread never enabled GETV_FUNFLIGHT there is nothing to preserve. */
    if (ge_funflight_state != 1 || ge_funflight_file == NULL) return;

    /* One-way latch.  The watchdog can report the same stall once a second;
     * only the first qualifying report is allowed to freeze the ring. */
    if (!__sync_bool_compare_and_swap(&ge_funflight_frozen, 0, 1)) return;

    __sync_synchronize();
    bytes = sizeof(struct GeFunctionFlightFile);
    msync(ge_funflight_file, bytes, MS_ASYNC);
    fprintf(stderr,
            "[getv][funflight] FROZEN on detected stall: events=%llu batches=%llu\n",
            (unsigned long long)ge_funflight_file->header.total_events,
            (unsigned long long)ge_funflight_file->header.total_batches);
    fflush(stderr);
}

static void ge_funflight_copy_label(char dst[24], const char *src)
{
    size_t n;
    if (src == NULL) return;
    n = strlen(src);
    if (n >= 24) n = 23;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static void ge_funflight_write(uint32_t function_id, uint32_t phase,
                               uintptr_t ptr0, uint64_t arg0,
                               uint64_t arg1, uint64_t arg2,
                               const char *label)
{
    struct GeFunctionFlightEvent tmp;
    struct GeFunctionFlightEvent *dst;
    uint64_t serial;

    if (!geFunctionFlightIsEnabled()) return;

    serial = ge_funflight_file->header.total_events + 1;
    memset(&tmp, 0, sizeof(tmp));
    tmp.serial = serial;
    tmp.monotonic_ns = ge_funflight_now_ns();
    tmp.batch_id = ge_funflight_batch;
    tmp.gpu_serial = ge_funflight_gpu_serial;
    tmp.ptr0 = (uint64_t)ptr0;
    tmp.arg0 = arg0;
    tmp.arg1 = arg1;
    tmp.arg2 = arg2;
    tmp.function_id = function_id;
    tmp.phase = phase;
    tmp.frame = ge_funflight_frame;
    tmp.cmd_progress = ge_funflight_cmd_progress;
    tmp.dl_depth = ge_funflight_dl_depth;
    tmp.opcode = ge_funflight_opcode;
    ge_funflight_copy_label(tmp.label, label);

    dst = &ge_funflight_file->events[(serial - 1) % GE_FUNFLIGHT_SLOTS];
    memcpy(dst, &tmp, sizeof(tmp));
    __sync_synchronize();
    ge_funflight_file->header.total_events = serial;
}

void geFunctionFlightSetFrame(uint32_t frame)
{
    if (!geFunctionFlightIsEnabled()) return;
    ge_funflight_frame = frame;
}

void geFunctionFlightSetCommandContext(int cmd_progress, int dl_depth,
                                       uint32_t opcode, uintptr_t cmd_ptr)
{
    if (!geFunctionFlightIsEnabled()) return;
    ge_funflight_cmd_progress = cmd_progress;
    ge_funflight_dl_depth = dl_depth;
    ge_funflight_opcode = opcode;
    ge_funflight_cmd_ptr = cmd_ptr;
}

void geFunctionFlightEvent(uint32_t function_id, uint32_t phase,
                           uintptr_t ptr0, uint64_t arg0,
                           uint64_t arg1, uint64_t arg2)
{
    ge_funflight_write(function_id, phase, ptr0, arg0, arg1, arg2, NULL);
}

uint64_t geFunctionFlightBeginBatch(const char *reason, size_t floats, size_t tris)
{
    if (!geFunctionFlightIsEnabled()) return 0;

    ge_funflight_batch = ++ge_funflight_file->header.total_batches;
    ge_funflight_gpu_serial = 0;
    ge_funflight_write(GE_FUNFLIGHT_FUNC_NONE, GE_FUNFLIGHT_PHASE_BATCH,
                       ge_funflight_cmd_ptr, (uint64_t)floats, (uint64_t)tris, 0,
                       reason);
    return ge_funflight_batch;
}

uint64_t geFunctionFlightCurrentBatch(void)
{
    if (!geFunctionFlightIsEnabled()) return 0;
    return ge_funflight_batch;
}

void geFunctionFlightLinkGpu(uint64_t gpu_serial, uint32_t frame,
                             uint64_t shader_id, uint32_t program_id,
                             size_t floats, size_t tris)
{
    if (!geFunctionFlightIsEnabled()) return;
    ge_funflight_gpu_serial = gpu_serial;
    ge_funflight_frame = frame;
    ge_funflight_write(GE_FUNFLIGHT_FUNC_GFX_OPENGL_DRAW_TRIANGLES,
                       GE_FUNFLIGHT_PHASE_GPU_LINK, ge_funflight_cmd_ptr,
                       shader_id, (uint64_t)program_id,
                       ((uint64_t)(uint32_t)tris << 32) | (uint32_t)floats,
                       NULL);
}

void geFunctionFlightEndBatch(void)
{
    if (!geFunctionFlightIsEnabled()) return;
    ge_funflight_batch = 0;
    ge_funflight_gpu_serial = 0;
}

#else

int geFunctionFlightIsEnabled(void) { return 0; }
void geFunctionFlightFreeze(void) {}
void geFunctionFlightSetFrame(uint32_t frame) { (void)frame; }
void geFunctionFlightSetCommandContext(int cmd_progress, int dl_depth,
                                       uint32_t opcode, uintptr_t cmd_ptr)
{
    (void)cmd_progress; (void)dl_depth; (void)opcode; (void)cmd_ptr;
}
void geFunctionFlightEvent(uint32_t function_id, uint32_t phase,
                           uintptr_t ptr0, uint64_t arg0,
                           uint64_t arg1, uint64_t arg2)
{
    (void)function_id; (void)phase; (void)ptr0;
    (void)arg0; (void)arg1; (void)arg2;
}
uint64_t geFunctionFlightBeginBatch(const char *reason, size_t floats, size_t tris)
{
    (void)reason; (void)floats; (void)tris; return 0;
}
uint64_t geFunctionFlightCurrentBatch(void) { return 0; }
void geFunctionFlightLinkGpu(uint64_t gpu_serial, uint32_t frame,
                             uint64_t shader_id, uint32_t program_id,
                             size_t floats, size_t tris)
{
    (void)gpu_serial; (void)frame; (void)shader_id; (void)program_id;
    (void)floats; (void)tris;
}
void geFunctionFlightEndBatch(void) {}

#endif
