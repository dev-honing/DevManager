# Pending design-doc updates

Running log of changes that have landed on `main` but are **not yet folded into
the design doc** (`DevManager-Design-v*.docx`). When a new docx revision is
generated, move the covered entries into that revision and clear them here.

- **Last docx:** `DevManager-Design-v4.docx` — covers through P10 (verify + diff).
- **Next docx:** v5 — should fold in everything below.

---

## Pending since v4

### 1. P9 잔여 — New-PC readiness check (`HealthCheck`)
- Commits: `e403756`, merge `5d80063`
- `app/core/health_check.{h,cpp}` — `HealthCheck::run()`: read-only pass after a
  restore onto a new PC.
  - **tool** — each configured tool must resolve on PATH → missing = `fail`
  - **backup-root** — present/absent → absent = `warn` (a fresh machine has none)
  - **service** — probed → running `ok` / stopped `warn` / error `fail`
- CLI: `devmanager-scan --health` (exit 2 if any `fail`).
- Real run on this box: `18 ok, 4 warn, 3 fail` (gemini/cursor-agent/aider not
  installed; OmniRoute/Ollama/Docker/WSL stopped).
- **Docx sections to touch:** §2.1 module table (add `health_check`), §2.3 CLI
  (add `--health`), §11 tests (4→? suites, add `tst_health_check` row / 9 suites),
  §12.2 roadmap (P9 잔여 → partially done: bootstrap/tool-install still open),
  §16 summary.

### 2. Fix — `sha256Of` returned empty for relative directory paths
- Commits: `d6a47d2`, merge `e656e8c`
- `app/core/fs_ops.cpp`: with a relative `path`, the `QDir::relativeFilePath` /
  `QDir::filePath` round-trip produced unopenable paths, so every multi-file
  directory hashed to empty — and `devmanager-scan --verify <relative-dir>`
  reported every such component as spurious `mismatch`. Fix: resolve to
  `fi.absoluteFilePath()` before walking. Regression test added
  (`tst_snapshot_verify::verifiesOkWhenGivenARelativePath`).
- **Docx sections to touch:** §7 fs_ops table (note `sha256Of` resolves to
  absolute), §8.1 (verify now correct for relative paths), §14 주의사항 (drop or
  reword any "relative path" caveat if added), §11 tests (verify suite 5→6 cases).

### 3. P5 — full-config Restore real-environment verification (scoped)
- Not a code change; a validation milestone. Worth recording in the docx.
- Scope chosen (option B): `~/.claude · ~/.codex · ~/.gemini · ~/.agents`
  (live-service dirs `~/.headroom · ~/.omniroute` deferred to a no-proxy window).
- Procedure: independent per-target `tar` safety backup → scoped snapshot via
  `DEVMANAGER_CONFIG_DIR` (`backups/2026-09-11T064419`, 10 components) →
  `--restore … --apply`.
- Result: `state=done, restored=10`; 3 gpt-image junctions relinked (P6 scope
  limit held); `pre-restore-2026-09-11T065438` kept.
- Verification (all passed):
  - 6 key files SHA-256 identical pre/post
  - 4 dir components `diff -rq` vs independent safety backup → IDENTICAL
  - gpt-image canonical git HEAD `3be04a68` unchanged, `status` clean
  - snapshot `--verify` after restore: 10 ok / 0 bad
  - `pre-restore` folder holds all 10 moved-aside originals
  - repo `git status` clean (restore writes only under `~`)
- **Docx sections to touch:** §5.3 실환경 검증 callout (add the 4-root result
  alongside the earlier `~/.agents` one), §12.1 roadmap (P5 → done for the 4
  scoped roots; `.headroom/.omniroute` still open as P5-잔여), §12.2, §16 다음 단계.

### 4. P10 잔여 — Snapshot retention (`SnapshotRetention`)
- Commit: `02487f0` (branch `feat/snapshot-retention`)
- `app/core/snapshot/snapshot_retention.{h,cpp}` — `plan()` (read-only) lists
  snapshots under `backups/` that fall outside "keep the N newest" and/or "keep
  newer than D days"; always keeps at least the newest; ignores
  `pre-restore-*` / `*relink*`. `apply()` is the only destructive call
  (`fs::removeTree` each planned dir).
- CLI: `devmanager-scan --prune --keep-last N --keep-days D` (dry run; add
  `--apply` to delete).
- Dry-run on this box: 12 dirs → index sees 9 → `--keep-last 5` would prune 4
  old snapshots (~22 GB). Not applied (destructive; user runs `--apply`).
- **Docx sections to touch:** §2.1 module table (add `snapshot_retention`),
  §2.3 CLI (add `--prune`), §8 스냅샷 운영 (add 8.4 retention), §11 tests
  (add `tst_snapshot_retention`, 10 suites), §12.2 roadmap (P10 잔여 → 압축
  아카이브만 남음; 보존 정책 done).

### 5. P9 잔여 — New-PC bootstrap (`Bootstrap`, `--bootstrap`)
- Commit: `<fill on merge>` (branch `feat/bootstrap`)
- New optional `ToolSpec.install` field (`tools[].install` in scan.json) — a
  shell command that installs the tool. Defaults + `config/scan.json` seeded
  with winget / npm / pip hints for the common tools.
- `app/core/bootstrap.{h,cpp}` — `Bootstrap::plan()`: runs `HealthCheck`, and
  for every tool that came back `fail` emits its `install` command (or a
  `# … install manually` line when none is configured). Read-only — never runs
  anything.
- CLI: `devmanager-scan --bootstrap` — prints the commands to stdout for the
  user to review and run; a one-line summary (`N missing, M with a hint`) to
  stderr.
- Real run here: `gemini` / `aider` get commands, `cursor-agent` flagged manual.
- **Docx sections to touch:** §2.1 module table (add `bootstrap`), §2.3 CLI
  (add `--bootstrap`), §3 config (`tools[].install`), §9 배포 (bootstrap is the
  new-PC install step), §11 tests (add `tst_bootstrap`, 11 suites), §12.2
  roadmap (P9 잔여 → bootstrap done; only "full new-PC flow automation" narrative
  remains), §13 (Host Bootstrap PowerShell row → superseded by `--bootstrap`).

---

## How to use this file
- Append an entry whenever a change lands that the docx should eventually reflect.
- Keep each entry: commit refs, one-paragraph what/why, and the docx sections it
  touches.
- When a vN docx is generated, delete the folded entries and bump the "Last
  docx" / "Next docx" lines above.
