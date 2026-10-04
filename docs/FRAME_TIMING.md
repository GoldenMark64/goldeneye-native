# Frame timing

GoldenEye's timing model was written for N64-era presentation rates, and some game systems advance
per update rather than purely from elapsed time. Native high-refresh rendering therefore needs more
than an uncapped renderer.

**Current position:** this port has made substantial timing improvements and has measured working
high-refresh clock behavior, but it does **not** claim that every timing-sensitive subsystem is
fully converted or perfectly retail-equivalent at arbitrary render rates.

Credit where it is due: **Graslu** raised the high-frame-rate behavior publicly and was right to.
It is one of the first things a knowledgeable GoldenEye player checks.

## What actually goes wrong

GoldenEye advances a main time base in video fields, but a large amount of game logic was authored
around the cadence of game-loop updates. Those are related, but they are not the same thing.

A naive uncapped port can therefore make the renderer run faster **and** cause update-counted
systems to execute more often than they did on the original machine. Depending on the subsystem,
that can affect animation cadence, weapon behavior, reactions, delays, or other state.

The important distinction is:

- **clock pacing**: how much game time is reported as having elapsed; and
- **frame/update-counted behavior**: logic that advances once because another simulation update ran.

Fixing the first does not automatically prove the second is perfect everywhere.

## Why a native port can improve this

An emulator executes the retail ROM and can change how quickly the emulated machine runs, but the
game's update-counted behavior is still compiled into the ROM. A native port built from the
decompilation can change that logic directly, measure it, and add tests around individual systems.

That makes a complete solution possible in principle. It does not make it automatic.

> No code from `Graslu/1964GEPD` or the 1964 lineage is used in this project. Those sources are
> outside this project's permitted reuse set; see [`REUSE_AUDIT.md`](REUSE_AUDIT.md).

## What the port does today

### Real-clock free-run

The player-facing high-refresh setting is:

```ini
framerate = off
```

That selects the real-clock free-run path. On this path the renderer can run without a presentation
cap while elapsed real time, rather than rendered-frame count, drives the main GoldenEye field
clock.

The configuration documentation records this measured Dam result:

| mode | clock | fields/sec (60.0 is nominal NTSC) | rendered fps |
|---|---|---:|---:|
| 60 fps default | synthetic | 60.0 | 60 |
| uncapped | synthetic | 811.9 | 812 |
| **uncapped / `off`** | **real** | **60.5** | **456** |

That is an important improvement: high render FPS no longer has to imply a main game clock running
hundreds of times per second.

It is **not** proof that every gameplay system behaves exactly like retail at 456 fps.

### Free-run divider correction

`0009-freerun-divider.patch` corrected an additional problem in the inherited high-refresh path.
A simulation divider layered on top of the real-time free-run path made camera interpolation follow
render-frame phase rather than elapsed time, which could present as flicker.

Under real-clock free-run, the divider is now held at 1 because elapsed time is already gating the
simulation.

### 30 Hz paired timing mode

`framerate = 30` pairs the presentation cap with `GETV_TICKFIELDS=2`. Each update reports two
elapsed video fields, preserving the main real-time clock while reducing update-counted systems to
30 updates per second.

This remains useful for testing behavior against a lower authored cadence.

### Timing-sensitive systems that have received focused fixes

The port has also corrected specific timing-sensitive paths rather than treating timing as one
global switch. Examples include:

- automatic fire converted to time-based behavior on the player and AI paths;
- gunbarrel cadence, Bond synchronization, blood-wipe presentation, and sequence-speed work;
- reverse-animation interpolation on the native path; and
- render-only-frame handling for auto-crouch state.

These are concrete improvements. They should not be read as evidence that every update-counted
system has been audited.

## Camera interpolation

The inherited high-refresh work includes camera interpolation for divided simulation modes.
`GETV_INTERP=1` is the normal path and `GETV_INTERP=0` is the control.

Only the camera basis is interpolated at render time: position, look direction, and up. The
interpolated values are not written back into game state.

That restriction matters because GoldenEye's AI can branch on visibility
(`IFImOnScreen`, `IFMyRoomIsOnScreen`). Blending arbitrary simulation state would risk changing
gameplay while trying to smooth presentation.

### Historical measurement

Stage 9, scripted forward walk, frames 700-850 of a fixed 901-frame run:

| | still frames | mean step | sd of step |
|---|---:|---:|---:|
| SIMDIV=1, INTERP=0 | 12.1% | 0.2553 | 0.1855 |
| SIMDIV=1, INTERP=1 | 12.1% | 0.2553 | 0.1855 |
| SIMDIV=2, INTERP=0 | 55.7% | 0.2633 | 0.3750 |
| **SIMDIV=2, INTERP=1** | **10.7%** | 0.2645 | **0.1877** |
| SIMDIV=4, INTERP=0 | 75.2% | 0.3112 | 0.6541 |
| **SIMDIV=4, INTERP=1** | **0.0%** | 0.3117 | **0.1843** |

Those results demonstrate a substantial reduction in visible camera stepping for the measured
scenario. They do not establish full high-refresh equivalence for all game state.

## What remains frame-counted

The current configuration reference notes that only 13 of the 135 translation units under
`src/game` scale by `g_GlobalTimerDelta`. Automatic fire is time-based by default, but other
systems still advance once per simulation update.

That remaining audit is the open-ended part of the timing problem. Candidates include behavior
such as animation stepping, delays, reactions, or other counters that may have been tuned against
the original update cadence.

Each conversion needs two things:

1. a clear statement of what retail behavior should be; and
2. a regression or measurement showing that the native change preserves it.

The project therefore treats timing as a collection of measurable subsystem behaviors, not as one
boolean "fixed/not fixed" property.

## Practical guidance

- **Default 60 fps on NTSC is the conservative general-purpose setting.**
- `framerate = off` is available for high-refresh rendering and has substantially improved clock
  behavior compared with naive uncapping.
- High-refresh mode should be described as **improved**, not as proof of perfect original timing
  across every subsystem.
- Deterministic measurement harnesses should normally stay on 60 fps because real-clock free-run is
  load-dependent and therefore not frame-for-frame reproducible.
- The 1.0 human campaign certification applies to the tested default release path; it is not an
  exhaustive certification of every optional timing mode.

See [`CONFIGURATION.md`](CONFIGURATION.md#framerate) for the player-facing settings and
[`../getv/patches/README.md`](../getv/patches/README.md) for the replayable timing-related
patches.
