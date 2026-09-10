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
- Commit: 269d9da (merge of `feat/bootstrap`)
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

### 6. P4 잔여 — Service lifecycle real-environment verification
- Commits: 846b547, merge 503edf2
- New CLI surface: `devmanager-scan --service <id> --service-op status|stop|start|restart`
  (`ServiceLifecycle` had only a GUI + coordinator entry point before).
- Code fix: `LifecycleResult.error` now surfaces the process **stderr** on
  failure — previously it showed only `exit N` and the real reason was lost.
  Test: `tst_service_lifecycle::surfacesStderrReasonOnFailure`.
- **Real-env results (2026-09-11):**
  - `status` — real Headroom + real OmniRoute → exit 0, correct output. ✅
  - OmniRoute `start` → `stop` — full cycle verified: `omniroute serve`
    (detached) bound port 20128; `omniroute stop` killed the server; port
    closed; environment restored to pre-test (stopped). ✅
  - Headroom `stop` / `start` / `restart` — **fail, and it is not a DevManager
    bug**: this machine's `init-user` profile uses `persistent-task` (Windows
    Scheduled Task) scheduling, and Headroom itself rejects
    `install stop|start|restart` for task deployments ("not supported for task
    deployments"). Every failed command was a clean no-op — Headroom stayed
    running, port 8787 open, state unchanged.
  - Config finding recorded in `config/scan.json` (`_comment` on the headroom
    service): for a task deployment, point stop/start/restart at
    `schtasks /end|/run /tn headroom-<profile>-startup`. Left as-is because the
    right command is deployment-specific and the lifecycle block is
    user-editable.
- **Docx sections to touch:** §2.3 CLI (add `--service` / `--service-op`),
  §6 Service Lifecycle (add: real-env — status both services ✅, OmniRoute
  start/stop cycle ✅, Headroom task-deployment caveat; error now carries
  stderr), §11 tests (lifecycle suite 5→6 cases), §12.1 roadmap (P4 → done:
  mechanics + status + one full start/stop cycle verified; Headroom
  task-deployment control is a config choice, not a code gap), §14 주의사항
  (Headroom persistent-task caveat).

### 7. P11 — Docker track (images + per-project containers + devcontainer + host profiles)
- Commits: `434c40b` (P11.1), `5eac40c` (P11.2/11.3), `11319d5` (P11.4),
  merge `<fill on merge>` (branch `feat/docker-track`)
- **P11.1 layered images** — `docker/base|cpp|nextjs/Dockerfile`:
  `ai-dev-base:0.1` (`node:22-slim` + Claude Code + Codex + git + ripgrep +
  tini, unprivileged `node` user, `/workspace`); `ai-dev-cpp:0.1` (+ gcc/cmake/
  gdb/ninja); `ai-dev-next:0.1` (+ corepack). `docker/build.{sh,bat}`,
  `docker/README.md`. All three build; tools verified inside each.
- **P11.2/11.3 per-project setup** — `app/core/project/project_env.{h,cpp}`:
  `ProjectEnv::resolve(name, type)` against `config/project-types.json`;
  `composeYaml()` (project bind-mount `..:/workspace` + named volumes
  `claude`/`codex`/`deps`, `host.docker.internal` wired); `devcontainerJson()`.
  `json::writeText` added. CLI: `devmanager-scan --project-init <name>
  --project-type <t>` writes `.devmanager/docker-compose.yml` +
  `.devcontainer/devcontainer.json`; `--project-up` / `--project-down` run
  `docker compose`. Verified end to end: init → up (network + 3 volumes +
  container) → `exec cc main.c` inside `ai-dev-cpp` → down.
- **P11.4 host profiles** — `ScanConfig.hostProfiles` (name → PATH-checkable
  tool ids); `HealthCheck::run(profile)` restricts the tool section to that
  list. `devmanager-scan --health --profile msvc-qt6`. `docs/host-profiles.md`.
  `project-types.json` `windows-cpp` → `hostProfile msvc-qt6` (docker:false).
- Tests: `tst_project_env` (3) new; `tst_health_check` +1 (profile filter);
  `tst_service_lifecycle` unchanged. **13 suites total.**
- **Docx sections to touch:** this is the big one — §11 (was "P11 보류 / 완전
  미착수") flips to **done**. Add:
  - §2.1 module table: `project/project_env`
  - §2.3 CLI: `--service`/`--service-op` (from #6), `--project-init/-up/-down`,
    `--health --profile`
  - §3 config: `hostProfiles`, and `tools[].install` (from #5)
  - new §: "Container track" — the three images, per-project compose + volumes,
    devcontainer, `host.docker.internal` + `HEADROOM_HOST=0.0.0.0` note
  - §9 배포 / §host-profiles: native `msvc-qt6` profile (DevManager itself is
    the reference impl)
  - §12.1 roadmap: **P11 → done** (images, per-project containers, devcontainer,
    hostProfile all built + verified). §12.2: P11 row removed from 잔여.
  - §13 v2 대비 미착수: the Docker/Project-Creator/Volume/VS-Code/hostProfile
    rows all move from "미착수" to "done (P11)".
  - §16 summary: DevManager now covers both the migration track AND the
    container-isolation track.

---

## How to use this file
- Append an entry whenever a change lands that the docx should eventually reflect.
- Keep each entry: commit refs, one-paragraph what/why, and the docx sections it
  touches.
- When a vN docx is generated, delete the folded entries and bump the "Last
  docx" / "Next docx" lines above.
