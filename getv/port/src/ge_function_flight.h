#ifndef GE_FUNCTION_FLIGHT_H
#define GE_FUNCTION_FLIGHT_H

#include <stddef.h>
#include <stdint.h>

/*
 * GETV_FUNFLIGHT=<path>
 *
 * Kill-safe rolling function/submission trace for the GPU-hang investigation.
 * It deliberately records only the narrow Fast3D -> OpenGL path needed to join
 * a mapped GPU-flight serial back to the C functions that emitted it.
 */

enum GeFunctionFlightFunction {
    GE_FUNFLIGHT_FUNC_NONE = 0,
    GE_FUNFLIGHT_FUNC_GFX_RUN = 1,
    GE_FUNFLIGHT_FUNC_GFX_RUN_DL = 2,
    GE_FUNFLIGHT_FUNC_GFX_SP_TRI1 = 3,
    GE_FUNFLIGHT_FUNC_GFX_FLUSH_REASON = 4,
    GE_FUNFLIGHT_FUNC_GFX_FLUSH = 5,
    GE_FUNFLIGHT_FUNC_GFX_OPENGL_DRAW_TRIANGLES = 6,
    GE_FUNFLIGHT_FUNC_GL_BUFFER_DATA = 7,
    GE_FUNFLIGHT_FUNC_GL_DRAW_ARRAYS = 8,
};

enum GeFunctionFlightPhase {
    GE_FUNFLIGHT_PHASE_ENTER = 1,
    GE_FUNFLIGHT_PHASE_EXIT = 2,
    GE_FUNFLIGHT_PHASE_CALL = 3,
    GE_FUNFLIGHT_PHASE_BATCH = 4,
    GE_FUNFLIGHT_PHASE_GPU_LINK = 5,
    GE_FUNFLIGHT_PHASE_BEFORE = 6,
    GE_FUNFLIGHT_PHASE_AFTER = 7,
};

int geFunctionFlightIsEnabled(void);
void geFunctionFlightFreeze(void);
void geFunctionFlightSetFrame(uint32_t frame);
void geFunctionFlightSetCommandContext(int cmd_progress, int dl_depth,
                                       uint32_t opcode, uintptr_t cmd_ptr);
void geFunctionFlightEvent(uint32_t function_id, uint32_t phase,
                           uintptr_t ptr0, uint64_t arg0,
                           uint64_t arg1, uint64_t arg2);
uint64_t geFunctionFlightBeginBatch(const char *reason, size_t floats, size_t tris);
uint64_t geFunctionFlightCurrentBatch(void);
void geFunctionFlightLinkGpu(uint64_t gpu_serial, uint32_t frame,
                             uint64_t shader_id, uint32_t program_id,
                             size_t floats, size_t tris);
void geFunctionFlightEndBatch(void);

#endif
