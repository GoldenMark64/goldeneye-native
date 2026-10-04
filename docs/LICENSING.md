# Licensing and provenance

**Last verified: October 4, 2026.**

This document records the current public-release boundary and the provenance questions that still
matter. It is a factual engineering record, **not legal advice** and not a legal conclusion about
the project as a whole.

The short version is:

- the repository has a root MIT license inherited from the original GoldenEye Native project;
- that root license does **not** relicense third-party code, the GoldenEye game, the user's ROM,
  extracted assets, or separately licensed dependencies;
- the current public tree is designed to remain source-only and to contain no ROM-derived game
  data; and
- the unresolved sm64ex/Fast3D baseline files are **not redistributed by this repository** in the
  1.0 clean-install design. They are fetched from a pinned upstream and transformed locally.

For the exact fetched-file mechanism, see [`THIRD_PARTY.md`](THIRD_PARTY.md). For file-level
port provenance, see [`../getv/port/PROVENANCE.md`](../getv/port/PROVENANCE.md).

---

## 1. Root repository license

The repository currently contains a root [`LICENSE`](../LICENSE) with the MIT License and:

```text
Copyright (c) 2026 SegfaultEvan
```

That file was inherited from the original project.

The presence of a root MIT license should be read in the normal scoped way: it covers material for
which the relevant copyright holder can grant those terms. It does **not** convert independently
copyrighted third-party material to MIT and does not grant rights to GoldenEye 007 game content.

Where a dependency or adapted source carries its own license or notice, that separate provenance
controls that material.

## 2. Current public repository boundary

The tracked public tree is intended not to contain:

- a GoldenEye ROM;
- extracted ROM assets;
- `base.zip` or equivalent generated game-data archives;
- saves/EEPROM data;
- locally built playable GoldenEye executables; or
- the fetched third-party Fast3D/audio baseline files described below.

The build instead combines public project source/patches with material obtained locally by the
builder.

Repository checks such as `tools/check_no_game_data.py` and the patch/reconstruction checks are
engineering safeguards. They are not blanket legal conclusions.

Historical Git objects and third-party clones may reflect older project states. A clean current tree
should not be treated as a representation about every object that has ever existed in every clone
or remote ref.

## 3. GoldenEye game source and user-supplied data

The user supplies a supported GoldenEye 007 cartridge dump locally.

The game decompilation is obtained from `n64decomp/007` during setup and is not vendored as the
public repository's own source. That upstream repository has its own provenance situation,
including source derived from the original game and libultra material with Silicon Graphics
notices.

This project does not claim to relicense that material.

The user's ROM and locally extracted assets remain outside the public source release.

## 4. sm64ex / Fast3D / historical mixer baseline

The 1.0 public release no longer needs to redistribute the fifteen inherited sm64ex baseline files.

[`THIRD_PARTY.md`](THIRD_PARTY.md) records the current mechanism:

- upstream: `sm64pc/sm64ex`;
- pinned commit: `d7ca2c04364a6dd0dac58b47151e04e26887e6f0`;
- fetch script: `tools/fetch-thirdparty.sh`;
- exact file list: `getv/patches/thirdparty/MANIFEST`; and
- local GoldenEye changes: replayable patch/reconstruction steps under
  `getv/patches/thirdparty/`.

The reason for this boundary is deliberate. The Fast3D lineage reaches this project through sm64ex
and `Emill/n64-fast3d-engine`, whose licensing history is not cleanly represented by a standard
permissive license in the lineage this project inherited.

Rather than treating that ambiguity as solved, the public release avoids redistributing those
baseline files and fetches them from their upstream source during local setup.

The project does **not** relicense those fetched files.

## 5. Perfect Dark-derived work

A small optional renderer compatibility experiment was adapted from the MIT-licensed Perfect Dark
PC port. Its retained notice is:

```text
LICENSES/perfect-dark-port-MIT.txt
```

The adaptation is kept separately attributable and is opt-in. GoldenEye's own frontend/display
lists remain distinct from the borrowed backend ideas.

The current release also uses a deterministic mixer reconstruction path described in
[`THIRD_PARTY.md`](THIRD_PARTY.md); its provenance is recorded there rather than being inferred
from the root repository license.

## 6. Other tracked third-party components

Examples include:

| component | source / terms | treatment |
|---|---|---|
| `stb_image.h` | Sean Barrett, MIT or public domain/Unlicense | notice retained in-file |
| `stb_truetype.h` | Sean Barrett, MIT or public domain/Unlicense | notice retained in-file |
| Roboto Condensed | SIL Open Font License 1.1 | OFL notice retained with the font |
| SDL2 and other build dependencies | their upstream licenses | fetched/built according to setup tooling |

This table is a summary, not a substitute for the notices shipped with each component.

## 7. Sources that are not permitted for code reuse here

The project keeps a provenance quarantine so that useful reference material does not silently become
copied code.

Current examples include:

| project | reason |
|---|---|
| GoldenRecomp | GPL-3.0; outside this repository's permissive reuse policy |
| `cblock85/GoldenEye64Recomp` | GPL-3.0 |
| `chrissotraidis/goldenpad` | no clean top-level permissive license; documented GPL obligation |
| `DeeStiz/007` | no license granting reuse |

Those projects may be useful for understanding behavior, but code should not be copied or adapted
from them into this repository under the current policy.

[`REUSE_AUDIT.md`](REUSE_AUDIT.md) contains the broader reuse record.

## 8. Repository history and current public home

The original public repository was `SegfaultEvan/goldeneye-native`. It was deleted on
August 29, 2026.

A preservation/continuation existed at `seb-patron/goldeneye-native`, and the current public home
of this maintained continuation is:

```text
https://github.com/GoldenMark64/goldeneye-native
```

The current repository preserves the earlier public history while adding the GoldenMark64
continuation and 1.0 stabilization work.

Do not use an old hosting URL as evidence of current maintenance status.

## 9. Distribution policy

The current public 1.0 policy is conservative:

- publish source, patches, tests, documentation and reconstruction tooling;
- require users to supply their own supported ROM locally;
- fetch unresolved third-party baseline source from pinned upstream rather than republishing it;
- do not publish ROM-derived assets or a prebuilt playable GoldenEye binary; and
- keep platform packaging decisions separate from the source release.

A future change to binary/package distribution should trigger a fresh licensing/provenance review.
The root MIT file alone is not sufficient evidence for redistributing every input that participates
in a local build.

## 10. What to update when provenance changes

When a new external source is adapted, record:

1. repository/project name;
2. exact commit or version;
3. source file(s);
4. license/notice;
5. what was actually adapted; and
6. where the retained attribution lives.

Update this file, [`THIRD_PARTY.md`](THIRD_PARTY.md), and
[`../getv/port/PROVENANCE.md`](../getv/port/PROVENANCE.md) together when the change affects the
port layer.
