# Getting started

This is the shortest path from a fresh computer or source checkout to playing GoldenEye-Native.
The installer fetches the open-source dependencies, prepares the decompilation, extracts assets
from your own ROM, and builds the native executable. Re-running it is safe and resumes completed
work.

You need:

- your own legally dumped US retail GoldenEye 007 ROM (`.z64`, `.n64`, or `.v64`);
- about 4 GB of free disk space; and
- an internet connection for source and dependency downloads during the first install.

No ROM or extracted game data is downloaded, bundled, or uploaded by this project.

## Install

### macOS

1. Put your legally dumped GoldenEye 007 ROM on your **Desktop**. Do not rename it.
2. On [the project page](https://github.com/seb-patron/goldeneye-native), click the green
   **Code** button, then **Download ZIP**. Double-click the downloaded file to unzip it.
3. Open the folder and double-click **Install on Mac**. If Gatekeeper blocks it, right-click the
   file, choose **Open**, and confirm once.
4. When installation finishes, double-click **Play GoldenEye** in the same folder.

The first install takes about 10 to 40 minutes. Re-running the installer resumes completed work.

**Optional terminal install.** If you cloned the repository or prefer the terminal, run this from
the repository root instead of step 3:

```bash
bash tools/install.sh
```

It finds a supported ROM on your Desktop or in Downloads. Pass an explicit file only when needed:

```bash
bash tools/install.sh --rom /path/to/your/rom.z64
```

The build supports Apple silicon and Intel Macs as separate native targets; see [`SETUP.md`](SETUP.md)
for the complete macOS toolchain and manual pipeline.

### Linux

1. Install `git` and `python3` from your package manager.
2. Download and unzip this repository, or clone it with Git, then open a terminal in that folder.
3. Run:

```bash
bash tools/install.sh
```

The installer finds a supported ROM on your Desktop or in Downloads. It never runs `sudo`; if a
system dependency is missing, it prints the appropriate package-manager command and stops. After
installing that package, run the same command again.

Use an explicit ROM path or add a per-user applications-menu entry only when needed:

```bash
bash tools/install.sh --rom /path/to/your/rom.z64
bash tools/install.sh --rom /path/to/your/rom.z64 --desktop
```

### Windows

The no-code Windows setup is still a release candidate. There is no official Windows download or
release automation yet; the current GitHub Actions workflow validates the package but does not
upload it. Publishing will be added in a focused follow-up after coordinated macOS launcher work,
clean-machine Windows acceptance, and licensing and code-signing review.

When this repository's Releases page lists an official coordinated package:

1. Have your supported US big-endian `.z64` dump ready. The setup app recognizes `.v64` and
   `.n64` byte orders but refuses them because the no-copy installer does not create a converted
   second file.
2. Download and extract the ROM-free Windows setup ZIP for the stable version you want.
3. Double-click `GoldenEye-Native-Setup.exe`, choose an empty installation folder, and select your
   ROM in the normal file picker.
4. Leave setup open for the first local build. When it finishes, double-click the locally built
   **GoldenEye** executable in the folder you chose; it opens the custom launcher.

You do not install Git, Python, a compiler, or the source repository. Setup downloads verified
portable tools privately under your Windows profile and does not need administrator access. See
[`WINDOWS_INSTALL.md`](WINDOWS_INSTALL.md) for current availability and SmartScreen details.

## Run

The normal OpenGL executables are:

```bash
./getv/build-mac/goldeneye --launcher       # macOS
./getv/build-linux/goldeneye --launcher     # Linux
```

```powershell
.\getv\build-windows\goldeneye.exe              # opens the Windows launcher
```

On macOS and Windows, double-click the game instead. The Windows executable now opens the custom
launcher by default, matching the Mac app. Set `GETV_LAUNCHER=0` only when a scripted Windows run
needs to bypass it; `--launcher` remains available on every desktop platform.

The application logs its selected renderer, config path, save path, controllers, and resolved
bindings at startup. Those lines are useful when troubleshooting.

### Native Metal on macOS

The standard installer builds OpenGL. To build the separate Metal executable after installation:

```bash
GETV_RENDERER=metal ./getv/build_mac.sh all
./getv/build-mac-metal/goldeneye-metal --launcher
```

OpenGL and Metal use separate build directories, so the two builds do not overwrite each other.

## Configure

The launcher is the easiest way to change video, gameplay, mouse, and controller settings. It
restarts the game after applying changes because most settings are read once during startup.

For a one-off override, use a command-line option:

```bash
./getv/build-mac/goldeneye --resolution=1920x1080 --fullscreen=1
```

For persistent settings, edit `goldeneye.cfg`. The exact file used is printed at startup. You can
also generate a commented template at a path of your choice:

```bash
./getv/build-mac/goldeneye --write-config=/path/to/goldeneye.cfg
./getv/build-mac/goldeneye --config=/path/to/goldeneye.cfg
```

Precedence is:

```text
command line > environment > config file > built-in defaults
```

See [`CONTROLS.md`](CONTROLS.md) for the complete input map and rebinding options, and
[`CONFIGURATION.md`](CONFIGURATION.md) for every setting.

## Rebuild after updating

From the repository root:

```bash
./getv/build_mac.sh all        # macOS OpenGL
./getv/build_linux.sh all      # Linux
```

```powershell
.\getv\build_windows.ps1 -Target all
```

If an update adds or changes a patch or dependency, rerun the installer instead. It detects work
that is already complete and applies the remaining steps in the required order.

## Quick checks

Run the ROM-free port-layer tests:

```bash
bash getv/port/tests/run_tests.sh
```

On Windows:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File getv\port\tests\run_tests.ps1
```

If the game built but will not launch, keep the first meaningful error and the build summary. A
successful build reports `0 failed` for every phase. See [`SETUP.md`](SETUP.md#7-troubleshooting)
for detailed troubleshooting and use the repository's guided issue forms if the problem remains.

Never attach a ROM, save file, `base.zip`, or extracted asset to an issue. Text logs and
screenshots of the running game are welcome.
