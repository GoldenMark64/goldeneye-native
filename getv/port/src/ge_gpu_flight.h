#ifndef GE_GPU_FLIGHT_H
#define GE_GPU_FLIGHT_H

/*
 * GETV_GPUFLIGHT=<path>
 *
 * Low-overhead, kill-safe GPU submission flight recorder for Intel hang attribution.
 *
 * Every OpenGL triangle submission is copied into a MAP_SHARED ring before glBufferData /
 * glDrawArrays. The ring records the exact CPU-side VBO hash/shape, shader/program identity,
 * bound texture identities/hashes and relevant render state plus the most recent Fast3D command
 * context. Draws whose vertex count matches GETV_GPUFLIGHT_PAYLOAD_VERTS (default: 24) also keep
 * the exact interleaved VBO bytes in a second ring. Generated GLSL is retained once per shader.
 *
 * The backing file is intentionally mmap'd MAP_SHARED rather than buffered through stdio: if the
 * Intel GPU wedges and the process is killed with SIGKILL, the kernel still owns the mapped pages
 * and the capture remains readable. This diagnostic does not add any synchronous GL API calls.
 *
 * The capture is private local diagnostic evidence. VBO payloads are derived game geometry; do
 * not publish the raw .bin file. tools/decode_gpu_flight.py emits the bounded metadata needed for
 * engineering review without dumping payload bytes.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ge_draw_diag.h"

#define GE_GPUFLIGHT_VERSION          3u
#define GE_GPUFLIGHT_DRAW_SLOTS      524288u
#define GE_GPUFLIGHT_PAYLOAD_SLOTS    16384u
#define GE_GPUFLIGHT_SHADER_SLOTS       128u
#define GE_GPUFLIGHT_PAYLOAD_MAX       4096u
#define GE_GPUFLIGHT_DEFAULT_VERTS       24u

#if defined(__GNUC__) || defined(__clang__)
#define GE_GPUFLIGHT_PACKED __attribute__((packed))
#else
#define GE_GPUFLIGHT_PACKED
#endif

struct GE_GPUFLIGHT_PACKED GeGpuFlightHeader {
    char magic[8];                 /* "GEGF0061" */
    uint32_t version;
    uint32_t header_bytes;
    uint32_t draw_bytes;
    uint32_t payload_record_bytes;
    uint32_t shader_record_bytes;
    uint32_t draw_slots;
    uint32_t payload_slots;
    uint32_t shader_slots;
    uint32_t payload_max_bytes;
    uint32_t payload_vertex_filter;
    uint64_t total_draws;
    uint64_t total_payloads;
    uint64_t total_shaders;
    char gl_vendor[96];
    char gl_renderer[128];
    char gl_version[96];
};

struct GE_GPUFLIGHT_PACKED GeGpuFlightDraw {
    uint64_t serial;               /* 1-based global submission serial */
    uint64_t monotonic_ns;
    uint64_t vbo_hash;
    uint64_t shader_id;
    uint64_t texture_hash[2];
    uint64_t height_hash[2];
    uint64_t gfx_w0;
    uint64_t gfx_w1;
    uint64_t gfx_cmd_ptr;
    uint64_t gfx_dl_stack[8];
    uint64_t gfx_dl_callsite[8];
    uint64_t gfx_source_owner;

    uint32_t frame;
    uint32_t draw_in_frame;
    uint32_t tris;
    uint32_t verts;
    uint32_t vbo_bytes;
    uint32_t stride_floats;
    uint32_t num_attribs;
    uint32_t program_id;
    uint32_t gfx_dl_stack_base_depth;
    uint32_t gfx_dl_stack_count;
    uint32_t gfx_source_kind;
    int32_t gfx_source_id;
    uint32_t gfx_source_aux;
    char gfx_source_name[32];

    int32_t texture_id[2];
    uint32_t texture_gl_id[2];
    int32_t texture_width[2];
    int32_t texture_height[2];
    int32_t texture_filter[2];
    int32_t texture_has_height[2];
    int32_t texture_wrap_s[2];
    int32_t texture_wrap_t[2];

