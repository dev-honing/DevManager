# D:\self_develop\DevManager\scripts\restore-current.ps1
#requires -Version 5.1

<#
    DevManager Restore v4.1

    Compatible with:
    - snapshot-current.ps1 v4.1
    - manifest schemaVersion 4

    Default behavior:
    - DRY RUN ONLY.
    - No files are modified unless -Apply is explicitly specified.

    Safety:
    - Reads restore targets dynamically from manifest.components.
    - Does not use a hardcoded component-name map.
    - Remaps the source user profile to the current user profile.
    - Existing targets are moved to a pre-restore backup before restore.
    - Automatically rolls back if restore fails.
    - Machine-specific configuration is skipped unless
      -RestoreMachineSettings is specified.
    - npm/pip package inventory is never automatically installed.
    - Masked environment variables are never restored.
    - Running Headroom/OmniRoute blocks an actual restore.
    - Linked skills are only recreated when their target exists.
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$From,

    [switch]$Apply,

    [switch]$Force,

    [switch]$RestoreMachineSettings,

    [switch]$RecreateLinks
)

$ErrorActionPreference = 'Stop'


# ============================================================
# Helpers
# ============================================================

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


function Normalize-FullPath
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )


    return [System.IO.Path]::GetFullPath(
        [Environment]::ExpandEnvironmentVariables($Path)
    )
}


function Test-PathPrefix
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,

        [Parameter(Mandatory = $true)]
        [string]$Prefix
    )


    try
    {
        $PathFull = Normalize-FullPath $Path
        $PrefixFull = Normalize-FullPath $Prefix


        if (
            $PathFull.Equals(
                $PrefixFull,
                [System.StringComparison]::OrdinalIgnoreCase
            )
        )
        {
            return $true
        }


        $PrefixWithSlash = $PrefixFull.TrimEnd('\') + '\'


        return $PathFull.StartsWith(
            $PrefixWithSlash,
            [System.StringComparison]::OrdinalIgnoreCase
        )
    }
    catch
    {
        return $false
    }
}


function Convert-RestorePath
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$SourcePath,

        [Parameter(Mandatory = $true)]
        [string]$SourceUserProfile,

        [Parameter(Mandatory = $true)]
        [string]$CurrentUserProfile
    )


    $SourceFull = Normalize-FullPath $SourcePath
    $OldProfile = Normalize-FullPath $SourceUserProfile
    $NewProfile = Normalize-FullPath $CurrentUserProfile


    if (
        $SourceFull.Equals(
            $OldProfile,
            [System.StringComparison]::OrdinalIgnoreCase
        )
    )
    {
        return $NewProfile
    }


    $OldPrefix = $OldProfile.TrimEnd('\') + '\'


    if (
        $SourceFull.StartsWith(
            $OldPrefix,
            [System.StringComparison]::OrdinalIgnoreCase
        )
    )
    {
        $Relative = $SourceFull.Substring(
            $OldPrefix.Length
        )


        return Join-Path `
            $NewProfile `
            $Relative
    }


    return $SourceFull
}


function Get-UniqueBackupPath
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$BaseDirectory,

        [Parameter(Mandatory = $true)]
        [string]$Name,

        [Parameter(Mandatory = $true)]
        [int]$Index
    )


    $SafeName = $Name -replace '[^a-zA-Z0-9._-]', '_'


    return Join-Path `
        $BaseDirectory `
        (
            '{0:D3}_{1}' -f
            $Index,
            $SafeName
        )
}


