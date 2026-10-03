# GoldenEye decomp changes - the only copy that survives a re-clone

`vendor/` is gitignored (see `.gitignore`), so **every change the port makes to
`vendor/ge-decomp` is untracked**. Re-cloning or resetting the decomp discards all of it.
That is the same arrangement the Perfect Dark port uses, and the same warning applies here:

> **Re-cloning `vendor/` loses the work unless the patches are re-applied.**

Split by *when* they can be applied rather than by subject.

| Patch | Size | Covers | Applied |
|---|---|---|---|
| `0001-source.patch` | 1.4 MB, 140 files | `src/` (131), `include/` (8), `tools/` (1) | immediately after cloning the decomp |
| `0002-assets.patch` | 45 KB, 7 files | generated asset sources | at the end of the asset pipeline |
| `0006-fov-live-setter.patch` | 1 file | `src/fr.c` | after `0001` |
| `0007-load-trace.patch` | 1 file | asset load tracing | after `0001` |
| `0008-crosshair-color.patch` | 1 file | `src/game/gunfire.c`, colour and scale | after `0001` |
| `0009-freerun-divider.patch` | 1 file | `src/game/frametiming.c` | after `0001` |
| `0010-state-dump-player-position.patch` | 1 file | `src/game/objective_status.c` | after `0001` |
| `0011-netplay-tick-integration.patch` | 2 KB, 2 files | `src/boss.c`, `src/ge_port_decls.h` | after `0001` |
| `0012-real-font-overlay.patch` | 1 file | `src/game/textrelated.c` | after `0001` |
| `0013-lockstep-pinned-sim-step.patch` | 1 file | `src/game/frametiming.c` | after `0001` |
| `0014-lockstep-cull-on-the-tick.patch` | 1 file | `src/game/lv.c` | after `0001` |
| `0015-aim-toggle.patch` | 1 file | `src/game/options.c` | after `0001` |
| `0016-freecam.patch` | 2 files | `src/game/bg.c`, `src/game/bondview2.c` | after `0001` |
| `0017-coop-friendly-fire.patch` | 1 file | `src/game/bondview2.c` | after `0001` |
| `0018-coop-one-death-is-not-the-team.patch` | 1 file | `src/game/bondview2.c` | after `0017` |
| `0019-coop-respawn.patch` | 1 file | `src/game/bondview2.c` | after `0018` |
| `0020-kill-selftest.patch` | 1 file | `src/game/bondview2.c` | after `0001` |
| `0021-stan-pointer-return-decls.patch` | 1 file | `src/game/stan.c` | after `0001` |
| `0022-lockstep-stop-shuffling-every-frame.patch` | 1 file | `src/game/player.c` | after `0001` |
| `0023-enemy-gibs.patch` | 5 files | `src/game/chr.c`, `chraction.c`, `explosion.c`, `explosion.h`, `propobj.c` | after `0022` |
| `0024-multi-ammo-endianness.patch` | 2 files | `src/bondtypes.h`, `src/game/propobj.c` | after `0023` |
| `0025-cuff-native-pointer-stride.patch` | 1 file | `src/game/bondview2.c` | after `0024` |
| `0026-bloodier-gibs.patch` | 4 files | `explosion.c`, `explosion.h`, `initexplosioncasing.c`, `chr.c`: blood, reset and visible self-test | after `0025` |
| `0027-external-rom-path.patch` | 2 files | extraction scripts accept the user's existing local ROM path | after `0026` |
| `0028-prop-allocator-telemetry.patch` | 3 files | `chrprop.c`, `lv.c`, `ge_port_decls.h`: allocator/lifecycle callbacks | after `0027` |
| `0033-guard-gun-sfx-native-layout.patch` | 1 file | `src/game/chraction.c`: native guard weapon-SFX item lookup uses `WeaponObjRecord.weaponnum` | after `0032` |
| `0034-surface2-remote-mine-native-layout.patch` | 1 file | `src/game/propobj.c`: native thrown-item lookup compares `WeaponObjRecord.weaponnum` instead of the legacy `KeyRecord` overlay | after `0033` |
| `0035-gunbarrel-authored-cadence.patch` | 1 file | `src/game/title.c`: native gunbarrel movement, Bond animation and blood wipe advance on the authored half-field cadence | after `0034` |
| `0036-tank-mount-diagnostic.patch` | 1 file | `src/game/bondview2.c`: env-gated tank mount state-transition diagnostics only; no gameplay behavior change | after `0035` |
| `0037-file-select-hitbox-native-layout.patch` | 1 file | `src/game/front.c`: native file-select hitboxes use real `coord2d` extent/projection pairs instead of scalar stack aliases | after `0036` |
| `0038-tank-contact-native-tolerance.patch` | 1 file | `src/game/bondview2.c`: native tank outer-contact check gets a local 0.01-unit tolerance so settled collision contact reaches the climb state | after `0037` |
| `0039-tank-inner-diagnostic.patch` | 1 file | `src/game/bondview2.c`: native tank inner-hull and top-move diagnostics for the downstream mount gate | after `0038` |
| `0040-tank-native-object-state-alias.patch` | 1 file | `src/game/propobj.c`: native object collision enable/disable writes `ObjectRecord.state` directly instead of the N64 `ChrRecord.accuracyrating` byte alias | after `0039` |
| `0041-archives-stall-watchdog-hooks.patch` | 3 files | `boss.c`, `bg.c`, `stan.c`: native-only phase/hotspot hooks for the opt-in Archives hard-stall watchdog; no gameplay behavior change | after `0040` |
| `0042-gunbarrel-native-cadence-split.patch` | 1 file | `src/game/title.c`: restore title/blood state to every render while keeping native NTSC Bond model animation at one tick per render | after `0041` |
| `0043-gunbarrel-native-bond-resync.patch` | 1 file | `src/game/title.c`: restore original NTSC Bond model count=2 so Bond stays synchronized with the every-render gunbarrel state restored by 0042 | after `0042` |
| `0044-depot-timedmine-diagnostic.patch` | 3 files | `src/game/objective_status.c`, `gun.c`, `propobj.c`: bounded native diagnostics for Depot network objective tags 1-3 and the timed-mine create/init/tick/countdown/explosion path; no gameplay behavior change | after `0043` |
| `0045-proximity-box-stall-diagnostic.patch` | 3 files | native-only proximity/explosion/room-list diagnostics for the intermittent box stall; no gameplay behavior change | after `0044` |
| `0046-stan-room-buffer-native-terminator.patch` | 1 file | `src/game/stan.c`: native room buffer holds 20 room IDs plus the `-1` terminator consumed by `roomGetProps()` | after `0045` |
| `0047-stan-los-subphase-diagnostic.patch` | 1 file | `src/game/stan.c`: native-only STAN LOS subphase markers and impossible collision-edge-count report | after `0046` |
| `0048-stan-los-deep-diagnostic.patch` | 3 files | `stan.c`, `chrprop.c`, `propobj.c`: deep room-chain/prop-list/STAN-Y diagnostics plus compile-time native `collision_data` layout/allocation proof; no gameplay behavior change | after `0047` |
| `0049-native-collision-polygon-extent.patch` | 5 files | native collision hulls index the real 8-point `coord2d` backing store instead of indexing past `rect4f.points[4]`; non-native member access is unchanged | after `0048` |
| `0050-gfx-run-progress-diagnostic.patch` | 1 file | `boss.c`: arms the native graphics-stall probe for each submitted task; port-side watchdog/renderer telemetry reports `gfx_run` subphase, command progress, opcode and display-list depth without changing rendering behavior | after `0049` |
| `0051-gpu-sync-present-diagnostic.patch` | 1 file | `boss.c`: arms an opt-in present probe; port-side `GETV_GLSYNC_DIAG=1` inserts `glFinish()` before `SDL_GL_SwapWindow()` and reports overlay/GPU-finish/present phases so queued GPU work can be separated from presentation blocking | after `0050` |
| `0052-draw-sync-attribution-diagnostic.patch` | 1 file | `boss.c`: arms an opt-in per-draw attribution probe; port-side `GETV_DRAWDIAG_SYNC=1` synchronises immediately after draws selected by the existing `GETV_DRAWDIAG_*` filters and reports `draw_sync` while blocked | after `0051` |
| `0054-native-ladder-locus-record.patch` | 1 file | `bondview2.c`: native `MoveBond()` uses the real widened `StandTileLocusCallbackRecord` instead of the retail two-word placeholder, preventing ladder/locus record writes and `count` reads from running past the local object on 64-bit builds | after `0052` |
| `0055-stan-recovery-safety.patch` | 2 files | `bondview2.c`, `stan.c`: native STAN recovery avoids zero-link modulo and resets per-tile `nearEdge` state before floor selection | after `0054` |
| `0056-stan-ground-cylinder-recovery.patch` | 4 files | `bondview2.c`, `stan.c`, `stan.h`, `stan_ground_native.h`: native out-of-bounds STAN recovery follows authored movement topology, filters plausible rooms, and selects center/cylinder floor support with PD-derived semantics; retail/non-native random recovery is preserved | after `0055` |
| `0057-native-reverse-animation-interpolation.patch` | 1 file | `model.c`: native reverse animation playback brackets fractional frames with `ceil(frame)` and the preceding sample, keeping interpolation fractions in [0,1]; forward and non-native behavior are unchanged | after `0056` |
| `0058-bunker2-cctv-objective-diagnostic.patch` | 1 file | `objective_status.c`: opt-in read-only Bunker 2 objective #2 trace for CCTV tags 28â€“33, reporting tag/object/prop health and destruction state without changing objective evaluation | after `0057` |
| `0059-gunbarrel-bond-speed-calibration.patch` | 1 file | `title.c`: native-only `GETV_GUNBARREL_BOND_SPEED` calibration override for Bond walk animation; default/non-native retail rate remains 0.91 | after `0058` |
| `0060-gpu-render-source-provenance.patch` | 2 files | `bg.c`, `model.c`: native-only registration of room/model display-list producers so synchronized i915/flight-recorder hangs can name the GoldenEye source subsystem and room/model identity | after `0059` |

