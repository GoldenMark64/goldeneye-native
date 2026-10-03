# Developer tooling plan

Status: accepted architecture and active delivery plan. The console core, cross-renderer UI/input
ownership, solo pause policy, always-available toggle, initial read-only handlers, runtime `gibs`,
explicit-slot player mutations, and controlled solo-mission transitions are implemented. The
inspector and native diagnostic capture remain follow-ons.

Initial priority:

1. Modern in-game console
2. Player and entity inspector
3. One-click local diagnostic bundle

Reproduction recording/playback, same-process quickstates and portable semantic snapshots are
valuable follow-ons, but they are deliberately outside the first delivery milestone.

## Decision

Build a new port-owned developer-tools layer and keep Rare's original debug menu separate.

The original menu is useful historical code and some of its underlying game functions may still
be useful. It is not a suitable console foundation: availability and behavior depend on the
original build, several paths are absent or stubbed in the native port, and its UI was not
designed for searchable commands, structured results, modern input capture, diagnostics or
versioned automation.

The new console should therefore own its parser, registry, result log, input behavior and UI.
A console handler may call a verified original function through a narrow game-side adapter, but
the console must not expose arbitrary memory writes or depend on the original debug-menu UI.

The console UI should be available whenever the optional ImGui dependency is present. The
explicit developer-tools runtime setting controls the observational overlay, not whether the
console hotkey works. Builds without ImGui must continue to work; the command core, schema
validation and tests should not depend on ImGui.

## Why these three features belong together

The console creates controlled actions and a structured history. The inspector turns live game
state into bounded, frame-stable data. The diagnostic bundle captures both in a form the existing
bug-report workflow can validate and explain.

```text
ImGui console ----> command registry ----> game-thread command pump ----> game adapters
tests/headless ----/        |                         |
                            v                         v
                    structured results         settled-frame snapshot
                            |                         |
                            +----------+--------------+
                                       v
                              local diagnostic export
                                       |
                                       v
                         tools/collect_bug_report.py
                                       |
                                       v
                         reviewed bug-report issue draft
```

This is one architecture, not one pull request. Each independently testable layer should land in
a focused PR.

## Current seams to build on

| Existing seam | What it provides | Planning consequence |
| --- | --- | --- |
| `getv/port/src/ge_imgui.{h,cpp}` | An optional in-game ImGui overlay and SDL event feed | Extend this UI boundary; do not put command parsing or game mutations in the renderer file. |
| `getv/port/fast3d/gfx_metal.mm` | Metal-side ImGui device/pass/render helpers | Finish the shared overlay path for Metal rather than creating an OpenGL-only console. The current `ge_imgui.cpp` implementation compiles to stubs under `RAPI_METAL`. |
| `getv/port/src/ge_player_api.h` | Explicit-slot player state, input tick and RNG fingerprint | Use player slots and ticks in results and snapshots; never sample an implicit current-player cursor. |
| `getv/port/src/ge_enemy_api.h` | Live enemies with stable `chrnum` and field-presence masks | Use it as the first live-entity provider and preserve absent-versus-zero semantics. |
| `getv/port/src/ge_world_api.h` | Static objectives, routes, guards, props and doors | Label static world knowledge separately from live entity state. |
| `getv/port/src/ge_event.{h,c}` | A small event bus with derived level/player/room/guard events | Add a bounded event history without replacing authoritative game-side events that may be added later. |
| `getv/port/src/ge_gibs.{h,c}` | A tested policy model initialized from `GETV_GIBS` | The console uses its explicit runtime setter; changing the environment after the cached first read is not sufficient. |
| `tools/collect_bug_report.py` | Sanitized logs, normalized screenshots, manifest/report generation and local-only output | Import a versioned game-session manifest rather than duplicating the reporting workflow. |
| `tools/check_no_game_data.py` | Publication guard against ROMs, saves, archives and unexpected binary data | Keep exports semantic and reviewable; never put a save, quickstate, raw memory dump or opaque archive in a bundle. |

The current overlay is observational: every SDL event still reaches gameplay. The console needs a
new input-capture contract. That behavior must be explicit and tested because an Enter, Space or
Tab typed into the console must not also fire, skip a cutscene or open the watch.

## Shared rules

These rules apply to all three prioritized workstreams.

### Thread and frame ownership

- ImGui may submit a command, but it must not directly mutate game state.
- A bounded queue carries parsed requests to one documented game-thread command pump.
- The pump runs once per simulation tick at a characterized boundary. Each result records the
  game tick and rendered frame it affected.
- Inspector providers copy state at one documented settled-frame boundary. ImGui and exporters
  read only the copy.
- Queues and snapshots have fixed bounds. Overflow is reported; it must not silently overwrite a
  request or pretend a snapshot is complete.

The first console PR must prove the chosen game-thread hook and write that proof next to the
hook. Renderer callbacks are not assumed to be safe mutation points merely because they happen
to run on one thread in a current build.

### Stable identity and absent data

- Player identity is the explicit slot number.
- Live enemy identity is `(stage_epoch, chrnum)`.
- A future live prop/door identity must include a stage epoch and a stable game identifier or
  generation. A raw address is never an identity.
- Every snapshot family has field-presence flags. Unsupported or unavailable data is absent, not
  serialized as a plausible zero.
- Static world records and live records are labeled by source so a user cannot mistake a spawn
  point for a guard's current position.

### Mutation and multiplayer safety

- Commands declare whether they are read-only or state-changing.
- State-changing commands declare mission, player, solo and determinism requirements.
- Mutating commands are refused during netplay by default. A later synchronized-cheat protocol
  would be a separate design and issue.
- Every player-affecting command takes an explicit slot. Convenience defaults may select slot 0
  only when there is exactly one local player, and the result must state the resolved slot.
- If a game-side adapter temporarily changes an original current-player cursor, it restores the
  prior value before returning, including on error.
- Developer-tool use is visible in session metadata so a diagnostic report never presents a
  modified run as an ordinary one.

### Publication and game-data safety

