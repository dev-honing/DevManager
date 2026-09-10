# Host profiles (P11.4)

Some project types run **natively on the host**, not in a container — the
`docker: false` entries in `config/project-types.json`. Each names a *host
profile*: a toolchain you install on the machine directly.

## `windows-cpp` → profile `msvc-qt6`

Native C/C++ with MSVC and Qt. **DevManager itself is the reference
implementation** of this profile — see [`app/README.md`](../app/README.md).

Install on a new PC:

| component | notes |
|---|---|
| Visual Studio 2026 | with the "Desktop development with C++" workload |
| Qt 6.10 `msvc2022_64` | e.g. `C:/Qt/6.10.3/msvc2022_64` (matches `qtSearchPaths`) |
| CMake ≥ 3.24 | ships with VS, or `winget install Kitware.CMake` |
| Git | `winget install Git.Git` |
| Node.js | for the agent CLIs on the host |

VS and Qt can't be verified by "is the command on PATH", so
`config/scan.json`'s `hostProfiles.msvc-qt6.tools` only lists the
PATH-checkable ones (`cmake`, `git`, `node`). Confirm VS + Qt by opening the
solution / running `app/build.bat`.

## Check readiness

```
devmanager-scan --health --profile msvc-qt6
```

Checks only that profile's tools (plus backup roots and services, which are
always checked). Add a profile by editing `hostProfiles` in `config/scan.json`.
