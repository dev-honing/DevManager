# D:\self_develop\DevManager\scripts\snapshot-current.ps1
#requires -Version 5.1

<#
    DevManager Snapshot v4.0

    Goals:
    - Discover the current AI development environment dynamically.
    - Do not hardcode individual skill/plugin names.
    - Discover skill roots and plugin roots from common config locations.
    - Discover global npm and pip packages dynamically.
    - Back up core AI tool configuration.
    - Back up discovered skills/plugins that are outside core backup roots.
    - Preserve inventory.json and manifest.json inside each snapshot.
    - Keep service-specific health probes separate from generic discovery.

    Safety:
    - Original configuration is never modified.
    - Snapshot only reads and copies existing files.
    - Backups may contain credentials and OAuth state.
    - Never commit backups/ to Git.
    - For migration-grade snapshots, stop Headroom and OmniRoute and use
      -RequireServicesStopped.
#>

[CmdletBinding()]
param(
    [string]$Root,

    [switch]$RequireServicesStopped
)

$ErrorActionPreference = 'Stop'


# ============================================================
# Root
# ============================================================

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


# ============================================================
# Paths
# ============================================================

$Stamp = Get-Date -Format 'yyyy-MM-ddTHHmmss'

$BackupsRoot = Join-Path $Root 'backups'
$Dest = Join-Path $BackupsRoot $Stamp

$InventoryPath = Join-Path $Root 'inventory.json'
$SnapshotInventoryPath = Join-Path $Dest 'inventory.json'
$ManifestPath = Join-Path $Dest 'manifest.json'


# ============================================================
# Helpers
# ============================================================

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


function Get-SafeName
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$Value
    )

    $Result = $Value -replace '[^a-zA-Z0-9._-]', '_'

    if ([string]::IsNullOrWhiteSpace($Result))
    {
        return 'unknown'
    }

    return $Result
}


function Get-ShortHash
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$Value
    )

    $Sha = [System.Security.Cryptography.SHA256]::Create()

    try
    {
        $Bytes = [System.Text.Encoding]::UTF8.GetBytes($Value)
        $Hash = $Sha.ComputeHash($Bytes)

        $Hex = -join (
            $Hash |
            ForEach-Object {
                $_.ToString('x2')
            }
        )

        return $Hex.Substring(0, 10)
    }
    finally
    {
        $Sha.Dispose()
    }
}


function Test-PathInside
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$ChildPath,

        [Parameter(Mandatory = $true)]
        [string]$ParentPath
    )

    try
    {
        $ChildFull = [System.IO.Path]::GetFullPath($ChildPath).
            TrimEnd('\') + '\'

        $ParentFull = [System.IO.Path]::GetFullPath($ParentPath).
            TrimEnd('\') + '\'

        return $ChildFull.StartsWith(
            $ParentFull,
            [System.StringComparison]::OrdinalIgnoreCase
        )
    }
    catch
    {
        return $false
    }
}


function Get-LinkInfo
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $Result = [ordered]@{
        isLink = $false
        linkType = $null
        target = $null
    }

    try
    {
        $Item = Get-Item `
            -LiteralPath $Path `
            -Force `
            -ErrorAction Stop

        if (
            $Item.Attributes -band
            [System.IO.FileAttributes]::ReparsePoint
        )
        {
            $Result.isLink = $true

            if ($Item.PSObject.Properties.Name -contains 'LinkType')
            {
                $Result.linkType = [string]$Item.LinkType
            }

            if ($Item.PSObject.Properties.Name -contains 'Target')
            {
                $Target = $Item.Target

                if ($Target -is [System.Array])
                {
                    $Target = $Target |
                        Select-Object -First 1
                }

                if (-not [string]::IsNullOrWhiteSpace([string]$Target))
                {
                    $TargetString = [string]$Target

                    if (
                        -not [System.IO.Path]::IsPathRooted(
                            $TargetString
                        )
                    )
                    {
                        $TargetString = Join-Path `
                            $Item.Parent.FullName `
                            $TargetString
                    }

                    try
                    {
                        $TargetString = [System.IO.Path]::GetFullPath(
                            $TargetString
                        )
                    }
                    catch
                    {
                    }

                    $Result.target = $TargetString
                }
            }
        }
    }
    catch
    {
    }

    return $Result
}


function Get-GitInfo
{
    param(
        [string]$Path
    )

    $Result = [ordered]@{
        detected = $false
        root = $null
        remote = $null
        commit = $null
        branch = $null
    }

    if (
        [string]::IsNullOrWhiteSpace($Path) -or
        (-not (Test-Path $Path))
    )
    {
        return $Result
    }

    $Git = Get-Command `
        -Name 'git' `
        -ErrorAction SilentlyContinue

    if ($null -eq $Git)
    {
        return $Result
    }

    try
    {
        $GitRoot = & git `
            -C $Path `
            rev-parse `
            --show-toplevel `
            2>$null

        if ($LASTEXITCODE -ne 0)
        {
            return $Result
        }

        $GitRoot = (
            $GitRoot |
            Select-Object -First 1
        ).ToString().Trim()

        if ([string]::IsNullOrWhiteSpace($GitRoot))
        {
            return $Result
        }

        $Result.detected = $true
        $Result.root = $GitRoot

        try
        {
            $Remote = & git `
                -C $Path `
                config `
                --get `
                remote.origin.url `
                2>$null

            if ($LASTEXITCODE -eq 0)
            {
                $Result.remote = (
                    $Remote |
                    Select-Object -First 1
                ).ToString().Trim()
            }
        }
        catch
        {
        }

        try
        {
            $Commit = & git `
                -C $Path `
                rev-parse `
                HEAD `
                2>$null

            if ($LASTEXITCODE -eq 0)
            {
                $Result.commit = (
                    $Commit |
                    Select-Object -First 1
                ).ToString().Trim()
            }
        }
        catch
        {
        }

        try
        {
            $Branch = & git `
                -C $Path `
                branch `
                --show-current `
                2>$null

            if ($LASTEXITCODE -eq 0)
            {
                $Result.branch = (
                    $Branch |
                    Select-Object -First 1
                ).ToString().Trim()
            }
        }
        catch
        {
        }
    }
    catch
    {
    }

    return $Result
}


