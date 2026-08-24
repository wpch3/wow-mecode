#requires -Version 5.1
# G17-C1: unlock spell 52226 in the CLIENT Spell.dbc (custom MPQ chain).
# Prequisites: R4 (patch-Z.MPQ owns the DBC archive) and R5 (zhCN Y mirror).
# Pipeline: extract Spell.dbc + AreaTable.dbc from patch-Z.MPQ ->
#           patch 52226 gates -> create new patch-Z.MPQ -> swap ->
#           byte-copy to patch-zhCN-Y.MPQ (R5 mirror contract) -> clear cache.
param(
    [string]$Workspace = "C:\Users\Administrator\Downloads\workspace",
    [string]$ClientRoot = "D:\WOW",
    [string]$MpqCliOverride = ""
)
$ErrorActionPreference = "Stop"
$UploadDir = Join-Path $Workspace "uploads"
$Result = Join-Path $UploadDir "G17C1_CLIENT_MPQ_UNLOCK_RESULT.txt"
$StateFile = Join-Path $UploadDir "G17C1_CLIENT_MPQ_UNLOCK_STATE.txt"
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$Tool = if ($MpqCliOverride) { $MpqCliOverride } else { Join-Path $PSScriptRoot "third_party\mpqcli-windows-amd64.exe" }
$Patcher = Join-Path $PSScriptRoot "tools\patch_g17c1_spell_dbc.py"
$SpellTarget = "DBFilesClient\Spell.dbc"
$AreaTarget = "DBFilesClient\AreaTable.dbc"
$ExpectedToolHash = "5dc56b130432098d18e911fdf4b3464adf850432b7fbb35003a2feff7c30a79f"
$ExpectedSpellHash = "dd25091167f671764735ce88c78b66485c6d661fadf05d322574c261f6e464ea"
$ExpectedSpellSize = 48956359
$ExpectedAreaHash = "1acef997a27f844a8abee9b477c44f2097f745168c6df85ece6b1a135568c233"
$ExpectedAreaSize = 362740
# Spell.dbc after the G17C1 unlock (deterministic; only 52226 record changes)
$ExpectedPatchedSpellHash = "03bf11fdeff7c296837fc6b0cc335476a9df33965baf8eed8ca671529577ccba"
$R4StateFile = Join-Path $UploadDir "G17R4_CLIENT_MPQ_UPGRADE_STATE.txt"
$R5StateFile = Join-Path $UploadDir "G17R5_LOCALE_MIRROR_STATE.txt"

$WorkRoot = ""; $BackupDir = ""; $TargetMpq = ""; $TemporaryTarget = ""; $SwapOld = ""
$NewArchiveHash = ""; $StateCommitted = $false

