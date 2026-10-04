# Provenance of `getv/port/`

**Current as of October 4, 2026.**

This is the file-level companion to [`docs/LICENSING.md`](../../docs/LICENSING.md) and
[`docs/THIRD_PARTY.md`](../../docs/THIRD_PARTY.md). It is a factual engineering record, not legal
advice.

## 1. Publicly tracked port code

The current repository tracks the GoldenEye-specific port layer, build glue, tests, and patch/
reconstruction instructions.

Project-specific examples include:

- `src/` platform/game integration;
- `mac/` platform integration;
- GoldenEye-specific renderer hooks such as `fast3d/ge_sky_rdp.{c,h}`;
- launcher/configuration integration;
- diagnostics, GPU/function-flight instrumentation, and regression tooling; and
- the patch stacks that transform pinned upstream/decomp sources locally.

The repository's root MIT license does not erase separate notices on adapted or third-party
material.

## 2. Fifteen sm64ex baseline files are fetched, not redistributed

The 1.0 clean-install design deliberately does **not** keep the inherited sm64ex Fast3D/audio
baseline as ordinary tracked source files.

`tools/fetch-thirdparty.sh` fetches fifteen files from pinned sm64ex commit:

```text
d7ca2c04364a6dd0dac58b47151e04e26887e6f0
```

The exact list lives in:

```text
getv/patches/thirdparty/MANIFEST
```

It includes the Fast3D renderer baseline, historical mixer inputs, `configfile.h`, and `fs.h`.

GoldenEye-specific changes are reconstructed locally from the public patch/template material. See
[`docs/THIRD_PARTY.md`](../../docs/THIRD_PARTY.md) for the exact pipeline and verification
mechanism.

## 3. Why the Fast3D lineage stays separated

The relevant lineage is:

```text
Emill/n64-fast3d-engine -> sm64ex -> GoldenEye Native local reconstruction
```

The form of the notice inherited through sm64ex is not treated by this project as a settled
standard permissive license. Rather than assert that the question is resolved, the public release
does not redistribute those baseline files.

This is a distribution boundary, not a claim that another project's legal interpretation is wrong.

## 4. Perfect Dark-derived compatibility work

The optional Perfect Dark renderer compatibility experiment adapts a small set of backend ideas from
`perfect-dark-pc-port/perfect_dark`, which is MIT-licensed.

The retained notice is:

```text
LICENSES/perfect-dark-port-MIT.txt
```

The experiment is opt-in and does not replace GoldenEye's Fast3D frontend or game code.

The current mixer reconstruction/provenance details are documented in
[`docs/THIRD_PARTY.md`](../../docs/THIRD_PARTY.md).

## 5. Other third-party components

Third-party headers/fonts keep their own notices. Examples include stb_image, stb_truetype, and
Roboto Condensed.

Do not infer their terms from the repository's root `LICENSE`.

## 6. Quarantine

Code is not to be copied/adapted from sources whose terms do not fit the current project policy.

Examples:

- GoldenRecomp — GPL-3.0;
- `cblock85/GoldenEye64Recomp` — GPL-3.0;
- `chrissotraidis/goldenpad` — no clean permissive top-level grant / documented GPL obligation;
- `DeeStiz/007` — no license granting reuse.

Reading for behavior/reference is separate from copying code.

## 7. Game data boundary

Never commit or publish:

- ROMs;
- extracted ROM assets;
- generated game-data archives;
- saves/EEPROM data; or
- locally built playable executables containing locally extracted game data.

The local build may necessarily combine user-supplied game data with code to produce a playable
binary. That does not make the resulting binary part of this source repository's redistributable
artifact set.

## 8. Recording future adaptations

Any new external adaptation must record:

- upstream repository;
- exact commit/version;
- source file(s);
- license;
- what was adapted; and
- retained notice location.

Update this file together with `docs/LICENSING.md` and `docs/THIRD_PARTY.md`.
