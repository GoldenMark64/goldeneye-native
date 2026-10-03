#!/usr/bin/env bash
# One-shot setup for a fresh macOS checkout: fetch-thirdparty, clone the decomp, apply
# 0001, generate assets from your ROM, namespace them, apply 0002, then build.
#
# This automates docs/SETUP.md sections 2-4 exactly, in order. It does not replace that
# document, read it if any step here fails, since it explains why each one exists.
#
# What it cannot do: SDL2 source and the ROM are yours to supply (README's "bring your
# own" rules). If either is missing this exits with the same instructions SETUP.md gives.
#
# usage: tools/setup-mac.sh [--skip-sdl]
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DECOMP="$HERE/vendor/ge-decomp"
SDLSRC="$HERE/deps/SDL2-2.30.9"
ROM="$HERE/roms/ge007.u.z64"
SKIP_SDL=0
[ "${1:-}" = "--skip-sdl" ] && SKIP_SDL=1

die() { echo "setup-mac: $*" >&2; exit 1; }
step() { echo; echo "== $* =="; }

# ---------------------------------------------------------------------- 1. third-party
step "third-party port sources"
if [ -f "$HERE/getv/port/fast3d/gfx_pc.c" ]; then
  echo "already present"
else
  "$HERE/tools/fetch-thirdparty.sh" fetch || die "fetch-thirdparty failed"
fi

# ---------------------------------------------------------------------- 2. SDL2 source
step "SDL2 2.30.9 source"
if [ -f "$SDLSRC/CMakeLists.txt" ]; then
  echo "already present at $SDLSRC"
elif [ "$SKIP_SDL" = "1" ]; then
  echo "skipped (--skip-sdl)"
else
  mkdir -p "$HERE/deps"
  URL="https://github.com/libsdl-org/SDL/releases/download/release-2.30.9/SDL2-2.30.9.tar.gz"
  echo "downloading $URL"
  curl -fL "$URL" -o "$HERE/deps/SDL2-2.30.9.tar.gz" \
    || die "download failed -- fetch it yourself per docs/SETUP.md section 2.3"
  tar -xzf "$HERE/deps/SDL2-2.30.9.tar.gz" -C "$HERE/deps" \
    || die "extract failed"
  rm -f "$HERE/deps/SDL2-2.30.9.tar.gz"
  [ -f "$SDLSRC/CMakeLists.txt" ] || die "extracted tree missing $SDLSRC/CMakeLists.txt"
fi

# ---------------------------------------------------------------------- 3. the decomp
step "decompiled game source"
if [ -d "$DECOMP/assets" ]; then
  echo "already cloned at $DECOMP"
else
  git clone https://github.com/n64decomp/007 "$DECOMP" || die "clone failed"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0001-source.patch" ) 2>/dev/null; then
  echo "0001-source.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0001-source.patch" ) \
    || die "0001-source.patch failed to apply -- checkout may not be pristine"
fi




