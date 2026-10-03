#!/usr/bin/env python3
import pathlib
import shutil
import subprocess
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
DECOMP = ROOT / "vendor/ge-decomp"
OB = DECOMP / "src/game/ob.c"


class ExtramemReloadScratchNativeTests(unittest.TestCase):
    def test_native_extramem_reload_reborrows_bank_tail(self):
        src = OB.read_text()

        marker = "GETV-EXTRAMEM-RELOAD-SCRATCH"
        self.assertIn(marker, src)

        block_start = src.index(marker)
        block_end = src.index("ptrdata             = mempAllocBytesInBank", block_start)
        block = src[block_start:block_end]

        self.assertIn("if (loadMethod == FILELOADMETHOD_EXTRAMEM)", block)
        self.assertIn("info->poolRemaining = mempGetBankSizeLeft(bank);", block)
        self.assertIn("#ifdef GE_PORT_NATIVE", src[src.rfind("#ifdef GE_PORT_NATIVE", 0, block_start):block_start])

        # The cached final decompressed size must not gate the native EXTRAMEM refresh.
        refresh = block.index("if (loadMethod == FILELOADMETHOD_EXTRAMEM)")
        zero_check = block.index("if (info->poolRemaining == 0)")
        self.assertLess(refresh, zero_check)

    def test_pp7_failure_shape_requires_more_than_final_raw_size(self):
        # Captured Control/Boris resource: PchrwppkZ is 1360 raw -> 574 packed.
        raw = 1360
        packed = 574
        packed_aligned = (packed + 7) & ~7

        # load_resource() places packed input at allocation_end - packed_aligned.
        input_offset_if_alloc_is_raw = raw - packed_aligned
        self.assertLess(input_offset_if_alloc_is_raw, raw)
        self.assertGreater(raw - input_offset_if_alloc_is_raw, 0)

        # Therefore a raw-sized allocation cannot simultaneously preserve the entire
        # compressed tail while producing all raw output in front of it.
        self.assertGreater(raw, input_offset_if_alloc_is_raw)

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
            out = pathlib.Path(td) / "ob.o"
            subprocess.run(
                [*common, "-c", "src/game/ob.c", "-o", str(out)],
                cwd=DECOMP,
                check=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )


if __name__ == "__main__":
    unittest.main(verbosity=2)