function Ensure-ParentDirectory
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )


    $Parent = Split-Path `
        -Path $Path `
        -Parent


    if ([string]::IsNullOrWhiteSpace($Parent))
    {
        return
    }


    if (-not (Test-Path $Parent))
    {
        New-Item `
            -ItemType Directory `
            -Path $Parent `
            -Force |
            Out-Null
    }
}


function Copy-RestoreItem
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$Source,

        [Parameter(Mandatory = $true)]
        [string]$Destination,

        [Parameter(Mandatory = $true)]
        [string]$Type
    )


    Ensure-ParentDirectory `
        -Path $Destination


    if ($Type -eq 'file')
    {
        Copy-Item `
            -LiteralPath $Source `
            -Destination $Destination `
            -Force `
            -ErrorAction Stop
    }
    else
    {
        Copy-Item `
            -LiteralPath $Source `
            -Destination $Destination `
            -Recurse `
            -Force `
            -ErrorAction Stop
    }


    if (-not (Test-Path $Destination))
    {
        throw "Restore destination was not created: $Destination"
    }
}


function Remove-RestoreItem
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )


    if (-not (Test-Path $Path))
    {
        return
    }


    Remove-Item `
        -LiteralPath $Path `
        -Recurse `
        -Force `
        -ErrorAction Stop
}


function Restore-OriginalItem
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$BackupPath,

        [Parameter(Mandatory = $true)]
        [string]$Destination
    )


    if (-not (Test-Path $BackupPath))
    {
        return
    }


    if (Test-Path $Destination)
    {
        Remove-RestoreItem `
            -Path $Destination
    }


    Ensure-ParentDirectory `
        -Path $Destination


    Move-Item `
        -LiteralPath $BackupPath `
        -Destination $Destination `
        -Force `
        -ErrorAction Stop
}


function Get-LinkRestorePlan
{
    param(
        $Inventory,

        [Parameter(Mandatory = $true)]
        [string]$SourceUserProfile,

        [Parameter(Mandatory = $true)]
        [string]$CurrentUserProfile
    )


    $Plans = @()


    if ($null -eq $Inventory)
    {
        return $Plans
    }


    if ($null -eq $Inventory.discovery)
    {
        return $Plans
    }


    if ($null -eq $Inventory.discovery.skills)
    {
        return $Plans
    }


    foreach ($Skill in @($Inventory.discovery.skills))
    {
        foreach ($Location in @($Skill.locations))
        {
            if ($null -eq $Location.link)
            {
                continue
            }


            if (-not [bool]$Location.link.isLink)
            {
                continue
            }


            $OldLocationPath = [string]$Location.path
            $OldTargetPath = [string]$Location.link.target


            if (
                [string]::IsNullOrWhiteSpace($OldLocationPath) -or
                [string]::IsNullOrWhiteSpace($OldTargetPath)
            )
            {
                continue
            }


            $NewLocationPath = Convert-RestorePath `
                -SourcePath $OldLocationPath `
                -SourceUserProfile $SourceUserProfile `
                -CurrentUserProfile $CurrentUserProfile


            $NewTargetPath = Convert-RestorePath `
                -SourcePath $OldTargetPath `
                -SourceUserProfile $SourceUserProfile `
                -CurrentUserProfile $CurrentUserProfile


            $Plans += [ordered]@{
                skill = [string]$Skill.name

                host = [string]$Location.host

                location = $NewLocationPath

                target = $NewTargetPath

                linkType = [string]$Location.link.linkType
            }
        }
    }


    return $Plans
}


function Recreate-SkillLinks
{
    param(
        [Parameter(Mandatory = $true)]
        [AllowEmptyCollection()]
        [object[]]$Plans
    )


    $Results = @()


    foreach ($Plan in $Plans)
    {
        $Result = [ordered]@{
            skill = $Plan.skill

            location = $Plan.location

            target = $Plan.target

            recreated = $false

            skipped = $false

            error = $null
        }


        if (-not (Test-Path $Plan.target))
        {
            $Result.skipped = $true

            $Result.error = 'Link target does not exist.'


            Write-Host (
                '  [WARN] Link target missing, keeping restored directory: ' +
                $Plan.skill
            ) -ForegroundColor Yellow


            Write-Host (
                '         target: ' +
                $Plan.target
            ) -ForegroundColor DarkGray


            $Results += $Result

            continue
        }


        try
        {
            if (Test-Path $Plan.location)
            {
                Remove-RestoreItem `
                    -Path $Plan.location
            }


            Ensure-ParentDirectory `
                -Path $Plan.location


            $ItemType = 'SymbolicLink'


            if (
                -not [string]::IsNullOrWhiteSpace($Plan.linkType) -and
                $Plan.linkType -match 'Junction'
            )
            {
                $ItemType = 'Junction'
            }


            New-Item `
                -ItemType $ItemType `
                -Path $Plan.location `
                -Target $Plan.target `
                -Force `
                -ErrorAction Stop |
                Out-Null


            $Result.recreated = $true


            Write-Host (
                '  [OK]   Recreated link: ' +
                $Plan.skill
            ) -ForegroundColor Green


            Write-Host (
                '         ' +
                $Plan.location +
                ' -> ' +
                $Plan.target
            ) -ForegroundColor DarkGray
        }
        catch
        {
            $Result.error = $_.Exception.Message


            Write-Host (
                '  [WARN] Failed to recreate link: ' +
                $Plan.skill
            ) -ForegroundColor Yellow


            Write-Host (
                '         ' +
                $_.Exception.Message
            ) -ForegroundColor Yellow
        }


        $Results += $Result
    }


    return $Results
}