New-Item -ItemType Directory -Path $UploadDir -Force | Out-Null
[IO.File]::WriteAllText($Result, "", $Utf8NoBom)
function W([string]$Text) { Write-Host $Text; [IO.File]::AppendAllText($Result, $Text + [Environment]::NewLine, $Utf8NoBom) }
function Read-KeyValueFile([string]$Path) {
    $Values = @{}
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $Values }
    foreach ($Line in [IO.File]::ReadAllLines($Path)) { if ($Line -match '^([^=]+)=(.*)$') { $Values[$Matches[1]] = $Matches[2] } }
    return $Values
}
function Invoke-NativeCapture {
    param([string]$FilePath, [string[]]$NativeArgs)
    $Saved = $ErrorActionPreference; $Exit = 9009; $Output = @()
    try { $ErrorActionPreference = "Continue"; $Output = @(& $FilePath @NativeArgs 2>&1); $Exit = $LASTEXITCODE }
    finally { $ErrorActionPreference = $Saved }
    return [pscustomobject]@{ ExitCode = [int]$Exit; Lines = @($Output) }
}
function Invoke-NativeLogged {
    param([string]$FilePath, [string[]]$NativeArgs, [string]$Prefix)
    $Native = Invoke-NativeCapture -FilePath $FilePath -NativeArgs $NativeArgs
    foreach ($Line in $Native.Lines) { W ($Prefix + "|" + $Line.ToString()) }
    return [int]$Native.ExitCode
}
function Extract-ArchiveTarget([string]$Archive, [string]$Target, [string]$Tag) {
    $Root = Join-Path $WorkRoot ("extract_" + $Tag + "_" + [Guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Path $Root | Out-Null
    $Native = Invoke-NativeCapture -FilePath $Tool -NativeArgs @("extract", "--output", $Root, "--keep", "--file", $Target, $Archive)
    $Extracted = Join-Path $Root $Target
    if ($Native.ExitCode -eq 0 -and (Test-Path -LiteralPath $Extracted -PathType Leaf)) {
        $Hash = (Get-FileHash -LiteralPath $Extracted -Algorithm SHA256).Hash.ToLowerInvariant()
        $Size = (Get-Item -LiteralPath $Extracted).Length
        return [pscustomobject]@{ State = "HIT"; Path = $Extracted; Hash = $Hash; Size = [int64]$Size }
    }
    $Text = (($Native.Lines | ForEach-Object { $_.ToString() }) -join " | ")
    if ($Text.Contains("File doesn't exist") -or $Text.Contains("does not exist")) {
        return [pscustomobject]@{ State = "NO_HIT"; Path = ""; Hash = ""; Size = 0 }
    }
    if ($Text.Length -gt 600) { $Text = $Text.Substring(0, 600) }
    return [pscustomobject]@{ State = "ERROR"; Path = ""; Hash = ""; Size = 0; Detail = $Text }
}
function Assert-NewArchive([string]$Archive, [string]$Prefix) {
    $Format = Invoke-NativeCapture -FilePath $Tool -NativeArgs @("info", "--property", "format-version", $Archive)
    $FormatText = (($Format.Lines | ForEach-Object { $_.ToString().Trim() }) -join "")
    W ($Prefix + "_FORMAT_VERSION=" + $FormatText)
    if ($FormatText -ne "2") { throw "unexpected MPQ format version: $FormatText" }
    $Count = Invoke-NativeCapture -FilePath $Tool -NativeArgs @("info", "--property", "file-count", $Archive)
    $CountText = (($Count.Lines | ForEach-Object { $_.ToString().Trim() }) -join "")
    W ($Prefix + "_FILE_COUNT=" + $CountText)
    if ($CountText -ne "4") { throw "new archive must contain 2 targets plus listfile/attributes; count=$CountText" }
    $Spell = Extract-ArchiveTarget -Archive $Archive -Target $SpellTarget -Tag ($Prefix + "_spell")
    $Area = Extract-ArchiveTarget -Archive $Archive -Target $AreaTarget -Tag ($Prefix + "_area")
    W ($Prefix + "_SPELL_SHA256=" + $Spell.Hash)
    W ($Prefix + "_SPELL_SIZE=" + $Spell.Size)
    W ($Prefix + "_AREA_SHA256=" + $Area.Hash)
    W ($Prefix + "_AREA_SIZE=" + $Area.Size)
    if ($Spell.State -ne "HIT" -or $Spell.Size -ne $ExpectedSpellSize) { throw "new archive Spell.dbc missing/wrong size" }
    if ($Area.State -ne "HIT" -or $Area.Hash -cne $ExpectedAreaHash -or $Area.Size -ne $ExpectedAreaSize) { throw "new archive AreaTable.dbc mismatch" }
    # Password-protected? no. Spell gate: allow either old patched or new unlocked.
    if ($Spell.Hash -cne $ExpectedPatchedSpellHash) { throw "new archive Spell.dbc hash not unlocked image: $($Spell.Hash)" }
}
$PatchedSpellHash = ""
try {
    W "G17C1_CLIENT_MPQ_UNLOCK_START"
    W "SCOPE=SPELL_52226_FOCUS_1553_AND_AURA_52255_CLEARED_IN_CLIENT_SPELL_DBC"
    W "MODIFIES_SERVER=False"
    W "EXECUTES_SQL=False"
    if (Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Name -match '^(?i:wow|wow-64)$' }) {
        throw "Wow client is running; close it before upgrade"
    }
    if (Get-Process worldserver -ErrorAction SilentlyContinue) {
        throw "worldserver is running; close it first (client-only package, but keep the server calm)"
    }
    foreach ($Required in @($Tool, $Patcher, $R4StateFile, $R5StateFile)) {
        if (-not (Test-Path -LiteralPath $Required -PathType Leaf)) { throw "required file missing: $Required" }
    }
    if (-not (Test-Path -LiteralPath $ClientRoot -PathType Container)) { throw "client root missing: $ClientRoot" }
    $ClientRoot = (Resolve-Path -LiteralPath $ClientRoot).Path
    $DataDir = Join-Path $ClientRoot "Data"
    if (-not (Test-Path -LiteralPath (Join-Path $ClientRoot "Wow.exe") -PathType Leaf) -or -not (Test-Path -LiteralPath $DataDir -PathType Container)) {
        throw "invalid WoW client root"
    }
    $ToolHash = (Get-FileHash -LiteralPath $Tool -Algorithm SHA256).Hash.ToLowerInvariant()
    W "MPQCLI_SHA256=$ToolHash"
    if ($ToolHash -cne $ExpectedToolHash -and -not $MpqCliOverride) { throw "mpqcli hash mismatch" }

    $R4 = Read-KeyValueFile $R4StateFile
    $R5 = Read-KeyValueFile $R5StateFile
    if ($R4["INSTALL_STATUS"] -cne "PASS") { throw "R4 client state is not PASS" }
    if ($R5["INSTALL_STATUS"] -cne "PASS") { throw "R5 locale mirror state is not PASS" }
    $RootMpq = $R4["INSTALLED_MPQ"]
    $Slot = $R4["PATCH_SLOT"]
    if (-not $RootMpq -or $Slot -notmatch '^[A-Z]$') { throw "R4 state target/slot missing" }
    $LocaleMpq = $R5["TARGET_LOCALE_MPQ"]
    if (-not $LocaleMpq) { throw "R5 state target missing" }
    $ExpectedRoot = Join-Path $DataDir ("patch-" + $Slot + ".MPQ")
    if ($RootMpq -ine $ExpectedRoot) { throw "R4 state target outside its owned slot" }
    if (-not (Test-Path -LiteralPath $RootMpq -PathType Leaf)) { throw "R4 owned MPQ missing: $RootMpq" }
    if (-not (Test-Path -LiteralPath $LocaleMpq -PathType Leaf)) { throw "R5 locale MPQ missing: $LocaleMpq" }
    $RootHash = (Get-FileHash -LiteralPath $RootMpq -Algorithm SHA256).Hash.ToLowerInvariant()
    $LocaleHash = (Get-FileHash -LiteralPath $LocaleMpq -Algorithm SHA256).Hash.ToLowerInvariant()
    W "R4_ROOT_MPQ=$RootMpq"
    W "R4_ROOT_MPQ_SHA256=$RootHash"
    W "R5_LOCALE_MPQ=$LocaleMpq"
    W "R5_LOCALE_MPQ_SHA256=$LocaleHash"
    if ($RootHash -cne $R4["NEW_MPQ_SHA256"].ToLowerInvariant()) { throw "R4 owned root MPQ hash no longer matches state" }
    if ($LocaleHash -cne $R5["TARGET_MPQ_SHA256"].ToLowerInvariant()) { throw "R5 locale MPQ hash no longer matches state" }
    if ($RootHash -cne $LocaleHash) { throw "R5 locale mirror no longer byte-identical to root" }

    if (Test-Path -LiteralPath $StateFile -PathType Leaf) {
        $C1 = Read-KeyValueFile $StateFile
        if ($C1["INSTALL_STATUS"] -ceq "PASS" -and $C1["CLIENT_ROOT"] -ine $ClientRoot -and
            $C1["ROOT_MPQ"] -ine $RootMpq -and $C1["NEW_MPQ_SHA256"] -ceq $RootHash -and
            $LocaleHash -ceq $RootHash) {
            $CurrentSpell = Extract-ArchiveTarget -Archive $RootMpq -Target $SpellTarget -Tag "current_spell"
            if ($CurrentSpell.State -ceq "HIT" -and $CurrentSpell.Hash -ceq $ExpectedPatchedSpellHash -and
                $CurrentSpell.Size -eq $ExpectedSpellSize) {
                W "G17C1_CLIENT_MPQ_UNLOCK=ALREADY_CURRENT"
                W "G17C1_CLIENT_MPQ_UNLOCK_RESULT=PASS"
                W "RESULT_FILE=$Result"
                exit 0
            }
            throw "G17C1 state exists but installed Spell.dbc is not the unlocked image"
        }
    }

    $ConfigWtf = Join-Path $ClientRoot "WTF\Config.wtf"
    $Locale = "UNKNOWN"
    if (Test-Path -LiteralPath $ConfigWtf -PathType Leaf) {
        foreach ($Line in [IO.File]::ReadAllLines($ConfigWtf)) {
            if ($Line -match '^SET\s+locale\s+"([A-Za-z]{4})"') { $Locale = $Matches[1]; break }
        }
    }
    W "CLIENT_LOCALE=$Locale"
    if ($Locale -cne "zhCN") { throw "zhCN locale required; detected $Locale" }

    $WorkRoot = Join-Path $UploadDir ("G17C1_Client_Work_" + (Get-Date -Format "yyyyMMdd_HHmmss") + "_" + $PID)
    if (Test-Path -LiteralPath $WorkRoot) { throw "work directory already exists" }
    New-Item -ItemType Directory -Path $WorkRoot | Out-Null

    $OldSpell = Extract-ArchiveTarget -Archive $RootMpq -Target $SpellTarget -Tag "old_spell"
    $OldArea = Extract-ArchiveTarget -Archive $RootMpq -Target $AreaTarget -Tag "old_area"
    W "R4_OWNED_SPELL_SHA256=$($OldSpell.Hash)"
    W "R4_OWNED_SPELL_SIZE=$($OldSpell.Size)"
    W "R4_OWNED_AREA_SHA256=$($OldArea.Hash)"
    if ($OldSpell.State -ne "HIT" -or $OldSpell.Hash -cne $ExpectedSpellHash -or $OldSpell.Size -ne $ExpectedSpellSize) {
        throw "R4 owned Spell.dbc mismatch"
    }
    if ($OldArea.State -ne "HIT" -or $OldArea.Hash -cne $ExpectedAreaHash -or $OldArea.Size -ne $ExpectedAreaSize) {
        throw "R4 owned AreaTable.dbc mismatch"
    }

    $PythonCandidates = @(
        (Join-Path $env:LOCALAPPDATA "Programs\Python\Python312\python.exe"),
        (Join-Path $env:LOCALAPPDATA "Programs\Python\Python310\python.exe")
    )
    $Python = @($PythonCandidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf })[0]
    if (-not $Python) {
        $PythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
        if ($PythonCommand -and $PythonCommand.Source -notmatch "\\WindowsApps\\") { $Python = $PythonCommand.Source }
    }
    if (-not $Python) { throw "Python 3.12/3.10 not found" }
    W "PYTHON=$Python"

    $checkReport = Join-Path $WorkRoot "G17C1_SPELL_DBC_CHECK_BEFORE.txt"
    $rc = Invoke-NativeLogged -FilePath $Python -NativeArgs @($Patcher, "check", "--input", $OldSpell.Path, "--report", $checkReport) -Prefix "CHECK_BEFORE"
    W "CHECK_BEFORE_EXIT=$rc"
    if ($rc -ne 0) { throw "extracted Spell.dbc did not pass semantic guards" }

    $GeneratedSpell = Join-Path $WorkRoot "generated\DBFilesClient\Spell.dbc"
    $patchReport = Join-Path $WorkRoot "G17C1_SPELL_DBC_PATCH_REPORT.txt"
    $rc = Invoke-NativeLogged -FilePath $Python -NativeArgs @($Patcher, "patch", "--input", $OldSpell.Path, "--output", $GeneratedSpell, "--report", $patchReport) -Prefix "SPELL_DBC_PATCH"
    W "SPELL_DBC_PATCH_EXIT=$rc"
    if ($rc -ne 0) { throw "Spell.dbc patch failed" }
    $PatchedSpellHash = (Get-FileHash -LiteralPath $GeneratedSpell -Algorithm SHA256).Hash.ToLowerInvariant()
    W "GENERATED_SPELL_SHA256=$PatchedSpellHash"
    if ((Get-Item -LiteralPath $GeneratedSpell).Length -ne $ExpectedSpellSize) { throw "patched Spell.dbc size changed" }

    $PackRoot = Join-Path $WorkRoot "pack_root"
    $PackSpell = Join-Path $PackRoot $SpellTarget
    $PackArea = Join-Path $PackRoot $AreaTarget
    New-Item -ItemType Directory -Path (Split-Path -Parent $PackSpell) -Force | Out-Null
    Copy-Item -LiteralPath $GeneratedSpell -Destination $PackSpell
    Copy-Item -LiteralPath $OldArea.Path -Destination $PackArea
    $BuiltArchive = Join-Path $WorkRoot ("patch-" + $Slot + ".MPQ")
    $CreateExit = Invoke-NativeLogged -FilePath $Tool -NativeArgs @("create", "--game", "wow-wotlk", "--output", $BuiltArchive, $PackRoot) -Prefix "MPQ_CREATE"
    W "MPQ_CREATE_EXIT=$CreateExit"
    if ($CreateExit -ne 0 -or -not (Test-Path -LiteralPath $BuiltArchive -PathType Leaf)) { throw "new MPQ creation failed" }
    Assert-NewArchive -Archive $BuiltArchive -Prefix "BUILT_MPQ"
    $NewArchiveHash = (Get-FileHash -LiteralPath $BuiltArchive -Algorithm SHA256).Hash.ToLowerInvariant()
    $NewArchiveSize = (Get-Item -LiteralPath $BuiltArchive).Length
    W "NEW_MPQ_SHA256=$NewArchiveHash"
    W "NEW_MPQ_SIZE=$NewArchiveSize"
    if ($NewArchiveHash -ceq $RootHash) { throw "new MPQ hash unexpectedly equals old" }

    $Stamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $BackupDir = Join-Path $UploadDir ("G17C1_Client_Unlock_Backup_" + $Stamp)
    if (Test-Path -LiteralPath $BackupDir) { throw "backup directory already exists" }
    New-Item -ItemType Directory -Path $BackupDir | Out-Null
    $BackupRoot = Join-Path $BackupDir ("before_G17C1_" + (Split-Path -Leaf $RootMpq))
    $BackupLocale = Join-Path $BackupDir ("before_G17C1_" + (Split-Path -Leaf $LocaleMpq))
    Copy-Item -LiteralPath $RootMpq -Destination $BackupRoot
    Copy-Item -LiteralPath $LocaleMpq -Destination $BackupLocale
    if ((Get-FileHash -LiteralPath $BackupRoot -Algorithm SHA256).Hash.ToLowerInvariant() -cne $RootHash) { throw "backup root MPQ verification failed" }
    if ((Get-FileHash -LiteralPath $BackupLocale -Algorithm SHA256).Hash.ToLowerInvariant() -cne $LocaleHash) { throw "backup locale MPQ verification failed" }
    W "BACKUP_DIR=$BackupDir"

    $TemporaryTarget = $RootMpq + ".g17c1.new.tmp"
    $SwapOld = $RootMpq + ".g17c1.old.tmp"
    if ((Test-Path -LiteralPath $TemporaryTarget) -or (Test-Path -LiteralPath $SwapOld)) { throw "swap temp path exists" }
    Copy-Item -LiteralPath $BuiltArchive -Destination $TemporaryTarget
    if ((Get-FileHash -LiteralPath $TemporaryTarget -Algorithm SHA256).Hash.ToLowerInvariant() -cne $NewArchiveHash) { throw "temp MPQ hash mismatch" }
    Move-Item -LiteralPath $RootMpq -Destination $SwapOld
    Move-Item -LiteralPath $TemporaryTarget -Destination $RootMpq
    Assert-NewArchive -Archive $RootMpq -Prefix "INSTALLED_ROOT"
    if ((Get-FileHash -LiteralPath $RootMpq -Algorithm SHA256).Hash.ToLowerInvariant() -cne $NewArchiveHash) { throw "installed root MPQ hash mismatch" }

    # R5 mirror contract: locale file is byte-identical to root.
    $LocaleTmp = $LocaleMpq + ".g17c1.new.tmp"
    $LocaleSwap = $LocaleMpq + ".g17c1.old.tmp"
    if ((Test-Path -LiteralPath $LocaleTmp) -or (Test-Path -LiteralPath $LocaleSwap)) { throw "locale swap temp exists" }
    Copy-Item -LiteralPath $RootMpq -Destination $LocaleTmp
    Move-Item -LiteralPath $LocaleMpq -Destination $LocaleSwap
    Move-Item -LiteralPath $LocaleTmp -Destination $LocaleMpq
    if ((Get-FileHash -LiteralPath $LocaleMpq -Algorithm SHA256).Hash.ToLowerInvariant() -cne $NewArchiveHash) { throw "locale mirror hash mismatch" }

    $CacheDir = Join-Path $ClientRoot "Cache"
    if (Test-Path -LiteralPath $CacheDir -PathType Container) {
        Remove-Item -LiteralPath $CacheDir -Recurse -Force
        W "CLIENT_CACHE_REMOVED=True"
    } else { W "CLIENT_CACHE_REMOVED=False" }

    $StateLines = @(
        "STATE_FORMAT=1",
        "INSTALL_STATUS=PASS",
        ("CLIENT_ROOT=" + $ClientRoot),
        ("ROOT_MPQ=" + $RootMpq),
        ("PATCH_SLOT=" + $Slot),
        ("LOCALE_MPQ=" + $LocaleMpq),
        ("OLD_SPELL_DBC_SHA256=" + $ExpectedSpellHash),
        ("NEW_SPELL_DBC_SHA256=" + $PatchedSpellHash),
        ("OLD_ROOT_MPQ_SHA256=" + $RootHash),
        ("OLD_LOCALE_MPQ_SHA256=" + $LocaleHash),
        ("NEW_MPQ_SHA256=" + $NewArchiveHash),
        ("BACKUP_DIR=" + $BackupDir),
        ("INSTALLED_AT=" + (Get-Date).ToString("o"))
    )
    $StateTemp = $StateFile + ".tmp"
    if (Test-Path -LiteralPath $StateTemp) { throw "state temp exists" }
    [IO.File]::WriteAllText($StateTemp, (($StateLines -join [Environment]::NewLine) + [Environment]::NewLine), $Utf8NoBom)
    Move-Item -LiteralPath $StateTemp -Destination $StateFile
    $StateCommitted = $true
    Remove-Item -LiteralPath $SwapOld -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $LocaleSwap -Force -ErrorAction SilentlyContinue
    $SwapOld = ""; $LocaleSwap = ""

    W "CLIENT_RESTART_REQUIRED=True"
    W "G17C1_CLIENT_MPQ_UNLOCK=PASS"
    W "G17C1_CLIENT_MPQ_UNLOCK_RESULT=PASS"
    W "RESULT_FILE=$Result"
    exit 0
} catch {
    $Message = $_.Exception.Message
    if (-not $StateCommitted) {
        try {
            if ($RootMpq -and $NewArchiveHash -and (Test-Path -LiteralPath $RootMpq -PathType Leaf)) {
                $Current = (Get-FileHash -LiteralPath $RootMpq -Algorithm SHA256).Hash.ToLowerInvariant()
                if ($Current -ceq $NewArchiveHash) {
                    $Rescue = if ($BackupDir) { Join-Path $BackupDir "FAILED_NEW_ROOT_MPQ_RESCUE.MPQ" } else { Join-Path $WorkRoot "FAILED_NEW_ROOT_MPQ_RESCUE.MPQ" }
                    Move-Item -LiteralPath $RootMpq -Destination $Rescue
                    W "FAILED_NEW_ROOT_MPQ_RESCUED=True"
                }
            }
            if ($SwapOld -and (Test-Path -LiteralPath $SwapOld) -and -not (Test-Path -LiteralPath $RootMpq)) {
                Move-Item -LiteralPath $SwapOld -Destination $RootMpq
                W "AUTO_ROLLBACK_ROOT=PASS"
            }
            if ($LocaleMpq -and (Test-Path -LiteralPath $LocaleSwap -PathType Leaf) -and -not (Test-Path -LiteralPath $LocaleMpq)) {
                Move-Item -LiteralPath $LocaleSwap -Destination $LocaleMpq
                W "AUTO_ROLLBACK_LOCALE=PASS"
            }
        } catch { W ("AUTO_ROLLBACK_ERROR=" + $_.Exception.Message) }
    }
    W ("G17C1_CLIENT_MPQ_UNLOCK_ERROR=" + $Message)
    W "G17C1_CLIENT_MPQ_UNLOCK_RESULT=FAIL"
    W "RESULT_FILE=$Result"
    exit 1
} finally {
    if ($TemporaryTarget -and (Test-Path -LiteralPath $TemporaryTarget -PathType Leaf)) { Remove-Item -LiteralPath $TemporaryTarget -Force -ErrorAction SilentlyContinue }
    if ($WorkRoot -and (Test-Path -LiteralPath $WorkRoot -PathType Container)) { Remove-Item -LiteralPath $WorkRoot -Recurse -Force -ErrorAction SilentlyContinue }
}
