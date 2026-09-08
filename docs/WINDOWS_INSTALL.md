# Installing and playing on Windows

This is the no-code path for someone who wants to try GoldenEye-Native on Windows. If you want to
edit the project or build from a source checkout, use [`BUILDING.md`](BUILDING.md) instead.

## Current status

The Windows setup app is an unsigned release candidate for 64-bit Windows 10 and 11. Official
downloads are published only from this repository's stable tagged Releases; do not use an
executable reposted elsewhere. If the Releases page has no Windows setup ZIP yet, there is no
official public candidate yet.

The downloaded setup package is not the game and cannot play without a ROM. It contains no ROM,
extracted game assets, decompiled game source, or playable `goldeneye.exe`. The playable program
is built only on your computer after you select your own supported cartridge dump.
Every fresh installation stops at the ROM picker and cannot build the game until that local file
has passed verification.

That technical separation is not legal advice. You are responsible for using a ROM and the
resulting local build in a way permitted where you live. See [`LICENSING.md`](LICENSING.md) for
the project's unresolved source-licensing questions.

## What you need

- A Windows 10 or 11 x86-64 computer with internet access.
- Your own supported US GoldenEye 007 big-endian `.z64` cartridge dump that you are permitted to
  use.
- About 4 GB of free disk space and 10 to 40 minutes for the first build.

You do **not** need to install Git, Python, a compiler, or this source repository. Setup uses
private, checksum-verified portable tools under your Windows user profile and does not need
administrator access.

## Download an official version

1. Open this repository's **Releases** page and choose the version you want.
2. Download `GoldenEye-Native-Windows-Setup-<version>.zip` and its `.sha256` file.
3. Compare the downloaded ZIP's SHA-256 with that checksum.
4. Extract the ZIP into a new folder. Keep these four files together:
   `GoldenEye-Native-Setup.exe`, `SHA256SUMS.txt`, `README.txt`, and
   `THIRD_PARTY_NOTICES.txt`.

## Install and play

1. Double-click `GoldenEye-Native-Setup.exe`.
2. Choose a new, empty installation folder.
3. Select your ROM when the normal Windows file picker opens.
4. Confirm that setup recognizes and verifies the file. It reads your selected ROM in place and
   does not copy, modify, or upload it.
5. Leave setup open while it downloads the public source and build tools, extracts the required
   data locally, and builds the playable program.
6. When all build groups report `0 failed`, click **Launch GoldenEye**.

Afterward, double-click **GoldenEye** or **Play GoldenEye** in the folder you chose. The game
executable itself opens the custom launcher, just like the Mac app; no command file, terminal or
command-line option is required. Re-running setup in that same folder is the supported way to
resume a stopped build; verified downloads and completed steps are reused.

## Windows SmartScreen

The setup app is not Authenticode-signed, so SmartScreen may identify it as an unrecognized app.
Confirm that it came from this repository's Releases page and compare both supplied SHA-256 files
before choosing **More info** and **Run anyway**. Do not bypass a warning for a copy obtained from
somewhere else.

## If setup fails

Use **Copy the log** in the setup window and include that text in a new issue. Never attach or
paste your ROM, its path, generated assets, save files, or the locally built `goldeneye.exe`.

The maintainer/tester checklist is in [`WINDOWS_PACKAGING.md`](WINDOWS_PACKAGING.md).
