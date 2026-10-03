# Licensing and provenance

This document records where every part of Goldeneye-Native came from and under what terms, and
it states plainly which of those terms are settled and which are not. It is a factual record.
It is not legal advice, and it does not reach a legal conclusion about anything.

One question in particular - the licence covering the Fast3D renderer and the audio mixer this
project inherited - is **unresolved**, and section 4 sets out the evidence and the available
options without choosing among them.

Last verified 2026-09-01 against the working tree and the repository's advertised Git refs. Every claim
below states how it was checked.

---

## 1. What this repository contains, and what it does not

### Current-tree and history status

The current tracked tree is intended not to contain a ROM or locally extracted game asset. That
statement is deliberately narrower than the former claim about the repository's entire history.
A generated-asset patch was found to retain both native-word and byte-stream representations of
the Rareware logo in reachable commits. The current-tree remediation removes that whole hunk and
performs the conversion only after the user extracts the source locally.

The publication guard now rejects suspicious high-density hexadecimal arrays in text and patch
files, in addition to the existing filename, header, archive, binary, and encoded-payload checks.
It measures both complete brace-delimited initializers and runs of hexadecimal data lines, so a
diff hunk that begins partway through an array, or a nested table whose innermost braces are each
small, is still rejected. Two reviewed port-owned files are exempt by exact path only:
`getv/port/src/ge_icon.h`, the launcher icon, and `getv/port/ge_mixer.c`, whose `resample_table`
is the stock libultra resampler coefficient table carried in from sm64ex. A renamed or copied
dense array is still rejected. Synthetic fixtures exercise the guard and the local transformer
without using game data.

A complete advertised-ref rewrite must still be reviewed and explicitly approved before it is
pushed. Until that happens, remote branch or tag history can continue to expose the old objects;
pull-request refs, release archives, Pages deployments, Actions artifacts, caches, forks, and
third-party clones require separate review or cannot be recalled. This is a factual remediation
record, not a conclusion that the project or its dependencies are legally clean.

### It does contain

| area | what it is |
|---|---|
| `getv/port/src/`, `getv/port/mac/`, `getv/port/include/` | This project's platform layer: windowing, input, audio device, filesystem, save handling, asset bridge, render loop. Roughly 8,700 lines. |
| `getv/port/fast3d/` | The Fast3D display-list renderer, inherited from sm64ex. Roughly 7,700 lines. **Licence unresolved - see section 4.** |
| `getv/port/audio/ge_mixer.{c,h}` | The N64 audio microcode in software, inherited from sm64ex. **Same unresolved licence - see section 4.** |
| `getv/port/configfile.h`, `getv/port/fs/fs.h` | Verbatim copies of sm64ex headers. **See section 4.4.** |
| `getv/port/fast3d/ge_sky_rdp.{c,h}` | Written for this project. Decodes GoldenEye's hand-assembled RDP triangle commands. |
| `getv/port/include/stb/stb_image.h` | stb_image v2.19 by Sean Barrett. Dual-licensed MIT / public domain (Unlicense); the notice is retained verbatim in the file. |
| `tools/`, `getv/patches/` | Build tooling and source/asset-stage patches; review provenance per file. |
| `getv/Sources/Assets.xcassets/` | Apple TV app icon and top-shelf artwork. **See section 2.** |

Two tracked entries are **symlinks, not files** (git mode `120000`):
`getv/port/include/PR` and `getv/port/include/platform_info.h`, both pointing into
`vendor/ge-decomp/include/`. Both are **relative** (`../../../vendor/ge-decomp/...`) and so
resolve correctly in any checkout. They carry no content of their own.

### The game's own source is not in this repository

The C source of GoldenEye 007 itself is **not** vendored here. It is obtained by the builder from
`n64decomp/007` into `vendor/ge-decomp/`, which `.gitignore` blocks. This repository supplies
only the platform layer that source is compiled against.

---

## 2. Four things settled before the first push

None of these was game data. All four were questions a maintainer should decide deliberately
rather than discover after publication, so each is recorded here with what was actually done.

### 2.1 The app icon and top-shelf artwork - removed

Fifteen PNGs under `getv/Sources/Assets.xcassets/App Icon & Top Shelf Image.brandassets/`
rendered the **"GoldenEye" wordmark and the 007 gun logo**. They were inspected directly, not
inferred from filenames.

They were not extracted from the ROM, but neither were they original work by this project: they
carried no provenance metadata, and the marks they reproduce belong to third parties - the 007
logo and the James Bond marks to Danjaq LLC and MGM, the GoldenEye 007 game branding to Nintendo
and Rare. They are not required to build or run anything.

They are no longer tracked. `.gitignore` blocks `**/*.xcassets/**` and `**/*.brandassets/**`, and
no commit in the published history contains them. Anyone building the tvOS target supplies their
own artwork.

### 2.2 SDL2 and Xcode build caches - not in the published history

An earlier local history vendored the full **SDL2 2.30.9** source tree, its tvOS CMake build
directory (a 2.8 MB `libSDL2.a` and several hundred `.o` files) and 1.2 MB of Xcode SDK stat
caches, along with the project's internal development notes.

The published history is a fresh one and contains none of it. `deps/SDL2`, `libSDL2.a`,
`tvos/build-device`, and the internal notes each appear in zero commits; the whole `.git`
directory is about 4 MB. `.gitignore` blocks `deps/`, `build/` and the notes so they cannot
return.

SDL2 is under the zlib licence, which permits source redistribution with the notice intact, so
this was repository hygiene and clone size rather than a licence problem. It is recorded because
the earlier state was described publicly and the record should not be left half-told.

### 2.3 Absolute local paths and personal identifiers - parameterised

Several tracked build files hardcoded an absolute home-directory path to a hand-built tvOS SDL2,
an Apple Developer Team ID, and one specific Apple TV's device identifier. None was a credential,
but all were personal identifiers that would have shipped, and the paths broke on every other
checkout.

All are now variables with sensible defaults:

- `${N64TVOS_PREFIX:-$HOME/.n64tvos}` in `getv/build.sh`, `getv/bt_build_sim.sh` and
  `getv/build_mac.sh`
- `${DEVELOPMENT_TEAM}` in `getv/project.yml`
- `$DEV_DEVICECTL` in `getv/build.sh`

