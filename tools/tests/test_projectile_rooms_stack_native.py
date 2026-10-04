#!/usr/bin/env python3
import pathlib
import shutil
import subprocess
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
DECOMP = ROOT / "vendor/ge-decomp"
GUN = DECOMP / "src/game/gun.c"


class ProjectileRoomsStackNativeTests(unittest.TestCase):
    def test_native_room_trace_has_real_twenty_entry_array(self):
        src = GUN.read_text()
        marker = "GETV-PROJECTILE-ROOMS-STACK-SAFETY"
        self.assertIn(marker, src)

        start = src.index("void gunInitProjectileFromPlayer")
        end = src.index("void generate_player_thrown_grenade", start)
        fn = src[start:end]

        self.assertIn("s32 traversedRooms[0x14];", fn)
        self.assertIn("obj->projectile->unkCC, traversedRooms, &sp50, 0x14", fn)

        # The old matching scalar remains only in the non-native branch; native must
        # never authorize 20 s32 writes through &sp54.
        native_call_start = fn.index("#ifdef GE_PORT_NATIVE", fn.index("rooms[1] = 0xff"))
        native_call_end = fn.index("#else", native_call_start)
        native_call = fn[native_call_start:native_call_end]
        self.assertNotIn("&sp54", native_call)

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
            out = pathlib.Path(td) / "gun.o"
            subprocess.run(
                [*common, "-c", "src/game/gun.c", "-o", str(out)],
                cwd=DECOMP,
                check=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )


if __name__ == "__main__":
    unittest.main(verbosity=2)