# ============================================================
# Resolve snapshot path
# ============================================================

$From = Normalize-FullPath $From


if (-not (Test-Path $From))
{
    throw "Snapshot directory does not exist: $From"
}


$ManifestPath = Join-Path `
    $From `
    'manifest.json'


$InventoryPath = Join-Path `
    $From `
    'inventory.json'


if (-not (Test-Path $ManifestPath))
{
    throw "manifest.json does not exist: $ManifestPath"
}


# ============================================================
# Read manifest
# ============================================================

try
{
    $Manifest = Get-Content `
        -LiteralPath $ManifestPath `
        -Raw `
        -ErrorAction Stop |
        ConvertFrom-Json
}
catch
{
    throw (
        'Failed to read manifest.json: ' +
        $_.Exception.Message
    )
}


if ([int]$Manifest.schemaVersion -ne 4)
{
    throw (
        'Unsupported manifest schemaVersion: ' +
        $Manifest.schemaVersion +
        '. Expected: 4'
    )
}


if (
    [string]::IsNullOrWhiteSpace(
        [string]$Manifest.sourceUserProfile
    )
)
{
    throw 'manifest.sourceUserProfile is missing.'
}


if ($null -eq $Manifest.components)
{
    throw 'manifest.components is missing.'
}


# ============================================================
# Read optional inventory
# ============================================================

$Inventory = $null


if (Test-Path $InventoryPath)
{
    try
    {
        $Inventory = Get-Content `
            -LiteralPath $InventoryPath `
            -Raw `
            -ErrorAction Stop |
            ConvertFrom-Json
    }
    catch
    {
        Write-Host (
            '[WARN] inventory.json could not be read: ' +
            $_.Exception.Message
        ) -ForegroundColor Yellow
    }
}
else
{
    Write-Host (
        '[WARN] inventory.json is missing. Link reconstruction will be unavailable.'
    ) -ForegroundColor Yellow
}


# ============================================================
# Header
# ============================================================

Write-Host ''
Write-Host '========================================' -ForegroundColor Cyan
Write-Host ' DevManager Restore v4.1' -ForegroundColor Cyan
Write-Host '========================================' -ForegroundColor Cyan
Write-Host ''

Write-Host "Snapshot       : $From"
Write-Host "Snapshot stamp : $($Manifest.stamp)"
Write-Host "Source machine : $($Manifest.sourceMachine)"
Write-Host "Source profile : $($Manifest.sourceUserProfile)"
Write-Host "Current profile: $env:USERPROFILE"
Write-Host ''


# ============================================================
# Service safety check
# ============================================================

Write-Host '[1/5] Checking restore safety...' -ForegroundColor Cyan


$HeadroomRunning = Test-TcpPort `
    -HostName '127.0.0.1' `
    -Port 8787


