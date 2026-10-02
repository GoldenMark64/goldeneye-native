# GoldenEye Native 1.0 — Release Draft

> **Status: PRE-RELEASE / NOT YET CERTIFIED**
>
> This document is being prepared ahead of the 1.0 release. Do not publish or tag 1.0 until the
> remaining Intel GPU hang is fixed, the final candidate is built from the intended release tree,
> and the human validation described in `TESTING_1.0.md` is complete.

## What 1.0 means

Version 1.0 is intended to mark the first stable release of this community-maintained fork that is:

- playable through the complete retail game;
- human-validated on all three retail difficulties;
- backed by focused regression and diagnostic testing for the major defects fixed in this fork; and
- released from a clearly identified source commit.

This is an independently maintained continuation of the GoldenEye Native project. It is not an
official release from the original upstream maintainer.

## Planned release statement

When certification is complete, the release may use wording along these lines:

> GoldenEye Native 1.0 is the first stable release of this community-maintained fork. Every retail
> mission has been completed through human playtesting on Agent, Secret Agent, and 00 Agent on the
> certified release candidate, in addition to automated regression and targeted diagnostic testing.
> The release includes fixes for gameplay, rendering, animation, collision, input, mission-state,
> and stability defects documented in this repository.

Do not use that statement until `TESTING_1.0.md` is complete.

## Major repair areas targeted for 1.0

The 1.0 candidate is expected to include fixes and validation work in these areas:

- native collision and traversal;
- ladders and tank climbing behavior;
- Archives and diagonal geometry defects;
- enemy and third-person weapon rendering;
- reverse animation interpolation;
- Boris reload behavior;
- post-mission input state;
- Train mission defects;
- VTXSTORE handling;
- NPC walk/pistol animation state;
- file-select hitbox behavior;
- gunbarrel timing and cadence;
- renderer and GPU stability work;
- other focused fixes listed in `FIXES_1.0.md`.

The final release notes should include only fixes actually present in the tagged release commit.

## Release gates

1. Resolve the remaining reproducible Intel Iris Xe / Mesa / i915 GPU hang.
2. Rebuild the release candidate from the intended final source tree.
3. Run focused automated regression tests.
4. Complete human campaign validation on:
   - Agent
   - Secret Agent
   - 00 Agent
5. Record the exact release commit, platform, renderer, and test environment in `TESTING_1.0.md`.
6. Review `FIXES_1.0.md` against the final tree and remove anything not actually included.
7. Tag the final commit as `v1.0.0`.
8. Publish the GitHub Release from that tag.

## Scope and support

The 1.0 certification statement should describe the platforms and hardware actually tested. It should
not be read as a claim that the port is bug-free or that every GPU, driver, operating system, or
optional mod configuration has been validated.

## Upstream relationship

This repository preserves the history and work of the project it descends from. Development in this
fork may proceed independently when useful. Individual fixes remain available for upstream
maintainers or other forks to review, adapt, or cherry-pick under the applicable project licenses.
