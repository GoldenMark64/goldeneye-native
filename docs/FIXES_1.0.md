# GoldenEye Native 1.0 — Fix Summary

> **Draft**
>
> This is a release-facing summary, not a chronological debugging log. Before v1.0.0 is tagged,
> verify every entry against the final release commit.

## Gameplay and mission behavior

- Fixed native traversal and collision defects investigated during the community continuation.
- Fixed ladder behavior.
- Fixed tank climbing / interaction behavior.
- Fixed post-mission stale input behavior.
- Fixed Boris reload behavior.
- Fixed Train-related mission issues investigated in the 1.0 repair cycle.
- Fixed NPC state behavior that could leave enemies appearing to walk continuously or retain the
  wrong pistol state.

## Rendering and geometry

- Fixed Archives partial-geometry defects.
- Fixed diagonal-geometry defects.
- Fixed missing enemy / third-person gunfire rendering.
- Fixed reverse animation interpolation behavior.
- Fixed VTXSTORE-related rendering behavior.
- Fixed gunbarrel timing / cadence defects.
- Added extensive renderer diagnostics and hardware correlation tooling used to isolate Intel GPU
  hangs.

## UI and input

- Fixed file-select hitbox behavior.
- Fixed post-mission input-state leakage.

## Stability

- Investigated and narrowed reproducible Intel Iris Xe / Mesa / i915 GPU hangs to a repeatable
  renderer workload path.
- **1.0 release blocker:** the remaining GPU hang must be fixed and validated before this document
  can describe the release as stable.

## Existing community work

This release also sits on top of earlier community-maintained work already tracked in
`PATCH_QUEUE.md`, including renderer, configuration, developer-console, effects, and telemetry
changes. The patch queue remains the detailed replay/provenance record for those changes.

## What this file intentionally omits

This file does not list every diagnostic build, failed experiment, temporary environment variable,
capture helper, or intermediate patch. Those belong in engineering handoffs and worklogs, not in the
public 1.0 release summary.
