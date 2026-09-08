# Official releases

Official player downloads are produced only from reviewed stable tags. Day-to-day branches and
pull requests can build and test a setup package, but they do not publish one.

## What may be released

The Windows release asset is
`GoldenEye-Native-Windows-Setup-vMAJOR.MINOR.PATCH.zip`. It contains the ROM-free setup app, its
checksum, first-run instructions, and required third-party notices. The setup app is not the game:
it asks the player for their own supported cartridge dump and creates the playable
`goldeneye.exe` only inside the installation folder they choose.

Never attach a playable `goldeneye.exe`, a ROM, a save, `base.zip`, extracted assets, generated
asset source, texture dumps, or audio banks to a tag, Actions run, or GitHub Release. The same
boundary applies to future macOS release packaging: a Mac download may contain only the ROM-free
client/setup surface and reviewed redistributable dependencies; its locally generated playable
game remains private.

## Release flow

1. Merge the reviewed release changes to `main` and confirm all required checks pass.
2. Choose a stable semantic version such as `v0.1.0`. The automated release accepts exactly
   `vMAJOR.MINOR.PATCH`; prerelease suffixes do not publish an official package.
3. Create an annotated tag on the reviewed `main` commit and push that tag. Do not move or reuse a
   published version tag.
4. **Package Windows setup** builds the package on a clean Windows runner, runs the tracked-data
   guard, ROM-format self-test, bootstrap syntax test, import inspection, and forbidden-string
   scan, then creates a draft GitHub Release.
5. The workflow attaches the versioned ZIP and its SHA-256 file before publishing the release. If
   any build or safety check fails, no official release is published.
6. Download the published ZIP from GitHub Releases and complete the clean-machine checklist in
   [`WINDOWS_PACKAGING.md`](WINDOWS_PACKAGING.md). Never substitute a file from an unrelated
   workflow run or repost.

The release job alone receives `contents: write`; pull-request builds remain read-only. Official
Actions are pinned to full commit SHAs. When the ROM-free macOS packaging update is ready, add its
build as another release asset producer and make the final publish job depend on both platforms,
so one version tag produces one coordinated Windows/macOS release.
