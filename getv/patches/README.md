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
| `0058-bunker2-cctv-objective-diagnostic.patch` | 1 file | `objective_status.c`: opt-in read-only Bunker 2 objective #2 trace for CCTV tags 28–33, reporting tag/object/prop health and destruction state without changing objective evaluation | after `0057` |
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
git apply ../../getv/patches/0032-manual-reload.patch
git apply ../../getv/patches/0033-guard-gun-sfx-native-layout.patch
git apply ../../getv/patches/0034-surface2-remote-mine-native-layout.patch
git apply ../../getv/patches/0035-gunbarrel-authored-cadence.patch
git apply ../../getv/patches/0036-tank-mount-diagnostic.patch
git apply ../../getv/patches/0037-file-select-hitbox-native-layout.patch
git apply ../../getv/patches/0038-tank-contact-native-tolerance.patch
git apply ../../getv/patches/0039-tank-inner-diagnostic.patch
git apply ../../getv/patches/0040-tank-native-object-state-alias.patch
git apply ../../getv/patches/0041-archives-stall-watchdog-hooks.patch
git apply ../../getv/patches/0042-gunbarrel-native-cadence-split.patch
git apply ../../getv/patches/0043-gunbarrel-native-bond-resync.patch
git apply ../../getv/patches/0044-depot-timedmine-diagnostic.patch
# ... run the asset pipeline (docs/SETUP.md 3.5) and the namespacing pass (3.6) ...
python3 ../../tools/transform_rarewarelogo.py
git apply ../../getv/patches/0002-assets.patch
```

`tools/install.sh` and `tools/install.ps1` do all of this for you.

## Refresh (before any commit that touches the decomp)

> **CHECK THAT `vendor/ge-decomp` IS A GIT REPOSITORY FIRST.**
>
> `git diff` in a directory that is not a repo prints **nothing and exits 0**. Redirecting that
> into `0001-source.patch` replaces 1.4 MB across 140 files with an empty file, and since
> `vendor/` is gitignored there is no other copy. The refresh command below is the single most
> destructive line in this repository.
>
> On at least one machine here the decomp is a plain directory rather than a clone -- the sources
> are present and patched, but there is no `.git`. Verify before running anything:
>
> ```bash
> cd vendor/ge-decomp
> git rev-parse --is-inside-work-tree || echo "NOT A REPO -- do not regenerate"
> ```
>
> Without a repo, add changes as a **new numbered patch** instead, or generate one against a
> saved copy of the file:
>
> ```bash
> cp src/game/foo.c /tmp/foo.c.orig     # before editing
> diff -u /tmp/foo.c.orig src/game/foo.c > /tmp/raw.patch
> # then rewrite the two header lines to a/src/game/foo.c and b/src/game/foo.c
> ```
>
> Whichever route, verify it round-trips onto a pristine copy before trusting it:
>
> ```bash
> patch -p1 --dry-run < ../../getv/patches/000N-thing.patch
> ```

Each patch must be regenerated over its own paths. A bare `git diff` would sweep the entire
extracted asset tree into `0001` - hundreds of megabytes of ROM-derived data.

```bash
cd vendor/ge-decomp
git diff -- src include tools > ../../getv/patches/0001-source.patch
git diff -- assets/animationtable_data.h assets/font_dl.c \
            assets/font/fontBankGothic.c assets/font/fontZurichBold.c \
            assets/obseg/setup/e/UsetuplenZ.c assets/obseg/setup/j/UsetuplenZ.c \
            assets/obseg/setup/u/UsetuplenZ.c > ../../getv/patches/0002-assets.patch
