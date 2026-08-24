#requires -Version 5.1
param(
    [string]$ClientRoot = "D:\WOW",
    [string]$PatchedSpellDbc = "C:\Users\Administrator\Downloads\workspace\uploads\G17R1_Client_Patch_Staging\DBFilesClient\Spell.dbc",
    [string]$ServerSpellDbc = "D:\TC-Build\bin\RelWithDebInfo\dbc\Spell.dbc"
)

$ErrorActionPreference = "Stop"
$Workspace = "C:\Users\Administrator\Downloads\workspace"
$UploadDir = Join-Path $Workspace "uploads"
$RunReport = Join-Path $UploadDir "G17R1_CLIENT_MPQ_INSTALL_RESULT.txt"
$StateFile = Join-Path $UploadDir "G17R1_CLIENT_MPQ_INSTALL_STATE.txt"
$Tool = Join-Path $PSScriptRoot "third_party\mpqcli-windows-amd64.exe"
$ExpectedToolHash = "5dc56b130432098d18e911fdf4b3464adf850432b7fbb35003a2feff7c30a79f"
$ExpectedPatchedHash = "dd25091167f671764735ce88c78b66485c6d661fadf05d322574c261f6e464ea"
$ExpectedPatchedSize = 48956359
$ExpectedServerHash = "df44e75ef1730e363dc06f1bc5ae064299b08d2d0047e663c0a1782ed4c8d10f"
$ArchiveTarget = "DBFilesClient\Spell.dbc"
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$InstalledThisRun = $false
$InstalledTarget = ""
$InstalledArchiveHash = ""
$TemporaryTarget = ""
$WorkRoot = ""

New-Item -ItemType Directory -Path $UploadDir -Force | Out-Null
[System.IO.File]::WriteAllText($RunReport, "", $Utf8NoBom)

function Write-Result([string]$Line) {
    Write-Host $Line
    [System.IO.File]::AppendAllText($RunReport, $Line + [Environment]::NewLine, $Utf8NoBom)
}

function Invoke-NativeCapture {
    param(
        [Parameter(Mandatory=$true)][string]$FilePath,
        [Parameter(Mandatory=$true)][string[]]$NativeArgs
    )
    $SavedErrorActionPreference = $ErrorActionPreference
    $NativeExit = 9009
    $NativeOutput = @()
    try {
        $ErrorActionPreference = "Continue"
        $NativeOutput = @(& $FilePath @NativeArgs 2>&1)
        $NativeExit = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $SavedErrorActionPreference
    }
    return [pscustomobject]@{ ExitCode = [int]$NativeExit; Lines = @($NativeOutput) }
}

function Invoke-NativeLogged {
    param(
        [Parameter(Mandatory=$true)][string]$FilePath,
        [Parameter(Mandatory=$true)][string[]]$NativeArgs,
        [Parameter(Mandatory=$true)][string]$Prefix
    )
    $Result = Invoke-NativeCapture -FilePath $FilePath -NativeArgs $NativeArgs
    foreach ($NativeLine in $Result.Lines) {
        Write-Result ($Prefix + "|" + $NativeLine.ToString())
    }
    return [int]$Result.ExitCode
}

function Read-KeyValueFile([string]$Path) {
    $Values = @{}
    foreach ($Line in [System.IO.File]::ReadAllLines($Path)) {
        if ($Line -match '^([^=]+)=(.*)$') {
            $Values[$Matches[1]] = $Matches[2]
        }
    }
    return $Values
}

