/*
 * Bounded, opt-in main-thread stall diagnostics.
 *
 * The watchdog thread deliberately knows nothing about GoldenEye game structures.
 * The game/main thread publishes only integer telemetry through SDL atomics.  This
 * keeps a hard-stall report useful without introducing unsynchronised reads of live
 * engine state from another thread.
 */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>

#include "ge_stall_watchdog.h"
#include "ge_function_flight.h"

enum GeStallMacroPhase {
    GE_STALL_IDLE = 0,
    GE_STALL_WAIT_LOOP,
    GE_STALL_INPUT,
    GE_STALL_TICK_START,
    GE_STALL_LVL_MANAGE,
    GE_STALL_VIEW_MOVE,
    GE_STALL_LVL_RENDER,
    GE_STALL_GFX_SUBMIT,
    GE_STALL_GFX_START_FRAME,
    GE_STALL_GFX_RUN,
    GE_STALL_GFX_END_FRAME,
    GE_STALL_POST_FRAME,
    GE_STALL_MEMA_DEFRAG
};

enum GeStallHotspot {
    GE_STALL_HOT_NONE = 0,
    GE_STALL_HOT_ROOM_LOAD,
    GE_STALL_HOT_STAN_LOS
};

static SDL_atomic_t ge_stall_heartbeat;
static SDL_atomic_t ge_stall_macro;
static SDL_atomic_t ge_stall_macro_detail;
static SDL_atomic_t ge_stall_hotspot;
static SDL_atomic_t ge_stall_hotspot_detail;
static SDL_atomic_t ge_stall_frame;
static SDL_atomic_t ge_stall_room_loads;
static SDL_atomic_t ge_stall_stan_calls;
static SDL_atomic_t ge_stall_stan_phase;
static SDL_atomic_t ge_stall_stan_detail;
static SDL_atomic_t ge_stall_stan_aux0;
static SDL_atomic_t ge_stall_stan_aux1;
static SDL_atomic_t ge_stall_stan_aux2;
static SDL_atomic_t ge_stall_stan_roomprops_active;
static SDL_atomic_t ge_stall_gfx_phase;
static SDL_atomic_t ge_stall_gfx_opcode;
static SDL_atomic_t ge_stall_gfx_depth;
static SDL_atomic_t ge_stall_gfx_progress;
static SDL_atomic_t ge_stall_gfx_syncprobe;
static SDL_atomic_t ge_stall_drawsync;

/* Main-thread-only accumulators.  The watchdog never reads these directly. */
static int ge_stall_room_loads_local;
static int ge_stall_stan_calls_local;

static int ge_stall_enabled = -1;
static int ge_stall_started;

static const char *ge_stall_macro_name(int phase)
{
    switch (phase) {
    case GE_STALL_WAIT_LOOP:       return "wait_loop";
    case GE_STALL_INPUT:           return "input";
    case GE_STALL_TICK_START:      return "tick_start";
    case GE_STALL_LVL_MANAGE:      return "lvl_manage";
    case GE_STALL_VIEW_MOVE:       return "view_move";
    case GE_STALL_LVL_RENDER:      return "lvl_render";
    case GE_STALL_GFX_SUBMIT:      return "gfx_submit";
    case GE_STALL_GFX_START_FRAME: return "gfx_start_frame";
    case GE_STALL_GFX_RUN:         return "gfx_run";
    case GE_STALL_GFX_END_FRAME:   return "gfx_end_frame";
    case GE_STALL_POST_FRAME:      return "post_frame";
    case GE_STALL_MEMA_DEFRAG:     return "mema_defrag";
    default:                       return "idle";
    }
}

static const char *ge_stall_hotspot_name(int hotspot)
{
    switch (hotspot) {
    case GE_STALL_HOT_ROOM_LOAD: return "room_load";
    case GE_STALL_HOT_STAN_LOS:  return "stan_los";
    default:                     return "none";
    }
}

static const char *ge_stall_stan_phase_name(int phase)
{
    switch (phase) {
    case 1:  return "entry";
    case 2:  return "tilewalk";
    case 3:  return "post_tilewalk";
    case 4:  return "roomprops";
    case 5:  return "post_roomprops";
    case 6:  return "bounds";
    case 7:  return "post_bounds";
    case 8:  return "edge_loop";
    case 9:  return "final_walk";
    case 10: return "return";
    case 11: return "roomprops_chunk";
    case 12: return "prop_iter";
    case 13: return "stany_start";
    case 14: return "stany_dest";
    default: return "none";
    }
}