- Nothing uploads automatically. One click means one click to create a local review directory.
- Never export a ROM, `base.zip`, EEPROM/save file, quickstate, raw memory, extracted asset,
  texture/audio dump or compiled game output.
- Never serialize raw pointers, arbitrary memory ranges, arbitrary game strings or a shell
  command transcript.
- Console evidence is built from parsed command IDs and allowlisted typed arguments. Raw input
  history may contain paths or secrets and is not included by default.
- Outputs are ordinary JSON, JSON Lines, Markdown and a metadata-free PNG in a directory outside
  the repository. Do not create an opaque archive.
- Every bundle remains a local draft until a human reviews every file and approves the exact
  issue submission.

## Workstream 1: modern in-game console

### User experience

A configurable desktop hotkey opens a searchable console whenever the binary includes Dear ImGui;
it does not depend on the optional developer overlay. The default is backquote/grave, with the
launcher/configuration surface exposing the binding and an environment setting remaining useful
for automation.

Opening the console in a solo mission requests a developer-owned pause reason and restores the
previous state when it closes. It must not clear a watch/menu pause that it did not create.
Multiplayer and netplay do not silently pause; the UI explains the active policy, and mutation is
blocked according to command metadata.

The console provides:

- completion and searchable help from registry metadata;
- command history for the current process;
- a bounded scrollback of timestamped structured results;
- clear success, refusal, usage and internal-error states;
- the resolved player, stage and tick for a state-changing action; and
- copy support for a selected safe result, without persisting raw input by default.

The first useful command set is intentionally small:

| Command family | Initial behavior |
| --- | --- |
| `help`, `commands`, `build`, `status` | Registry help, current build compatibility facts and current session facts. |
| `player list`, `player show <slot>`, `where <slot>` | Read through explicit-slot player APIs. |
| `objective list` | Read objective status through a verified accessor; mark unavailable fields honestly. |
| `god <slot> <on|off>` | Apply through a narrow game-side adapter. |
| `give <slot> <weapon>` and `ammo <slot> <amount|full>` | Validate symbolic names/IDs and refuse unsupported mission states. |
| `gibs <off|explosions|high_damage|always>` | Use a new runtime policy setter; never mutate the cached environment indirectly. |
| `restart` and `level <stage>` | Use controlled game transition/relaunch paths, never raw global assignment. |

Free camera and cheat adapters may follow once their existing behavior is verified. A shell,
arbitrary scripting evaluator, memory poke, raw cvar setter and save/quickstate commands are not
part of console v1.

The initial read-only handlers are registered during native boot through a copied provider table.
The provider names all four player slots explicitly and reaches live objectives only through
`gePortObjectiveCount()` / `gePortObjectiveStatus()`; no handler reads `g_CurrentPlayer` or an
objective pointer. `objective list` captures at most ten entries, reports total versus captured,
marks failed provider reads unavailable, and encodes presence/status in bounded typed payloads.
Provider state is sampled only when a command is queued, so an idle closed console does not add a
per-tick player walk.

Useful forms are:

```text
help
help player show
commands
build
status
player list
player show 0
where 0
objective list
gibs <off|explosions|high_damage|always>
god <slot> <on|off>
give <slot> <weapon-name|item-id>
ammo <slot> <0-1000|full>
restart
level <stage-id>
```

`gibs` is registered through a copied mutation-provider table and changes the cached port policy
through `gePortGibsSetMode()`; it never rewrites `GETV_GIBS`. Its enum argument and typed
previous/current-mode payload are diagnostic-safe and recordable. The command may set policy
before a mission or during local multiplayer, but the shared command pump refuses it during
netplay before the setter can run. Changing policy preserves existing character/hit bookkeeping
and consumes no gameplay RNG; the existing effect derives its private visual seed from tick and
character identity.

`god`, `give`, and `ammo` require an active mission and an occupied explicit slot. They are valid
in local multiplayer but, like every mutation, are refused before their provider runs in netplay.
The adapter is the only console code allowed to move the original game's implicit current-player
cursor and restores the prior slot after success, unsupported context, or provider refusal.
`give` accepts canonical player-facing weapon names, selected internal aliases, or validated
`ITEM_IDS` values 1-32, then adds and equips the weapon. `ammo` sets the equipped weapon's
ammunition through the game's existing clip/reserve policy with a bounded 0-1000 amount, or
resolves `full` through that weapon's game-defined maximum. A weapon without ammunition is an
explicit context refusal.

`restart` and `level <stage-id>` require an active solo mission. Like every mutation, the command
pump refuses them in netplay before their provider runs. `level` accepts only a numeric stage ID
from the tracked loadable-stage catalog; cut, title, and unknown IDs are argument refusals, while
the six multiplayer-only stages are explicit unsupported-context refusals. `restart` also refuses
a current stage outside the supported solo-mission set. After all validation succeeds, both
commands call a single provider backed by `bossSetLoadedStage()`, which schedules the engine's
normal graphics drain, stage unload, and reload. They never assign `g_StageNum` or
`g_MainStageNum`. Structured results target the requested stage and retain typed previous/requested
stage payloads, including provider failure results. A `level` request for the current stage is a
deliberate same-stage reload, but retains the `level` command ID and message. Native generated
pad and bound-pad sections use process-lifetime pointer identity to apply destructive coordinate
scaling only on their first load; every reload still refreshes stan/room links against the newly
loaded collision data.

`build` currently reports platform, architecture, renderer, console schema, and command schema.
The authoritative source commit/build-compatibility identifier belongs to the versioned native
`session.json` contract in issue #14; until that exists, this command does not invent one from the
collector checkout or process environment.

### Command contract

The registry is a C-facing port subsystem independent of ImGui. A command definition includes:

- canonical name, aliases, summary and typed argument schema;
- flags such as `read_only`, `mutates_game`, `requires_mission`, `requires_player`, `solo_only`,
  `recordable` and `diagnostic_safe`;
- a handler identifier and optional completion provider; and
- a stable command/schema version for future reproduction playback.

