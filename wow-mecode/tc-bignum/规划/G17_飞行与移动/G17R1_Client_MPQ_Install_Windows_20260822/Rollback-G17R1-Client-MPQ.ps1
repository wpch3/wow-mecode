#requires -Version 5.1
param(
    [string]$ClientRoot = "D:\WOW"
)

$ErrorActionPreference = "Stop"
$Workspace = "C:\Users\Administrator\Downloads\workspace"
$UploadDir = Join-Path $Workspace "uploads"
$StateFile = Join-Path $UploadDir "G17R1_CLIENT_MPQ_INSTALL_STATE.txt"
$RunReport = Join-Path $UploadDir "G17R1_CLIENT_MPQ_ROLLBACK_RESULT.txt"
$ExpectedPatchedHash = "dd25091167f671764735ce88c78b66485c6d661fadf05d322574c261f6e464ea"
$ExpectedServerHash = "df44e75ef1730e363dc06f1bc5ae064299b08d2d0047e663c0a1782ed4c8d10f"
$ServerSpellDbc = "D:\TC-Build\bin\RelWithDebInfo\dbc\Spell.dbc"
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)

New-Item -ItemType Directory -Path $UploadDir -Force | Out-Null
[System.IO.File]::WriteAllText($RunReport, "", $Utf8NoBom)

function Write-Result([string]$Line) {
    Write-Host $Line
    [System.IO.File]::AppendAllText($RunReport, $Line + [Environment]::NewLine, $Utf8NoBom)
}

function Read-KeyValueFile([string]$Path) {
    $Values = @{}
    foreach ($Line in [System.IO.File]::ReadAllLines($Path)) {
        if ($Line -match '^([^=]+)=(.*)$') { $Values[$Matches[1]] = $Matches[2] }
    }
    return $Values
}

