## Problem

<!-- Link one issue and describe the observed and expected behavior. -->

Fixes #

## Change

<!-- Explain the root cause and the smallest change that fixes it. -->

## Evidence

<!-- Give exact reproduction steps and measurements. Visual bug fixes must embed before/after
screenshots in this body. Upload reviewed runtime screenshots as attachments; never commit them. -->

<!-- Label Before and After (plus Reference for renderer fixes), with descriptive alt text and
captions saying where to look. Use columns or separate images; add matching crops for subtle bugs.
Record the shared scene, input, frame, resolution and quality settings.
Examples: https://github.com/seb-patron/goldeneye-native/pull/1 (columns and crops)
and https://github.com/seb-patron/goldeneye-native/pull/3 (labeled images and captions).
Use uploaded attachment URLs, not local file paths. Verify images render in the published body. -->

<!-- For renderer changes, include OpenGL/reference, old-backend and fixed-backend screenshots.
Generate the quantitative rows with:
python3 tools/compare_render_fingerprints.py --format markdown --reference REF.bmp "Before=OLD.bmp" "Fixed=NEW.bmp"

| Comparison with reference | Worst channel delta | Mean channel delta | Channels over tolerance |
| --- | ---: | ---: | ---: |
| Before | | | |
| Fixed | | | |
-->

## Validation

<!-- List exact commands and results, including build counts and pre-existing failures. -->

- [ ] Focused regression test or check:
- [ ] Bug fixes: regression fails on unchanged base for the reported reason and passes with the fix:
- [ ] Bug fixes: CI runs the required regression cases and fails if their prerequisites are missing, they skip, or none execute (N/A for documentation-only changes):
- [ ] Bug fixes: CI run link, tested revision, command, executed test count and result (or explain pending/unavailable coverage; N/A for documentation-only changes):
- [ ] Complete self-test workflow:
- [ ] Relevant platform build:
- [ ] `git diff --check`:
- [ ] `python3 tools/check_no_game_data.py --changed origin/main`:

## Agent assistance

<!-- State material agent assistance briefly and say what the human reviewed and ran. Do not paste
private reasoning or a full transcript. Write "None" when no agent assisted. -->

- Assistance:
- Human review and verification:

## Upstream replay

<!-- Identify the code-and-test commit only. Keep PATCH_QUEUE bookkeeping separate. -->

- Replayable fix commit:
- Depends on:
- Community-only changes in this PR:

## Submission checks

- [ ] This pull request addresses one logical bug.
- [ ] I checked current `main` and existing issues/pull requests for duplicate work.
- [ ] I reviewed the complete diff and removed unrelated cleanup and generated files.
- [ ] Skill changes: before/after behavioral evals, sanitized result records, comparison report
      and evidence manifest are committed and match the changed skills (N/A if no skills changed).
- [ ] Visual bug fixes: before/after images render in the PR body, with a reference image for
      renderer changes (N/A for nonvisual changes; explain any upload blocker).
- [ ] No ROM is included in any form. I did not add, copy, stage, commit, upload, attach, paste,
      encode, archive or link to one.
- [ ] No save, `base.zip`, extracted asset, generated asset source, texture dump or audio bank is
      included or linked.
- [ ] The implementation follows the provenance rules in `CONTRIBUTING.md` and
      `docs/LICENSING.md`.
