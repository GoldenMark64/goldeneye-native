# Player API

**Status: implemented; inherited from the pre-takeover SegfaultEvan development line.**

The native player-input/state seam lives in:

- `getv/port/src/ge_player_api.h`
- `getv/port/src/ge_player_api.c`

It is used by automation, bots and the experimental netplay path. The implementation and its
original validation were developed in SegfaultEvan's August 2026 history before the GoldenMark64
1.0 stabilization work. The earlier version of this document was written while the interface was
still research/design work; Git history preserves that research. This page documents what the
current tree contains without treating the inherited tests as GoldenMark64 certification.

## Purpose

A bot, scripted agent and network peer all need the same primitive:

1. claim a player slot;
2. provide input for a numbered game-thread tick; and
3. read back what happened.

Keeping that seam in one native C API avoids separate input implementations drifting apart.

## How input reaches GoldenEye

The API attaches through GoldenEye's own demo-playback hook, `joySetPlaybackFunc()`.

That choice gives the API several useful properties:

- the callback runs on the game thread immediately before GoldenEye consumes the pad samples;
- all four controller slots are decided together;
- injected slots work without pretending that physical controllers are connected;
- menus and gameplay read the same injected pad stream; and
- hardware-controlled and injected slots can coexist.

The playback hook is installed lazily. Calling `gePlayerApiInit()` alone does not change the
front end; the hook is installed when at least one slot is claimed as `GE_SLOT_INJECTED` and is
removed again when every slot returns to `GE_SLOT_HARDWARE`.

## Public input surface

The public header exposes:

```c
void gePlayerApiInit(void);
void gePlayerApiShutdown(void);

void gePlayerClaim(int slot, GeSlotSource src);
GeSlotSource gePlayerSource(int slot);

unsigned long gePlayerTick(void);
int gePlayerPost(int slot, unsigned long tick,
                 const GePlayerInput *in, int hold_ticks);
void gePlayerClearQueue(int slot);
```

`GePlayerInput` contains intent bits plus N64-style stick values:

```c
typedef struct GePlayerInput {
    unsigned int buttons;   /* GE_IN_* intent flags */
    int stick_x;
    int stick_y;
} GePlayerInput;
```

Stick values are clamped to the N64-scale range expected by the game. Callers should express
actions through `GE_IN_*` intents rather than hard-coded N64 button bits so the API can resolve
control-style differences.

A post returns **1** when accepted and **0** when refused. Refusal is observable on purpose: a late
or full-queue input must not silently disappear, especially in netplay.

Passing `tick == 0` schedules against the API's current consumable tick. Positive
`hold_ticks` keeps the action applied for that many playback callbacks; zero is normalized to one
tick.

## Tick and determinism helpers

```c
unsigned long gePlayerTick(void);
void gePlayerPinDelta(int fields);
int gePlayerPinnedDelta(void);
unsigned int gePlayerSeedFingerprint(void);
```

The tick increments after a complete four-slot playback sample has been prepared.

`gePlayerPinDelta()` exists for deterministic/lockstep work. The experimental netplay path uses a
pinned simulation step so peers do not derive different world advances from different host timing.

`gePlayerSeedFingerprint()` exposes the low 32 bits of GoldenEye's random seed after the frame
sample is decided. This is a cheap divergence signal, not a full simulation-state hash.

For the current network-determinism investigation, see [`NETPLAY.md`](NETPLAY.md).

## State readout

The current API can expose player state through:

```c
int gePlayerStateGet(int slot, GePlayerState *out);
int gePlayerSlotCount(void);
int gePlayerControlType(int slot);
int gePlayerSlotIsDrivable(int slot);
int gePlayerCommandedMove(int slot);
```

`GePlayerState.fields` says which values are valid. Do not interpret a zero value as available
unless its corresponding `GE_ST_*` bit is present.

Current field groups are:

- position;
- room;
- heading/angle;
- health, armour and dead state;
- weapon plus clip/reserve ammunition; and
- multiplayer score counters.

The game-side accessors live in the patched decompilation because the port layer cannot directly
name GoldenEye's private player structures.

## Two-controller control styles

GoldenEye's 2.x control styles are genuinely two-controller layouts. The current API handles the
important movement trap: the walk axis is routed to the companion pad that the game actually reads,
so injected players no longer merely turn while standing still.

That does **not** mean every two-controller button combination is representable through one
`GePlayerInput`.

In particular:

- on 2.1/2.2, FIRE is available on the primary pad but AIM belongs to the second controller;
- on 2.3/2.4, AIM is available on the primary pad but FIRE belongs to the second controller; and
- companion-pad strafe is not yet expressed by this API.

When a requested FIRE/AIM intent cannot be represented safely, the implementation warns once per
slot rather than sending a bit that means the opposite action.

This matters because the port's modern default is 2.2 Galore. Movement is supported there; callers
should still respect the button limitation above.

## Co-op status

The current source contains the movement-routing changes that superseded the earlier "players do not
walk" investigation, but that conclusion comes from SegfaultEvan's pre-takeover measurements, not
from GoldenMark64's 1.0 campaign testing.

The relevant history includes the August 25 player-API companion-pad routing work and later co-op
commits documenting first-person transition and measured player movement. [`COOP.md`](COOP.md)
preserves that evidence. Until GoldenMark64 independently reruns a co-op validation pass, describe
this as **inherited documented behavior**, not as a newly certified 1.0 result.

## Consumers

### Bots

Bots claim injected slots and post the same `GePlayerInput` actions a different external policy
could post. Bot behavior and navigation live outside this API; see [`BOTS.md`](BOTS.md).

### Netplay

Netplay uses the player seam for local/remote slot input and deterministic tick ordering. The
transport is integrated and real sessions run, but simulation desynchronization remains an open
problem. See [`NETPLAY.md`](NETPLAY.md).

### Automation and research

The API is also suitable for deterministic harnesses and external-agent experiments. Higher-level
wrappers should be treated as consumers of this C interface rather than as a replacement for it.

## Current limitations

- Maximum player slots remain GoldenEye's native four.
- Lockstep netplay is not yet deterministic across the tested peer sessions.
- Some 2.x FIRE/AIM semantics require the second physical/controller pad and are not yet fully
  representable through one `GePlayerInput`.
- Companion-pad strafe is not expressed by the current movement-routing helper.
- The seed fingerprint is deliberately small and should not be mistaken for a complete state hash.

These limitations are narrower than the old "design, not implemented" status: the API itself is
real and exercised; the remaining work is in specific consumers and control-style edge cases.
