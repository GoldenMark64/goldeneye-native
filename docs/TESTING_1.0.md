# GoldenEye Native 1.0 — Human Validation Record

> **Status: INCOMPLETE**
>
> This file becomes the human-testing record for v1.0.0. Do not mark the release certified until
> every required row below is complete using the final release candidate.

## Release candidate identity

| Item | Value |
|---|---|
| Release tag | `v1.0.0` (planned) |
| Release commit | **TBD** |
| Build date | **TBD** |
| Primary test platform | **TBD** |
| OS | **TBD** |
| CPU | **TBD** |
| GPU | **TBD** |
| Mesa / graphics driver | **TBD** |
| Renderer | **TBD** |
| ROM revision | US retail |
| Tester | Human playtest |

## Campaign certification

Every retail mission must be completed from the final release candidate on each retail difficulty.

| Difficulty | Full campaign completed | Date | Notes |
|---|---:|---|---|
| Agent | ☐ | | |
| Secret Agent | ☐ | | |
| 00 Agent | ☐ | | |

## Required interpretation

A checked row means the retail campaign was completed by a human player using the final release
candidate. It does not mean every cheat, mod, multiplayer configuration, controller, graphics
option, or hardware combination was exhaustively tested.

## Focused regression areas

Before certification, confirm the final candidate against the repair areas most likely to regress:

- collision and traversal, including ladders and tank interaction;
- mission completion and post-mission input;
- NPC animation and weapon state;
- gunbarrel timing and transition behavior;
- previously affected mission-specific geometry;
- reload and scripted character behavior;
- renderer stability in known stress locations;
- the Intel GPU hang reproduction path used during the final renderer investigation.

## Intermittent / special validation notes

Record any final checks for previously intermittent observations here. Historical investigation has
included areas such as Surface 2 remote mines, delayed Bunker audio, Control thrown grenades, and an
intermittent Silo post-completion stall. These should be described accurately based on the final
candidate rather than treated as automatically certified.

## Certification statement

Only after all three campaign rows are complete and the release gates pass, change the status at
the top of this file to **CERTIFIED** and record the exact release commit.

Suggested final wording:

> The v1.0.0 release candidate was completed through human playtesting across all retail missions on
> Agent, Secret Agent, and 00 Agent. Focused automated and diagnostic regression testing was also
> performed for the major defects repaired in this fork.
