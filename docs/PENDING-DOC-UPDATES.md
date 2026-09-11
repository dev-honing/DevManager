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

### 1. End-to-end migrate orchestrator (`Migrate`, `--migrate`)
- Commit: `<fill on merge>` (branch `feat/migrate-orchestrator`)
- Closes the v5 §14.1 "선택 사항" item: bundle→unbundle→restore→health as one
  command instead of four.
- `app/core/migrate.{h,cpp}`: `Migrate::run(bundleFile, outDir, apply)` calls
  `SnapshotBundle::unpack` → `RestorePlanner::compute` (always, read-only) →
  `RestoreExecutor::run` only when `apply` is true → `HealthCheck::run`.
  Dry mode (default) never touches live files.
- CLI: `devmanager-scan --migrate <bundle.tar.gz> [--migrate-out <dir>]
  [--apply]`. Dry run prints the restore plan + health; `--apply` prints the
  restore result + health. Exit reflects the migrate step itself, not
  health (mirrors `--health`'s own separate exit code).
- Verified: `tst_migrate` (dry run leaves live files untouched; apply restores
  and reports health) + real CLI smoke (bundle an existing snapshot →
  `--migrate` dry run → correct plan + full health readout).
- **Docx sections to touch:** §2.3 CLI (add `--migrate`/`--migrate-out`), §8.3
  bundle (note the one-command wrapper), §14.1 잔여 (remove — this closes it),
  §11 tests (add `tst_migrate`, 14 suites), README already updated.

---

## How to use this file
- Append an entry whenever a change lands that the docx should eventually reflect.
- Keep each entry: commit refs, one-paragraph what/why, and the docx sections it
  touches.
- When a vN docx is generated, delete the folded entries and bump the "Last
  docx" / "Next docx" lines above.
