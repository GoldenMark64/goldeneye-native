"""Exercise the production relaunch function against inert executables on macOS."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]

class HandoffTests(unittest.TestCase):
    def test_exec_and_recovery_contract(self):
        source = (ROOT / 'getv/port/src/ge_launcher.cpp').read_text()
        function = source[source.index('void relaunch()'):source.index('/* ---------------------------------------------------------------- look')]
        with tempfile.TemporaryDirectory(prefix='renderer handoff ') as directory:
            root = Path(directory)
            harness = root / 'handoff.cpp'
            harness.write_text('''#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <TargetConditionals.h>
#define GE_PLATFORM_MAC 1
#define ge_errno errno
#include "ge_renderer_choice.h"
int g_argc; char **g_argv;
bool self_path(char *out, size_t n) { return snprintf(out, n, "%s", g_argv[0]) > 0; }
''' + function + '\nint main(int argc, char **argv) { g_argc=argc; g_argv=argv; relaunch(); return 99; }\n')
            binary = root / 'handoff'
            subprocess.run(['clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                            '-I', str(ROOT / 'getv/port/mac'), str(harness),
                            '-framework', 'CoreFoundation', '-o', str(binary)], check=True)
            target = root / 'metal fixture'
            target.write_text('#!/bin/sh\nprintf "%s\\n" "$GETV_LAUNCHER" "$GETV_PROFILE_PLUS" "$GETV_MAC_APP_CONFIG_DIR" "$@"\n')
            target.chmod(0o700)
            env = {**os.environ, 'GETV_MAC_RENDERER_APP': '1', 'GETV_MAC_APP_RENDERER': 'metal',
                   'GETV_MAC_APP_METAL': str(target), 'GETV_MAC_APP_METAL_AVAILABLE': '1',
                   'GETV_LAUNCHER': '1', 'GETV_PROFILE_PLUS': '1', 'GETV_MAC_APP_CONFIG_DIR': directory}
            result = subprocess.run([str(binary), '--launcher', '--config=config with spaces.cfg'], env=env,
                                    text=True, capture_output=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stdout, f'0\n1\n{directory}\n--config=config with spaces.cfg\n')
            # A file disappearing after the UI's availability check must fail to recovery,
            # not fall through into the settings executable's renderer.
            target.unlink()
            result = subprocess.run([str(binary), '--launcher'], env=env, capture_output=True, timeout=10)
            self.assertEqual(result.returncode, 1)
            target.write_text('invalid executable')
            target.chmod(0o700)
            result = subprocess.run([str(binary), '--launcher'], env=env, capture_output=True, timeout=10)
            self.assertEqual(result.returncode, 1)
            self.assertIn(b'renderer exec failed', result.stderr)

if __name__ == '__main__':
    unittest.main()
