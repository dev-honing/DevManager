#requires -Version 5.1
<#
  Phase 0 — 현재 개발환경 인벤토리 + 원복 백업.
  읽기/복사만 한다. 아무것도 변경하지 않는다.
  출력:
    backups/<timestamp>/  ... 설정 디렉터리 사본 + manifest.json
    inventory.json        ... 설치된 도구 버전 스냅샷 (repo 루트, gitignore됨)
#>
[CmdletBinding()]
param(
  [string]$Root = (Split-Path $PSScriptRoot -Parent)
)
$ErrorActionPreference = 'Stop'
$stamp   = Get-Date -Format 'yyyy-MM-ddTHHmmss'
$dest    = Join-Path $Root "backups/$stamp"
New-Item -ItemType Directory -Force -Path $dest | Out-Null

function Get-Ver([string]$exe, [string[]]$args) {
  try { (& $exe @args 2>&1 | Select-Object -First 1) -join ' ' } catch { $null }
}
function Test-Port([int]$p) {
  try { (Test-NetConnection -ComputerName 127.0.0.1 -Port $p -WarningAction SilentlyContinue).TcpTestSucceeded } catch { $false }
}
function Get-Http([string]$url) {
  try { Invoke-RestMethod -Uri $url -TimeoutSec 3 } catch { $null }
}
function Mask([string]$s) {
  if ([string]::IsNullOrEmpty($s)) { return $s }
  if ($s.Length -le 8) { return '***' }
  return $s.Substring(0,4) + '...' + $s.Substring($s.Length-4)
}

Write-Host "[1/3] 도구 버전 수집..."
$inv = [ordered]@{
  capturedAt = (Get-Date).ToString('o')
  machine    = $env:COMPUTERNAME
  os         = (Get-CimInstance Win32_OperatingSystem).Caption + ' ' + [Environment]::OSVersion.Version
  tools = [ordered]@{
    docker    = Get-Ver docker  @('--version')
    wsl       = (wsl.exe -l -v 2>&1 | Out-String).Trim()
    node      = Get-Ver node    @('--version')
    npm       = Get-Ver npm     @('--version')
    python    = Get-Ver python  @('--version')
    cmake     = Get-Ver cmake   @('--version')
    git       = Get-Ver git     @('--version')
    claude    = Get-Ver claude  @('--version')
    codex     = Get-Ver codex   @('--version')
    headroom  = Get-Ver headroom @('--version')
    omniroute = (npm ls -g omniroute --depth=0 2>&1 | Select-String 'omniroute@').ToString().Trim()
  }
  npmGlobal = (npm ls -g --depth=0 2>&1 | Out-String).Trim()
  qt        = (Get-ChildItem 'C:/Qt' -Directory -ErrorAction SilentlyContinue | Where-Object Name -match '^\d+\.\d+' | ForEach-Object Name) -join ', '
  vs        = (& "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe" -all -property catalog_productDisplayVersion 2>$null) -join ', '
  services = [ordered]@{
    headroom = [ordered]@{ port = 8787;  listening = (Test-Port 8787);  health = (Get-Http 'http://127.0.0.1:8787/health') }
    omniroute= [ordered]@{ port = 20128; listening = (Test-Port 20128) }
  }
  env = @{}
}
Get-ChildItem Env: | Where-Object Name -match 'ANTHROPIC|CLAUDE|OMNIROUTE|HEADROOM|DOCKER|QT|CMAKE' | ForEach-Object {
  $v = $_.Value
  if ($_.Name -match 'KEY|TOKEN|SECRET') { $v = Mask $v }
  $inv.env[$_.Name] = $v
}
$inv | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $Root 'inventory.json') -Encoding utf8
Write-Host "  -> inventory.json"

Write-Host "[2/3] 설정 디렉터리 백업..."
$targets = @(
  "$env:USERPROFILE/.headroom",
  "$env:USERPROFILE/.omniroute",
  "$env:USERPROFILE/.claude",
  "$env:USERPROFILE/.codex",
  "$env:APPDATA/Docker/settings.json",
  "$env:USERPROFILE/.wslconfig"
)
$copied = @()
foreach ($t in $targets) {
  if (Test-Path $t) {
    $leaf = Split-Path $t -Leaf
    Copy-Item $t (Join-Path $dest $leaf) -Recurse -Force -ErrorAction SilentlyContinue
    $copied += $t
    Write-Host "  + $leaf"
  }
}

Write-Host "[3/3] manifest..."
[ordered]@{
  stamp   = $stamp
  root    = $Root
  copied  = $copied
  note    = 'restore-current.ps1 -From backups/' + $stamp + ' 로 복원. 토큰 포함 — 커밋 금지.'
} | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $dest 'manifest.json') -Encoding utf8

Write-Host ""
Write-Host "완료: $dest" -ForegroundColor Green
Write-Host "원복: pwsh scripts/restore-current.ps1 -From backups/$stamp"
