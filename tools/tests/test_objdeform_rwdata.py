#!/usr/bin/env python3
"""Compile production objDeform slot expression and model accessor, ROM-free."""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

from test_joy_poll_handshake import extract_function

ROOT = Path(__file__).resolve().parents[2]
HARNESS = ROOT / "getv/port/tests/game/objdeform_rwdata_harness.c"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", default=os.environ.get("CC", "cc"))
    parser.add_argument("--source-root", type=Path, default=ROOT / "vendor/ge-decomp")
    parser.add_argument("--objdeform-source", type=Path,
                        help="saved unchanged propobj.c for the vulnerable control")
    parser.add_argument("--expect", choices=("vulnerable", "fixed"), required=True)
    args = parser.parse_args()
    compiler = shutil.which(args.cc)
    if compiler is None:
        raise SystemExit(f"compiler not found: {args.cc}")
    source_root = args.source_root.resolve()
    propobj = args.objdeform_source or source_root / "src/game/propobj.c"
    model = source_root / "src/game/model.c"
    bondtypes = source_root / "src/bondtypes.h"
    for required in (propobj, model, bondtypes, HARNESS):
        if not required.is_file():
            raise SystemExit(f"required production/test source absent: {required}")

    types = bondtypes.read_text()
    if not re.search(r"union ModelRwData\s*\*\*datas\s*;", types):
        raise SystemExit("production Model.datas pointer type changed")
    if not re.search(r"u16\s+RwDataIndex\s*;", types):
        raise SystemExit("production RwDataIndex declaration absent")
    deform = extract_function(propobj.read_text(), "void objDeform(")
    accessor = extract_function(model.read_text(), "union ModelRwData* modelGetNodeRwData(")
    assignments = re.findall(r"^[ \t]*vtxslot\s*=\s*[^;]+;", deform, re.MULTILINE)
    if len(assignments) != 1:
        raise SystemExit(f"expected exactly one production vtxslot assignment, found {len(assignments)}")
    assignment = assignments[0]
    if not re.search(r"\b(?:model->datas|modelGetNodeRwData)\b", assignment):
        raise SystemExit("production objDeform assignment does not exercise model rwdata")
    if "GE_PORT_NATIVE" not in accessor or "((u32 *)data)[index]" not in accessor:
        raise SystemExit("production accessor no longer establishes native word stride")
    if "root->Parent" not in accessor or "tmp->RwDatas" not in accessor:
        raise SystemExit("production accessor no longer follows attached head pool")

    with tempfile.TemporaryDirectory(prefix="ge-objdeform-rwdata-") as temp:
        directory = Path(temp)
        (directory / "production_objdeform_assignment.inc").write_text(assignment + "\n")
        (directory / "production_model_accessor.inc").write_text(accessor + "\n")
        executable = directory / ("objdeform_rwdata.exe" if os.name == "nt" else "objdeform_rwdata")
        command = [compiler, "-std=c11", "-O0", "-fno-strict-aliasing", "-Werror=return-type",
                   "-DGE_PORT_NATIVE=1", f"-DEXPECT_FIXED={int(args.expect == 'fixed')}",
                   f"-I{directory}", str(HARNESS), "-o", str(executable)]
        compiled = subprocess.run(command, capture_output=True, text=True, timeout=60)
        if compiled.returncode:
            print(compiled.stdout + compiled.stderr, end="")
            return compiled.returncode
        result = subprocess.run([str(executable)], capture_output=True, text=True, timeout=10)
        print(result.stdout + result.stderr, end="")
        if result.returncode or not re.search(
            rf"mode={args.expect} checks=8 failures=0\b", result.stdout
        ):
            return result.returncode or 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
