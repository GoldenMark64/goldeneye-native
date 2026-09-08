/* Opt-in, content-free PropRecord allocator telemetry.
 *
 * GETV_PROP_TELEMETRY=1 enables two versioned JSON Lines for a bounded run: one at the
 * post-setup boundary and one immediately before GETV_EXIT_FRAME terminates the process.
 * GETV_PROP_TELEMETRY_RUN_ID supplies the caller-owned, path-free correlation token.
 * No pointer, path, setup definition, or game content crosses this boundary.
 */
#ifndef GE_PROP_ALLOCATOR_TELEMETRY_H
#define GE_PROP_ALLOCATOR_TELEMETRY_H

#ifdef __cplusplus
extern "C" {
#endif

void gePortPropAllocatorReset(unsigned int capacity, unsigned int onscreen_capacity);
void gePortPropAllocatorAllocated(unsigned int slot_index);
void gePortPropAllocatorAllocationFailed(void);
void gePortPropAllocatorFreed(unsigned int slot_index);
void gePortPropAllocatorStageReady(const char *build_variant, int stage_id, int difficulty,
                                   int player_count, unsigned long long game_tick,
                                   unsigned long long render_frame);
void gePortPropAllocatorFrame(int onscreen_count);
void gePortPropAllocatorRunFinal(int stage_id, int difficulty, int player_count,
                                 unsigned long long game_tick,
                                 unsigned long long render_frame);

#ifdef __cplusplus
}
#endif
#endif /* GE_PROP_ALLOCATOR_TELEMETRY_H */