The two tracked symlinks that had the same problem were made relative. `sm64tv/` and `tvos/` are
no longer tracked at all.

### 2.4 The repository is now published

The repository was originally published at `https://github.com/SegfaultEvan/goldeneye-native`.
After that repository was deleted on August 29, 2026, its public history was preserved at
`https://github.com/seb-patron/goldeneye-native`. A current clone's `origin` should resolve to the
community continuation while the original repository remains unavailable.

That changes the cost of everything above. While the history was local, rewriting it was cheap;
now it is not. The history was therefore made clean *before* the first push rather than after, and
the checks in 2.1 through 2.3 are the record of that. **Nothing should be rewritten now without an
explicit decision**, because anyone who has already cloned holds the old objects regardless.

---

## 3. Per-component provenance

| component | path | origin | terms | state |
|---|---|---|---|---|
| Platform layer | `getv/port/src/`, `getv/port/mac/`, `getv/port/include/{config,platform,platform_info,port_support,system}.h` | This project | Not yet declared - the repository has **no root LICENSE file** | Open, but ours to decide |
| Sky RDP decoder | `getv/port/fast3d/ge_sky_rdp.{c,h}` | This project | Same as above | Ours |
| Build tooling | `tools/*.py`, `getv/patches/`, `*/build.sh`, `*/project.yml` | This project | Same as above | Ours |
| Fast3D renderer | `getv/port/fast3d/gfx_*.{c,h}` | **sm64ex**, which took it from **`Emill/n64-fast3d-engine`** | **Contested** | **Unresolved - section 4** |
| Audio mixer | `getv/port/audio/ge_mixer.{c,h}` | **sm64ex** `src/pc/mixer.{c,h}`, Emill's implementation of the N64 audio microcode | **Contested - same lineage** | **Unresolved - section 4** |
| Two inherited headers | `getv/port/configfile.h`, `getv/port/fs/fs.h` | **sm64ex** `src/pc/configfile.h`, `src/pc/fs/fs.h` - copied verbatim | **sm64ex ships no licence at all** | **Unresolved - section 4.4** |
| Image loader | `getv/port/include/stb/stb_image.h` | `nothings/stb`, v2.19 | MIT **or** public domain, at the user's option; notice retained in-file | Settled |
| Font rasteriser | `getv/port/include/stb_truetype.h` | `nothings/stb`, by Sean Barrett | MIT **or** public domain (Unlicense), at the user's option; notice retained in-file | Settled. Note it sits directly under `include/` rather than in `include/stb/` beside `stb_image.h`, which is inconsistent but harmless; the include path in `ge_text_overlay.c` matches. |
| Launcher font | `getv/port/assets/fonts/RobotoCondensed-VF.ttf` | `google/fonts`, path `ofl/robotocondensed` | **SIL Open Font License 1.1**; `OFL.txt` sits beside the font and is copied next to the binary by the build, which is what the licence's retention requirement asks for | Settled |
| Game source | `vendor/ge-decomp/` (not distributed here) | `n64decomp/007` | **No licence file in the upstream repository.** 58 files under `src/libultra/` carry Silicon Graphics proprietary notices | Upstream's situation; see section 5 |
| Reference: Perfect Dark | `vendor/pd-ext`, `vendor/pd-port` (not distributed here) | `perfect-dark-pc-port/perfect_dark` | MIT, Â© 2022 Ryan Dwyer - verified by reading `vendor/pd-port/LICENSE` | Cleared for adaptation **with attribution** |
| Reference: mgb64 | `akratch/mgb64` | Upstream | MIT | Cleared for adaptation **with attribution** |

### Quarantined - no code may be taken from these

| project | licence | why |
|---|---|---|
| GoldenRecomp | GPL-3.0 | Incompatible with permissive publication |
| `cblock85/GoldenEye64Recomp` | GPL-3.0 | Same |
| `chrissotraidis/goldenpad` | No top-level licence; documents an N64ModernRuntime GPL-3.0 obligation | Both problems at once |
| `DeeStiz/007` | **No licence at all** | May be read for understanding; nothing may be copied or adapted |

---

## 4. The Fast3D question

This is the one unresolved item. It is stated here as fact, with the evidence, and the options
are laid out with their consequences. **No option is chosen here.**

### 4.1 What is established

**`Emill/n64-fast3d-engine` has never been MIT-licensed.** From its own repository history:

- `LICENSE.txt` has exactly four commits.
- The initial commit **`a99492dd`** (2020-04-24) is a BSD-2-Clause-shaped notice whose
  condition 2 reads, in full: **"Redistributions in binary form are not allowed."** A flat ban.
- Commit **`881eb68b`** (2021-10-26, *"Updating license"*) changed one line, adding the
  carve-out *"except in cases where the binary contains no assets you do not have the right to
  distribute."*
- **GitHub classifies the repository `NOASSERTION`** - its licence detector does not recognise
  the text as any standard licence.

**This project's lineage is the stricter, pre-2021 one.** Verified locally against
`vendor/sm64ex`:

- sm64ex ships **no root licence file** (`ls vendor/sm64ex | grep -i licen` returns nothing).
- It reproduces Emill's notice in exactly one place, `src/pc/README-n64-fast32-engine.md`, and
  that copy is the **pre-2021 form**: `Copyright (c) 2020, Emill`, condition 2 reading
  `Redistributions in binary form are not allowed.`, **with no asset carve-out.**

So the strict reading is not one downstream project's opinion. It is the notice this project's
own upstream ships.

**How much of that upstream is actually still present** was measured line-by-line with a
sequence matcher over line-ending-normalised text (`difflib.SequenceMatcher`, `autojunk=False`),
comparing `vendor/sm64ex/src/pc/gfx/` against `getv/port/fast3d/`:

| file | sm64ex lines | our lines | identical lines | share of upstream surviving verbatim |
|---|---|---|---|---|
| `gfx_pc.c` | 1,832 | 5,515 | 1,593 | 86% |
| `gfx_opengl.c` | 791 | 968 | 736 | 93% |
| `gfx_cc.c` | 41 | 47 | 40 | 97% |
| `gfx_sdl2.c` | 354 | 432 | 348 | 98% |
| `gfx_cc.h` | 58 | 104 | 55 | 94% |
| `gfx_rendering_api.h` | 36 | 36 | 34 | 94% |
| `gfx_pc.h`, `gfx_opengl.h`, `gfx_sdl.h`, `gfx_screen_config.h`, `gfx_window_manager_api.h` | 83 | 83 | 83 | 100% |
| `src/pc/mixer.c` -> `audio/ge_mixer.c` | 871 | 1,244 | 852 | 97% |