static const char *ge_stall_gfx_phase_name(int phase)
{
    switch (phase) {
    case 1: return "wapi_start";
    case 2: return "rapi_start";
    case 3: return "display_list";
    case 4: return "flush";
    case 5: return "rapi_end";
    case 6: return "swap_begin";
    case 7: return "overlay";
    case 8: return "gpu_finish";
    case 9: return "present";
    case 10: return "present_done";
    case 11: return "draw_sync";
    default: return "none";
    }
}

static int ge_stall_on(void)
{
    if (ge_stall_enabled < 0) {
        const char *e = getenv("GETV_STALLTRACE");
        ge_stall_enabled = (e != NULL && *e == '1') ? 1 : 0;
    }
    return ge_stall_enabled;
}

static int ge_stall_watchdog(void *unused)
{
    int last_heartbeat = SDL_AtomicGet(&ge_stall_heartbeat);
    Uint32 last_progress = SDL_GetTicks();
    Uint32 last_report = 0;
    int reporting = 0;
    int function_flight_frozen = 0;
    (void)unused;

    for (;;) {
        Uint32 now;
        Uint32 age;
        int heartbeat;

        SDL_Delay(50);
        now = SDL_GetTicks();
        heartbeat = SDL_AtomicGet(&ge_stall_heartbeat);

        if (heartbeat != last_heartbeat) {
            if (reporting) {
                fprintf(stderr, "[getv][stall] recovered after %ums\n",
                        (unsigned)(now - last_progress));
                fflush(stderr);
            }
            last_heartbeat = heartbeat;
            last_progress = now;
            reporting = 0;
            continue;
        }

        age = now - last_progress;
        if (age < 250) {
            continue;
        }

        /* Preserve the high-rate renderer trace before a recoverable stall can
         * resume and overwrite it.  The observed GPU hangs are ~10 seconds; a
         * 2-second threshold stays far inside them while avoiding a one-shot
         * freeze on an ordinary loading/shader-compilation hitch.  No renderer
         * events advance while the main thread is stalled, so waiting 2 seconds
         * does not lose the pre-stall function history. */
        if (!function_flight_frozen && age >= 2000) {
            geFunctionFlightFreeze();
            function_flight_frozen = 1;
        }

        if (!reporting || now - last_report >= 1000) {
            int macro = SDL_AtomicGet(&ge_stall_macro);
            int macro_detail = SDL_AtomicGet(&ge_stall_macro_detail);
            int hotspot = SDL_AtomicGet(&ge_stall_hotspot);
            int hotspot_detail = SDL_AtomicGet(&ge_stall_hotspot_detail);
            int frame = SDL_AtomicGet(&ge_stall_frame);
            int room_loads = SDL_AtomicGet(&ge_stall_room_loads);
            int stan_calls = SDL_AtomicGet(&ge_stall_stan_calls);
            int stan_phase = SDL_AtomicGet(&ge_stall_stan_phase);
            int stan_detail = SDL_AtomicGet(&ge_stall_stan_detail);
            int stan_aux0 = SDL_AtomicGet(&ge_stall_stan_aux0);
            int stan_aux1 = SDL_AtomicGet(&ge_stall_stan_aux1);
            int stan_aux2 = SDL_AtomicGet(&ge_stall_stan_aux2);
            int gfx_phase = SDL_AtomicGet(&ge_stall_gfx_phase);
            int gfx_opcode = SDL_AtomicGet(&ge_stall_gfx_opcode);
            int gfx_depth = SDL_AtomicGet(&ge_stall_gfx_depth);
            int gfx_progress = SDL_AtomicGet(&ge_stall_gfx_progress);

            fprintf(stderr,
                    "[getv][stall] age=%ums frame=%d macro=%s detail=%d "
                    "last_hotspot=%s hotdetail=%d roomloads=%d stancalls>=%d "
                    "stanphase=%s standetail=%d stanaux=%d,%d,%d "
                    "gfxphase=%s gfxcmd=%d gfxop=0x%02x gfxdepth=%d\n",
                    (unsigned)age, frame, ge_stall_macro_name(macro), macro_detail,
                    ge_stall_hotspot_name(hotspot), hotspot_detail,
                    room_loads, stan_calls, ge_stall_stan_phase_name(stan_phase),
                    stan_detail, stan_aux0, stan_aux1, stan_aux2,
                    ge_stall_gfx_phase_name(gfx_phase), gfx_progress,
                    gfx_opcode & 0xff, gfx_depth);
            fflush(stderr);
            last_report = now;
            reporting = 1;
        }
    }

    return 0;
}

static void ge_stall_start(void)
{
    SDL_Thread *thread;

    if (!ge_stall_on() || ge_stall_started) {
        return;
    }

    ge_stall_started = 1;
    thread = SDL_CreateThread(ge_stall_watchdog, "getv-stall-watchdog", NULL);
    if (thread == NULL) {
        fprintf(stderr, "[getv][stall] watchdog start FAILED: %s\n", SDL_GetError());
        fflush(stderr);
        return;
    }

    SDL_DetachThread(thread);
    fprintf(stderr,
            "[getv][stall] watchdog active (250ms threshold, 1000ms repeat)\n");
    fflush(stderr);
}