$OmniRouteRunning = Test-TcpPort `
    -HostName '127.0.0.1' `
    -Port 20128


if ($HeadroomRunning)
{
    Write-Host (
        '  [WARN] Headroom :8787 is running.'
    ) -ForegroundColor Yellow
}
else
{
    Write-Host (
        '  [OK]   Headroom :8787 is stopped.'
    ) -ForegroundColor Green
}


if ($OmniRouteRunning)
{
    Write-Host (
        '  [WARN] OmniRoute :20128 is running.'
    ) -ForegroundColor Yellow
}
else
{
    Write-Host (
        '  [OK]   OmniRoute :20128 is stopped.'
    ) -ForegroundColor Green
}


# ============================================================
# Build restore plan
# ============================================================

Write-Host ''
Write-Host '[2/5] Building restore plan...' -ForegroundColor Cyan


$RestorePlan = @()
$SkippedComponents = @()
$PlanIndex = 0


foreach ($Component in @($Manifest.components))
{
    if (-not [bool]$Component.exists)
    {
        continue
    }


    if (-not [bool]$Component.copied)
    {
        Write-Host (
            '  [SKIP] Not successfully backed up: ' +
            $Component.name
        ) -ForegroundColor DarkGray

        continue
    }


    if (
        [string]$Component.category -eq 'machine' -and
        (-not $RestoreMachineSettings)
    )
    {
        $SkippedComponents += $Component


        Write-Host (
            '  [SKIP] Machine-specific component: ' +
            $Component.name
        ) -ForegroundColor DarkGray

        continue
    }


    $BackupPath = Join-Path `
        $From `
        ([string]$Component.backupName)


    if (-not (Test-Path $BackupPath))
    {
        throw (
            'Snapshot component is missing: ' +
            $BackupPath
        )
    }


    $Destination = Convert-RestorePath `
        -SourcePath ([string]$Component.source) `
        -SourceUserProfile ([string]$Manifest.sourceUserProfile) `
        -CurrentUserProfile $env:USERPROFILE


    $PlanIndex++


    $RestorePlan += [ordered]@{
        index = $PlanIndex

        name = [string]$Component.name

        category = [string]$Component.category

        type = [string]$Component.type

        sourceOriginal = [string]$Component.source

        backup = $BackupPath

        destination = $Destination

        destinationExists = (
            Test-Path $Destination
        )

        requiresStoppedService = (
            [bool]$Component.requiresStoppedService
        )
    }
}


if ($RestorePlan.Count -eq 0)
{
    throw 'No restorable components were found in the snapshot.'
}


foreach ($Plan in $RestorePlan)
{
    Write-Host (
        '  [' +
        $Plan.index +
        '] ' +
        $Plan.name
    ) -ForegroundColor White


    Write-Host (
        '      FROM: ' +
        $Plan.backup
    ) -ForegroundColor DarkGray


    Write-Host (
        '      TO  : ' +
        $Plan.destination
    ) -ForegroundColor DarkGray


    Write-Host (
        '      Current target exists: ' +
        $Plan.destinationExists
    ) -ForegroundColor DarkGray
}


# ============================================================
# Link plan
# ============================================================

$LinkPlan = @(
    Get-LinkRestorePlan `
        -Inventory $Inventory `
        -SourceUserProfile ([string]$Manifest.sourceUserProfile) `
        -CurrentUserProfile $env:USERPROFILE
)


Write-Host ''

if ($LinkPlan.Count -gt 0)
{
    Write-Host (
        '  Linked skill locations detected: ' +
        $LinkPlan.Count
    ) -ForegroundColor Cyan


    foreach ($Link in $LinkPlan)
    {
        Write-Host (
            '    ' +
            $Link.skill +
            ': ' +
            $Link.location +
            ' -> ' +
            $Link.target
        ) -ForegroundColor DarkGray
    }
}
else
{
    Write-Host (
        '  Linked skill locations detected: 0'
    ) -ForegroundColor DarkGray
}


