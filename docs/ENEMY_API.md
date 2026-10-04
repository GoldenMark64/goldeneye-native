# Enemy API

**Status: port-side API implemented; live game-side source adapter not yet installed.**

The query implementation lives in:

- `getv/port/src/ge_enemy_api.h`
- `getv/port/src/ge_enemy_api.c`

The current consolidated decompilation patch does not install the `ChrRecord` source callbacks,
so a normal current build has no live enemy source for this API unless another caller explicitly
installs one. With no source installed, the API deliberately returns an empty/absent result rather
than inventing data.

This page separates what is already implemented from what still needs wiring.

## Why this API exists

Static level data can tell a bot where guards were placed. It cannot answer live questions such as:

- which character slots are currently occupied;
- where a living enemy is now;
- how much damage it has taken;
- whether it is alert;
- where it last believed its target to be; or
- how recently it saw or heard that target.

GoldenEye already tracks those facts in `ChrRecord`. The enemy API is the port-side query surface
for exposing them without making every consumer depend directly on game-private structs.

## Implemented port-side surface

The public header defines:

```c
void geEnemySourceInstall(GeEnemyCountFn count_fn, GeEnemyAtFn at_fn);
int geEnemySourceInstalled(void);

int geEnemyCount(void);
int geEnemy(int index, GeEnemy *out);
int geEnemyById(int id, GeEnemy *out);
int geEnemiesNear(float x, float y, float z, float radius,
                  GeEnemy *out, int max);
int geEnemyThreatAt(float x, float y, float z, float radius);
```

The source is registered as two callbacks:

- a slot-count callback; and
- a callback that fills one flat, versioned field row for a requested slot.

Both callbacks must be present. Installing only one is rejected by uninstalling the source, because
"there are enemies but none can be described" would be a misleading partial state.

## Field model

The flat source row currently has fourteen slots:

- position: x/y/z;
- accumulated damage and maximum damage;
- alertness and hearing scale;
- last-known target position;
- frames since target was seen/heard;
- character id; and
- alive/dead state.

The callback returns a `GE_EN_*` mask saying which groups are actually valid.

That same rule carries into `GeEnemy`: unavailable data must stay distinguishable from a real
zero. A guard at full health and a guard whose health could not be read are not the same fact.

## Query behavior

### `geEnemy()`

Reads one character slot. Empty or invalid slots return no enemy.

### `geEnemyById()`

Looks up a live row by GoldenEye character id (`chrnum`). Use this when a consumer wants to follow
one character across frames rather than depending on a slot index.

### `geEnemiesNear()`

Returns living enemies within a horizontal radius, nearest first. The navigation layer treats this
as a floor-plan query, so Y is deliberately not part of the distance calculation.

### `geEnemyThreatAt()`

Counts living enemies whose **belief position** lies within the requested radius.

This is intentionally different from "how many enemies are physically near here." An empty
location can still be dangerous if several guards are converging on the place where they last saw
their target.

The threat query does not filter out a character merely because its current alertness is low:
belief describes where it is acting toward, while alertness describes another part of its current
state.

## What is still missing

The port-side API cannot name `ChrRecord` directly because the port layer does not compile against
the game's private type surface.

The remaining integration is a small game-side adapter in the patched decompilation that:

1. returns the character-slot loop bound;
2. reads one slot safely;
3. fills the fourteen-field row;
4. returns the correct `GE_EN_*` availability mask; and
5. installs the callbacks with `geEnemySourceInstall()` at the appropriate lifetime boundary.

The adapter should uninstall the source on level teardown so stale pointers cannot survive into the
next level.

As of this documentation update, no `geEnemySourceInstall` / `gePortEnemy*` source adapter is
present in the current `getv/patches/0001-source.patch`, and the later playtested source-catchup
patch does not add one either.

## Where the adapter belongs

The natural implementation point is beside the existing game-side port accessors that already
bridge private GoldenEye state into flat values.

Do **not** solve this by duplicating `ChrRecord` layout in the port layer. That would turn a
private game-structure change or native-layout correction into silent field corruption.

The flat callback boundary exists specifically to avoid that coupling.

## Relationship to bots

The enemy API is not the bot AI itself. It is one possible live-observation source.

A bot can operate from simpler sensing/world APIs today; once the live source adapter lands,
`geEnemyThreatAt()` and the belief fields can support more informed retreat, pursuit and
destination scoring.

See [`BOTS.md`](BOTS.md) for the current bot architecture and policy layer.

## Completion criteria

This API should be described as fully live only after a normal clean reconstruction:

- installs the source automatically during gameplay;
- reports nonzero live enemies on a stage with guards;
- distinguishes empty/dead/live slots correctly;
- returns stable `chrnum` lookup results;
- exercises health/alert/belief field masks; and
- tears the source down cleanly between levels.

Until then the accurate status is: **implemented query layer, pending live game-side source wiring**.
