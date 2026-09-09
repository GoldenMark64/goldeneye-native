#!/bin/bash
# Native, ROM-free app policy/process tests. Unsupported hosts fail, never skip green.
set -euo pipefail
cd "$(dirname "$0")/../.."
[ "$(uname -s)" = Darwin ] || { echo "Native renderer app tests require macOS" >&2; exit 1; }
output="$(mktemp -d "${TMPDIR:-/tmp}/ge-renderer-tests.XXXXXX")"
trap 'rm -rf "$output"' EXIT
clang -fobjc-arc -Wall -Wextra -Werror -Wno-unused-parameter \
  -framework Cocoa -framework Metal getv/port/mac/ge_renderer_app.m -o "$output/app"
clang -fobjc-arc -Wall -Wextra -Werror -Wno-unused-parameter -Wno-incompatible-pointer-types \
  -framework Cocoa -framework Metal tools/tests/test_renderer_app.m -o "$output/test"
"$output/test"
bash getv/port/tests/run_tests.sh config

python3 tools/tests/test_renderer_handoff.py