# ============================================================
# Dry Run
# ============================================================

if (-not $Apply)
{
    Write-Host ''
    Write-Host '========================================' -ForegroundColor Cyan
    Write-Host ' DRY RUN RESULT' -ForegroundColor Cyan
    Write-Host '========================================' -ForegroundColor Cyan
    Write-Host ''

    Write-Host '[OK] Restore plan validated.' -ForegroundColor Green

    Write-Host ''
    Write-Host (
        'Components to restore : ' +
        $RestorePlan.Count
    )

    Write-Host (
        'Machine items skipped : ' +
        $SkippedComponents.Count
    )

    Write-Host (
        'Linked skill locations: ' +
        $LinkPlan.Count
    )


    Write-Host ''
    Write-Host 'Files changed:' -NoNewline
    Write-Host ' NO' -ForegroundColor Green


    Write-Host ''
    Write-Host 'Environment variables changed:' -NoNewline
    Write-Host ' NO' -ForegroundColor Green


    Write-Host ''
    Write-Host 'npm/pip packages installed:' -NoNewline
    Write-Host ' NO' -ForegroundColor Green


    if ($HeadroomRunning -or $OmniRouteRunning)
    {
        Write-Host ''

        Write-Host (
            '[WARN] Actual restore is currently blocked because Headroom and/or OmniRoute is running.'
        ) -ForegroundColor Yellow
    }


    Write-Host ''
    Write-Host 'To perform the actual restore:' -ForegroundColor Cyan

    Write-Host (
        'powershell.exe -ExecutionPolicy Bypass ' +
        '-File "' +
        $PSCommandPath +
        '" -From "' +
        $From +
        '" -Apply'
    )


    if ($RestoreMachineSettings)
    {
        Write-Host (
            'Machine-specific restore is enabled for this plan.'
        ) -ForegroundColor Yellow
    }
    else
    {
        Write-Host (
            'Machine-specific settings are excluded by default.'
        ) -ForegroundColor DarkGray
    }


    return
}


# ============================================================
# Actual restore safety gate
# ============================================================

Write-Host ''
Write-Host '[3/5] Validating actual restore...' -ForegroundColor Cyan


if ($HeadroomRunning)
{
    throw (
        'Actual restore blocked: Headroom is running on port 8787. ' +
        'Stop Headroom and retry.'
    )
}


if ($OmniRouteRunning)
{
    throw (
        'Actual restore blocked: OmniRoute is running on port 20128. ' +
        'Stop OmniRoute and retry.'
    )
}


Write-Host (
    '  [OK]   Required services are stopped.'
) -ForegroundColor Green


if (-not $Force)
{
    Write-Host ''

    Write-Host (
        'This will replace ' +
        $RestorePlan.Count +
        ' current configuration target(s).'
    ) -ForegroundColor Yellow


    Write-Host (
        'Existing targets will first be moved to a pre-restore backup.'
    ) -ForegroundColor Yellow


    $Answer = Read-Host 'Type yes to continue'


    if ($Answer -ne 'yes')
    {
        Write-Host ''
        Write-Host '[CANCEL] No files were changed.' -ForegroundColor Yellow

        return
    }
}


# ============================================================
# Prepare pre-restore backup
# ============================================================

$RestoreStamp = Get-Date -Format 'yyyy-MM-ddTHHmmss'

$SnapshotParent = Split-Path `
    -Path $From `
    -Parent


$PreRestoreRoot = Join-Path `
    $SnapshotParent `
    (
        'pre-restore-' +
        $RestoreStamp
    )


