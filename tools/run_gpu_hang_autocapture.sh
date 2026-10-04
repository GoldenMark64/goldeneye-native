#!/usr/bin/env bash
set -uo pipefail

# Launch one GoldenEye GPU-hang forensic run that requires no keyboard/mouse
# interaction once the game starts.  The in-process watchdog freezes the
# function-flight ring on a sustained stall.  This external watcher then waits
# for i915 to publish a first-error state for the same PID before invoking the
# normal capture helper and killing the game.

if [[ $# -lt 1 ]]; then
    echo "usage: $0 /path/to/goldeneye-binary"
    exit 2
fi

BIN="$(readlink -f "$1")"
if [[ ! -x "$BIN" ]]; then
    echo "not executable: $BIN"
    exit 2
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CAPTURE_HELPER="${GETV_HANG_CAPTURE_HELPER:-$SCRIPT_DIR/capture_gpu_hang.sh}"
if [[ ! -x "$CAPTURE_HELPER" && -x "$SCRIPT_DIR/kill-goldeneye.sh" ]]; then
    CAPTURE_HELPER="$SCRIPT_DIR/kill-goldeneye.sh"
fi
if [[ ! -x "$CAPTURE_HELPER" ]]; then
    echo "capture helper not executable: $CAPTURE_HELPER"
    exit 2
fi

BASE="${GETV_PLAYTEST_BASE:-$HOME/GoldenEye-Playtests/$(date +%Y%m%d)}"
STAMP="$(date +%Y%m%d-%H%M%S)"
GPU="$BASE/gpu-flight-auto-$STAMP.bin"
FUNC="$BASE/function-flight-auto-$STAMP.bin"
LOG="$BASE/playtest-auto-$STAMP.log"
AUTOLOG="$BASE/autocapture-$STAMP.log"
mkdir -p "$BASE"

echo '============================================================'
echo ' GOLDENEYE — UNATTENDED GPU HANG CAPTURE'
echo '============================================================'
echo "Binary:   $BIN"
echo "GPU:      $GPU"
echo "Function: $FUNC"
echo "Game log: $LOG"
echo "Auto log: $AUTOLOG"
echo

# Authenticate while the desktop is responsive.  Everything after launch uses
# sudo -n so the capture path can never block on a password prompt during a hang.
echo 'Authorizing privileged capture before launch...'
sudo -v || exit 1

ERRNODE=""
for p in /sys/class/drm/card*/error /sys/kernel/debug/dri/*/i915_error_state; do
    if sudo -n test -e "$p"; then
        ERRNODE="$p"
        break
    fi
done

if [[ -n "$ERRNODE" ]]; then
    # Previous incidents are already archived by the capture helper.  Start this
    # run with a clean first-error slot so PID matching is unambiguous.
    printf '1\n' | sudo -n tee "$ERRNODE" >/dev/null 2>&1 || true
    echo "i915 error node: $ERRNODE"
else
    echo 'WARNING: no i915 error-state node found; timeout capture will still run.'
fi

# Keep the already-authorized sudo timestamp warm.  This loop never prompts.
(
    while :; do
        sudo -n -v >/dev/null 2>&1 || exit 0
        sleep 30
    done
) &
SUDO_KEEPALIVE=$!

cleanup() {
    kill "$SUDO_KEEPALIVE" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

echo
echo 'Launching GoldenEye. No keyboard interaction is required after this point.'

env \
  -u GETV_PD_RENDERER \
  -u GETV_GLSYNC_DIAG \
  -u GETV_DRAWDIAG \
  -u GETV_DRAWDIAG_SYNC \
  INTEL_DEBUG=capture-all \
  GETV_GPUFLIGHT="$GPU" \
  GETV_GPUFLIGHT_PAYLOAD_VERTS=12 \
  GETV_FUNFLIGHT="$FUNC" \
  GETV_STALLTRACE=1 \
  GETV_LAUNCHER=0 \
  "$BIN" >"$LOG" 2>&1 &
GAME_PID=$!

echo "PID=$GAME_PID"
echo 'Waiting for sustained-stall freeze...'

FROZEN=0
while kill -0 "$GAME_PID" 2>/dev/null; do
    if grep -Fq '[getv][funflight] FROZEN on detected stall' "$LOG" 2>/dev/null; then
        FROZEN=1
        break
    fi
    sleep 0.10
done

if [[ "$FROZEN" -ne 1 ]]; then
    echo 'GoldenEye exited before the function-flight recorder froze.'
    wait "$GAME_PID" 2>/dev/null || true
    exit 0
fi

echo 'SUSTAINED STALL DETECTED — function-flight evidence is frozen.' | tee -a "$AUTOLOG"

# Give i915 up to 15 seconds to publish the first-error state for this exact
# process.  The observed hangs recover at about 10 seconds.  If the kernel never
# publishes one, capture anyway at the deadline; the frozen function trace is
# still preserved and the process snapshot remains useful.
MATCHED=0
for _ in $(seq 1 60); do
    if ! kill -0 "$GAME_PID" 2>/dev/null; then
        break
    fi
    if [[ -n "$ERRNODE" ]]; then
        FIRST="$(sudo -n head -n 1 "$ERRNODE" 2>/dev/null || true)"
        if [[ "$FIRST" == *"[$GAME_PID]"* ]]; then
            MATCHED=1
            echo "i915 first-error matched PID $GAME_PID" | tee -a "$AUTOLOG"
            break
        fi
    fi
    sleep 0.25
done

if [[ "$MATCHED" -ne 1 ]]; then
    echo 'i915 PID match not observed before deadline; capturing anyway.' | tee -a "$AUTOLOG"
fi

echo 'Running full capture helper automatically...' | tee -a "$AUTOLOG"
"$CAPTURE_HELPER" >>"$AUTOLOG" 2>&1 || true

echo
echo '============================================================'
echo ' AUTOMATIC CAPTURE COMPLETE'
echo '============================================================'
cat "$AUTOLOG"