# ============================================================
# Dynamic root discovery
# ============================================================

function Get-DiscoveryRoots
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$LeafName
    )

    $Results = @()
    $Seen = @{}

    $ExplicitCandidates = @(
        "$env:USERPROFILE\.claude\$LeafName",
        "$env:USERPROFILE\.codex\$LeafName",
        "$env:USERPROFILE\.agents\$LeafName",
        "$env:USERPROFILE\.gemini\$LeafName",
        "$env:USERPROFILE\.gemini\config\$LeafName",
        "$env:USERPROFILE\.config\$LeafName",
        "$env:USERPROFILE\.local\$LeafName"
    )

    foreach ($Candidate in $ExplicitCandidates)
    {
        if (-not (Test-Path $Candidate))
        {
            continue
        }

        try
        {
            $Full = [System.IO.Path]::GetFullPath($Candidate)

            if (-not $Seen.ContainsKey($Full.ToLowerInvariant()))
            {
                $Seen[$Full.ToLowerInvariant()] = $true
                $Results += $Full
            }
        }
        catch
        {
        }
    }


    $ParentCandidates = @(
        $env:USERPROFILE,
        $env:APPDATA,
        $env:LOCALAPPDATA
    )


    foreach ($Parent in $ParentCandidates)
    {
        if (
            [string]::IsNullOrWhiteSpace($Parent) -or
            (-not (Test-Path $Parent))
        )
        {
            continue
        }

        $Children = Get-ChildItem `
            -LiteralPath $Parent `
            -Directory `
            -Force `
            -ErrorAction SilentlyContinue

        foreach ($Child in $Children)
        {
            $Candidates = @(
                (Join-Path $Child.FullName $LeafName),
                (Join-Path $Child.FullName "config\$LeafName")
            )

            foreach ($Candidate in $Candidates)
            {
                if (-not (Test-Path $Candidate))
                {
                    continue
                }

                try
                {
                    $Full = [System.IO.Path]::GetFullPath($Candidate)
                    $Key = $Full.ToLowerInvariant()

                    if (-not $Seen.ContainsKey($Key))
                    {
                        $Seen[$Key] = $true
                        $Results += $Full
                    }
                }
                catch
                {
                }
            }
        }
    }


    return @(
        $Results |
        Sort-Object
    )
}


function Get-HostFromRoot
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$RootPath
    )

    $Lower = $RootPath.ToLowerInvariant()

    if ($Lower -match '\\\.claude\\')
    {
        return 'claude'
    }

    if ($Lower -match '\\\.codex\\')
    {
        return 'codex'
    }

    if ($Lower -match '\\\.agents\\')
    {
        return 'agents'
    }

    if ($Lower -match '\\\.gemini\\')
    {
        return 'gemini'
    }

    try
    {
        $Parent = Split-Path `
            -Path $RootPath `
            -Parent

        $ParentItem = Get-Item `
            -LiteralPath $Parent `
            -ErrorAction SilentlyContinue

        if ($null -ne $ParentItem)
        {
            return $ParentItem.Name
        }
    }
    catch
    {
    }

    return 'unknown'
}


