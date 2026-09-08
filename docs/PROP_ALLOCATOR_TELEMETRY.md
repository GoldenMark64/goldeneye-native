# PropRecord allocator telemetry

## Purpose

`GETV_PROP_TELEMETRY=1` enables a content-free JSON Lines report from the allocator that owns the
fixed runtime `PropRecord` pool. It measures allocator activity directly; it never reconstructs
occupancy from setup definitions and never calls a count safe, qualified, or recommended.

The initial consumer is
[`goldeneye-mission-editor#49`](https://github.com/seb-patron/goldeneye-mission-editor/issues/49).
The language-neutral schema is also the legacy boundary for a future Rust capability. The producer
does not compute editor evidence keys: the caller binds a report to its measured executable,
materialized source, patch, profile, and configuration fingerprints.

For a graphical setup, use the desktop launcher’s **Developer Tools** page. See the
[guide and FAQ](DEVELOPER_TOOLS.md) for manual recording, report locations and interpretation.

## Enable a bounded run

Supply both raw diagnostic gates:

```bash
GETV_PROP_TELEMETRY=1 \
GETV_PROP_TELEMETRY_RUN_ID=01936f5d-2417-7a10-8650-47a691999a71 \
GETV_KEYBOARD_IDLE=0 \
GETV_STAGE=34 GETV_INTROCAM=0 GETV_EXIT_FRAME=301 \
./getv/build-mac/goldeneye
```

`GETV_PROP_TELEMETRY` accepts exactly `1`. The run ID is a caller-owned opaque token of 1 through
128 ASCII characters. Its first character must be alphanumeric; later characters may also use
`.`, `_`, `:`, and `-`. Paths, whitespace, quotes, and separators are rejected. A missing or invalid
run ID disables telemetry and writes one diagnostic to stderr.

With valid input, one `stage-ready` record is emitted after stage setup and one `run-final` record
is emitted immediately before `GETV_EXIT_FRAME` uses its bounded `_exit` path. With telemetry
disabled, the hooks do no accounting and ordinary stdout is unchanged.

## Optional local files

`GETV_PROP_TELEMETRY_FILE` specifies a new JSONL file in an existing local directory. When
telemetry is enabled, the producer also creates a readable `<file>.txt` summary. Both files
use exclusive creation: existing files are never overwritten. Each observation is flushed;
only a timed `run-final` marks completion. Files contain telemetry only, not surrounding logs.
File errors are reported on stderr; stdout telemetry continues. Disabled telemetry creates no files.
The destination path is never included in the records. The version 1 JSON schema is unchanged.

## Contract version 1

Every record has `schema: "goldeneye-native.prop-allocator"` and `version: 1`. Records from one
process use the caller's `runId`, an increasing `sequence`, and a `poolEpoch` that advances when the
game rebuilds the allocator free list.

`identity` contains only bounded runtime identities:

- `platform`, `architecture`, `renderer`, and game `buildVariant`;
- numeric `stageId`, `difficulty`, and `playerCount`; and
- numeric `gameTick` and completed `renderFrame` ordinals.

`allocator` keeps units explicit:

- `capacity`, `currentAllocated`, `freeSlots`, `highWaterAllocated`, and `minimumFreeSlots` are
  runtime prop-pool slots;
- `stageReadyAllocated` is the direct occupancy at the post-setup boundary;
- `postStageReadyHighWaterAllocated` is the highest direct occupancy from that boundary onward;
- allocation calls, successes, failures, and frees are raw event counters; the four
  `postStageReady*` fields are the corresponding suffix of the run; and
- `observedExhaustion` says only that the real allocator returned no slot at least once.

`onscreen` is a separate namespace. `arrayCapacity`, `currentCount`, and `highWaterCount` describe
the transient on-screen pointer list, not the allocator pool. The list stores a trailing null
pointer, so `currentCount < arrayCapacity` is the measured terminator-safe relation.

`invariants` reports producer self-checks without assigning product policy:

- `accountingConsistent` checks aggregate allocate/free arithmetic;
- `slotTrackingConsistent` checks that allocator callbacks name valid, non-duplicated slots; and
- `onscreenWithinCapacity` checks the separate on-screen list relation above.

The fixed serializer refuses truncation. Values are emitted as compact JSON followed by one
newline. No record accepts arbitrary field names or strings beyond the bounded identity tokens.

## Limits of the evidence

Version 1 does not measure room-list allocation failures: the audited allocator paths expose no
room-list failure event at this boundary. It also does not measure model slots, display-list,
vertex, matrix, texture, audio, or renderer pools. Each requires its own explicit namespace and
instrumentation before it can support a finding.

A clean bounded run proves only what its two records say for that exact binary, stage, profile,
configuration, patch, and workload. It does not prove a maximum authoring count, valid placement,
startup across other configurations, combat/pathing behavior, or a gameplay-safe ceiling. Those
conclusions require correlated editor receipts and repeated stress evidence.
