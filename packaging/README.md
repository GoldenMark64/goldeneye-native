# Packaging

Icons for all three desktop platforms come from one source image and one script.

    tools/make_icons.sh

It reads `assets/icon/goldeneye-plus-transparent.png` and writes:

| Output | Used by |
| --- | --- |
| `assets/icon/goldeneye-plus.icns` | the macOS bundle |
| `assets/icon/goldeneye-plus.ico` | the Windows executable and its shortcuts |
| `assets/icon/hicolor/<size>/apps/goldeneye-plus.png` | the Linux icon theme |
| `getv/port/src/ge_icon.h` | the running window, on every platform |

Every output is committed, so a normal build needs none of this -- run the script only after
changing the source image. It needs ImageMagick, and `iconutil` for the `.icns` (macOS only; on
other systems it leaves the `.iconset` directory behind for a Mac to finish).

The window icon is compiled in rather than loaded from disk. SDL2 without SDL_image cannot read a
PNG, and a build people copy between machines should not lose its icon to a missing file.

## Linux

Install the theme icons under `/usr/share/icons/hicolor/` mirroring the directory layout, and
`goldeneye-plus.desktop` under `/usr/share/applications/`, then:

    gtk-update-icon-cache /usr/share/icons/hicolor

## Windows

`goldeneye-plus.ico` is the resource to attach to the built executable, which also gives Explorer
shortcuts and the taskbar button the right image.

## macOS

`getv/build_mac.sh app` and `all` create **GoldenEye.app** beside the OpenGL executable, or
**GoldenEye Metal.app** beside the Metal executable. Double-clicking the app starts the existing
binary with `--launcher`, using the custom mission/settings window before starting the game.
The installer points players to this app. **Play GoldenEye.command** remains supported.

`tools/make_macos_launcher_app.py` writes the small launcher bundle and uses
`goldeneye-plus.icns` for its icon. The bundle contains a shell entry point, metadata and the
icon; it does not copy the game binary, settings or extracted data. Keep it beside its binary.
Moving the whole build folder works, and the app can be dragged to the Dock. It is a local
build shortcut, not a standalone signed/notarized distribution to move to Applications.

To add or recreate it for an existing build, run `./getv/build_mac.sh bundle` (add
`GETV_RENDERER=metal` for Metal). Rebuilding the binary updates the game the app will launch.
Direct terminal invocation of the bare binary is unchanged.