if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0006-fov-live-setter.patch" ) 2>/dev/null; then
  echo "0006-fov-live-setter.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0006-fov-live-setter.patch" ) \
    || die "0006-fov-live-setter.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0007-load-trace.patch" ) 2>/dev/null; then
  echo "0007-load-trace.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0007-load-trace.patch" ) \
    || die "0007-load-trace.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0008-crosshair-color.patch" ) 2>/dev/null; then
  echo "0008-crosshair-color.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0008-crosshair-color.patch" ) \
    || die "0008-crosshair-color.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0009-freerun-divider.patch" ) 2>/dev/null; then
  echo "0009-freerun-divider.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0009-freerun-divider.patch" ) \
    || die "0009-freerun-divider.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0010-state-dump-player-position.patch" ) 2>/dev/null; then
  echo "0010-state-dump-player-position.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0010-state-dump-player-position.patch" ) \
    || die "0010-state-dump-player-position.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0011-netplay-tick-integration.patch" ) 2>/dev/null; then
  echo "0011-netplay-tick-integration.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0011-netplay-tick-integration.patch" ) \
    || die "0011-netplay-tick-integration.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0012-real-font-overlay.patch" ) 2>/dev/null; then
  echo "0012-real-font-overlay.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0012-real-font-overlay.patch" ) \
    || die "0012-real-font-overlay.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0013-lockstep-pinned-sim-step.patch" ) 2>/dev/null; then
  echo "0013-lockstep-pinned-sim-step.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0013-lockstep-pinned-sim-step.patch" ) \
    || die "0013-lockstep-pinned-sim-step.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0014-lockstep-cull-on-the-tick.patch" ) 2>/dev/null; then
  echo "0014-lockstep-cull-on-the-tick.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0014-lockstep-cull-on-the-tick.patch" ) \
    || die "0014-lockstep-cull-on-the-tick.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0015-aim-toggle.patch" ) 2>/dev/null; then
  echo "0015-aim-toggle.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0015-aim-toggle.patch" ) \
    || die "0015-aim-toggle.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0016-freecam.patch" ) 2>/dev/null; then
  echo "0016-freecam.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0016-freecam.patch" ) \
    || die "0016-freecam.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0017-coop-friendly-fire.patch" ) 2>/dev/null; then
  echo "0017-coop-friendly-fire.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0017-coop-friendly-fire.patch" ) \
    || die "0017-coop-friendly-fire.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0018-coop-one-death-is-not-the-team.patch" ) 2>/dev/null; then
  echo "0018-coop-one-death-is-not-the-team.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0018-coop-one-death-is-not-the-team.patch" ) \
    || die "0018-coop-one-death-is-not-the-team.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0019-coop-respawn.patch" ) 2>/dev/null; then
  echo "0019-coop-respawn.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0019-coop-respawn.patch" ) \
    || die "0019-coop-respawn.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0020-kill-selftest.patch" ) 2>/dev/null; then
  echo "0020-kill-selftest.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0020-kill-selftest.patch" ) \
    || die "0020-kill-selftest.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0021-stan-pointer-return-decls.patch" ) 2>/dev/null; then
  echo "0021-stan-pointer-return-decls.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0021-stan-pointer-return-decls.patch" ) \
    || die "0021-stan-pointer-return-decls.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0022-lockstep-stop-shuffling-every-frame.patch" ) 2>/dev/null; then
  echo "0022-lockstep-stop-shuffling-every-frame.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0022-lockstep-stop-shuffling-every-frame.patch" ) \
    || die "0022-lockstep-stop-shuffling-every-frame.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0023-enemy-gibs.patch" ) 2>/dev/null; then
  echo "0023-enemy-gibs.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0023-enemy-gibs.patch" ) \
    || die "0023-enemy-gibs.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0024-multi-ammo-endianness.patch" ) 2>/dev/null; then
  echo "0024-multi-ammo-endianness.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0024-multi-ammo-endianness.patch" ) \
    || die "0024-multi-ammo-endianness.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0025-cuff-native-pointer-stride.patch" ) 2>/dev/null; then
  echo "0025-cuff-native-pointer-stride.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0025-cuff-native-pointer-stride.patch" ) \
    || die "0025-cuff-native-pointer-stride.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0026-bloodier-gibs.patch" ) 2>/dev/null; then
  echo "0026-bloodier-gibs.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0026-bloodier-gibs.patch" ) \
    || die "0026-bloodier-gibs.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0027-external-rom-path.patch" ) 2>/dev/null; then
  echo "0027-external-rom-path.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0027-external-rom-path.patch" ) \
    || die "0027-external-rom-path.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0028-prop-allocator-telemetry.patch" ) 2>/dev/null; then
  echo "0028-prop-allocator-telemetry.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0028-prop-allocator-telemetry.patch" ) \
    || die "0028-prop-allocator-telemetry.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0029-modern-mouse-look.patch" ) 2>/dev/null; then
  echo "0029-modern-mouse-look.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0029-modern-mouse-look.patch" ) \
    || die "0029-modern-mouse-look.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0030-autocrouch-render-ticks.patch" ) 2>/dev/null; then
  echo "0030-autocrouch-render-ticks.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0030-autocrouch-render-ticks.patch" ) \
    || die "0030-autocrouch-render-ticks.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0031-native-joy-poll-handshake.patch" ) 2>/dev/null; then
  echo "0031-native-joy-poll-handshake.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0031-native-joy-poll-handshake.patch" ) \
    || die "0031-native-joy-poll-handshake.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0032-manual-reload.patch" ) 2>/dev/null; then
  echo "0032-manual-reload.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0032-manual-reload.patch" ) \
    || die "0032-manual-reload.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0033-guard-gun-sfx-native-layout.patch" ) 2>/dev/null; then
  echo "0033-guard-gun-sfx-native-layout.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0033-guard-gun-sfx-native-layout.patch" ) \
    || die "0033-guard-gun-sfx-native-layout.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0034-surface2-remote-mine-native-layout.patch" ) 2>/dev/null; then
  echo "0034-surface2-remote-mine-native-layout.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0034-surface2-remote-mine-native-layout.patch" ) \
    || die "0034-surface2-remote-mine-native-layout.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0035-gunbarrel-authored-cadence.patch" ) 2>/dev/null; then
  echo "0035-gunbarrel-authored-cadence.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0035-gunbarrel-authored-cadence.patch" ) \
    || die "0035-gunbarrel-authored-cadence.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0036-tank-mount-diagnostic.patch" ) 2>/dev/null; then
  echo "0036-tank-mount-diagnostic.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0036-tank-mount-diagnostic.patch" ) \
    || die "0036-tank-mount-diagnostic.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0037-file-select-hitbox-native-layout.patch" ) 2>/dev/null; then
  echo "0037-file-select-hitbox-native-layout.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0037-file-select-hitbox-native-layout.patch" ) \
    || die "0037-file-select-hitbox-native-layout.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0038-tank-contact-native-tolerance.patch" ) 2>/dev/null; then
  echo "0038-tank-contact-native-tolerance.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0038-tank-contact-native-tolerance.patch" ) \
    || die "0038-tank-contact-native-tolerance.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0039-tank-inner-diagnostic.patch" ) 2>/dev/null; then
  echo "0039-tank-inner-diagnostic.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0039-tank-inner-diagnostic.patch" ) \
    || die "0039-tank-inner-diagnostic.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0040-tank-native-object-state-alias.patch" ) 2>/dev/null; then
  echo "0040-tank-native-object-state-alias.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0040-tank-native-object-state-alias.patch" ) \
    || die "0040-tank-native-object-state-alias.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0041-archives-stall-watchdog-hooks.patch" ) 2>/dev/null; then
  echo "0041-archives-stall-watchdog-hooks.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0041-archives-stall-watchdog-hooks.patch" ) \
    || die "0041-archives-stall-watchdog-hooks.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0042-gunbarrel-native-cadence-split.patch" ) 2>/dev/null; then
  echo "0042-gunbarrel-native-cadence-split.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0042-gunbarrel-native-cadence-split.patch" ) \
    || die "0042-gunbarrel-native-cadence-split.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0043-gunbarrel-native-bond-resync.patch" ) 2>/dev/null; then
  echo "0043-gunbarrel-native-bond-resync.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0043-gunbarrel-native-bond-resync.patch" ) \
    || die "0043-gunbarrel-native-bond-resync.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0044-depot-timedmine-diagnostic.patch" ) 2>/dev/null; then
  echo "0044-depot-timedmine-diagnostic.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0044-depot-timedmine-diagnostic.patch" ) \
    || die "0044-depot-timedmine-diagnostic.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0045-proximity-box-stall-diagnostic.patch" ) 2>/dev/null; then
  echo "0045-proximity-box-stall-diagnostic.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0045-proximity-box-stall-diagnostic.patch" ) \
    || die "0045-proximity-box-stall-diagnostic.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0046-stan-room-buffer-native-terminator.patch" ) 2>/dev/null; then
  echo "0046-stan-room-buffer-native-terminator.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0046-stan-room-buffer-native-terminator.patch" ) \
    || die "0046-stan-room-buffer-native-terminator.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0047-stan-los-subphase-diagnostic.patch" ) 2>/dev/null; then
  echo "0047-stan-los-subphase-diagnostic.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0047-stan-los-subphase-diagnostic.patch" ) \
    || die "0047-stan-los-subphase-diagnostic.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0048-stan-los-deep-diagnostic.patch" ) 2>/dev/null; then
  echo "0048-stan-los-deep-diagnostic.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0048-stan-los-deep-diagnostic.patch" ) \
    || die "0048-stan-los-deep-diagnostic.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0049-native-collision-polygon-extent.patch" ) 2>/dev/null; then
  echo "0049-native-collision-polygon-extent.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0049-native-collision-polygon-extent.patch" ) \
    || die "0049-native-collision-polygon-extent.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0050-gfx-run-progress-diagnostic.patch" ) 2>/dev/null; then
  echo "0050-gfx-run-progress-diagnostic.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0050-gfx-run-progress-diagnostic.patch" ) \
    || die "0050-gfx-run-progress-diagnostic.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0051-gpu-sync-present-diagnostic.patch" ) 2>/dev/null; then
  echo "0051-gpu-sync-present-diagnostic.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0051-gpu-sync-present-diagnostic.patch" ) \
    || die "0051-gpu-sync-present-diagnostic.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0052-draw-sync-attribution-diagnostic.patch" ) 2>/dev/null; then
  echo "0052-draw-sync-attribution-diagnostic.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0052-draw-sync-attribution-diagnostic.patch" ) \
    || die "0052-draw-sync-attribution-diagnostic.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0054-native-ladder-locus-record.patch" ) 2>/dev/null; then
  echo "0054-native-ladder-locus-record.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0054-native-ladder-locus-record.patch" ) \
    || die "0054-native-ladder-locus-record.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0055-stan-recovery-safety.patch" ) 2>/dev/null; then
  echo "0055-stan-recovery-safety.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0055-stan-recovery-safety.patch" ) \
    || die "0055-stan-recovery-safety.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0056-stan-ground-cylinder-recovery.patch" ) 2>/dev/null; then
  echo "0056-stan-ground-cylinder-recovery.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0056-stan-ground-cylinder-recovery.patch" ) \
    || die "0056-stan-ground-cylinder-recovery.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0057-native-reverse-animation-interpolation.patch" ) 2>/dev/null; then
  echo "0057-native-reverse-animation-interpolation.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0057-native-reverse-animation-interpolation.patch" ) \
    || die "0057-native-reverse-animation-interpolation.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0058-bunker2-cctv-objective-diagnostic.patch" ) 2>/dev/null; then
  echo "0058-bunker2-cctv-objective-diagnostic.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0058-bunker2-cctv-objective-diagnostic.patch" ) \
    || die "0058-bunker2-cctv-objective-diagnostic.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0059-gunbarrel-bond-speed-calibration.patch" ) 2>/dev/null; then
  echo "0059-gunbarrel-bond-speed-calibration.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0059-gunbarrel-bond-speed-calibration.patch" ) \
    || die "0059-gunbarrel-bond-speed-calibration.patch failed to apply"