`0029-modern-mouse-look.patch` adds the direct mouse-angle consumer in `bondview2.c`,
after `0028`. It respects game input locks and leaves vehicle controls on the original path.

## The gap at 0003, 0004 and 0005 is deliberate

They were folded into `0001` the last time it was refreshed, and nobody retired them
afterwards. Applying them on top of `0001` then tried to add lines that were already there and
failed with `patch does not apply`. That is issue #4: it stopped `tools/install.sh` dead on a
fresh clone, and it had been broken for everyone who was not working from a tree that had
already been set up by hand.

Verified before removing them rather than assumed. On a fresh clone with `0001` applied,
`include/platform_info.h` and `src/game/stan.h` are already byte-identical to a working tree,
and `src/game/objective_status.c` differs only by what `0010` adds. Nothing in the three was
still reachable.

So the numbering is a record of what has existed rather than a promise that it stays
contiguous. A gap is not automatically a mistake, and a missing file is not automatically a
lost patch. Check against the scripts below before assuming either way.

## What 0001 carries beyond the decomp's own source

Most of it is the port layer's own changes to `src/` and `include/`. The part worth calling out
is the accessor surface, because it is what the port links against and it used to live in its
own patch:

- Player state behind `GePlayerState`: position (from `prop->pos`, not `player->pos`), angle,
  room from the stan tile, health, weapon, score.
- Control style helpers: `gePortPlayerControlStyle`, `gePortPlayerUsesTwoPads`,
  `gePortPlayerMovePad`.
- Navigation probes: `gePortProbeStandable`, `gePortProbeWalkable`, `gePortCanStandAt`,
  `gePortPathClear`, `gePortObstacleEdge`.
- The engine's own navigation graph: `gePortNavCount`, `gePortNavAt`, `gePortNavNeighbours`,
  `gePortNavNearest`, `gePortNavNearestPad`, `gePortNavRoute`.
- The guard door path, `gePortOpenDoorAhead`, which honours the door's key flags.
- The live enemy readout, `gePortEnemyCount` / `gePortEnemyAt` / `gePortEnemyFacing`.
- Objective state and prop extents.

See `docs/PLAYER_API.md` and `docs/ENEMY_API.md` for what each one returns.

**If you add a symbol the port layer calls, it belongs in a patch the same day.** `vendor/` is
gitignored, so decomp changes do not travel in a bundle. A build once failed to link for a full
day on `gePortPlayerMovePad`, `gePortPlayerAngle`, `gePortPlayerRoom`, `gePortPlayerHealth`,
`gePortPlayerWeapon`, `gePortPlayerScore` and `gePortProbeStandable`, every one of them written,
tested and committed on the other machine. Everything compiled; only the link knew. A commit
that builds on one machine and cannot link on the other is indistinguishable from a broken
commit, and the other side has no way to tell which.

