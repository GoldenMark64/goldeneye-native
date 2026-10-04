#!/usr/bin/env python3
import pathlib
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
DECOMP = ROOT / "vendor/ge-decomp"
CHRAI = DECOMP / "src/game/chrai.c"
PROPOBJ = DECOMP / "src/game/propobj.c"
SETUP = DECOMP / "assets/obseg/setup/u/UsetuptraZ.c"


class TrainPostHackExplosionDiagnosticTests(unittest.TestCase):
    def test_probe_markers_and_script_contract(self):
        chrai = CHRAI.read_text()
        propobj = PROPOBJ.read_text()
        setup = SETUP.read_text()

        self.assertIn("GETV_TRAIN_EXPLOSION_DEBUG", chrai)
        self.assertIn("[getv][train-explosion]", chrai)
        self.assertIn('ge_train_explosion_debug_object("destroy-before"', chrai)
        self.assertIn('ge_train_explosion_debug_object("destroy-after"', chrai)

        self.assertIn("GETV_TRAIN_EXPLOSION_DEBUG", propobj)
        self.assertIn('ge_train_explosion_debug_tick("grenade-explode-before"', propobj)
        self.assertIn('ge_train_explosion_debug_tick("grenade-explode-after"', propobj)

        self.assertIn("if_objective_bitfield_is_set_on(0x00000800, 0x01)", setup)
        for tag in (0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x2B, 0x2C, 0x2D):
            self.assertIn(f"object_destroy(0x{tag:02x})", setup)

    def test_native_translation_units_compile(self):
        compiler = shutil.which("clang") or shutil.which("gcc")
        if not compiler:
            raise unittest.SkipTest("a C compiler is required")
        warning_flags = ["-Werror=return-type"]
        if "clang" in pathlib.Path(compiler).name:
            warning_flags = ["-Wno-everything", "-Werror=return-type", "-ferror-limit=0"]
        common = [
            compiler,
            "-fms-extensions",
            "-include", "src/ge_port_decls.h",
            "-I", ".", "-I", "include", "-I", "include/PR",
            "-I", "src", "-I", "src/game", "-I", "src/inflate",
            "-DVERSION_US", "-DLANG_US", "-DREFRESH_NTSC",
            "-DLEFTOVERDEBUG", "-DLEFTOVERSPECTRUM", "-DBUGFIX_R0",
            "-DTARGET_N64", "-DGE_PORT_NATIVE", "-DNON_MATCHING=1",
            "-DAVOID_UB=1", "-D_LANGUAGE_C=1",
            *warning_flags,
            "-fno-strict-aliasing", "-O1",
        ]
        with tempfile.TemporaryDirectory() as td:
            for rel in ("src/game/chrai.c", "src/game/propobj.c"):
                src = DECOMP / rel
                out = pathlib.Path(td) / (src.stem + ".o")
                subprocess.run(
                    [*common, "-c", rel, "-o", str(out)],
                    cwd=DECOMP,
                    check=True,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    text=True,
                )


if __name__ == "__main__":
    unittest.main(verbosity=2)