static void ge_stall_macro_mark(int phase, int detail)
{
    if (!ge_stall_on()) {
        return;
    }
    ge_stall_start();
    SDL_AtomicSet(&ge_stall_macro_detail, detail);
    SDL_AtomicSet(&ge_stall_macro, phase);
    SDL_AtomicAdd(&ge_stall_heartbeat, 1);
}

void gePortStallWaitLoop(void)   { ge_stall_macro_mark(GE_STALL_WAIT_LOOP, 0); }
void gePortStallInput(void)      { ge_stall_macro_mark(GE_STALL_INPUT, 0); }
void gePortStallLvlManage(void)  { ge_stall_macro_mark(GE_STALL_LVL_MANAGE, 0); }
void gePortStallLvlRender(void)  { ge_stall_macro_mark(GE_STALL_LVL_RENDER, 0); }
void gePortStallGfxSubmit(void)  { ge_stall_macro_mark(GE_STALL_GFX_SUBMIT, 0); }
void gePortStallMemaDefrag(void) { ge_stall_macro_mark(GE_STALL_MEMA_DEFRAG, 0); }

void gePortStallViewMove(int player)
{
    ge_stall_macro_mark(GE_STALL_VIEW_MOVE, player);
}

void gePortStallTickStart(int frame)
{
    if (!ge_stall_on()) {
        return;
    }
    ge_stall_room_loads_local = 0;
    ge_stall_stan_calls_local = 0;
    SDL_AtomicSet(&ge_stall_room_loads, 0);
    SDL_AtomicSet(&ge_stall_stan_calls, 0);
    SDL_AtomicSet(&ge_stall_stan_phase, 0);
    SDL_AtomicSet(&ge_stall_stan_detail, 0);
    SDL_AtomicSet(&ge_stall_stan_aux0, 0);
    SDL_AtomicSet(&ge_stall_stan_aux1, 0);
    SDL_AtomicSet(&ge_stall_stan_aux2, 0);
    SDL_AtomicSet(&ge_stall_stan_roomprops_active, 0);
    SDL_AtomicSet(&ge_stall_hotspot, GE_STALL_HOT_NONE);
    SDL_AtomicSet(&ge_stall_hotspot_detail, 0);
    SDL_AtomicSet(&ge_stall_frame, frame);
    ge_stall_macro_mark(GE_STALL_TICK_START, 0);
}

void gePortStallGfxStartFrame(void)
{
    ge_stall_macro_mark(GE_STALL_GFX_START_FRAME, 0);
}

void gePortStallGfxRun(void)
{
    ge_stall_macro_mark(GE_STALL_GFX_RUN, 0);
}

void gePortStallGfxEndFrame(void)
{
    ge_stall_macro_mark(GE_STALL_GFX_END_FRAME, 0);
}

void gePortStallGfxTaskArm(void)
{
    if (!ge_stall_on()) {
        return;
    }
    SDL_AtomicSet(&ge_stall_gfx_phase, 0);
    SDL_AtomicSet(&ge_stall_gfx_opcode, 0);
    SDL_AtomicSet(&ge_stall_gfx_depth, 0);
    SDL_AtomicSet(&ge_stall_gfx_progress, 0);
}

void gePortStallGfxPhase(int phase)
{
    if (!ge_stall_on()) {
        return;
    }
    SDL_AtomicSet(&ge_stall_gfx_phase, phase);
}

void gePortStallGfxCommand(int opcode, int depth, int progress)
{
    if (!ge_stall_on()) {
        return;
    }
    SDL_AtomicSet(&ge_stall_gfx_opcode, opcode);
    SDL_AtomicSet(&ge_stall_gfx_depth, depth);
    SDL_AtomicSet(&ge_stall_gfx_progress, progress);
}

void gePortStallGfxPresentProbeArm(void)
{
    const char *e;

    if (!ge_stall_on()) {
        SDL_AtomicSet(&ge_stall_gfx_syncprobe, 0);
        return;
    }

    e = getenv("GETV_GLSYNC_DIAG");
    SDL_AtomicSet(&ge_stall_gfx_syncprobe, e != NULL && *e == '1');
}

int gePortStallGfxSyncProbeEnabled(void)
{
    return SDL_AtomicGet(&ge_stall_gfx_syncprobe) != 0;
}

