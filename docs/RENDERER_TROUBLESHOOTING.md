# Advanced renderer and GPU troubleshooting

GoldenEye Native 1.0 contains an opt-in renderer-forensics subsystem built during investigation of
an intermittent Intel/i915 whole-desktop GPU hang. Normal play does not enable it. The tools are
kept in the release so a future renderer failure can be correlated from the game's Fast3D command
stream all the way down to the hardware batch without rebuilding a one-off diagnostic port.

## What the subsystem records

`GETV_GPUFLIGHT=<path>` enables a rolling mmap-backed GPU submission recorder immediately before
OpenGL submission. Each record includes a monotonically increasing draw serial, frame and draw
ordinal, vertex/triangle counts, VBO size/hash, shader and GL program, texture IDs/content hashes,
depth/blend/decal state, viewport/scissor, current Fast3D command and display-list stack, and the
registered GoldenEye producer when one is known.

`GETV_GPUFLIGHT_PAYLOAD_VERTS=<n>` retains exact VBO bytes only for draws with the chosen vertex
count. This keeps the normal recorder bounded while allowing a hardware primitive with a known
vertex count to be compared byte-for-byte with the application's submitted data.

`GETV_FUNFLIGHT=<path>` enables a second rolling mmap-backed ring around the renderer call chain.
It records entry/exit or before/after events for `gfx_run`, nested display-list execution,
triangle emission, flush reasons, `gfx_flush`, `gfx_opengl_draw_triangles`, `glBufferData` and
`glDrawArrays`. GPU-link events join a function batch to the corresponding GPU-flight serial.

`GETV_STALLTRACE=1` enables the low-frequency stall watchdog. For a sustained renderer stall, the
watchdog freezes the function-flight ring exactly once. This is essential for recoverable GPU
hangs: without the freeze, normal rendering after recovery can overwrite the function history that
preceded the hang.

All of these are **off by default**. Flight paths must be supplied explicitly and the stall
watchdog must be explicitly enabled. The Perfect Dark-derived compatibility path described below is
also off by default and is enabled separately with `pd_renderer = 1` or the launcher checkbox.

## Companion tools

The 1.0 source tree includes:

- `tools/decode_gpu_flight.py` — bounded textual view of the GPU submission ring and optional
  private extraction of retained payloads;
- `tools/decode_function_flight.py` — renderer call/batch decoding and GPU-serial lookup;
- `tools/map_gpu_hang_draw.py` — maps a decoded i915 active batch back onto the most likely
  GoldenEye draw sequence;
- `tools/analyze_frozen_gpu_hang.py` — performs the same correlation when the capture is taken
  while the GPU is still wedged and the function ring is frozen;
- `tools/capture_gpu_hang.sh` — copies the flight rings first, then captures process/GDB/kernel and
  i915 first-error evidence before killing the game; and
- `tools/run_gpu_hang_autocapture.sh` — unattended hard-hang workflow. It authorizes privileged
  capture before launching, waits for the function ring to freeze and for the i915 first-error
  state to name the same PID, then invokes the full capture helper automatically.

The unattended workflow exists because the Intel failure that motivated it froze the keyboard,
mouse and desktop together with GoldenEye. A diagnostic procedure that requires the player to open
a terminal while the GPU is wedged is not a diagnostic procedure that can work for that failure.

Raw `.bin` flight files are local engineering evidence. They may be large (hundreds of megabytes)
and can contain exact transient rendering buffers. Do not attach them to public issues or pull
requests. Decode them locally and publish only bounded, reviewed text that contains no game data.

## 1.0 Intel/i915 investigation

The release blocker presented as a complete desktop stall lasting about ten seconds. Kernel
evidence consistently identified the Intel render engine (`rcs0`) and the same hang family rather
than a GoldenEye CPU deadlock. The final investigation deliberately stopped trying to guess a
rendering fix and built a chain of evidence instead.

### 1. Establish the hardware failure

Fresh i915 first-error captures showed the active render batch stopped in a `3DPRIMITIVE` packet.
The kernel reported the recurring `GPU HANG: ecode 12:1:84dffffb` family with failed render-engine
reset/GuC recovery. GDB captures taken after recovery could find the game back in ordinary frame
pacing, so a CPU stack captured late was not evidence for the cause.

### 2. Record exact GoldenEye submissions

