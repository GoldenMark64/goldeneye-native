#!/usr/bin/env python3
"""Build the actual patched game header against synthetic setup words, without assets."""

import argparse
import os
from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
DECOMP_REV = "c4356466796c697dfd298010b9bed261f9ed8c6a"
DECOMP_URL = "https://github.com/n64decomp/007.git"


def run(args, cwd=None):
    print("+ " + " ".join(map(str, args)), flush=True)
    subprocess.run(list(map(str, args)), cwd=cwd, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", default=os.environ.get("CC", "cc"),
                        help="C compiler executable (MinGW gcc.exe on Windows)")
    parser.add_argument("--source-repo", default=DECOMP_URL,
                        help="upstream Git URL or local source repository containing the pinned commit")
    parser.add_argument("--patch-root", type=Path, default=ROOT,
                        help="repository whose patches are tested (use the unchanged base for a negative control)")
    args = parser.parse_args()

    # Never use a developer's patched/generated working tree. Fetch committed
    # source and replay this revision's patches. bondconstants.h also includes
    # the committed image-ID definition list, not image data or generated source.
    # No extractor, installer, third-party renderer or asset-generation step runs.
    with tempfile.TemporaryDirectory(prefix="ge-multi-ammo-") as temporary:
        scratch = Path(temporary)
        run(["git", "init", "-q", scratch])
        run(["git", "-c", "core.autocrlf=false", "fetch", "--quiet", "--depth=1",
             args.source_repo, DECOMP_REV], scratch)
        run(["git", "-c", "core.autocrlf=false", "checkout", "FETCH_HEAD", "--",
             "src", "include", "assets/images.def"], scratch)
        patches = sorted((args.patch_root.resolve() / "getv/patches").glob("0*.patch"))
        if not patches:
            raise SystemExit("No patches found; refusing an unpatched test")
        for patch in patches:
            if patch.name.startswith("0002-"):
                continue  # Generated asset patch: deliberately outside this test.
            run(["git", "apply", "--include=src/*", "--include=include/*", patch], scratch)

        # Match native game declaration/ABI flags. Deliberately omit the game's
        # forced ge_port_decls.h include: it requires generated assets and is not
        # needed to exercise bondtypes.h. Do not use the port-only Windows compat
        # header, which conflicts with the game's own standard-library shadows.
        flags = ["-std=gnu17", "-fms-extensions", "-fno-strict-aliasing", "-O1",
                 "-DVERSION_US", "-DLANG_US", "-DREFRESH_NTSC", "-DLEFTOVERDEBUG",
                 "-DLEFTOVERSPECTRUM", "-DBUGFIX_R0", "-DTARGET_N64", "-DGE_PORT_NATIVE",
                 "-DNON_MATCHING=1", "-DAVOID_UB=1", "-D_LANGUAGE_C=1",
                 "-Werror=return-type", "-D_FORTIFY_SOURCE=0"]
        for directory in [".", "include", "include/PR", "src", "src/game", "src/inflate"]:
            flags += ["-I", directory]
        if os.name == "nt":
            flags += ["-mno-ms-bitfields"]
            # MinGW's compiler subprocesses and runtime DLLs must be discoverable.
            compiler_dir = str(Path(args.cc).resolve().parent)
            os.environ["PATH"] = compiler_dir + os.pathsep + os.environ.get("PATH", "")
        executable = scratch / ("multi-ammo.exe" if os.name == "nt" else "multi-ammo")
        run([args.cc, *flags, ROOT / "getv/port/tests/game/test_multi_ammo_layout.c",
             "-o", executable], scratch)
        run([executable], scratch)


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as error:
        raise SystemExit(error.returncode)
