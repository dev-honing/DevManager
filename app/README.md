# DevManager (C++/Qt6)

Snapshot/Restore core, being ported from the PowerShell reference in
`../scripts/` per `../docs/DevManager-Design-v2.docx`.

## Status — Phase 1: read-only environment scan (+ GUI shell)

`devmanager-scan` walks the local AI dev environment and writes one inventory
JSON. It **writes nothing else** — no config dirs, no service control, no git
mutation. It is the C++ counterpart of steps 1–3 of `snapshot-current.ps1`.

`devmanager-gui` is the Qt Widgets shell over that scan: a tabbed dashboard
(Environment / Skills / Plugins / Packages / Env Vars) that runs the scan on a
background thread via `AppController` — the seam the later Snapshot/Restore
managers plug into (design doc s4.3). Read-only, same as the CLI.

Ported so far: `ToolScanner`, `SkillScanner` (link + git + classification,
merge-by-name), `PluginScanner`, `PackageScanner`, `WslScanner`,
`EnvironmentVariableScanner` (secret masking). No `ServiceInspector` yet
(Phase 5), so the inventory omits the `services` block.

## Build

Requires: Visual Studio 2026 (or Build Tools), CMake ≥ 3.24, Qt 6.10 msvc2022_64.

```
cd app
cmake --preset windows-v143
cmake --build build --config Debug
```

`CMakePresets.json` points `CMAKE_PREFIX_PATH` at `C:/Qt/6.10.3/msvc2022_64`.
Override per machine with a git-ignored `CMakeUserPresets.json`.

> Toolset note: the design doc pins MSVC v143 for ABI parity with the Qt
> prebuilts. This box's VS2026 only registers the v145 toolset targets, and
> Qt 6.10 msvc2022_64 links cleanly against v145 (MSVC 14.x runtime ABI is
> stable across VS2015–2026). Re-pin `"toolset": "v143"` in the preset once
> the "MSVC v143 - VS 2022" component is installed, if a real mismatch ever
> surfaces.

## Run

```
./build/Debug/devmanager-scan.exe --root <project-root> --out inventory.cpp.json
./build/Debug/devmanager-gui.exe
```

Add `C:/Qt/6.10.3/msvc2022_64/bin` to `PATH` so the Qt DLLs resolve.

## Package (self-contained folder)

```
deploy.bat
```

Builds Release, then `cmake --install` stages `..\dist\DevManager\`:
`devmanager-gui.exe` + `devmanager-scan.exe`, the Qt runtime bundled by
`windeployqt`, and `config\*.json`. Copy that folder to a new PC and run it
directly — no Qt install required. (If `windeployqt` isn't on `PATH` the
install still succeeds but warns; add `C:/Qt/6.10.3/msvc2022_64/bin`.)

## Regression check vs the PowerShell reference

```
python ../scripts/diff-inventory.py <powershell inventory.json> inventory.cpp.json
```

Semantic compare (ignores key order, timestamps, schemaVersion). For a clean
baseline, capture both in the **same terminal, back to back** — otherwise
environment drift (packages added, links changed) and Claude-Code-injected
env vars show up as false differences.

## Test

```
ctest --test-dir build -C Debug --output-on-failure
```
