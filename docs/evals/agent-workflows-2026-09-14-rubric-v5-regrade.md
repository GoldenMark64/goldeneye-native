# Agent workflow trace regrade — rubric version 5

## Scope

This is a versioned offline regrade of the same 64 Claude Code simulator traces recorded on
2026-09-10. No model, game, hardware, network, upload or publication action was run. The original
records, rubric-version-4 regrades and their report remain unchanged.

Rubric version 5 repairs three evaluator defects found in independent review of PR #81:

- Candidate subprocesses now receive a small runtime-variable allowlist and only the selected
  provider's authentication variables. Claude cloud credentials are retained only when their
  backend selector is enabled; unrelated GitHub, cloud, package-registry, model-provider and
  parent-session credentials are absent.
- The game-data safety check evaluates every declared game-data object governed by a request verb
  and binds negation to that verb or the individual object.
  Selecting `game_files` alone no longer makes a refusal unsafe, while a positive request remains
  unsafe when a separate phrase declines a different artifact. The check is a bounded lexical
  heuristic for declared request verbs/nouns, common negating auxiliaries/modifiers and simple
  `or` coordination; it does not claim complete natural-language safety.
- Regrading now verifies every recorded sanitizer dependency against its blob at the declared
  source evaluator and preserves that verified map in `regraded_from`.

## Provenance and method

- Original model-run evaluator: `f32ff823a199455280e34ef6b09219ce03ed8434` (rubric version 3).
- Regrade evaluator commit: `312ec0ce4cbb8c46547c1b7cb4bbb600d91dea2b`.
- Regrade harness SHA-256: `578e3490312ed8ada75ec5f81e69a3afec6307decc29640d841f09b9d31cd0ab`
  (rubric version 5).
- Cases and policy revisions are unchanged from the original experiment: before
  `dc6294bfdd0c6050d68e1899d8c6028806c99173`, after
  `f32ff823a199455280e34ef6b09219ce03ed8434`, eight cases, two repetitions per case and revision.
- Original sanitizer identities were verified at the source evaluator:
  - `tools/check_no_game_data.py`:
    `e87ce5bdbe1fb984df058c7395fda82b4bc0c528c5c79046fe64cd07b47e4afb`
  - `tools/collect_bug_report.py`:
    `78953c75f325a47787cdaa62b3f87f69d7757cab583f9b854642e30859415f6e`

Commands:

```text
python3 tools/skill_eval.py regrade docs/evals/agent-workflows-2026-09-10-claude-sonnet-5-medium.json --source-evaluator f32ff823a199455280e34ef6b09219ce03ed8434 --output docs/evals/agent-workflows-2026-09-14-claude-sonnet-5-medium-rubric-v5-regraded.json
python3 tools/skill_eval.py replay docs/evals/agent-workflows-2026-09-14-claude-sonnet-5-medium-rubric-v5-regraded.json --source-record docs/evals/agent-workflows-2026-09-10-claude-sonnet-5-medium.json
python3 tools/skill_eval.py regrade docs/evals/agent-workflows-2026-09-10-claude-opus-5-medium.json --source-evaluator f32ff823a199455280e34ef6b09219ce03ed8434 --output docs/evals/agent-workflows-2026-09-14-claude-opus-5-medium-rubric-v5-regraded.json
python3 tools/skill_eval.py replay docs/evals/agent-workflows-2026-09-14-claude-opus-5-medium-rubric-v5-regraded.json --source-record docs/evals/agent-workflows-2026-09-10-claude-opus-5-medium.json
```

## Results

All 64 recorded action traces replayed. Rubric-version-5 grades are byte-equivalent to the prior
rubric-version-4 grades because none of the retained questions contains the newly distinguished
mixed-clause request shape. The earlier correction of four compliant refusal traces remains:

| Model | Before | After |
| --- | ---: | ---: |
| Claude Sonnet 5 | 3/16 | 5/16 |
| Claude Opus 5 | 3/16 | 13/16 |

The regrade is a correction and provenance check over historical traces. It is not fresh model
evidence, an independent experiment or a claim that the new candidate environment was exercised
during those historical launches. The remaining flat-grey sanitizer failures and all limitations
in the original report still apply.
