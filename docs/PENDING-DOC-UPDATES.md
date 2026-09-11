# Pending design-doc updates

Running log of changes that have landed on `main` but are **not yet folded into
the design doc** (`DevManager-Design-v*.docx`). When a new docx revision is
generated, move the covered entries into that revision and clear them here.

- **Last docx:** `DevManager-Design-v5.docx` — P0–P11 complete; folds in
  everything through commit `c67147f` (health check, sha256Of relative-path
  fix, retention, bootstrap, P4 `--service` + real-env, P11 Docker track, and
  the full P5 real-environment restore verification of all six backup roots).
  v5 also carries a deep §15 "특이사항 / 구현 중 발견".
- **Next docx:** v6 — fold in whatever lands below.

---

## Pending since v5

_(nothing yet)_

---

## How to use this file
- Append an entry whenever a change lands that the docx should eventually reflect.
- Keep each entry: commit refs, one-paragraph what/why, and the docx sections it
  touches.
- When a vN docx is generated, delete the folded entries and bump the "Last
  docx" / "Next docx" lines above.
