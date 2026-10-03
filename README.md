# Goldeneye-Native

> [!IMPORTANT]
> **Project maintenance status**
>
> The original `SegfaultEvan/goldeneye-native` repository was deleted on August 29, 2026. This
> fork preserves the latest publicly available version of upstream `main` from before its
> deletion, including the complete Git history through commit `47fe1a1`.
>
> I am not the original maintainer. This fork is a community continuation, not an official handoff
> from the original maintainer. The project has now completed its 1.0 stabilization and human
> campaign-certification pass; development continues here for optional features, portability and
> any new defects that are reported.
>
> Development continues on this fork. Reviewed fixes are merged into a stable, playable `main`
> one at a time and recorded in the [`PATCH_QUEUE.md`](PATCH_QUEUE.md) so they can be offered back
> individually if the original upstream returns. See the
> [community maintenance workflow](docs/MAINTAINING.md) for the branch and replay process.
>
> Found a problem? [Open a guided bug report](https://github.com/seb-patron/goldeneye-native/issues/new/choose).
> Community patches are welcome.

**GoldenEye 007, compiled as a native application for macOS, Linux and Windows.** Mouse and
keyboard. Real widescreen. Hundreds of frames a second with the game still running at the original 1997 animation speed
Rare tuned it to. Online Multiplayer or Co Op, AI Bots, Lua mod pack scripting, horde mode, a free flying photo camera, HD upgrades, and more!

![Silo, from the walkway beside the missile](docs/images/screenshot-01.jpg)

This is not an emulator. It is the game's own source code, from the
[`n64decomp/007`](https://github.com/n64decomp/007) decompilation, built into a real binary for
your machine. There is no N64 being pretended at underneath, which is why the things emulators
can only work around, this one just fixes.

You supply your own legally dumped cartridge. No game data ships here, and none ever will.

## GoldenEye Native 1.0 certification

The 1.0 release line has been completed through human playtesting across **Agent, Secret Agent and
00 Agent**, covering the complete retail campaign. As of October 3, 2026, there are **no known
game-breaking bugs remaining in the tested retail campaign path**. This is a statement about the
tested 1.0 configuration, not a claim that every optional mod, graphics setting, controller or GPU
driver combination is exhaustively certified.

The final Linux stability blocker was an Intel Iris Xe / i915 whole-desktop GPU hang. A dedicated
GPU/function-flight forensic subsystem was built to correlate GoldenEye display-list work with
Mesa/i915 command batches while the machine was wedged. The captured renderer calls returned
normally; the GPU failure occurred asynchronously and was recovered by disabling i915 GuC
submission on the affected Latitude (`i915.enable_guc=0`). See
[`docs/RENDERER_TROUBLESHOOTING.md`](docs/RENDERER_TROUBLESHOOTING.md) and
[`docs/TESTING_1.0.md`](docs/TESTING_1.0.md).

## Documentation

- **New player:** [`docs/GETTING_STARTED.md`](docs/GETTING_STARTED.md)
- **No-code Windows install:** [`docs/WINDOWS_INSTALL.md`](docs/WINDOWS_INSTALL.md)
- **Controls and rebinding:** [`docs/CONTROLS.md`](docs/CONTROLS.md)
- **All settings:** [`docs/CONFIGURATION.md`](docs/CONFIGURATION.md)
- **1.0 certification and fixes:** [`docs/RELEASE_1.0.md`](docs/RELEASE_1.0.md),
  [`docs/TESTING_1.0.md`](docs/TESTING_1.0.md), and [`docs/FIXES_1.0.md`](docs/FIXES_1.0.md)
- **Advanced renderer/GPU troubleshooting:**
  [`docs/RENDERER_TROUBLESHOOTING.md`](docs/RENDERER_TROUBLESHOOTING.md)
- **Contributor source build and workflow:** [`docs/BUILDING.md`](docs/BUILDING.md),
  [`CONTRIBUTING.md`](CONTRIBUTING.md), and
  [`docs/DEVELOPMENT.md`](docs/DEVELOPMENT.md)
- **How the codebase works:** [`docs/CODEBASE.md`](docs/CODEBASE.md)
- **Complete documentation index:** [`docs/README.md`](docs/README.md)

## Install it

### On a Mac

**1.** Get your own GoldenEye 007 ROM and leave it on your **Desktop**. Do not rename it.

**2.** On [the project page](https://github.com/seb-patron/goldeneye-native), click the green
**Code** button, then **Download ZIP**. Double-click the downloaded file to unzip it.

**3.** Open the folder that appears and **double-click `Install on Mac`**.

A window opens and does the rest. It takes 10 to 40 minutes the first time, almost all of it
reading your cartridge, and you can leave it running. If macOS says the file is from an
unidentified developer, right-click it and choose **Open** instead, then **Open** again.

**4.** When it finishes, open `getv/build-mac` and **double-click `GoldenEye.app`** to choose
a mission and settings in the custom launcher.

Keep the app beside the `goldeneye` executable. Drag the app to your Dock for quick access.
`Play GoldenEye` in the repository folder also opens the launcher.

**Optional terminal install.** If you cloned the repository or prefer the terminal, run this from
the repository root instead of step 3:

```bash
bash tools/install.sh
```

It finds a supported ROM on your Desktop or in Downloads. Pass an explicit file only when needed:

```bash
bash tools/install.sh --rom /path/to/your/rom.z64
```

### On Windows

**1.** Have your own supported GoldenEye 007 US big-endian `.z64` ROM dump ready. The setup app
verifies it locally and reads it from the path you select without copying or uploading it. The
project does not supply ROMs or instructions for acquiring one.

**2.** The no-code Windows package is still a release candidate; there is no official download or
release automation yet. When this project's **Releases** page lists a coordinated Windows package,
download that versioned ROM-free setup ZIP there. The setup app opens a normal ROM file picker,
builds everything on your computer, and never uploads your dump. See
[`docs/WINDOWS_PACKAGING.md`](docs/WINDOWS_PACKAGING.md) for the package and test process.

**3.** Extract the ZIP and double-click `GoldenEye-Native-Setup.exe`. Choose an installation folder
and select your ROM. You do not install Git, Python, or a compiler and do not need to handle source
code: the setup app downloads private, checksum-verified portable tools and performs the local
build. It takes 10 to 40 minutes the first time and produces the playable executable only on your
computer. After setup, double-click **GoldenEye** or **Play GoldenEye** in that folder. The game
itself opens the custom launcher, matching the Mac app.

### On Linux

**1.** Install `git` and `python3` from your package manager.

**2.** Download and unzip this repository, or clone it with Git, then open a terminal in that
folder.

**3.** Run:

```bash
bash tools/install.sh
```

It finds a supported ROM on your Desktop or in Downloads and prints the exact package command for
your distribution if anything is missing. Use `--rom /path/to/your/rom.z64` to select the file
explicitly and `--desktop` to add a per-user applications-menu entry.

### If something goes wrong

Nothing here can break your computer or your ROM, and nothing is ever uploaded anywhere.

- **It stopped partway.** Run it again. It picks up where it left off rather than starting over.
- **The Windows setup cannot use your ROM.** Select the supported US big-endian `.z64` dump.
  The setup verifies and reads that file in place without converting or copying it.
- **The Windows setup says a private tool is missing.** Run the setup app again so it can verify
  and repair its portable tool downloads.
- **Anything else.** Open an issue with the last twenty lines it printed. Never attach your ROM
  or a save file.

### What you need, in full

| | |
|---|---|
| Your own GoldenEye 007 cartridge dump | Nothing playable ships here, ever |
| About 4 GB of free disk | The source, the extracted assets and the build |
| 10 to 40 minutes, once | After that, starting the game is instant |

Developers and advanced users can still build from a source checkout on Windows, macOS, or Linux.
Start with [`docs/BUILDING.md`](docs/BUILDING.md) and then read
[`CONTRIBUTING.md`](CONTRIBUTING.md).

## Screenshots

| | |
|---|---|
| ![Facility](docs/images/screenshot-02.jpg) | ![Dam](docs/images/screenshot-03.jpg) |
| ![Multiplayer split screen](docs/images/screenshot-04.jpg) | ![Bunker](docs/images/screenshot-05.jpg) |

![FXAA on and off](docs/images/fxaa-comparison.png)

Captured from this port, not from an emulator, on the default settings unless the caption says
otherwise.

## Which platforms work

| Platform | Renderer | State |
|---|---|---|
| **macOS** (Apple silicon) | OpenGL or native Metal | Builds and plays. Primary target. |
| **Linux** (x86-64 and arm64) | OpenGL | Builds. Verified on Debian 12 aarch64. |
| **Windows** (x86-64) | OpenGL | Builds native mingw-w64. Self-test 16 of 16. |
| **tvOS** (Apple TV) | GL ES or Metal | Builds, signs and deploys to real hardware. |
| **iOS** | Metal | Bring-up. Builds; deploying needs a paired device. |
| **Android** (arm64) | GL ES | Bring-up. The port layer builds for arm64 GLES and carries on-screen touch controls as a virtual pad. Not yet a running game. |

Same source tree everywhere. One build script each.

## The frame-rate fix, which is the reason the rest is possible

GoldenEye counts time in whole video fields. On every emulator ever made, running it faster runs
the *game* faster: guards firing at double speed, ammunition draining, the AI thinking quicker
than it was tuned to. That is baked into the game rather than the hardware, so nobody could fix
it from outside.

It is fixed here. The world keeps its own time while the renderer runs as fast as your machine
allows.

Measured on the Dam, 1280x960, Apple M1, three runs each:

| Profile | Game speed (fields/sec, 60 is correct) | Rendered frames per second |
|---|---|---|
| 97 Console, as shipped | 59.2 | 59 |
| 97 Console, uncapped | 61.0 | 486 |
| GoldenEye+ | 61.0 | 182 |
| GoldenEye+ with an HD texture pack | 60.8 | 177 |

486 frames a second with the game itself ticking at the 60 it should. Bond moves at the speed he
moved in 1997 and the picture is as smooth as your monitor can show.

[`docs/FRAME_TIMING.md`](docs/FRAME_TIMING.md) has the whole account.

## What works

| Feature | State | Detail |
|---|---|---|
| **Fixed frame tick** | **Done** | Uncapped rendering with the world still ticking at 60. Nothing else here is possible without it. |
| **Mouse and keyboard** | **Done** | The default. Real mouse look, tuned and unit-tested. [`MOUSE.md`](docs/MOUSE.md) |
| **Controller support** | **Done** | Xbox, PlayStation and MFi pads through SDL2, plugged in and detected. All 8 retail control styles. |
| **Widescreen and ultrawide** | **Done** | The renderer takes its aspect from the actual framebuffer, so any window shape works, 16:9 through ultrawide. HUD and gun sight corrected. |
| **27 loadable stages** | **Done** | Every mission the cartridge shipped, plus the multiplayer-only arenas. Counted by [`stage_census.sh`](tools/stage_census.sh). |
| **Split screen** | **Done** | Two, three and four players, all 64 characters, the radar, every scenario. |
| **HD texture packs** | **Done** | PNGs named by texture hash, dropped in a folder. Verified end to end at 4x upscale. |
| **Lua scripting and mod packs** | **Done** | Scripted mods loaded from a folder, with a real API. [`MODDING.md`](docs/MODDING.md) |
| **Built-in CRT filter** | **Done** | Scanlines, shadow mask, curvature and vignette, each adjustable. No shader pack to install. |
| **Cheats, built in** | **Done** | The game's own cheat system exposed by name, without the unlock grind. Not GameShark codes. [`CHEATS.md`](docs/CHEATS.md) |
| **Game mode presets** | **Done** | `classic`, `hardcore`, `survival`, `chaos` and `horde` rulesets, freely combinable. |
| **Enemy gibs** | **Beta** | `off`, explosion, high-damage or always policies feeding one Quake-like effect: solid chunks bounce, settle and linger without changing gameplay. [`GIBS.md`](docs/GIBS.md) |
| **Launcher modes** | **Done** | `Base Game` for the original experience; `GoldenEye+` for enhanced graphics and optional customization. |
| **Toggle aim** | **Done** | Press once to raise the sight instead of holding the button. The game's own option, exposed as `aim_toggle`. |
| **Coloured reticle** | **Done** | Any RRGGBB, and a smaller modern sight size. How cleanly a colour takes depends on the baked asset. |
| **Post-processing** | **Done** | FXAA, MSAA to 4x, supersampling, anisotropic filtering, mipmaps, parallax mapping. |
| **Two renderers** | **Done** | OpenGL everywhere, plus a native Metal backend on Apple platforms. Not MoltenVK. |
| **Launcher** | **Done** | A window for level, ruleset, cheats and video settings. No config file needed. |
| **Real-font text** | **Done** | Optional crisp menu text through a TrueType atlas instead of stretched 24-pixel glyphs. Off by default. |
| **Saves** | **Done** | Persistent, in your platform's normal application-data directory. |
| **Around 250 settings** | **Done** | Every development gate settable by name, from the config file or the command line. |
| **Free camera** | **Beta** | Photo mode. `F8` unpins the camera from Bond and hands you the room: fly it on `W` `A` `S` `D`, look with the arrows, rise and drop on `R` and `F`, hold shift or ctrl to change pace. The camera is Rare's own -- a six-degree fly camera that shipped inside the cartridge and was never reachable from a controller. Visibility still roots at Bond, so leaving his room culls the world behind you. |
| **Bots** | **Beta** | Fightable opponents with skill tiers that vary five dials, not one. [`BOTS.md`](docs/BOTS.md) |
| **Horde mode** | **Beta** | Waves of enemies. Never in the original. [`COOP.md`](docs/COOP.md) |
| **Co-op** | **Beta** | Two to four players through a solo mission's own geometry, objectives and cutscenes. Everyone gets their own spawn and camera, nobody can shoot a team mate, one death does not lose the run, and the player who went down is back on their feet five seconds later. Still beta because the mission is authored around one Bond, so extra players are present rather than accounted for. [`COOP.md`](docs/COOP.md) |
| **LAN multiplayer** | **Beta** | Connects, exchanges input, completes a real session over UDP. It also desyncs. Read the limitation before planning an evening around it. [`NETPLAY.md`](docs/NETPLAY.md) |

## Which game exactly

| | |
|---|---|
| Game | GoldenEye 007 |
| Revision | US retail, SHA-1 `abe01e4aeb033b6c0836819f549c791b26cfde83` |
| Status | The only revision built and tested |

The Windows setup verifies a big-endian z64 dump against that SHA-1 before building. A valid
byte-swapped dump is recognized but refused because the no-copy setup does not create a converted
ROM. A wrong or damaged file is refused before it can produce a broken build twenty minutes later.

## GoldenEye+ versus Base Game

The launcher offers two modes, with a description b‹­¦ëm®éÜj×¢¸ Šv¥jšk£¦j×­¢G§r‹§·]4ç}Ë˜İ\İÛHØ[Y\^H™\Ù]ËÛË[Ü[™™]^KˆÙ^X›Ø\™[™[İ\ÙHİ[ÛÜšÈ\È[œ]Y\\œËÚ]Û\ÜÚXÈ[İ\ÙH™\ÜÛœÙKˆ™\ÛÛ][Ûˆ[™[ØÜ™Y[ˆ™[XZ[ˆY\İX›NÈÛÛ™›Xİ[™ÈÜ[ÛœÂ˜\™H\ØX›Yˆ^\İ[™ÈØ]™H›ÙÜ™\ÜÈ[™HÜšYÚ[˜[[‹YØ[YHY[\È™[XZ[ˆ]˜Z[X›K‚‚ŠŠ‘ÛÛ[‘^YJÊŠˆ[İÜÈİ\İÛZ^˜][Û‹ˆÛˆHZ\ÜÚ[ÛˆYÙKÚÛÜÙH
Š“ÜšYÚ[˜[Ø[YHİ\
ŠˆÜ‚ŠŠ“Z\ÜÚ[ÛˆÙ[XİÜŠŠ‹ˆHZ\ÜÚ[Ûˆ\İİ^\Èš\ÚX›H]Ü™^YYİ]Ú[ˆÜšYÚ[˜[İ\\\ÂœÙ[XİYˆH
Š‘Ø[Y\^JŠˆYÙHÛÛZ[œÈØ[Y\^H™\Ù]È[™Ü[Û˜[œ][Y™™XİË‚‚•HÛÛ™šYË[Û›H™\Ù]H˜Z][[™™\Ù]H\Ø™]Z[ˆZ\ˆ^\İ[™ÈÜ˜\XÜË\™\Ù]˜™Z]š[Ü‹ˆH][˜Ú\‰ÜÈİšXİ\ˆ˜\ÙHØ[YH[ÙH\È\YYY\ˆÛÛ™šYÈ[™ÛÛ[X[™[[™H\œÚ[™Ë‚‚ŠŠ‘ÛÛ[‘^YJÊŠˆİ\Y\È\ÙHÜ˜\XÜÈY˜][Î‚‚Ÿ\›œÈÛˆ˜[YHŸKK_KK_Ÿİ\\œØ[\[™ÈŸTĞPHŸ[š\Ûİ›ÜXÈš[\š[™ÈŸZ\X\ÈÛˆŸ^\™\ÈÛˆŸ\˜[^X\[™ÈÛˆŸ–PHÛˆŸÜ›ÜÜÚZ\ˆØØ[H‹HÛX[\ˆ[Ù\›ˆ™]XÛHŸœ˜[YH˜]H[˜Ø\YÛˆH™X[ÛØÚÈ‚[][™ÈH›Ùš[HØ[Y]›İ[™[™XYHÙ][ˆ[İ\ˆÛÛ™šYÈ\È˜[YYÛˆİİ]]İ\\œ˜]\ˆ[ˆ\ÜÙYİ™\ˆ]ZY]K™XØ]\ÙHH™\Ù]]Ú[[HXÛ[™YÈ[˜Ø\Hœ˜[YH˜]B›ÛÚÜÈ^XİHZÙHH™\Ù]]Y›İÛÜšË‚‚ˆÈÈH][˜Ú\‚‚“ÛˆÚ[™İÜËİX›KXÛXÚÚ[™ÈÛÛ[™^YK™^XÜ[œÈHÚ[™İÈ›ÜˆÚÛÜÚ[™ÈH]™[H[\Ù]ÚX]Â˜[™šY[ÈÙ][™ÜÈ™Y›Ü™HHØ[YHİ\ËˆÛˆXXÓÔËİX›KXÛXÚÂ˜Ù]‹ØZ[[XXËÑÛÛ[‘^YK˜\ÈH›Ü›X[Z[Ü™X]\È\È\[™˜‹ÙÙ]‹ØZ[ÛXXËœÚ[™XYÈ]È[ˆ^\İ[™ÈZ[‚‚˜K[][˜Ú\˜Ü[œÈHÚ[™İÈ›ÜˆÚÛÜÚ[™ÈH]™[H[\Ù]ÚX]È[™šY[ÈÙ][™ÜÈ™Y›Ü™HB™Ø[YHİ\Èœ›ÛHH[^ÜˆZ[ˆXXÓÔÈ^Xİ]X›KÛÈ›Û™HÙˆ\È™YYÈHÛÛ™šYÈš[K‚‚ŸŸKK_KK_ŸVÓ][˜Ú\ˆÛÛ›ÛÈYÙWJØÜËÚ[XYÙ\ËÛ][˜Ú\‹XÛÛ›ÛËœ™ÊHVÓ][˜Ú\ˆÔ•YÙWJØÜËÚ[XYÙ\ËÛ][˜Ú\‹XÜœ™ÊH‚ˆVÓ][˜Ú\ˆ[ÙÈYÙWJØÜËÚ[XYÙ\ËÛ][˜Ú\‹[[ÙËœ™ÊB‚’]\ÈH\Ù\ˆ[\™˜XÙHİ™\ˆHÙ][™ÜÈ][™XYH^\İY˜]\ˆ[ˆ™]ÈØ\Xš[]Nˆ]™\B˜ÛÛ›Û™\ÛÛ™\ÈÈHØ]H]ÛÜšÙYœ›ÛHHÚ[[™XXÚÛ™HÜ[œÈÚİÚ[™ÈH˜[YHB˜ÛÛ™šYÈ^Y\ˆ\İ™\ÛÛ™YÛÈ]™Y›XİÈ[İ\ˆÛÛ™šYÈš[H˜]\ˆ[ˆÛÛ\][™ÈÚ]]‚‚’]™\İ\ÈHØ[YHÈ\K[X™\˜][Kˆ[ÜİÙˆHÜ	ÜÈÙ][™ÜÈ\™H™XYÛ˜ÙHÛˆš\œİ\ÙKÛÈÚ[™Ú[™ÈÛ™HY\ˆHØ[YH\Èİ\YÙ\È›İ[™ËÚ[[KˆH][˜Ú\ˆÙ]ÈB™[š\›Û›Y[[™™KY^Xİ]\ËÛÈHØ[YH™YÚ[œÈ[ˆH›ØÙ\ÜÈÚ\™H›İ[™È\È™Y[ˆ™XYY]‚‚ˆÈÈš\œİ][˜Ú‚˜˜\Ú‹‹ÙÙ]‹ØZ[[XXËÙÛÛ[™^YHÈXXÓÔÂ‹‹ÙÙ]‹ØZ[[[^ÙÛÛ[™^YHÈ[^™Ù]—Z[]Ú[™İÜ×ÛÛ[™^YK™^HÈÚ[™İÜÂ˜‚•Ú[™İÜÈÜ[œÈHÙ][™ÜÈ][˜Ú\ˆHY˜][ÈÑU—ÓUSÒTL\ÈH^XÚ]\\ÜÈ›ÜˆBœØÜš\Y\™Xİİ\ˆYK[][˜Ú\˜ÈH[^ÜˆZ[ˆXXÓÔÈš[˜\KˆÛˆ[^H]™[Ü\‚œÛİ\˜ÙKXZ[ØÜš\Ø[ˆ[ÛÈ™YÚ\İ\ˆH\ÚİÜ[KÛÈHØ[YHÚİÜÈ\[ˆ[İ\ˆ\XØ][ÛœÂ›Y[NÈ]\ÚÜÈš\œİ[™™[[İš[™È]Û™Hš[H[œ™YÚ\İ\œÈ]‚‚”Ù][™ÜÈ]™H[ˆHZ[ˆ^š[H]HØ[YHÜš]\ÈÛˆš\œİ[‹[™]š[ÈH]]\ÙY‚–ØØÜËĞÓÓ‘’QÕTUSÓ‹›YJØÜËĞÓÓ‘’QÕTUSÓ‹›Y
H\İÈ]™\HÙ^K‚‚ˆÈÈYˆ]Ù\È›İÛÜšÂ‚ŠŠ”Ù]\ÜˆHÛİ\˜ÙHZ[İÜYŠŠˆ]˜[Y\ÈHİ\[™H™X\ÛÛ‹ˆ™K\[›š[™È\ÈØY™H[™œ™\İ[Y\ËˆHÛÛ[[Û™\İØ]\Ù\È\™HHZ\ÜÚ[™È™\™\]Z\Ú]HÜˆH“ÓH]Ûİ[›İ™\šYK‚‚ŠŠ’]Ø^\È]Ø[››İš[™[İ\ˆ“ÓKŠŠˆ]]ÛˆH\ÚİÜÚ]H›Üˆ™^[œÚ[Û‹Üˆ\ÜÈH]\™XİNˆ˜\ÚÛÛËÚ[œİ[œÚK\›ÛHÜ]İËÜ›ÛK‚‚ŠŠ’]Z[]Ú[›İİ\ŠŠˆ[ˆHÙ[‹]\İ[™™\ÜÚ]]Ø^\Î‚‚˜˜\Ú˜˜\ÚÙ]‹ÜÜİ\İËÜ[—İ\İËœÚ˜‚ŠŠ”™\Ü[™ÈH›Ø›[KŠŠˆ\ÙHB–ÙİZYY\ÜİYH›Ü›\×JÎ‹ËÙÚ]X‹˜ÛÛKÜÙX‹\]›Û‹ÙÛÛ[™^YK[˜]]™KÚ\ÜİY\ËÛ™]ËØÚÛÜÙJH[™[˜ÛYB[İ\ˆ^XİÛÛ[Z]]›Ü›KH›İ\ˆZ[Ûİ[ÈH[œİ[\ˆš[Y[™Hš\œİYX[š[™Ù[™\œ›Ü‹ˆ™]™\ˆ]XÚH“ÓKHØ]™KÜˆ[][™È^˜XİYœ›ÛHÛ™NÈ\ÜİY\ÈÛÛZ[š[™ÈØ[YH]HÙ]˜ÛÜÙYÚ]İ]™Z[™È™XY‚‚ˆÈÈÛÛ›ÛÂ‚’Ù^X›Ø\™[™[İ\ÙHÛÜšÈ[Û™ÜÚYH[HÑ‹XÛÛ\]X›HÛÛ›Û\‹ˆHÛÜ™HÙ^X›Ø\™X\\ÈĞTÑÈ[İ™K\œ›İÜÈÜˆ[İ\ÙHÈÛÚËÜXÙXÛY[İ\ÙHÈš\™KXÜšYÚ[İ\ÙHÈZ[KXÜˆ˜È\ÙK˜ÈŞXÛHÙX\ÛœËX˜È]\ÙKØÛYÚYÈÜ›İXÚ[™˜Èİ[™‚‚‘Ø[Y\YXİ[ÛœÈØ[ˆ™H™X›İ[™ÛØ˜[HÜˆ\ˆ^Y\ˆ[ˆH][˜Ú\‹ÛÛ™šYÈš[KÜˆÛÛ[X[™›[™KˆH\ÚXØ[Ù^X›Ø\™^[İ]\Èİ\œ™[Hš^YÈ]\È›È\˜š]˜\HÙ^KXš[™[™ÈRKˆ[™ZYÚ™]Z[ÛÛ›Ûİ[\È\™Hİ\ÜY[™HÜY˜][ÈÈHX[X[˜[ÙÈ‹ŒˆØ[Ü™X›^[İ]›ÜˆH[Ù\›ˆÛË\İXÚÈY‚‚•H[X\Ø[Y\Y]Ûˆ˜[Y\Ë\‹\^Y\ˆ^[\\Ë[İ\ÙHÙ][™ÜË[™]™HXXŒLXœÚÜİ]È\™H[ˆØØÜËĞÓÓ•“ÓË›YJØÜËĞÓÓ•“ÓË›Y
K‚‚•Ú]ÑU—Ñ”‘QPĞSOLX[™ÈHØ[Y\˜HÈ[İKˆ]›Y\ÈÛˆHØ[YHØXØÛÚÜÂÚ]H\œ›İÜËš\Ù\È[™›ÜÈÛˆ˜[™˜[™Ú[™Ù\ÈXÙHÚ[HÚYÜˆİ›\È[‚•ÜÙHÙ^\ÈÙY\Z\ˆ›Ü›X[›ØœÈ]HØ[YH[YHKH˜İ[ŞXÛ\ÈÙX\ÛœÈ[™\›™X][İHKB˜™XØ]\ÙHHØ[YHÙ\È›İ]\ÙHÚ[H[İH›KˆYØZ[ˆÚ]™\ÈHØ[Y\˜H˜XÚÈÈ›Û™‚‚“[İ\ÙHÙ[œÚ]]š]H[™[™\œÚ[Ûˆ\™HY\İX›NÈH\š]Y]XÈ™Z[™[İ\ÙHÛÚÈ\ÈÜš][ˆ\˜[™[š]]\İY[ˆØØÜËÓSÕTÑK›YJØÜËÓSÕTÑK›Y
K‚‚ˆÈÈ™\ÛÛ][Û‹\™›Ü›X[˜ÙH[™İÈ]ÛÚÜÂ‚”™\ÛÛ][Û‹İ\\œØ[\[™ËTĞPK[š\Ûİ›ÜXÈš[\š[™Ë–PH[™HÔ•š[\ˆ\™H[[ˆB›][˜Ú\ˆ[™[ˆHÛÛ™šYÈš[KˆÛˆ[ˆLHHØ[YH[œÈ]Ù]™\˜[[™™Yœ˜[Y\ÈHÙXÛÛ™]ŒLMŒÚ]HÛÛ[‘^YJÈ›Ùš[HÛ‹ÛÈ[ÜİÙˆ\ÙHÛÜİ›İ[™È[İHÚ[›İXÙK‚‚’^\™HXÚÜÈÛÈ[ˆH›Û\ˆ[™\™HX]ÚYH^\™H\Úˆ›Û™HÚ\È\™K›ÜˆHØ[YBœ™X\ÛÛˆ›È“ÓHÙ\ËˆHXXÚ[™\H\È™\šYšYYˆHLÌˆ^\™HØ\È™\XÙYHHLLLœXÚÈ[XYÙH][™™[™\™Y‚‚–ØØÜËÔT‘“Ô“PSÑK›YJØÜËÔT‘“Ô“PSÑK›Y
H\ÈHYX\İ\™[Y[Ë‚‚ˆÈÈ™\›ÙXÚX›HØØ[\ÜÙ]›İ[™\B‚•Hİ\œ™[˜XÚÙY™YH\È\ÚYÛ™Y›İÈİÜ™HH“ÓK^˜XİY\ÜÙ]ÜˆXÛÛ\[YØ[YBœÛİ\˜ÙKˆÚ]\ÈİÜ™Y\ÈHÜ^Y\‹Z[ØÜš\Ë[™]Ú\È\YYÈHXÛÛ\[][Û‚[İHÛÛ™H[İ\œÙ[‹ˆÛ™HÙ[™\˜]YÙÛÈXÛ\˜][Ûˆ™YYÈH˜]]™H]K[Ü™\ˆÛÛ™\œÚ[ÛÈBš[œİ[\ˆ\™›Ü›\È]Û›HÛˆHYÛ›Ü™Y\ÜÙ]^˜XİYœ›ÛHH\Ù\‰ÜÈ“ÓK˜]\ˆ[‚˜Ø\œZ[™ÈZ]\ˆ™\™\Ù[][Ûˆ[ˆH]ÚˆÛÛËØÚXÚ×Ü]Ú\ËœÚ™\šYšY\ÈH™[XZ[š[™Âœ]Ú\Ë[™ÛÛËØÚXÚ×Û›×ÙØ[YWÙ]KœX™Z™XİÈİ\ÜXÚ[İ\È[œÙH^YXÚ[X[\œ˜^\È\ÈÙ[\ÂšÛ›İÛˆØ[YKX\Y˜XİÚ\\Ëˆ\ÙHÚXÚÜÈ\™HØY™YİX\™Ë›İH›[šÙ]YØ[ÛÛ˜Û\Ú[Û‹‚‚ˆÈÈÛ›İÛˆ[Z]][ÛœÂ‚•Üš][ˆZ[›K™XØ]\ÙHH‘PQQH]İ™\œÙ[È\ÈÛÜœÙH[ˆÛ™H][™\œÙ[Ë‚‚‹H
Š“™]ÛÜšÈ^H\Ş[˜ÜËŠŠˆH˜[œÜÜ\ØÛİ™\K][˜Ú\ˆYÙH[™Ø[YK[ÛÜXÚÈ\™H[ˆÚ\™Y[™ÛÈXXÚ[™\ÈÈÛÛ\]HH™X[Ù\ÜÚ[Ûˆİ™\ˆQˆ]^HšYİ]ÙˆŞ[˜ÈÚ]ˆ›Ø›ÙHİXÚ[™ÈHÛÛ›Û\ˆš]™HšX[ÈÙˆÛÈ›ØÙ\ÜÙ\ÈÚ]›È[œ]™\›ÈYÜ™YYˆÛÂˆØ]\Ù\È]™H™Y[ˆ›İ[™[™š^Y[™™Z]\ˆØ\ÈİY™šXÚY[ˆ™X]Sˆ^H\ÈÛÛY][™ÈÂˆ^\š[Y[Ú]›İÈ[ˆ[ˆ]™[š[™È\›İ[™ˆØØÜËÓ‘UVK›YJØÜËÓ‘UVK›Y
B‹H
Š“[^\È[œ^YYŠŠˆ]Z[È[™™[™\œÎÈ›Ø›ÙH\È^YYHZ\ÜÚ[Ûˆ›İYÚÛˆ]‚‹H
Š•Ú[™İÜÈ\È[œ^YYÛËŠŠˆ]Z[Ë›ÛİË\ÜÙ\ÈHÙ[‹]\İMˆÙˆM‹[™HÙ][™ÜÂˆ]™HÚ[˜ÙH™Y[ˆYX\İ\™Y\™H˜]\ˆ[ˆ\Üİ[YYˆšY[ÙˆšY]ËÜ›ÜÜÚZ\ˆØØ[KHÔ•ˆš[\ˆ[™^\™HXÚÜÈ[ZÙHY™™Xİ[™™]^HÜ[œÈHÙ\ÜÚ[Û‹ˆÚ]›Ø›ÙH\ÈÛ™BˆÛˆÚ[™İÜÈ\ÈÚ]İÛˆ[™^HHZ\ÜÚ[Ûˆ›İYÚ‚‹H
Š•HRMHÜ™\İ\ÈZ\ÜÚ[™ÊŠˆÛˆH][\^Y\ˆÚ\˜Xİ\ˆÙ[XİˆHØ[YH\ÜÙ]™[™\œÂˆÛÜœ™XİHÛˆHš[K\Ù[XİØÜ™Y[‹ÛÈHXÛÙH]\ÈÛİ[™[™H˜][\È[Ù]Ú\™K‚‹H
Š”Ù[Xİš[H˜]ÜÈH›]›XÚÈ˜XÚÙÜ›İ[™
ŠˆÚ\™HHÜšYÚ[˜[\ÈH˜Z[Ø]\›X\šË‚‹H
Š”ÛÛYH][\^Y\ˆYÙHØ\Ù\ÊŠˆ\™H[™[™›Ü˜ÙYÛˆHXY\ÜÈ][˜ÛY[™ÈØÛÜ™HØ\Ë‚‹H
Š“ÔËSÔÈ[™[™›ÚY\™Hœš[™Ë]\
Š‹›İ›ÙXİËˆ“ÔÈ[™SÔÈZ[[™\ŞNÈ[™›ÚYˆ\ÈHÜ^Y\ˆ]ÛÛ\[\È›Üˆ\›MÓTÈ[™İXÚÛÛ›ÛË[™\È›İ™Y[ˆ[‹‚‚–ØØÜËÔ“ĞQPT›YJØÜËÔ“ĞQPT›Y
HØ\œšY\ÈH[\İ[™Ú]\È[›™Y‚‚ˆÈÈœ™\]Y[H\ÚÙY]Y\İ[ÛœÂ‚ŠŠ’\È\È[ˆ[][]ÜÊŠˆ›Ëˆ[ˆ[][]Üˆ[\œ™]ÈXXÚ[™HÛÙH[™™][™ÈÈ™HBš\™Ø\™Kˆ\È\ÈHØ[YIÜÈİÛˆËÛÛ\[Y›Üˆ[İ\ˆ›ØÙ\ÜÛÜ‹˜]Ú[™È›İYÚ[İ\ˆÔKˆ]š\ÈÚHHœ˜[YK\˜]H›Ø›[H\Èš^X›H\™H[™›İ\™K‚‚ŠŠ‘ÈH™YYH“ÓOÊŠˆY\Ë[İ\ˆİÛ‹ˆ›İ[™È^XX›HÚ\È\™KˆH[œİ[\ˆ™XYÈ[İ\ˆÛÜK™^˜XİÈH\ÜÙ]È]™YYÈÛˆ[İ\ˆXXÚ[™K[™™]™\ˆ\ØYÈ[][™Ë‚‚ŠŠ’\È]YØ[ÊŠˆ\È›Ú™XİÙ\È›İXZÙHH›[šÙ]YØ[ÛZ[KˆH›İ™[˜[˜ÙH™XÛÜ™[‚–ØØÜËÓPÑS”ÒS‘Ë›YJØÜËÓPÑS”ÒS‘Ë›Y
HY[YšY\È[œ™\ÛÛ™Y\İ™X[H[™™[™\™\ˆ]Y\İ[ÛœË˜[™HZ[™\]Z\™\ÈHØ\šYÙH[\İ\YYØØ[HHH\Ù\‹ˆÛÛ[‘^YHÈ[™]Â˜Y[X\šÜÈ™[Û™ÈÈZ\ˆİÛ™\œÎÈ\È›Ú™Xİ\È[˜Y™š[X]YÚ]š[[™Ë˜\™KQÓHÜˆSÓ‹‚‚ŠŠ•Ú[][ˆÛˆ^HXXÚ[™OÊŠˆYˆ]\ÈHÔHœ›ÛHH\İXØYH[™[œÈXXÓÔË[^Ü‚•Ú[™İÜË[[ÜİÙ\Z[›Kˆ]\È›İ[X[™[™ÎÈHÜšYÚ[˜[\™Ù]YNNMˆ\™Ø\™K‚‚ŠŠØ[ˆH^HÚ]HÛÛ›Û\ÊŠˆY\Ë[™Ú][İ\ÙH[™Ù^X›Ø\™[™Ú][ZYÚÙˆB™Ø[YIÜÈÜšYÚ[˜[ÛÛ›Ûİ[\Ë‚‚ŠŠ‘Ù\È][\^Y\ˆÛÜšÏÊŠˆÜ]ØÜ™Y[‹Y\Ë[Kˆİ™\ˆH™]ÛÜšË›İ™[XX›HY][™B’Û›İÛˆ[Z]][ÛœÈÙXİ[ÛˆØ^\È^XİHİÈ˜\ˆ]Ù]Ë‚‚ŠŠ•ÚH\È]Ø[YÛÛ[‘^YJÏÊŠˆ]\ÈH˜[YH›ÜˆH›Ùš[H]\›œÈÛˆ]™\][™È\ÂœÜYËˆH˜Z][›Ùš[H\Èİ[HY˜][‚‚ŠŠØ[ˆH\ÙH^\™HXÚÜÏÊŠˆY\Ëˆ›Ü‘ÜÈ˜[YYH^\™H\Ú[ÈHXÚÈ›Û\‹ˆ›Û™Bš\È[˜ÛYY‚‚ŠŠ•Ú\™HÈØ]™\ÈÛÏÊŠˆ[İ\ˆ]›Ü›IÜÈ\İX[\XØ][Û‹Y]H\™XİÜKˆH^Xİ]\Âœš[Y]İ\\‚‚ŠŠ•ÚHÙ\™H^H^ÜÚ[ÛœÈ˜Z[˜›İËXÛÛİ\™YÊŠˆ^HÙ\™HXÛÙ[™È‘ĞLMˆ^[È[ˆHÜ›Û™È]B›Ü™\‹ÚXÚ\›™YÜ˜[™ÙH[ÈXYÙ[H[™™XYÛˆØÜ™Y[ˆ\ÈÛÛ™™]Kˆ]\Èš^Y[™ÛˆB™Y˜][›İËˆ]Ø\È™]™\ˆHZ[˜[ÚX]ÚXÚ\ÈÚ]]Ù]ÈZ\İZÙ[ˆ›Ü‹‚–ØØÜËĞÓÓÕT—Ğ•QÔË›YJØÜËĞÓÓÕT—Ğ•QÔË›Y
H\ÈHYX\İ\™[Y[Ë‚‚ŠŠØ[ˆ›İ\ˆÙˆ\È^HÛˆÛ™HØÜ™Y[ÊŠˆY\ËˆÛË™YH[™›İ\ˆ^Y\ˆÜ]ØÜ™Y[‹]™\BœØÙ[˜\š[Ë[Ú\˜Xİ\œÈ[™H˜Y\‹‚‚ŠŠ‘Ù\È]™YY[ˆ[\›™]ÛÛ›™Xİ[ÛÊŠˆ›Ëˆ›İ[™È\™HÛ™\ÈÛYK[™H[œİ[\ˆÛ›Bœ™XXÚ\ÈH™]ÛÜšÈÈÛÛ™HHXÛÛ\[][Ûˆ[™™]ÚZ[\[™[˜ÚY\Ë‚‚ŠŠØ[ˆHÚ[™ÙHHšY[ÙˆšY]ÏÊŠˆY\Ë]™KÚ]İ]™\İ\[™Ë‚‚ŠŠ’\È\™HHXYÈY[OÊŠˆY\Ë[™HİYÙHÙ[œİ\Ë™[™\‹\™Y™\™[˜ÙH[™[œ]]˜XÙHÛÛÈBœ›Ú™Xİ\Ù\ÈÛˆ]Ù[ˆ\™H[[ˆÛÛËØ‚‚ˆÈÈ›Ú™XİX\‚Ÿ]Ú]\È[ˆ]ŸKK_KK_ŸØÛÛËÚ[œİ[œÚJÛÛËÚ[œİ[œÚ
HH]™[Ü\ˆÛİ\˜ÙKXZ[›Ûİİ˜\›ÜˆXXÓÔÈ[™[^ˆŸØÛÛËÚ[œİ[œÌXJÛÛËÚ[œİ[œÌJHH]™[Ü\ˆÛİ\˜ÙKXZ[›Ûİİ˜\›ÜˆÚ[™İÜËˆŸØÛÛËÜXÚØYÙWİÚ[™İÜ×İÚ^˜\™œÌXJÛÛËÜXÚØYÙWİÚ[™İÜ×İÚ^˜\™œÌJHZ[È[™ØY™]KXÚXÚÜÈH“ÓKYœ™YHÚ[™İÜÈÙ]\XÚØYÙKˆŸØÙ]‹ÜÜØJÙ]‹ÜÜÊHHÜ^Y\ˆ™[™\™\‹[œ]]Y[ËØ]™\Ë™]^KˆŸØÙ]‹ÜÜÙ˜\İÙØJÙ]‹ÜÜÙ˜\İÙÊHH\Ü^K[\İ™[™\™\‹Ü[‘Ó[™Y][˜XÚÙ[™ËˆŸØÙ]‹Ü]Ú\ËØJÙ]‹Ü]Ú\ËÊH]™\HÚ[™ÙHXYHÈHXÛÛ\[][Û‹\È[X™\™Y]Ú\ËˆŸØÙ]‹ÜÜİ\İËØJÙ]‹ÜÜİ\İËÊHHÙ[‹]\İİZ]KˆŸØØÜËÑÑUS‘×ÔÕT•Q›YJØÜËÑÑUS‘×ÔÕT•Q›Y
H[œİ[][˜ÚÛÛ™šYİ\™K\]K[™š\œİÚXÚÜËˆŸØØÜËÕÒS‘ÕÔ×ÒS”ÕS›YJØÜËÕÒS‘ÕÔ×ÒS”ÕS›Y
H›ËXÛÙHÚ[™İÜÈ[œİ[][Ûˆ[™š\œİ[‹ˆŸØØÜËĞ•RSS‘Ë›YJØÜËĞ•RSS‘Ë›Y
H]™[Ü\ˆÛİ\˜ÙKXZ[[HÚ[È›ÜˆÚ[™İÜËXXÓÔË[™[^ˆŸØØÜËĞÓÓ•“ÓË›YJØÜËĞÓÓ•“ÓË›Y
HÙ^X›Ø\™[İ\ÙKØ[Y\Y™Xš[™[™Ë[™ÚÜİ]ËˆŸØØÜËĞÓÑPTÑK›YJØÜËĞÓÑPTÑK›Y
H\˜Ú]Xİ\™K[[YH›İË[™Ú\™HÚ[™Ù\È™[Û™ËˆŸØØÜËÑU‘SÔQS•›YJØÜËÑU‘SÔQS•›Y
HHØØ[Y]Z[\İ[™˜[Y][ÛˆÛÜˆŸØØÜËÔÑUT›YJØÜËÔÑUT›Y
HX[X[XXÓÔÈZ[[\›˜[È[™Y\›İX›\ÚÛİ[™ËˆŸØØÜËĞÓÓ‘’QÕTUSÓ‹›YJØÜËĞÓÓ‘’QÕTUSÓ‹›Y
H]™\HÙ][™ËÚ]Ú]]Ù\ËˆŸØØÜËÑ”SQWÕSRS‘Ë›YJØÜËÑ”SQWÕSRS‘Ë›Y
HHœ˜[YK\˜]Hš^[ˆ[ˆŸØØÜËÓSÑS‘Ë›YJØÜËÓSÑS‘Ë›Y
HXH[ÙÈ[™^\™HXÚÜËˆŸØØÜËĞ“ÕË›YJØÜËĞ“ÕË›Y
HH›İŞ\İ[KˆŸØØÜËÓ‘UVK›YJØÜËÓ‘UVK›Y
H™]ÛÜšÈ^K[˜ÛY[™ÈÚ]\Èœ›ÚÙ[ˆ[™ÚKˆŸØØÜËÔT‘“Ô“PSÑK›YJØÜËÔT‘“Ô“PSÑK›Y
HYX\İ\™[Y[ËˆŸØØÜËÔ“ĞQPT›YJØÜËÔ“ĞQPT›Y
Hİ]KÛ›İÛˆ\ÜİY\Ë[›™YÛÜšËˆŸØØÜËÓPRS•RS’S‘Ë›YJØÜËÓPRS•RS’S‘Ë›Y
HÛÛ[][š]HÛÜšÙ›İÈ[™]\™H\İ™X[H™\^KˆŸØØÜËÕÒS‘ÕÔ×ÔPÒĞQÒS‘Ë›YJØÜËÕÒS‘ÕÔ×ÔPÒĞQÒS‘Ë›Y
HÚ[™İÜÈXÚØYÙH›İ[™\KZ[ÒH\Y˜Xİ[™\İÚXÚÛ\İˆŸØUÒÔUQUQK›YJUÒÔUQUQK›Y
HÛ™KXK[Û™H[™^ÙˆÛÛ[][š]Hš^\ËˆŸØØÜËÓPÑS”ÒS‘Ë›YJØÜËÓPÑS”ÒS‘Ë›Y
H]™\H\™\\HÛÛ\Û™[[™]ÈXÙ[˜ÙKˆ‚ˆÈÈZ[[™Èœ›ÛHÛİ\˜ÙB‚”Ûİ\˜ÙHZ[È\™H›Üˆ]™[Ü\œÈ[™Y˜[˜ÙY\Ù\œÈÚ]HÚXÚÛİ][™]›Ü›HÛÛÚZ[‹ˆBœİ\ÜY[HÛÛ[X[™Ë™\™\]Z\Ú]\Ëİ]]ØØ][ÛœË[™ØY™]H›İ[™\H\™H[‚–ØØÜËĞ•RSS‘Ë›YJØÜËĞ•RSS‘Ë›Y
KˆH^]\İ]™HX[X[XXÈ\[[™H[™]È›İX›\ÚÛİ[™Â››İ\È™[XZ[ˆ[ˆØØÜËÔÑUT›YJØÜËÔÑUT›Y
K‚‚ˆÈÈÛÛšX][™Â‚”™XYØÓÓ•’P•US‘Ë›YJÓÓ•’P•US‘Ë›Y
Hš\œİˆHÚÜ™\œÚ[ÛˆØ^HÚ][İHYX\İ\™Y[‚HÙ[‹]\İ[™[ˆÛÛËØÚXÚ×Ü]Ú\ËœÚYˆ[İHİXÚY[][™È[ˆÙ]‹Ü]Ú\ËØˆš[B›Û™H›Ø›[H\ˆÚ\ÜİYWJÎ‹ËÙÚ]X‹˜ÛÛKÜÙX‹\]›Û‹ÙÛÛ[™^YK[˜]]™KÚ\ÜİY\ËÛ™]ËØÚÛÜÙJH[™ÙY\™XXÚ[™\]Y\İÈÛ™HÙÚXØ[š^ÛÈ]™[XZ[œÈ™]šY]ØX›H[™[™\[™[H™\^XX›K‚‚YÙ[X\ÜÚ\İYÛÛšX][ÛœÈ\™HÙ[ÛÛYKˆØØÜËĞQÑS•P×ĞÓÓ•’P•US‘Ë›YJØÜËĞQÑS•P×ĞÓÓ•’P•US‘Ë›Y
B™Øİ[Y[ÈH™\ÜÚ]ÜH[œİXİ[ÛœËYË\™\ÜÚÚ[[\™\]Y\İÚÚ[[™[X[ˆ™]šY]ÈØ]\Ë‚‚“™]™\ˆ[˜ÛYHÜˆ]XÚH“ÓK]™\‹ˆ™]™\ˆ]XÚHØ]™Hš[HÜˆ^˜XİYØ[YH]HÈ[ˆ\ÜİYB›ÜˆH[™\]Y\İ‚‚ˆÈÈXÙ[œÚ[™È[™Ü™Y]Â‚•HÜ^Y\ˆ\™H\Èİ\œËˆ]™\][™È™[™Ü™Y\È\İYÚ]]ÈXÙ[˜ÙH[‚–ØØÜËÓPÑS”ÒS‘Ë›YJØÜËÓPÑS”ÒS‘Ë›Y
K[˜ÛY[™ÈH˜\İÑ™[™\™\ˆ[™]Y[ÈZ^\ˆ[š\š]Y™œ›ÛHÛM^İ—Ú[XYÙH[™İ—İY]\K[™H›Ø›İÈÛÛ™[œÙY›Û[™\ˆHÒSÜ[ˆ›Û“XÙ[œÙK‚‚•HXÛÛ\[][Ûˆ\ÈHÛÜšÈÙˆHØXÛÛ\ÌØJÎ‹ËÙÚ]X‹˜ÛÛKÛXÛÛ\ÌÊH›Ú™Xİ˜[™]™\[Û™HÚÈÛÛšX]YÈ]ˆÚ]İ]]›Û™HÙˆ\È^\İË‚‚‘ÛÛ[‘^YHÈ\ÈH›Ü\HÙˆ]ÈšYÚÈÛ\œËˆ\È›Ú™Xİ\È[›Ù™šXÚX[[˜Y™š[X]Y˜[™Ú\È›È\ÙˆHØ[YK‚