#requires -Version 5.1
<#
  Phase 0 원복 — snapshot-current.ps1 이 만든 백업을 제자리로 되돌린다.
  파괴적: ~/.headroom, ~/.omniroute, ~/.claude, ~/.codex 등을 덮어쓴다.
#>
[CmdletBinding()]
param(
  [Parameter(Mandatory)][string]$From,
  [switch]$Force
)
$ErrorActionPreference = 'Stop'
if (-not (Test-Path $From)) { throw "백업 폴더 없음: $From" }
$manifest = Get-Content (Join-Path $From 'manifest.json') -Raw | ConvertFrom-Json
Write-Host "복원 대상 ($($manifest.stamp)):" -ForegroundColor Yellow
$manifest.copied | ForEach-Object { Write-Host "  $_" }

if (-not $Force) {
  $ans = Read-Host "위 경로를 백업본으로 덮어쓴다. 계속? (yes)"
  if ($ans -ne 'yes') { Write-Host "취소."; return }
}

$map = @{
  '.headroom'      = "$env:USERPROFILE/.headroom"
  '.omniroute'     = "$env:USERPROFILE/.omniroute"
  '.claude'        = "$env:USERPROFILE/.claude"
  '.codex'         = "$env:USERPROFILE/.codex"
  'settings.json'  = "$env:APPDATA/Docker/settings.json"
  '.wslconfig'     = "$env:USERPROFILE/.wslconfig"
}
foreach ($item in Get-ChildItem $From -Exclude 'manifest.json') {
  $target = $map[$item.Name]
  if (-not $target) { Write-Warning "매핑 없음, 건너뜀: $($item.Name)"; continue }
  if (Test-Path $target) { Remove-Item $target -Recurse -Force }
  Copy-Item $item.FullName $target -Recurse -Force
  Write-Host "  복원 $($item.Name) -> $target" -ForegroundColor Green
}
Write-Host "완료. Headroom/OmniRoute/Docker Desktop을 재시작하라."
