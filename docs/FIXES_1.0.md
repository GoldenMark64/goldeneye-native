# GoldenEye Native 1.0 — Fix Summary

This is a release-facing summary of the 1.0 stabilization cycle rather than a chronological debug
log. Individual replayable fixes, dependencies and source provenance remain in the patch queue,
focused tests and subsystem documentation.

## Campaign and gameplay stability

- Fixed native traversal/collision issues that could block normal campaign progression.
- Fixed ladder behavior, including the native widened callback-record mismatch behind intermittent
  ladder failures.
- Fixed tank mounting/climbing/contact behavior affected by native object/layout assumptions.
- Fixed post-mission stale-input behavior and mission-transition problems investigated during the
  campaign pass.
- Fixed Control Boris/resource and player-thrown-projectile failures that could stop or crash the
  mission.
- Fixed Train mission defects found during the stabilization playthrough.
- Fixed Surface 2 remote-mine native-layout behavior.
- Fixed file-select hitboxes affected by native layout assumptions.

## Animation, weapons and presentation

- Fixed NPC state behavior that could leave enemies continuously walking or showing the wrong
  pistol/weapon state.
- Fixed native reverse-animation interpolation.
- Fixed third-person/guard weapon behavior investigated during campaign testing.
- Fixed VTXSTORE/native object-type behavior involved in rendering/gameplay failures.
- Fixed gunbarrel authored cadence, Bond timing and transition behavior.

## Rendering and geometry

- Fixed previously observed Archives/partial-geometry and diagonal-geometry defects in the native
  renderer path.
- Added dynamic texture refresh handling for mutable transient graphics buffers.
- Added source provenance for GPU-flight draws so hardware submissions can be tied back to
  GoldenEye background/model display-list producers.

## Intel GPU stability and forensic tooling

- Built a rolling GPU submission-flight recorder containing draw serials, state, shader/texture
  identity, VBO fingerprints and Fast3D context.
- Built a function/submission flight recorder that links GPU serials to the renderer C call chain.
- Added a one-shot freeze latch so a recoverable GPU stall cannot overwrite its own pre-stall
  function history.
- Added unattended capture for stalls that freeze the keyboard/mouse/desktop, including fresh i915
  first-error capture, GDB stacks and automatic termination after evidence is safe.
- Added decoders and hardware-batch mapping/analyzer tools capable of correlating an i915 active
  `3DPRIMITIVE` back to GoldenEye draw/function provenance.
- Closed the tested Linux/Intel whole-system stall by disabling i915 GuC submission with
  `i915.enable_guc=0`. This is classified as a tested platform workaround/fix, not a proven
  GoldenEye source-code root cause.

## Perfect Dark-derived compatibility work

The 1.0 source retains an optional backend profile adapted from the MIT-licensed Perfect Dark PC
port at commit `514bf7affd3259b7919165201342ff81a026d92c`, `port/fast3d/gfx_opengl.cpp`.

Behind the explicit `pd_renderer` / `GETV_PD_RENDERER` gate it provides:

- desktop OpenGL 3.3 compatibility context selection;
- GLSL 1.30 shader vocabulary while preserving GoldenEye's combiner formulas;
- a dedicated VAO;
- `GL_RGBA8` texture storage; and
- an end-of-frame `glFlush()`.

It did not eliminate the Intel hang and is therefore **off by default**. It is retained as an
advanced compatibility/troubleshooting option. GoldenEye's Fast3D frontend, display lists and game
renderer semantics remain in place.

## Release status

The complete retail campaign has been human-playtested on Agent, Secret Agent and 00 Agent. As of
October 3, 2026, there are no known game-breaking bugs remaining in the tested 1.0 retail campaign
path. See `TESTING_1.0.md` for the certification scope and `RENDERER_TROUBLESHOOTING.md` for the GPU
investigation.
