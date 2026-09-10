# D:\self_develop\DevManager\scripts\snapshot-current.ps1
#requires -Version 5.1

<#
    DevManager Snapshot v3.2

    Purpose:
    - Collect current development environment inventory.
    - Back up Claude, Codex, Headroom and OmniRoute state.
    - Back up Docker Desktop and WSL settings when present.
    - Generate manifest.json for restore-current.ps1.

    Safety:
    - Does not modify original Claude/Codex/Headroom/OmniRoute settings.
    - Only reads existing state and creates backup copies.
    - inventory.json is created or replaced in the project root.
    - backups\<timestamp>\ is created for each snapshot.

    Important:
    - Backups may contain OAuth tokens, API keys and other secrets.
    - Never commit the backups directory to Git.
    - OmniRoute SQLite state may not be fully consistent while OmniRoute is running.
    - For migration-grade snapshots, use -RequireServicesStopped.
#>

[CmdletBinding()]
param(
    [string]$Root,

    [switch]$RequireServicesStopped
)

$ErrorActionPreference = 'Stop'


# ------------------------------------------------------------
# Root
# ------------------------------------------------------------

if ([string]::IsNullOrWhiteSpace($Root))
{
    if ([string]::IsNullOrWhiteSpace($PSScriptRoot))
    {
        throw 'PSScriptRoot is empty. Specify the project root with -Root.'
    }

    $Root = Split-Path -Path $PSScriptRoot -Parent
}

$Root = [System.IO.Path]::GetFullPath($Root)

if (-not (Test-Path $Root))
{
    throw "Project root does not exist: $Root"
}


# ------------------------------------------------------------
# Paths
# ------------------------------------------------------------

$Stamp = Get-Date -Format 'yyyy-MM-ddTHHmmss'

$BackupsRoot = Join-Path $Root 'backups'
$Dest = Join-Path $BackupsRoot $Stamp

$InventoryPath = Join-Path $Root 'inventory.json'
$ManifestPath = Join-Path $Dest 'manifest.json'


# ------------------------------------------------------------
# Helpers
# ------------------------------------------------------------

function Get-CommandPath
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,

        [string[]]$FallbackPaths = @()
    )

    try
    {
        $Command = Get-Command `
            -Name $Name `
            -ErrorAction SilentlyContinue

        if ($null -ne $Command)
        {
            if (-not [string]::IsNullOrWhiteSpace($Command.Source))
            {
                return $Command.Source
            }

            if (-not [string]::IsNullOrWhiteSpace($Command.Path))
            {
                return $Command.Path
            }

            if (-not [string]::IsNullOrWhiteSpace($Command.Definition))
            {
                return $Command.Definition
            }
        }
    }
    catch
    {
    }

    foreach ($FallbackPath in $FallbackPaths)
    {
        if ([string]::IsNullOrWhiteSpace($FallbackPath))
        {
            continue
        }

        if (Test-Path $FallbackPath)
        {
            return $FallbackPath
        }
    }

    return $null
}


function Get-VersionSafe
{
    param(
        [string]$Executable,

        [string[]]$Arguments = @()
    )

    if ([string]::IsNullOrWhiteSpace($Executable))
    {
        return $null
    }

    try
    {
        $Output = & $Executable @Arguments 2>&1

        if ($null -eq $Output)
        {
            return $null
        }

        $FirstLine = $Output |
            Select-Object -First 1

        if ($null -eq $FirstLine)
        {
            return $null
        }

        return $FirstLine.ToString().Trim()
    }
    catch
    {
        return $null
    }
}


function Test-TcpPort
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$HostName,

        [Parameter(Mandatory = $true)]
        [int]$Port,

        [int]$TimeoutMs = 1000
    )

    $Client = $null
    $AsyncResult = $null

    try
    {
        $Client = New-Object System.Net.Sockets.TcpClient

        $AsyncResult = $Client.BeginConnect(
            $HostName,
            $Port,
            $null,
            $null
        )

        $Connected = $AsyncResult.AsyncWaitHandle.WaitOne(
            $TimeoutMs,
            $false
        )

        if (-not $Connected)
        {
            return $false
        }

        $Client.EndConnect($AsyncResult)

        return [bool]$Client.Connected
    }
    catch
    {
        return $false
    }
    finally
    {
        if ($null -ne $AsyncResult)
        {
            try
            {
                $AsyncResult.AsyncWaitHandle.Close()
            }
            catch
            {
            }
        }

        if ($null -ne $Client)
        {
            try
            {
                $Client.Close()
            }
            catch
            {
            }
        }
    }
}


