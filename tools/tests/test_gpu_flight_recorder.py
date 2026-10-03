#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
HEADER = ROOT / "getv/port/src/ge_gpu_flight.h"
DRAW_HEADER = ROOT / "getv/port/src/ge_draw_diag.h"
DECODER = ROOT / "tools/decode_gpu_flight.py"
CAPTURE_HELPER = ROOT / "tools/capture_gpu_hang.sh"
GFX_PC = ROOT / "getv/port/fast3d/gfx_pc.c"
GFX_PC_H = ROOT / "getv/port/fast3d/gfx_pc.h"
GFX_OPENGL = ROOT / "getv/port/fast3d/gfx_opengl.c"
PROVENANCE_PATCH = ROOT / "getv/patches/thirdparty/0005-gpu-flight-source-provenance.patch"

HARNESS = r'''
#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ge_gpu_flight.h"

int main(int argc, char **argv)
{
    struct GeDrawDiagState st;
    float small[6 * 4];
    float target[24 * 4];
    float final[3 * 4];

    if (argc != 2) return 90;
    if (setenv("GETV_GPUFLIGHT", argv[1], 1) != 0) return 91;
    if (setenv("GETV_GPUFLIGHT_PAYLOAD_VERTS", "24", 1) != 0) return 92;

    memset(&st, 0, sizeof(st));
    st.shader_id = UINT64_C(0x1122334455667788);
    st.program_id = 7;
    st.stride_floats = 4;
    st.num_attribs = 1;
    st.texture_id[0] = 3;
    st.texture_gl_id[0] = 55;
    st.texture_width[0] = 32;
    st.texture_height[0] = 16;
    st.texture_hash[0] = UINT64_C(0x1234567890abcdef);
    st.depth_test = 1;
    st.depth_write = 1;
    st.viewport[2] = 640;
    st.viewport[3] = 480;
    st.gfx_cmd_progress = 4565;
    st.gfx_dl_depth = 1;
    st.gfx_opcode = 0xb1;
    st.gfx_w0 = UINT64_C(0xb100000000000000);
    st.gfx_w1 = UINT64_C(0x0102030405060708);
    st.gfx_cmd_ptr = UINT64_C(0x00007fff33330120);
    st.gfx_dl_stack_base_depth = 2;
    st.gfx_dl_stack_count = 3;
    st.gfx_dl_stack[0] = UINT64_C(0x00007fff11110000);
    st.gfx_dl_stack[1] = UINT64_C(0x00007fff22220000);
    st.gfx_dl_stack[2] = UINT64_C(0x00007fff33330000);
    st.gfx_dl_callsite[0] = 0;
    st.gfx_dl_callsite[1] = UINT64_C(0x00007fff11110180);
    st.gfx_dl_callsite[2] = UINT64_C(0x00007fff22220240);
    st.gfx_source_kind = 2;
    st.gfx_source_id = 77;
    st.gfx_source_aux = 0x1234;
    st.gfx_source_owner = UINT64_C(0x00007fff44440000);
    memcpy(st.gfx_source_name, "bg.primary", 11);

    for (int i = 0; i < (int)(sizeof small / sizeof small[0]); i++) small[i] = (float)i;
    for (int i = 0; i < (int)(sizeof target / sizeof target[0]); i++) target[i] = 1000.0f + (float)i;
    for (int i = 0; i < (int)(sizeof final / sizeof final[0]); i++) final[i] = -100.0f - (float)i;

    geGpuFlightRecordShader(st.shader_id, st.program_id,
                            "#version 120\nvoid main(){gl_Position=vec4(0.0);}\n", 52,
                            "#version 120\nvoid main(){gl_FragColor=vec4(1.0);}\n", 54);

    geGpuFlightRecordDraw(10, small, sizeof small / sizeof small[0], 2, &st);
    geGpuFlightRecordDraw(10, target, sizeof target / sizeof target[0], 8, &st);
    geGpuFlightRecordDraw(11, final, sizeof final / sizeof final[0], 1, &st);

    /* Deliberately bypass cleanup/unmap/stdio teardown. The MAP_SHARED recorder must remain
     * readable after exactly the kind of SIGKILL the hang helper uses on GoldenEye. */
    kill(getpid(), SIGKILL);
    return 0;
}
'''


