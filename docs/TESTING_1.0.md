# GoldenEye Native 1.0 — Human Validation Record

> **Status: CERTIFIED — October 3, 2026**

This is the human campaign-certification record for the GoldenEye Native 1.0 release line.

## Primary validation environment

| Item | Value |
|---|---|
| Release line | GoldenEye Native 1.0 |
| Certification date | 2026-10-03 |
| Primary Linux test system | Dell Latitude 5530 |
| OS / kernel at final GPU validation | Ubuntu 24.04 HWE / `7.0.0-38-generic` |
| CPU | Intel Core i7-1270P |
| GPU | Intel Alder Lake-P GT2 / Iris Xe, PCI `8086:46a6` |
| Kernel graphics driver | `i915` |
| Intel stability setting | `i915.enable_guc=0` on the affected Latitude |
| Renderer | Normal GoldenEye OpenGL path for final clean stability playtest |
| Game revision | Supported US retail build inputs, supplied locally by the tester |
| Tester | Human campaign playtest |

The release documentation and dormant diagnostic/compatibility tooling may be committed after the
runtime candidate without changing the default gameplay/render path. The Git commit carrying this
record is therefore documentation identity, not a claim that a ROM-derived binary is distributed
from GitHub.

## Campaign certification

The complete retail campaign has been completed by a human player on all three retail difficulties.

| Difficulty | Full campaign completed | Certification date | Result |
|---|---:|---|---|
| Agent | **Yes** | 2026-10-03 | Working |
| Secret Agent | **Yes** | 2026-10-03 | Working |
| 00 Agent | **Yes** | 2026-10-03 | Working |

This certification means the campaign can be completed through the tested release line and that no
known game-breaking defect remains in that tested retail path. It does not imply exhaustive coverage
of every cheat, mod, multiplayer configuration, input device, optional graphics feature or hardware
combination.

## Stabilization areas exercised

The 1.0 repair cycle included focused work around:

- native collision, STAN and traversal behavior;
- ladders and tank interaction;
- mission completion and post-mission input state;
- Control scripted/resource edge cases and thrown-projectile room bookkeeping;
- Train mission progression/crash cases;
- Surface 2 remote-mine native layout;
- file-select native-layout hitboxes;
- NPC animation and weapon state;
- reverse-animation interpolation;
- Archives/diagonal geometry and other native rendering defects;
- gunbarrel authored timing/cadence;
- third-person/guard weapon behavior; and
- renderer/GPU stability under known stress locations.

Focused source tests, patch-stack reconstruction checks and diagnostic captures supplement the human
campaign pass. They do not replace it.

## Release reconstruction equivalence

The final source-only release review found that several fixes present in the human-playtested
`vendor/ge-decomp` working tree had never been exported back into the public `getv/patches/` queue.
The most visible symptom was guards being forced to use walking animation in a fresh GitHub build
even though the campaign-tested build had already fixed that defect.

`0061-playtested-native-source-catchup.patch` closes that publication gap. Starting from the pinned
decomp source and applying the published source queue through `0060`, then `0061`, reproduces the
playtested `vendor/ge-decomp/src` tree byte-for-byte. The only remaining differences are
`ge_asset_fileview.h` and `ge_asset_fileview_check.c`, which are generated locally during asset
preparation and are intentionally not published.

The catch-up carries the already validated native fixes and dormant diagnostics that were present in
the campaign-tested source, including model-slot lifecycle metadata, CCTV `lookpad`, objDeform
RW-data indexing, AI_PRINT sizing, Train objective interpretation and diagnostics, VTXSTORE character
typing, EXTRAMEM reload scratch sizing, projectile room-stack safety, gunbarrel RLE/Gfx corrections,
and NPC run/rifle animation selection.

Focused regression rerun after the catch-up: AI_PRINT 4/4, Train door/objective 5/5, Train post-hack
2/2, VTXSTORE 2/2, EXTRAMEM 3/3, projectile rooms 2/2, guard-gun 4/4, NPC animation 4/4,
reverse-animation 3/3, and gunbarrel timing PASS.

## Intel GPU blocker closure

The final recurring blocker was a whole-desktop Intel Iris Xe GPU stall. The investigation created
GPU submission and function-flight rings, Fast3D producer provenance, an unattended hard-hang
capture path, and hardware-batch correlation tooling. The decisive capture showed application GL
submission calls returning normally while i915 remained wedged asynchronously; the hardware-mapped
draw also belonged to a shader/state path that had executed successfully many times immediately
before the hang.

A controlled platform A/B then held GoldenEye and the kernel version constant while booting the
same `i915` driver with `i915.enable_guc=0`. After that change, several levels were played without
reproducing the previously recurring stalls. The Latitude environment is therefore certified with
GuC disabled. The investigation is documented in `RENDERER_TROUBLESHOOTING.md`.

## Optional features are not part of the default certification

The Perfect Dark-derived renderer compatibility path is included for advanced comparison work but
defaults off (`pd_renderer = 0`). The GPU/function-flight recorders and stall watchdog also require
explicit diagnostic activation. Their presence does not alter the normal certified renderer path.
