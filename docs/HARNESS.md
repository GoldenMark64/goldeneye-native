# Automated input harnesses

GoldenEye Native has two useful input-injection layers. Older versions of this document said
`GETV_SCRIPT` did not move a player; that was true of an earlier implementation and is no longer
current.

## `GETV_SCRIPT`: device-side scripted input

`GETV_SCRIPT` is implemented in `getv/port/src/port_input.c`. It injects a synthetic
`GePadState` **upstream** of the normal N64 controller mapping and sample-ring path. Downstream,
the production input path still performs N64 button mapping, stick scaling, C-button thresholding,
`osContGetReadData()`, the `joy.c` sample ring and press-edge detection.

A live script forces its selected port present, so it does not require a physical controller or a
separate `GETV_PADS` setting.

Syntax:

```text
GETV_SCRIPT="<frame>:<keys>[:<hold>][,...]"
GETV_SCRIPT_PORT=<n>       # default 0
GETV_SCRIPT_TRACE=0        # optional: silence per-entry trace
```

Keys are N64-oriented and can include `A B X Y START BACK Z L R DU DD DL DR CU CD CL CR LT RT`
plus `SX=<n>` and `SY=<n>` for N64 stick counts in the -80..80 range. Buttons should normally
be held for at least two frames because the game's edge detector derives presses from consecutive
samples; the default script hold is four frames.

The script path has priority over keyboard/mouse input for values it asserts and also overlays a
real controller, which keeps unattended runs reproducible whether or not a pad happens to be
connected.

## Player API: tick-addressed injected input

The player API is a different seam. `gePlayerClaim(slot, GE_SLOT_INJECTED)` and
`gePlayerPost(...)` queue explicit per-slot input against the player API's game tick. This is the
right interface for bots, external agents and netplay because a caller can target a slot and a
specific tick and can detect refusal when input is posted too late or the queue is full.

That refusal is significant: netplay and automation must be able to distinguish “applied” from
“missed the simulation tick.”

## Which one to use

Use `GETV_SCRIPT` when the test should behave like a synthetic local controller going through the
normal device/mapping path: front-end navigation, button sequences, bounded gameplay actions and
reproducible local input scenarios.

Use the player API when the test or subsystem needs explicit player ownership, tick scheduling or a
shared control seam with bots/netplay/agents.

They are complementary, not competing implementations.

## Measurement rules

Two old lessons remain valid regardless of injection path:

1. **Always run a no-input control.** Intro/cutscene motion can make a player appear to have moved
   even when injected movement did nothing.
2. **Sample a fixed frame/tick, not merely the last log line.** A scripted hold can end before the
   process exits; looking only at the final sample can turn a successful action into an apparent
   no-op.

For higher-level bounded scenarios, combine scripted/player-API input with the current state/event
interfaces and an explicit termination condition rather than treating renderer activity as a
gameplay pass condition.