class GpuFlightRecorderTests(unittest.TestCase):
    def test_layout_constants_and_no_gpu_sync(self):
        text = HEADER.read_text()
        self.assertIn("GE_GPUFLIGHT_DRAW_SLOTS      524288u", text)
        self.assertIn("GE_GPUFLIGHT_PAYLOAD_SLOTS    16384u", text)
        self.assertIn("GE_GPUFLIGHT_DEFAULT_VERTS       24u", text)
        self.assertIn("MAP_SHARED", text)
        self.assertNotIn("glFinish", text)
        self.assertNotIn("glGetError", text)

        draw = DRAW_HEADER.read_text()
        for field in (
            "texture_hash[2]",
            "height_hash[2]",
            "texture_wrap_s[2]",
            "viewport[4]",
            "gfx_cmd_progress",
            "gfx_opcode",
            "gfx_w0",
            "gfx_cmd_ptr",
            "gfx_dl_stack[8]",
            "gfx_dl_callsite[8]",
            "gfx_source_kind",
            "gfx_source_name[32]",
        ):
            self.assertIn(field, draw)

    def test_capture_helper_preserves_flight_before_privileged_delay_and_kill(self):
        source = CAPTURE_HELPER.read_text()
        flight_copy = source.index('cp --reflink=auto "$FLIGHT"')
        sudo_prompt = source.index("sudo -v")
        sigkill = source.index('kill -9 "$PID"')
        self.assertLess(flight_copy, sudo_prompt)
        self.assertLess(sudo_prompt, sigkill)
        self.assertIn("i915_error_state", source)
        self.assertIn("intel_error_decode", source)
        self.assertIn("--show-shaders", source)
        self.assertIn("pgrep -n -x goldeneye", source)
        self.assertIn("goldeneye-[^[:space:]/]+", source)
        self.assertIn("GETV_GPUFLIGHT_PAYLOAD_VERTS", source)
        self.assertIn('--verts "$PAYLOAD_VERTS"', source)
        self.assertIn("--gaps-ms 50", source)
        self.assertIn("realtime_ns", source)
        self.assertIn("monotonic_ns", source)
        self.assertIn("offset_ns", source)
        self.assertIn("i915 first-error state is stale", source)
        self.assertIn("Cleared i915 first-error state", source)

    def test_renderer_carries_command_pointer_and_display_list_ancestry(self):
        pc = GFX_PC.read_text()
        pc_h = GFX_PC_H.read_text()
        ogl = GFX_OPENGL.read_text()
        overlay = PROVENANCE_PATCH.read_text()

        self.assertIn("static const void *ge_dl_stack[GE_DL_MAX_DEPTH]", pc)
        self.assertIn("*cmd_ptr = (uintptr_t)e->cmd", pc)
        self.assertIn("ge_dl_stack[0] = commands", pc)
        self.assertIn("ge_dl_stack[ge_dl_depth] = target", pc)
        self.assertIn("ge_dl_callsite[ge_dl_depth] = cmd", pc)
        self.assertIn("uint32_t *dl_stack_base_depth", pc_h)
        self.assertIn("uintptr_t *dl_stack", pc_h)
        self.assertIn("uintptr_t *dl_callsite", pc_h)
        self.assertIn("state.gfx_cmd_ptr = (uint64_t)gfx_cmd_ptr", ogl)
        self.assertIn("state.gfx_dl_stack_count = gfx_dl_stack_count", ogl)
        self.assertIn("state.gfx_dl_callsite[i] = (uint64_t)gfx_dl_callsite[i]", ogl)
        self.assertIn("uintptr_t *cmd_ptr", overlay)
        self.assertIn("uintptr_t *dl_stack, uintptr_t *dl_callsite", overlay)
        self.assertIn("ge_dl_stack[0] = commands", overlay)

    def test_sigkill_capture_decodes_and_keeps_exact_24_vertex_payload(self):
        cc = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        if not cc:
            raise unittest.SkipTest("C compiler required")

        spec = importlib.util.spec_from_file_location("decode_gpu_flight", DECODER)
        self.assertIsNotNone(spec)
        self.assertIsNotNone(spec.loader)
        decoder = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(decoder)

        with tempfile.TemporaryDirectory(dir=ROOT / "scratch") as td:
            td_path = Path(td)
            src = td_path / "flight_harness.c"
            exe = td_path / "flight_harness"
            capture = td_path / "flight.bin"
            outdir = td_path / "payloads"
            src.write_text(HARNESS)

            build = subprocess.run(
                [
                    cc,
                    "-std=c11",
                    "-O1",
                    "-D_POSIX_C_SOURCE=200809L",
                    "-I", str(ROOT / "getv/port/src"),
                    str(src),
                    "-o", str(exe),
                ],
                capture_output=True,
                text=True,
            )
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)

            run = subprocess.run([str(exe), str(capture)], capture_output=True, text=True)
            self.assertIn(run.returncode, (-signal.SIGKILL, 128 + signal.SIGKILL))
            self.assertTrue(capture.exists())
            self.assertGreater(capture.stat().st_size, 30 * 1024 * 1024)

            data = capture.read_bytes()
            header = decoder.read_header(data)
            self.assertEqual(header["total_draws"], 3)
            self.assertEqual(header["total_payloads"], 1)
            self.assertEqual(header["total_shaders"], 1)
            self.assertEqual(header["payload_vertex_filter"], 24)

            draws = decoder.retained_draws(data, header)
            self.assertEqual([d["verts"] for d in draws], [6, 24, 3])
            target = draws[1]
            self.assertEqual(target["gfx_cmd_progress"], 4565)
            self.assertEqual(target["gfx_opcode"], 0xB1)
            self.assertEqual(target["gfx_cmd_ptr"], 0x00007FFF33330120)
            self.assertEqual(target["gfx_dl_stack_base_depth"], 2)
            self.assertEqual(target["gfx_dl_stack_count"], 3)
            self.assertEqual(
                target["gfx_dl_stack"][:3],
                [0x00007FFF11110000, 0x00007FFF22220000, 0x00007FFF33330000],
            )
            self.assertEqual(
                target["gfx_dl_callsite"][:3],
                [0, 0x00007FFF11110180, 0x00007FFF22220240],
            )
            self.assertEqual(target["gfx_source_kind"], 2)
            self.assertEqual(target["gfx_source_id"], 77)
            self.assertEqual(target["gfx_source_aux"], 0x1234)
            self.assertEqual(target["gfx_source_owner"], 0x00007FFF44440000)
            self.assertEqual(target["gfx_source_name"], "bg.primary")
            self.assertEqual(target["payload_bytes"], 24 * 4 * 4)
            self.assertGreaterEqual(target["payload_slot"], 0)

            pserial, pbytes, ptruncated, payload = decoder.parse_payload(
                data, header, int(target["payload_slot"])
            )
            self.assertEqual(pserial, target["serial"])
            self.assertEqual(pbytes, 24 * 4 * 4)
            self.assertEqual(ptruncated, 0)
            self.assertEqual(len(payload), pbytes)

            report = subprocess.run(
                [
                    sys.executable,
                    str(DECODER),
                    str(capture),
                    "--last", "3",
                    "--verts", "24",
                    "--gaps-ms", "50",
                    "--show-shaders",
                    "--extract-payloads", str(outdir),
                ],
                capture_output=True,
                text=True,
            )
            self.assertEqual(report.returncode, 0, report.stdout + report.stderr)
            self.assertIn("total submissions: 3", report.stdout)
            self.assertIn("verts=24", report.stdout)
            self.assertIn("cmd=4565 op=0xb1", report.stdout)
            self.assertIn("cmdptr=00007fff33330120 cmdoff=+0x120", report.stdout)
            self.assertIn(
                "dl=2+3[00007fff11110000@0000000000000000>"
                "00007fff22220000@00007fff11110180>"
                "00007fff33330000@00007fff22220240]",
                report.stdout,
            )
            self.assertIn("src=bg.primary/k2/id77/aux0x1234/owner00007fff44440000", report.stdout)
            self.assertIn("mono_ns=", report.stdout)
            self.assertIn("retained monotonic range:", report.stdout)
            self.assertIn("retained frame range:", report.stdout)
            self.assertIn("SUBMISSION GAPS >= 50 ms", report.stdout)
            self.assertIn("SHADER 1122334455667788", report.stdout)
            payloads = list(outdir.glob("*.vbo.bin"))
            self.assertEqual(len(payloads), 1)
            self.assertEqual(payloads[0].read_bytes(), payload)


if __name__ == "__main__":
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(GpuFlightRecorderTests)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    if result.skipped or result.testsRun == 0:
        print("GPU flight recorder tests must execute without skips.", file=sys.stderr)
        sys.exit(1)
    sys.exit(0 if result.wasSuccessful() else 1)
