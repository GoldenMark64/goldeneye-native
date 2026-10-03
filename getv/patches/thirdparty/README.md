# `getv/patches/thirdparty/` - the port's changes to fetched sm64ex sources

Fifteen files under `getv/port/` are not in this repository. They are inherited from sm64ex,
whose Fast3D lineage carries an unresolved redistribution restriction, so they are fetched at
build time instead of being vendored. `docs/THIRD_PARTY.md` is the full account; this file is
the operational note.

This directory holds the manifest plus the ordered patch stack the repository keeps:

| file | what it is |
|---|---|
| `MANIFEST` | the fifteen upstream paths and where each one lands |
| `0001-getv-port-layer.patch` | historical GoldenEye-Native baseline over the pinned sm64ex files |
| `0002-baseline-catchup-1.0.patch` | small 1.0 catch-up overlay for renderer diagnostics/state seams that were previously folded into a locally regenerated baseline; kept separate so the published historical `0001` remains unchanged and passes the public-artifact guard |
| `0002-dynamic-texture-refresh.patch` | focused overlay that forces transient dynamic textures through re-upload while preserving their selected cache node |
| `0003-gfx-state-boundary-diagnostic.patch` | focused 0053 diagnostic overlay that tags Fast3D batch flush reasons and dumps bounded RDP/renderer state at an opt-in command ordinal |
| `0004-gpu-submission-flight-recorder.patch` | focused 0055 diagnostic overlay that records every OpenGL triangle submission into a kill-safe shared-memory flight ring, including Fast3D command provenance, texture hashes/state, and exact VBO payloads for a selected vertex count |
| `0005-gpu-flight-source-provenance.patch` | extends the flight recorder's Fast3D attribution with the exact current command pointer plus a bounded snapshot of the active display-list call/branch stack, so an i915 hardware batch can be walked back toward the GoldenEye display-list producer that generated it |
| `0006-perfect-dark-renderer-ab.patch` | opt-in `GETV_PD_RENDERER=1` A/B profile adapted from Perfect Dark PC port `514bf7a`: modern desktop GL context, GLSL 1.30 qualifiers/output, dedicated VAO, explicit RGBA8 texture storage, and PD-style end-frame `glFlush`, while retaining GoldenEye's incompatible Fast3D/vertex frontend |
| `0007-function-submission-flight-recorder.patch` | opt-in `GETV_FUNFLIGHT=<path>` renderer call-chain overlay that links Fast3D function/batch events and exact GL upload/draw markers to the existing GPU-flight submission serial without adding GPU synchronization |

## Get the files

```bash
tools/fetch-thirdparty.sh            # clone at the pin, copy, patch
tools/fetch-thirdparty.sh status     # show the pin and which files are present
tools/fetch-thirdparty.sh verify     # re-derive from pin + patch, compare byte for byte
tools/fetch-thirdparty.sh clean      # remove them again
```

Upstream is `https://github.com/sm64pc/sm64ex` pinned at
`d7ca2c04364a6dd0dac58b47151e04e26887e6f0`.

If the machine is offline or you want to use a mirror, point `GETV_SM64EX_REPO` at any clone
that contains that commit and no network access is attempted.

## After editing any of the fifteen files

> **The patch is the only place your change is recorded.** The files themselves are gitignored.
> Editing one and not regenerating loses the work on the next `clean` or fresh checkout.

```bash
tools/fetch-thirdparty.sh regen
```

`regen` rewrites `0001-getv-port-layer.patch` from the current working tree after reversing
the focused overlays, then runs `verify` on the complete stack. A successful run is therefore
proof that the baseline plus overlays reproduce the working files byte-for-byte.

This is the same hazard `getv/patches/README.md` describes for `vendor/ge-decomp`, and it has
the same fix: refresh the patch before any commit that touches the code it covers.

## Zero context

The patch is generated with `diff -U0`. It applies to exactly one upstream commit, so there is
nothing for context lines to disambiguate, and omitting them keeps unmodified upstream text out
of a file this repository distributes. The consequence is that the patch will not apply to any
other version of sm64ex, which is intended - a hunk that silently fuzzes into place against a
different upstream is worse than a refusal.

## What is not covered here

`getv/port/fast3d/ge_sky_rdp.c` and `ge_sky_rdp.h` are this project's own work - they decode
GoldenEye's hand-assembled RDP triangle commands, which sm64 never emits - and are tracked
normally in the repository. They are deliberately absent from `MANIFEST`.
