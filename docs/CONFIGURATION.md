# Configuration

Every setting has the same name on the command line and in the configuration file. Source of
truth is `getv/port/src/ge_config.c`.

For the player-facing keyboard, mouse, gamepad, rebinding, and shortcut guide, start with
[`CONTROLS.md`](CONTROLS.md). This document is the exhaustive setting reference.

## Where the file lives

The first of these that exists is used, and the rest are ignored:

1. `$GETV_CONFIG`
2. `--config=PATH`
3. `goldeneye.cfg` in the same directory as the binary
4. `goldeneye.cfg` in the platform user-data directory

If none exists and none was asked for, the game writes the commented template to location 4 and
reads it back. That first-run write is how the port's tuned defaults actually reach you; a value
that only appears in a template nobody has generated does nothing.

On macOS the user-data config is
`~/Library/Application Support/Goldeneye-Native/goldeneye.cfg`. Windows and Linux use the path
returned by SDL for the platform. The selected path is printed at startup, which is the
authoritative location for the current machine. Older macOS installs with a config under the
pre-rename `GoldenEye` directory are detected and kept in place.

Save data is a separate `eeprom.bin` in the platform user-data directory, or in `save_dir` if you
set one.

## File format

```
# comment
; also a comment
key = value
```

Whitespace around the key and value is trimmed. `#` and `;` begin a comment anywhere on a line.
There are no sections, no quoting and no line continuations. An unrecognised key is reported on
stdout rather than silently ignored, as is a line with no `=`.

Values are lowercased before matching, except for `save_dir` and raw `GETV_*` passthrough, where
case is meaningful.

## Precedence

    command line  >  environment  >  config file  >  built-in default

This is implemented with one mechanism: the config file calls `setenv` with overwrite disabled,
so it can never displace a variable that is already set, while command-line flags call it with
overwrite enabled. Every consumer in the port reads the environment and needed no changes. An
environment variable always beats the file.

## Command-line flags

| Flag | Effect |
|---|---|
| `--help`, `-h` | Print the built-in usage summary and exit. |
| `--list-cheats` | Print every named cheat with its id and whether it is live. Exits. |
| `--config=PATH` | Read this file instead of searching. `--config PATH` also works. |
| `--write-config[=PATH]` | Write the commented default config and exit. With no path, writes to the platform user-data directory, creating it. |

Any other setting is passed as `--key=value`, for example `--resolution=1920x1440`. A bare
`--key value` pair is also accepted when the next argument does not begin with `-`.

## Display

### `resolution`

`WIDTHxHEIGHT`, or the literal `fullscreen` / `native`. Width must be at least 320 and height at
least 240; anything else is rejected with an error and the default is kept. `fullscreen` and
`native` are shorthand for `fullscreen = 1`.

Default `1280x960`. The built-in default is clamped down in whole 4:3 steps if it would not fit
the usable display area, because an oversized SDL window on macOS opens with its title bar under
the menu bar and cannot be moved.

### `aspect`

`4:3`, `16:9` or `auto`. `43` and `169` are accepted as aliases. No default.

Read the scope carefully. The renderer derives its aspect ratio from the actual framebuffer
dimensions, so it does not need an aspect setting - it needs a correctly shaped window. This key
therefore does two things and nothing else: if `resolution` is unset it picks a default window
shape (`1280x960` for 4:3, `1600x900` for 16:9), and if `resolution` is set it checks the window
shape against the requested aspect and prints a note if they disagree by more than 3%. An explicit
resolution always wins. `auto` is a no-op.

There is no second, independent aspect control, and there deliberately is not one: two of them
would aspect-correct twice.

### `fullscreen`

`0` or `1`; `on`/`off`, `true`/`false` and `yes`/`no` are accepted. Default `0`. Desktop builds
start windowed.

### `supersample`

`1` or `2`. Anything else is rejected. Default `1`.

`2` renders at double size and downsamples. Note that it changes the framebuffer size and
therefore the heap layout, so two runs at different supersample settings are not comparable to
each other for anything more precise than "it looks better".

### `filtering`

| Value | Aliases | Meaning |
|---|---|---|
| `point` | `nearest`, `n64` | Literal N64 point sampling. |
| `bilinear` | `linear` | Standard bilinear. |
| `three-point` | `threepoint`, `3point`, `default` | What the N64 RDP actually did. |

Default `three-point`.

Two independent mechanisms exist behind this key - the renderer's `configFiltering` and the
`GETV_POINT_FILTER` gate - and they do not mean the same thing. The key sets both consistently so
they cannot disagree.

### `widescreen`

`on` or `off` (`1`/`0`, `true`/`false`). Default `on`.

The console's own render path (`gfx_pc.c`'s `ge_scale()`/`gfx_adjust_x_for_aspect_ratio()`)
deliberately pillarboxes to the N64's native 4:3 rather than stretching a wider window - correct
for avoiding distortion, but with no way to ask for an actually-wider view until this key existed.
`on` fills the real window at its own aspect instead, with a wider field of view rather than a
stretched image; `off` restores the original letterboxed framing byte-for-byte. Single-player
only - split-screen's per-viewport aspect is untouched either way, since it has not been audited
against an arbitrary host window.

