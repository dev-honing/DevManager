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

### 2. GUI integration of Health / Bootstrap / Retention (new Settings page)
- Commit: `<fill on merge>` (branch `feat/settings-gui`)
- Replaces the empty "Settings" placeholder with `app/gui/pages/settings_page.{h,cpp}`,
  three sections in one page:
  - **Environment Health** — profile combo (built-in "All tools" + config
    `hostProfiles`), "Run Health Check" → `HealthCheck::run(profile)` off-thread,
    table (group/name/status badge/detail), READY/NOT READY summary.
  - **Bootstrap** — "Show Install Commands" → `Bootstrap::plan()`, read-only
    text box of commands (or "install manually" lines) + "Copy All" to clipboard.
    Never executes anything.
  - **Snapshot Retention** — keep-last/keep-days spin boxes, "Preview" →
    `SnapshotRetention::plan()`, table of keep/prune, "Delete old snapshots"
    gated by a typed "PRUNE" confirmation (same `QInputDialog` pattern as
    Restore's "RESTORE") → `SnapshotRetention::apply()`, refreshes Recent
    Snapshots via a new `snapshotsPruned()` signal.
- Removed `gui/pages/placeholder_page.{h,cpp}` (only caller was the old Settings
  page; `PluginsPage` builds its own empty state inline).
- `gui/main.cpp` dev hook generalized: added `--shot-click "<button text>"`
  (clicks any visible/enabled QPushButton by substring) alongside the existing
  `--shot-dryrun`/`--shot-create`, so any page's buttons are screenshot-testable,
  not just Snapshots'.
- Verified against the real running app (not just fixtures): `--shot-nav
  settings` renders the page; `--shot-dryrun` (Preview reuses the `dryCheckBtn`
  objectName) shows a real prune plan ("10 kept, 2 to prune, 11388 MB") against
  the actual `backups/`; `--shot-click "Run Health Check"` shows a real health
  readout ("NOT READY · 21 ok, 1 warn, 3 fail"); `--shot-click "Show Install"`
  shows the real bootstrap commands (gemini/aider/cursor-agent).
- **Docx sections to touch:** §2.2 GUI (Settings page real content, drop
  "placeholder"), §9 Health & Bootstrap (note GUI access, not just CLI), §8.4
  retention (note GUI access), §11 tests (GUI has no unit suite by convention —
  screenshot-verified like other pages).

---

## How to use this file
- Append an entry whenever a change lands that the docx should eventually reflect.
- Keep each entry: commit refs, one-paragraph what/why, and the docx sections it
  touches.
- When a vN docx is generated, delete the folded entries and bump the "Last
  docx" / "Next docx" lines above.