fi

if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0060-gpu-render-source-provenance.patch" ) 2>/dev/null; then
  echo "0060-gpu-render-source-provenance.patch: already applied"
else
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0060-gpu-render-source-provenance.patch" ) \
    || die "0060-gpu-render-source-provenance.patch failed to apply"
fi

# The ROM's SHA-1, from whichever tool this machine actually has.
#
# sha1sum on its own is not enough. Under the setup wizard, which runs this script with its output
# on a pipe, MSYS coreutils can fail with "failed to set file descriptor text/binary mode: Bad
# file descriptor" and produce nothing on stdout while still exiting through the pipeline. The
# check below then compared an EMPTY string against the expected hash and reported the ROM as
# wrong -- the user is told to find a different dump when the dump was never read. Reported as
# issue #6 by three people, all with correct ROMs.
#
# python3 is already a checked prerequisite of this script, so it is the reliable last resort
# rather than an extra dependency.
ge_rom_sha1() {
  if command -v sha1sum >/dev/null 2>&1; then
    sha1sum "$1" 2>/dev/null | awk '{print $1}'
  elif command -v shasum >/dev/null 2>&1; then
    shasum -a 1 "$1" 2>/dev/null | awk '{print $1}'
  elif command -v openssl >/dev/null 2>&1; then
    openssl dgst -sha1 "$1" 2>/dev/null | awk '{print $NF}'
  fi
}