    int32_t depth_test;
    int32_t depth_write;
    int32_t blend;
    int32_t decal;

    int32_t viewport[4];
    int32_t scissor[4];

    int32_t gfx_cmd_progress;
    int32_t gfx_dl_depth;
    uint32_t gfx_opcode;

    int32_t payload_slot;          /* -1 unless exact VBO bytes were retained */
    uint32_t payload_bytes;        /* bytes retained, <= GE_GPUFLIGHT_PAYLOAD_MAX */
    uint32_t payload_truncated;    /* original VBO did not fit payload slot */
};

struct GE_GPUFLIGHT_PACKED GeGpuFlightPayload {
    uint64_t serial;
    uint32_t bytes;
    uint32_t truncated;
    unsigned char data[GE_GPUFLIGHT_PAYLOAD_MAX];
};

struct GE_GPUFLIGHT_PACKED GeGpuFlightShader {
    uint64_t shader_id;
    uint32_t program_id;
    uint32_t vs_len;
    uint32_t fs_len;
    uint64_t vs_hash;
    uint64_t fs_hash;
    char vs[1024];
    char fs[4096];
};

struct GeGpuFlightFile {
    struct GeGpuFlightHeader header;
    struct GeGpuFlightDraw draws[GE_GPUFLIGHT_DRAW_SLOTS];
    struct GeGpuFlightPayload payloads[GE_GPUFLIGHT_PAYLOAD_SLOTS];
    struct GeGpuFlightShader shaders[GE_GPUFLIGHT_SHADER_SLOTS];
};

static uint64_t geGpuFlightHashBytes(const void *data, size_t bytes)
{
    const unsigned char *p = (const unsigned char *)data;
    uint64_t h = UINT64_C(14695981039346656037);
    size_t i;

    if (p == NULL) {
        return 0;
    }

    for (i = 0; i < bytes; i++) {
        h ^= p[i];
        h *= UINT64_C(1099511628211);
    }
    return h;
}

static uint64_t geGpuFlightNowNs(void)
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

static struct GeGpuFlightFile *ge_gpuflight_file;
static int ge_gpuflight_fd = -1;
static int ge_gpuflight_state = -1;

static uint32_t geGpuFlightPayloadVerts(void)
{
    const char *e = getenv("GETV_GPUFLIGHT_PAYLOAD_VERTS");
    char *end = NULL;
    unsigned long v;

    if (e == NULL || *e == '\0') {
        return GE_GPUFLIGHT_DEFAULT_VERTS;
    }

    v = strtoul(e, &end, 10);
    if (end == e || *end != '\0' || v > 65535UL) {
        return GE_GPUFLIGHT_DEFAULT_VERTS;
    }
    return (uint32_t)v;
}

