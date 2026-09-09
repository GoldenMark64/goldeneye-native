# Committed skill-evidence requirement

This follow-up adds a mandatory committed-evidence rule and CI gate to PR #65. Every change under
either repository skill directory requires new committed before/after results, report and
manifest. Missing, stale, incomplete or uncommitted evidence fails the check. Scores need not
improve; failures and ties remain reviewable evidence.

Validation at `ecaa3f2`: 16 evidence-gate tests passed, zero failures or skips. They exercise real
temporary Git histories, missing/stale evidence, scenario coverage, immutable historical reports,
Claude entrypoints, committed-content matching, replay failures and missing snapshot retrieval.
The gate also replayed all 36 committed comparison traces and their original/corrected lineage,
then verified both changed shared skill files against the PR baseline and submitted contents.
These checks make no model calls. `git diff --check` and the changed-file game-data guard passed.

Clarification of the [earlier comparison report](screenshot-policy-2026-09-09.md): its tested
shared skill files remain unchanged from `60a6c47`, but this follow-up changes `AGENTS.md`,
`CONTRIBUTING.md` and the PR template to require evidence. The full policy snapshot therefore no
longer matches the statement in that historical report that it remains unchanged in later PR
commits. The recorded 18/18 versus 18/18 result applies to the recorded snapshots, not a new model
evaluation of this enforcement rule. The CI gate deliberately binds the changed skill contents;
it does not claim that later policy-document edits have been behaviorally evaluated.

The initial behavioral runner covers the two shared `SKILL.md` files. Other skill paths must
gain matching snapshot/context/scenario support before their changes can pass the gate. The
exact-head CI job may fetch missing evaluated commit IDs from `origin` after squash merges.
No branch-protection settings or automatic model runs are introduced.