ge_rom_sha1_py() {
  python3 -c 'import hashlib,sys
h=hashlib.sha1()
with open(sys.argv[1],"rb") as f:
    for b in iter(lambda: f.read(1<<20), b""): h.update(b)
print(h.hexdigest())' "$1" 2>/dev/null
}

# ---------------------------------------------------------------------- 4. the ROM
step "ROM"
[ -f "$ROM" ] || die "no ROM at $ROM -- see README.md 'Bring your own ROM'. Not something this script can fetch for you."
SHA="$(ge_rom_sha1 "$ROM")"
# Empty means the hashing tool failed, not that the ROM is wrong. Fall back before judging it.
if [ -z "$SHA" ]; then
  SHA="$(ge_rom_sha1_py "$ROM")"
fi
[ -n "$SHA" ] || die "could not compute the ROM's SHA-1 on this machine -- no working sha1sum, shasum, openssl or python3. The ROM itself has not been checked and may well be fine."
WANT="abe01e4aeb033b6c0836819f549c791b26cfde83"
[ "$SHA" = "$WANT" ] || die "ROM SHA-1 $SHA does not match $WANT -- see docs/SETUP.md 3.4"
ln -sf "../../roms/ge007.u.z64" "$DECOMP/baserom.u.z64"
echo "ROM ok, symlinked into $DECOMP/baserom.u.z64"