A result includes a monotonically increasing sequence, command ID, status code, severity,
submission and execution tick, rendered frame, resolved target and an allowlisted message or
typed payload. Diagnostic export uses this structure, not the raw line typed by the user.

### Delivery slices

1. ROM-free parser/registry/result-ring library, bounded queue and unit tests.
2. Cross-renderer ImGui console, SDL captv‹­¦ëm®éÜj×¢¸ Šv¥jšk£¦j×­¢G§r‹§·]uç~XYÛ›ÜİXÈØ\\™K‚‹H›ÈY]šY[ËÚ[]ÛœË[\ÜÈÜˆ\˜š]˜\H›Ü\HÜš]\È[ˆ[œÜXİÜˆŒKˆÜÙH\™BˆÛÛ[X[™ËÚ]ÛÛ[X[™Y]Y]H[™[ˆ]Y]™\İ[Yˆ^H\™HYY]\‹‚‚ˆÈÈÈ[]™\HÛXÙ\Â‚ŒKˆ™\œÚ[Û™YÛ˜\ÚİİXİ\™\Ë›İšY\ˆ[\™˜XÙ\È[™˜ZÙK\›İšY\ˆ\İË‚Œ‹ˆ^Y\‹Ù[™[^KÛØš™Xİ]™HØ\\™H]HÙ]YYœ˜[YH›İ[™\K‚ŒËˆ[QİZHX›\Ëš[\š[™ËİX›HÙ[Xİ[Ûˆ[™]™[\İÜK‚ˆØY™H”ÓÓˆ›Ú™Xİ[ÛˆÛÛœİ[YY]\ˆHXYÛ›ÜİXÜË‚‚ˆÈÈÈXØÙ\[˜ÙHØ]B‚‹HHRH™XYÈÛ›HÜ[İÛ™YÛ˜\ÚİÈ[™\È›È™]Z[™Y˜]ÈØ[YHÚ[\œË‚‹H[›İ\ˆ^Y\ˆÛİÈ\™H^XÚ]È›Èİ\œ™[\^Y\ˆİ\œÛÜˆ\ÈØ[\Y‚‹H[™[^HÙ[Xİ[Ûˆİ\š]™\È\İ™[Ü™\š[™È[™\È[˜[Y]YÛˆİYÙH\ØÚÚ[™ÙK‚‹HXœÙ[šY[Ëİ]XÈ]K\š]™Y]H[™]™H]H\™Hš\ÚX›H\İ[˜İ‚‹H›İ[™È[™[˜Ø][Ûˆ\™H\İY[™š\ÚX›K‚‹HÜ[‘Ó[™Y][ÚİÈHØ[YH]K[™Ø\\™HÛÜİ\ÈYX\İ\™YÚ]H[œÜXİÜˆÛÜÙY[™ˆÜ[‹‚‹HH“ÓKYœ™YH˜ZÙHÛ˜\Úİš]™\ÈHÛÛ\]HX›KÙš[\‹ÜÙ[Xİ[Ûˆ[Ù[[ˆ\İË‚‹HHØY™H”ÓÓˆ›Ú™Xİ[Ûˆ™Z™XİÈ˜]ÈÚ[\œÈ[™[œ™YÚ\İ\™Yœ™YKY›Ü›HšY[Ë‚‚ˆÈÈÛÜšÜİ™X[HÎˆÛ™KXÛXÚÈØØ[XYÛ›ÜİXÈ[™B‚ˆÈÈÈYX[š[™ÈÙˆÛ™HÛXÚÂ‚[ˆ[‹YØ[YH
ŠØ\\™HXYÛ›ÜİXÜÊŠˆXİ[Û‹\ÈHÛÛœÛÛH[X\ËÜ™X]\ÈÛ™H™]ÈØØ[\™XİÜKœÚİÜÈ]È][™İ]\È]›İ[™ÈØ\È\ØYYˆH\™XİÜH\È™XYH›Üˆ[X[ˆ™]šY]È[™™›Üˆ[\ÜHÛÛËØÛÛXİØY×Ü™\ÜœX‚‚”X›\Ú[™È[ˆ\ÜİYH™[XZ[œÈHÙ\\˜]K^XÚ]\›İ˜[İ\ˆHØ[YH]\İ›İØ[Ú]X‹›Ü[ˆHœ›İÜÙ\ˆİX›Z\ÜÚ[ÛˆÜˆÚ[[H]XÚš[\Ë‚‚ˆÈÈÈİ\œ™[ŒH[\[Y[][Û‚‚‘\ÚİÜZ[È›İÈ™\Ù\™HŒØ›ÜˆHØØ[XYÛ›ÜİXÈØ\\™KˆHÙ^H\ÈYÙK]šYÙÙ\™Y
ÑšÙ^H™\X]Ù\È›İÜ™X]H][\H[™\ÊK\ÈÛÛœİ[YY™Y›Ü™HØ[Y\^H[œ][™ÛÜšÜÈ]™[‚Ú[ˆH]™[Ü\ˆÛÛœÛÛHİÛœÈÙ^X›Ø\™[œ]‚‚•H˜]]™HŒH\™XİÜHÛÛZ[œÎ‚‚‹H™\Ü›YˆØØ[[Û›H™\ÜİXˆÚ]H^Xİœ˜[YKİXÚËÜİYÙKÙY™šXİ[NÂ‹HÙ\ÜÚ[Û‹šœÛÛ˜ˆØÚ[XH™\œÚ[ÛˆK]›Ü›KØ\˜Ú]Xİ\™KÜ™[™\™\‹ØY™HY™™Xİ]™HÙ][™ÜËˆ™\ÛÛ™Y[œ]š[™[™ÜËØÜ™Y[œÚİİ]\È[™\İYœ˜[YHY[]NÂ‹Hİ]KšœÛÛ˜ˆ›İ[™Y^Y\ˆšY[È[™Øš™Xİ]™Hİ]\Ù\ÎÂ‹H]™[ËšœÛÛ›ˆH™]Ù\İ\Y]™[X\È™XÛÜ™ËÛ\İš\œİÈ[™‹HØÜ™Y[œÚİ˜›\ˆH™[™\™\‹[˜]]™HØÙ[™HØ\\™K‚‚“Ü[‘Ó™XYÈHØ[YH™KR[QİZHØÙ[™H\ÙYHÑU—ÔÒÕ”SQXˆY][[\Ü˜\š[HXZÙ\ÈÛ›HB‘ŒÈœ˜[YIÜÈ˜]ØX›H™XYX›K›]ÈHØ[YKÜÜİ™\İ[ÈHš]˜]H^\™H™Y›Ü™H[QİZK[™œ™\İÜ™\Èœ˜[YXY™™\“Û›XY\Ø\™ˆHÚ[][[™[İ\ÈÑU—ÔÒÕ”SQX™\]Y\İ™[XZ[œÈ[™\[™[‚‚•\È\È[[[Û˜[HH
Š›ØØ[Ø\\™H›Ü›X]
Š‹›İY]HX›XØ][Û‹\™XYH[™H\ØÜšX™Y˜HHXØÙ\[˜ÙHØ]H™[İËˆH[›š[™Èš[˜\HÙ\È›İİ\œ™[H^Ü[ˆ]]Üš]]]™B˜Z[ÛÛ[Z]ØÛÛ\]Xš[]HQÛÈÜÙHšY[È\™H^XÚ][˜[Y\È˜]\ˆ[ˆİY\ÜÙY™œ›ÛHH]\ˆÛÛXİÜˆÚXÚÛİ]ˆ˜]]™HŒH[ÛÈÙ\È›İY]Ü™X]HX[šY™\İšœÛÛ˜˜ÛÛ[X[™ËšœÛÛ›[œÜXİÜ‹šœÛÛ˜ÒKLMˆ[šY\ÈÜˆHY]Y]KYœ™YH‘ÎÈÜÙH™[XZ[‚˜ÛÛXİÜ‹ÜØÚ[XH›ÛİË[ÛˆÛÜšËˆÛÛËØÛÛXİØY×Ü™\ÜœX™[XZ[œÈHØ[š]^˜][Ûˆ[™”‘Ë[›Ü›X[^˜][Ûˆ›İ[™\H™Y›Ü™H[][™È\ÈÚ\™Y‚‚‘ŒÈ[ÛÈØ[››İXYÛ›ÜÙHH\™œ™Y^™HÛ˜ÙHHXZ[ˆ™XY\ÈİÜY[\[™ÈÑ]™[Ë‚˜ÑU—ÔÕSPÑOLX›İÈÛİ™\œÈ]Ø\Ú]HXYÛ›ÜİXË[Û›HØ]ÚÙËˆHØ[YH™XYX›\Ú\Â›Û›H[YÙ\ˆ\ÙKÚİÜİ[[Y]H›İYÚÑ]ÛZXÜÎÈHØ]ÚÙÈ™XY™]™\ˆ™XYÈ]™HØ[YBœİXİ\™\ËˆYˆHXXÜ›È\ÙHÙ\È›İY˜[˜ÙH›ÜˆL\È]™\ÜÈH\ÙK\İ›ÛÛK[ØYÓÔÂšİÜİ[™›İ[™Y\‹Yœ˜[YHÛİ[\œË™\X][™È][ÜİÛ˜ÙH\ˆÙXÛÛ™[[›ÙÜ™\ÜÈ™\İ[Y\Ë‚•\È\È[[™YÈØØ[^™HH[™È™Y›Ü™HHXYÙÙ\‹Ü\™ˆ\ÜË›İÈ™\XÙHİXÚÈØ[\[™ÈÚ[‚H™\ÜY\ÙHİ[ÛÛZ[œÈÛÈ]XÚÛÙK‚‚ˆÈÈÈ™[™\™\ˆ[™ÔH›YÚ™XÛÜ™\œÂ‚•HKŒ™YH[ÛÈÛÛZ[œÈHY\\ˆ™[™\™\‹Y›Ü™[œÚXÜÈ^Y\ˆÜ™X]Y›ÜˆH[[\š\ÈH[™Âš[™\İYØ][Û‹ˆ]\È[X™\˜][HÜ›X[\š[™È›Ü›X[^Kˆ›İ[™È\È[ØØ]YÜˆÜš][‚[›\ÜÈHÛÜœ™\ÜÛ™[™ÈXYÛ›ÜİXÈÙ][™È\È^XÚ]Hİ\YY‚‚‹HÑU—ÑÔQ“QÒO]˜Ü™X]\È[ˆ[X\X˜XÚÙY›Û[™È™XÛÜ™ÙˆÜ[‘ÓİX›Z\ÜÚ[ÛœËˆ™XÛÜ™ÈØ\œBˆœ˜[YKÙ˜]ÈÙ\šX[Ë“ÈÚ^™\È[™\Ú\ËÚY\‹Ü›ÙÜ˜[HY[]K^\™HY[]H[™\Ú\Ëˆ\Ø›[™ÙXØ[İ]K˜\İÑÛÛ[X[™ÛÛ^[™\Ü^K[\İ›İ™[˜[˜ÙK‚‹HÑU—ÑÔQ“QÒÔVSĞQÕ‘T•ÏO˜Y][Û˜[H™]Z[œÈ^Xİ“È^[ØYÈ›ÜˆHÙ[XİY™\^ˆÛİ[ÛÈH\™Ø\™H˜]ÚØ[ˆ™HÛÛ\\™Y]KY›Ü‹X]HÚ]İ]™XÛÜ™[™È]™\H˜]È^[ØY‚‹HÑU—Ñ•S‘“QÒO]˜™XÛÜ™ÈH™[™\™\ˆØ[ÚZ[ˆ
ÙÜ[˜\Ü^K[\İ^Xİ][Û‹ˆ›\Ú\ËÛY™™\‘]XÛ˜]Ğ\œ˜^\Ø
H[™[šÜÈXXÚ˜]Ú˜XÚÈÈHÔKY›YÚÙ\šX[‚‹HÑU—ÔÕSPÑOLX[œÈHİËYœ™\]Y[˜ŞHØ]ÚÙÈ\ØÜšX™YX›İ™KˆÛˆHİ\İZ[™Yİ[]ˆœ™Y^™\ÈH[˜İ[Û‹Y›YÚš[™ÈÛÈH™XÛİ™\˜X›H[‹\ÙXÛÛ™ÔH[™ÈØ[››İİ™\Üš]HBˆİX‹\ÙXÛÛ™\İÜH]Y[È]‚‚•HÛÛ\[š[ÛˆÛÛÈ\™HXÛÙWÙÜWÙ›YÚœXXÛÙWÙ[˜İ[Û—Ù›YÚœX˜X\ÙÜWÚ[™×Ù˜]ËœX[˜[^™WÙœ›Ş™[—ÙÜWÚ[™ËœXØ\\™WÙÜWÚ[™ËœÚ[™˜[—ÙÜWÚ[™×Ø]]ØØ\\™KœÚˆH[˜][™Y][˜Ú\ˆ^\İÈ™XØ]\ÙHH™X[ÔHÙYÙHØ[ˆœ™Y^™BHÙ^X›Ø\™[İ\ÙH[™\ÚİÜ[Û™ÈÚ]HØ[YNˆ]™KX]]Üš^™\ÈHš]š[YÙYØ\\™KØZ]È›ÜˆH[˜İ[Ûˆš[™ÈÈœ™Y^™H[™›ÜˆNLMIÜÈš\œİY\œ›Üˆİ]HÈ˜[YHHØ[YHQ[‚˜Ø\\™\ÈH]šY[˜ÙH[™\›Z[˜]\ÈHØ[YHÚ]İ]™\]Z\š[™È[œ]\š[™ÈHİ[‚‚•\ÙH˜]È›YÚš[\È\™HØØ[[™Ú[™Y\š[™È]šY[˜ÙK›İYË\™\Ü]XÚY[Ëˆ^HØ[ˆ™Bš[™™YÈÙˆYYØX]\È[™X^HÛÛZ[ˆ^Xİ˜[œÚY[™[™\ˆY™™\œËˆ™YXÙH[HÈ›İ[™Y^Ú]HXÛÙ\‹Ø[˜[^™\ˆÛÛÈ™Y›Ü™HÚ\š[™È[][™ËˆH[ÛÜšÙ›İÈ[™HKŒ[[Ø\ÙBœİYH\™H[ˆØ‘S‘T‘T—Õ“ÕP“TÒÓÕS‘Ë›YJ‘S‘T‘T—Õ“ÕP“TÒÓÕS‘Ë›Y
K‚‚ˆÈÈÈ]]Üš]]]™HÙ\ÜÚ[ÛˆX[šY™\İ‚YH™\œÚ[Û™Y˜]]™HÙ\ÜÚ[Û‹šœÛÛ˜ˆ]\È]]Üš]]]™H›ÜˆHš[˜\H[™Ù\ÜÚ[Ûˆ]Ù\™B˜XİX[H\İY‚‚‹HXYÛ›ÜİXÈØÚ[XH™\œÚ[Ûˆ[™Ø\\™H[Y\İ[\Â‹Hš[˜\HZ[ÛÛ[Z][™Z[ÛÛ\]Xš[]HQÂ‹H]›Ü›K\˜Ú]Xİ\™H[™™[™\™\ˆ™\ÜYHH[›š[™Èš[˜\NÂ‹HİYÙKY™šXİ[KİYÙH\ØÚØ[YHXÚÈ[™™[™\™Yœ˜[YNÂ‹HY™™Xİ]™H[[YHÛÛ™šYİ\˜][Û‹œ›ÛH[ˆ[İÛ\İÙˆ›Û‹\ÙXÜ™]Ù][™ÜÎÂ‹H]™[Ü\‹]ÛÛİ]H[™Ú]\ˆ]]][ÛˆÛÛ[X[™ÈÙ\™H\ÙYÂ‹H\Y˜Xİ\Kš[[˜[YK]HÛİ[[™ÒKLMÈ[™‹H[˜Ø][Û‹[˜]˜Z[X›K\›İšY\ˆ[™Ø\\™KY˜Z[\™H™X\ÛÛœË‚‚•\ÈÛÜœ™XİÈÛÈ[Z]][ÛœÈ[ˆHİ\œ™[ÛÛXİÜ‹ˆ]È™\ÜÚ]ÜHY]Y]H\ØÜšX™\ÈB˜ÚXÚÛİ][›š[™ÈH]ÛˆÛÛÚXÚX^H›İ™HHÚXÚÛİ]]Z[HØ[YKˆ]Â˜ÑU—Ê˜ØØ[ˆ\ØÜšX™\ÈHÛÛXİÜˆ›ØÙ\ÜËÚXÚX^H›İ]™H[š\š]YHØ[YIÜÈY™™Xİ]™B˜ÛÛ™šYİ\˜][Û‹ˆY\ˆÙ\ÜÚ[Ûˆ[\ÜH™\ÜÚİ[˜[YH\ÙHÙ\\˜][H\È
Š\İYš[˜\B˜[™Ù\ÜÚ[ÛŠŠˆ™\œİ\È
Š˜ÛÛXİÜˆÚXÚÛİ][™[š\›Û›Y[
Š‹‚‚•[šÛ›İÛˆØÚ[XH™\œÚ[ÛœÈ˜Z[ÛÜÙYÚ]H\ÙY[\œ›Ü‹ˆ™]ÈÜ[Û˜[šY[ÈX^H™HYYÚ][‚˜H™\œÚ[Û‹]Ú[™Ú[™ÈYX[š[™ÈÜˆXØÙ\[™ÈH™]È\Y˜Xİ˜[Z[H™\]Z\™\ÈHØÚ[XH[\[™\İË‚‚ˆÈÈÈ[™HÛÛ[Â‚•HÛ™KXÛXÚÈ\™XİÜHX^HÛÛZ[ˆÛ›H™YÚ\İ\™Y›İ[™Y\Y˜XİÎ‚‚Ÿš[HÛÛ[ÈŸKKHKKHŸ™\Ü›YØØ[˜YÚ]Ù\ÜÚ[Ûˆ˜XİËØ\\™HÛÛ[È[™ÜXÙ\È›ÜˆXİX[Ù^XİYÜ™\›ÙXİ[Û‹ˆŸX[šY™\İšœÛÛ˜ØY™]Hİ]H[™\Ú\È›Üˆ]™\HİYÙY\Y˜XİˆŸÙ\ÜÚ[Û‹šœÛÛ˜]]Üš]]]™H\İYXš[˜\H[™Y™™Xİ]™K\Ù\ÜÚ[Ûˆ˜XİËˆŸÛÛ[X[™ËšœÛÛ›™XÙ[XYÛ›ÜİXË\ØY™H\œÙYÛÛ[X[™È[™™\İ[ÎÈ›È˜]È[œ][™\ËˆŸ]™[ËšœÛÛ›™XÙ[™YÚ\İ\™Y]™[ÈÚ]XÚËÙœ˜[YH[™\Y[YÙ\ˆšY[ËˆŸ[œÜXİÜ‹šœÛÛ˜Û™H›İ[™YÛ˜\Úİ\ÈÙ[XİY™XÛÜ™\Ú[™ÈH[œÜXİÜˆØÚ[XKˆŸØÜ™Y[œÚİœ™ØÜ[Û˜[Y]Y]KYœ™YHØÙ[™HØ\\™HZÙ[ˆ™Y›Ü™HH]™[Ü\ˆİ™\›^H\È˜]Û‹ˆ‚“Z\ÜÚ[™ÈÜ[Û˜[\Y˜XİÈ\™H™\™\Ù[YHH™X\ÛÛˆ[ˆHX[šY™\İˆÙ[™\˜[İİ]Üİ\œ‚›ÙÜÈ[™Ü˜\Ú™\ÜÈ™[XZ[ˆÜ[Û˜[[œ]ÈÈH]ÛˆÛÛXİÜ‹ÚXÚ[™XYHØ[š]^™\Â[KˆH˜]]™H^Ü\ˆÚİ[›İÛÜH\˜š]˜\Hš[\Ë‚‚’YˆHİ\œ™[˜]]™H™[™\™\ˆ\È›ÈØY™HY]Y]KYœ™YH‘È]YÜˆY\HÙ\\˜][Bœ™]šY]ÙY\›Z\ÜÚ]™H[˜ÛÙ\ˆÚ]]È›İXÙKÜˆXZÙHØÜ™Y[œÚİÛÛ\][ÛˆH›Øİ\ÙY™\™\]Z\Ú]K‚‘È›İØ[H]›Ü›HØÜ™Y[œÚİÙ\šXÙH]X^HØ\\™Hİ\ˆÚ[™İÜË[™È›İÚ\H“T˜\ÈHİ\ÜÙYHX›XØ][Û‹\™XYH]XÚY[‚‚ˆÈÈÈÛÛXİÜˆ[™ÚÚ[[YÜ˜][Û‚‚‘^[™ÛÛËØÛÛXİØY×Ü™\ÜœXÚ]HÙ\ÜÚ[Û‹Z[\ÜÜ[Ûˆ]‚‚‹H˜[Y]\ÈHØÚ[XKš[HÙ]›İ[™Ë\Ú\ËU‹NÒ”ÓÓˆÚ\H[™\Y˜Xİ\\ÎÂ‹H™Y\Ù\ÈŞ[[[šÜË˜]™\œØ[[šÛ›İÛˆš[\ËØ]™\Ë\˜Ú]™\È[™š[˜\H^[ØYÎÂ‹H™X]ÈH[›š[™ÈØ[YIÜÈX[šY™\İ\È]]Üš]]]™H›Üˆ\İYš[˜\KÜ™[™\™\‹ÜİYÙKÜÙ][™ÜÎÂ‹H™XÛÜ™ÈÛÛXİÜˆÚXÚÛİ]ÜŞ\İ[H[™›Ü›X][Ûˆ[™\ˆHÙ\\˜]HÙ^NÂ‹H™K\[œÈHX›XØ][ÛˆİX\™İ™\ˆ]™\H[\ÜY\Y˜XİÂ‹H™\Ù\™\È^\İ[™ÈX[X[ÙËØÜ˜\ÚÜØÜ™Y[œÚİ[œ]ÎÈ[™‹HÜ™X]\ÈHœ™\Úİ]]\™XİÜH˜]\ˆ[ˆY][™ÈHÛİ\˜ÙHØ\\™H[ˆXÙK‚‚•\]HÛÛËİ\İËİ\İØYÙ[İÛÛËœXØÜËĞQÑS•P×ĞÓÓ•’P•US‘Ë›Y[™˜˜YÙ[ËÜÚÚ[ËÜ™\ÜYÛÛ[™^YKXYËÔÒÒS›Y[ˆH›Øİ\ÙYˆ]Ú[™Ù\È\È]šY[˜ÙB˜ÛÛ˜XİˆHÚÚ[Úİ[[H™\Ü\ˆÈØ\\™HØØ[K[œÜXİ]™\Hš[KÙX\˜Ú›ÜˆB™\XØ]H[™ØZ[ˆ^XÚ]\›İ˜[›ÜˆH^Xİ\ÜİYH›ÙH[™]XÚY[Ë‚‚ˆÈÈÈXØÙ\[˜ÙHØ]B‚‹HÛ™H[‹YØ[YHXİ[ÛˆÜ™X]\ÈH™]È™]šY]È\™XİÜHİ]ÚYHH™\ÜÚ]ÜH[™ÛX\›Hİ]\Âˆ]]Ø\È›İ\ØYY‚‹HH\İYš[˜\HÛÛ[Z]™[™\™\‹İYÙKÙY™šXİ[H[™Y™™Xİ]™HÙ][™ÜÈÛÛYHœ›ÛHBˆ[›š[™ÈÙ\ÜÚ[Û‹›İH]\ˆÛÛXİÜˆ›ØÙ\ÜË‚‹HİXİ\™YÛÛ[X[™Ù]™[Ú[œÜXİÜˆ\Y˜XİÈ\™H›İ[™YØÚ[XK]˜[Y]Y[™ØY™HBˆÛÛœİXİ[Û‹‚‹HHØÜ™Y[œÚİ^ÛY\ÈHİ™\›^H[™\ÈHY]Y]KYœ™YH‘ËÜˆHX[šY™\İİ]\ÈÚH›ÂˆØÜ™Y[œÚİØ\ÈØ\\™Y‚‹HH^Ü\ˆ™]™\ˆ[˜ÛY\ÈH“ÓKØ]™K]ZXÚÜİ]K˜\ÙKš\^˜XİY]K˜]ÈY[[ÜKˆ\˜š]˜\Hš[HÜˆ\˜Ú]™K‚‹HÛÛXİÜˆ\İÈÛİ™\ˆ˜[Y[\Ü[šÛ›İÛˆØÚ[XK[\\™Y\Ú˜]™\œØ[ÜŞ[[[šË[™^XİYˆ\Y˜Xİİ™\œÚ^™Y]K›Ü˜šY[ˆš[[˜[YKØÛÛ[[™[\›˜[H[˜ÛÛœÚ\İ[Z[ÜÙ\ÜÚ[Û‚ˆY[]HšY[ËˆHÛÛXİÜ‹XÚXÚÛİ]Z\ÛX]Ú\È[İÙY[™™\ÜY›İ™X]Y\ÈH˜YˆØ\\™K‚‹HH^\İ[™ÈX[X[ÛÛXİÜˆÛÜšÙ›İÈ™[XZ[œÈÛÛ\]X›K‚‹HHÚXÚÙYZ[ˆYË\™\ÜÚÚ[[™ÛÛšX][™ÈØİ[Y[][ÛˆX]ÚH[\[Y[YØÚ[XK‚‚ˆÈÈÜ[Û˜[YÚ]˜[YH›ÛİË[ÛœÂ‚‘È›İØÚY[H\ÙH\È[\[Y[][Ûˆ\ÜİY\È[[H™YHš[Üš]^™YÛÜšÜİ™X[\È]™H™X[\ÙH[™YX\İ\™[Y[Ë‚‚ˆÈÈÈ™\›ÙXİ[Ûˆ™XÛÜ™[™È[™^X˜XÚÂ‚•\È™XÛÛY\È\ÙY[˜]\ˆ[ˆÜXİ[]]™KÛ˜ÙHÛÛ[X[™È]™HİX›HQËİ]H\ÈİX›BšY[]H[™XYÛ›ÜİXÜÈÛ›İÈH\İYZ[ˆHÚ\™XX›H™XÛÜ™[™ÈÚİ[ÛÛZ[ˆÛ›Bœ\‹]XÚÈÛÛ›Û\ˆ[[œ˜[YH[KŞ[˜Ú›Ûš^™Y™XÛÜ™X›HÛÛ[X[™Ë“‘Èš[™Ù\œš[È[™™\œÚ[Û™YY]Y]Kˆ]]\İ›İÛÛZ[ˆHØ]™K˜]ÈY[[ÜHÜˆ^˜XİYØ[YH]K‚‚•HÛËÛ›ËYÛÈ]Y\İ[Ûˆ\È˜XİXØ[ˆY\ˆÛÛœÛÛK[œÜXİÜˆ[™[™\È\™H\ÙYÛˆ™X[YÜË˜\™H[\Ü[™\ÜÈİ[\™È™\›ÙXÙHœ›ÛHZ\ˆİXİ\™Y]šY[˜ÙOÈYˆY\Ë›İİ\Bœ™XÛÜ™[™ÈYØZ[œİ]\›Z[š\İXÈ\İ[œÈ[™™\]Z\™HH^X˜XÚÈ]™\™Ù[˜ÙH™\ÜˆYˆ›ËÙY\HÛX[\ˆÛÛË‚‚•Hİ\œ™[X›XØ][ÛˆÚXÚÙ\ˆ™Z™XİÈ[™^XİYš[˜\H›Ü›X]È[™\˜Ú]™\ËˆH]\™BœÚ\™XX›H™XÛÜ™[™È™YYÈ[ˆ^XÚ]H˜[Y]Y^Ò”ÓÓˆ™\™\Ù[][ÛˆÜˆH[X™\˜]K\İYØY™]K\ÛXŞH\]KˆÈ›İÜ™X]H[ˆÜ\]YH™Ù\™\›Ø[™\\ÜÈHÚXÚÙ\‹‚‚ˆÈÈÈØ[YK\›ØÙ\ÜÈ]ZXÚÜİ]\Â‚•\ÙHØ[ˆÜYYØØ[]\˜][Ûˆ]Úİ[[š]X[H™H\[Y\˜[Ø[YK\›ØÙ\ÜÈ[™Ø[YKXZ[‚•^H™\]Z\™HHİXœŞ\İ[HØ]™KÜ™\İÜ™H™YÚ\İK^XÚ][˜[Y][Ûˆ[™›ÛÙˆ]Ú[\œËœ™[™\™\ˆ™\Ûİ\˜Ù\Ë]Y[Èİ]H[™^\›˜[[™\È\™H›İ™\İÜ™Y\Èİ[H]\Ëˆ^H\™B›™]™\ˆHYË\™\Ü]XÚY[‚‚ˆÈÈÈÜX›HÙ[X[XÈÛ˜\ÚİÂ‚HÜX›HÛ˜\ÚİÛİ[™\İÜ™H˜[YYÙ[X[XÈİ]H›İYÚ™\œÚ[Û™YY\\œÈ˜]\ˆ[‚˜ÛÜZ[™ÈSKˆ]\È]XÚ[Ü™H^[œÚ]™H[™Ø[ˆİ[˜Z[È™XÜ™X]HRKØÜš\Ë[Y\œÈ[™œ™[™\™\ˆİ]KˆÛÛœÚY\ˆ]Û›HYˆ]\›Z[š\İXÈ^X˜XÚÈ[™ØØ[]ZXÚÜİ]\ÈX]™HHYX\İ\™Y™XYÛ›ÜİXÈØ\ˆH™XY[Û›HÙ[X[XÈÛ˜\Úİ\ÙY\È]šY[˜ÙH\È[™XYH›İšYYHBš[œÜXİÜÈ™\İÜ˜][Ûˆ\ÈHÙ\\˜]H›Ø›[K‚‚ˆÈÈ[]™\H[™ÛÛÜ™[˜][Û‚‚•\ÙHÛ™H˜XÚÚ[™È\ÜİYK™YHÚ[\ÜİY\È[™H›ÜÜÙY]™[Ü\ˆÛÛ[™ÈŒXZ[\İÛ™KˆBœ™\ÜÚ]ÜHİ\œ™[H\ÈHİ[™\™[š[˜Ù[Y[[™Øİ[Y[][Û˜X™[È]›ÈYXØ]Y™]™[Ü\‹]ÛÛÈX™[È\ÙH^\İ[™ÈX™[È[š]X[H˜]\ˆ[ˆÜ™X][™È^Û›Û^H›Üˆ›İ\‚š\ÜİY\Ë‚‚”™XÛÛ[Y[™YÜ™\‚‚ŒKˆ[™ÛÛœÛÛH™YÚ\İKÜ]Y]YH[™Ü›ÜÜË\™[™\™\ˆRKÚ[œ]™Z]š[Ü‹‚Œ‹ˆ[™™\šYšYY™XY[Û›H[™]]][ÛˆY\\œË‚ŒËˆ[™H™XY[Û›HÛ˜\Úİ[Ù[[™[œÜXİÜ‹‚ˆ[™˜]]™HXYÛ›ÜİXÈ^Ü[ˆÛÛXİÜ‹ÜØÚ[XKÜÚÚ[[YÜ˜][Û‹‚‚™Y›Ü™Hİ\[™ÈXXÚ‹™XÚXÚÈİ\œ™[\ÜİY\ËœÈ[™ØØ[]›Ü›HÛÜšËˆÛÛœÛÛHRKÚ[œ]\Â™\ÜXÚX[HZÙ[HÈİ™\›\][˜Ú\‹Ñ[œ][™™[™\™\ˆÚ[™Ù\Ëˆ™X˜\ÙHÛÈİ\œ™[XZ[˜˜[™ÙY\XXÚˆ]Û™HXœİ˜Xİ[Ûˆ›İ[™\K‚‚•H^\İ[™È›İY›Øİ\ÙYØÜËÔ“ĞQPT›YÚİ[™XÙZ]™HÛ›HHÚÜ[šÈY\ˆ\È[ˆ\Â˜XØÙ\YÈ\XØ][™È\È\˜Ú]Xİ\™H\™HÛİ[Ü™X]HÛÈÛİ\˜Ù\ÈÙˆ]‚‚ˆÈÈ™]šY]ÈXÚ\Ú[ÛœÂ‚\›İ™H\È[ˆÛ›HYˆHXZ[Z[™\ˆYÜ™Y\ÈÚ][ÙˆH›ÛİÚ[™Î‚‚‹HH[Ù\›ˆÛÛœÛÛH\ÈÜ[İÛ™Y[™Ù\\˜]Hœ›ÛH˜\™IÜÈÜšYÚ[˜[XYË[Y[HRNÂ‹H[š]X[]]][ÛˆÛÛ[X[™È\™H˜\œ›İË^XÚ]\Ûİ[™›ØÚÙY[ˆ™]^NÂ‹HÛÛœÛÛHÜ[š[™È\Ù\ÈİÛ™Y]\ÙHÙ[X[XÜÈ[™™]™\ˆXZÜÈ\Y[œ][ÈØ[Y\^NÂ‹HÜ[‘Ó[™Y][\™H›İ\ÙˆÛÛœÛÛKÚ[œÜXİÜˆXØÙ\[˜ÙKÚ[H›ËR[QİZHZ[È™[XZ[‚ˆİ\ÜYÂ‹HH[œÜXİÜˆ\È™XY[Û›H[™Û˜\ÚİX˜\ÙYÚ]İX›HQÈ[™Û™\İXœÙ[šY[ÎÂ‹HÛ™KXÛXÚÈXYÛ›ÜİXÜÈYX[œÈØØ[Ø\\™K›İ]]ÛX]XÈX›XØ][ÛÂ‹HH[›š[™ÈÙ\ÜÚ[Ûˆ\È]]Üš]]]™H›Üˆ\İYZ[ØÛÛ™šYİ\˜][ÛˆY]Y]NÂ‹HXYÛ›ÜİXÈ^ÜÈ™]™\ˆÛÛZ[ˆØ]™\Ë]ZXÚÜİ]\Ë˜]ÈY[[ÜK\˜Ú]™\ÈÜˆ\˜š]˜\Hš[\ÎÂ‹HH™YHÛÜšÜİ™X[\ÈX^H\ÙHÙ]™\˜[›Øİ\ÙYœÈ[œİXYÙˆÛ™H\™ÙH[\[Y[][ÛˆÈ[™‹H™\›ÙXİ[Ûˆ^X˜XÚÈ[™İ]H™\İÜ˜][Ûˆ™[XZ[ˆÜ[Û˜[[[™X[\ØYÙH[[Ûœİ˜]\ÈBˆY][Û˜[˜[YK‚‚’Yˆ[HÙˆÜÙHXÚ\Ú[ÛœÈÚİ[Ú[™ÙK™]š\ÙH\ÈØİ[Y[[™HÛÜœ™\ÜÛ™[™È\ÜİYH˜Y˜™Y›Ü™HÜ™X][™ÈÚ]Xˆ\ÜİY\ËˆÛ˜ÙHXØÙ\YÜ™X]HHZ[\İÛ™H[™˜XÚÚ[™È\ÜİYHš\œİYH\ÜÚYÛ™Y\ÜİYH[X™\œÈÈHÚ[˜YË[ˆX›\ÚH™YHÚ[\ÜİY\Ë‚