New-Item `
    -ItemType Directory `
    -Path $PreRestoreRoot `
    -Force |
    Out-Null


$MovedOriginals = @()
$AffectedTargets = @()


# ============================================================
# Move current configuration aside
# ============================================================

Write-Host ''
Write-Host '[4/5] Preserving current configuration...' -ForegroundColor Cyan


try
{
    foreach ($Plan in $RestorePlan)
    {
        $Record = [ordered]@{
            name = $Plan.name

            destination = $Plan.destination

            existedBefore = (
                Test-Path $Plan.destination
            )

            preRestorePath = $null

            moved = $false
        }


        if ($Record.existedBefore)
        {
            $BackupPath = Get-UniqueBackupPath `
                -BaseDirectory $PreRestoreRoot `
                -Name $Plan.name `
                -Index $Plan.index


            Ensure-ParentDirectory `
                -Path $BackupPath


            Move-Item `
                -LiteralPath $Plan.destination `
                -Destination $BackupPath `
                -Force `
                -ErrorAction Stop


            $Record.preRestorePath = $BackupPath
            $Record.moved = $true


            Write-Host (
                '  [OK]   ' +
                $Plan.name +
                ' -> ' +
                $BackupPath
            ) -ForegroundColor Green
        }
        else
        {
            Write-Host (
                '  [INFO] No current target: ' +
                $Plan.name
            ) -ForegroundColor DarkGray
        }


        $MovedOriginals += $Record
    }
}
catch
{
    $PreserveError = $_.Exception.Message


    Write-Host ''
    Write-Host (
        '[FAIL] Could not preserve current configuration.'
    ) -ForegroundColor Red


    Write-Host (
        '       ' +
        $PreserveError
    ) -ForegroundColor Red


    Write-Host (
        '[ROLLBACK] Restoring items already moved...'
    ) -ForegroundColor Yellow


    foreach ($Record in @($MovedOriginals | Select-Object -Reverse))
    {
        if (-not $Record.moved)
        {
            continue
        }


        try
        {
            Restore-OriginalItem `
                -BackupPath $Record.preRestorePath `
                -Destination $Record.destination


            Write-Host (
                '  [OK]   ' +
                $Record.name
            ) -ForegroundColor Green
        }
        catch
        {
            Write-Host (
                '  [FAIL] Rollback failed: ' +
                $Record.name +
                ': ' +
                $_.Exception.Message
            ) -ForegroundColor Red
        }
    }


    throw 'Restore aborted during pre-restore preservation.'
}


# ============================================================
# Restore snapshot
# ============================================================

Write-Host ''
Write-Host '[5/5] Restoring snapshot...' -ForegroundColor Cyan


$RestoreFailed = $false
$RestoreFailureMessage = $null


try
{
    foreach ($Plan in $RestorePlan)
    {
        $AffectedTargets += $Plan.destination


        Copy-RestoreItem `
            -Source $Plan.backup `
            -Destination $Plan.destination `
            -Type $Plan.type


        Write-Host (
            '  [OK]   ' +
            $Plan.name +
            ' -> ' +
            $Plan.destination
        ) -ForegroundColor Green
    }
}
catch
{
    $RestoreFailed = $true
    $RestoreFailureMessage = $_.Exception.Message
}


# ============================================================
# Rollback after restore failure
# ============================================================

