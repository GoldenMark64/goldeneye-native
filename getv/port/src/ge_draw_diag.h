#ifndef GE_DRAW_DIAG_H
#define GE_DRAW_DIAG_H

/* Opt-in pre-submit draw diagnostics for renderer hang investigation.
 *
 * This header is deliberately self-contained and header-only so the Fast3D OpenGL file can use
 * it without adding another object to every platform build. With GETV_DRAWDIAG unset the hot path
 * is one cached branch. When enabled, each selected draw is fingerprinted before glBufferData /
 * glDrawArrays so a later GPU hang can be correlated with the exact CPU-side payload that was
 * about to be submitted.
 */

#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define GE_DRAWDIAG_DEFAULT_LIMIT 50000UL
#define GE_DRAWDIAG_HARD_LIMIT   250000UL
#define GE_DRAWDIAG_HUGE_FLOAT   1000000.0f

struct GeDrawDiagState {
    uint64_t shader_id;
    uint32_t program_id;
    uint32_t stride_floats;
    uint32_t num_attribs;
    int used_texture[2];
    int texture_id[2];
    uint32_t texture_gl_id[2];
    int texture_width[2];
    int texture_height[2];
    int texture_filter[2];
    int texture_has_height[2];
    uint64_t texture_hash[2];
    uint64_t height_hash[2];
    int texture_wrap_s[2];
    int texture_wrap_t[2];
    int depth_test;
    int depth_write;
    int blend;
    int decal;
    int viewport[4];
    int scissor[4];
    int gfx_cmd_progress;
    int gfx_dl_depth;
    uint32_t gfx_opcode;
    uint64_t gfx_w0;
    uint64_t gfx_w1;
    uint64_t gfx_cmd_ptr;
    uint32_t gfx_dl_stack_base_depth;
    uint32_t gfx_dl_stack_count;
    uint64_t gfx_dl_stack[8];
    uint64_t gfx_dl_callsite[8];
    uint32_t gfx_source_kind;
    int32_t gfx_source_id;
    uint32_t gfx_source_aux;
    uint64_t gfx_source_owner;
    char gfx_source_name[32];
};

struct GeDrawDiagStats {
    uint64_t hash;
    size_t finite_count;
    size_t nan_count;
    size_t inf_count;
    size_t huge_count;
    float min_value;
    float max_value;
    float max_abs;
    float pos_max_abs;
    int have_finite;
};

static int ge_drawdiag_last_selected;

static int geDrawDiagIsEnabled(void)
{
    static int enabled = -1;

    if (enabled < 0) {
        const char *e = getenv("GETV_DRAWDIAG");
        enabled = (e != NULL && *e != '\0' && *e != '0') ? 1 : 0;
    }

    return enabled;
}

static unsigned long geDrawDiagParsePositive(const char *name, unsigned long fallback,
                                             unsigned long maximum)
{
    const char *e = getenv(name);
    char *end = NULL;
    unsigned long value;

    if (e == NULL || *e == '\0') {
        return fallback;
    }

    value = strtoul(e, &end, 10);

    if (end == e || *end != '\0' || value == 0) {
        return fallback;
    }

    return value > maximum ? maximum : value;
}

static unsigned long geDrawDiagFrameFilter(void)
{
    static unsigned long frame = ULONG_MAX;

    if (frame == ULONG_MAX) {
        const char *e = getenv("GETV_DRAWDIAG_FRAME");
        char *end = NULL;
        unsigned long value = 0;

        if (e != NULL && *e != '\0') {
            value = strtoul(e, &end, 10);
            if (end == e || *end != '\0') {
                value = 0;
            }
        }

        frame = value;
    }

    return frame;
}

static unsigned long geDrawDiagOptionalFilter(const char *name)
{
    const char *e = getenv(name);
    char *end = NULL;
    unsigned long value = 0;

    if (e == NULL || *e == '\0') {
        return 0;
    }

    value = strtoul(e, &end, 10);
    return (end != e && *end == '\0') ? value : 0;
}

static int geDrawDiagFlushEach(void)
{
    static int flush_each = -1;

    if (flush_each < 0) {
        const char *draw = getenv("GETV_DRAWDIAG_FLUSH");
        const char *global = getenv("GETV_LOGFLUSH");
        flush_each = ((draw != NULL && *draw == '1') ||
                      (global != NULL && *global == '1')) ? 1 : 0;
    }

    return flush_each;
}