try {
    Write-Result "G17R1_CLIENT_MPQ_ROLLBACK_START"
    $RunningWow = @(Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Name -match '^(?i:wow|wow-64)$' })
    if ($RunningWow.Count -ne 0) { throw "Wow client is running; close it before rollback" }
    if (-not (Test-Path -LiteralPath $StateFile -PathType Leaf)) { throw "install state file missing: $StateFile" }
    if (-not (Test-Path -LiteralPath $ClientRoot -PathType Container)) { throw "client root missing: $ClientRoot" }
    if (-not (Test-Path -LiteralPath $ServerSpellDbc -PathType Leaf)) { throw "server Spell.dbc missing: $ServerSpellDbc" }

    $ClientRoot = (Resolve-Path -LiteralPath $ClientRoot).Path
    $State = Read-KeyValueFile $StateFile
    foreach ($Key in @("STATE_FORMAT","INSTALL_STATUS","CLIENT_ROOT","INSTALLED_MPQ","INSTALLED_MPQ_SHA256","PATCH_SLOT","PATCHED_DBC_SHA256","SERVER_DBC_SHA256","TARGET_PREIMAGE")) {
        if (-not $State.ContainsKey($Key)) { throw "state file missing key: $Key" }
    }
    if ($State["STATE_FORMAT"] -cne "1" -or $State["INSTALL_STATUS"] -cne "PASS") { throw "install state is not an active PASS state" }
    if ($State["CLIENT_ROOT"] -ine $ClientRoot) { throw "state belongs to a different client root" }
    if ($State["PATCHED_DBC_SHA256"] -cne $ExpectedPatchedHash) { throw "state patched DBC hash mismatch" }
    if ($State["SERVER_DBC_SHA256"] -cne $ExpectedServerHash) { throw "state server DBC hash mismatch" }
    if ($State["TARGET_PREIMAGE"] -cne "ABSENT") { throw "rollback only supports the non-overwrite absent-target policy" }

    $InstalledMpq = [System.IO.Path]::GetFullPath($State["INSTALLED_MPQ"])
    $PathSeparators = [char[]]@([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar)
    $ExpectedDataPrefix = [System.IO.Path]::GetFullPath((Join-Path $ClientRoot "Data")).TrimEnd($PathSeparators) + [System.IO.Path]::DirectorySeparatorChar
    if (-not $InstalledMpq.StartsWith($ExpectedDataPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "state target is outside the selected client Data directory"
    }
    if ((Split-Path -Leaf $InstalledMpq) -notmatch '^(?i:patch-[A-Z]\.MPQ)$') { throw "state target is not a permitted lettered patch MPQ" }
    if (-not (Test-Path -LiteralPath $InstalledMpq -PathType Leaf)) { throw "installed MPQ missing: $InstalledMpq" }

    $InstalledHash = (Get-FileHash -LiteralPath $InstalledMpq -Algorithm SHA256).Hash.ToLowerInvariant()
    $ServerHashBefore = (Get-FileHash -LiteralPath $ServerSpellDbc -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-Result "CLIENT_ROOT=$ClientRoot"
    Write-Result "INSTALLED_MPQ=$InstalledMpq"
    Write-Result "INSTALLED_MPQ_SHA256=$InstalledHash"
    Write-Result "SERVER_DBC_SHA256_BEFORE=$ServerHashBefore"
    if ($InstalledHash -cne $State["INSTALLED_MPQ_SHA256"]) { throw "installed MPQ changed; refusing rollback" }
    if ($ServerHashBefore -cne $ExpectedServerHash) { throw "server Spell.dbc changed; refusing rollback" }

    $Confirmation = Read-Host "Type ROLLBACK to remove the state-owned G17-R1 MPQ from the client"
    if ($Confirmation -cne "ROLLBACK") { throw "rollback confirmation was not provided" }

    $Stamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $BackupRoot = Join-Path $UploadDir ("G17R1_Client_MPQ_Rollback_Backup_" + $Stamp)
    if (Test-Path -LiteralPath $BackupRoot) { throw "rollback backup directory already exists: $BackupRoot" }
    New-Item -ItemType Directory -Path $BackupRoot | Out-Null
    $BackupMpq = Join-Path $BackupRoot (Split-Path -Leaf $InstalledMpq)
    $BackupState = Join-Path $BackupRoot "G17R1_CLIENT_MPQ_INSTALL_STATE.txt"

    Copy-Item -LiteralPath $StateFile -Destination $BackupState
    Move-Item -LiteralPath $InstalledMpq -Destination $BackupMpq
    $MovedHash = (Get-FileHash -LiteralPath $BackupMpq -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($MovedHash -cne $InstalledHash) {
        Move-Item -LiteralPath $BackupMpq -Destination $InstalledMpq
        throw "rollback backup hash mismatch; installed MPQ was restored"
    }
    try {
        Remove-Item -LiteralPath $StateFile
    }
    catch {
        Move-Item -LiteralPath $BackupMpq -Destination $InstalledMpq
        Remove-Item -LiteralPath $BackupState -Force -ErrorAction SilentlyContinue
        throw "could not retire active state; installed MPQ was restored"
    }

    if (Test-Path -LiteralPath $InstalledMpq) { throw "client MPQ still exists after rollback" }
    if (Test-Path -LiteralPath $StateFile) { throw "active state file still exists after rollback" }
    $ServerHashAfter = (Get-FileHash -LiteralPath $ServerSpellDbc -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($ServerHashAfter -cne $ServerHashBefore) { throw "server Spell.dbc changed during rollback" }

    Write-Result "ROLLBACK_BACKUP_MPQ=$BackupMpq"
    Write-Result "ROLLBACK_BACKUP_STATE=$BackupState"
    Write-Result "TARGET_POSTIMAGE=ABSENT"
    Write-Result "SERVER_DBC_SHA256_AFTER=$ServerHashAfter"
    Write-Result "SERVER_DBC_MODIFIED=False"
    Write-Result "G17R1_CLIENT_MPQ_ROLLBACK_RESULT=PASS"
    Write-Result "RESULT_FILE=$RunReport"
    exit 0
}
catch {
    Write-Result ("G17R1_CLIENT_MPQ_ROLLBACK_ERROR=" + $_.Exception.Message)
    Write-Result "G17R1_CLIENT_MPQ_ROLLBACK_RESULT=FAIL"
    Write-Result "RESULT_FILE=$RunReport"
    exit 1
}