**One patch per file.** The split that became `0003-enemy-shim` and `0004-player-accessors` was
generated twice against the same base, so only the first would apply, and a fresh clone got the
enemy shim without the player accessors. Two patches touching one file is the trap described in
the refresh notes below.

## Registration: two mechanisms, and only one of them is automatic

`tools/install.sh` and `tools/install.ps1` glob `getv/patches/0*.patch` in numeric order and
skip `0002` until the asset pipeline has run, so a patch added later is picked up on its own and
cannot be forgotten.

`tools/setup.sh` and `tools/setup-mac.sh` name each patch explicitly, so **a patch added there
must be registered by hand, in the same commit**. A patch that is committed but never applied by
a setup script is invisible: the tree builds, nothing complains, and the change simply is not
there. That has happened twice, to `0003` and then to `0007`, which is twice more than it should.

## Restore

Every patch in numeric order, with `0002` last because it needs the generated assets to exist.
Do not abbreviate this list: it used to stop after `0003`, and a tree missing `0006` links with
`gePortSetFovScale` undefined, which is issue #5.

```bash
cd vendor/ge-decomp
git apply ../../getv/patches/0001-source.patch
git apply ../../getv/patches/0006-fov-live-setter.patch
git apply ../../getv/patches/0007-load-trace.patch
git apply ../../getv/patches/0008-crosshair-color.patch
git apply ../../getv/patches/0009-freerun-divider.patch
git apply ../../getv/patches/0010-state-dump-player-position.patch
git apply ../../getv/patches/0011-netplay-tick-integration.patch
git apply ../../getv/patches/0012-real-font-overlay.patch
git apply ../../getv/patches/0013-lockstep-pinned-sim-step.patch
git apply ../../getv/patches/0014-lockstep-cull-on-the-tick.patch
git apply ../../getv/patches/0015-aim-toggle.patch
git apply ../../getv/patches/0016-freecam.patch
git apply ../../getv/patches/0017-coop-friendly-fire.patch
git apply ../../getv/patches/0018-coop-one-death-is-not-the-team.patch
git apply ../../getv/patches/0019-coop-respawn.patch
git apply ../../getv/patches/0020-kill-selftest.patch
git apply ../../getv/patches/0021-stan-pointer-return-decls.patch
git apply ../../getv/patches/0022-lockstep-stop-shuffling-every-frame.patch
git apply ../../getv/patches/0023-enemy-gibs.patch
git apply ../../getv/patches/0024-multi-ammo-endianness.patch
git apply ../../getv/patches/0025-cuff-native-pointer-stride.patch
git apply ../../getv/patches/0026-bloodier-gibs.patch
git apply ../../getv/patches/0027-external-rom-path.patch
git apply ../../getv/patches/0028-prop-allocator-telemetry.patch
git apply ../../getv/patches/0029-modern-mouse-look.patch
git apply ../../getv/patches/0030-autocrouch-render-ticks.patch
git apply ../../getv/patches/0031-native-joy-poll-handshake.patch
git apply ..‹­¦ëm®éÜj×¢¸ Šv¥jšk£¦j×­¢G§r‹§·];ÓkYKÙ›ÛË˜È[™‹ÜÜ˜ËÙØ[YKÙ›ÛË˜Âˆ‚ˆÚXÚ]™\ˆ›İ]K™\šYH]›İ[™]š\ÈÛÈHš\İ[™HÛÜH™Y›Ü™H\İ[™È]‚‚ˆ˜\Úˆ]Ú\HKYK\[ˆ‹‹Ë‹‹ÙÙ]‹Ü]Ú\ËÌ‹][™Ëœ]Úˆ‚‘XXÚ]Ú]\İ™H™YÙ[™\˜]Yİ™\ˆ]ÈİÛˆ]ËˆH˜\™HÚ]Y™˜Ûİ[İÙY\H[\™B™^˜XİY\ÜÙ]™YH[ÈXH[™™YÈÙˆYYØX]\ÈÙˆ“ÓKY\š]™Y]K‚‚˜˜\Ú˜Ù™[™Ü‹ÙÙKYXÛÛ\™Ú]Y™ˆKHÜ˜È[˜ÛYHÛÛÈˆ‹‹Ë‹‹ÙÙ]‹Ü]Ú\ËÌK\Ûİ\˜ÙKœ]Ú™Ú]Y™ˆKH\ÜÙ]ËØ[š[X][ÛX›WÙ]Kš\ÜÙ]ËÙ›ÛÙ˜Èˆ\ÜÙ]ËÙ›ÛÙ›Û˜[šÑÛİXË˜È\ÜÙ]ËÙ›ÛÙ›Û\šXÚ›Û˜Èˆ\ÜÙ]ËÛØœÙYËÜÙ]\ÙKÕ\Ù]\[–‹˜È\ÜÙ]ËÛØœÙYËÜÙ]\Ú‹Õ\Ù]\[–‹˜Èˆ\ÜÙ]ËÛØœÙYËÜÙ]\İKÕ\Ù]\[–‹˜Èˆ‹‹Ë‹‹ÙÙ]‹Ü]Ú\ËÌ‹X\ÜÙ]Ëœ]Ú˜‚“™]™\ˆY\ÜÙ]ËÜ˜\™]Ø\™[ÙÛË˜ØÈ\ÈÛÛ[X[™ˆ]È™\]Z\™YXÛ\˜][Ûˆ[™]K[Ü™\ˆÚ[™ÙBš\È\™›Ü›YYÛ›HÛˆHÛÛšX]Ü‰ÜÈYÛ›Ü™YØØ[H^˜XİYš[HB˜ÛÛËİ˜[œÙ›Ü›WÜ˜\™]Ø\™[ÙÛËœX‚‚ŠŠ”[ˆ˜\ÚÛÛËØÚXÚ×Ü]Ú\ËœÚY\ˆ™YÙ[™\˜][™È[][™ËŠŠˆ]ÛÛ™\ÈHš\İ[™HXÛÛ\˜[™\Y\È]™\H]ÚÈ]ÚXÚ\ÈHÛ™H[™ÈHÛÜšÚ[™È™YHØ[››İ[[İNˆH™YB][™XYH\ÈHÚ[™Ù\ÈÚ[XØÙ\H]Ú]›Èœ™\ÚÛÛ™HÛİ[ˆ]ÚXÚÈ\ÈB™Y™™\™[˜ÙH™]ÙY[ˆš[™[™È\È›İÈ[™Hİ˜[™Ù\ˆš[™[™È]ÛˆZ\ˆš\œİ[œİ[ˆ][ÛÂ™\šYšY\ÈXXÚ]Ú\È˜[YYH[™YHÙ]\ØÜš\Ë‚‚”™YÙ[™\˜][™ÈH[™\ÈİÈ˜Ø\ÈÛÜœ\YˆÛ™HÛÛ^[™HØ\È[˜Ø]YÚ]\Xœ™Z™XİY][™H™YH]Ø\ÈYX[Èš^Ûİ[›İ[šËˆ]\È\ÜİYHÍKˆ›İ[™]š\]™\Bœ™YÙ[™\˜]Y]ÚÛÈHš\İ[™HÛÜH™Y›Ü™HÛÛ[Z][™È]‚‚ˆÈÈÚHˆ\ÈÙ\\˜]B‚•HÙ]™[ˆš[\È[ˆ˜È›İ^\İ[ˆHœ™\ÚXÛÛ\ˆ^H\™H›ÙXÙYœ›ÛHH“ÓHHBœ\[[™H[ˆØÜËÔÑUT›YÙXİ[ÛˆËKˆ\Z[™È]X\›H˜Z[Ë[™\Z[™È]™Y›Ü™B˜[š\]ZYWØ\ÜÙ]ÜŞ[X›ÛËœX[œÈÙ]ÈH›ÛŞ[X›ÛÈİX›K\™Yš^Y‚‚˜X[ÛÈÜ™X]\Èš]™Hš[\È]]™H›È\İ™X[HÛİ[\œ\Û™HÙˆÚXÚ˜ÛÛËÙÙ[—Ü›ÜY—Û^[İ]œXH\[[™H]Ù[ˆØ[Ë‚‚ˆÈÈÚ]\È[X™\˜][H›İ[ˆ\™B‚‘Ù[™\˜]Y]HHH]Y[ÈÙYÛY[HØœÙYÈ›ØœËH[š[X][Ûˆ›ØœËH[XYÙ\ÈÙYÛY[H\‹[[Ù[[Ù[˜Øš[\Ëˆ^H\™H\™ÙK\š]™Yœ›ÛHH“ÓK[™™\›ÙXÚX›Hœ›ÛB˜ÛÛËØ[™ØÜš\ËØÈÙYHØÜËÔÑUT›YÙXİ[ÛˆËKˆ
Š“™]™\ˆÛÛ[Z]“ÓKY\š]™Y]KŠŠ‚‚˜ÌX]]ØÜ›İXÚ\™[™\‹]XÚÜËœ]Ú™\Ù\™\È]]ËXÜ›İXÚ[[XÜ›ÜÜÈ™\›Ë][YBœ™[™\ˆœ˜[Y\ËÛÈ[˜Ø\Y™[™\š[™ÈØ[ˆİÙ\ˆ›Û™[ÈİÈ\ÜØYÙ\Ë‚‚˜ÌK[˜]]™KZ›ŞK\ÛZ[™ÚZÙKœ]Ú\\ÜÙ\ÈHYØXŞHÛ™K\ÛİÛ\ØX›KÙ[˜X›Bš[™ÚZÙHÛˆ˜]]™HZ[Ë™]™[[™ÈH›ÜY[˜X›H™\]Y\İœ›ÛH\›X[™[Hİ\™\ÜÚ[™Â˜ÛÛ›Û\ˆ™XYÈY\ˆZ\ÜÚ[Û‹\Ø]™KÜİ]\È˜[œÚ][ÛœËˆ›Û‹[˜]]™H™Z]š[Üˆ\È[˜Ú[™ÙY‚‚˜Ì‹[X[X[\™[ØYœ]ÚYÈHÜ[İÛ™Y™[ØYXİ[ÛˆÚ]İ]Ú[™Ú[™ÈH™]Z[š[\˜Xİ[ÛˆÚYHY™™XİËˆHYXØ]Y™[ØYš[™[™ÈØ[ˆ™\XÙHH\ÙH]Û‰ÜÈ™[ØY™İX›H]HÚ[HX]š[™ÈÛÜˆ[™Øš™Xİ[\˜Xİ[ÛˆÛˆ]ÈÜšYÚ[˜[Ø[]‚‚˜ÌËYİX\™Yİ[‹\Ù[˜]]™K[^[İ]œ]ÚÙY\ÈH™]Z[İX\™Yš\™HÛİ[™ÛÚİ\[˜Ú[™ÙYÛ‚ŒÌ‹Xš]Z[ÈÚ[H˜]]™HZ[È™XYH\]Z\YÙX\ÛˆQ›İYÚ˜ÙX\Û“Øš”™XÛÜ™ÙX\Û›[XˆHÛÚ”™XÛÜ™˜XİØ]XÚË˜]XÚ×Ú][Xİ™\›^H›ÈÛ™Ù\‚˜Y™\ÜÙ\È]]HY\ˆ˜]]™HÚ[\ˆÚY[š[™Ë‚‚˜Í\İ\™˜XÙL‹\™[[İK[Z[™K[˜]]™K[^[İ]œ]Ú™\Ù\™\ÈH™]Z[›İÛ‹Z][HÛÚİ\İ]ÚYB›˜]]™HZ[ËˆÛˆ˜]]™H]KY[™X[ˆÜİÈ]™X]È]™H\H\È“ÔÕTWÕÑPTÓ˜[™˜ÛÛ\\™\ÈÙX\Û“Øš”™XÛÜ™ÙX\Û›[X\™XİKˆH™]Z[Ù^T™XÛÜ™šÙ^RQİ™\›^H›ÈÛ™Ù\‚˜[X\Ù\ÈÙX\Û›[XY\ˆÑWÔÕP•ÓÔ‘Ø™[Ü™\œÈHÙX\ÛˆİX™šY[ÎÈ][X\Ù\È[Y\˜ÚXÚ˜Ø[ˆXZÙH[ˆÜ™[˜\HÜ™[˜YH˜[Ù[HØ]\ÙHİ\™˜XÙH‰ÜÈ™[[İK[Z[™H˜Z[\™HÚXÚË‚‚˜ÍKYİ[˜˜\œ™[X]]Ü™YXØY[˜ÙKœ]ÚÙY\ÈH[›È™[™\š[™È]™\H™\Ù[][Ûˆœ˜[YH]™Ø]\È]Èœ˜[YKXÛİ[Y[İ™[Y[›Û™[š[X][ÛˆXÚÜËÛİ[\œÈ[™›ÛÙYœ˜[YHXÛÙ[™ÈÛ‚˜[“Z[\Ô™]š[İ\ĞÛİ[\˜ˆ]\ÈH^\İ[™ÈÌˆ•ĞÈÈHˆS]]Ü™YÛØÚÈ\š]™Y™œ›ÛHHšY[ËYšY[Ûİ[\‹ÛÈHŒˆ˜]]™H™\Ù[][Ûˆ›ÈÛ™Ù\ˆY˜[˜Ù\ÈH[›ÈÚXÙB˜\ÈÙ[ˆÚ[HH›Û‹[˜]]™H]ÛÛ[Y\ÈÈY˜[˜ÙHÛ˜ÙH\ˆ™]Z[™[™\‹‚‚˜Í‹][šË[[İ[YXYÛ›ÜİXËœ]ÚYÈÑU—ÕS’ÑP•QÏLXØœÙ\˜Xš[]H]H›İ\ˆ[šË[[İ[š[™Ù™ˆ›İ[™\šY\È[ˆ›Û™šY]Ì‹˜ØˆÛÛ\Ú[Ûˆ\ØÛİ™\Kİ]\‹Ú[›™\ˆ[šÈÛYÛÛˆİ]H[™˜Û[X‹ZZYÚ›ÙÜ™\ÜÚ[Û‹‹X]Ûˆ[YÚXš[]K[™İXØÙ\ÜÙ[[Kˆ]š[ÈÛ›HÛˆİ]B˜Ú[™Ù\ÈÜˆ^XÚ]ˆ\È[™Ù\È›İ[\ˆÛÛ\Ú[Û‹Û[XˆÜˆ[HXÚ\Ú[ÛœËˆY\ˆHš\œİ™˜Z[Y[Ø^H˜XÙH›İ™Y]\ØÛİ™\H™]™\ˆÙ]È×ÕÛÜ›[šÔ›ÜH›Ø™HØ\È^[™YÂœ™XÛÜ™İ[”Ø]™YÛÛÜÜÑ]XØ[™Y]H˜[œÚ][ÛœÈ[™H‹]\Û˜\ÚİÙˆH™X\™\İXİ]™B[šÉÜÈÙ[™\šXÈÛÛ\Ú[Ûˆ[›ÛÛH™YÚ\İ˜][Ûˆ[™Øš™Xİİ]Kˆ\ÈÙ\\˜]\ÈZ\ÜÚ[™ÈÛÛ\Ú[Û‚™Ù[ÛY]Hœ›ÛH›ÛÛKÙ\ØÛİ™\HİÛ™\œÚ\™Y›Ü™H[H[šÈ™Z]š[Üˆ\ÈÚ[™ÙYˆH™^[Ø^H˜XÙBœ›İ™YHÙ[™\šXÈ[[™\ØÛİ™\H\™HX[H]Hİ]\‹XÛÛXİØ]H™]™\ˆİXØÙYYËˆBœ›Ø™H\™Y›Ü™H[ÛÈ™XÛÜ™ÈHXİX[^Y\ˆ˜Y]\ËZ[š[][HYÙH\İ[˜ÙKÚ[Ü˜Y]\ÈÛÛXİœ™\İ[È[™›İ\ˆ[šË\ÛYÛÛˆ™\XÙ\ÈÛˆİ]\‹XÛÛXİ˜[œÚ][ÛœË[İÚ[™È[ˆ^Xİ\˜Y]\Â˜›İ[™\H˜Z[\™HÈ™H›İ™[ˆÚ]İ]Ú[™Ú[™ÈØ[Y\^K‚‚˜ÍËYš[K\Ù[XİZ]›Ş[˜]]™K[^[İ]œ]Úš^\ÈH›İ\‹Y›Û\ˆš[K\Ù[Xİ]]\İÛˆ˜]]™B˜Z[ËˆH™]Z[Ûİ\˜ÙH\ÜÙY›İ\ˆØØ[\ˆİXÚÈ›Ø]ÈÈ›Ú™Xİ™XİÛÜ›™\œÕÌ‘

X\ÈİYÚ^HÙ\™HÛÛÜ™™İXİ\™\ÎÈĞĞÈXYÛ›ÜÙ\È[›İ\ˆ\™İ[Y[È\È[˜ÛÛ\]X›HÚ[\ˆ\\È[™›˜]]™HÛÛ\[\œÈÈ›İİX\˜[YHHØØ[\ˆØØ[È\™HY˜XÙ[ÜˆÜ™\™YˆH˜]]™H]›İÂ\Ù\È^XÚ]ÛÛÜ™™[œ]Ûİ]]Z\œÈ[™ÛÜY\ÈH›Ú™XİY˜[Y\È[È™Xİ˜›ŞÚ[BH›Û‹[˜]]™H]™[XZ[œÈ]KY›Ü‹X]HİXİ\˜[H\]Z]˜[[ÈHÜšYÚ[˜[XÛÛ\ÙÚXË‚‚˜Î][šËXÛÛXİ[˜]]™K]Û\˜[˜ÙKœ]Ú\ÈHš\œİ™Z]š[Üˆ™\Z\ˆ›ÙXÙYœ›ÛHH[Ø^B[šÈ[œİ[Y[][Û‹ˆ[[YH]šY[˜ÙHÚİÙYÜ™[˜\HÛÛ\Ú[Ûˆ™\X]YH\ØÛİ™\š[™ÈH[šÂÚ[H›Û™Ù]YÛ›HH™]Èİ\Ø[™È™^[Û™\È›ÛZ[˜[Ì][š]˜Y]\È
›İYÚHÌŒÂŒÌŒM[š]Èœ›ÛHH[
KˆÚ›Øš•\İÚ[ÛYÛÛÛÛ\Ú[ÛŠ
X\Ù\ÈİšXİ˜Y]\Ø\İËÛÂHÜXÚX[[šÈİ]\‹XÛÛXİØ]Hİ^YY˜[ÙH›Ü™]™\‹ˆ˜]]™HZ[È›İÈYÛ›HŒHÛÜ›[š]ÈÈH˜Y]\È\ÜÙYH][šË\ÜXÚYšXÈØ]KˆHÚ\™YÛYÛÛˆ[\ˆ\È[˜Ú[™ÙYB››Û‹[˜]]™H]\È[˜Ú[™ÙY[™HXYÛ›ÜİXÈš[È›İ›ÛZ[˜[[™ÛÛXİ˜YZK‚‚˜ÎK][šËZ[›™\‹YXYÛ›ÜİXËœ]Ú™XÛÜ™ÈH[›™\ˆ[šÈ[[™H™\]Y\İY™\œİ\ÈXØÙ\YÜ[Ù‹XÛ[Xˆ[İ™[Y[ˆH[Ø^H˜XÙHÚİÙY›Û™™XXÚ[™ÈHÛ[Xˆ\™Ù]Ú[H[Ø\™[İ[Û‚Ø\È™YXÙYÈ[[Üİ™\›ËX]š[™ÈH[›™\‹ØØ[‹Y[\ˆØ]H˜[ÙK‚‚˜][šË[˜]]™K[Øš™Xİ\İ]KX[X\Ëœ]Ú™\Z\œÈHØ]\ÙHY[YšYYœ›ÛH]˜XÙKˆH›Øš™XİÛÛ\Ú[ÛˆÙÙÛHÜš]\È]Hˆ›İYÚÚ”™XÛÜ™˜XØİ\˜XŞ\˜][™ØÚXÚ[X\Ù\Â˜Øš™Xİ™XÛÜ™œİ]XÛˆHÜšYÚ[˜[šYËY[™X[ˆ^[İ]ˆ˜]]™HÑWÔÕP•ÓÔ‘ØXÙ\Èİ]X]˜]HH[™^˜\ØØ[X]]H‹ÛÈHYØXŞH[X\ÈÚ[™ÙYØØ[H]H[™YÛÛ\Ú[Û‚™[˜X›Yˆ˜]]™HZ[È›İÈÙÙÛH“ÔÕUWÌŒ›İYÚH˜[YYØš™Xİ™XÛÜ™œİ]XšY[Â››Û‹[˜]]™HZ[È™]Z[ˆHÜšYÚ[˜[[X\Ë‚‚˜KX\˜Ú]™\Ë\İ[]Ø]ÚÙËZÛÚÜËœ]ÚYÈ˜]]™K[Û›H[œİ[Y[][ÛˆÛÚÜÈ]HXZ[‹[ÛÜœ\ÙH›İ[™\šY\È\È™ÓØY›ÛÛS[Ù[]J
X[™İ[•\İ[™U[›ØœİXİY

XˆH˜XÚÚ[™ÂœÜ[^Y\ˆØ]ÚÙÈ\ÈÜZ[ˆÚ]ÑU—ÔÕSPÑOLX™XYÈÛ›HXYÛ›ÜİXË[İÛ™YÑ]ÛZXÜÂ™œ›ÛH]ÈÛÜšÙ\ˆ™XY[™™\ÜÈHİ[HXXÜ›È\ÙH\ÈH\İ›ÛÛK[ØYÓÔÈİÜİˆ]\Â™XYÛ›ÜİXÈÛ›Nˆ›È›ÛÛK[ØYYÙ]ÛÛ\Ú[ÛˆXÚ\Ú[Û‹RH™Z]š[Ü‹™[™\™\ˆ™Z]š[ÜˆÜ‚››Û‹[˜]]™H]\ÈÚ[™ÙY‚‚˜‹Yİ[˜˜\œ™[[˜]]™KXØY[˜ÙK\Ü]œ]ÚÛÜœ™XİÈHİ™\‹Xœ›ØYÍH[Z[™È™\Z\ˆY\‚š[X[ˆ˜[Y][ÛˆÚİÙYHÛÛXš[™YÙ\]Y[˜ÙHY™XÛÛYHÛÈÛİÈ[™H™Yœ™\ÚY›ÛÙ[XYÙBœÚ[[Y\™YˆHÛÈŞ[\Û\ÈÚ\™HHÛÛ˜Ü™]HØ]\ÙNˆ›ÛÙœ˜[Y\È\™HİÜ™Y[ˆÛÛ[‘^YIÜÈÛÂ˜[\›˜][™È[ˆY™™\œË[™™]Z[XÛÙ\ÈH™]È›ÛÙœ˜[YH]™\HÙXÛÛ™™[™\‹^XİHÚ[ˆBœØ[YH\™[˜H™]\›œËˆØ][™È]İ]HXXÚ[™HÈ[‹\˜]HXYHHXÛÙHØØİ\ˆ]™\H›İ\œ™\Ù[][Ûˆœ˜[YK[İÚ[™ÈÛ™H™[™\ˆÈ™]\ÙHHİÛš[™È\™[˜HÚ]İ]™Yœ™\Ú[™ÈH[XYÙK‚Œˆ\™Y›Ü™H™\İÜ™\ÈH]Hİ]HXXÚ[™H[™›ÛÙXÛÙ\ˆÈZ\ˆÜšYÚ[˜[]™\K\™[™\‚˜ØY[˜ÙKˆHÜšYÚ[˜[˜]]™H•ĞÈÜYYÛÛ\Z[\ÈÙ\\ÛÛ]YÈ›Û™	ÜÈ[Ù[]H\Ú[™Â›Û™HİX—ÑĞSQWÍÑŒÑŒÌ[š[X][ÛˆXÚÈ\ˆ˜]]™H™[™\È™]Z[•ĞÈ™[XZ[œÈÛÈ[™S™[XZ[œÂ›Û™Kˆ›È[Z[™ÈÛÛœİ[ÈÜˆ[˜[ZXË]^\™HØXÚH™Z]š[Üˆ\™HÚ[™ÙY‚‚˜ËYİ[˜˜\œ™[[˜]]™KX›Û™\™\Ş[˜Ëœ]Ú›ÛİÜÈHš\œİˆ[X[ˆ\İˆH›ÛÙÚ[[Y\‚Ø\ÈÛÛ™H[™Hİ™\˜[Ù\]Y[˜ÙHÜYYØ\È]XÚÛÜÙ\‹]Hİ[˜˜\œ™[š\ÚX›H[İ™YZXYÙ‚›Û™ˆˆY™\İÜ™Y]H[İ™[Y[È]™\H™[™\ˆÚ[H[[[Û˜[HX]š[™È˜]]™H•ĞÂ›Û™]Û™H[Ù[XÚÈ\ˆ™[™\‹ÛÈZ\ˆ™[]]™HØY[˜ÙHYÚ[™ÙYH^XİHŒKˆÂœ™\İÜ™\ÈHÜšYÚ[˜[•ĞÈİX—ÑĞSQWÍÑŒÑŒÌ
‹‹‹‹‹‹ŠXÛİ[[™X]™\ÈS]Û™Kˆ]Ù\Â››İÚ[™ÙHİ[˜˜\œ™[[[ÙH\˜][Û‹]HÛÛœİ[Ë›ÛÙXÛÙHØY[˜ÙKÜˆH[˜[ZXË]^\™Bœ™Yœ™\Ú‚‚˜Y\İ][YYZ[™KYXYÛ›ÜİXËœ]Ú\ÈXYÛ›ÜİXÈÛ›KˆÚ]ÑU—ÓĞ’—ÑP•QÏLX\İ›Øš™Xİ]™HH™\ÜÈ›İ[™YÙ[X[XÈÛ˜\ÚİÈ›ÜˆYÜÈKˆ[™Ë[˜ÛY[™È˜]ÈYÈ[šØYÙKHØš™Xİ]™HÛÚİ\™\İ[›Üİ\HY[]K[[YH›YÜË\İXİ[Ûˆİ]H[™[XYÙKˆÚ]˜ÑU—ÕSQQRS‘WÑP•QÏLX[YYZ[™\È™\Ü[Y\ˆ\ÜÚYÛ›Y[›Ú™Xİ[H[š]X[^˜][Û‹š\œİÙX\Û‹]XÚÈİ]KÛØ\œÙHÛİ[İÛˆ˜[œÚ][ÛœÈ[™H™X\‹^™\›ËÙ^ÜÚ[Ûˆ›İ[™\KˆHZ[™BXÚÈ›Ø™H[ÛÈ™XÛÙÛš^™\ÈH[YY[Z[™H[Ù[ÛÈHÛÜœ\YÙX\Û›[XØ[››İXZÙHB™XYÛ›ÜİXÈ\Ø\X\‹ˆ™Z]\ˆØ]HÚ[™Ù\ÈØš™Xİ]™HXÚ\Ú[ÛœË[Y\ˆ˜[Y\ËØš™Xİİ]HÜ‚™^ÜÚ[Ûˆ™Z]š[Ü‹‚‚˜K\›Ş[Z]KX›Ş\İ[YXYÛ›ÜİXËœ]Ú™XÛÜ™È›Ş[Z]K[Z[™H^ÜÚ[Ûˆ[KÜ™]\›ˆ[™HÚ\™Y›ÛÛK\›Ü\İÙ[™\˜][Ûˆ\›İ[™^ÜÚ[Ûˆ[XYÙKˆ]™\ÜÈYˆH™\İY˜›ÛÛQÙ]›ÜÊ
XØ[]]]\ÈH\İÚ[HHİ]\ˆ^ÜÚ[ÛˆØØ[ˆ\ÈÛÛœİ[Z[™È]ˆB™XYÛ›ÜİXÈ\È˜]]™K[Û›H[™Ù\È›İÚ[™ÙH^ÜÚ[ÛˆÜˆ[XYÙH™Z]š[Ü‹‚‚˜‹\İ[‹\›ÛÛKXY™™\‹[˜]]™K]\›Z[˜]Ü‹œ]ÚÚ]™\ÈH˜]]™B˜İ[•\İ[™U[›ØœİXİY

X›ÛÛH\œ˜^HÛ™H^˜H[[Y[ˆH[HØ[ÈØ[ˆ›ÙXÙHŒ›ÛÛB’QË[™H›ÛİÚ[™È›ÛÛQÙ]›ÜÊ
XØ[™\]Z\™\ÈHLX\›Z[˜]Ü‹ÛÈH˜]]™H\œ˜^Bš\ÈŒH[šY\ÈÚ[HHÜšYÚ[˜[™]Z[[^[İ]XÛ\˜][Ûˆ\È™\Ù\™Yİ]ÚYB˜ÑWÔÔ•ÓUU‘X‚‚˜Ë\İ[‹[ÜË\İXœ\ÙKYXYÛ›ÜİXËœ]ÚXZÙ\ÈHİ[Ø]ÚÙÈ\İ[™İZ\ÚH[š]X[[BØ[Ë›ÛÛK\›ÜÛÚİ\ÛÛ\Ú[Û‹X›İ[™ÈÛÚİ\ÛYÛÛˆYÙHÛÜš[˜[[HØ[È[™™]\›‹‚’][ÛÈ™\ÜÈÛÛ\Ú[ÛˆYÙHÛİ[Èİ]ÚYHHİ\ÜY‹˜[™ÙH™Y›Ü™HHÜšYÚ[˜[ÛÙB˜ÛÛ[Y\ËÛÈHÛÜœ\YÛİ[™[XZ[œÈØœÙ\˜X›HÚ]İ]X\ÚÚ[™ÈH˜Z[\™K‚‚˜\İ[‹[ÜËYY\YXYÛ›ÜİXËœ]Ú˜\œ›İÜÈH™[XZ[š[™È›[™ÜİÈÚ]İ]Y[™ÈB˜™Z]š[Ü˜[İX\™ˆ\š[™ÈHÕS‹[İÛ™Y›ÛÛQÙ]›ÜÊ
XØ[]X›\Ú\È›ÛÛKØÚ[šÈ˜]™\œØ[œ›ÙÜ™\ÜÈ[™™\ÜÈ[\ÜÜÚX›H›ÛÛHQËÚ[šÈ[™XÙ\Ë›Ü[™XÙ\ËÚ[šËXÚZ[ˆ[™İ[™›ÛÚİ\XY™™\ˆØ\XÚ]KˆHÕSˆÛÛœİ[Y\ˆÙ\\˜][HX›\Ú\È›Ü[\İÛİÚ[™^›ÙÜ™\ÜÈ[™HÛÈİ[‘Ù]ÜÚ][Û–U˜[YJ
X\Ù\ËˆÛÛ\[K][YH\ÜÙ\[ÛœÈ›İ™HH˜]]™B˜ÛÛ\Ú[Û—Ù]XP’H\ÈÈ]\ÈÚ]YÙ\ËÜÛYÛÛ‹İÜØ›İÛX]˜ÌÌÌ[™\™Y›Ü™Hİ[š]È›İYØXŞHLX]H[ØØ][ÛœËˆH\™XÛÙY˜[ØØ][ÛˆÚ^™H\È[[[Û˜[H›İÚ[™ÙY™XØ]\ÙH]\È›İ[™\œÚ^™YÛˆH˜]]™HP’K‚‚˜K[˜]]™KXÛÛ\Ú[Û‹\ÛYÛÛ‹Y^[œ]Ú™\Z\œÈHÙ\\˜]H]\›Z[š\İXÈ˜]]™HÈPˆY™Xİ™›İ[™Ú[H]Y][™ÈHØ[YHÛÛ\Ú[Ûˆ]ˆÛÛ\Ú[Û—Ù]X\ÈZYÚÛÛÜ™™[Ú[Ë˜]HYØXŞHT\È^ÜÙH]İÜ˜YÙH\È™Xİˆ
˜ÚÜÙHXÛ\™YÚ[Ö×XY[X™\ˆ\ÈÛ›B™›İ\ˆ[[Y[Ëˆ˜]]™H[›Ú™Xİ[ÛˆØ[ˆYÚ][X][H›ÙXÙHKNYÙ\ÎÈİšXİP”Ø[ˆ™\›ÙXÙ\Â˜[ˆ[™^Mİ][Ù‹X›İ[™ÈXØÙ\ÜÈÚ]HÚ^YYÙH[]™[ˆİYÚH˜XÚÚ[™È[ØØ][Ûˆ\È›ÛÛK‚“˜]]™HZ[È›İÈ[™^ÛÛ\Ú[ÛˆÛYÛÛœÈ›İYÚHXİX[ÛÛÜ™™˜XÚÚ[™È\œ˜^HÚ[B››Û‹[˜]]™HZ[ÈÙY\HÜšYÚ[˜[™Xİ‹œÚ[Ö×X^™\ÜÚ[Û‹ˆH[Z[\‹ÕSˆÔËÂ›Û[YHØ[Ù\œËÙ[™\šXÈÛYÛÛˆÛÛ\Ú[Ûˆ[\œË˜^HÛÛ\Ú[Ûˆ][™[šÈXYÛ›ÜİXÈYÙBØ[È[\ÙHHXØÙ\ÜÛÜ‹ˆ\ÈÙ\È›İØ\YÙ\ÈÜˆÚ[™ÙH˜[YÛYÛÛˆÙ[ÛY]NÈ]™[[İ™\ÂH˜[ÙH›İ\‹\Ú[È\H›İ[™‚‚˜M[˜]]™K[Y\‹[Øİ\Ë\™XÛÜ™œ]Ú™\Z\œÈH˜]]™HİXÚË[^[İ]Y™Xİ[ˆ[İ™P›Û™

X	ÜÂ›Y\‹ÜÜXÚX[TÕSˆ]Y\šY\Ëˆ™]Z[ÛÙH\Ù\È[İ™WØ›Û™İ[\ÜİXİHÛË]ÛÜ™XÙZÛ\ˆÚÜÙB]\ÈÛİ™\ˆHÜšYÚ[˜[Ì‹Xš]›ÛÛ\ØÚ[\ˆ[™Ûİ[šY[ˆÛˆHXš]˜]]™HZ[BœÚ[\ˆÚY[œÈÈ]\Ë[İš[™Èİ[™[SØİ\ĞØ[˜XÚÔ™XÛÜ™˜Ûİ[ÈÙ™œÙ]ÈHÛX]BœXÙZÛ\ˆ\™Y›Ü™H[™È™Y›Ü™HHšY[]İ[‘Ù]Øİ\ĞÛİ[

X™XYËÚ[B˜İ[•[Q\İ[˜ÙT™[]Y

X[ÛÈ[š]X[\Ù\È]\ˆ™XÛÜ™šY[È™^[Û™]ˆ˜]]™HZ[È›İÈ\ÙBH™X[ÚY[™YØ[˜XÚÈ™XÛÜ™Ú[H›Û‹[˜]]™HZ[È™]Z[ˆH™]Z[XÙZÛ\‹ÜİXÚÈÚ\K‚‚˜NKYİ[˜˜\œ™[X›Û™\ÜYYXØ[Xœ˜][Û‹œ]ÚYÈ˜]]™K[Û›HÛÛ›ÛÙˆHš[˜[™[XZ[š[™Â™İ[˜˜\œ™[Û\Ú][KˆÑU—ÑÕST”‘SĞ“Ó‘ÔÔQQO›Ø]˜Ú[™Ù\ÈH[š]X[˜›Û™Ù^YWİØ[ØÜÙH˜]H›Ü›X[H\ÜÙY\ÈLY˜ÈHY˜][™[XZ[œÈLY˜››Û‹[˜]]™HZ[È™]Z[ˆH]\˜[™]Z[Ø[H•ĞËÔS[Ù[]XÚÈØY[˜ÙH\È[˜Ú[™ÙY˜[™İ[˜˜\œ™[[İ™[Y[Ø›ÛÙ[Z[™È\™H[İXÚYˆ™XØ]\ÙHHØ[È[š[X][Ûˆ[ÛÈØ\œšY\È›Ûİ˜[œÛ][Û‹˜]]™HØ[Xœ˜][Ûˆ[™\œÙ[HØØ[\È]˜[œÛ][ÛˆHLHÈÜYY[™™\İÜ™\ÂœØØ[HKŒ™Y›Ü™HH\›‹Ùš\™H[š[X][Û‹™\Ù\š[™ÈH[™XYK]˜[Y]Y›Û™Ø˜\œ™[[YÛ›Y[‚•HØ[YH]Ú[ÛÈYÈÑU—ÑÕST”‘SÔÑTUQSÑWÔÔQQÈİ[˜˜\œ™[ÜÙ\]Y[˜ÙWÜÜYY›ÜˆBœ™K\Úİ]]Ü™YØY[˜ÙKˆ]\Ù\ÈHœ˜Xİ[Û˜[XØİ[][]Ü‹ÛÈH˜[YHİXÚ\ÈLŒÌ˜Y˜[˜Ù\ÈX›İ]H]]Ü™Yİ\È›Üˆ]™\HLÈÜİ™[™\œÈÚ[Hİ[™[™\š[™È]™\Hœ˜[YK‚“Û˜ÙH›Û™š\™\ËØY[˜ÙH™]\›œÈÈÛ™H]]Ü™Yİ\\ˆ™[™\ˆÛÈH[™XYKXXØÙ\Y›ÛÙ[Z[™È\È›İ\İ\˜™Yˆ[X[ˆÛÛ\\š\ÛÛˆYØZ[œİHİ\YY•ĞÈ™Y™\™[˜ÙHXØÙ\Y˜LŒÌ\ÈÛÜœ™XİÛÈ]\È›İÈH˜]]™HY˜][Ú[H™[XZ[š[™È\Ù\‹XY\İX›K‚•HÜÛÛ™šYÈ^Y\ˆ^ÜÙ\ÈHØ[YH˜[YH\œÚ\İ[H\Èİ[˜˜\œ™[Ø›Û™ÜÜYY[‚˜ÛÛ[™^YKš[šX
Ú]YØXŞHÛÛ[™^YK˜Ù™Øİ[XØÙ\Y
Kˆ˜[Y\Èİ]ÚYHŒK‹ŒKLÜ‚›X[›Ü›YY˜[Y\È˜[˜XÚÈÈLK‚