static uint64_t geDrawDiagHashBytes(const void *data, size_t bytes)
{
    const unsigned char *p = (const unsigned char *)data;
    uint64_t hash = UINT64_C(14695981039346656037);
    size_t i;

    for (i = 0; i < bytes; i++) {
        hash ^= p[i];
        hash *= UINT64_C(1099511628211);
    }

    return hash;
}

static struct GeDrawDiagStats geDrawDiagScan(const float *buf, size_t len,
                                             size_t vertex_count, size_t stride_floats)
{
    struct GeDrawDiagStats stats;
    size_t i;

    stats.hash = geDrawDiagHashBytes(buf, len * sizeof(float));
    stats.finite_count = 0;
    stats.nan_count = 0;
    stats.inf_count = 0;
    stats.huge_count = 0;
    stats.min_value = 0.0f;
    stats.max_value = 0.0f;
    stats.max_abs = 0.0f;
    stats.pos_max_abs = 0.0f;
    stats.have_finite = 0;

    for (i = 0; i < len; i++) {
        const float value = buf[i];

        if (isnan(value)) {
            stats.nan_count++;
            continue;
        }

        if (isinf(value)) {
            stats.inf_count++;
            continue;
        }

        {
            const float abs_value = fabsf(value);
            stats.finite_count++;

            if (!stats.have_finite) {
                stats.min_value = value;
                stats.max_value = value;
                stats.have_finite = 1;
            } else {
                if (value < stats.min_value) stats.min_value = value;
                if (value > stats.max_value) stats.max_value = value;
            }

            if (abs_value > stats.max_abs) stats.max_abs = abs_value;
            if (abs_value > GE_DRAWDIAG_HUGE_FLOAT) stats.huge_count++;
        }
    }

    /* aVtxPos is always the first four floats of each interleaved vertex in gfx_opengl.c. */
    if (stride_floats >= 4) {
        for (i = 0; i < vertex_count; i++) {
            size_t base;
            size_t j;

            if (i > (SIZE_MAX / stride_floats)) {
                break;
            }

            base = i * stride_floats;
            if (base > len || len - base < 4) {
                break;
            }

            for (j = 0; j < 4; j++) {
                const float value = buf[base + j];
                if (!isnan(value) && !isinf(value)) {
                    const float abs_value = fabsf(value);
                    if (abs_value > stats.pos_max_abs) stats.pos_max_abs = abs_value;
                }
            }
        }
    }

    return stats;
}