Sets `GETV_WIDESCREEN`, read by both the renderer (`configWidescreen`, `port_support.c`) and the
game layer (`lv.c`'s per-player aspect write).

### `pd_renderer` / `perfect_dark_renderer`

`0` or `1`, default **`0`**. Desktop OpenGL only. The launcher exposes the same switch as
**Perfect Dark renderer compatibility path** under Video.

This is an opt-in backend compatibility profile adapted from the MIT-licensed Perfect Dark PC
port. It requests an OpenGL 3.3 compatibility context, emits the same GoldenEye combiner logic as
GLSL 1.30, binds a dedicated VAO, uses `GL_RGBA8` texture storage and performs an end-of-frame
`glFlush()`. GoldenEye's Fast3D frontend, display lists, vertex interpretation and game logic remain
GoldenEye's own path.

It was built as a controlled A/B during the Intel GPU-hang investigation. It did **not** eliminate
that hang, so it is not the default renderer and is not presented as the 1.0 stability fix. It is
kept because it is useful for backend compatibility testing and future renderer work. The normal
1.0 OpenGL path is unchanged unless this setting is explicitly enabled.

Sets `GETV_PD_RENDERER=1`. See [`RENDERER_TROUBLESHOOTING.md`](RENDERER_TROUBLESHOOTING.md) for
the investigation and [`THIRD_PARTY.md`](THIRD_PARTY.md) for provenance.

### `hd_textures` / `texpack`

`hd_textures` is `on` or `off`, default **off**. `texpack` is a directory path, default
`hdtextures` (resolved next to the executable if no such folder exists relative to the working
directory - same `GETV_EXEDIR` fallback `moddir` uses).

When on, every N64 texture is looked up in the pack directory by content hash before upload - a
file named `<hash>.png` there replaces the console's own texture; anything not present in the pack
renders exactly as it did before this key existed. The hash is FNV-1a 64 over the raw N64 texel
bytes plus format/size, so it is stable across runs and does not depend on where the source data
sits in memory. `GETV_TEXPACK_DUMP=<dir>` (environment only, not a config key - it is a developer
tool, not a player setting; **not** `GETV_TEXDUMP`, which is [`image.c`'s own unrelated gate](MODDING.md)
- a byte count, not a path) writes a same-named `.ppm` baseline the first time each texture is
decoded, which is how a pack gets started: dump, convert the ones worth upscaling to `.png`, drop
them back in named by hash.

**Off by default, unlike `filtering` and `widescreen` above.** End-to-end validation dumped the 56
textures decoded during DAM's first 121 frames, replaced each with a same-sized flat-magenta PNG,
and compared the resulting frame with the baseline: 91% of sampled pixels changed and 3,244
magenta pixels appeared where the baseline had none. The lookup, content-hash naming and upload
path therefore have direct runtime validation. It remains opt-in because faithful presentation is
the default, not because the mechanism is unverified.

Sets `GETV_HD_TEXTURES` and `GETV_TEXPACK`, read by `configHDTextures` and the pack-directory
resolver in `port_support.c`.

### `mouse_mode`

`modern` (default) or `classic`, also available as `GETV_MOUSE_MODE`. Modern mouse travel
directly changes camera angles without stick acceleration, a turn-speed cap, or carried
surplus motion. Classic preserves the original mouse-to-N64-stick mapping. The existing
mouse sensitivity and inversion settings apply to both. Physical controllers retain their
current response. Vehicles, network sessions and scripted/replay input retain Classic N64;
see [MOUSE.md](MOUSE.md).

The other launcher controls on the same page also have friendly config names: `mouse` and
`keyboard` are `0` or `1`, `mouse_sens` is a percentage from `1` through `1000`, and
`mouse_invert` is `0` or `1`. Their raw equivalents are `GETV_MOUSE`, `GETV_KEYBOARD`,
`GETV_MOUSE_SENS` and `GETV_MOUSE_INVERT`.

### `framerate`

`30`, `50`, `60`, or `off` (`0`, `uncapped` and `unlimited` are accepted for the last). Default
`60` on NTSC builds, `50` on PAL.

**A frame cap above 60 is refused, and `off` is the high-refresh setting.** GoldenEye advances
its clock in whole video fields. On the default synthetic counter `osGetCount()` moves a fixed
amount per call, so one rendered frame is one video field by construction and the world runs
exactly as fast as the renderer. Measured on DAM, ten seconds each, reading the game's own
`currentFrameCounter` against a real millisecond clock:

| `framerate` | clock | fields/sec (60.0 is correct) | fps |
|---|---|---|---|
| `60` (default) | synthetic | **60.0** | 60 |
| `120` | synthetic | 117.6 | 118 |
| uncapped | synthetic | 811.9 | 812 |
| `120` | real | 60.3 | 60 |
| **`off`** | **real** | **60.5** | **456** |

`off` sets `GETV_REALCLOCK=1` as well as removing the cap, because uncapped on the synthetic
clock is the worst configuration available here and there is no reason to let someone reach it
by accident. On the real timebase a field is a unit of real time, and `waitForNextFrame`'s
free-run path stops blocking on the field boundary, so the renderer runs ahead while the world
keeps its own clock. `put()` does not overwrite, so `realclock = 0` alongside it still wins for
anyone deliberately measuring the synthetic behaviour.

A cap never free-runs, which is why `120` on the real clock still delivers 60 fps. So a capped
rate above 60 is either wrong or pointless depending on the clock, with no third case, which is
what the rejection message says.

**The tick divider must be 1 under free-run.** `gePortSimAlpha()` is `phase / divider`, so a
divider above 1 blends the camera against a frame phase rather than a fraction of elapsed time,
and under a real clock those are unrelated. It presents as flicker rather than as a wrong number.
Elapsed time already gates the simulation there. Fixed in `0009-freerun-divider.patch`.

**The cost of `off` is reproducibility.** Elapsed fields become load-dependent, so no two runs
are frame-for-frame comparable and measurement harnesses should stay on 60.

`framerate=30` additionally sets `GETV_TICKFIELDS=2`, so each update reports two elapsed fields.
Game time stays real, thirty updates a second times two fields being sixty, while the
frame-counted systems drop to 30 Hz.

**What is still frame-counted.** Only 13 of the 135 translation units under `src/game` scale by
`g_GlobalTimerDelta`. Automatic fire is converted and time-based by default on both the player
and the AI side (`GETV_TIMEFIRE`, `gunfire.c` and `chraction.c`); the rest still advance once per
update.

`GETV_FPS`, `GETV_REALCLOCK` and `GETV_TICKFIELDS` in the environment all reach the pacing code
directly and override the pairings above.

## Live keybindings

A handful of settings can change during a running game, no restart needed, bound directly in
`gfx_sdl2.c`'s `gfx_sdl_onkeydown()` rather than exposed through any menu. Everything else in this
file is read once at startup (a `getenv` call or an `__attribute__((constructor))`, see
`docs/PERFECT_DARK.md` section 6 row 17 for the full list of what cannot honestly be made live
without a restart) and stays fixed for the process lifetime.

| Key | Effect |
|---|---|
| `F11`, `Alt+Enter` (Windows), `Cmd+F` (macOS) | Toggle fullscreen |
| `F9` | Toggle vsync |
| `F5` / `F6` | FOV -10% / +10%, clamped to 50-160% |

Fullscreen and vsync both flip `configWindow` fields and set `configWindow.settings_changed`,
the same apply path a window resize already goes through. FOV bypasses its cached `GETV_FOV`
env read entirely once touched (`gePortSetFovScale()`, `fr.c`) - the projection is recomputed
from it fresh every frame per viewport regardless, so nothing downstream needs telling.

This is deliberately not a menu. An in-game options page was considered and scoped down: the
Watch's data-driven settings page (`options.c`) is the only reusable one in the codebase, and
adding a page to it is blocked by an explicit maintainer comment on `WATCH_NUMBER_SCREENS`
(`options.h`) not to change that constant until `struct player` is fully shiftable. See
`docs/PERFECT_DARK.md` section 6 row 17.

## Co-op team rules

Read only when `coop` is 2 or more. Both are on the launcher's CO-OP page.

| Setting | Value | What it does |
|---|---|---|
| `coop_friendly_fire` | `0` or `1`, default `0` | Whether players can damage each other. Off is the co-op default and the opposite of multiplayer: everyone starts on one pad facing the same way, so with it on the first shot usually finds a team mate. Guards, explosions and falls are unaffected either way. |
| `coop_respawn` | seconds, 0-30, default `5` | How long a dead player waits before being put back on the level's start pad. `0` waits for a button press instead. If everyone is down at once the mission is lost and nobody returns. |

## Free camera

Unpins the camera from Bond and lets it fly, which is a photo mode rather than a cheat: the game
carries on around you, guards keep patrolling, and nothing about the simulation changes.

The camera itself is not new code. `debug_camera.c` has carried a six-degree fly camera since
1997 and `lv.c` already called it every frame, gated behind a debug mode that a retail cartridge
could not reach. What it never had was a consumer -- nothing outside that file read
`debugCameraPosition`, so it moved its own state and no render path ever saw it. This supplies
the missing j‹­¦ëm®éÜj×¢¸ Šv¥jšk£¦j×­¢G§r‹§·_5ã©\™H]™[\Ú[™ÈB™[™Ú[™IÜÈİÛˆÚ”Ü]Û]ÛÛÜ™[š\š][™ÈHXYİX\™	ÜÈ›ÙH[™RH\İÈHØ]™B›[X™\ˆš\Ù\È]™\HØ]™WÚÚ[ØÚ[È[™YÈÜ›İİÈHÜ]ÛˆÛİ[\ÈHØ\‚‚ŠŠ”Ü]Ûš[™ÈØ[ˆ™H™Y\ÙY[™]\È›İ[ˆ\œ›Ü‹ŠŠˆ×ĞÚ”ÛİØ\È[ØØ]YÚ]Û›B˜
İX\™Ûİ[
ÈL
X[šY\È[™H[™Ú[™HXÛ[™\ÈÈÜ]ÛˆÚ]™]Ù\ˆ[ˆ™YHœ™YKÛÂH™X[ÙZ[[™È™[Û™ÜÈÈH]™[ˆH™Y\ÙYÜ]ÛˆX]™\ÈHØ]™HÛX[\ˆ˜]\ˆ[‚™˜Z[[™Ë‚‚ŠŠ•ÛÈÙˆ\ÙH\™H[™\Y[\›˜[JŠ‹[™H[\[Y[][ÛˆÛÛ\[œØ]\ÈÛÈB\Ù\‹Y˜XÚ[™È˜[YHYX[œÈÚ]]Ø^\Ëˆ[™[^WÚX[]šY\È×ĞZRX[[ÙYšY\˜™XØ]\ÙB]ÛØ˜[ØØ[\È[XYÙHX[
ÊˆHİX\™È^Y\—ÚX[][\Y\ÈXİX[ÚX[˜™XØ]\ÙH›Û™X[˜[ÈH[XYÙHÈXİX[ÚX[ˆ[™[^WÜ™XXİ[Û˜\ÈØİ[Y[YÚ]İ]HY™šXİ[HÛZ[Nˆ]ØØ[\ÈH\\ˆ›İ[™ÙˆH˜[™ÛZ\ÙYRH[Y\‹[™ÚXÚ™\™Xİ[Ûˆ™Y[È\™\ˆ\È›İ™Y[ˆYX\İ\™Y‚‚ŠŠ•™\šYZ[™ÈH[\Ù]ÛÚÈY™™XİŠŠˆ[H›Û‹\İØÚÈ[\Ù]š[ÈÛ˜ÙH]]™[ØY›İÚ]Ø\È™\]Y\İY[™Ú]H[™Ú[™H[™Y\Û[™Î‚‚˜–ÙÙ]—VÜ[\Ù]Hš\™ÛÜ™HˆKHİYÚ\ˆİX\™Ë\ÜÈ[[[Ë[ˆH^Y\ˆX[–ÙÙ]—VÜ[\Ù]H[™[^NˆX[Œ	H[XYÙHML	HXØİ\˜XŞHLÌ	H™XXİ[ÛˆL	B–ÙÙ]—VÜ[\Ù]H^Y\ˆX[L	H\›[İ\ˆL	H[[[ÈL	H^ÜÚ[ÛˆL	H\œ™]L	B–ÙÙ]—VÜ[\Ù]H\YYˆZRX[LKŒZQ[XYÙOLÍLZPXØİ\˜XŞOLÎ‹‹ˆ[[[ÏLKŒ˜‚•HÙXÛÛ™[™H\ÈHÛZ[H[™H\YY˜[™H\ÈHYX\İ\™[Y[ˆÛˆYÙ[İØÚÂ˜ZRX[\È‹ŒÛÈ\™ÛÜ™IÜÈKŒ\ÈİX\™ÈZÚ[™È[ˆH[XYÙH^H\ÙYË‚‚˜ÑU—ÒÔ‘WÔÑS•TÕOœ˜[YO˜Ü]ÛœÈÛ™H™\XÙ[Y[œ›ÛHH]™HİX\™]]XÚÈÚ]İ]˜HÚ[]š[™È\[™Yˆ]^\İÈ™XØ]\ÙHÛÛX˜]Ø[››İ™Hš]™[ˆ™[XX›Hœ›ÛHHXY\ÜÂœ[‹[™]^\˜Ú\Ù\ÈHØ[YHÜ]Ûˆ]H™X[X]Ù\Ë‚‚ˆÈÈÈ™\Ù]KHHÛÛ[‘^YJÈ›Ùš[B‚˜™\Ù]H\Ø\ÈÛ™HİÚ]Ú›Üˆ]™\][™È\ÈÜ\ÈYY[™™\šYšYYˆ]XØÙ\Â˜˜Z][
[X\Ù\ÈMØÛÛœÛÛX
H[™\Ø
[X\Ù\È[š[˜ÙYÛÛ[™^YJØÙJØ
K‚‚Ÿ]\›œÈÛˆ˜[YHŸKK_KK_Ÿİ\\œØ[\XˆŸ\ØXXŸ[š\Ûİ›ÜXØŸZ\X\ØÛˆŸİ^\™\ØÛˆŸ\˜[^ÛˆŸXXÛˆŸÜ›ÜÜÚZ\—ÜØØ[XˆŸœ˜[Y\˜]XÙ™‹Ú]H™X[ÛØÚÈ‚•H›Ùš[Hš[ÈØ\È[™\ÜXÙ\È›İ[™Î‚‚˜˜ÛÛ[X[™[™Hˆ[š\›Û›Y[ˆ[İ\ˆİÛˆÛÛ™šYÈ[™\Èˆ™\Ù]˜‚”ÛÈ™\Ù]H\Ø›ÛİÙYHXHHÚ]™\ÈHÚÛH›Ùš[HÚ]İ]–PKÚ\™]™\ˆBÛÈ[™\ÈÚ]™[]]™HÈXXÚİ\‹ˆ[][™ÈH›Ùš[HØ[Y]›İ[™[™XYHÙ]\Â›˜[YYÛˆİİ]]İ\\˜]\ˆ[ˆ\ÜÙYİ™\ˆ]ZY]K™XØ]\ÙHH™\Ù]]Ú[[B™XÛ[™YÈ[˜Ø\Hœ˜[YH˜]HÛÚÜÈ^XİHZÙHH™\Ù]]Y›İÛÜšË‚‚•HÙ[™\˜]YÛÛ™šYÈÚ\Èİ\\œØ[\X[™œ˜[Y\˜]XÛÛ[Y[Yİ]›Üˆ]™X\ÛÛ‹ˆB˜ÛÛ™šYÈÜš][ˆ™Y›Ü™H\È^\İY\È[H\È]™H[™\Ë[™Hİ\\Y\ÜØYÙHÚ[Ø^BœÛÎÈÛÛ[Y[[Hİ]È]H›Ùš[H]™H[K‚‚‘˜Z][\È[™İ^\ÈHY˜][ˆHÛÚÈ\ÈH›ÙXİ[™HØ^HÛÜœ™Xİ™\ÜÈÙ]Â˜ÚXÚÙY\™H\ÈÛÛ\\š\ÛÛˆYØZ[œİ™X[Ø\\™\ËÛÈ[][™È][\œÈİ]]\ÈÈ™BœÛÛY][™È[İH\ÚÙY›Ü‹‚‚ŠŠ”™\Ù\™Y\œÙY][™\ŠŠ‚‚•\ÙH\™HHÛ™\È]İ[È›İ[™Ëˆ^H\œÙH[™˜[Y]HÛÈHÜ[Ûˆİ\™˜XÙH\ÂœİX›H™Y›Ü™HH™X]\™\È[™‚‚ŸÙ^HXØÙ\È[[™YY™™XİŸKK_KK_KK_Ÿ›Ù×Ü\—Ü^[H\‹\^[›ÙËˆ›ÙÈ\È\‹]™\^ˆŸ]^›WÛYÚØH[˜[ZXÈYÚ[™ÈÛˆ]^›H›\Ú\ËˆŸ]Y[×ÌÙHÜÚ][Û˜[]Y[ÈÈ•‹ˆ[X\È˜ˆŸÜØ[ØHØÜ™Y[‹\ÜXÙH[XšY[ØØÛ\Ú[Û‹ˆŸÚYİÜØH™X[][YHÚYİÈX\ËˆHØ[YHÚ\È›ØˆÚYİÜËˆŸ\—Ü^[ÛYÚ[™ØH\‹\^[YÚ[™ËˆYÚ[™È\È\‹]™\^Ûİ\˜]YÛÈ\ÈÚ[™Ù\ÈHÛÚÈH[ÜİÙˆ[][™ÈÛˆH\İˆ‚’[YÙ\ˆÙ^\È\™HÛ[\YÈZ\ˆ˜[™ÙH˜]\ˆ[ˆ™Z™XİY‚‚ˆÈÈ[™[^HÚXœÂ‚˜ÚXœÈHÙ™ˆ^ÜÚ[ÛœÈYÚÙ[XYÙH[Ø^\ØY˜][Ù™˜‚‚Ÿ˜[YH]X[YZ[™ÈX]ÈŸKK_KK_ŸÙ™˜›Û™NÈ™]Z[™Z]š[Ü‹ˆŸ^ÜÚ[ÛœØ\™XH^ÜÚ[ÛœÈ[™\™Xİ›ØÚÙ][\XİËˆŸYÚÙ[XYÙXHš[˜[]X[[™È]X\İŒ[\›˜[[XYÙH[š]ËˆŸ[Ø^\Ø]™\HØœÙ\™Y›Û‹\^Y\ˆÚ\˜Xİ\ˆX][˜ÛY[™ÈØÜš\YX]Ëˆ‚‘]™\H[˜X›YÛXŞH\Ù\ÈHØ[YHY™™XİˆÙ[™HÛÛYÚ[šÜÈ][˜Úœ›ÛHHÚ\˜Xİ\‹ÛÛYBÚ]]™[›ÛÜœÈ[™Ø[Ë›İ[˜ÙH\È™YH[Y\ËÙ]H›ÜˆX›İ][ˆÙXÛÛ™Ë[ˆ˜YK‚•HÜšYÚ[˜[X]™XÛÜ™İ[[™\ÈØÛÜš[™ËRH›İYšXØ][Û‹›ÜY][\ËØš™Xİ]™\È[™˜ÛX[\ˆ^Y\œÈ™[XZ[ˆ[˜Y™™XİY‚‚˜Û˜YXY\ØX[™^ÜÚ[Û˜\™H[X\Ù\È›Üˆ^ÜÚ[ÛœØˆHÙ][™ÈX\ÈÈHØ[YB˜Ø[›ÛšXØ[˜[Y\È[ˆÑU—ÑÒP”Øˆ[šÛ›İÛˆ˜[Y\È˜Z[ÛÜÙYÈÙ™˜ˆ“ÔÕTWĞÒ˜[ÛÈÛİ™\œÂœÛÛYHœšY[™H[™Ú]š[X[ˆZ\ÜÚ[ÛˆXİÜœËÛÈ[Ø^\Ø[HYX[œÈ[”ÈXİÜœÈ˜]\ˆ[‚šÜİ[\ÈÛ›KˆÙYHÑÒP”Ë›YJÒP”Ë›Y
H›Üˆ[\[Y[][Ûˆ›İ[™\šY\Ë\İÈ[™^[œÚ[ÛˆİZY[˜ÙK‚‚ˆÈÈÈœ][ÛÛ[‘^YH›ÛÙ[™˜\ÙHØ[YB‚•H][˜Ú\‰ÜÈØ[Y\^HYÙH\ÈH
Šœ][ÛÛ[‘^YJŠˆÙXİ[Û‹ˆ\ÙHÚÚXÙ\È\HÚ[ˆ[İB›][˜ÚHØ[YKˆH][˜Ú\ˆÛÈÚÚXÙ\È›Üˆ]][˜ÚÈ][H[ˆÛÛ[™^YK˜Ù™ØÈÙY\œ™Y™\™[˜Ù\È™]ÙY[ˆ\XØ][ÛˆÙ\ÜÚ[ÛœË‚‚˜[šB˜˜\ÙWÙØ[YHHÙ™‚ˆÈ˜\ÙWÛÛ›H\È[ˆ[X\È›Üˆ˜\ÙWÙØ[YK‚™ÚXœÈH^ÜÚ[ÛœÂ˜›ÛÙH[š[˜ÙY˜›ÛÙÛ[Z]HL˜‚‹H˜\ÙWÙØ[YHHÛ˜\ØX›\È[œ][ÛÛ[‘^YHY™™XİË[˜ÛY[™ÈHÜšYÚ[˜[Ü[Û˜[ÚXœËˆÚ]İ]™\XÚ[™ÈHÚÜÙ[ˆÚXœØ›ÛÙÜˆ›ÛÙÛ[Z]˜[Y\ËˆİÚ]Ú]Ù™ˆÈ\ÙHÜÙBˆ™Y™\™[˜Ù\ÈYØZ[‹ˆ]ÛÛ›ÛÈ\È™X]\™H˜[Z[NÈİ\ˆ[ÙËÚX]È[™[\Ù]È]™HZ\‚ˆİÛˆÛÛ›ÛËˆY˜][ˆÙ™˜Ú]ÚXœÈHÙ™˜İ[™\Ù\š[™ÈİØÚÈX]ÈHY˜][‚‹H›ÛÙHÜšYÚ[˜[[š[˜ÙY^Ù\ÜÚ]™XÛÛ›ÛÈYY›ÛÙœ›ÛHÚXˆX]ËˆÜšYÚ[˜[ˆ
[ÛÈÙ™˜
H™]Z[œÈH™]š[İ\ÈÚ[šÈY™™XİÚ]›ÈYY›ÛÙˆ[š[˜ÙY\ÈHY˜][Âˆ^Ù\ÜÚ]™XÜ™X]\ÈH[œÙ\ˆÚÜ\œİ]XÚ\™Ù\ˆİ™\›\[™Èİ\™˜XÙHÜ\Ú\Ë[™Hœ›ØYˆ[[YYX]HÛÛˆÜ™[˜\H]ÈÈ›İY][Z]YY›ÛÙ‚‹H›ÛÙÛ[Z]HM‹‹LL˜Ø\È\œÚ\İ[İZ[œËY˜][LˆXZ›ÜˆÜ\Ú\È]™Hš[Üš]Hİ™\ˆ[BˆX\šÜÈÚ[ˆ[ÈZ\˜›Ü›™H›ÛÙ\ÈHÙ\\˜]Hš^YØ\ˆ™[™\š[™È[ÛÈ™\ÜXİÈH]˜Z[X›HÜ˜\XÜÂˆY[[ÜKÛÈÜ›İÙYšY]ÜÈX^HÚİÈ™]Ù\ˆY™™XİË‚‚•HÛÜ™K[Û›HÚXÚØ›Ş\ÈØ[Y
Š‘[˜X›Hœ][Y™™XİÊŠˆÈ\İ[™İZ\Ú]œ›ÛHH[ŠŠ˜\ÙHØ[YJŠˆ][˜Ú\ˆ[ÙKˆ]\ÈÙ™ˆHY˜][È[˜X›[™È]Ù[XİÈ^ÜÚ[Û‹]šYÙÙ\™YÚXœÈYˆ›ÈšYÙÙ\ˆ\ÈÙ[XİY‚•H[™\œÙHÛÛ™šYÈÙ^H˜\ÙWÙØ[YX™[XZ[œÈÛÛ\]X›HÚ]^\İ[™Èš[\Ë‚‚•H˜]ÈØ]\È\™HÑU—ĞTÑWÑĞSQXÑU—Ğ“ÓÑ[™ÑU—Ğ“ÓÑÓSRUˆHœ][İ™\œšYBÚ[œÈİ™\ˆH[[YHÚXœØÛÛœÛÛHÛÛ[X[™ÛËˆYY›ÛÙ]XÚ\ÈÈİ]XÈ]™[šX[™Û\Ëš[˜ÛY[™ÈØ[È[™ÙZ[[™ÜË[™˜Y\ÈY\ˆ›İYÚH\HÙXÛÛ™ÈÙˆÚ[][][Ûˆ[YKˆÛÜœÂ˜[™İ\ˆ[İš[™ÈØš™XİÈÈ›İ™XÙZ]™H›ÛÙXØ[Ë‚‚ˆÈÈ]™[Ü\ˆİ™\›^H[™ÛÛœÛÛB‚•H\ÚİÜ][˜Ú\ˆÜ›İ\ÈHİ™\›^KÛÛœÛÛHİÙ^K[[Y]H™XÛÜ™[™È[™XYÈÙÙÚ[™Â›Ûˆ]È
Š‘]™[Ü\ˆÛÛÊŠˆYÙKˆÙYHHÙİZYH[™TWJU‘SÔT—ÕÓÓË›Y
K‚‚‚•Ú[ˆH[QİZH\[™[˜ŞH\È™\Ù[™\ÜÈ˜XÚÜ][İKÙÜ˜]™HÈÜ[ˆHÛÛ[X[™ÛÛœÛÛHÛˆZ]\‚“Ü[‘ÓÜˆY][ˆHÛÛœÛÛHİÙ^H\È[Ø^\È]˜Z[X›NÈ]Ù\È›İ\[™Ûˆ]™[Ü\ˆ[ÙK‚‚•HÙ\\˜]H\™›Ü›X[˜ÙKÙXYÈİ™\›^H\ÈÙ™ˆHY˜][ˆ[˜X›H]Ú]]™[Ü\—İÛÛÈHXH][˜Ú\‰ÜÈ]™[Ü\‹[İ™\›^HÚXÚØ›ŞÜˆÑU—ÒSQÕROLX‚‚˜ÛÛœÛÛWÚÙ^HHÑØØ[˜ÛÙH˜[YO˜Ú[™Ù\È]\ÚİÜÙÙÛKˆH][˜Ú\ˆ^ÜÙ\ÈHØ[YB™šY[[™ÑU—ĞÓÓ”ÓÓWÒÑVX\ÈH]]ÛX][Û‹Ü˜]ËYØ]H›Ü›Kˆ^[\\È[˜ÛYHŒLŒL˜[™˜Ü˜]™XÈ[ˆ[šÛ›İÛˆ˜[YH\È™\ÜY[™˜[È˜XÚÈÈÜ˜]™K‚‚•Ú[HHÛÛœÛÛH\ÈÜ[ˆ]İÛœÈÙ^X›Ø\™[™[İ\ÙH]™[È\ÈÙ[\ÈHÙ\\˜][HÛYÑ™]šXÙHİ]KˆÛÜÚ[™ÈÙY\ÈHÚÜ™[X\ÙH]X\˜[[™H[[]™\HÙ^H[™[İ\ÙH]Ûˆ\È\ÛÂ\[™È[\‹ÜXÙHÜˆXˆØ[››İ[ÛÈš\™KÚÚ\Hİ]ØÙ[™HÜˆÜ[ˆHØ]ÚˆØ[Y\YÈ™[XZ[‚˜]˜Z[X›Kˆ˜]ÈÛÛ[X[™\İÜH\È›İ[™YÈHİ\œ™[›ØÙ\ÜÈ[™\È›İÜš][ˆÈÛÛ™šYÈÜ‚™XYÛ›ÜİXÈİ]]‚‚•H[š]X[ÛÛ[X[™Ù][˜ÛY\È™XY[Û›HÙ\ÜÚ[Û‹Ü^Y\‹ÛØš™Xİ]™H]Y\šY\ËÚXœØ^XÚ]\Ûİ˜ÛÙØÚ]™XØ[[[Ø]]][ÛœË[™ÛÈÛÛ›ÛYZ\ÜÚ[Ûˆ˜[œÚ][ÛœÎ‚‚˜^œ™\İ\›]™[İYÙKZY‚˜‚›İ˜[œÚ][ÛˆÛÛ[X[™È™\]Z\™H[ˆXİ]™HÛÛÈZ\ÜÚ[Ûˆ[™\™H™Y\ÙY\š[™È™]^HÜˆØØ[›][\^Y\ˆ™Y›Ü™HHØ[YH˜[œÚ][ÛˆØ[˜XÚÈ[œËˆ]™[XØÙ\ÈH[Y\šXÈQÙˆHØYX›BœÛÛÈZ\ÜÚ[ÛÈ]Kİ][šÛ›İÛ‹[™][\^Y\‹[Û›HİYÙ\È\™H™Y\ÙYˆH˜[œÚ][Ûˆ\Ù\ÈB™Ø[YIÜÈ›Ü›X[˜Z[‹İ[›ØYÜ™[ØY™\]Y\İÛÈ]Ù\È›İ™]Üš]H˜]ÈİYÙHÛØ˜[ËˆHİXØÙ\ÜÙ[˜™\İ\™[ØYÈHİ\œ™[Z\ÜÚ[Û‹Ú[H]™[ÌØØÚY[\È[H[™™\Ù\™\ÈHİ\œ™[™Y™šXİ[Kˆ™\]Y\İ[™ÈHİYÙH[™XYH[ˆ^H\È[ÛÈHÛÛ›ÛY™[ØYˆ]™[ÌØÚ[HÛ‚‘[H›ÛİÜÈHØ[YH[™Ú[™H]\È™\İ\]]È™\İ[™[XZ[œÈH]™[™\İ[Ú]B››Ü›X[™]š[İ\ËÜ™\]Y\İY\İYÙHY]Y]K‚‚˜ÑU—ĞÓÓ”ÓÓWÓÔSLXÜ[œÈHÚ[™İÈ]İ\\›Üˆ›İ[™YRHÛ[ÚÙH\İËˆ]\È›İH\œÚ\İYœÙ][™Ë‚‚ˆÈÈ˜]ÈØ]\Â‚[HÙˆHÜ	ÜÈ]™[ÜY[Ø]\ÈØ[ˆ™HÙ]H]È™X[˜[YK[ˆHš[HÜˆÛˆHÛÛ[X[™›[™N‚‚˜‘ÑU—ÔÕQÑHHÍ‘ÑU—ÑVUÑ”SQHHŒB˜‚˜˜\Ú‹‹ØZ[[XXËÙÛÛ[™^YHKQÑU—ÔÕQÑOLÍ˜‚”˜]È˜[Y\È\™HX]ÚY™Y›Ü™HœšY[™HÛ™\ËÛÈHœšY[™HÙ^HØ[ˆ™]™\ˆÚYİÈHØ]Kˆ\™H\™B˜\›İ[™LÙˆ[NÈØSÑS‘Ë›YJSÑS‘Ë›Y
HÛİ™\œÈH\ÙY[Û™\Ë‚‚ˆÈÈÈÑU—Ô“ÔÕSSQU–XKHÛÜœ™[]Y[ØØ]ÜˆØœÙ\˜][ÛœÂ‚˜ÑU—Ô“ÔÕSSQU–OLXÙÙ]\ˆÚ]H›İ[™Y]Yœ™YB˜ÑU—Ô“ÔÕSSQU–WÔ•S—ÒQOÚÙ[˜[Z]È™\œÚ[Û™Y”ÓÓˆ[™\Èœ›ÛHH™X[›Ü™XÛÜ™˜[ØØ]Ü‹ˆ]\ÈHXYÛ›ÜİXÈØ]K›İHØ\XÚ]HÙ][™Ë[™™[XZ[œÈÚ[[HY˜][ˆÙYB–Ø“ÔĞSĞĞUÔ—ÕSSQU–K›YJ“ÔĞSĞĞUÔ—ÕSSQU–K›Y
H›ÜˆH^XİØÚ[XKY™XŞXÛB˜›İ[™\šY\Ë[™]šY[˜ÙH[Z]Ë‚‚ˆÈÈÈZ[WİÙÙÛXKH™\ÜÈÈZ[K[œİXYÙˆÛ[™Â‚˜ÜˆXˆY˜][ÚXÚ\ÈH™]Z[ÛˆZ[WİÙÙÛHHXXZÙ\ÈZ[HHÙÙÛNˆ™\ÜÂ›Û˜ÙHÈ˜Z\ÙHHÚYÚ[™YØZ[ˆÈİÙ\ˆ]ÛÈ[İH\™H›İÛ[™ÈHÙ^HİÛˆÚ]HØ[YBš[™[İH[İ™HÚ]ˆ[X\Ù\ÈÙÙÛWØZ[X[™™Y™\œ™Y›İËZ[WÛ[ÙHHÙÙÛXKHÚXÚÙ]Â\ÈØ[YHØ]H[™Ú]È™^ÈÜ›İXÚÛ[ÙXÚ\™HH^Y\ˆÚ[ÛÚÈ›Üˆ]‚‚•\È\ÈÛÛ[‘^YIÜÈİÛˆÜ[Ûˆ˜]\ˆ[ˆÛÛY][™ÈHÜ[™[Yˆ›Û™šY]Ì‹˜ÎMX˜[™XYHœ˜[˜Ú\ÈÛˆ]ˆÚ]Û[œÚYÚZ[[[ÙX\ÈÙ]œ›ÛHH]Ûˆ]™\Hœ˜[YNÈÚ]ÙÙÛK]›\ÈÛˆH™\ÜÈYÙKˆ[HÜÙ\È\È[œİÙ\ˆ]]Y\İ[Û‹ÚXÚ\ÈÚH]š\ÈH›İ\‹[[™H]Ú[™›İH]Ú[ˆH[œ]^Y\ˆKHHÙXÛÛ™[\[Y[][Ûˆ\™HÛİ[œ˜XÙHH™X[Û™HÚ[™]™\ˆÛÛY[Û™H[ÛÈÙ]HÜ[Ûˆ[ˆHØ[YIÜÈİÛˆY[K‚‚’]\È›Ü˜ÙY]HÚ[H[™Ú[™H™XYÈH˜[YK›İÜš][ˆÛ˜ÙH]İ\\™XØ]\ÙHBœİÜ™YÙ][™È\È\‹\^Y\‹\È™[ØYYœ›ÛHHØ]™YØ[YHHš[L‹˜ÎŒMÎ[™Ø[ˆ™HÚ[™ÙY™œ›ÛHHÜ[ÛœÈY[Kˆ[œİÙ\š[™È]H™XYİ\š]™\È[™YK[™X]™\ÈHY[HÚİÚ[™ÂÚ]]™\ˆ[İH\İÚÜÙH˜]\ˆ[ˆ]ZY]H™]Üš][™È[İ\ˆØ]™K‚‚•ÛÜÛ›İÚ[™È™Y›Ü™H[İH\›ˆ]ÛˆZ[H[ÙH\Èİ[Z[H[ÙKˆHØ[YHİÜÈ[İHØ[Ú[™ÈÚ[BHÚYÚ\È\[™ÛÈÜ›İXÚ™Z[™]ÛËÛÈHÙÙÛH™[[İ™\ÈH[Ù^H[™Ù\È›İ\›ˆZ[Z[™È[ÈH[İ™KX[™\ÚÛİ[ÙKˆ]Ø][™È\ÈHØ[YIÜË[ˆ›Û™šY]Ì‹˜Ø‚‚ˆÈÈÈ[™ÜÈ]Ú[ÛÜİ[İH[ˆY\››ÛÛ‚‚“YX\İ\™YÛˆÚ[™İÜÈ[™YH]™\]Ú\™KÛÛXİY™XØ]\ÙHXXÚÛ™HÛÚÜÈZÙHHœ›ÚÙ[ˆ™X]\™Bœ˜]\ˆ[ˆHÙ][™Ë‚‚‹H
Š˜ÑU—ÒÑVP“ĞT‘ÒQX\ÈÛˆÚ[™]™\ˆÑU—ÑVUÑ”SQX\ÈÙ]ŠŠˆØÜš\Y[œ]\È[‚ˆYÛ›Ü™YÚ]›È\œ›Üˆš[YÛÈ[ˆ[œ]\İÛÚÜÈZÙHH[™\İ[˜]\ˆ[ˆBˆ\ØX›YÛ™K‚‹H
Š”ØÜ™Y[œÚİÈ\™H[Ø^\ÈXš]“T
Š‹Ú]]™\ˆ^[œÚ[ÛˆÑU—ÔÒÕU\ÈÚ]™[‹ˆ˜[Z[™ÈBˆš[Hœ™Ø›ÙXÙ\ÈH“TØ[Yœ™Ø‚‹H
Š˜ÑU—ĞRSWÔÑS•TÕZÙ\ÈHœ˜[YH[X™\‹›İH›ÛÛX[‹ŠŠˆZ[H\È™XY\ÈHÙÙÛHÛˆBˆš\Ú[™ÈYÙK[™HÛİ\[™È]œ˜[YH\È[™XYHİÛˆ›İYÚHØÚÙYXÛÛ›ÛÈ[›ËˆÛÈ›ÈYÙH]™\ˆ\œš]™\È[™›İ[™È\[œËˆÚ]™H]Hœ˜[YHY\ˆH[›Ë‚‹H
Š˜[—İ\İËœÌXY˜][ÈÈSZ[™İÈÎ—\Ş\ÍZ[™İÍ
ŠˆÚ[HH›Ú™Xİ[œİ[ÈÂˆÎ—Z[™İÍˆ[ˆ]Ú]İ]H›YÈ[™]Ú[[H\Ù\ÈHY™™\™[ÛÛ\[\‹‚‚ˆÈÈÈÑU—Ô‘ĞLM‘X[™ÑU—ÕVM‘XKHM‹Xš]^\™H]HÜ™\‚‚“X]™H›İ[œÙ]ˆHØ[YIÜÈ^\™HXÛÙ\ˆİØ\È]È˜]]™HM‹Xš]^[ÈÈšYËY[™X[ˆ™Y›Ü™B\ØY
ÑU—ÕVM‘XY˜][JK[™H™[™\™\ˆ™XYÈ]™\H‘ĞLMˆ^[šYËY[™X[‚ŠÑU—Ô‘ĞLM‘XY˜][
KˆÙÙ]\ˆ^H˜]È^ÜÚ[ÛœÈÜ˜[™ÙH[™H›ÛİÙ\]Y[˜ÙIÜÈ˜][™ÂœÙX[˜\™]Ø\™HÙÛÈ[™ÓÓS‘VQHÙÛÈÛÜœ™XİK‚‚‹HÑU—ÕVM‘OLÚ]ÑU—Ô‘ĞLM‘OLX\ÈH™]š[İ\ÈZ\š[™ÎˆÜ˜[™ÙH^ÜÚ[ÛœË]H›ÛİˆÙÛÜÈ˜]È\ÈÛÛİ\ˆ›Ú\ÙK‚‹HÑU—ÕVM‘OLÚ]ÑU—Ô‘ĞLM‘OL\›œÈ^ÜÚ[ÛœÈXYÙ[KÚXÚØ\È™\ÜY\ÂˆÛÛ™™]Kˆ]\È›İHZ[˜[ÚX]‚‹HÑU—Ô‘ĞLM‘OL˜İØ\ÈHÛÈ]\ÈÙˆXXÚ^[[™\ÈÙ\\ÈHÛÛ›Û‚‚”ÙYHØÓÓÕT—Ğ•QÔË›YJÓÓÕT—Ğ•QÔË›Y
H›ÜˆHYX\İ\™[Y[Ë‚‚ˆÈÈÈÑU—Ô‘PSÑ“Ó•ØKHH™X[Y›Û^İ™\›^B‚“Ù™ˆHY˜][[™H˜]ÈØ]H˜]\ˆ[ˆHœšY[™HÙ^H™XØ]\ÙH]\È›İš[š\ÚY[›İYÚÂœ›Û[İKˆÑU—Ô‘PSÑ“Ó•ÏLX˜]ÜÈ^™[™\˜Ø^™[™\“İ][™Yİš[™ÜÈ›İYÚBœİ—İY]\H]\È˜ZÙYœ›ÛHÙ]‹ÜÜØ\ÜÙ]ËÙ›ÛËÔ›Ø›İĞÛÛ™[œÙYU‘‹˜[œİXYÙˆB™Ø[YIÜÈİÛˆš]X\Û\ËÚXÚ\™H\^[\ÜÙ]È™Z[™Èİ™]ÚY]\ÚİÜ™\ÛÛ][ÛœË‚‚’]š[ÈÚ]]Y]İ\\ÛÈ[İHØ[ˆ[HY™™\™[˜ÙH™]ÙY[ˆÙ™ˆ[™œ›ÚÙ[‚‚˜–ÙÙ]—Vİ^H™X[Y›Ûİ™\›^H™XYNˆ‹‹‹Ô›Ø›İĞÛÛ™[œÙYU‘‹‹MHÚ\œÈ˜ZÙY]
ˆ]\È›İÜÊB˜‚’YˆH›Û\ÈZ\ÜÚ[™È]Ø^\ÈÛÈ[™˜[È˜XÚÈÈHš]X\Û\È˜]\ˆ[ˆ˜]Ú[™Â››İ[™Ëˆ›İ]Y^
Hš[K\Ù[Xİ›Û\ˆXœÊH\È[X™\˜][H^[\[™İ[˜]ÜÂ›İYÚHÜšYÚ[˜[]‚‚•ÛÈÛ›İÛˆY™™\™[˜Ù\Ë›İÛÜÛY]XÈ[™›İÜš][ˆ\[‚˜Ù]‹ÜÜÜÜ˜ËÙÙWİ^Ûİ™\›^K˜Ø‚‚‹HY[HYÚYÚ›Ş\È\™HÜÚ][Û™Yœ›ÛHHš]X\›Û	ÜÈY]šXÜËÛÈHBˆY™šXİ[K\Ù[XİØÜ™Y[ˆ
ÑU—ÓQS•ON
HH›Ş\Èš\ÚX›HšYYœ›ÛH]ÈX™[‚‹HÛÛYHİš[™ÜÈÚ[™ÙHØ\ÙKˆHHØ]Ú]\ÙHØÜ™Y[ˆ™XYÈHĞUÒŒ‹ŒH‘UX[™\ˆBˆš]X\›Û[™HØ]ÚŒ‹ŒH™]X[™\ˆHİ™\›^K™XØ]\ÙHHİš[™È[ˆHØ[YIÜÈ]Bˆ\È[™XYHİÙ\˜Ø\ÙH[™Hš]X\Û\È™[™\ˆ]Ø\ÙKZ[œÙ[œÚ]]™[Kˆ™X[Z\™Ø\™BˆØ\\™\ÈÚİÈH\\˜Ø\ÙH›Ü›KÛÈ\™HHİ™\›^H\ÈHÛ™H]Y™™\œÈœ›ÛH™]Z[‚