if ($RestoreFailed)
{
    Write-Host ''
    Write-Host '[FAIL] Restore failed.' -ForegroundColor Red

    Write-Host (
        '       ' +
        $RestoreFailureMessage
    ) -ForegroundColor Red


    Write-Host ''
    Write-Host '[ROLLBACK] Removing restored targets...' -ForegroundColor Yellow


    foreach ($Plan in @($RestorePlan | Select-Object -Reverse))
    {
        try
        {
            if (Test-Path $Plan.destination)
            {
                Remove-RestoreItem `
                    -Path $Plan.destination


                Write-Host (
                    '  [OK]   Removed: ' +
                    $Plan.name
                ) -ForegroundColor Green
            }
        }
        catch
        {
            Write-Host (
                '  [WARN] Could not remove restored target: ' +
                $Plan.name +
                ': ' +
                $_.Exception.Message
            ) -ForegroundColor Yellow
        }
    }


    Write-Host ''
    Write-Host '[ROLLBACK] Restoring previous configuration...' -ForegroundColor Yellow


    $RollbackFailed = $false


    foreach ($Record in @($MovedOriginals | Select-Object -Reverse))
    {
        if (-not $Record.moved)
        {
            continue
        }


        try
        {
            Restore-OriginalItem `
                -BackupPath $Record.preRestorePath `
                -Destination $Record.destination


            Write-Host (
                '  [OK]   Restored previous: ' +
                $Record.name
            ) -ForegroundColor Green
        }
        catch
        {
            $RollbackFailed = $true


            Write-Host (
                '  [FAIL] Previous configuration rollback failed: ' +
                $Record.name +
                ': ' +
                $_.Exception.Message
            ) -ForegroundColor Red
        }
    }


    Write-Host ''


    if ($RollbackFailed)
    {
        Write-Host (
            '[CRITICAL] Restore failed and rollback was incomplete.'
        ) -ForegroundColor Red

        Write-Host (
            '           Manual recovery data: ' +
            $PreRestoreRoot
        ) -ForegroundColor Red

        exit 3
    }


    Write-Host (
        '[OK] Previous configuration was restored successfully.'
    ) -ForegroundColor Green


    throw 'Snapshot restore failed. Automatic rollback completed.'
}


# ============================================================
# Optional linked skill reconstruction
# ============================================================

$LinkResults = @()


if ($RecreateLinks)
{
    Write-Host ''
    Write-Host 'Recreating linked skills...' -ForegroundColor Cyan


    $LinkResults = @(
        Recreate-SkillLinks `
            -Plans $LinkPlan
    )
}
elseif ($LinkPlan.Count -gt 0)
{
    Write-Host ''
    Write-Host (
        '[INFO] Linked skill metadata exists, but links were not recreated.'
    ) -ForegroundColor DarkGray

    Write-Host (
        '       Use -RecreateLinks if the link targets already exist on this machine.'
    ) -ForegroundColor DarkGray
}


# ============================================================
# Final verification
# ============================================================

$VerificationFailures = @()


foreach ($Plan in $RestorePlan)
{
    if (-not (Test-Path $Plan.destination))
    {
        $VerificationFailures += $Plan
    }
}


Write-Host ''
Write-Host '========================================' -ForegroundColor Cyan
Write-Host ' Restore Result' -ForegroundColor Cyan
Write-Host '========================================' -ForegroundColor Cyan
Write-Host ''


if ($VerificationFailures.Count -gt 0)
{
    Write-Host (
        '[WARN] Restore completed but verification found missing targets.'
    ) -ForegroundColor Yellow


    foreach ($Failure in $VerificationFailures)
    {
        Write-Host (
            '       ' +
            $Failure.name +
            ': ' +
            $Failure.destination
        ) -ForegroundColor Yellow
    }
}
else
{
    Write-Host (
        '[OK] Snapshot restored successfully.'
    ) -ForegroundColor Green
}


Write-Host ''
Write-Host "Snapshot            : $From"
Write-Host "Pre-restore backup  : $PreRestoreRoot"
Write-Host "Components restored : $($RestorePlan.Count)"


Write-Host ''
Write-Host 'Environment variables changed:' -NoNewline
Write-Host ' NO' -ForegroundColor Green


Write-Host ''
Write-Host 'npm/pip packages installed:' -NoNewline
Write-Host ' NO' -ForegroundColor Green


if (-not $RestoreMachineSettings)
{
    Write-Host ''
    Write-Host (
        'Machine-specific settings restored: NO'
    ) -ForegroundColor Green
}
else
{
    Write-Host ''
    Write-Host (
        'Machine-specific settings restore was ENABLED.'
    ) -ForegroundColor Yellow
}


Write-Host ''
Write-Host 'IMPORTANT:' -ForegroundColor Yellow

Write-Host (
    '  The pre-restore backup was intentionally kept.'
)

Write-Host (
    '  Do not delete it until Claude, Codex, Headroom and OmniRoute are verified.'
)

Write-Host (
    '  Restart Headroom and OmniRoute before testing the restored environment.'
)