static int geGpuFlightInit(void)
{
    const char *path;
    size_t bytes;

    if (ge_gpuflight_state >= 0) {
        return ge_gpuflight_state;
    }

    path = getenv("GETV_GPUFLIGHT");
    if (path == NULL || *path == '\0') {
        ge_gpuflight_state = 0;
        return 0;
    }

    bytes = sizeof(struct GeGpuFlightFile);
    ge_gpuflight_fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0600);
    if (ge_gpuflight_fd < 0) {
        fprintf(stderr, "[getv][gpuflight] open %s failed: %s\n", path, strerror(errno));
        ge_gpuflight_state = 0;
        return 0;
    }

    if (ftruncate(ge_gpuflight_fd, (off_t)bytes) != 0) {
        fprintf(stderr, "[getv][gpuflight] ftruncate %s failed: %s\n", path, strerror(errno));
        close(ge_gpuflight_fd);
        ge_gpuflight_fd = -1;
        ge_gpuflight_state = 0;
        return 0;
    }

    ge_gpuflight_file = (struct GeGpuFlightFile *)mmap(
        NULL, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, ge_gpuflight_fd, 0);
    if (ge_gpuflight_file == MAP_FAILED) {
        fprintf(stderr, "[getv][gpuflight] mmap %s failed: %s\n", path, strerror(errno));
        ge_gpuflight_file = NULL;
        close(ge_gpuflight_fd);
        ge_gpuflight_fd = -1;
        ge_gpuflight_state = 0;
        return 0;
    }

    memset(ge_gpuflight_file, 0, bytes);
    memcpy(ge_gpuflight_file->header.magic, "GEGF0061", 8);
    ge_gpuflight_file->header.version = GE_GPUFLIGHT_VERSION;
    ge_gpuflight_file->header.header_bytes = (uint32_t)sizeof(struct GeGpuFlightHeader);
    ge_gpuflight_file->header.draw_bytes = (uint32_t)sizeof(struct GeGpuFlightDraw);
    ge_gpuflight_file->header.payload_record_bytes = (uint32_t)sizeof(struct GeGpuFlightPayload);
    ge_gpuflight_file->header.shader_record_bytes = (uint32_t)sizeof(struct GeGpuFlightShader);
    ge_gpuflight_file->header.draw_slots = GE_GPUFLIGHT_DRAW_SLOTS;
    ge_gpuflight_file->header.payload_slots = GE_GPUFLIGHT_PAYLOAD_SLOTS;
    ge_gpuflight_file->header.shader_slots = GE_GPUFLIGHT_SHADER_SLOTS;
    ge_gpuflight_file->header.payload_max_bytes = GE_GPUFLIGHT_PAYLOAD_MAX;
    ge_gpuflight_file->header.payload_vertex_filter = geGpuFlightPayloadVerts();

    /* Header is tiny and written once; make the format/version durable before gameplay starts.
     * Draw records themselves remain ordinary MAP_SHARED stores to avoid a syscall per draw. */
    msync(ge_gpuflight_file, sizeof(struct GeGpuFlightHeader), MS_ASYNC);

    printf("[getv][gpuflight] recording every GL triangle submission to %s "
           "(draw slots=%u payload verts=%u payload slots=%u)\n",
           path, GE_GPUFLIGHT_DRAW_SLOTS,
           ge_gpuflight_file->header.payload_vertex_filter,
           GE_GPUFLIGHT_PAYLOAD_SLOTS);
    fflush(stdout);

    ge_gpuflight_state = 1;
    return 1;
}

static int geGpuFlightIsEnabled(void)
{
    return geGpuFlightInit();
}

static int geGpuFlightNeedsGlInfo(void)
{
    return geGpuFlightIsEnabled() && ge_gpuflight_file->header.gl_vendor[0] == '\0';
}

