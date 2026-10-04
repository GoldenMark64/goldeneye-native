#!/usr/bin/env python3
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
OGL = ROOT / "getv/port/fast3d/gfx_opengl.c"
SDL = ROOT / "getv/port/fast3d/gfx_sdl2.c"
PATCH = ROOT / "getv/patches/thirdparty/0006-perfect-dark-renderer-ab.patch"
LAUNCHER = ROOT / "getv/port/src/ge_launcher.cpp"
CONFIG = ROOT / "getv/port/src/ge_config.c"


class PdRendererAbTests(unittest.TestCase):
    def test_backend_profile_is_opt_in_and_preserves_ge_frontend(self):
        ogl = OGL.read_text()
        sdl = SDL.read_text()
        self.assertIn('getenv("GETV_PD_RENDERER")', ogl)
        self.assertIn('getenv("GETV_PD_RENDERER")', sdl)
        self.assertIn('"#version 130"', ogl)
        self.assertIn('"#define GE_ATTR in"', ogl)
        self.assertIn('"#define GE_VARY out"', ogl)
        self.assertIn('"#define GE_TEXTURE texture"', ogl)
        self.assertIn('glGenVertexArrays(1, &ge_pd_vao)', ogl)
        self.assertIn('ge_pd_renderer_mode() ? GL_RGBA8 : GL_RGBA', ogl)
        self.assertIn('if (ge_pd_renderer_mode()) {\n        glFlush();', ogl)
        self.assertIn('SDL_GL_CONTEXT_PROFILE_COMPATIBILITY', sdl)

    def test_overlay_is_durable_and_attributed(self):
        overlay = PATCH.read_text()
        self.assertIn('GETV_PD_RENDERER', overlay)
        self.assertIn('514bf7affd3259b7919165201342ff81a026d92c', overlay)

    def test_public_configuration_keeps_pd_path_default_off(self):
        launcher = LAUNCHER.read_text()
        config = CONFIG.read_text()
        self.assertIn('m.pd_renderer = env_bool("GETV_PD_RENDERER", false)', launcher)
        self.assertIn('setenv("GETV_PD_RENDERER", m.pd_renderer ? "1" : "0", 1)', launcher)
        self.assertIn('Perfect Dark renderer compatibility path', launcher)
        self.assertIn('pd_renderer = 0', config)
        self.assertIn('key_bool_gate("GETV_PD_RENDERER"', config)


if __name__ == "__main__":
    unittest.main(verbosity=2)