static void geDrawDiagBeforeSubmit(unsigned long frame, const float *buf, size_t len,
                                   size_t tris, const struct GeDrawDiagState *state)
{
    static unsigned long last_frame = ULONG_MAX;
    static unsigned long draw_in_frame = 0;
    static unsigned long emitted = 0;
    static unsigned long long serial = 0;
    static int announced = 0;
    static int truncated = 0;
    const unsigned long limit = geDrawDiagParsePositive("GETV_DRAWDIAG_LIMIT",
                                                         GE_DRAWDIAG_DEFAULT_LIMIT,
                                                         GE_DRAWDIAG_HARD_LIMIT);
    const unsigned long frame_filter = geDrawDiagFrameFilter();
    const unsigned long verts_filter = geDrawDiagOptionalFilter("GETV_DRAWDIAG_VERTS");
    const unsigned long bytes_filter = geDrawDiagOptionalFilter("GETV_DRAWDIAG_BYTES");
    const unsigned long stride_filter = geDrawDiagOptionalFilter("GETV_DRAWDIAG_STRIDE");
    const int flush_each = geDrawDiagFlushEach();
    unsigned long this_draw;
    unsigned long long this_serial;
    size_t vertex_count;
    size_t expected_floats = 0;
    size_t bytes = 0;
    int shape_ok = 0;
    struct GeDrawDiagStats stats;

    ge_drawdiag_last_selected = 0;

    if (!geDrawDiagIsEnabled()) {
        return;
    }

    if (!announced) {
        if (frame_filter != 0) {
            printf("[getv][drawdiag] enabled pre-submit limit=%lu frame=%lu flush=%d\n",
                   limit, frame_filter, flush_each);
        } else {
            printf("[getv][drawdiag] enabled pre-submit limit=%lu frame=all flush=%d\n",
                   limit, flush_each);
        }
        if (verts_filter != 0 || bytes_filter != 0 || stride_filter != 0) {
            printf("[getv][drawdiag] filter verts=%lu bytes=%lu stride_b=%lu\n",
                   verts_filter, bytes_filter, stride_filter);
        }
        if (flush_each) fflush(stdout);
        announced = 1;
    }

    if (frame != last_frame) {
        last_frame = frame;
        draw_in_frame = 0;
    } else {
        draw_in_frame++;
    }

    this_draw = draw_in_frame;
    this_serial = serial++;

    if (frame_filter != 0 && frame != frame_filter) {
        return;
    }

    if (emitted >= limit) {
        if (!truncated) {
            printf("[getv][drawdiag] limit=%lu reached; further draw lines suppressed\n", limit);
            if (flush_each) fflush(stdout);
            truncated = 1;
        }
        return;
    }

    if (buf == NULL || state == NULL || len > SIZE_MAX / sizeof(float) ||
        tris > SIZE_MAX / 3) {
        printf("[getv][drawdiag] frame=%lu draw=%lu serial=%llu invalid-input "
               "buf=%p state=%p floats=%zu tris=%zu\n",
               frame, this_draw, this_serial, (const void *)buf, (const void *)state, len, tris);
        if (flush_each) fflush(stdout);
        emitted++;
        return;
    }

    vertex_count = tris * 3;
    bytes = len * sizeof(float);

    if (state->stride_floats > 0 && vertex_count <= SIZE_MAX / state->stride_floats) {
        expected_floats = vertex_count * state->stride_floats;
        shape_ok = (expected_floats == len);
    }

    if ((verts_filter != 0 && vertex_count != verts_filter) ||
        (bytes_filter != 0 && bytes != bytes_filter) ||
        (stride_filter != 0 &&
         state->stride_floats * (unsigned long)sizeof(float) != stride_filter)) {
        return;
    }

    ge_drawdiag_last_selected = 1;

    stats = geDrawDiagScan(buf, len, vertex_count, state->stride_floats);

    printf("[getv][drawdiag] frame=%lu draw=%lu serial=%llu tris=%zu verts=%zu "
           "floats=%zu bytes=%zu stride_f=%u stride_b=%u expected=%zu shape=%s "
           "hash=%016llx finite=%zu min=%.9g max=%.9g maxabs=%.9g nan=%zu inf=%zu "
           "huge=%zu posmax=%.9g shader=%016llx prog=%u attribs=%u usedtex=%d%d "
           "t0=%d/%d/%u/%dx%d/f%d/h%d t1=%d/%d/%u/%dx%d/f%d/h%d "
           "depth=%d zwrite=%d blend=%d decal=%d\n",
           frame, this_draw, this_serial, tris, vertex_count, len, bytes,
           state->stride_floats, state->stride_floats * (unsigned)sizeof(float),
           expected_floats, shape_ok ? "ok" : "MISMATCH",
           (unsigned long long)stats.hash, stats.finite_count,
           (double)stats.min_value, (double)stats.max_value, (double)stats.max_abs,
           stats.nan_count, stats.inf_count, stats.huge_count, (double)stats.pos_max_abs,
           (unsigned long long)state->shader_id, state->program_id, state->num_attribs,
           state->used_texture[0], state->used_texture[1],
           state->texture_id[0] >= 0, state->texture_id[0], state->texture_gl_id[0],
           state->texture_width[0], state->texture_height[0], state->texture_filter[0],
           state->texture_has_height[0],
           state->texture_id[1] >= 0, state->texture_id[1], state->texture_gl_id[1],
           state->texture_width[1], state->texture_height[1], state->texture_filter[1],
           state->texture_has_height[1],
           state->depth_test, state->depth_write, state->blend, state->decal);

    if (flush_each) fflush(stdout);
    emitted++;
}

static int geDrawDiagLastSelected(void)
{
    return ge_drawdiag_last_selected;
}

#endif
