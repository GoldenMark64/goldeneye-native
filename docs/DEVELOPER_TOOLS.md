# Developer Tools guide and FAQ

Open the desktop launcher and choose **Developer Tools** in the left sidebar. The page groups
optional diagnostics without changing normal gameplay defaults. Its settings apply to the next
launch; they are not saved into `goldeneye.cfg`. The desktop ImGui launcher supports this page on
macOS, Windows and Linux; it is not added to the native iOS/tvOS launcher.

## Which switch should I use?

| Control | What it does | Where the results appear |
|---|---|---|
| Record telemetry for this launch | Measures runtime prop-slot occupancy, peak usage, allocations, frees and allocation failures. | Local `.jsonl` report and `.jsonl.txt` summary; also standard output. |
| Show the developer overlay in game | Displays the existing developer overlay independently of recording. | In the game window. |
| Console hotkey | Selects the key that opens the existing developer console. Default: grave/backquote. | In-game console. Close it to give input back to gameplay. |
| Input logging | Off, device diagnostics, or detailed keyboard/mouse input diagnostics. | Standard output, not the telemetry files. |
| Log frame pacing | Emits frame-clock diagnostics for timing investigations. | Standard output, not the telemetry files. |

These controls are independent. You can record telemetry without showing an overlay or turning
on debug logs. Detailed logging can affect performance, so leave it off when comparing timings
unless the diagnostic requires it. The console remains available with the overlay off; its hotkey
opens and closes it. See the [console guide](CONFIGURATION.md#developer-overlay-and-console).

Scripted input, forced pause, synthetic mouse movement, renderer probes and build-time debug menus
remain advanced documented tools. They are not ordinary play-session switches: some deliberately
replace input or modify execution. See [configuration and diagnostics](CONFIGURATION.md).

## Record a manual telemetry test

1. On **Mission**, choose the mission you want to test.
2. On **Developer Tools**, enable **Record telemetry for this launch**.
3. Select 3,600, 10,800 or 18,000 rendered frames. These are approximately one, three or five
   minutes at 60 FPS; slower rendering or pausing can extend the wall-clock time.
4. Start the mission. Move, open doors, shoot and collect items as appropriate for your test.
   Keyboard and mouse remain enabled for the recording.
5. Let the game close automatically at its frame limit. That is expected: it writes the final
   record immediately before exiting.
6. Reopen the launcher, choose **Developer Tools**, and click **Latest Summary** or **Open Reports**.

The launcher creates a new run ID and filename for each recording. It does not overwrite an
existing report. If a file cannot be created, the game logs an error and telemetry continues on
standard output. There is no automatic upload or sharing.

## Where are my reports?

**Open Reports** opens the per-user directory selected by SDL, typically:

- macOS: `~/Library/Application Support/goldeneyenative/developer-tools/`
- Windows: `%APPDATA%\goldeneyenative\developer-tools\`
- Linux: `$XDG_DATA_HOME/goldeneyenative/developer-tools/`, or
  `~/.local/share/goldeneyenative/developer-tools/` when that variable is unset.

Each run has `run-<id>.jsonl` and a readable `run-<id>.jsonl.txt` companion. **Latest Summary**
opens the most recently modified matching summary in your default text viewer. These files are
outside the repository and contain only the telemetry's counters and bounded identities, not
screenshots, game assets, full runtime logs or user paths. Do not commit the reports.

Debug logging is separate. To capture it, start the game or launcher from a terminal and redirect
standard output and standard error to a local log. Inspect and sanitize the full log before sharing
it: the telemetry file's narrow content contract does not apply to surrounding runtime messages.

## How do I read a summary?

- **Slots occupied / free** describe the actual runtime prop pool at the recorded boundary.
- **Peak occupied** is the highest usage seen in that pool epoch.
- **Allocations / frees since stage ready** show activity during gameplay, excluding initial setup.
- **Allocation failures** indicate that the actual allocator returned no available slot.
- **On-screen count / peak** describe a separate list, not additional prop-pool slots.
- **Accounting / slot tracking / on-screen bounds** are consistency checks. A failed check means
  the report needs investigation; do not use it as valid capacity evidence.

For example, a peak of 403 out of 600 slots with zero allocation failures means that this run
observed that peak and no exhausted allocation. It does not prove a safe mission size or coverage
of every encounter. Compare repeated runs with the same executable, mission, configuration and
workload. Model, texture, audio and other pools are not measured by this telemetry.

The machine-readable contract is in [Prop allocator telemetry](PROP_ALLOCATOR_TELEMETRY.md).

## FAQ

### Why did the game close?

Recording uses an explicit frame limit. The automatic exit writes `run-final`. Turn recording off
for an ordinary play session without a launcher-supplied time limit.

### What if I close the window early or the game crashes?

The files keep the observations already flushed. The summary starts with an incomplete-run notice;
only a `run-final` section followed by **Timed recording complete** confirms the timed endpoint.
Do not treat a partial file as a completed test. Each stage load starts a new pool epoch, so a run
with stage transitions can contain more than two records.

### Why did keyboard and mouse stop working in a terminal-launched test?

`GETV_EXIT_FRAME` normally marks an automated measurement and idles physical input. Add
`GETV_KEYBOARD_IDLE=0` for a manual recording. The launcher's recording option does this for you.
Also close the developer console if it is open; it intentionally owns keyboard and mouse input.

### Can I enable recording without the launcher?

Yes. From a built checkout, choose a new, unused local report filename:

```bash
GETV_PROP_TELEMETRY=1 \
GETV_PROP_TELEMETRY_RUN_ID=my-test-001 \
GETV_PROP_TELEMETRY_FILE=/private/tmp/my-test-001.jsonl \
GETV_KEYBOARD_IDLE=0 \
GETV_STAGE=34 GETV_INTROCAM=0 GETV_EXIT_FRAME=10800 \
./getv/build-mac/goldeneye
```

The file destination is optional; omit it for standard output only. The run ID is required and
must follow the [token rules](PROP_ALLOCATOR_TELEMETRY.md#enable-a-bounded-run). On Linux or
Windows, use your platform's executable and environment-variable syntax.

### Is the top-left overlay the telemetry recorder?

No. The overlay displays interactive developer information. The recorder writes allocator
observations for later inspection. Either can be enabled without the other.