function Get-HttpSafe
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$Url,

        [hashtable]$Headers = $null
    )

    try
    {
        if ($null -ne $Headers)
        {
            return Invoke-RestMethod `
                -Uri $Url `
                -Headers $Headers `
                -TimeoutSec 5 `
                -ErrorAction Stop
        }

        return Invoke-RestMethod `
            -Uri $Url `
            -TimeoutSec 5 `
            -ErrorAction Stop
    }
    catch
    {
        return $null
    }
}


function Mask-Secret
{
    param(
        [string]$Value
    )

    if ([string]::IsNullOrEmpty($Value))
    {
        return $Value
    }

    if ($Value.Length -le 8)
    {
        return '***'
    }

    return (
        $Value.Substring(0, 4) +
        '...' +
        $Value.Substring($Value.Length - 4)
    )
}


function Test-SecretName
{
    param(
        [string]$Name
    )

    if ([string]::IsNullOrWhiteSpace($Name))
    {
        return $false
    }

    return (
        $Name -match
        'KEY|TOKEN|SECRET|PASSWORD|PASSWD|CREDENTIAL|AUTH|PRIVATE'
    )
}


function Get-WslInfo
{
    try
    {
        $Wsl = Get-Command `
            -Name 'wsl.exe' `
            -ErrorAction SilentlyContinue

        if ($null -eq $Wsl)
        {
            return $null
        }

        $Output = wsl.exe -l -v 2>&1 |
            Out-String

        if ([string]::IsNullOrWhiteSpace($Output))
        {
            return $null
        }

        return $Output.Trim()
    }
    catch
    {
        return $null
    }
}


function Get-NpmGlobal
{
    try
    {
        $Npm = Get-Command `
            -Name 'npm' `
            -ErrorAction SilentlyContinue

        if ($null -eq $Npm)
        {
            return $null
        }

        $Output = npm ls -g --depth=0 2>&1 |
            Out-String

        if ([string]::IsNullOrWhiteSpace($Output))
        {
            return $null
        }

        return $Output.Trim()
    }
    catch
    {
        return $null
    }
}


function Get-OmniRouteVersion
{
    try
    {
        $OmniRouteCommand = Get-Command `
            -Name 'omniroute' `
            -ErrorAction SilentlyContinue

        if ($null -ne $OmniRouteCommand)
        {
            $Version = Get-VersionSafe `
                -Executable 'omniroute' `
                -Arguments @('--version')

            if (-not [string]::IsNullOrWhiteSpace($Version))
            {
                return $Version
            }
        }

        $Npm = Get-Command `
            -Name 'npm' `
            -ErrorAction SilentlyContinue

        if ($null -ne $Npm)
        {
            $Match = npm ls -g omniroute --depth=0 2>&1 |
                Select-String 'omniroute@' |
                Select-Object -First 1

            if ($null -ne $Match)
            {
                return $Match.ToString().Trim()
            }
        }

        return $null
    }
    catch
    {
        return $null
    }
}


function Get-VisualStudioVersions
{
    try
    {
        $ProgramFilesX86 = ${env:ProgramFiles(x86)}

        if ([string]::IsNullOrWhiteSpace($ProgramFilesX86))
        {
            return $null
        }

        $VsWhere = Join-Path `
            $ProgramFilesX86 `
            'Microsoft Visual Studio\Installer\vswhere.exe'

        if (-not (Test-Path $VsWhere))
        {
            return $null
        }

        $Versions = & $VsWhere `
            -all `
            -property catalog_productDisplayVersion `
            2>$null

        if ($null -eq $Versions)
        {
            return $null
        }

        return ($Versions -join ', ').Trim()
    }
    catch
    {
        return $null
    }
}


