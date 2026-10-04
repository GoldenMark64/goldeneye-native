#ifndef GE_STALL_WATCHDOG_H
#define GE_STALL_WATCHDOG_H

/*
 * GETV_STALLTRACE=1
 *
 * Diagnostic-only main-thread stall locator.  The watchdog owns its telemetry and
 * reads only SDL atomics; it never dereferences game state from the watchdog thread.
 */
void gePortStallWaitLoop(void);
void gePortStallInput(void);
void gePortStallTickStart(int frame);
void gePortStallLvlManage(void);
void gePortStallViewMove(int player);
void gePortStallLvlRender(void);
void gePortStallGfxSubmit(void);
void gePortStallMemaDefrag(void);

void gePortStallGfxStartFrame(void);
void gePortStallGfxRun(void);
void gePortStallGfxEndFrame(void);
void gePortStallPostFrame(int frame);
void gePortStallGfxTaskArm(void);
void gePortStallGfxPhase(int phase);
void gePortStallGfxCommand(int opcode, int depth, int progress);
void gePortStallGfxPresentProbeArm(void);
int gePortStallGfxSyncProbeEnabled(void);
void gePortDrawSyncTaskArm(void);
int gePortDrawSyncEnabled(void);

void gePortStallRoomLoad(int room);
void gePortStallStanCall(void);
void gePortStallStanPhase(int phase, int detail);
void gePortStallStanRoomPropsBegin(int roomcount);
void gePortStallStanRoomPropsProgress(int room, int chunk, int steps, int writes);
void gePortStallStanRoomPropsEnd(void);
int gePortStallStanRoomPropsActive(void);
void gePortStallStanPropIter(int slot, int propindex);

#endif