function Get-DynamicItems
{
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Roots,

        [Parameter(Mandatory = $true)]
        [string]$ItemType
    )

    $Results = @()


    foreach ($RootPath in $Roots)
    {
        if (-not (Test-Path $RootPath))
        {
            continue
        }

        $HostName = Get-HostFromRoot `
            -RootPath $RootPath


        $Items = Get-ChildItem `
            -LiteralPath $RootPath `
            -Directory `
            -Force `
            -ErrorAction SilentlyContinue


        foreach ($Item in $Items)
        {
            $Link = Get-LinkInfo `
                -Path $Item.FullName

            $GitProbePath = $Item.FullName

            if (
                $Link.isLink -and
                (-not [string]::IsNullOrWhiteSpace($Link.target)) -and
                (Test-Path $Link.target)
            )
            {
                $GitProbePath = $Link.target
            }

            $GitInfo = Get-GitInfo `
                -Path $GitProbePath


            $HasSkillMd = $false

            if ($ItemType -eq 'skill')
            {
                $HasSkillMd = (
                    Test-Path (
                        Join-Path $Item.FullName 'SKILL.md'
                    )
                )
            }


            $Results += [ordered]@{
                name = $Item.Name

                type = $ItemType

                host = $HostName

                root = $RootPath

                path = $Item.FullName

                hasSkillMd = $HasSkillMd

                link = $Link

                git = $GitInfo
            }
        }
    }


    return $Results
}


function Merge-DynamicItemsByName
{
    param(
        [Parameter(Mandatory = $true)]
        [object[]]$Items
    )

    $Groups = [ordered]@{}


    foreach ($Item in $Items)
    {
        $Key = $Item.name.ToLowerInvariant()


        if (-not $Groups.Contains($Key))
        {
            $Groups[$Key] = [ordered]@{
                name = $Item.name

                locations = @()
            }
        }


        $Groups[$Key].locations += [ordered]@{
            host = $Item.host

            root = $Item.root

            path = $Item.path

            hasSkillMd = $Item.hasSkillMd

            link = $Item.link

            git = $Item.git
        }
    }


    $Results = @()


    foreach ($Key in $Groups.Keys)
    {
        $Results += $Groups[$Key]
    }


    return $Results
}


# ============================================================
# Package discovery
# ============================================================

function Get-NpmGlobalPackages
{
    $Results = @()


    try
    {
        $Npm = Get-Command `
            -Name 'npm' `
            -ErrorAction SilentlyContinue

        if ($null -eq $Npm)
        {
            return $Results
        }


        $Raw = npm ls -g --depth=0 --json 2>$null |
            Out-String


        if ([string]::IsNullOrWhiteSpace($Raw))
        {
            return $Results
        }


        $Json = $Raw |
            ConvertFrom-Json


        if ($null -eq $Json.dependencies)
        {
            return $Results
        }


        foreach ($Property in $Json.dependencies.PSObject.Properties)
        {
            $Package = $Property.Value

            $Results += [ordered]@{
                manager = 'npm'

                name = $Property.Name

                version = [string]$Package.version
            }
        }
    }
    catch
    {
    }


    return @(
        $Results |
        Sort-Object name
    )
}


function Get-PipGlobalPackages
{
    $Results = @()


    try
    {
        $Python = Get-Command `
            -Name 'python' `
            -ErrorAction SilentlyContinue

        if ($null -eq $Python)
        {
            return $Results
        }


        $Raw = python -m pip list --format=json 2>$null |
            Out-String


        if ([string]::IsNullOrWhiteSpace($Raw))
        {
            return $Results
        }


        $Packages = $Raw |
            ConvertFrom-Json


        foreach ($Package in @($Packages))
        {
            $Results += [ordered]@{
                manager = 'pip'

                name = [string]$Package.name

                version = [string]$Package.version
            }
        }
    }
    catch
    {
    }


    return @(
        $Results |
        Sort-Object name
    )
}


# ============================================================
# WSL
# ============================================================

function Get-WslInventory
{
    $Result = [ordered]@{
        installed = $false

        distributions = @()
    }


    try
    {
        $Wsl = Get-Command `
            -Name 'wsl.exe' `
            -ErrorAction SilentlyContinue


        if ($null -eq $Wsl)
        {
            return $Result
        }


        $Result.installed = $true


        $TempFile = Join-Path `
            $env:TEMP `
            (
                'devmanager-wsl-' +
                [Guid]::NewGuid().ToString('N') +
                '.txt'
            )


        try
        {
            Start-Process `
                -FilePath 'wsl.exe' `
                -ArgumentList @('-l', '-v') `
                -NoNewWindow `
                -Wait `
                -RedirectStandardOutput $TempFile


            if (-not (Test-Path $TempFile))
            {
                return $Result
            }


            $Bytes = [System.IO.File]::ReadAllBytes($TempFile)


            if ($Bytes.Length -eq 0)
            {
                return $Result
            }


            $Text = $null


            if (
                $Bytes.Length -ge 2 -and
                $Bytes[0] -eq 0xFF -and
                $Bytes[1] -eq 0xFE
            )
            {
                $Text = [System.Text.Encoding]::Unicode.GetString(
                    $Bytes,
                    2,
                    $Bytes.Length - 2
                )
            }
            else
            {
                $ZeroCount = 0


                foreach ($Byte in $Bytes)
                {
                    if ($Byte -eq 0)
                    {
                        $ZeroCount++
                    }
                }


                if ($ZeroCount -gt ($Bytes.Length / 4))
                {
                    $Text = [System.Text.Encoding]::Unicode.GetString(
                        $Bytes
                    )
                }
                else
                {
                    $Text = [System.Text.Encoding]::UTF8.GetString(
                        $Bytes
                    )
                }
            }


            $Lines = $Text -split "`r?`n"


            foreach ($Line in $Lines)
            {
                if ([string]::IsNullOrWhiteSpace($Line))
                {
                    continue
                }


                $CleanLine = $Line.Trim()


                if ($CleanLine -match '^NAME\s+STATE\s+VERSION$')
                {
                    continue
                }


                $CleanLine = $CleanLine -replace '^\*\s*', ''


                if (
                    $CleanLine -match
                    '^(?<name>.+?)\s{2,}(?<state>Running|Stopped)\s+(?<version>\d+)$'
                )
                {
                    $Result.distributions += [ordered]@{
                        name = $Matches.name.Trim()

                        state = $Matches.state.Trim()

                        version = [int]$Matches.version
                    }
                }
            }
        }
        finally
        {
            if (Test-Path $TempFile)
            {
                Remove-Item `
                    -LiteralPath $TempFile `
                    -Force `
                    -ErrorAction SilentlyContinue
            }
        }
    }
    catch
    {
    }


    return $Result
}


# ============================================================
# Platform inventory
# ============================================================

function Get-OsCaption
{
    try
    {
        $Os = Get-CimInstance `
            -ClassName Win32_OperatingSystem `
            -ErrorAction Stop

        return $Os.Caption
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
            -LiteralPath 'C:\Qt' `
            -Directory `
            -ErrorAction SilentlyContinue |
            Where-Object {
                $_.Name -match '^\d+\.\d+'
            } |
            ForEach-Object {
                $_.Name
            }


        return ($Versions -join ', ')
    }
    catch
    {
        return $null
    }
}


function Get-OmniRouteVersion
{
    $Version = Get-VersionSafe `
        -Executable 'omniroute' `
        -Arguments @('--version')

    return $Version
}


function Get-OmniRouteModelSummary
{
    param(
        $ModelsResponse
    )


    $Result = [ordered]@{
        apiReachable = ($null -ne $ModelsResponse)

        modelCount = 0

        selectedChecks = [ordered]@{
            'auto/best-coding' = $false

            'codex/gpt-5.6-terra' = $false
        }
    }


    if ($null -eq $ModelsResponse)
    {
        return $Result
    }


    try
    {
        $Models = @($ModelsResponse.data)

        $Result.modelCount = $Models.Count


        foreach ($Model in $Models)
        {
            $Id = [string]$Model.id


            if ($Result.selectedChecks.Contains($Id))
            {
                $Result.selectedChecks[$Id] = $true
            }
        }
    }
    catch
    {
    }


    return $Result
}


# ============================================================
# Backup
# ============================================================

function Copy-SnapshotItem
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$Source,

        [Parameter(Mandatory = $true)]
        [string]$Destination
    )


    Copy-Item `
        -LiteralPath $Source `
        -Destination $Destination `
        -Recurse `
        -Force `
        -ErrorAction Stop


    if (-not (Test-Path $Destination))
    {
        throw "Backup destination was not created: $Destination"
    }
}


function Add-BackupTarget
{
    param(
        [Parameter(Mandatory = $true)]
        [System.Collections.ArrayList]$Targets,

        [Parameter(Mandatory = $true)]
        [hashtable]$Seen,

        [Parameter(Mandatory = $true)]
        [string]$Name,

        [Parameter(Mandatory = $true)]
        [string]$Source,

        [Parameter(Mandatory = $true)]
        [string]$BackupName,

        [string]$Type = 'directory',

        [bool]$RequiresStoppedService = $false,

        [string]$Category = 'config'
    )


    if ([string]::IsNullOrWhiteSpace($Source))
    {
        return
    }


    try
    {
        $Key = [System.IO.Path]::GetFullPath($Source).
            ToLowerInvariant()
    }
    catch
    {
        $Key = $Source.ToLowerInvariant()
    }


    if ($Seen.ContainsKey($Key))
    {
        return
    }


    $Seen[$Key] = $true


    [void]$Targets.Add(
        [ordered]@{
            name = $Name

            source = $Source

            backupName = $BackupName

            type = $Type

            category = $Category

            requiresStoppedService = $RequiresStoppedService
        }
    )
}


# ============================================================
# Header
# ============================================================

Write-Host ''
Write-Host '========================================' -ForegroundColor Cyan
Write-Host ' DevManager Snapshot v4.0' -ForegroundColor Cyan
Write-Host '========================================' -ForegroundColor Cyan
Write-Host ''

Write-Host "Root      : $Root"
Write-Host "Timestamp : $Stamp"
Write-Host ''


# ============================================================
# 1. Service checks
# ============================================================

Write-Host '[1/5] Checking infrastructure services...' -ForegroundColor Cyan


$HeadroomListening = Test-TcpPort `
    -HostName '127.0.0.1' `
    -Port 8787


$OmniRouteListening = Test-TcpPort `
    -HostName '127.0.0.1' `
    -Port 20128


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


$OmniRouteModelSummary = Get-OmniRouteModelSummary `
    -ModelsResponse $OmniRouteModels


if ($HeadroomListening)
{
    Write-Host '  [OK]   Headroom :8787' -ForegroundColor Green
}
else
{
    Write-Host '  [INFO] Headroom :8787 not listening.' -ForegroundColor DarkGray
}


if ($OmniRouteListening)
{
    Write-Host '  [OK]   OmniRoute :20128' -ForegroundColor Green
}
else
{
    Write-Host '  [INFO] OmniRoute :20128 not listening.' -ForegroundColor DarkGray
}


if ($RequireServicesStopped)
{
    if ($HeadroomListening)
    {
        throw 'Headroom is running. Stop it and retry.'
    }

    if ($OmniRouteListening)
    {
        throw 'OmniRoute is running. Stop it and retry.'
    }
}


# ============================================================
# Create destination
# ============================================================

New-Item `
    -ItemType Directory `
    -Path $Dest `
    -Force |
    Out-Null


# ============================================================
# 2. Dynamic discovery
# ============================================================

Write-Host ''
Write-Host '[2/5] Discovering skills, plugins and packages...' -ForegroundColor Cyan


$SkillRoots = @(
    Get-DiscoveryRoots `
        -LeafName 'skills'
)


$PluginRoots = @(
    Get-DiscoveryRoots `
        -LeafName 'plugins'
)


$SkillLocations = @(
    Get-DynamicItems `
        -Roots $SkillRoots `
        -ItemType 'skill'
)


$PluginLocations = @(
    Get-DynamicItems `
        -Roots $PluginRoots `
        -ItemType 'plugin'
)


$Skills = @(
    Merge-DynamicItemsByName `
        -Items $SkillLocations
)


$Plugins = @(
    Merge-DynamicItemsByName `
        -Items $PluginLocations
)


$NpmPackages = @(
    Get-NpmGlobalPackages
)


$PipPackages = @(
    Get-PipGlobalPackages
)


$GlobalPackages = @(
    $NpmPackages + $PipPackages
)


Write-Host (
    '  [OK]   Skill roots discovered : ' +
    $SkillRoots.Count
) -ForegroundColor Green


Write-Host (
    '  [OK]   Skills discovered      : ' +
    $Skills.Count
) -ForegroundColor Green


foreach ($Skill in $Skills)
{
    $Hosts = @(
        $Skill.locations |
        ForEach-Object {
            $_.host
        } |
        Sort-Object -Unique
    )


    Write-Host (
        '         skill: ' +
        $Skill.name +
        ' [' +
        ($Hosts -join ', ') +
        ']'
    ) -ForegroundColor DarkGray
}


Write-Host (
    '  [OK]   Plugin roots discovered: ' +
    $PluginRoots.Count
) -ForegroundColor Green


Write-Host (
    '  [OK]   Plugins discovered     : ' +
    $Plugins.Count
) -ForegroundColor Green


foreach ($Plugin in $Plugins)
{
    Write-Host (
        '         plugin: ' +
        $Plugin.name
    ) -ForegroundColor DarkGray
}


Write-Host (
    '  [OK]   npm global packages    : ' +
    $NpmPackages.Count
) -ForegroundColor Green


Write-Host (
    '  [OK]   pip packages           : ' +
    $PipPackages.Count
) -ForegroundColor Green


# ============================================================
# 3. Environment inventory
# ============================================================

Write-Host ''
Write-Host '[3/5] Building environment inventory...' -ForegroundColor Cyan


$HeadroomExe = Get-CommandPath `
    -Name 'headroom' `
    -FallbackPaths @(
        "$env:USERPROFILE\.local\bin\headroom.exe"
    )


$WslInventory = Get-WslInventory


$HeadroomRunningVersion = $null
$HeadroomReady = $false
$HeadroomUpstreamUrl = $null


if ($null -ne $HeadroomHealth)
{
    try
    {
        $HeadroomRunningVersion = [string]$HeadroomHealth.version
    }
    catch
    {
    }


    try
    {
        $HeadroomReady = [bool]$HeadroomHealth.ready
    }
    catch
    {
    }


    try
    {
        $HeadroomUpstreamUrl = [string](
            $HeadroomHealth.checks.upstream.url
        )
    }
    catch
    {
    }
}


$Inventory = [ordered]@{
    schemaVersion = 4

    snapshotVersion = '4.0'

    capturedAt = (Get-Date).ToString('o')

    machine = $env:COMPUTERNAME

    userProfile = $env:USERPROFILE

    root = $Root

    os = [ordered]@{
        caption = Get-OsCaption

        version = [Environment]::OSVersion.Version.ToString()

        is64Bit = [Environment]::Is64BitOperatingSystem
    }

    tools = [ordered]@{
        docker = Get-VersionSafe 'docker' @('--version')

        node = Get-VersionSafe 'node' @('--version')

        npm = Get-VersionSafe 'npm' @('--version')

        python = Get-VersionSafe 'python' @('--version')

        cmake = Get-VersionSafe 'cmake' @('--version')

        git = Get-VersionSafe 'git' @('--version')

        claude = Get-VersionSafe 'claude' @('--version')

        codex = Get-VersionSafe 'codex' @('--version')

        headroom = Get-VersionSafe $HeadroomExe @('--version')

        omniroute = Get-OmniRouteVersion
    }

    qt = Get-QtVersions

    visualStudio = Get-VisualStudioVersions

    wsl = $WslInventory

    discovery = [ordered]@{
        skillRoots = $SkillRoots

        pluginRoots = $PluginRoots

        skills = $Skills

        plugins = $Plugins

        globalPackages = $GlobalPackages
    }

    services = [ordered]@{
        headroom = [ordered]@{
            port = 8787

            listening = $HeadroomListening

            healthApiReachable = (
                $null -ne $HeadroomHealth
            )

            executable = $HeadroomExe

            version = $HeadroomRunningVersion

            ready = $HeadroomReady

            upstreamUrl = $HeadroomUpstreamUrl
        }

        omniroute = [ordered]@{
            port = 20128

            listening = $OmniRouteListening

            modelsApiReachable = (
                [bool]$OmniRouteModelSummary.apiReachable
            )

            modelCount = (
                [int]$OmniRouteModelSummary.modelCount
            )

            selectedChecks = (
                $OmniRouteModelSummary.selectedChecks
            )
        }
    }

    env = [ordered]@{}
}


Get-ChildItem Env: |
    Where-Object {
        $_.Name -match (
            'ANTHROPIC|' +
            'CLAUDE|' +
            'CODEX|' +
            'OMNIROUTE|' +
            'HEADROOM|' +
            'DOCKER|' +
            'QT|' +
            'CMAKE|' +
            'GEMINI'
        )
    } |
    ForEach-Object {

        $Value = $_.Value


        if (Test-SecretName $_.Name)
        {
            $Value = Mask-Secret $Value
        }


        $Inventory.env[$_.Name] = $Value
    }


$InventoryJson = $Inventory |
    ConvertTo-Json -Depth 20


$InventoryJson |
    Set-Content `
        -LiteralPath $InventoryPath `
        -Encoding UTF8


$InventoryJson |
    Set-Content `
        -LiteralPath $SnapshotInventoryPath `
        -Encoding UTF8


Write-Host "  [OK]   $InventoryPath" -ForegroundColor Green


# ============================================================
# 4. Build backup plan dynamically
# ============================================================

Write-Host ''
Write-Host '[4/5] Backing up discovered environment...' -ForegroundColor Cyan


$Targets = New-Object System.Collections.ArrayList
$SeenTargets = @{}


$CoreDirectories = @(
    [ordered]@{
        name = 'claude'
        source = "$env:USERPROFILE\.claude"
        backup = '.claude'
        stopped = $false
    },

    [ordered]@{
        name = 'codex'
        source = "$env:USERPROFILE\.codex"
        backup = '.codex'
        stopped = $false
    },

    [ordered]@{
        name = 'agents'
        source = "$env:USERPROFILE\.agents"
        backup = '.agents'
        stopped = $false
    },

    [ordered]@{
        name = 'gemini'
        source = "$env:USERPROFILE\.gemini"
        backup = '.gemini'
        stopped = $false
    },

    [ordered]@{
        name = 'headroom'
        source = "$env:USERPROFILE\.headroom"
        backup = '.headroom'
        stopped = $true
    },

    [ordered]@{
        name = 'omniroute'
        source = "$env:USERPROFILE\.omniroute"
        backup = '.omniroute'
        stopped = $true
    }
)


foreach ($Core in $CoreDirectories)
{
    Add-BackupTarget `
        -Targets $Targets `
        -Seen $SeenTargets `
        -Name $Core.name `
        -Source $Core.source `
        -BackupName $Core.backup `
        -RequiresStoppedService $Core.stopped `
        -Category 'core'
}


# ------------------------------------------------------------
# Optional machine configuration
# ------------------------------------------------------------

Add-BackupTarget `
    -Targets $Targets `
    -Seen $SeenTargets `
    -Name 'omniroute-appdata' `
    -Source "$env:APPDATA\omniroute" `
    -BackupName 'omniroute-appdata' `
    -Category 'core' `
    -RequiresStoppedService $true


Add-BackupTarget `
    -Targets $Targets `
    -Seen $SeenTargets `
    -Name 'docker-settings' `
    -Source "$env:APPDATA\Docker\settings.json" `
    -BackupName 'docker-settings.json' `
    -Type 'file' `
    -Category 'machine'


Add-BackupTarget `
    -Targets $Targets `
    -Seen $SeenTargets `
    -Name 'wslconfig' `
    -Source "$env:USERPROFILE\.wslconfig" `
    -BackupName '.wslconfig' `
    -Type 'file' `
    -Category 'machine'


# ------------------------------------------------------------
# Add dynamically discovered skills not covered by core roots
# ------------------------------------------------------------

$CoreSourcePaths = @(
    $CoreDirectories |
    ForEach-Object {
        $_.source
    }
)


foreach ($SkillLocation in $SkillLocations)
{
    $Covered = $false


    foreach ($CoreSource in $CoreSourcePaths)
    {
        if (
            (Test-Path $CoreSource) -and
            (Test-PathInside `
                -ChildPath $SkillLocation.path `
                -ParentPath $CoreSource)
        )
        {
            $Covered = $true
            break
        }
    }


    if ($Covered)
    {
        continue
    }


    $SafeHost = Get-SafeName $SkillLocation.host
    $SafeSkill = Get-SafeName $SkillLocation.name
    $Hash = Get-ShortHash $SkillLocation.path


    $BackupName = Join-Path `
        'discovered\skills' `
        (
            $SafeHost +
            '__' +
            $SafeSkill +
            '__' +
            $Hash
        )


    Add-BackupTarget `
        -Targets $Targets `
        -Seen $SeenTargets `
        -Name (
            'skill:' +
            $SkillLocation.host +
            ':' +
            $SkillLocation.name
        ) `
        -Source $SkillLocation.path `
        -BackupName $BackupName `
        -Category 'skill'
}


# ------------------------------------------------------------
# Add dynamically discovered plugins not covered by core roots
# ------------------------------------------------------------

foreach ($PluginLocation in $PluginLocations)
{
    $Covered = $false


    foreach ($CoreSource in $CoreSourcePaths)
    {
        if (
            (Test-Path $CoreSource) -and
            (Test-PathInside `
                -ChildPath $PluginLocation.path `
                -ParentPath $CoreSource)
        )
        {
            $Covered = $true
            break
        }
    }


    if ($Covered)
    {
        continue
    }


    $SafeHost = Get-SafeName $PluginLocation.host
    $SafePlugin = Get-SafeName $PluginLocation.name
    $Hash = Get-ShortHash $PluginLocation.path


    $BackupName = Join-Path `
        'discovered\plugins' `
        (
            $SafeHost +
            '__' +
            $SafePlugin +
            '__' +
            $Hash
        )


    Add-BackupTarget `
        -Targets $Targets `
        -Seen $SeenTargets `
        -Name (
            'plugin:' +
            $PluginLocation.host +
            ':' +
            $PluginLocation.name
        ) `
        -Source $PluginLocation.path `
        -BackupName $BackupName `
        -Category 'plugin'
}


$Components = @()


foreach ($Target in $Targets)
{
    $Exists = Test-Path $Target.source


    $Component = [ordered]@{
        name = $Target.name

        category = $Target.category

        source = $Target.source

        backupName = $Target.backupName

        type = $Target.type

        exists = $Exists

        copied = $false

        error = $null

        requiresStoppedService = (
            $Target.requiresStoppedService
        )
    }


    if (-not $Exists)
    {
        Write-Host (
            '  [SKIP] ' +
            $Target.name +
            ' not found.'
        ) -ForegroundColor DarkGray

        $Components += $Component

        continue
    }


    $BackupPath = Join-Path `
        $Dest `
        $Target.backupName


    $BackupParent = Split-Path `
        -Path $BackupPath `
        -Parent


    if (-not (Test-Path $BackupParent))
    {
        New-Item `
            -ItemType Directory `
            -Path $BackupParent `
            -Force |
            Out-Null
    }


    try
    {
        Copy-SnapshotItem `
            -Source $Target.source `
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


# ============================================================
# 5. Manifest
# ============================================================

Write-Host ''
Write-Host '[5/5] Writing manifest...' -ForegroundColor Cyan


$FailedComponents = @(
    $Components |
    Where-Object {
        $_.exists -and
        (-not $_.copied)
    }
)


$ConsistentSnapshot = (
    (-not $HeadroomListening) -and
    (-not $OmniRouteListening)
)


$Manifest = [ordered]@{
    schemaVersion = 4

    snapshotVersion = '4.0'

    stamp = $Stamp

    capturedAt = (Get-Date).ToString('o')

    sourceMachine = $env:COMPUTERNAME

    sourceUserProfile = $env:USERPROFILE

    sourceRoot = $Root

    inventoryFile = 'inventory.json'

    discovery = [ordered]@{
        skillRootCount = $SkillRoots.Count

        skillCount = $Skills.Count

        pluginRootCount = $PluginRoots.Count

        pluginCount = $Plugins.Count

        npmPackageCount = $NpmPackages.Count

        pipPackageCount = $PipPackages.Count
    }

    serviceStateAtSnapshot = [ordered]@{
        headroomListening = $HeadroomListening

        omnirouteListening = $OmniRouteListening
    }

    consistentSnapshot = $ConsistentSnapshot

    components = $Components

    warnings = @(
        'Backup data may contain OAuth tokens, API keys and other secrets.',
        'Never commit backups/ to Git.',
        'Environment variables in inventory.json are masked and are validation data only.',
        'Do not restore masked secret values as real environment variables.',
        'A live OmniRoute snapshot may contain changing database state.',
        'Machine-specific Docker and WSL settings require explicit migration handling.',
        'Absolute paths inside third-party configuration may require path translation on another PC.',
        'Service-specific health probes are adapters; skill and package discovery is dynamic.'
    )
}


$Manifest |
    ConvertTo-Json -Depth 20 |
    Set-Content `
        -LiteralPath $ManifestPath `
        -Encoding UTF8


Write-Host "  [OK]   $ManifestPath" -ForegroundColor Green


# ============================================================
# Result
# ============================================================

Write-Host ''
Write-Host '========================================' -ForegroundColor Cyan
Write-Host ' Snapshot Result' -ForegroundColor Cyan
Write-Host '========================================' -ForegroundColor Cyan
Write-Host ''


if ($FailedComponents.Count -gt 0)
{
    Write-Host '[FAIL] Snapshot has backup errors.' -ForegroundColor Red


    foreach ($Failed in $FailedComponents)
    {
        Write-Host (
            '       ' +
            $Failed.name +
            ': ' +
            $Failed.error
        ) -ForegroundColor Red
    }


    Write-Host ''
    Write-Host "Snapshot: $Dest" -ForegroundColor Yellow

    exit 2
}


Write-Host '[OK] Snapshot completed successfully.' -ForegroundColor Green

Write-Host ''
Write-Host "Snapshot directory : $Dest"
Write-Host "Root inventory     : $InventoryPath"
Write-Host "Snapshot inventory : $SnapshotInventoryPath"
Write-Host "Manifest           : $ManifestPath"


Write-Host ''
Write-Host 'Dynamic discovery:' -ForegroundColor Cyan

Write-Host (
    '  Skills          : ' +
    $Skills.Count
)

Write-Host (
    '  Plugins         : ' +
    $Plugins.Count
)

Write-Host (
    '  npm packages    : ' +
    $NpmPackages.Count
)

Write-Host (
    '  pip packages    : ' +
    $PipPackages.Count
)


if ($ConsistentSnapshot)
{
    Write-Host ''
    Write-Host '[OK] Snapshot consistency: MIGRATION READY' -ForegroundColor Green
}
else
{
    Write-Host ''

    Write-Host (
        '[WARN] Snapshot consistency: LIVE SNAPSHOT'
    ) -ForegroundColor Yellow

    Write-Host (
        '       Stop Headroom and OmniRoute for the final migration snapshot.'
    ) -ForegroundColor Yellow
}


Write-Host ''
Write-Host 'Original configuration changed:' -NoNewline
Write-Host ' NO' -ForegroundColor Green


Write-Host ''
Write-Host 'IMPORTANT:' -ForegroundColor Yellow

Write-Host (
    '  Snapshot data may contain authentication credentials.'
)

Write-Host (
    '  Keep backups/ private and out of Git.'
)