void gePortDrawSyncTaskArm(void)
{
    const char *e;

    if (!ge_stall_on()) {
        SDL_AtomicSet(&ge_stall_drawsync, 0);
        return;
    }

    e = getenv("GETV_DRAWDIAG_SYNC");
    SDL_AtomicSet(&ge_stall_drawsync, e != NULL && *e == '1');
}

int gePortDrawSyncEnabled(void)
{
    return SDL_AtomicGet(&ge_stall_drawsync) != 0;
}

void gePortStallPostFrame(int frame)
{
    if (!ge_stall_on()) {
        return;
    }
    SDL_AtomicSet(&ge_stall_frame, frame);
    SDL_AtomicSet(&ge_stall_room_loads, ge_stall_room_loads_local);
    SDL_AtomicSet(&ge_stall_stan_calls, ge_stall_stan_calls_local);
    ge_stall_macro_mark(GE_STALL_POST_FRAME, 0);
}

void gePortStallRoomLoad(int room)
{
    if (!ge_stall_on()) {
        return;
    }
    ge_stall_start();
    ge_stall_room_loads_local++;
    SDL_AtomicSet(&ge_stall_room_loads, ge_stall_room_loads_local);
    SDL_AtomicSet(&ge_stall_hotspot_detail, room);
    SDL_AtomicSet(&ge_stall_hotspot, GE_STALL_HOT_ROOM_LOAD);
}

void gePortStallStanCall(void)
{
    if (!ge_stall_on()) {
        return;
    }
    ge_stall_start();
    ge_stall_stan_calls_local++;

    /*
     * Publish the first call and then one sample per 32 calls.  The local increment is
     * cheap even in a LOS storm; doing two atomic operations for every call would perturb
     * the very amplification this probe is meant to measure.
     */
    if (ge_stall_stan_calls_local == 1 || (ge_stall_stan_calls_local & 31) == 0) {
        SDL_AtomicSet(&ge_stall_stan_calls, ge_stall_stan_calls_local);
        SDL_AtomicSet(&ge_stall_hotspot_detail, ge_stall_stan_calls_local);
        SDL_AtomicSet(&ge_stall_hotspot, GE_STALL_HOT_STAN_LOS);
    }
}

void gePortStallStanPhase(int phase, int detail)
{
    if (!ge_stall_on()) {
        return;
    }
    ge_stall_start();
    SDL_AtomicSet(&ge_stall_stan_phase, phase);
    SDL_AtomicSet(&ge_stall_stan_detail, detail);
    SDL_AtomicSet(&ge_stall_stan_aux0, 0);
    SDL_AtomicSet(&ge_stall_stan_aux1, 0);
    SDL_AtomicSet(&ge_stall_stan_aux2, 0);
    SDL_AtomicSet(&ge_stall_hotspot, GE_STALL_HOT_STAN_LOS);
}

void gePortStallStanRoomPropsBegin(int roomcount)
{
    if (!ge_stall_on()) {
        return;
    }
    SDL_AtomicSet(&ge_stall_stan_roomprops_active, 1);
    gePortStallStanPhase(4, roomcount);
}

void gePortStallStanRoomPropsProgress(int room, int chunk, int steps, int writes)
{
    if (!ge_stall_on() || !SDL_AtomicGet(&ge_stall_stan_roomprops_active)) {
        return;
    }
    ge_stall_start();
    SDL_AtomicSet(&ge_stall_stan_detail, steps);
    SDL_AtomicSet(&ge_stall_stan_aux0, room);
    SDL_AtomicSet(&ge_stall_stan_aux1, chunk);
    SDL_AtomicSet(&ge_stall_stan_aux2, writes);
    SDL_AtomicSet(&ge_stall_stan_phase, 11);
    SDL_AtomicSet(&ge_stall_hotspot, GE_STALL_HOT_STAN_LOS);
}

void gePortStallStanRoomPropsEnd(void)
{
    if (!ge_stall_on()) {
        return;
    }
    SDL_AtomicSet(&ge_stall_stan_roomprops_active, 0);
}

int gePortStallStanRoomPropsActive(void)
{
    return ge_stall_on() && SDL_AtomicGet(&ge_stall_stan_roomprops_active);
}

void gePortStallStanPropIter(int slot, int propindex)
{
    if (!ge_stall_on()) {
        return;
    }
    ge_stall_start();
    SDL_AtomicSet(&ge_stall_stan_detail, slot);
    SDL_AtomicSet(&ge_stall_stan_aux0, propindex);
    SDL_AtomicSet(&ge_stall_stan_aux1, 0);
    SDL_AtomicSet(&ge_stall_stan_aux2, 0);
    SDL_AtomicSet(&ge_stall_stan_phase, 12);
    SDL_AtomicSet(&ge_stall_hotspot, GE_STALL_HOT_STAN_LOS);
}