function Get-ArchiveTargetProbe([string]$ArchivePath) {
    $ListResult = Invoke-NativeCapture -FilePath $Tool -NativeArgs @("list", $ArchivePath)
    if ($ListResult.ExitCode -ne 0) {
        return [pscustomobject]@{ ExitCode = [int]$ListResult.ExitCode; Hits = 0 }
    }

    $ProbeRoot = Join-Path $WorkRoot ("higher_target_probe_" + [Guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Path $ProbeRoot | Out-Null
    try {
        $ExtractResult = Invoke-NativeCapture -FilePath $Tool -NativeArgs @("extract", "--output", $ProbeRoot, "--keep", "--file", $ArchiveTarget, $ArchivePath)
        $ExtractedTarget = Join-Path $ProbeRoot "DBFilesClient\Spell.dbc"
        $TargetExists = Test-Path -LiteralPath $ExtractedTarget -PathType Leaf
        if ($ExtractResult.ExitCode -eq 0 -and $TargetExists) {
            return [pscustomobject]@{ ExitCode = 0; Hits = 1 }
        }
        $ExtractText = (($ExtractResult.Lines | ForEach-Object { $_.ToString() }) -join [Environment]::NewLine)
        if ($ExtractResult.ExitCode -ne 0 -and -not $TargetExists -and $ExtractText.Contains("File doesn't exist")) {
            return [pscustomobject]@{ ExitCode = 0; Hits = 0 }
        }
        return [pscustomobject]@{ ExitCode = [int]$ExtractResult.ExitCode; Hits = 0 }
    }
    finally {
        Remove-Item -LiteralPath $ProbeRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}

function Assert-ArchiveContent([string]$ArchivePath, [string]$ExtractRoot, [string]$Prefix) {
    $ListResult = Invoke-NativeCapture -FilePath $Tool -NativeArgs @("list", $ArchivePath)
    if ($ListResult.ExitCode -ne 0) {
        foreach ($Line in $ListResult.Lines) { Write-Result ($Prefix + "_LIST|" + $Line.ToString()) }
        throw "mpqcli list failed for archive: $ArchivePath"
    }
    $TargetHits = 0
    foreach ($Line in $ListResult.Lines) {
        $Normalized = $Line.ToString().Trim().Replace("/", "\")
        if ($Normalized -ieq $ArchiveTarget) { $TargetHits++ }
    }
    Write-Result ($Prefix + "_LIST_TARGET_HITS=" + $TargetHits)
    if ($TargetHits -ne 1) { throw "archive must contain exactly one DBFilesClient\Spell.dbc; hits=$TargetHits" }

    $FormatResult = Invoke-NativeCapture -FilePath $Tool -NativeArgs @("info", "--property", "format-version", $ArchivePath)
    if ($FormatResult.ExitCode -ne 0) { throw "mpqcli info format-version failed" }
    $FormatText = (($FormatResult.Lines | ForEach-Object { $_.ToString().Trim() }) -join "")
    Write-Result ($Prefix + "_FORMAT_VERSION=" + $FormatText)
    if ($FormatText -ne "2") { throw "unexpected WotLK MPQ format version: $FormatText" }

    $FileCountResult = Invoke-NativeCapture -FilePath $Tool -NativeArgs @("info", "--property", "file-count", $ArchivePath)
    if ($FileCountResult.ExitCode -ne 0) { throw "mpqcli info file-count failed" }
    $FileCountText = (($FileCountResult.Lines | ForEach-Object { $_.ToString().Trim() }) -join "")
    Write-Result ($Prefix + "_FILE_COUNT=" + $FileCountText)
    if ($FileCountText -ne "3") { throw "archive must contain target plus listfile and attributes; file-count=$FileCountText" }

    New-Item -ItemType Directory -Path $ExtractRoot -Force | Out-Null
    $ExtractExit = Invoke-NativeLogged -FilePath $Tool -NativeArgs @("extract", "--output", $ExtractRoot, "--keep", "--file", $ArchiveTarget, $ArchivePath) -Prefix ($Prefix + "_EXTRACT")
    Write-Result ($Prefix + "_EXTRACT_EXIT=" + $ExtractExit)
    if ($ExtractExit -ne 0) { throw "mpqcli extract failed; exit=$ExtractExit" }
    $ExtractedDbc = Join-Path $ExtractRoot "DBFilesClient\Spell.dbc"
    if (-not (Test-Path -LiteralPath $ExtractedDbc -PathType Leaf)) { throw "extracted Spell.dbc missing" }
    $ExtractedHash = (Get-FileHash -LiteralPath $ExtractedDbc -Algorithm SHA256).Hash.ToLowerInvariant()
    $ExtractedSize = (Get-Item -LiteralPath $ExtractedDbc).Length
    Write-Result ($Prefix + "_EXTRACTED_DBC_SIZE=" + $ExtractedSize)
    Write-Result ($Prefix + "_EXTRACTED_DBC_SHA256=" + $ExtractedHash)
    if ($ExtractedHash -cne $ExpectedPatchedHash -or $ExtractedSize -ne $ExpectedPatchedSize) {
        throw "extracted client DBC does not match the verified G17-R1 staging file"
    }
}

try {
    Write-Result "G17R1_CLIENT_MPQ_INSTALL_START"
    Write-Result "CLIENT_ROOT_REQUESTED=$ClientRoot"
    Write-Result "PATCHED_SPELL_DBC=$PatchedSpellDbc"
    Write-Result "SERVER_SPELL_DBC=$ServerSpellDbc"

    $RunningWow = @(Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Name -match '^(?i:wow|wow-64)$' })
    if ($RunningWow.Count -ne 0) { throw "Wow client is running; close it before installing the MPQ" }

    foreach ($RequiredFile in @($Tool, $PatchedSpellDbc, $ServerSpellDbc)) {
        if (-not (Test-Path -LiteralPath $RequiredFile -PathType Leaf)) { throw "required file missing: $RequiredFile" }
    }

    $ToolHash = (Get-FileHash -LiteralPath $Tool -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-Result "MPQCLI_SHA256=$ToolHash"
    if ($ToolHash -cne $ExpectedToolHash) { throw "bundled mpqcli hash mismatch" }
    $ToolVersionExit = Invoke-NativeLogged -FilePath $Tool -NativeArgs @("version") -Prefix "MPQCLI_VERSION"
    Write-Result "MPQCLI_VERSION_EXIT=$ToolVersionExit"
    if ($ToolVersionExit -ne 0) { throw "bundled mpqcli cannot start; exit=$ToolVersionExit" }

    $PatchedHash = (Get-FileHash -LiteralPath $PatchedSpellDbc -Algorithm SHA256).Hash.ToLowerInvariant()
    $PatchedSize = (Get-Item -LiteralPath $PatchedSpellDbc).Length
    $ServerHashBefore = (Get-FileHash -LiteralPath $ServerSpellDbc -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-Result "PATCHED_DBC_SIZE=$PatchedSize"
    Write-Result "PATCHED_DBC_SHA256=$PatchedHash"
    Write-Result "SERVER_DBC_SHA256_BEFORE=$ServerHashBefore"
    if ($PatchedHash -cne $ExpectedPatchedHash -or $PatchedSize -ne $ExpectedPatchedSize) {
        throw "staged client Spell.dbc is not the exact verified G17-R1 output"
    }
    if ($ServerHashBefore -cne $ExpectedServerHash) { throw "server Spell.dbc has changed; refusing to continue" }

    if (-not (Test-Path -LiteralPath $ClientRoot -PathType Container)) { throw "client root missing: $ClientRoot" }
    $ClientRoot = (Resolve-Path -LiteralPath $ClientRoot).Path
    $WowExe = Join-Path $ClientRoot "Wow.exe"
    $DataDir = Join-Path $ClientRoot "Data"
    if (-not (Test-Path -LiteralPath $WowExe -PathType Leaf)) { throw "Wow.exe missing: $WowExe" }
    if (-not (Test-Path -LiteralPath $DataDir -PathType Container)) { throw "Data directory missing: $DataDir" }
    $WowHash = (Get-FileHash -LiteralPath $WowExe -Algorithm SHA256).Hash.ToLowerInvariant()
    $WowVersion = (Get-Item -LiteralPath $WowExe).VersionInfo.FileVersion
    Write-Result "CLIENT_ROOT=$ClientRoot"
    Write-Result "WOW_EXE_SHA256=$WowHash"
    Write-Result "WOW_EXE_FILE_VERSION=$WowVersion"

    $ConfigWtf = Join-Path $ClientRoot "WTF\Config.wtf"
    $Locale = "UNKNOWN"
    if (Test-Path -LiteralPath $ConfigWtf -PathType Leaf) {
        foreach ($Line in [System.IO.File]::ReadAllLines($ConfigWtf)) {
            if ($Line -match '^SET\s+locale\s+"([A-Za-z]{4})"') { $Locale = $Matches[1]; break }
        }
    }
    Write-Result "CLIENT_LOCALE=$Locale"
    if ($Locale -ne "UNKNOWN") {
        $LocaleDir = Join-Path $DataDir $Locale
        Write-Result ("CLIENT_LOCALE_DIR_EXISTS=" + (Test-Path -LiteralPath $LocaleDir -PathType Container))
    }

    $Stamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $WorkRoot = Join-Path $UploadDir ("G17R1_Client_MPQ_Work_" + $Stamp + "_" + $PID)
    if (Test-Path -LiteralPath $WorkRoot) { throw "work directory already exists: $WorkRoot" }
    New-Item -ItemType Directory -Path $WorkRoot | Out-Null

    if (Test-Path -LiteralPath $StateFile -PathType Leaf) {
        $State = Read-KeyValueFile $StateFile
        if ($State["INSTALL_STATUS"] -cne "PASS") { throw "existing install state is not PASS; manual review required: $StateFile" }
        if (-not $State.ContainsKey("CLIENT_ROOT") -or $State["CLIENT_ROOT"] -ine $ClientRoot) {
            throw "existing install state belongs to a different client root"
        }
        $CurrentTarget = $State["INSTALLED_MPQ"]
        if (-not (Test-Path -LiteralPath $CurrentTarget -PathType Leaf)) { throw "state exists but installed MPQ is missing: $CurrentTarget" }
        $CurrentHash = (Get-FileHash -LiteralPath $CurrentTarget -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($CurrentHash -cne $State["INSTALLED_MPQ_SHA256"]) { throw "installed MPQ changed after G17-R1 install" }
        Assert-ArchiveContent -ArchivePath $CurrentTarget -ExtractRoot (Join-Path $WorkRoot "already_current_extract") -Prefix "ALREADY_CURRENT"
        $ServerHashAfterCurrent = (Get-FileHash -LiteralPath $ServerSpellDbc -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($ServerHashAfterCurrent -cne $ExpectedServerHash) { throw "server DBC changed during verification" }
        Write-Result "SERVER_DBC_SHA256_AFTER=$ServerHashAfterCurrent"
        Write-Result "SERVER_DBC_MODIFIED=False"
        Write-Result "G17R1_CLIENT_MPQ_INSTALL=ALREADY_CURRENT"
        Write-Result "G17R1_CLIENT_MPQ_INSTALL_RESULT=PASS"
        Write-Result "RESULT_FILE=$RunReport"
        exit 0
    }

    $Slots = @("Z","Y","X","W","V","U","T","S","R","Q","P","O","N","M","L","K","J","I","H","G","F","E","D","C","B","A")
    $SelectedSlot = ""
    $SelectedTarget = ""
    foreach ($Slot in $Slots) {
        $Candidate = Join-Path $DataDir ("patch-" + $Slot + ".MPQ")
        if (-not (Test-Path -LiteralPath $Candidate)) {
            $SelectedSlot = $Slot
            $SelectedTarget = $Candidate
            break
        }
        if (Test-Path -LiteralPath $Candidate -PathType Container) {
            $LooseTarget = Join-Path $Candidate "DBFilesClient\Spell.dbc"
            $LooseHit = [int](Test-Path -LiteralPath $LooseTarget -PathType Leaf)
            Write-Result ("HIGHER_SLOT=" + $Slot + "; TYPE=DIRECTORY; SPELL_DBC_HITS=" + $LooseHit + "; PATH=" + $Candidate)
            if ($LooseHit -ne 0) { throw "higher-priority loose patch already contains Spell.dbc: $Candidate" }
        }
        elseif (Test-Path -LiteralPath $Candidate -PathType Leaf) {
            $Probe = Get-ArchiveTargetProbe $Candidate
            Write-Result ("HIGHER_SLOT=" + $Slot + "; TYPE=PACKED_MPQ; PROBE_EXIT=" + $Probe.ExitCode + "; SPELL_DBC_HITS=" + $Probe.Hits + "; PATH=" + $Candidate)
            if ($Probe.ExitCode -ne 0) { throw "cannot inspect higher-priority custom MPQ safely: $Candidate" }
            if ($Probe.Hits -ne 0) { throw "higher-priority custom MPQ already contains Spell.dbc: $Candidate" }
        }
        else {
            throw "occupied patch slot is neither a regular file nor a directory: $Candidate"
        }
    }
    if (-not $SelectedSlot) { throw "no free patch-A through patch-Z slot is available" }
    Write-Result "SELECTED_PATCH_SLOT=$SelectedSlot"
    Write-Result "SELECTED_PATCH_PATH=$SelectedTarget"
    Write-Result "TARGET_PREIMAGE=ABSENT"

    $PackRoot = Join-Path $WorkRoot "pack_root"
    $PackDbc = Join-Path $PackRoot "DBFilesClient\Spell.dbc"
    New-Item -ItemType Directory -Path (Split-Path -Parent $PackDbc) -Force | Out-Null
    Copy-Item -LiteralPath $PatchedSpellDbc -Destination $PackDbc
    $PackHash = (Get-FileHash -LiteralPath $PackDbc -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($PackHash -cne $ExpectedPatchedHash) { throw "pack-root DBC copy hash mismatch" }

    $BuildArchive = Join-Path $WorkRoot ("patch-" + $SelectedSlot + ".MPQ")
    $CreateExit = Invoke-NativeLogged -FilePath $Tool -NativeArgs @("create", "--game", "wow-wotlk", "--output", $BuildArchive, $PackRoot) -Prefix "MPQ_CREATE"
    Write-Result "MPQ_CREATE_EXIT=$CreateExit"
    if ($CreateExit -ne 0) { throw "MPQ creation failed; exit=$CreateExit" }
    if (-not (Test-Path -LiteralPath $BuildArchive -PathType Leaf)) { throw "MPQ creation returned success but output is missing" }
    $BuildArchiveHash = (Get-FileHash -LiteralPath $BuildArchive -Algorithm SHA256).Hash.ToLowerInvariant()
    $BuildArchiveSize = (Get-Item -LiteralPath $BuildArchive).Length
    Write-Result "BUILT_MPQ_SIZE=$BuildArchiveSize"
    Write-Result "BUILT_MPQ_SHA256=$BuildArchiveHash"
    Assert-ArchiveContent -ArchivePath $BuildArchive -ExtractRoot (Join-Path $WorkRoot "build_verify_extract") -Prefix "BUILT_MPQ"

    if (Test-Path -LiteralPath $SelectedTarget) { throw "selected patch target became occupied before install: $SelectedTarget" }
    $TemporaryTarget = $SelectedTarget + ".g17r1.tmp"
    if (Test-Path -LiteralPath $TemporaryTarget) { throw "temporary client target already exists: $TemporaryTarget" }
    Copy-Item -LiteralPath $BuildArchive -Destination $TemporaryTarget
    $TemporaryHash = (Get-FileHash -LiteralPath $TemporaryTarget -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($TemporaryHash -cne $BuildArchiveHash) {
        Remove-Item -LiteralPath $TemporaryTarget -Force -ErrorAction SilentlyContinue
        throw "temporary client MPQ hash mismatch"
    }
    Move-Item -LiteralPath $TemporaryTarget -Destination $SelectedTarget
    $InstalledThisRun = $true
    $InstalledTarget = $SelectedTarget
    $InstalledArchiveHash = $BuildArchiveHash

    $InstalledHash = (Get-FileHash -LiteralPath $SelectedTarget -Algorithm SHA256).Hash.ToLowerInvariant()
    $InstalledSize = (Get-Item -LiteralPath $SelectedTarget).Length
    Write-Result "INSTALLED_MPQ_SIZE=$InstalledSize"
    Write-Result "INSTALLED_MPQ_SHA256=$InstalledHash"
    if ($InstalledHash -cne $BuildArchiveHash) { throw "installed MPQ hash mismatch" }
    Assert-ArchiveContent -ArchivePath $SelectedTarget -ExtractRoot (Join-Path $WorkRoot "installed_verify_extract") -Prefix "INSTALLED_MPQ"

    $ServerHashAfter = (Get-FileHash -LiteralPath $ServerSpellDbc -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-Result "SERVER_DBC_SHA256_AFTER=$ServerHashAfter"
    if ($ServerHashAfter -cne $ServerHashBefore -or $ServerHashAfter -cne $ExpectedServerHash) {
        throw "server Spell.dbc changed during client patch installation"
    }

    $StateLines = @(
        "STATE_FORMAT=1",
        "INSTALL_STATUS=PASS",
        ("CLIENT_ROOT=" + $ClientRoot),
        ("INSTALLED_MPQ=" + $SelectedTarget),
        ("INSTALLED_MPQ_SHA256=" + $InstalledHash),
        ("INSTALLED_MPQ_SIZE=" + $InstalledSize),
        ("PATCH_SLOT=" + $SelectedSlot),
        ("PATCHED_DBC_SHA256=" + $ExpectedPatchedHash),
        ("SERVER_DBC_SHA256=" + $ExpectedServerHash),
        "TARGET_PREIMAGE=ABSENT",
        ("INSTALLED_AT=" + (Get-Date).ToString("o"))
    )
    $StateTemporary = $StateFile + ".tmp"
    if (Test-Path -LiteralPath $StateTemporary) { throw "temporary state file already exists: $StateTemporary" }
    [System.IO.File]::WriteAllText($StateTemporary, (($StateLines -join [Environment]::NewLine) + [Environment]::NewLine), $Utf8NoBom)
    Move-Item -LiteralPath $StateTemporary -Destination $StateFile

    Write-Result "INSTALL_STATE_FILE=$StateFile"
    Write-Result "SERVER_DBC_MODIFIED=False"
    Write-Result "CLIENT_RESTART_REQUIRED=True"
    Write-Result "G17R1_CLIENT_MPQ_INSTALL=PASS"
    Write-Result "G17R1_CLIENT_MPQ_INSTALL_RESULT=PASS"
    Write-Result "RESULT_FILE=$RunReport"
    exit 0
}
catch {
    $ErrorMessage = $_.Exception.Message
    if ($TemporaryTarget -and (Test-Path -LiteralPath $TemporaryTarget -PathType Leaf)) {
        try {
            $TemporaryRescue = Join-Path $WorkRoot ("FAILED_TEMP_" + (Split-Path -Leaf $TemporaryTarget))
            Move-Item -LiteralPath $TemporaryTarget -Destination $TemporaryRescue
            Write-Result "TEMP_TARGET_CLEANUP=PASS"
            Write-Result "TEMP_TARGET_RESCUE_PATH=$TemporaryRescue"
        }
        catch {
            Write-Result ("TEMP_TARGET_CLEANUP=FAIL; ERROR=" + $_.Exception.Message)
        }
    }
    if ($InstalledThisRun -and $InstalledTarget -and (Test-Path -LiteralPath $InstalledTarget -PathType Leaf)) {
        try {
            $CurrentInstalledHash = (Get-FileHash -LiteralPath $InstalledTarget -Algorithm SHA256).Hash.ToLowerInvariant()
            if ($CurrentInstalledHash -ceq $InstalledArchiveHash) {
                $RescuePath = Join-Path $WorkRoot ("FAILED_INSTALL_ROLLED_BACK_" + (Split-Path -Leaf $InstalledTarget))
                Move-Item -LiteralPath $InstalledTarget -Destination $RescuePath
                Write-Result "AUTO_ROLLBACK_AFTER_FAILURE=PASS"
                Write-Result "AUTO_ROLLBACK_RESCUE_PATH=$RescuePath"
            }
            else {
                Write-Result "AUTO_ROLLBACK_AFTER_FAILURE=SKIPPED_HASH_CHANGED"
            }
        }
        catch {
            Write-Result ("AUTO_ROLLBACK_AFTER_FAILURE=FAIL; ERROR=" + $_.Exception.Message)
        }
    }
    Write-Result ("G17R1_CLIENT_MPQ_INSTALL_ERROR=" + $ErrorMessage)
    Write-Result "SERVER_DBC_WRITE_ATTEMPTED=False"
    Write-Result "G17R1_CLIENT_MPQ_INSTALL_RESULT=FAIL"
    Write-Result "RESULT_FILE=$RunReport"
    exit 1
}
