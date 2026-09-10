# DevManager

AI 개발환경 관리자 — Windows Host + Docker 기반으로 Claude Code / Codex 개발환경을
여러 PC·여러 프로젝트에서 등가 재현하기 위한 오케스트레이터.

- 설계 문서: [`docs/DevManager-Design-v2.docx`](docs/DevManager-Design-v2.docx)
- 목표: byte 단위 클론이 아니라 **동일 도구 버전 + 설정으로 재현되는 등가 환경**

## 구성

| 계층 | 역할 |
|---|---|
| `host/` | PowerShell — Windows 설치/검증/복구 |
| `docker/` | `ai-dev-base` → `ai-dev-cpp` / `ai-dev-next` 이미지 |
| `app/` | Qt6 GUI (C++17, MSVC v143 툴셋 고정) |
| `config/` | 버전·유형·포트 정책 (JSON) |
| `scripts/` | Phase 0 인벤토리/백업, 프로젝트 생성/백업 |

## ⚠️ 시작 전 (Phase 0)

이 머신의 세팅 과정은 기록돼 있지 않다. **아무 Bootstrap도 돌리기 전에** 현재 상태를
백업한다:

```powershell
pwsh scripts/snapshot-current.ps1
```

되돌리려면:

```powershell
pwsh scripts/restore-current.ps1 -From backups/<날짜>
```

`backups/`, `inventory.json`, `config/versions.json`은 `.gitignore` 대상 — 커밋 금지.

## 로드맵

- [x] Phase 0 — 인벤토리 / 원복 백업 스크립트
- [ ] Phase 1 — `host/check.ps1` 진단기 (JSON 출력)
- [ ] Phase 2 — Docker base/cpp/next 이미지
- [ ] Phase 3 — Qt6 Dashboard
- [ ] Phase 4~8 — Project Creator / Session 격리 / Migration / Image Upgrade / 패키징

## 라이선스

MIT (`LICENSE`). Qt는 LGPLv3 — 배포 시 동적 링크 유지.