function Get-QtVersions
{
    try
    {
        if (-not (Test-Path 'C:\Qt'))
        {
            return $null
        }

        $Versions = Get-ChildItem `
            -Path 'C:\Qt' `
            -Directory `
            -ErrorAction SilentlyContinue |
            Where-Object {
                $_.Name -match '^\d+\.\d+'
            } |
            ForEach-Object {
                $_.Name
            }

        if ($null -eq $Versions)
        {
            return $null
        }

        return ($Versions -join ', ')
    }
    catch
    {
        return $null
    }
}


function Get-OsCaption
{
    try
    {
        $Os = Get-CimInstance `
            -ClassName Win32_OperatingSystem `
            -ErrorAction Stop

        if ($null -eq $Os)
        {
            return $null
        }

        return $Os.Caption
    }
    catch
    {
        return $null
    }
}


function Copy-SnapshotItem
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$Source,

        [Parameter(Mandatory = $true)]
        [string]$Destination
    )

    Copy-Item `
        -Path $Source `
        -Destination $Destination `
        -Recurse `
        -Force `
        -ErrorAction Stop

    if (-not (Test-Path $Destination))
    {
        throw "Backup destination was not created: $Destination"
    }
}


# ------------------------------------------------------------
# Header
# ------------------------------------------------------------

Write-Host ''
Write-Host '========================================' -ForegroundColor Cyan
Write-Host ' DevManager Snapshot v3.2' -ForegroundColor Cyan
Write-Host '========================================' -ForegroundColor Cyan
Write-Host ''

Write-Host "Root      : $Root"
Write-Host "Timestamp : $Stamp"
Write-Host ''


# ------------------------------------------------------------
# 1. Service status
# ------------------------------------------------------------

Write-Host '[1/4] Checking services...' -ForegroundColor Cyan


$HeadroomListening = Test-TcpPort `
    -HostName '127.0.0.1' `
    -Port 8787 `
    -TimeoutMs 1000


$OmniRouteListening = Test-TcpPort `
    -HostName '127.0.0.1' `
    -Port 20128 `
    -TimeoutMs 1000


$HeadroomHealth = $null

if ($HeadroomListening)
{
    $HeadroomHealth = Get-HttpSafe `
        -Url 'http://127.0.0.1:8787/health'
}


$OmniRouteHeaders = $null

if (-not [string]::IsNullOrWhiteSpace($env:OMNIROUTE_API_KEY))
{
    $OmniRouteHeaders = @{
        Authorization = (
            'Bearer ' +
            $env:OMNIROUTE_API_KEY
        )
    }
}


$OmniRouteModels = $null

if ($OmniRouteListening)
{
    $OmniRouteModels = Get-HttpSafe `
        -Url 'http://127.0.0.1:20128/v1/models' `
        -Headers $OmniRouteHeaders
}


if ($HeadroomListening)
{
    Write-Host '  [OK]   Headroom :8787 is listening.' -ForegroundColor Green
}
else
{
    Write-Host '  [WARN] Headroom :8787 is not listening.' -ForegroundColor Yellow
}


if ($OmniRouteListening)
{
    Write-Host '  [OK]   OmniRoute :20128 is listening.' -ForegroundColor Green
}
else
{
    Write-Host '  [WARN] OmniRoute :20128 is not listening.' -ForegroundColor Yellow
}


if ($null -ne $HeadroomHealth)
{
    Write-Host '  [OK]   Headroom health API responded.' -ForegroundColor Green
}
elseif ($HeadroomListening)
{
    Write-Host '  [WARN] Headroom port is open but /health failed.' -ForegroundColor Yellow
}


if ($null -ne $OmniRouteModels)
{
    Write-Host '  [OK]   OmniRoute /v1/models responded.' -ForegroundColor Green
}
elseif ($OmniRouteListening)
{
    Write-Host '  [WARN] OmniRoute port is open but /v1/models failed.' -ForegroundColor Yellow
}


if ($RequireServicesStopped)
{
    if ($HeadroomListening)
    {
        throw 'Headroom is running. Stop Headroom and run the snapshot again.'
    }

    if ($OmniRouteListening)
    {
        throw 'OmniRoute is running. Stop OmniRoute and run the snapshot again.'
    }
}


# ------------------------------------------------------------
# Create snapshot directory
# ------------------------------------------------------------

if (-not (Test-Path $BackupsRoot))
{
    New-Item `
        -ItemType Directory `
        -Path $BackupsRoot `
        -Force |
        Out-Null
}


New-Item `
    -ItemType Directory `
    -Path $Dest `
    -Force |
    Out-Null


# ------------------------------------------------------------
# 2. Inventory
# ------------------------------------------------------------

Write-Host ''
Write-Host '[2/4] Collecting tool versions...' -ForegroundColor Cyan


$HeadroomExe = Get-CommandPath `
    -Name 'headroom' `
    -FallbackPaths @(
        "$env:USERPROFILE\.local\bin\headroom.exe",
        "$env:USERPROFILE\.local\bin\headroom.EXE"
    )


$OsCaption = Get-OsCaption


$HeadroomRunningVersion = $null
$HeadroomReady = $false


if ($null -ne $HeadroomHealth)
{
    try
    {
        if ($null -ne $HeadroomHealth.version)
        {
            $HeadroomRunningVersion = $HeadroomHealth.version
        }
    }
    catch
    {
    }

    try
    {
        if ($null -ne $HeadroomHealth.ready)
        {
            $HeadroomReady = [bool]$HeadroomHealth.ready
        }
    }
    catch
    {
    }
}


$Inventory = [ordered]@{
    schemaVersion = 3

    snapshotVersion = '3.2'

    capturedAt = (Get-Date).ToString('o')

    machine = $env:COMPUTERNAME

    userProfile = $env:USERPROFILE

    root = $Root

    os = [ordered]@{
        caption = $OsCaption

        version = (
            [Environment]::OSVersion.Version.ToString()
        )

        is64Bit = (
            [Environment]::Is64BitOperatingSystem
        )
    }

    tools = [ordered]@{
        docker = Get-VersionSafe `
            -Executable 'docker' `
            -Arguments @('--version')

        wsl = Get-WslInfo

        node = Get-VersionSafe `
            -Executable 'node' `
            -Arguments @('--version')

        npm = Get-VersionSafe `
            -Executable 'npm' `
            -Arguments @('--version')

        python = Get-VersionSafe `
            -Executable 'python' `
            -Arguments @('--version')

        cmake = Get-VersionSafe `
            -Executable 'cmake' `
            -Arguments @('--version')

        git = Get-VersionSafe `
            -Executable 'git' `
            -Arguments @('--version')

        claude = Get-VersionSafe `
            -Executable 'claude' `
            -Arguments @('--version')

        codex = Get-VersionSafe `
            -Executable 'codex' `
            -Arguments @('--version')

        headroom = Get-VersionSafe `
            -Executable $HeadroomExe `
            -Arguments @('--version')

        omniroute = Get-OmniRouteVersion
    }

    npmGlobal = Get-NpmGlobal

    qt = Get-QtVersions

    visualStudio = Get-VisualStudioVersions

    services = [ordered]@{
        headroom = [ordered]@{
            port = 8787

            listening = $HeadroomListening

            installedExecutable = $HeadroomExe

            runningVersion = $HeadroomRunningVersion

            ready = $HeadroomReady

            health = $HeadroomHealth
        }

        omniroute = [ordered]@{
            port = 20128

            listening = $OmniRouteListening

            modelsApiReachable = (
                $null -ne $OmniRouteModels
            )

            models = $OmniRouteModels
        }
    }

    env = [ordered]@{}
}


# ------------------------------------------------------------
# Environment variable inventory
# ------------------------------------------------------------

try
{
    Get-ChildItem Env: |
        Where-Object {
            $_.Name -match (
                'ANTHROPIC|' +
                'CLAUDE|' +
                'OMNIROUTE|' +
                'HEADROOM|' +
                'DOCKER|' +
                'QT|' +
                'CMAKE'
            )
        } |
        ForEach-Object {

            $Value = $_.Value

            if (Test-SecretName -Name $_.Name)
            {
                $Value = Mask-Secret -Value $Value
            }

            $Inventory.env[$_.Name] = $Value
        }
}
catch
{
    Write-Host (
        '  [WARN] Environment variable inventory failed: ' +
        $_.Exception.Message
    ) -ForegroundColor Yellow
}


$Inventory |
    ConvertTo-Json -Depth 15 |
    Set-Content `
        -Path $InventoryPath `
        -Encoding UTF8


Write-Host "  [OK]   Inventory: $InventoryPath" -ForegroundColor Green


# ------------------------------------------------------------
# Display important detected versions
# ------------------------------------------------------------

$ToolDisplay = @(
    @('Docker', $Inventory.tools.docker),
    @('Node', $Inventory.tools.node),
    @('npm', $Inventory.tools.npm),
    @('Python', $Inventory.tools.python),
    @('CMake', $Inventory.tools.cmake),
    @('Git', $Inventory.tools.git),
    @('Claude', $Inventory.tools.claude),
    @('Codex', $Inventory.tools.codex),
    @('Headroom', $Inventory.tools.headroom),
    @('OmniRoute', $Inventory.tools.omniroute)
)


foreach ($Tool in $ToolDisplay)
{
    $ToolName = $Tool[0]
    $ToolVersion = $Tool[1]

    if ([string]::IsNullOrWhiteSpace([string]$ToolVersion))
    {
        Write-Host (
            '  [SKIP] ' +
            $ToolName +
            ' version not detected.'
        ) -ForegroundColor DarkGray
    }
    else
    {
        Write-Host (
            '  [OK]   ' +
            $ToolName +
            ': ' +
            $ToolVersion
        ) -ForegroundColor Green
    }
}


# ------------------------------------------------------------
# 3. Snapshot targets
# ------------------------------------------------------------

Write-Host ''
Write-Host '[3/4] Backing up configuration...' -ForegroundColor Cyan


$Targets = @(
    [ordered]@{
        name = 'headroom'
        source = "$env:USERPROFILE\.headroom"
        backupName = '.headroom'
        type = 'directory'
        requiresStoppedService = $true
    },

    [ordered]@{
        name = 'omniroute-userprofile'
        source = "$env:USERPROFILE\.omniroute"
        backupName = '.omniroute'
        type = 'directory'
        requiresStoppedService = $true
    },

    [ordered]@{
        name = 'omniroute-appdata'
        source = "$env:APPDATA\omniroute"
        backupName = 'omniroute-appdata'
        type = 'directory'
        requiresStoppedService = $true
    },

    [ordered]@{
        name = 'claude'
        source = "$env:USERPROFILE\.claude"
        backupName = '.claude'
        type = 'directory'
        requiresStoppedService = $false
    },

    [ordered]@{
        name = 'codex'
        source = "$env:USERPROFILE\.codex"
        backupName = '.codex'
        type = 'directory'
        requiresStoppedService = $false
    },

    [ordered]@{
        name = 'docker-settings'
        source = "$env:APPDATA\Docker\settings.json"
        backupName = 'docker-settings.json'
        type = 'file'
        requiresStoppedService = $true
    },

    [ordered]@{
        name = 'wslconfig'
        source = "$env:USERPROFILE\.wslconfig"
        backupName = '.wslconfig'
        type = 'file'
        requiresStoppedService = $false
    }
)


$Components = @()


foreach ($Target in $Targets)
{
    $Source = $Target.source

    $BackupPath = Join-Path `
        $Dest `
        $Target.backupName

    $SourceExists = Test-Path $Source


    $Component = [ordered]@{
        name = $Target.name

        source = $Source

        backupName = $Target.backupName

        type = $Target.type

        exists = $SourceExists

        copied = $false

        error = $null

        requiresStoppedService = (
            $Target.requiresStoppedService
        )
    }


    if (-not $SourceExists)
    {
        Write-Host (
            '  [SKIP] ' +
            $Target.name +
            ' not found: ' +
            $Source
        ) -ForegroundColor DarkGray

        $Components += $Component

        continue
    }


    try
    {
        Copy-SnapshotItem `
            -Source $Source `
            -Destination $BackupPath


        $Component.copied = $true


        Write-Host (
            '  [OK]   ' +
            $Target.name +
            ' -> ' +
            $Target.backupName
        ) -ForegroundColor Green
    }
    catch
    {
        $Component.error = $_.Exception.Message


        Write-Host (
            '  [FAIL] ' +
            $Target.name +
            ': ' +
            $_.Exception.Message
        ) -ForegroundColor Red
    }


    $Components += $Component
}


$FailedComponents = @(
    $Components |
    Where-Object {
        $_.exists -and
        (-not $_.copied)
    }
)


# ------------------------------------------------------------
# 4. Manifest
# ------------------------------------------------------------

Write-Host ''
Write-Host '[4/4] Writing manifest...' -ForegroundColor Cyan


$ConsistentSnapshot = (
    (-not $HeadroomListening) -and
    (-not $OmniRouteListening)
)


$Manifest = [ordered]@{
    schemaVersion = 3

    snapshotVersion = '3.2'

    stamp = $Stamp

    capturedAt = (Get-Date).ToString('o')

    sourceMachine = $env:COMPUTERNAME

    sourceUserProfile = $env:USERPROFILE

    root = $Root

    inventoryFile = 'inventory.json'

    serviceStateAtSnapshot = [ordered]@{
        headroomListening = $HeadroomListening

        omnirouteListening = $OmniRouteListening
    }

    consistentSnapshot = $ConsistentSnapshot

    components = $Components

    warnings = @(
        'Backup data may contain OAuth tokens, API keys and other secrets.',
        'Never commit the backups directory to Git.',
        'An OmniRoute snapshot taken while OmniRoute is running may not represent a fully consistent SQLite state.',
        'Docker settings.json may not be compatible with a different Docker Desktop version.',
        '.wslconfig may require adjustment for another machine.'
    )
}


$Manifest |
    ConvertTo-Json -Depth 10 |
    Set-Content `
        -Path $ManifestPath `
        -Encoding UTF8


Write-Host "  [OK]   Manifest: $ManifestPath" -ForegroundColor Green


# ------------------------------------------------------------
# Result
# ------------------------------------------------------------

Write-Host ''
Write-Host '========================================' -ForegroundColor Cyan
Write-Host ' Snapshot Result' -ForegroundColor Cyan
Write-Host '========================================' -ForegroundColor Cyan


if ($FailedComponents.Count -gt 0)
{
    Write-Host ''
    Write-Host '[FAIL] Snapshot completed with backup errors.' -ForegroundColor Red

    foreach ($Failed in $FailedComponents)
    {
        Write-Host (
            '  - ' +
            $Failed.name +
            ': ' +
            $Failed.error
        ) -ForegroundColor Red
    }

    Write-Host ''
    Write-Host "Snapshot directory: $Dest" -ForegroundColor Yellow
    Write-Host 'Do not use this snapshot for restore until the failures are resolved.' -ForegroundColor Yellow

    exit 2
}


Write-Host ''
Write-Host '[OK] Snapshot completed successfully.' -ForegroundColor Green
Write-Host ''
Write-Host "Snapshot directory : $Dest"
Write-Host "Inventory          : $InventoryPath"
Write-Host "Manifest           : $ManifestPath"


if ($ConsistentSnapshot)
{
    Write-Host ''
    Write-Host '[OK] Snapshot consistency: MIGRATION READY' -ForegroundColor Green
    Write-Host '     Headroom and OmniRoute were stopped during snapshot.'
}
else
{
    Write-Host ''
    Write-Host '[WARN] Snapshot consistency: LIVE SNAPSHOT' -ForegroundColor Yellow
    Write-Host '       Headroom and/or OmniRoute were running.' -ForegroundColor Yellow
    Write-Host '       Suitable for inspection/testing, but not recommended as the final migration snapshot.' -ForegroundColor Yellow
}


Write-Host ''
Write-Host 'Original configuration changed:' -NoNewline
Write-Host ' NO' -ForegroundColor Green

Write-Host ''
Write-Host 'Created/updated files:'
Write-Host "  $InventoryPath"
Write-Host "  $Dest"


Write-Host ''
Write-Host 'WARNING:' -ForegroundColor Yellow
Write-Host '  The snapshot may contain authentication credentials.'
Write-Host '  Keep the backups directory private and out of Git.'


Write-Host ''
Write-Host 'Suggested restore command:' -ForegroundColor Cyan


$RestoreScript = Join-Path `
    $PSScriptRoot `
    'restore-current.ps1'


Write-Host (
    'powershell.exe -ExecutionPolicy Bypass ' +
    '-File "' +
    $RestoreScript +
    '" -From "' +
    $Dest +
    '"'
)