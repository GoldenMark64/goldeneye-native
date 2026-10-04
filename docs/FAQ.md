# Frequently asked questions

These answers describe the current GoldenEye Native 1.0 line. For exact certification scope, see
[`TESTING_1.0.md`](TESTING_1.0.md); for known limitations and current feature status, see the
[front page](../README.md).

Some optional systems predate the GoldenMark64 1.0 stabilization work. In particular, the co-op
and netplay status below is inherited from SegfaultEvan's August 2026 commits and measurements.
Those results are preserved as project history but were **not independently re-certified by the
GoldenMark64 1.0 campaign pass**.

## Is this an emulator?

No. An emulator runs the retail N64 program by emulating the original hardware. GoldenEye Native
builds from the [`n64decomp/007`](https://github.com/n64decomp/007) reconstruction of the game's C
source and compiles it as a native application for the host operating system.

You still supply your own supported cartridge dump locally. The project does not ship the ROM or
extracted game assets.

## Is this the Xbox 360 remaster or the cancelled XBLA release?

No. Those are separate codebases. This project is based on the Nintendo 64 game and its
decompilation.

## How is this different from a static recompilation?

A static recompiler translates existing machine code for another CPU. This project works from
editable reconstructed C source. That allows native-port bugs and game logic to be changed and
tested directly rather than only patched around from outside.

## Which platforms work?

The current desktop targets are macOS, Linux and Windows.

- **Linux:** builds and plays; the complete 1.0 retail campaign was human-certified on the primary
  Ubuntu 24.04 x86-64 / Intel Iris Xe system. Other GPU, driver and distribution combinations are
  not exhaustively certified.
- **macOS:** builds and plays on Apple silicon and Intel; OpenGL and a native Metal backend exist.
- **Windows:** builds natively with mingw-w64 and passes the project's self-test, but does not yet
  have the same full campaign-certification evidence as the primary Linux system.

tvOS and iOS are bring-up targets. Android has a compiling arm64/GLES port layer but is not yet a
running game.

See the [platform table](../README.md#which-platforms-work).

## Do I need a ROM?

Yes. You need your own supported US retail GoldenEye 007 dump. Setup verifies the expected dump
locally and extracts the data needed for your local build.

The repository is intentionally source-only: it does not ship a ROM, extracted assets, or a
prebuilt playable GoldenEye binary containing those locally extracted assets.

## Does setup upload or modify my ROM?

The current installers read and verify the selected dump locally. They do not upload it, and the
normal setup path does not modify the original file.

## Does it support mouse and keyboard and controllers?

Yes. Mouse and keyboard work alongside SDL2-compatible controllers. Mouse sensitivity and
inversion are configurable, controller bindings are exposed through the launcher/configuration
surface, and all eight retail GoldenEye control styles are represented.

See [`CONTROLS.md`](CONTROLS.md) and [`MOUSE.md`](MOUSE.md).

## Does it run at 60 fps? Can it go higher?

The conservative default on NTSC builds is 60 fps.

High-refresh rendering is also available. The real-clock free-run path allows rendering to run
substantially faster without simply making the main GoldenEye field clock advance once per
rendered frame. This has been measured: the documented Dam tests kept the main field clock near
60 fields/sec while rendering hundreds of frames per second.

That is a **timing improvement, not a claim that every timing-sensitive subsystem is completely
fixed**. Some game logic still advances per simulation update and remains subject to
subsystem-by-subsystem audit.

See [`FRAME_TIMING.md`](FRAME_TIMING.md) and the
[README timing summary](../README.md#frame-rate-and-timing-improvements).

## Is 30 fps the only faithful setting?

No blanket claim like that is made. The project keeps 60 fps as the conservative default and uses
deterministic 60 fps runs for many measurements. A 30 Hz paired timing mode also exists and is
useful when testing lower update cadence.

The correct answer depends on the subsystem being measured; see [`FRAME_TIMING.md`](FRAME_TIMING.md).

## Does widescreen work?

Yes. Widescreen and ultrawide output are implemented, and the renderer uses the actual framebuffer
aspect rather than merely advertising a wider window. HUD and gun-sight corrections are included.

Field of view is separately configurable. See [`CONFIGURATION.md`](CONFIGURATION.md).

## Can it run at 4K?

The internal resolution is configurable and supersampling is available. Practical performance
depends on the renderer, GPU, resolution, filtering and other selected features.

High resolution and high refresh are separate questions: resolution support does not imply that
every gameplay subsystem has been certified at arbitrary render rates.

## Are HD texture packs supported?

Yes. Texture replacement is implemented and has been verified end to end with a 4x replacement.
Pack images are matched by texture hash. No HD pack or original game texture is shipped by this
repository.

See [`MODDING.md`](MODDING.md).

## Can I mod it?

Yes. The project exposes several opt-in modification surfaces, including Lua mods, configuration
and environment gates, texture replacement, gameplay presets and developer tooling.

Those extensions are separate from the 1.0 base-campaign certification. Start with
[`MODDING.md`](MODDING.md).

## Is split-screen multiplayer supported?

Yes. The original local multiplayer supports two to four players, radar, all 64 selectable
characters, scenarios and multiplayer arenas.

## Is LAN multiplayer finished?

No. It is **beta/experimental**, and the current status is inherited rather than newly certified.

SegfaultEvan's pre-takeover work wired the network path into the boot/tick loop and documented real
UDP peer sessions exchanging synchronized input. The same work also documented repeated simulation
desynchronization even after several obvious causes were controlled.

GoldenMark64's 1.0 campaign-certification work did not independently repeat those LAN trials, so the
accurate present claim is: **the repository contains an integrated experimental netplay path whose
last documented upstream measurements reached real sessions but still desynchronized**.

See [`NETPLAY.md`](NETPLAY.md).

## Can multiple people play campaign missions together?

The repository contains an experimental co-op path, but its movement/camera validation is inherited
from SegfaultEvan's pre-takeover work rather than from the GoldenMark64 1.0 campaign pass.

SegfaultEvan's August 2026 commits document separate player cameras, transition to first person,
and later measured co-op movement. The current tree preserves that work. GoldenMark64 has **not**
independently re-certified those co-op results as part of 1.0.

It remains **beta** in any case because GoldenEye's mission scripting, objectives, AI and cutscenes
were authored around one Bond.

See [`COOP.md`](COOP.md).

## Are bots finished?

Bots are available as a beta feature with skill tiers and behavioral policies, but they are an
optional extension rather than part of the certified retail campaign path.

See [`BOTS.md`](BOTS.md).

## Does it need Wine, Proton, WSL, MSYS2 or Cygwin?

The desktop builds are native. Windows uses mingw-w64; macOS and Linux build native host
executables. Compatibility layers are not required to run the resulting desktop build.

## Is there a no-code installer?

macOS has a double-click setup path and Linux has the `tools/install.sh` path. The Windows
no-code setup package exists as a release candidate, but the main README currently does not claim a
coordinated official downloadable Windows package.

See [`GETTING_STARTED.md`](GETTING_STARTED.md) and
[`WINDOWS_INSTALL.md`](WINDOWS_INSTALL.md).

## Is the source available?

The public repository contains the project's source, patches, tests, documentation and
reconstruction tooling, subject to the provenance boundaries documented in
[`LICENSING.md`](LICENSING.md) and [`THIRD_PARTY.md`](THIRD_PARTY.md).

The root MIT license does not relicense the GoldenEye game, user-supplied ROM data, extracted
assets, or independently licensed third-party source.

## Is this affiliated with Nintendo, Rare or the James Bond rights holders?

No. GoldenEye Native is an independent community project and is not affiliated with or endorsed by
Nintendo, Rare, MGM, Danjaq or EON Productions.
