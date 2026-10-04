"""ROM-free regression checks for AI_PRINT/debug_log bytecode sizing.

There are two source encodings of command 0xAD in the decomp:

* modern PRINT() has no useful text payload, so aicommands2.h emits an explicit
  empty-string terminator after the opcode;
* legacy bondaicommands.h debug_log carries a real NUL-terminated inline string.

Both must use the same variable-length chraiitemsize() rule.
"""
from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
DECOMP = ROOT / "vendor/ge-decomp"
CHRAI = DECOMP / "src/game/chrai.c"
AICOMMANDS2 = DECOMP / "src/aicommands2.h"

HARNESS = r"""
#include <stdio.h>
typedef unsigned char u8;
typedef int s32;
#define AI_PRINT 0x7f
#define AICMDSIZE 1

s32 test_itemsize(u8 *AIList, s32 offset)
{
    switch (AIList[offset])
    {
__CASE__
        default:
            return -1;
    }
}

int main(int argc, char **argv)
{
    u8 modern[] = {AI_PRINT, 0x00, 0xc3, 0x08};
    u8 legacy[] = {AI_PRINT, 'I', 'N', '1', '\n', 0x00, 0xc3};
    printf("%d\n", test_itemsize(argc > 1 ? legacy : modern, 0));
    return 0;
}
"""


class NativeAiPrintTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not CHRAI.exists():
            raise unittest.SkipTest("prepared decomp source is required")
        cls.compiler = shutil.which(os.environ.get("CC", "clang")) or shutil.which("gcc")
        if not cls.compiler:
            raise unittest.SkipTest("a C compiler is required")

        cls.directory = tempfile.TemporaryDirectory(
            prefix=".ge-ai-print-",
            dir=DECOMP,
        )
        cls.addClassCleanup(cls.directory.cleanup)
        cls.tmp = Path(cls.directory.name)

        source = CHRAI.read_text()
        func_start = source.index("s32 chraiitemsize(")
        case_start = source.index("        case AI_PRINT:", func_start)
        case_end = source.index("        default:", case_start)
        cls.case_source = source[case_start:case_end].rstrip()

    def test_full_chrai_translation_unit_compiles_native(self):
        obj = self.tmp / "chrai.o"
        warning_flags = ["-Werror=return-type"]
        if "clang" in Path(self.compiler).name:
            warning_flags = [
                "-Wno-everything",
                "-Werror=return-type",
                "-ferror-limit=0",
            ]
        cmd = [
            self.compiler,
            "-fms-extensions",
            "-include", "src/ge_port_decls.h",
            "-I", ".", "-I", "include", "-I", "include/PR",
            "-I", "src", "-I", "src/game", "-I", "src/inflate",
            "-DVERSION_US", "-DLANG_US", "-DREFRESH_NTSC",
            "-DLEFTOVERDEBUG", "-DLEFTOVERSPECTRUM", "-DBUGFIX_R0",
            "-DTARGET_N64", "-DGE_PORT_NATIVE", "-DNON_MATCHING=1",
            "-DAVOID_UB=1", "-D_LANGUAGE_C=1", "-fno-strict-aliasing",
            *warning_flags,
            "-O1", "-c", "src/game/chrai.c", "-o", str(obj),
        ]
        result = subprocess.run(cmd, cwd=DECOMP, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue(obj.exists())

    def _compile_harness(self) -> Path:
        src = self.tmp / "sizing.c"
        binary = self.tmp / "sizing"
        src.write_text(HARNESS.replace("__CASE__", self.case_source))
        cmd = [self.compiler, "-std=c11", "-O1", str(src), "-o", str(binary)]
        result = subprocess.run(cmd, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return binary

    def _run_harness(self, legacy: bool) -> int:
        binary = self._compile_harness()
        args = [str(binary)]
        if legacy:
            args.append("legacy")
        result = subprocess.run(args, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return int(result.stdout.strip())

    def test_modern_print_empty_payload_is_two_bytes(self):
        self.assertEqual(self._run_harness(legacy=False), 2)

    def test_legacy_debug_log_inline_string_is_scanned(self):
        self.assertEqual(self._run_harness(legacy=True), 6)

    def test_modern_print_macro_emits_nul_terminator(self):
        source = AICOMMANDS2.read_text()
        start = source.index("#define PRINT(STRING)")
        body = source[start:source.index("//==============================================================================", start)]
        self.assertIn("AI_PRINT", body)
        self.assertRegex(body, r"AI_PRINT\s*,\s*\\\s*\n\s*0\s*,")


if __name__ == "__main__":
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(NativeAiPrintTests)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    if result.skipped or result.testsRun == 0:
        print("AI_PRINT regression tests must execute without skips.", file=sys.stderr)
        sys.exit(1)
    sys.exit(0 if result.wasSuccessful() else 1)
