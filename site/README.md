# Project site source

Plain static HTML/CSS, no build step, no framework, no external requests (system fonts only,
every image local). This is an optional public-facing landing site. It does not replace the
repository README or `docs/`; those are the authoritative sources for installation, feature
status, certification scope, licensing and contributor guidance.

The old tracked `wiki/` mirror was removed because it had become stale and duplicated the
maintained documentation. Do not add site links that depend on that deleted mirror or on the
former `seb-patron/goldeneye-native` repository.

## Preview locally

```bash
cd site
python3 -m http.server 8080
```

Then open `http://localhost:8080`.

## Publishing

The Pages workflow publishes this directory only when GitHub Pages is deliberately enabled for the
repository. Before enabling or re-enabling publication, compare the site against current
`README.md`, `docs/FAQ.md`, `docs/ROADMAP.md`, `docs/TESTING_1.0.md` and
`docs/LICENSING.md`.

The site must never become a second independent status database. If a feature or platform claim
cannot be supported by those maintained documents, link to the maintained document instead of
inventing a stronger site-specific claim.

## Content accuracy

Use these rules when editing the site:

- `README.md` and `docs/FAQ.md` define current player-facing status.
- `docs/TESTING_1.0.md` defines what the 1.0 campaign certification actually covered.
- `docs/ROADMAP.md` carries current known limitations and planned work.
- `docs/VISION.md`, `docs/PORTING.md`, `docs/ASSET_LOADING.md` and other dated engineering
  records may contain intentionally preserved historical status; do not promote their old
  DONE/PARTIAL/OPEN or bring-up statements into current site copy.
- Co-op and network-play claims must preserve their inherited/pre-takeover provenance and must not
  be presented as part of the GoldenMark64 1.0 campaign certification.
- Windows has a tested no-code setup **candidate**, not an official downloadable setup package,
  until the repository's Releases page says otherwise.
- High-refresh timing is improved and measured; do not claim every timing-sensitive subsystem is
  completely fixed.

No recreated GoldenEye/007 trademark art (wordmark, gun logo) belongs anywhere in this folder,
matching the decision in `docs/LICENSING.md`. The gold ring mark
(`assets/icon/goldeneye-plus-transparent.png`, copied here as `assets/images/mark.png`) is this
project's own packaging icon.
