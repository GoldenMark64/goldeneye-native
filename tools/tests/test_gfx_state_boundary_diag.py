#!/usr/bin/env python3
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "getv/port/fast3d/gfx_pc.c"
PATCH = ROOT / "getv/patches/thirdparty/0003-gfx-state-boundary-diagnostic.patch"


def function_body(text: str, name: str) -> str:
    m = re.search(rf"\b{name}\s*\([^;]*?\)\s*\{{", text)
    if not m:
        raise AssertionError(f"missing function {name}")
    depth = 0
    for i in range(m.end() - 1, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return text[m.start(): i + 1]
    raise AssertionError(f"unterminated function {name}")


class GfxStateBoundaryDiagnostic(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.src = SRC.read_text()
        cls.patch = PATCH.read_text()

    def test_is_opt_in_by_exact_command_ordinal(self):
        fn = function_body(self.src, "ge_gfxboundary_target")
        self.assertIn('getenv("GETV_GFXBOUNDARY_CMD")', fn)
        dump = function_body(self.src, "ge_gfxboundary_dump")
        self.assertIn("ge_stall_gfx_cmd_progress != target", dump)
        self.assertIn("buf_vbo_len == 0", dump)

    def test_diagnostic_does_not_add_gpu_sync(self):
        dump = function_body(self.src, "ge_gfxboundary_dump")
        flush = function_body(self.src, "gfx_flush_reason")
        self.assertNotIn("glFinish", dump)
        self.assertNotIn("glFinish", flush)
        self.assertEqual(flush.count("gfx_flush();"), 1)

    def test_tri_path_flushes_are_semantically_tagged(self):
        tri = function_body(self.src, "gfx_sp_tri1")
        reasons = (
            "depth_test", "depth_write", "decal", "cloud", "viewport", "scissor",
            "shader", "alpha_blend", "texture_import0", "texture_import1",
            "sampler0", "sampler1", "batch_full",
        )
        for reason in reasons:
            self.assertIn(f'"{reason}"', tri)
        self.assertNotIn("\n        gfx_flush();", tri)

    def test_state_dump_covers_renderer_and_rdp_boundary(self):
        dump = function_body(self.src, "ge_gfxboundary_dump")
        for token in (
            "rdp.other_mode_h", "rdp.other_mode_l", "rdp.combine_mode",
            "rsp.geometry_mode", "rendering_state.depth_test",
            "rendering_state.depth_mask", "rendering_state.shader_program",
            "rdp.loaded_texture", "rdp.texture_tile", "rendering_state.textures",
            "buf_vbo_num_tris", "buf_vbo_len",
        ):
            self.assertIn(token, dump)

    def test_command_and_control_flow_rings_are_compactly_emitted(self):
        dump = function_body(self.src, "ge_gfxboundary_dump")
        self.assertIn("[getv][gfxboundary] ops:", dump)
        self.assertIn("GFX_TRACE_N", dump)
        self.assertIn("[getv][gfxboundary] dl:", dump)
        self.assertIn("GFX_DLTRACE_N", dump)

    def test_output_is_flushed_before_a_gpu_hang_can_block(self):
        dump = function_body(self.src, "ge_gfxboundary_dump")
        self.assertIn("fflush(stdout);", dump)

    def test_focused_overlay_carries_only_gfx_pc(self):
        self.assertIn("--- a/getv/port/fast3d/gfx_pc.c", self.patch)
        self.assertIn("+++ b/getv/port/fast3d/gfx_pc.c", self.patch)
        self.assertNotIn("gfx_opengl.c", self.patch)
        self.assertIn("GETV_GFXBOUNDARY_CMD", self.patch)

    def test_rare_tri4_handler_remains_intact(self):
        self.assertIn("case 0xB1:", self.src)
        self.assertIn("while (rem1 != 0)", self.src)
        self.assertIn("gfx_sp_tri1(a_, b_, c_);", self.src)


if __name__ == "__main__":
    unittest.main(verbosity=2)