# ---------------------------------------------------------------------- 5. asset pipeline
step "asset generation (docs/SETUP.md 3.5)"
if [ -f "$DECOMP/assets/ge_animation_offsets.h" ]; then
  echo "already generated"
else
  (
    cd "$DECOMP"
    python3 "$HERE/tools/enable_bg_extraction.py"
    bash scripts/extract_baserom.u.sh
    python3 scripts/generate_chr_c.py
    python3 scripts/generate_gun_c.py
    python3 scripts/generate_prop_model_c.py
    python3 "$HERE/tools/gen_obseg_blobs.py"
    python3 scripts/make/sync_imagelist_with_def.py build/imagelist.csv
    bash scripts/make/combine_images_named.sh build/imagelist.csv assets/images/combined
    python3 "$HERE/tools/gen_images_segment.py"
    python3 "$HERE/tools/fix_asset_switchnodes.py"
    python3 "$HERE/tools/gen_anim_blobs.py"
    python3 "$HERE/tools/gen_audio_segment.py"
    python3 "$HERE/tools/gen_asset_fileview.py"
    python3 tools/gen_propdef_layout.py
  ) || die "asset generation failed -- rerun by hand per docs/SETUP.md 3.5 to see which step"
fi

# The same count check tools/setup-windows.sh and tools/install.sh make, for the same reason: the
# chr, gun and prop generators report per model and can stop partway without failing the block
# they run in. That was found on Windows, where all three died on their first file and the
# install still linked cleanly and produced a complete-looking binary missing two thirds of its
# assets. Nothing about the failure mode is Windows-specific -- an interrupted run or an
# unreadable ROM would do it here too -- and the check costs three globs.
for _spec in "chr:80" "gun:92" "prop:340"; do
  _d="${_spec%%:*}"; _want="${_spec##*:}"
  _have="$(ls -1 "$DECOMP/assets/obseg/$_d/"*/Model.c 2>/dev/null | wc -l | tr -d ' ')"
  [ "$_have" -ge "$_want" ] || die "only $_have of $_want $_d models exist -- the generator stopped early. Delete $DECOMP/assets/ge_animation_offsets.h and re-run, and read the log above for the first traceback."
done
echo "models: $(ls -1 "$DECOMP/assets/obseg/chr/"*/Model.c 2>/dev/null | wc -l | tr -d ' ') chr, $(ls -1 "$DECOMP/assets/obseg/gun/"*/Model.c 2>/dev/null | wc -l | tr -d ' ') gun, $(ls -1 "$DECOMP/assets/obseg/prop/"*/Model.c 2>/dev/null | wc -l | tr -d ' ') prop"

# ---------------------------------------------------------------------- 6. namespacing + local transform + 0002
step "symbol namespacing (docs/SETUP.md 3.6)"
python3 "$HERE/tools/transform_rarewarelogo.py" || die "local Rareware logo transform failed"
if ( cd "$DECOMP" && git apply --reverse --check "$HERE/getv/patches/0002-assets.patch" ) 2>/dev/null; then
  echo "already namespaced and patched"
else
  (
    cd "$DECOMP"
    python3 "$HERE/tools/uniquify_asset_symbols.py" assets/obseg/chr   --recurse
    python3 "$HERE/tools/uniquify_asset_symbols.py" assets/obseg/gun   --recurse
    python3 "$HERE/tools/uniquify_asset_symbols.py" assets/obseg/prop  --recurse
    python3 "$HERE/tools/uniquify_asset_symbols.py" assets/obseg/setup
    python3 "$HERE/tools/uniquify_asset_symbols.py" assets/obseg/setup/u
    python3 "$HERE/tools/uniquify_asset_symbols.py" assets/obseg/stan
  ) || die "namespacing failed"
  ( cd "$DECOMP" && git apply "$HERE/getv/patches/0002-assets.patch" ) \
    || die "0002-assets.patch failed to apply"
fi

# ---------------------------------------------------------------------- 7. build
step "build"
cd "$HERE/getv"
[ "$SKIP_SDL" = "1" ] || ./build_mac.sh sdl || die "SDL2 build failed"
./build_mac.sh all || die "build failed"

echo
echo "setup-mac: done -- run ./getv/build_mac.sh run"
