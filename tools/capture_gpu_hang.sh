#!/usr/bin/env bash
set -u

# Capture one GoldenEye GPU-hang incident before killing the process.
# Designed for GETV_GPUFLIGHT=<file> runs, optionally paired with GETV_FUNFLIGHT=<file>.
# The flight files are copied FIRST, before any sudo password prompt can delay the operator
# and allow recovered frames to overwrite either rolling ring.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT="${GETV_HANG_CAPTURE_DIR:-$HOME/GoldenEye-Playtests/$(date +%Y%m%d)/hang-captures}"
STAMP="$(date +%Y%m%d-%H%M%S)"
mkdir -p "$OUT"

PID="$(pgrep -n -x goldeneye 2>/dev/null || true)"
if [[ -z "$PID" ]]; then
    # Diagnostic/playtest binaries are intentionally kept under distinct names
    # (for example goldeneye-prov-0060). Match any executable basename beginning
    # with goldeneye- rather than only the older numeric goldeneye-123 form.
    PID="$(pgrep -n -f '(^|/)goldeneye-[^[:space:]/]+([[:space:]]|$)' 2>/dev/null || true)"
fi
if [[ -z "$PID" ]]; then
    echo "GoldenEye is not running"
    exit 0
fi

PREFIX="$OUT/$STAMP"

echo "=== GOLDENEYE GPU HANG CAPTURE ==="
echo "PID=$PID"
echo "STAMP=$STAMP"
echo "PREFIX=$PREFIX"

# ---------------------------------------------------------------- clock correlation
# Flight draw timestamps use CLOCK_MONOTONIC. Preserve a near-simultaneous realtime pair so
# kernel wall-clock GPU HANG lines can be mapped back to exact draw serials after capture.
python3 - <<'PY_CLOCK' > "$PREFIX-clock.txt" 2>/dev/null || true
import time
real_ns = time.time_ns()
mono_ns = time.monotonic_ns()
print(f"realtime_ns={real_ns}")
print(f"monotonic_ns={mono_ns}")
print(f"offset_ns={real_ns - mono_ns}")
PY_CLOCK
echo "Clock map: $PREFIX-clock.txt"

# ---------------------------------------------------------------- flight first
PROC_ENV="$(tr '\0' '\n' < "/proc/$PID/environ" 2>/dev/null || true)"
FLIGHT="$(printf '%s\n' "$PROC_ENV" | sed -n 's/^GETV_GPUFLIGHT=//p' | tail -n 1)"
FUNFLIGHT="$(printf '%s\n' "$PROC_ENV" | sed -n 's/^GETV_FUNFLIGHT=//p' | tail -n 1)"
PAYLOAD_VERTS="$(printf '%s\n' "$PROC_ENV" | sed -n 's/^GETV_GPUFLIGHT_PAYLOAD_VERTS=//p' | tail -n 1)"
if [[ -z "$PAYLOAD_VERTS" ]]; then
    PAYLOAD_VERTS=24
fi

echo
echo "=== GPU SUBMISSION FLIGHT FILE ==="
if [[ -n "$FLIGHT" && -f "$FLIGHT" ]]; then
    cp --reflink=auto "$FLIGHT" "$PREFIX-gpu-flight.bin" 2>/dev/null         || cp "$FLIGHT" "$PREFIX-gpu-flight.bin"
    echo "Saved: $PREFIX-gpu-flight.bin"

    DECODER="${GETV_GPUFLIGHT_DECODER:-$SCRIPT_DIR/decode_gpu_flight.py}"
    if [[ -f "$DECODER" ]]; then
        python3 "$DECODER" "$PREFIX-gpu-flight.bin" \
            --last 512 --verts "$PAYLOAD_VERTS" --gaps-ms 50 --show-shaders \
            > "$PREFIX-gpu-flight.txt" 2>&1 || true
        echo "Saved: $PREFIX-gpu-flight.txt"
    else
        echo "Decoder not found at: $DECODER"
    fi
else
    echo "No GETV_GPUFLIGHT file found in process environment"
fi

