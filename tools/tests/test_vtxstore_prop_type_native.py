#!/usr/bin/env python3
import pathlib
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
DECOMP = ROOT / "vendor/ge-decomp"
VTXSTORE = DECOMP / "src/game/vtxstore.c"
CONSTANTS = DECOMP / "src/bondconstants.h"


class VtxstorePropTypeNativeTests(unittest.TestCase):
    def test_fix_refs_visits_character_props_not_object_props(self):
        src = VTXSTORE.read_text()
        constants = CONSTANTS.read_text()

        self.assertIn("if (var_s1->type == PROP_TYPE_CHR)", src)
        self.assertNotIn("if (var_s1->type == 1)", src)

        enum_slice = constants[
            constants.index("typedef enum PROP_TYPE"):
            constants.index("} PROP_TYPE;", constants.index("typedef enum PROP_TYPE"))
        ]
        self.assertLess(enum_slice.index("PROP_TYPE_OBJ"), enum_slice.index("PROP_TYPE_CHR"))
        self.assertIn("PROP_TYPE_NUL,\n        PROP_TYPE_OBJ,\n        PROP_TYPE_DOOR,\n        PROP_TYPE_CHR,", enum_slice)

    def test_native_translation_unit_compiles(self):
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
            out = pathlib.Path(td) / "vtxstore.o"
            subprocess.run(
                [*common, "-c", "src/game/vtxstore.c", "-o", str(out)],
                cwd=DECOMP,
                check=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )


if __name__ == "__main__":
    unittest.main(verbosity=2)
