# GoldenEye Native 1.0

**Status: CERTIFIED RELEASE LINE — October 3, 2026**

GoldenEye Native 1.0 is the first stable release line of this community-maintained continuation.
The complete retail campaign has been completed through human playtesting on **Agent, Secret Agent
and 00 Agent**. As of this certification pass, there are **no known game-breaking bugs remaining in
the tested retail campaign path**.

This is an independently maintained continuation of GoldenEye Native. It is not an official
release from the original upstream maintainer, Nintendo, Rare, MGM or Danjaq.

## What was certified

- Every retail mission was completed through human playtesting on Agent.
- Every retail mission was completed through human playtesting on Secret Agent.
- Every retail mission was completed through human playtesting on 00 Agent.
- Previously game-breaking native-port defects repaired during the stabilization cycle were
  exercised through campaign play and focused diagnostics/regressions.
- The final recurring Linux/Intel whole-system GPU stall was isolated from GoldenEye game logic and
  resolved on the primary Ubuntu/i915 test environment by disabling i915 GuC submission.

The certification applies to the tested default 1.0 gameplay path. It does not claim exhaustive
coverage of every mod, cheat, multiplayer/netplay combination, controller, optional rendering
feature, operating system or graphics driver.

## Major stabilization areas

The 1.0 line includes repairs and validation across native pointer/layout assumptions, collision and
traversal, ladders and tank interaction, mission progression, post-mission input, scripted character
behavior, NPC animation/weapon state, gunbarrel timing, geometry/rendering defects and platform
stability. `FIXES_1.0.md` is the release-facing summary; the patch queue and focused regression tests
remain the implementation/provenance record.

The final source-only review also performed a reconstruction-equivalence audit against the exact
playtested decomp source. `0061-playtested-native-source-catchup.patch` captures fixes that had been
validated in the private working tree but were not yet represented in the public patch queue. After
that catch-up, a clean source reconstruction matches the playtested source tree except for files that
the installer intentionally generates locally from the user's own game data. See `TESTING_1.0.md`.

The final Linux x86-64 publication check also aligned the compiler with frozen 0064. The reference
binary records GCC 13.3.0; the public Linux build now defaults x86-64 to GCC while preserving
explicit compiler overrides. A fresh GCC 13.3.0 rebuild was human-checked at the Dam view that had
previously exposed the persistent stray-line/geometry defect, and the defect was absent. This was a
focused reconstruction acceptance check, not a second full-campaign certification pass. See
`TESTING_1.0.md` for the exact build evidence.

## Advanced renderer diagnostics retained in 1.0

The Intel investigation produced a reusable GPU/function-flight subsystem capable of joining a
hardware i915 batch to GoldenEye draw serials, Fast3D command provenance and the C/OpenGL call chain.
It is included in 1.0 for future renderer troubleshooting but is **inactive during normal play**.
See `RENDERER_TROUBLESHOOTING.md` and `DEBUG_TOOLING.md`.

## Perfect Dark renderer compatibility path

An optional desktop OpenGL backend profile was adapted from the MIT-licensed Perfect Dark PC port
as a controlled A/B during the GPU investigation. It uses a GL 3.3 compatibility context, GLSL
1.30 vocabulary, a dedicated VAO, `GL_RGBA8` texture storage and an end-of-frame `glFlush()` while
retaining GoldenEye's own Fast3D frontend and display lists.

It did not fix the Intel hang and is **off by default**. It remains available for compatibility
testing through the launcher's **Perfect Dark renderer compatibility path** checkbox or:

```ini
pd_renderer = 1
```

The normal 1.0 renderer path is unchanged unless explicitly enabled. Provenance is recorded in
`THIRD_PARTY.md` and the retained MIT notice.

## Intel/i915 note

On the primary Ubuntu x86-64 Intel/i915 test environment, recurrent render-engine hangs were
eliminated after booting with:

```text
i915.enable_guc=0
```

That is recorded as a platform workaround/fix for the tested machine, not as proof that a specific
GoldenEye source defect caused the GPU hang. `RENDERER_TROUBLESHOOTING.md` documents the evidence and
the exact diagnostic chain that led to that conclusion.

## Game-data boundary

No ROM, extracted game assets, `base.zip`, saves, texture dumps or other game data are part of the
release source. Players provide their own legally obtained supported cartridge dump and build the
native executable locally according to the project documentation.