The port has grown these files substantially - `gfx_pc.c` is three times its upstream length -
but it has **not** displaced them. Between 86% and 100% of upstream's text is still present
verbatim. This is inherited code that has been extended, not a reimplementation.

Note that this extends to **`getv/port/audio/ge_mixer.c`**, which an earlier internal note
listed as original work. Its own file header states it is sm64ex's `src/pc/mixer.c`, Emill's
implementation of the N64 audio microcode, and the measurement above confirms 97% of that file
survives verbatim. The Fast3D question therefore covers the mixer too.

**Downstream projects label the same lineage inconsistently:**

- **Perfect Dark's port** carries `port/fast3d/LICENSE.txt` reading plain **MIT**,
  `Copyright (c) 2020 Emill, MaikelChan` - verified by reading the file in `vendor/pd-port`.
  That label arrived in commit **`9508b136`** (2023-08-01, *"replace old fast3d with
  libultraship-fast3d"*), which **deleted Emill's custom text and inserted standard MIT while
  keeping the Emill copyright line.** PD's fast3d files are `.cpp` and do not correspond
  file-for-file to ours; ours are the `.c` sm64ex set.
- **mgb64**, which is MIT overall, **explicitly retracts** an earlier MIT claim about the Emill
  engine and describes it as custom BSD-2-Clause with a binary-redistribution restriction.

### 4.2 What is not established

Whether this project's Fast3D is nearer to Emill's original or to libultraship's later rewrite,
and what either text permits. Nobody on this project has asked Emill.

At present `getv/port/fast3d/` carries **no licence file, no provenance note and no attribution
header of any kind**; `gfx_pc.c` still opens on `#include <math.h>`. Whatever is decided below,
that gap should be closed: the notice has to travel with the source under every reading of it.

### 4.3 A related finding: the inheritance is wider than Fast3D

The measurement above was run as a sweep, not a spot check. Every one of the 45 C and header
files under b‹­¦ëm®éÜj×¢¸ Šv¥jšk£¦j×­¢G§r‹§·]8ïÁ”×ÒØ‚‚•\ÈX]\œÈ™XØ]\ÙH]ÚY[œÈH]Y\İ[Û‹ˆ
ŠœÛM^Ú\È›ÈXÙ[˜ÙHš[H[™XZÙ\È›ÈXÙ[˜ÙBœİ][Y[[ˆ]È‘PQQJŠˆH™\šYšYYH\İ[™ÈHÚXÚÛİ]›Ûİ[™Ü™\[™ÈH‘PQQNÈBœØ[YH\ÈYHÙˆÛM\Üˆ[Z[	ÜÈ›İXÙH[‚˜Ü˜ËÜËÔ‘PQQK[Y˜\İÌ‹Y[™Ú[™K›Y\ÈHÛ›HXÙ[˜ÙH^[]Ú\™H[ˆHÛM^™YK[™š]Ûİ™\œÈÛ›HH˜\İÑ[™Ú[™KˆHİ\ˆÛÈXY\œÈ\™H[š\š]Yœ›ÛH[ˆ\İ™X[H]œİ]\È›İ[™È][[™ÛM^	ÜÈİÛˆ›Û‹Q˜\İÑÛÙH\ØÙ[™Èœ›ÛHHİ\\ˆX\š[È™XÛÛ\[][Û‹ÚXÚZÙ]Ú\ÙHØ\œšY\È›ÈXÙ[˜ÙK‚‚›İXY\œÈ\™HÛX[[™™Z]\ˆ\È™XÙ\ÜØ\Kˆ™]Üš][™È[Hœ›ÛHH[\™˜XÙ\È\ÈÜ˜XİX[H\Ù\ÈÛİ[™[[İ™H[Hœ›ÛHH]Y\İ[Ûˆ]İÈÛÜİ[™\ÈÛÜÛÛœÚY\š[™Âš[™\[™[HÙˆÚ]]™\ˆ\ÈXÚYYX›İ]˜\İÑ‚‚ˆÈÈÈÜ[ÛœÈ[™Z\ˆÛÛœÙ\]Y[˜Ù\Â‚ŠŠŠJHÚ\Ûİ\˜ÙHÛ›NÈ\Ù\œÈZ[Z\ˆİÛˆš[˜\KŠŠ‚•H™KLŒŒH›İXÙK\ÈÜš][‹Y™\ÜÙ\ÈÛÈØ\Ù\Îˆ]\›Z]È™Y\İšX][Ûˆ
š[ˆÛİ\˜ÙB™›Ü›Jˆ›İšYYH›İXÙH\È™]Z[™Y[™İ]\È]™Y\İšX][Ûˆ
š[ˆš[˜\H›Ü›Jˆ\È›İ˜[İÙYˆHÛİ\˜ÙK[Û›H™[X\ÙHÚ][Z[	ÜÈ›İXÙH™\İÜ™YÈÙ]‹ÜÜÙ˜\İÙØ\ÈHØ\ÙBH^Y™\ÜÙ\ÈY™š\›X]]™[KˆÛÛœÙ\]Y[˜ÙNˆ›È™[X\ÙHš[˜\šY\Ë›È\İÜ™HÜˆ\İ›YÚ™\İšX][Û‹[™]™\H\Ù\ˆ™YYÈH[ÛÛÚZ[‹ˆ]\ÈHÚX\\İÜ[ÛˆÈ^Xİ]H[™]›X]™\È]™\Hİ\ˆÜ[Ûˆ™[İÈİ[]˜Z[X›K‚‚ŠŠŠŠH™\XÙH˜\İÑÚ][ˆ[˜[XšYİ[İ\ÛHXÙ[œÙY™[™\™\‹ŠŠ‚Ø[™Y]\È\™HX[˜\Ú\	ÜÈ˜\İÙ
RU0ªHŒŒˆÙ[š^ÊHÜˆHœ›ÛK\ØÜ˜]ÚŒÑ[\œ™]\‹‚ÛÛœÙ\]Y[˜ÙNˆH]Y\İ[Ûˆ\Ø\X\œÈ\›X[™[H[™š[˜\šY\È™XÛÛYHÜÜÚX›Kˆ]HYX\İ\™Y™]™\™Ù[˜ÙHX›İ™Hİ]ÈHİ\ˆØ^H\™HHÙÜË˜Ø\ÈÜ›İÛˆœ›ÛHKÌˆÈKLMH[™\ÈÚ]‘ÛÛ[‘^YK\ÜXÚYšXÈÛÜšÈ
HŒÑ×Õ’M^[œÚ[Û‹ÚŞH‘ÛÚÜËÛÛ[‘^YIÜÈÑ[™^[™[œÚ]H™Z]š[İ\‹™X\‹\[™H™Z™Xİ[Ûˆ[™YÈHØ[YIÜÈİÛˆZXÜ›ØÛÙJKˆ[Ùˆ]Ûİ[š]™HÈ™H™K[[™YÛˆHY™™\™[˜\ÙKˆ\™ÙK[™]]ÈÛÜšÚ[™È™[™\š[™È]š\ÚË‚‚ŠŠŠÊH\ÚÈ[Z[›ÜˆÛ\šYšXØ][Û‹ŠŠ‚HÚÜÜXÚYšXÈ]Y\İ[ÛˆÙ\ÈHİ\œ™[PÑS”ÑK\H™]›ØXİ]™[HÈHÛÙH\È]œİÛÙ[ˆŒŒ[™\ÈH\ÜÙ]Ø\™K[İ][[™YÈ\›Z]š[˜\šY\ÈÛÛZ[š[™È›ÈØ[YH\ÜÙ]ÏÂÛÛœÙ\]Y[˜ÙNˆÚX\[™HÛX\ˆ[œİÙ\ˆ™\ÛÛ™\ÈHX]\ˆ›Üˆ]™\[Û™HİÛœİ™X[Kˆ]]\Â›İ]ÚYH\È›Ú™Xİ	ÜÈÛÛ›ÛX^HÛÈ[˜[œİÙ\™Y[™[ˆ[™˜]›İ\˜X›H[œİÙ\ˆ›Ü™XÛÜÙ\Â›Ü[Ûˆ

H^XÚ]H˜]\ˆ[ˆX]š[™È]Y\™[H[˜Ù\Z[‹‚‚ŠŠŠ
HÚ\š[˜\šY\È[™XØÙ\Hš\ÚËŠŠ‚ÛÛœÙ\]Y[˜ÙNˆÙˆH›İ\‹\È\ÈHÛ›HÛ™H]›ØÙYYÈÛÛ˜\HÈH›İXÙH\Âœ›Ú™Xİ	ÜÈİÛˆ\İ™X[HÚ\Ë˜]\ˆ[ˆ\›İ[™]ˆÛÈ\\ˆ˜XİÈ™[Û™ÈÚ]HXÚ\Ú[Û‹‚‘š\œİ]\ÈHXÚ\Ú[ÛˆX›İ]ÛÈÙ\\˜]H[™ÜË›İÛ™NˆÙXİ[ÛˆH™XÛÜ™È]\ÈÜ˜ÛÛ\[\È^˜XİYØ[YH\ÜÙ]È\™XİH[ÈH^Xİ]X›KÛÈHš[˜\HÙˆÛÛ[™^YKS˜]]™B˜ÛÛZ[œÈHØ[YIÜÈ]HÚ]]™\ˆH˜\İÑ]Y\İ[Ûˆ\›œÈİ]ÈYX[‹ˆÙXÛÛ™HŒŒB˜Ø\™K[İ]H
ˆ™^Ù\[ˆØ\Ù\ÈÚ\™HHš[˜\HÛÛZ[œÈ›È\ÜÙ]È[İHÈ›İ]™HHšYÚÂ™\İšX]HŠˆH\ÈÜš][ˆ›Üˆš[˜\šY\È]Ø\œH›ÈİXÚ\ÜÙ]ËÚXÚ\È›İÚ]\ÈÜ˜İ\œ™[H›ÙXÙ\Ë‚‚‹KKB‚ˆÈÈKˆœš[™È[İ\ˆİÛˆ“ÓB‚ŠŠ“›ÈØ[YH]H\È\İšX]YH\È›Ú™Xİ[™›Û™HØ[ˆ™KŠŠˆHÚZ[ˆ\È\È›ÛİÜË‚‚ˆÈÈÈÚ]H\Ù\ˆ]\İİ\B‚H[\ÙˆZ\ˆİÛˆYØ[•ĞÈ
TÊHÛÛ[‘^YHÈØ\šYÙN‚‚‹HL‹N‹LLˆ]\ËšYËY[™X[ˆ›Ü›X]
XYÚXÈÍÌL[\›˜[˜[YHÓÓS‘VQX
B‹HÒKLHX™LYMYXŒÌØ˜ÌÍNYMXÍÎLXŒ˜Ù™NØ‚•]\ÚX]Ú\ÈÙLËKœÚLX[ˆHXÛÛ\[][Û‹ÛÈH[\Ú]]˜[YH\Â˜]KZY[XØ[ÈÚ]HÛÜœ™XİTÈZ[ÙˆHXÛÛ\[][Ûˆ›ÙXÙ\ËˆH[\Ú]H›Ü‚˜^[œÚ[Û‹ÜˆHXY\ˆÙˆÍÎL˜\È]K\İØ\Y[™]\İ™HÛÛ™\YÈ˜]]™B˜šYËY[™X[ˆš\œİ‚‚•Hš[HÛÙ\È]™[™Ü‹ÙÙKYXÛÛ\Ø˜\Ù\›ÛKKˆHÛÛ™[[Ûˆ\ÙY\™H\ÈÈÙY\[\È[‚˜›Û\ËØ]H™\ÜÚ]ÜH›Ûİ[™Ş[[[šÈÛ™H[ÈXÙNÈ™Ú]YÛ›Ü™X›ØÚÜÈ›İØØ][ÛœË‚‚ˆÈÈÈÚ]HZ[Ù\ÈÚ]]‚•HXÛÛ\[][Û‰ÜÈİÛˆ^˜Xİ[Ûˆİ\ÛÛœİ[Y\ÈH“ÓKˆ™[™Ü‹ÙÙKYXÛÛ\ÓXZÙYš[X\™Ù]˜^˜XİİX™\]Z\™\È˜\Ù\›ÛKKÈ™H™\Ù[[™[›ÚÙ\Â˜ØÜš\ËÙ^˜XİØ˜\Ù\›ÛKKœÚÈ™\™\]Z\Ú]\Ø\[™ÈÛˆ^˜Xİ\ÜÙ]ØˆH™\İ[\ÂŠŠŒKˆÈÛİ\˜ÙHš[\ËLŒÈP‹[™\ˆ™[™Ü‹ÙÙKYXÛÛ\Ø\ÜÙ]ËØ
ŠˆHHØ[YIÜÈ[Ù[Ë^\™\Ë]™[˜XÚÙÜ›İ[™Ëİ[ˆÛÛ\Ú[Ûˆ]KÙ]\š[\Ë[š[X][ÛˆX›\Ë^˜[šÜÈ[™˜]Y[ÈÙYÛY[Ë˜[œØÜšX™Yœ›ÛHHØ\šYÙH[ÈË‚‚•ÜÙHš[\È\™H[ˆÛÛ\[Y[™
Š›[šÙY[ÈH^Xİ]X›JŠ‹ˆÛˆH^H]™Y[ˆ“ÓBœÙYÛY[È[™Ù\™HPIÙ[ˆ][[YNÈÛÛ\[Y˜]]™[H^H\™HÜ™[˜\H[šÙY]HÚ]™X[œÚ[\œËÛÈ\™H\È›È“ÓHØY\ˆ][[YH[™›ÈÙ™œÙ]]Ë\Ú[\ˆ˜[œÛ][Û‹‚‚ˆÈÈÈÚH›ÈØ[YH]HØ[ˆ™H™Y\İšX]Y‚•ÛÈÛÛœÙ\]Y[˜Ù\È›ÛİË[™^H\™HÛÜÙ\\˜][™Î‚‚ŒKˆ
Š•H^˜XİY\ÜÙ]ËØ™YH\ÈØ[YH]H[ˆHY™™\™[š[H›Ü›X]ŠŠˆ˜[œØÜšXš[™ÈBˆ^\™H[ÈHÈ\œ˜^HÙ\È›İÚ[™ÙHÚ]]\Ëˆ]Ø[››İ™HÛÛ[Z]YZ\œ›Ü™YÜ‚ˆ]XÚYÈH™[X\ÙK‚Œ‹ˆ
ŠHZ[š[˜\HÙˆ\ÈÜÛÛZ[œÈH[\™HØ[YKŠŠˆ\È\È\˜Ú]Xİ\˜[HY™™\™[ˆœ›ÛH\™™Xİ\šÉÜÈÜÚXÚ™XYÈ]È“ÓH][[YHœ›ÛHH]KØ\™XİÜH[™ÚÜÙBˆš[˜\H\È\™Y›Ü™H“ÓKYœ™YKˆİ\œÈ\È›İˆ
ŠH™[X\ÙHš[˜\HÙˆÛÛ[™^YKS˜]]™HÛİ[ˆÛÛZ[ˆHÛÜ\šYÚYØ[YH[ˆ[
Š‹[™]\ÈYH\œ™\ÜXİ]™HÙˆH˜\İÑ]Y\İ[Û‚ˆ[ˆÙXİ[Ûˆ‚‚”Ú[ˆ\ÈH™X\ÛÛˆH˜œš[™È[İ\ˆİÛˆ“ÓHˆ[Ù[\™HYX[œÈ
˜œš[™È[İ\ˆİÛˆ“ÓH[™Z[š][İ\œÙ[Š‹˜]\ˆ[ˆ
™İÛ›ØYİ\ˆš[˜\H[™İ\HH“ÓH][[YJ‹ˆÚ[™Ú[™È]Ûİ[›YX[ˆ™KX\˜Ú]Xİ[™È\ÜÙ][]™\HÈØYœ›ÛH\ÚÈ][[YKÚXÚ\ÈHÚYÛšYšXØ[YXÙHÙ‚ÛÜšÈ[™\È›İİ\œ™[H[›™Y‚‚ˆÈÈÈXÚšXØ[›İ[™\HÙˆHÚ[™İÜÈÙ]\Ø[™Y]B‚˜ÛÛ[‘^YKS˜]]™KTÙ]\™^X\ÈH›ÜÜÙYš\œİ\[ˆÙ]\\XØ][Ûˆ›ÜˆÚ[™İÜË›İH^XX›B˜š[˜\Kˆ]İÛ›ØYÈš]˜]HÜX›HZ[ÛÛÈ[™HX›XÈÛİ\˜ÙK\ÚÜÈ›ÜˆH\Ù\‹\İ\YY˜šYËY[™X[ˆ“ÓK™\šYšY\È]ÈÒKLH[ˆXÙK[™\ÜÙ\È]ÈÜšYÚ[˜[]Â˜ÛÛËÜÙ]\]Ú[™İÜËœÚˆH“ÓH\È›İÛÜYYÜˆ\ØYYÈ^˜Xİ[Ûˆ[™H^XX›HZ[š\[ˆÛˆH\Ù\‰ÜÈXXÚ[™K‚‚•HÚ^˜\™\È[X™\˜][H\ÚYÛ™YÈØ\œH›È“ÓKY\š]™YÜˆ\ÜÙ]ËØ]KXÛÛ\[][ÛˆÛÙK›Üˆ˜\İÑ™[™\™\ˆÛÙKˆH™[™\™\ˆ[™“ÓKY\š]™Y\ÜÙ]È\™H[šÙY[ÈHØØ[HZ[˜ÛÛ[™^YK™^X™]™\ˆ[ÈHÙ]\Ø[™Y]Kˆ\È\ÈH˜\œ›İÙ\ˆXÚšXØ[\Y˜Xİ[ˆBœ^XX›Hš[˜\NÈ]\È
Š››İ
ŠˆHÛÛ˜Û\Ú[Ûˆ]\İšX][™È]\È\›Z]Yˆ[ˆ\Xİ[\‹\Âœ™\ÜÚ]ÜH\È›È›ÛİXÙ[˜ÙH›Üˆ]ÈİÛˆÛÙH[™H\İ™X[HXÛÛ\[][Ûˆ\È›ÈXÙ[˜ÙHš[K‚•ÜÙH]Y\İ[ÛœÈ™\]Z\™HXZ[Z[™\ˆ[™Ú\™H\›ÜšX]KYØ[™]šY]È™Y›Ü™HX›XÈ™[X\ÙK‚‚•]\ÈÚXÚÙY˜]\ˆ[ˆ\ÜÙ\YˆÙ]‹ØZ[İÚ^˜\™œÌX[šÜÈ^XİH™YHÙˆ\Âœ›Ú™Xİ	ÜÈİÛˆÛÛ\[][Ûˆ[š]ÈKHÙ]\İÚ^˜\™˜ÜÚLK˜Ø[™ÙWÚXÛÛ—Ø\K˜ØKH\È]Â˜\XØ][Û‹[X[šY™\İ™\Ûİ\˜ÙHYØZ[œİX\ˆ[QİZH
RU
KÑˆ
›XŠH[™ÓUË‚˜ÛÛËÜXÚØYÙWİÚ[™İÜ×İÚ^˜\™œÌX[ˆ[œÈH™\šYšXØ][ÛˆÙ[‹]\İ›İ™\ÈHU‹NX[šY™\İš\È[X™YYÚXÚÜÈH[\ÜÈ[™š[˜\HÚ^™K[™ØØ[œÈ™\™\Ù[]]™HÙ[™\˜]YX\ÜÙ]™XÛÛ\[][Û‹[™˜\İÑX\šÙ\œÈ™Y›Ü™H]İYÙ\ÈHXÚØYÙKˆHXÚØYÙH[˜ÛY\ÈZ\ˆÛÛ\]B››İXÙ\Èœ›ÛHÙ]‹İÚ^˜\™ÕT‘ÔT•WÓ“ÕPÑTËˆH“ÓH[Û™H\ÈLˆP‹‚‚ŠŠ•\È›Ú™Xİ]\İ›İX›\ÚÛÛ[™^YK™^X[™\ˆHİ\œ™[XÚØYÚ[™È[Ù[ŠŠˆ]\È›İ˜Ú[™ÙYˆYˆHÚ^˜\™]™\ˆÜ›İÜÈH\[™[˜ŞHÛˆHÜ^Y\ˆ›Ü\‹HXÚšXØ[›İ[™\B™Øİ[Y[Y\™HİÜÈ™Z[™ÈYH[™HØ[™Y]H™[X\ÙH›ØÙ\ÜÈ]\İİÜ›Üˆ[›İ\ˆ™]šY]Ë‚‚˜™Ú]YÛ›Ü™X[™›Ü˜Ù\ÈHš\œİÚ[YXÚ[šXØ[Kˆ]›ØÚÜÈ›Û\ËØ]™\H
‹È
‹›Â˜
‹È
‹™[˜
Š‹Ø˜\ÙKš\™[™Ü‹Ø\ËØ[Ù]‹ØZ[J˜[™Z[[XXËJ˜[™˜Z[\Ú[KJ˜\™XİÜšY\Ë]™\H
‹›ØÈ
‹˜XÈ
‹™ÖSX[]Ú\™H[ˆH™YK
‹˜›\œ˜[YB˜Ø\\™\Ë[™ØÜ˜]ÚYØˆXXÚÙˆÜÙH[\ÈØ\È™\šYšYYYØZ[œİH™X[]]^\İÈÛ‚™\ÚÈ
ÙXİ[ÛˆJKˆ
Š‘È›İY™X][KŠŠ‚‚ˆÈÈÈH[X™YY–ÜXİ[H[][]Ü‚‚˜™[™Ü‹ÙÙKYXÛÛ\ÜÜ˜ËÙØ[YKÜÜXİ[K˜Ø\È
ŠLLH[™\È[\[Y[[™ÈHÛÛ\]H[][]ÜŠŠ‹ÚXÚH™]Z[Ø\šYÙH\ÙYÈ[ˆ[ˆ[[X]H^HHØ[YH]\È\È[ˆ[›ØÚØX›H^˜K‚’]›İÈÛÛ\[\È[ˆ\ÈÜ‚‚•H\İ[˜İ[Ûˆ\™H\ÈHØ[YHÛ™H\È›ÜˆH˜\ÙH“ÓK[™]\ÈÛÜİ][™ÈÙ\\˜][B˜™XØ]\ÙHHš[IÜÈÚ^™H[š]\ÈHÜ›Û™È\Üİ[\[Û‚‚‹H
Š•H[][]ÜˆÛÙH\È\ÙˆHXÛÛ\[][Û‹ŠŠˆ]Ø\œšY\ÈHØ[YHİ]\È\ÈH™\İÙ‚ˆ™[™Ü‹ÙÙKYXÛÛ\ÜÜ˜ËØH›ÈXÙ[˜ÙHš[H\İ™X[K›İ\İšX]YH\È™\ÜÚ]ÜK‚‹H
Š•H[][]Üˆ[X™YÈ›ÈØ[YH]KŠŠˆ™\šYšYYH™XY[™È]ˆHÛ›H]KX\œ˜^H]\˜[È[‚ˆHš[H\™HÛX[[™Ù^X›Ø\™X›\È][™\ÈMKMM‹ÍKNH[™LLËLLM‹ˆH[ˆØ[Y\È\™BˆØYY][[YHœ›ÛH]È\İY]ÜXİ[K˜ÎNKLLˆ[KÙ]KÜØXœ™KœÙYËœ˜ˆ]XØ™]XØ™]X[˜[Y[İ[™œšYÚ[™\˜ÛšYÚÜ™XÜÜİÛÛÚÚYX‚‹H
Š•ÜÙH[ˆš[\È\™H›İ™\Ù[[]Ú\™H[ˆ\È™\ÜÚ]ÜHÜˆ[ˆHXÛÛ\[][Û‚ˆÚXÚÛİ]
ŠˆHÛÛ™š\›YYHHÚÛK]™YHÙX\˜Ú›Üˆ
‹œÙYËœ˜ÚXÚ™]\›œÈ›İ[™Ëˆ^HÛÛYBˆİ]ÙˆH\Ù\‰ÜÈİÛˆ“ÓHšXHH^˜Xİ[Ûˆİ\^XİHZÙH]™\Hİ\ˆ\ÜÙ]‚‚•H[ˆØ[Y\È\™HÙ\\˜]H\™\\HÛÜšÜÈÚ]Z\ˆİÛˆšYÚÈÛ\œË\İ[˜İœ›ÛB‘ÛÛ[‘^YHÈ]Ù[‹ˆ
Š•^H\™H›İ\È›Ú™Xİ	ÜÈÈ\İšX]K[™HØ[YH[H\Y\ÈÂ[H\ÈÈH˜\ÙH“ÓNˆœš[™È[İ\ˆİÛ‹ŠŠ‚‚ˆÈÈÈHXÛÛ\[][Û‰ÜÈİÛˆÜÚ][Û‚‚˜XÛÛ\ÌØ
Šš\È›ÈXÙ[˜ÙHš[JŠˆH™\šYšYYH\İ[™ÈHÚXÚÛİ]›Ûİˆ]Â˜Ü˜ËÛX[˜KØÛİ\˜Ù\ÈØ\œHÚ[XÛÛˆÜ˜\XÜÈ›ÜšY]\H›İXÙ\È™XY[™Ë[ˆ\]^BŠˆ˜ÛÛZ[ˆ[œX›\ÚY›ÜšY]\H[™›Ü›X][ÛˆÙˆÚ[XÛÛˆÜ˜\XÜË[˜ËˆŠˆ[™X^H›İ™B™\ØÛÜÙYÜˆÛÜYYÚ]İ]Üš][ˆÛÛœÙ[È
ŠNš[\È[™\ˆÜ˜ËØØ\œH]XY\‹ŠŠ‚‚•]\È\İ™X[IÜÈÚ]X][Û‹›İÛÛY][™È\È›Ú™XİÜ™X]YÜˆØ[ˆ™\ÛÛ™Kˆ]\È™XÛÜ™Y˜™XØ]\ÙH]\ÈH˜XİX›İ]H˜\ÙH\ÈÜ\ÈZ[Û‹[™™XØ]\ÙH]™X\œÈÛˆ[HXÚ\Ú[Û‚š[ˆÙXİ[Ûˆ‚‚‹KKB‚ˆÈÈ‹ˆ]šX][Û‚‚‘]™\HY\][Ûˆœ›ÛH[›İ\ˆ›Ú™Xİ]\İ™XÛÜ™
Šœ™\ÜÚ]ÜKÛÛ[Z][™š[H]B˜Y\][ÛˆÚ]H[ˆHÛİ\˜ÙK[™[ˆ\ÈØİ[Y[ŠŠˆ]\ÈHİ[™[™È[K›İB™›Ü›X[]HH]\ÈÚ]XZÙ\ÈHX›H[ˆÙXİ[ÛˆÈ™\šYšXX›HHÛÛY[Û™HÚÈØ\È›İ\™K‚‚ˆÈÈÈİ\œ™[HØ\œšYY‚ŸÚ]œ›ÛHÚ\™H][™ÈŸKK_KK_KK_Ÿ˜\İÑ™[™\™\ˆÛM^Ü˜ËÜËÙÙØ
œ›ÛH[Z[ÛY˜\İÙY[™Ú[™X
HÙ]‹ÜÜÙ˜\İÙÙÙÊ‹ØËXˆ
Š“›È›İXÙHš[H™\Ù[HÙYHŒ‹ŠŠˆŸ]Y[ÈZXÜ›ØÛÙH[ˆÛÙØ\™HÛM^Ü˜ËÜËÛZ^\‹˜Ø
[Z[
HÙ]‹ÜÜØ]Y[ËÙÙWÛZ^\‹˜ØˆHš[HXY\ˆ˜[Y\È]ÈÜšYÚ[ˆ[™\İÈ]È›İ\ˆÚ[™Ù\ËˆŸ”ÔZXÜ›ØÛÙH™Y™\™[˜ÙH\™™XİY\šË\Ë\ÜÜ\™™XİÙ\šØÜ˜ËÜœÜÙÜÜœØH[ˆ[››İ]YÛÜHÙˆHØ[YHZXÜ›ØÛÙHÛÛ[‘^YH[œÈ™XY›İÛÜYYˆÚ]Y]Ù]‹ÜÜÙ˜\İÙÙÙÜË˜ÎŒŒMŒÌMM[™Ù]‹ÜÜÙ˜\İÙÙÙWÜÚŞWÜ™˜ÎŒÌÌ˜ˆŸİ—Ú[XYÙHŒ‹ŒNH›İ[™ÜËÜİ˜ÙX[ˆ˜\œ™]Ù]‹ÜÜÚ[˜ÛYKÜİ‹Üİ—Ú[XYÙKšXÙ[˜ÙH›İXÙH[Xİ[‹Yš[KˆŸİ—İY]\H›İ[™ÜËÜİ˜ÙX[ˆ˜\œ™]Ù]‹ÜÜÚ[˜ÛYKÜİ—İY]\KšXÙ[˜ÙH›İXÙH[Xİ[‹Yš[KˆYYÚ]H™X[Y›Û^İ™\›^KˆŸ\Y]\İØ\Ú]˜Z[[İYY˜][
ÑWÔÕĞT
H\™™XİY\šË\Ë\ÜÜ\™™XİÙ\šØLM™ØXÜÚ[˜ÛYKÜ™\›ØÙ\ÜËØÛÛ[[Û‹š
ÔÕĞTQÕSØÔÕĞTÕS
KˆRU
ÊHŒŒˆX[ˆŞY\‹ˆÙ]‹ÜÜÚ[˜ÛYKÙÙWİ\YÜİØ\šİÛˆ[\[Y[][ÛˆÙˆHÑÙ[™\šXØY\Ü]ÚXÚš\]YK›İXÙH[™Ú]H[ˆHš[HXY\‹ˆŸŞ[[™\‹X]Ø\™HÜ›İ[™İ\ÜÙ[Xİ[ÛˆXÛÛ\Ü\™™XİÙ\šØMYY™Ø™˜ŒØMŒ™X™X˜ŒÍÌÍLMÜ˜ËÛX‹ØÛÛ\Ú[Û‹˜Ø
ÙÙš[™ÙÜ›İ[™Ùš[˜[\ÙXÙÙš[™ÙÜ›İ[™Ø]ØŞ[Ê˜
KˆRU0ªHŒŒˆX[ˆŞY\‹ˆ™[™Ü‹ÙÙKYXÛÛ\ÜÜ˜ËÙØ[YKÜİ[—ÙÜ›İ[™Û˜]]™Kš[™H˜]]™H™\ÛÛ™\ˆ[ˆİ[‹˜ØÈÛÛ[‘^YHÕS‹Ü›ÛÛH[\[Y[][ÛˆÚ]	ÜÈÙ[\‹Yš\œİ[ˆŞ[[™\‹YYÙHİ\ÜÙ[X[XÜËˆ[RU›İXÙNˆPÑS”ÑTËÜ\™™XİY\šË\ÜSRUˆ‚ˆÈÈÈ^Xİ\İ™X[H™]š\Ú[ÛœÂ‚]šX][Ûˆ\ÈÈ˜[YHHÛÛ[Z]›İ\İH™\ÜÚ]ÜKˆ\ÙH\™HH™]š\Ú[ÛœÈ™\Ù[[ˆ\ÂÛÜšÚ[™È™YH]H[YHÙˆÜš][™ËØZ[™YÚ]Ú]PÈ\ˆÙÈLX‚‚ŸÚXÚÛİ]\İ™X[Hœ˜[˜ÚÜˆYÈÛÛ[Z]]HŸKK_KK_KK_KK_KK_Ÿ™[™Ü‹Ü\Ü\™™XİY\šË\Ë\ÜÜ\™™XİÙ\šØÜYÈÚKY]‹XZ[LM™ØY™™ÌNXÎLNLMLŒLÍ™™XL™L˜ØŒ‹LKLHŸ™[™Ü‹ÜY^\™™XİY\šË\Ë\ÜÜ\™™XİÙ\šØLØMMYLŒÙYNML˜NŒÍŒYY™˜ŒKLL‹LˆŸ™[™Ü‹ÙÙKYXÛÛ\XÛÛ\ÌØX\İ\˜
Ü˜YY
HÍÍMÎM˜ÍMÙ™NLX™YŒYYYÍ˜XŒ‹LLMÈŸ™[™Ü‹ÜÛM^ÛMËÜÛM^šYÚX
Ü˜YY
HØØL˜ÌÍM™XÍNÌMLYLLÙM™ŒŒLL‹LMÈŸ™[™Ü‹ÜÛM\ÜÛM\ÜÜÛM\ÜX\İ\˜
Ü˜YY
H˜ŒMÙXÎMÎNŒÌXLYÍÌYŒÍÎNMŒLÙ™˜ÎXŒLLKLMH‚›İ\™™Xİ\šÈÚXÚÛİ]ÈØ\œHHØ[YHÛÈXÙ[˜ÙHš[\ÎˆH›ÛİPÑS”ÑX™XY[™ÈRU°ªHŒŒˆX[ˆŞY\‹[™ÜÙ˜\İÙÓPÑS”ÑK™XY[™ÈRU0ªHŒŒ[Z[XZZÙ[Ú[‹ˆHÛÂ˜˜\İÙÓPÑS”ÑKš[\È\™H]KZY[XØ[ÈXXÚİ\‹‚‚“›İH]™[™Ü‹ÙÙKYXÛÛ\™[™Ü‹ÜÛM^[™™[™Ü‹ÜÛM\Ü\™H
Š™Ü˜YYÚ[İÂ˜ÛÛ™\ÊŠ‹ˆZ\ˆØØ[\İÜH\È[˜Ø]YÛÈHÛÛ[Z]\Úœ›ÛH[HY[YšY\ÈH™]š\Ú[Û‚˜]Ù\È›İ][[Û™H™XÛÛœİXİ]È[˜Ù\İHØØ[K‚‚ˆÈÈÈ™\]Z\™Y›Üˆ[][™ÈYÜY[ˆ]\™B‚”\™™Xİ\šÈ
™[™Ü‹ÜY^™[™Ü‹Ü\Ü
H[™YØ\™HRU[™ÛX\™Y›ÜˆY\][Û‹‚•Z\ˆRU›İXÙ\È]\İ™H™\›ÙXÙY[™XXÚY\][ÛˆÚ]H]\İØ\œHHÛÛ[Y[˜[Z[™ÈB\İ™X[H™\ÜÚ]ÜKHÛÛ[Z]Y\Yœ›ÛK[™H\İ™X[Hš[KˆÙYHØÜËÔT‘‘PÕÑT’Ë›Y›ÜˆHÜXÚYšXÂ˜Ø[™Y]\ËH^XİÛÛ[Z]ÈÈÚ]K[™ÛÈ]šX][ÛˆØ\È[œÚYH\™™Xİ\šÉÜÈİÛˆ™YBŠÜÙ˜\İÙÙÛYØØ\œšY\È›È›İXÙH][
H]™YY™\ÛÛš[™È™Y›Ü™H[][™È\ÈZÙ[ˆœ›ÛBÜÙH\™XİÜšY\Ë‚‚“Û™HY\][Ûˆ\È[™XYH[ˆXÙH[™
Š˜İ\œ™[H[˜]šX]Y
ŠˆH×ÔÔÑMÌW×ØÈ×ĞT“WÓ‘SÓ˜”ÒSQ\Ü]Ú›ØÚÈ]Ù]‹ÜÜØ]Y[ËÙÙWÛZ^\‹˜ÎŒÍMX]Ú\Â˜™[™Ü‹Ü\ÜÜÜÜÜ˜ËÛZ^\‹˜ÎŒL‹LXˆ]Úİ[Ø\œHHÛÛ[Y[˜[Z[™Â˜\™™XİY\šË\Ë\ÜÜ\™™XİÙ\šÈLM™ØXÜÜÜ˜ËÛZ^\‹˜ØRU0ªHŒŒˆX[ˆŞY\‹‚‚ˆÈÈÈ›İY]XÚYY‚ŠŠ•\È™\ÜÚ]ÜH\È›È›ÛİPÑS”ÑHš[KŠŠˆH]›Ü›H^Y\‹HÚŞH‘XÛÙ\ˆ[™B˜Z[ÛÛ[™È\™H\È›Ú™Xİ	ÜÈİÛˆÛÜšÈ[™›È\›\È]™H™Y[ˆXÛ\™Y›Üˆ[Kˆ]\ÈBœÙ\\˜]HXÚ\Ú[Ûˆœ›ÛHÙXİ[Ûˆ[™Ø[ˆ™HXYH[™\[™[HÙˆ]‚