The GPU-flight recorder was added so each OpenGL triangle submission could be correlated by
sequence, vertex pitch, VBO extent, shader, textures and Fast3D command. Exact VBO payload capture
was added for selected vertex counts. Hardware batches could then be mapped to specific application
draw serials instead of assuming the last CPU submission was the draw the asynchronous GPU was
executing.

### 3. Add producer provenance

Display-list registration connected dynamic Fast3D lists back to GoldenEye producer classes such
as background-room primary geometry and model display lists. This gave useful context without
mistaking correlation for cause: for example, a hardware-mapped Control draw came from a room-80
background list, but suppressing that room did not prevent the stall.

### 4. Compare a Perfect Dark-derived backend profile

A controlled backend A/B was adapted from the MIT-licensed Perfect Dark PC port. GoldenEye kept its
own Fast3D frontend and display lists while the optional profile requested a GL 3.3 compatibility
context, emitted GLSL 1.30, used a dedicated VAO, selected `GL_RGBA8` texture storage and issued an
end-of-frame `glFlush()`. The hang still reproduced. That ruled out this shallow modernization as a
fix and prevented the investigation from confusing a different backend generation with a root
cause.

The profile remains in 1.0 as `pd_renderer = 1` / **Perfect Dark renderer compatibility path** for
future compatibility testing. It is default-off. Provenance is recorded in `THIRD_PARTY.md`.

### 5. Freeze the function history during the stall

The first function-flight attempt demonstrated an important failure mode in rolling diagnostics:
the game recovered from several ten-second stalls and continued rendering long enough to overwrite
the sub-second function history that preceded them. The recorder was changed so a sustained stall
freezes its ring permanently. The external unattended watcher was then added so the machine could
capture and terminate the game without keyboard or mouse input during the GPU wedge.

### 6. Correlate the live hardware batch to the C call chain

The decisive unattended capture froze the function ring while the process was still blocked in
the DRM synchronization wait, and i915's first-error state named the same GoldenEye PID. The active
hardware primitive matched GoldenEye frame 8390, draw 59, GPU serial 1187616 with about 99.48%
sequence agreement. The linked application chain was:

```text
gfx_run
  -> gfx_run_dl
  -> gfx_run_dl
  -> gfx_flush_reason
  -> gfx_flush
  -> gfx_opengl_draw_triangles
```

The key negative result was just as important: `glBufferData` returned, `glDrawArrays` returned,
and GoldenEye submitted dozens of later draws before the CPU reached the synchronization point that
waited for the already-wedged GPU. The target shader/program, texture pair, command position,
stride, draw shape and render state had also executed repeatedly in immediately preceding frames,
and that shader occurred extensively throughout the retained history. The evidence did not support
declaring room 80, one shader, one Fast3D triangle command or malformed CPU VBO data to be the root
cause.

### 7. A/B the Intel submission mechanism

With the game held constant, the affected Ubuntu HWE x86-64 Intel/i915 test system was booted
with the same kernel/driver combination and one controlled change:

```text
i915.enable_guc=0
```

Boot verification confirmed the Intel GPU was bound to `i915`, the kernel command line
contained the option, and the driver accepted it. A clean GoldenEye build was then played across
several levels without reproducing the previously recurring stalls.

For this tested Linux/Intel environment, the whole-system GPU-stall issue is therefore marked
**fixed by the i915 GuC-disable workaround**. This is intentionally not described as a GoldenEye
source-code root-cause fix. A future kernel/driver may be retested with the default GuC policy.

## Using the subsystem on another hard hang

For an Intel/i915 hard hang, prefer `run_gpu_hang_autocapture.sh` so sudo is authorized while the
desktop is responsive and the capture is automatic. For a responsive desktop, `capture_gpu_hang.sh`
can be run manually. In either case, analyze the preserved copies rather than the live rings.

The useful proof chain is:

```text
i915 active batch / BBADDR
        -> hardware primitive and buffer binding
        -> GPU-flight draw serial
        -> Fast3D command and GoldenEye producer
        -> function-flight batch and C call chain
        -> API before/after boundary
```

Do not jump directly from a producer label or room ID to a game fix. A GPU is asynchronous: the
draw active when the hardware wedges may be many submissions behind the CPU, and the API call where
the CPU finally blocks may only be the first synchronization point after the real failure.