static void geGpuFlightCopyString(char *dst, size_t cap, const char *src)
{
    size_t n;
    if (dst == NULL || cap == 0 || src == NULL) return;
    n = strlen(src);
    if (n >= cap) n = cap - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static void geGpuFlightSetGlInfo(const char *vendor, const char *renderer, const char *version)
{
    if (!geGpuFlightIsEnabled()) return;
    if (ge_gpuflight_file->header.gl_vendor[0] != '\0') return;

    geGpuFlightCopyString(ge_gpuflight_file->header.gl_vendor,
                          sizeof(ge_gpuflight_file->header.gl_vendor), vendor);
    geGpuFlightCopyString(ge_gpuflight_file->header.gl_renderer,
                          sizeof(ge_gpuflight_file->header.gl_renderer), renderer);
    geGpuFlightCopyString(ge_gpuflight_file->header.gl_version,
                          sizeof(ge_gpuflight_file->header.gl_version), version);
}

static void geGpuFlightRecordShader(uint64_t shader_id, uint32_t program_id,
                                    const char *vs, size_t vs_len,
                                    const char *fs, size_t fs_len)
{
    struct GeGpuFlightShader tmp;
    struct GeGpuFlightShader *dst;
    uint64_t n;
    size_t vcopy;
    size_t fcopy;

    if (!geGpuFlightIsEnabled()) return;

    memset(&tmp, 0, sizeof(tmp));
    tmp.shader_id = shader_id;
    tmp.program_id = program_id;
    tmp.vs_len = (uint32_t)(vs_len > UINT32_MAX ? UINT32_MAX : vs_len);
    tmp.fs_len = (uint32_t)(fs_len > UINT32_MAX ? UINT32_MAX : fs_len);
    tmp.vs_hash = geGpuFlightHashBytes(vs, vs_len);
    tmp.fs_hash = geGpuFlightHashBytes(fs, fs_len);

    vcopy = vs_len;
    if (vcopy >= sizeof(tmp.vs)) vcopy = sizeof(tmp.vs) - 1;
    if (vs != NULL && vcopy != 0) memcpy(tmp.vs, vs, vcopy);

    fcopy = fs_len;
    if (fcopy >= sizeof(tmp.fs)) fcopy = sizeof(tmp.fs) - 1;
    if (fs != NULL && fcopy != 0) memcpy(tmp.fs, fs, fcopy);

    n = ge_gpuflight_file->header.total_shaders;
    dst = &ge_gpuflight_file->shaders[n % GE_GPUFLIGHT_SHADER_SLOTS];
    memcpy(dst, &tmp, sizeof(tmp));
    __sync_synchronize();
    ge_gpuflight_file->header.total_shaders = n + 1;
}

static uint64_t geGpuFlightRecordDraw(uint32_t frame, const float *buf, size_t len,
                                      size_t tris, const struct GeDrawDiagState *state)
{
    static uint32_t last_frame = UINT32_MAX;
    static uint32_t draw_in_frame;
    struct GeGpuFlightDraw tmp;
    struct GeGpuFlightDraw *dst;
    uint64_t serial;
    size_t bytes;
    uint32_t verts;

    if (!geGpuFlightIsEnabled() || buf == NULL || state == NULL) return 0;
    if (len > SIZE_MAX / sizeof(float) || tris > UINT32_MAX / 3u) return 0;

    bytes = len * sizeof(float);
    verts = (uint32_t)(tris * 3u);

    if (last_frame != frame) {
        last_frame = frame;
        draw_in_frame = 0;
    } else {
        draw_in_frame++;
    }

    serial = ge_gpuflight_file->header.total_draws + 1;

    memset(&tmp, 0, sizeof(tmp));
    tmp.serial = serial;
    tmp.monotonic_ns = geGpuFlightNowNs();
    tmp.vbo_hash = geGpuFlightHashBytes(buf, bytes);
    tmp.shader_id = state->shader_id;
    tmp.texture_hash[0] = state->texture_hash[0];
    tmp.texture_hash[1] = state->texture_hash[1];
    tmp.height_hash[0] = state->height_hash[0];
    tmp.height_hash[1] = state->height_hash[1];
    tmp.gfx_w0 = state->gfx_w0;
    tmp.gfx_w1 = state->gfx_w1;
    tmp.gfx_cmd_ptr = state->gfx_cmd_ptr;
    memcpy(tmp.gfx_dl_stack, state->gfx_dl_stack, sizeof(tmp.gfx_dl_stack));
    memcpy(tmp.gfx_dl_callsite, state->gfx_dl_callsite, sizeof(tmp.gfx_dl_callsite));
    tmp.gfx_source_owner = state->gfx_source_owner;

    tmp.frame = frame;
    tmp.draw_in_frame = draw_in_frame;
    tmp.tris = (uint32_t)tris;
    tmp.verts = verts;
    tmp.vbo_bytes = (uint32_t)(bytes > UINT32_MAX ? UINT32_MAX : bytes);
    tmp.stride_floats = state->stride_floats;
    tmp.num_attribs = state->num_attribs;
    tmp.program_id = state->program_id;
    tmp.gfx_dl_stack_base_depth = state->gfx_dl_stack_base_depth;
    tmp.gfx_dl_stack_count = state->gfx_dl_stack_count;
    tmp.gfx_source_kind = state->gfx_source_kind;
    tmp.gfx_source_id = state->gfx_source_id;
    tmp.gfx_source_aux = state->gfx_source_aux;
    memcpy(tmp.gfx_source_name, state->gfx_source_name, sizeof(tmp.gfx_source_name));

    memcpy(tmp.texture_id, state->texture_id, sizeof(tmp.texture_id));
    memcpy(tmp.texture_gl_id, state->texture_gl_id, sizeof(tmp.texture_gl_id));
    memcpy(tmp.texture_width, state->texture_width, sizeof(tmp.texture_width));
    memcpy(tmp.texture_height, state->texture_height, sizeof(tmp.texture_height));
    memcpy(tmp.texture_filter, state->texture_filter, sizeof(tmp.texture_filter));
    memcpy(tmp.texture_has_height, state->texture_has_height, sizeof(tmp.texture_has_height));
    memcpy(tmp.texture_wrap_s, state->texture_wrap_s, sizeof(tmp.texture_wrap_s));
    memcpy(tmp.texture_wrap_t, state->texture_wrap_t, sizeof(tmp.texture_wrap_t));
    memcpy(tmp.viewport, state->viewport, sizeof(tmp.viewport));
    memcpy(tmp.scissor, state->scissor, sizeof(tmp.scissor));

    tmp.depth_test = state->depth_test;
    tmp.depth_write = state->depth_write;
    tmp.blend = state->blend;
    tmp.decal = state->decal;
    tmp.gfx_cmd_progress = state->gfx_cmd_progress;
    tmp.gfx_dl_depth = state->gfx_dl_depth;
    tmp.gfx_opcode = state->gfx_opcode;
    tmp.payload_slot = -1;

    if (verts == ge_gpuflight_file->header.payload_vertex_filter) {
        uint64_t pn = ge_gpuflight_file->header.total_payloads;
        uint32_t slot = (uint32_t)(pn % GE_GPUFLIGHT_PAYLOAD_SLOTS);
        struct GeGpuFlightPayload *pdst = &ge_gpuflight_file->payloads[slot];
        size_t keep = bytes;

        if (keep > GE_GPUFLIGHT_PAYLOAD_MAX) keep = GE_GPUFLIGHT_PAYLOAD_MAX;

        pdst->serial = 0;
        pdst->bytes = (uint32_t)keep;
        pdst->truncated = bytes > GE_GPUFLIGHT_PAYLOAD_MAX ? 1u : 0u;
        if (keep != 0) memcpy(pdst->data, buf, keep);
        __sync_synchronize();
        pdst->serial = serial;

        tmp.payload_slot = (int32_t)slot;
        tmp.payload_bytes = (uint32_t)keep;
        tmp.payload_truncated = pdst->truncated;
        ge_gpuflight_file->header.total_payloads = pn + 1;
    }

    dst = &ge_gpuflight_file->draws[(serial - 1) % GE_GPUFLIGHT_DRAW_SLOTS];
    memcpy(dst, &tmp, sizeof(tmp));
    __sync_synchronize();
    ge_gpuflight_file->header.total_draws = serial;
    return serial;
}

#else

/* Windows builds retain the exact gameplay/renderer path; this Linux/macOS diagnostic simply
 * compiles away there. A Windows mapping implementation can be added if the Intel bug is ever
 * reproduced on that platform. */
static int geGpuFlightIsEnabled(void) { return 0; }
static int geGpuFlightNeedsGlInfo(void) { return 0; }
static void geGpuFlightSetGlInfo(const char *vendor, const char *renderer, const char *version)
{ (void)vendor; (void)renderer; (void)version; }
static void geGpuFlightRecordShader(uint64_t shader_id, uint32_t program_id,
                                    const char *vs, size_t vs_len,
                                    const char *fs, size_t fs_len)
{ (void)shader_id; (void)program_id; (void)vs; (void)vs_len; (void)fs; (void)fs_len; }
static uint64_t geGpuFlightRecordDraw(uint32_t frame, const float *buf, size_t len,
                                      size_t tris, const struct GeDrawDiagState *state)
{ (void)frame; (void)buf; (void)len; (void)tris; (void)state; return 0; }

#endif

#endif