```

Never add `assets/rarewarelogo.c` to this command. Its required declaration and byte-order change
is performed only on the contributor's ignored, locally extracted file by
`tools/transform_rarewarelogo.py`.

**Run `bash tools/check_patches.sh` after regenerating anything.** It clones a pristine decomp
and applies every patch to that, which is the one thing a working tree cannot tell you: a tree
that already has the changes will accept a patch that no fresh clone would. That check is the
difference between finding this now and a stranger finding it on their first install. It also
verifies each patch is named by all three setup scripts.

Regenerating by hand is how `0006` was corrupted: one context line was truncated, `git apply`
rejected it, and the tree it was meant to fix would not link. That is issue #5. Round-trip every
regenerated patch onto a pristine copy before committing it.

## Why 0002 is separate

The seven files in `0002` do not exist in a fresh decomp. They are produced from the ROM by the
pipeline in `docs/SETUP.md` section 3.5. Applying it early fails, and applying it before
`uniquify_asset_symbols.py` runs gets the font symbols double-prefixed.

`0001` also creates five files that have no upstream counterpart, one of which,
`tools/gen_propdef_layout.py`, the pipeline itself calls.

## What is deliberately not in here

Generated data - the audio segment, the obseg blobs, the animation blobs, the images segment,
the per-model `Model.c` files. They are large, derived from the ROM, and reproducible from
`tools/` and `scripts/`; see `docs/SETUP.md` section 3.5. **Never commit ROM-derived data.**

`0030-autocrouch-render-ticks.patch` preserves auto-crouch intent across zero-time
render frames, so uncapped rendering can lower Bond into low passages.

`0031-native-joy-poll-handshake.patch` bypasses the legacy one-slot poll disable/enable
handshake on native builds, preventing a dropped enable request from permanently suppressing
controller reads after mission-save/status transitions. Non-native behavior is unchanged.

`0032-manual-reload.patch` adds the port-owned reload action without changing the retail
interaction side effects. A dedicated reload binding can replace the use button's reload
double duty while leaving door and object interaction on its original call path.

`0033-guard-gun-sfx-native-layout.patch` keeps the retail guard-fire sound lookup unchanged on
32-bit builds while native builds read the equipped weapon ID through
`WeaponObjRecord.weaponnum`. The old `ChrRecord.act_attack.attack_item` overlay no longer
addresses that byte after native pointer widening.

`0034-surface2-remote-mine-native-layout.patch` preserves the retail thrown-item lookup outside
native builds. On native little-endian hosts it treats live type 4 as `PROP_TYPE_WEAPON` and
compares `WeaponObjRecord.weaponnum` directly. The retail `KeyRecord.keyID` overlay no longer
aliases `weaponnum` after `GE_SUBWORD3` reorders the weapon subfields; it aliases `timer`, which
can make an ordinary grenade falsely satisfy Surface 2's remote-mine failure check.

`0035-gunbarrel-authored-cadence.patch` keeps the intro rendering every presentation frame but
gates its frame-counted movement, Bond animation ticks, counters and blood-frame decoding on
`halfMinusPreviousCounter`. That is the existing 30 Hz NTSC / 25 Hz PAL authored clock derived
from the video-field counter, so a 60 Hz native presentation no longer advances the intro twice
as often while the non-native path continues to advance once per retail render.

`0036-tank-mount-diagnostic.patch` adds `GETV_TANKDEBUG=1` observability at the four tank-mount
handoff boundaries in `bondview2.c`: collision discovery, outer/inner tank polygon state and
climb-height progression, B-button eligibility, and successful entry. It prints only on state
changes or explicit B taps and does not alter collision, climb or entry decisions. After the first
failed Runway trace proved that discovery never sets `g_WorldTankProp`, the probe was extended to
record `stanSavedColl_posData` candidate transitions and a B-tap snapshot of the nearest active
tank's generic collision hull, room registration and object state. This separates missing collision
geometry from room/discovery ownership before any tank behavior is changed. The next Runway trace
proved the generic hull and discovery are healthy but the outer-contact gate never succeeds. The
probe therefore also records the actual player radius, minimum edge distance, point/radius contact
results and four tank-polygon vertices on outer-contact transitions, allowing an exact-radius
boundary failure to be proven without changing gameplay.

`0037-file-select-hitbox-native-layout.patch` fixes the four-folder file-select hit-test on native
builds. The retail source passed four scalar stack floats to `projectRectCornersTo2D()` as though
they were `coord2d` structures; GCC diagnoses all four arguments as incompatible pointer types and
native compilers do not guarantee the scalar locals are adjacent or ordered. The native path now
uses explicit `coord2d` input/output pairs and copies the projected values into `rectbbox`, while
the non-native path remains byte-for-byte structurally equivalent to the original decomp logic.

`0038-tank-contact-native-tolerance.patch` is the first behavior repair produced from the Runway
tank instrumentation. Runtime evidence showed ordinary collision repeatedly discovering the tank
while Bond settled only a few thousandths beyond his nominal 30-unit radius (roughly 30.0004 to
30.0054 units from the hull). `chrobjTestPointPolygonCollision()` uses strict `< radius` tests, so
the special tank outer-contact gate stayed false forever. Native builds now add only 0.01 world
units to the radius passed by that tank-specific gate. The shared polygon helper is unchanged, the
non-native path is unchanged, and the diagnostic prints both nominal and contact radii.

`0039-tank-inner-diagnostic.patch` records the inner tank hull and the requested versus accepted
top-of-climb movement. The Runway trace showed Bond reaching the climb target while inward motion
was reduced to almost zero, leaving the inner/can-enter gate false.

`0040-tank-native-object-state-alias.patch` repairs the cause identified from that trace. The N64
object collision toggle writes byte 2 through `ChrRecord.accuracyrating`, which aliases
`ObjectRecord.state` on the original big-endian layout. Native `GE_SUBWORD3` places `state` at
byte 1 and `extrascale` at byte 2, so the legacy alias changed scale data and left collision
enabled. Native builds now toggle `PROPSTATE_20` through the named `ObjectRecord.state` field;
non-native builds retain the original alias.

`0041-archives-stall-watchdog-hooks.patch` adds native-only instrumentation hooks at the main-loop
phase boundaries plus `bgLoadRoomModelData()` and `stanTestLineUnobstructed()`. The backing
port-layer watchdog is opt-in with `GETV_STALLTRACE=1`, reads only diagnostic-owned SDL atomics
from its worker thread, and reports a stale macro phase plus the last room-load/LOS hotspot. It is
diagnostic only: no room-load budget, collision decision, AI behavior, renderer behavior or
non-native path is changed.

`0042-gunbarrel-native-cadence-split.patch` corrects the over-broad 0035 timing repair after
human validation showed the combined sequence had become too slow and the refreshed blood image
shimmered. The two symptoms share a concrete cause: blood frames are stored in GoldenEye's two
alternating dyn buffers, and retail decodes a new blood frame every second render, exactly when the
same arena returns. Gating that state machine to half-rate made the decode occur every fourth
presentation frame, allowing one render to reuse the owning arena without refreshing the image.
0042 therefore restores the title state machine and blood decoder to their original every-render
cadence. The original native NTSC speed complaint is kept isolated to Bond's model path by using
one `sub_GAME_7F007F30` animation tick per native render; retail NTSC remains two and PAL remains
one. No timing constants or dynamic-texture cache behavior are changed.

`0043-gunbarrel-native-bond-resync.patch` follows the first 0042 human test. The blood shimmer
was gone and the overall sequence speed was much closer, but the gunbarrel visibly moved ahead of
Bond. 0042 had restored title movement to every render while intentionally leaving native NTSC
Bond at one model tick per render, so their relative cadence had changed by exactly 2:1. 0043
restores the original NTSC `sub_GAME_7F007F30(..., 2, ...)` count and leaves PAL at one. It does
not change gunbarrel-mode duration, title constants, blood decode cadence, or the dynamic-texture
refresh.

`0044-depot-timedmine-diagnostic.patch` is diagnostic only. With `GETV_OBJ_DEBUG=1`, Depot
objective 1 reports bounded semantic snapshots for tags 1, 2 and 3, including raw tag linkage,
the objective lookup result, prop/type identity, runtime flags, destruction state and damage. With
`GETV_TIMEDMINE_DEBUG=1`, timed mines report timer assignment, projectile initialization, first
weapon-tick state, coarse countdown transitions and the near-zero/explosion boundary. The mine
tick probe also recognizes the timed-mine model so a corrupted `weaponnum` cannot make the
diagnostic disappear. Neither gate changes objective decisions, timer values, object state or
explosion behavior.

`0045-proximity-box-stall-diagnostic.patch` records proximity-mine explosion entry/return and
the shared room-prop list generation around explosion damage. It reports if a nested
`roomGetProps()` call mutates the list while the outer explosion scan is consuming it. The
diagnostic is native-only and does not change explosion or damage behavior.

`0046-stan-room-buffer-native-terminator.patch` gives the native
`stanTestLineUnobstructed()` room array one extra element. The tile walk can produce 20 room
IDs, and the following `roomGetProps()` call requires a `-1` terminator, so the native array
is 21 entries while the original retail-layout declaration is preserved outside
`GE_PORT_NATIVE`.

`0047-stan-los-subphase-diagnostic.patch` makes the stall watchdog distinguish the initial tile
walk, room-prop lookup, collision-bounds lookup, polygon edge loop, final tile walk and return.
It also reports collision edge counts outside the supported 0..8 range before the original code
continues, so a corrupted count remains observable without masking the failure.

`0048-stan-los-deep-diagnostic.patch` narrows the remaining blind spots without adding a
behavioral guard. During a STAN-owned `roomGetProps()` call it publishes room/chunk traversal
progress and reports impossible room IDs, chunk indices, prop indices, chunk-chain length and
lookup-buffer capacity. The STAN consumer separately publishes prop-list slot/index progress and
the two `stanGetPositionYValue()` phases. Compile-time assertions prove the native
`collision_data` ABI is 0x4c bytes with `edges/polygon/top/bottom` at
`0x00/0x04/0x44/0x48`, and therefore still fits both legacy 0x50-byte allocations. The hard-coded
allocation size is intentionally not changed because it is not undersized on the native ABI.

`0049-native-collision-polygon-extent.patch` repairs a separate deterministic native C UB defect
found while auditing the same collision path. `collision_data` has eight `coord2d` hull points,
but the legacy APIs expose that storage as `rect4f *`, whose declared `points[]` member has only
four elements. Native hull projection can legitimately produce 5-8 edges; strict UBSan reproduces
an index-4 out-of-bounds access with a six-edge hull even though the backing allocation has room.
Native builds now index collision polygons through the actual `coord2d` backing array while
non-native builds keep the original `rect4f.points[]` expression. The hull builder, STAN LOS/
volume walkers, generic polygon collision helpers, ray collision path and tank diagnostic edge
walk all use the accessor. This does not cap edges or change valid polygon geometry; it removes
the false four-point C type bound.

`0054-native-ladder-locus-record.patch` repairs a native stack-layout defect in `MoveBond()`'s
ladder/special-STAN queries. Retail code uses `move_bond_temp_struct`, a two-word placeholder whose
8 bytes cover the original 32-bit `rooms` pointer and `count` field. On a 64-bit native build the
pointer widens to 8 bytes, moving `StandTileLocusCallbackRecord.count` to offset 8; the old 8-byte
placeholder therefore ends before the field that `stanGetLocusCount()` reads, while
`stanTileDistanceRelated()` also initialises later record fields beyond it. Native builds now use
the real widened callback record while non-native builds retain the retail placeholder/stack shape.

`0059-gunbarrel-bond-speed-calibration.patch` adds native-only control of the final remaining
gunbarrel polish item. `GETV_GUNBARREL_BOND_SPEED=<float>` changes the initial
`bond_eye_walk` pose rate normally passed as `0.91f`; the default remains `0.91f`,
non-native builds retain the literal retail call, the NTSC/PAL model-tick cadence is unchanged,
and gunbarrel movement/blood timing are untouched. Because the walk animation also carries root
translation, native calibration inversely scales that translation by `0.91 / speed` and restores
scale 1.0 before the turn/fire animation, preserving the already-validated Bond/barrel alignment.
The same patch also adds `GETV_GUNBARREL_SEQUENCE_SPEED` / `gunbarrel_sequence_speed` for the
pre-shot authored cadence. It uses a fractional accumulator, so a value such as `0.692308`
advances about 9 authored steps for every 13 host renders while still rendering every frame.
Once Bond fires, cadence returns to one authored step per render so the already-accepted blood
timing is not disturbed. Human comparison against the supplied NTSC N64 reference accepted
`0.692308` as correct, so that is now the native default while remaining user-adjustable.
The port config layer exposes the same value persistently as `gunbarrel_bond_speed` in
`goldeneye.ini` (with legacy `goldeneye.cfg` still accepted). Values outside 0.25..1.50 or
malformed values fall back to 0.91.
