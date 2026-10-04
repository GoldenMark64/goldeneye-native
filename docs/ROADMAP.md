# Roadmap

**Current as of October 4, 2026.**

GoldenEye Native 1.0 has completed its primary Linux campaign-certification pass: a human player
completed the retail campaign on Agent, Secret Agent and 00 Agent on the primary Ubuntu/Intel test
system. The roadmap is therefore no longer "make the campaign completable." The next work is about
hardening the release, broadening confidence, and improving optional systems without weakening the
source-only/reproducible boundary.

For the exact 1.0 certification scope, see [`TESTING_1.0.md`](TESTING_1.0.md). For the release
summary, see [`RELEASE_1.0.md`](RELEASE_1.0.md).

## 1. Keep the 1.0 release reproducible

This is the highest-priority maintenance rule.

- Keep the public repository free of ROMs, extracted game assets, saves, and built playable
  executables.
- Keep the decompilation changes represented by replayable patches rather than relying on a dirty
  local `vendor/` tree.
- Keep the third-party renderer/audio reconstruction pinned and byte-verifiable.
- Treat a clean checkout/install as the publication test, not a working developer tree.
- Preserve focused regressions for bugs found during the campaign pass.

A release that works only from one maintainer's long-lived checkout is not a release.

## 2. Broaden platform validation

The full 1.0 campaign certification is currently strongest on the primary Linux system. Other
platforms build and have varying levels of runtime validation, but should not be described as
equally campaign-certified until they have equivalent evidence.

### Windows

- Complete clean-machine end-user installation testing.
- Play full missions, then a representative campaign subset, before making stronger runtime claims.
- Keep the no-code setup package ROM-free.
- Do not publish a coordinated binary/setup release until the licensing and code-signing review is
  satisfactory.

### macOS

- Keep Apple silicon and Intel source builds healthy.
- Continue runtime checks of both OpenGL and the optional Metal path.
- Expand human mission testing beyond build/launcher validation where practical.

### Linux

- Retest on additional GPU/driver combinations.
- Keep the Intel Iris Xe / i915 GuC workaround documented as hardware-specific rather than a
  universal GoldenEye fix.
- Add reports from other distributions and architectures without turning anecdotal success into
  blanket support claims.

## 3. Continue the timing audit

High-refresh timing is **improved, not declared complete**.

The real-clock free-run path prevents render FPS from directly driving the main field clock, and
specific timing-sensitive systems have been corrected. Other systems still advance per simulation
update and need subsystem-by-subsystem review.

Priorities:

- identify remaining update-counted gameplay and animation paths;
- define expected retail behavior before changing them;
- convert only where a measurable equivalence target exists;
- add regressions or repeatable measurements for each conversion; and
- keep 60 fps as the conservative deterministic test configuration.

See [`FRAME_TIMING.md`](FRAME_TIMING.md).

## 4. Close known presentation defects

Known non-game-breaking visual issues should remain visible rather than being lost behind the 1.0
label.

Current examples include:

- the missing MI5 crest on multiplayer character select;
- the flat black Select File background where the original has a faint watermark; and
- any newly reported geometry, texture, aspect, or animation defect that reproduces on the current
  release line.

Fixes should continue to use reference captures or other concrete evidence where possible.

## 5. Improve network play

LAN multiplayer is experimental. Peers can connect and exchange input, but sessions can desync.

Next useful milestones are:

- reproduce divergence with bounded fingerprints;
- identify the first differing simulation state rather than only the visible symptom;
- test across two real machines/architectures after same-machine determinism passes; and
- avoid presenting netplay as reliable until long sessions remain synchronized.

See [`NETPLAY.md`](NETPLAY.md).

## 6. Mature optional gameplay systems

The optional systems are valuable, but they are not part of the default 1.0 campaign certification.

### Co-op

Co-op can place multiple players into solo missions, but mission scripting was authored around one
Bond. Improve objective/cutscene semantics before treating it as complete.

### Bots and automation

The repository contains substantial sensing, routing, CLI, and automation work. Continue it as an
optional engine-opening effort rather than as a blocker for the stable base game.

The old Train-focused two-machine bot plan is preserved in [`TASK_QUEUE.md`](TASK_QUEUE.md) as
historical engineering context; it is no longer the project's release roadmap.

### Free camera

Free camera works as a photo/debug feature, but visibility remains rooted in Bond's room. A true
fly-anywhere camera requires moving the portal/culling visibility root safely.

### Horde, gibs, mods and scripting

Continue hardening opt-in features independently. A beta feature should stay labelled beta until its
failure modes and save/gameplay interactions are understood.

## 7. Keep provenance and licensing explicit

The public 1.0 design deliberately avoids redistributing the fifteen unresolved sm64ex/Fast3D
baseline files: they are fetched from a pinned upstream and transformed locally.

Continue to:

- keep the root repository license distinct from third-party terms;
- preserve notices for code adapted from permissively licensed projects;
- quarantine sources whose terms are incompatible or absent;
- keep ROM/game-data boundaries enforced by tooling; and
- update [`LICENSING.md`](LICENSING.md), [`THIRD_PARTY.md`](THIRD_PARTY.md), and
  [`../getv/port/PROVENANCE.md`](../getv/port/PROVENANCE.md) when provenance changes.

## 8. Documentation is part of the release

When behavior changes, update the player-facing claim and the technical record together.

The authoritative order is:

1. current source/script behavior;
2. focused tests and measurements;
3. release/testing records;
4. subsystem documentation; and
5. summary/marketing language.

If two documents disagree, the contradiction is a bug.

## Not release blockers

The following may continue after 1.0 without undermining the certified base campaign:

- experimental netplay;
- bots and autonomous mission play;
- horde and co-op expansion;
- additional graphics effects;
- new modding APIs;
- mobile/tvOS bring-up; and
- convenience packaging on platforms that already have a source-build path.

The stable base game should not be held hostage to optional experiments, and optional experiments
should not inherit the word "stable" merely because they share the same repository.
