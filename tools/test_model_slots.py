#!/usr/bin/env python3
"""Run extracted production model-slot lifecycle code without game data."""

import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BASE = "0a6572e65c9a94c972230b1063f915770589ebde"
DECOMP = "c4356466796c697dfd298010b9bed261f9ed8c6a"
URL = "https://github.com/n64decomp/007.git"
NAMES = {
    "initunk_005520.c": ("modelmgrAllocateModelSlots", "modelmgrAllocateAnimModelSlots"),
    "model.c": ("modelmgrCanSlotFitRwdata", "ge_model_pool_index",
                "ge_anim_pool_index", "modelmgrInstantiateModel", "clear_model_obj",
                "modelmgrInstantiateModelWithAnim", "clear_aircraft_model_obj"),
}


def run(command, cwd):
    print("+", *map(str, command), flush=True)
    subprocess.run(list(map(str, command)), cwd=cwd, check=True)


def extract(source, name):
    pattern = r"^(?:static\s+)?(?:bool|void|s32|Model\s*\*)\s*" + name + r"\s*\([^;]*?\)\s*\{"
    match = re.search(pattern, source, re.M | re.S)
    if not match:
        if name.startswith("ge_"):
            return ""
        raise RuntimeError("production function missing: " + name)
    end = source.find("\n}\n", match.end())
    if end < 0:
        raise RuntimeError("unterminated production function: " + name)
    return source[match.start():end + 3]


def prepare(directory, source_repo, negative):
    run(["git", "init", "-q", directory], directory)
    run(["git", "-c", "core.autocrlf=false", "fetch", "--quiet", "--depth=1",
         source_repo, DECOMP], directory)
    run(["git", "-c", "core.autocrlf=false", "checkout", "FETCH_HEAD", "--",
         "src", "include", "assets/images.def"], directory)
    if negative:
        listing = subprocess.run(
            ["git", "ls-tree", "-r", "--name-only", BASE, "--", "getv/patches"],
            cwd=ROOT, check=True, capture_output=True, text=True).stdout.splitlines()
        names = sorted(
            name for name in listing
            if re.fullmatch(r"getv/patches/0[^/]*\.patch", name)
            and not Path(name).name.startswith("0002-")
        )
        if not names:
            raise RuntimeError("no base patches found")
        patches = []
        for name in names:
            content = subprocess.run(
                ["git", "show", f"{BASE}:{name}"],
                cwd=ROOT, check=True, capture_output=True).stdout
            patch = directory / Path(name).name
            patch.write_bytes(content)
            patches.append(patch)
    else:
        patches = [
            patch for patch in sorted((ROOT / "getv/patches").glob("0*.patch"))
            if not patch.name.startswith("0002-")
        ]
        if not patches:
            raise RuntimeError("no patches found")

    for patch in patches:
        run(["git", "apply", "--include=src/*", "--include=include/*", patch], directory)
    init = (directory / "src/game/initunk_005520.c").read_text()
    model = (directory / "src/game/model.c").read_text()
    constants = init[init.index("#define MODEL_SPARE_SLOTS"):
                     init.index("void modelmgrResetSlotCounts")]
    macros = ""
    if "#define GE_SLOT_FREE" in model:
        macros = model[model.index("#ifdef GE_PORT_NATIVE\n#define GE_SLOT_FREE"):
                       model.index("Model *modelmgrInstantiateModel")]
    parts = ['#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n'
             '#include <memp.h>\n#include "model.h"\n#include "objecthandler.h"\n',
             constants, macros]
    for file, names in NAMES.items():
        source = (directory / "src/game" / file).read_text()
        parts.extend(extract(source, name) for name in names)
    (directory / "production_model_slots.inc").write_text("\n".join(parts))


def case(label, cc, source_repo, parent):
    directory = parent / label
    directory.mkdir()
    prepare(directory, source_repo, label == "negative")
    flags = ["-std=gnu17", "-fms-extensions", "-fno-strict-aliasing", "-O1",
             "-DVERSION_US", "-DLANG_US", "-DREFRESH_NTSC", "-DLEFTOVERDEBUG",
             "-DLEFTOVERSPECTRUM", "-DBUGFIX_R0", "-DTARGET_N64", "-DGE_PORT_NATIVE",
             "-DNON_MATCHING=1", "-DAVOID_UB=1", "-D_LANGUAGE_C=1",
             "-Werror=return-type", "-D_FORTIFY_SOURCE=0"]
    for path in (".", "include", "include/PR", "src", "src/game", "src/inflate"):
        flags += ["-I", path]
    if os.name == "nt":
        flags += ["-mno-ms-bitfields"]
        os.environ["PATH"] = str(Path(cc).resolve().parent) + os.pathsep + os.environ.get("PATH", "")
    binary = directory / ("model-slots.exe" if os.name == "nt" else "model-slots")
    run([cc, *flags, ROOT / "getv/port/tests/game/test_model_slot_lifecycle.c",
         "-o", binary], directory)
    result = subprocess.run([str(binary)], cwd=directory, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    print(result.stdout, end="", flush=True)
    match = re.search(r"(\d+) checks, (\d+) failures", result.stdout)
    if not match or int(match[1]) != 106:
        raise RuntimeError(f"{label}: expected 106 executed checks")
    failures = int(match[2])
    if label == "repaired" and (result.returncode != 0 or failures != 0):
        raise RuntimeError(f"repaired lifecycle failed: {failures} assertions")
    if label == "negative" and (result.returncode != 1 or failures == 0
                                or "animated slot exhausted" not in result.stdout):
        raise RuntimeError("unchanged base did not fail for slot exhaustion")
    print(f"{label}: expected result; 106 checks, {failures} failures", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", default=os.environ.get("CC", "cc"))
    parser.add_argument("--source-repo", default=URL)
    parser.add_argument("--case", choices=("negative", "repaired", "both"), default="both")
    args = parser.parse_args()
    source = Path(args.source_repo)
    source_repo = source.resolve() if source.exists() else args.source_repo
    with tempfile.TemporaryDirectory(prefix="ge-model-slots-") as temporary:
        parent = Path(temporary)
        for label in (("negative", "repaired") if args.case == "both" else (args.case,)):
            case(label, args.cc, source_repo, parent)


if __name__ == "__main__":
    try:
        main()
    except (subprocess.CalledProcessError, RuntimeError) as error:
        raise SystemExit(str(error))