echo
echo "=== FUNCTION / SUBMISSION FLIGHT FILE ==="
if [[ -n "$FUNFLIGHT" && -f "$FUNFLIGHT" ]]; then
    cp --reflink=auto "$FUNFLIGHT" "$PREFIX-function-flight.bin" 2>/dev/null \
        || cp "$FUNFLIGHT" "$PREFIX-function-flight.bin"
    echo "Saved: $PREFIX-function-flight.bin"

    FUNDECODER="${GETV_FUNFLIGHT_DECODER:-$SCRIPT_DIR/decode_function_flight.py}"
    if [[ -f "$FUNDECODER" ]]; then
        python3 "$FUNDECODER" "$PREFIX-function-flight.bin" --last 2048 \
            > "$PREFIX-function-flight.txt" 2>&1 || true
        echo "Saved: $PREFIX-function-flight.txt"
    else
        echo "Decoder not found at: $FUNDECODER"
    fi
else
    echo "No GETV_FUNFLIGHT file found in process environment"
fi

# ---------------------------------------------------------------- process snapshot
echo
echo "=== PROCESS ==="
ps -o pid,ppid,stat,wchan:40,etime,comm,args -p "$PID"     | tee "$PREFIX-process.txt"

# ---------------------------------------------------------------- privileged captures
echo
echo "=== PRIVILEGED CAPTURE ==="
echo "You may be asked for sudo once."
sudo -v

echo
echo "=== THREAD BACKTRACES ==="
if command -v gdb >/dev/null 2>&1; then
    sudo gdb -q -batch         -ex 'set pagination off'         -ex 'thread apply all bt full'         -p "$PID"         > "$PREFIX-gdb.txt" 2>&1 || true
    echo "Saved: $PREFIX-gdb.txt"
else
    echo "gdb is not installed"
fi

echo
echo "=== KERNEL GPU EVENTS ==="
sudo journalctl -k -b --no-pager     | grep -Ei 'GPU HANG|Engine reset failed|context reset|reset request timed out|failed to reset engine|rcs0|goldeneye|GuC'     | tail -200     > "$PREFIX-kernel.txt"
echo "Saved: $PREFIX-kernel.txt"

# ---------------------------------------------------------------- fresh i915 crash state
echo
echo "=== I915 ERROR STATE ==="
ERRNODE=""
for p in /sys/class/drm/card*/error /sys/kernel/debug/dri/*/i915_error_state; do
    if sudo test -e "$p"; then
        ERRNODE="$p"
        break
    fi
done

if [[ -n "$ERRNODE" ]]; then
    echo "Node: $ERRNODE"
    sudo cat "$ERRNODE" > "$PREFIX-i915-error-state.txt" 2>&1 || true
    echo "Saved: $PREFIX-i915-error-state.txt"

    ERROR_PID="$(head -n 1 "$PREFIX-i915-error-state.txt" | awk 'match($0, /\[[0-9]+\]/) { print substr($0, RSTART + 1, RLENGTH - 2) }')"
    if [[ -n "$ERROR_PID" && "$ERROR_PID" != "$PID" ]]; then
        echo "WARNING: i915 first-error state is stale: captured PID=$ERROR_PID current PID=$PID"
    fi

    if command -v intel_error_decode >/dev/null 2>&1; then
        intel_error_decode "$PREFIX-i915-error-state.txt"             > "$PREFIX-i915-error-decoded.txt" 2>&1 || true
        echo "Saved: $PREFIX-i915-error-decoded.txt"
    fi
else
    echo "No i915 error-state node found"
fi

# i915 retains the first error until explicitly cleared. Evidence above is already safe on disk,
# so clear it now to guarantee the next run can capture its own first-error state.
if [[ -n "$ERRNODE" ]]; then
    if printf '1\n' | sudo tee "$ERRNODE" >/dev/null 2>&1; then
        echo "Cleared i915 first-error state for the next run"
    else
        echo "WARNING: could not clear i915 first-error state"
    fi
fi

# ---------------------------------------------------------------- kill only after evidence is safe
echo
echo "=== SIGKILL ==="
kill -9 "$PID" 2>/dev/null || true
sleep 1

if ps -p "$PID" >/dev/null 2>&1; then
    echo "WARNING: PID $PID still exists"
else
    echo "GoldenEye is gone"
fi

echo
echo "=== CAPTURE FILES ==="
ls -lh "$PREFIX"-* 2>/dev/null || true
echo
echo "Capture prefix:"
echo "$PREFIX"
