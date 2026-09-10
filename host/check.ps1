#requires -Version 5.1
<#
  Phase 1 — Host 진단기.  단일 JSON을 stdout으로 출력한다. (사람이 읽는 로그는 stderr)
  Qt 앱은 이 스크립트를 QProcess로 실행하고 JSON만 파싱한다.

  출력 계약 (이 형태를 유지):
  {
    "ok": bool,                 // 모든 필수 항목 통과 여부
    "checkedAt": "ISO8601",
    "checks": {
      "<name>": { "ok": bool, "detail": "...", "value": <any> }
    }
  }

  규칙:
  - API Key / 토큰은 값 그대로 넣지 않는다 (Mask 사용).
  - /stats 전체를 넣지 않는다. status/version/mode 정도만.
  - 예외가 나도 크래시하지 말고 해당 check.ok=false 로 기록.
#>
[CmdletBinding()]
param()

function Mask([string]$s) {
  if ([string]::IsNullOrEmpty($s)) { return $null }
  if ($s.Length -le 8) { return '***' }
  $s.Substring(0,4) + '...' + $s.Substring($s.Length-4)
}
function Test-Port([int]$p) {
  try { (Test-NetConnection 127.0.0.1 -Port $p -WarningAction SilentlyContinue).TcpTestSucceeded } catch { $false }
}

$checks = [ordered]@{}

# --- 예시 1: Headroom health ------------------------------------------------
try {
  $h = Invoke-RestMethod 'http://127.0.0.1:8787/health' -TimeoutSec 3
  $checks.headroom = @{
    ok     = ($h.status -eq 'healthy')
    detail = "v$($h.version) / ready=$($h.ready)"
    value  = @{ version = $h.version; status = $h.status }
  }
} catch {
  $checks.headroom = @{ ok = $false; detail = "unreachable: $($_.Exception.Message)"; value = $null }
}

# --- 예시 2: OmniRoute 포트 ----------------------------------------------------
$checks.omniroutePort = @{
  ok     = (Test-Port 20128)
  detail = 'tcp 127.0.0.1:20128'
  value  = $null
}

# --- TODO (직접 채우기) -----------------------------------------------------
#  docker            : docker version --format '{{.Server.Version}}'  (데몬 안 뜨면 ok=false)
#  wsl               : wsl.exe -l -v  파싱, VERSION=2 인 distro 존재?
#  omnirouteModels   : GET http://127.0.0.1:20128/v1/models  → config/required-models.json 대조
#  headroomStats     : GET /stats?cached=1  → summary.mode 만 추출
#  node / cmake / qt : 버전 존재 + config/versions.json 기대값과 비교
#  env               : ANTHROPIC_BASE_URL 등 존재 여부 (KEY/TOKEN 은 Mask)
#  vscode            : code --version

$result = [ordered]@{
  ok        = -not ($checks.Values | Where-Object { -not $_.ok })
  checkedAt = (Get-Date).ToString('o')
  checks    = $checks
}
$result | ConvertTo-Json -Depth 8
