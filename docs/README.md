# Documentation

Choose the path that matches what you are trying to do. On Windows,
[`WINDOWS_INSTALL.md`](WINDOWS_INSTALL.md) records the no-code setup candidate and its current
availability; there is not yet an official downloadable Windows setup package. Contributors should
start with [`BUILDING.md`](BUILDING.md).

## Play and configure

| Guide | Use it for |
|---|---|
| [`GETTING_STARTED.md`](GETTING_STARTED.md) | Install, launch, configure, update, and run a first check on macOS, Linux, or Windows. |
| [`WINDOWS_INSTALL.md`](WINDOWS_INSTALL.md) | Windows no-code setup candidate, current download availability, first run, and safe failure reporting. |
| [`BUILDING.md`](BUILDING.md) | Developer source builds on Windows, macOS, and Linux. |
| [`CONTROLS.md`](CONTROLS.md) | Complete keyboard/mouse map, gamepad defaults, rebinding, control styles, and live shortcuts. |
| [`CONFIGURATION.md`](CONFIGURATION.md) | Full config-file, command-line, environment-gate, and launcher reference. |
| [`SETUP.md`](SETUP.md) | Detailed manual macOS pipeline and deep build troubleshooting. |
| [`WINDOWS_PACKAGING.md`](WINDOWS_PACKAGING.md) | Maintainer guide for producing and testing the ROM-free Windows setup package. |
| [`RELEASING.md`](RELEASING.md) | Review the gates and security requirements for future coordinated, ROM-free player packages. |
| [`RELEASE_1.0.md`](RELEASE_1.0.md) | GoldenEye Native 1.0 release scope and certification statement. |
| [`TESTING_1.0.md`](TESTING_1.0.md) | Human campaign certification record for Agent, Secret Agent and 00 Agent. |
| [`FIXES_1.0.md`](FIXES_1.0.md) | Release-facing summary of the 1.0 stabilization fixes. |
| [`FAQ.md`](FAQ.md) | Common player and project questions. |
| [`CHEATS.md`](CHEATS.md) | GoldenEye's built-in named cheat system. |

## Contribute

| Guide | Use it for |
|---|---|
| [`../CONTRIBUTING.md`](../CONTRIBUTING.md) | Contribution rules, game-data boundary, provenance, review scope, and evidence expectations. |
| [`AGENTIC_CONTRIBUTING.md`](AGENTIC_CONTRIBUTING.md) | Safe agent-assisted bug reports and pull requests. |
| [`CODEBASE.md`](CODEBASE.md) | Architecture, build/runtime/input flows, repository map, and where a change belongs. |
| [`DEVELOPMENT.md`](DEVELOPMENT.md) | Branch, edit, rebuild, test, validate, and review loop. |
| [`../getv/patches/README.md`](../getv/patches/README.md) | Safely record changes to the ignored decompilation. |
| [`HARNESS.md`](HARNESS.md) | Current scripted-input and player-API automation seams. |
| [`TASK_QUEUE.md`](TASK_QUEUE.md) | Historical two-machine bot/Windows task queue and engineering record. |

## Features and subsystems

| Guide | Subject |
|---|---|
| [`MODDING.md`](MODDING.md) | Lua mods, environment gates, asset seams, and HD texture packs. |
| [`MOUSE.md`](MOUSE.md) | Mouse-look design, measurements, and tests. |
| [`FRAME_TIMING.md`](FRAME_TIMING.md) | High-refresh timing improvements, measurements, and remaining frame-counted limitations. |
| [`PERFORMANCE.md`](PERFORMANCE.md) | Performance profiles and measurements. |
| [`BOTS.md`](BOTS.md) | Bot architecture and skill policies. |
| [`COOP.md`](COOP.md) | Cooperative mission behavior and limitations. |
| [`NETPLAY.md`](NETPLAY.md) | LAN implementation and known synchronization failures. |
| [`GIBS.md`](GIBS.md) | Opt-in enemy-gib implementation and policies. |
| [`STANCE.md`](STANCE.md) | Current crouch action plus jump/lean design notes. |
| [`COLOUR_BUGS.md`](COLOUR_BUGS.md) | Texture/colour decoding investigations. |

## Internal interfaces and ports

| Guide | Subject |
|---|---|
| [`PLAYER_API.md`](PLAYER_API.md) | Player state and control API used by bots, netplay, and automation. |
| [`ENEMY_API.md`](ENEMY_API.md) | Port-side live-enemy query API; game-side source adapter is still pending. |
| [`ASSET_LOADING.md`](ASSET_LOADING.md) | Historical early asset-loading investigation; retained for forensic reasoning, not current status. |
| [`asset-converter-spec.md`](asset-converter-spec.md) | Asset converter contract. |
| [`PORTING.md`](PORTING.md) | Historical August 2026 Windows/Linux porting plan and subsequent engineering record. |
| [`WINDOWS_STAN_ORDERING.md`](WINDOWS_STAN_ORDERING.md) | Windows geometry-ordering root cause and fix. |
| [`PROP_ALLOCATOR_TELEMETRY.md`](PROP_ALLOCATOR_TELEMETRY.md) | Versioned, content-free runtime prop-pool observations. |
| [`PERFECT_DARK.md`](PERFECT_DARK.md) | Audited opportunities from the MIT-licensed Perfect Dark port. |
| [`RENDERER_TROUBLESHOOTING.md`](RENDERER_TROUBLESHOOTING.md) | GPU/function flight recording, hardware-batch correlation and the Intel i915/GuC case study. |

## Direction and maintenance

| Guide | Subject |
|---|---|
| [`ROADMAP.md`](ROADMAP.md) | Current state, known issues, and planned work. |
| [`VISION.md`](VISION.md) | Long-term design ideas; older feature-status tables are historical. |
| [`MAINTAINING.md`](MAINTAINING.md) | Community branch workflow and future upstream replay. |
| [`../PATCH_QUEUE.md`](../PATCH_QUEUE.md) | Independently replayable community fixes. |
| [`REUSE_AUDIT.md`](REUSE_AUDIT.md) | Reuse inventory and dated adoption audit; current feature status lives in README/ROADMAP. |

## Licensing and provenance

| Guide | Subject |
|---|---|
| [`LICENSING.md`](LICENSING.md) | Repository-wide provenance and permitted sources. |
| [`THIRD_PARTY.md`](THIRD_PARTY.md) | Fetched dependencies, versions, and redistribution constraints. |
| [`../getv/port/PROVENANCE.md`](../getv/port/PROVENANCE.md) | File-level port-layer origin record. |

If two documents disagree about a command or setting, treat the executable/script help and current
source as authoritative, then fix the stale document. Documentation changes are welcome.

- [Developer Tools guide and FAQ](DEVELOPER_TOOLS.md) — launcher diagnostics, local reports and recording walkthrough.
