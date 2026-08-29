#requires -Version 5.1
# G17-C10: ALL-archetype visual refinement (dragon row UNCHANGED from the
# user-verified C9 v3). Same proven v3 installer flow.
#
# v3 INSTALLER FIX RELEASE. The v1/v2 installer could never pass (three fatal
# gates inherited from the C6 template):
#   1. version gate grepped the C6-era patcher variable name, but the C9
#      patcher variable is G17C9_VERSION  -> OBSOLETE_PACKAGE on every run
#   2. input gate required the C3-era image hash, but the user's client is at
#      the C6/C7/C8 state
#   3. output gate required the C6-era image hash, but the C9 patcher output
#      is different by design
#   4. C3-state env mode required the root MPQ hash to still equal the C3-era
#      value (C6/C7/C8 installs changed it)
#   5. the rollback script was an empty file
# v3 rebuilds the flow on the pattern PROVEN by G17-C8 on the user's machine:
# state files are used for PATHS ONLY (no hash pinning), input state is
# detected by the patcher itself, and output is verified BY CONTENT
# (patcher check -> COMPLETE) plus a round-trip extraction check.
param(
    [string]$Workspace = "C:\Users\Administrator\Downloads\workspace",
    [string]$ClientRoot = "D:\WOW",
    [string]$MpqCliOverride = ""
)
$ErrorActionPreference = "Stop"
$UploadDir = Join-Path $Workspace "uploads"
$Result = Join-Path $UploadDir "G17C10_CLIENT_VISUALS_RESULT.txt"
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$Patcher = Join-Path $PSScriptRoot "tools\patch_g17c10.py"
$Tool = if ($MpqCliOverride) { $MpqCliOverride } else { Join-Path $PSScriptRoot "third_party\mpqcli-windows-amd64.exe" }
$SpellTarget = "DBFilesClient\Spell.dbc"
$AreaTarget = "DBFilesClient\AreaTable.dbc"
$ExpectedToolHash = "5dc56b130432098d18e911fdf4b3464adf850432b7fbb35003a2feff7c30a79f"
$ExpectedAreaHash = "1acef997a27f844a8abee9b477c44f2097f745168c6df85ece6b1a135568c233"
$ExpectedAreaSize = 362740
$ExpectedSpellSize = 48985408
$BuildFingerprint = "v1_all_archetypes_visuals"
# State files, newest chain state first. PATHS ONLY - never hash-pinned,
# because every later client package legitimately rewrites the root MPQ.
$StateFiles = @(
    (Join-Path $UploadDir "G17C10_CLIENT_VISUALS_STATE.txt"),
    (Join-Path $UploadDir "G17C10_CLIENT_VISUALS_STATE.txt"),
    (Join-Path $UploadDir "G17C8_STATE.txt"),
    (Join-Path $UploadDir "G17C6_CLIENT_VISUALS_STATE.txt"),
    (Join-Path $UploadDir "G17C3_CLIENT_BAR_BUTTONS_STATE.txt")
)
$ResultStateFile = Join-Path $UploadDir "G17C10_CLIENT_VISUALS_STATE.txt"

$WorkRoot = ""; $BackupDir = ""; $RootMpq = ""; $LocaleMpq = ""; $Slot = "Z"
$SwapOld = ""; $NewArchiveHash = ""; $PatchedSpellHash = ""; $StateCommitted = $false

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
    return [pscustomobject]@{ State = "NO_HIT"; Path = ""; Hash = ""; Size = 0 }
}
function Find-Python {
    $candidates = @(
        (Join-Path $env:LOCALAPPDATA "Programs\Python\Python312\python.exe"),
        (Join-Path $env:LOCALAPPDATA "Programs\Python\Python310\python.exe"),
        "C:\Python312\python.exe", "C:\Python310\python.exe")
    $python = @($candidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf })[0]
    if (-not $python) {
        $cmd = Get-Command python.exe -ErrorAction SilentlyContinue
        if ($cmd -and $cmd.Source -notmatch "\\WindowsApps\\") { $python = $cmd.Source }
    }
    return $python
}

try {
    W "G17C10_CLIENT_VISUALS_START"
    W ("G17C10_BUILD=" + $BuildFingerprint)
    W "SCOPE=PATCH_SPELLVISUAL_PER_ARCHETYPE_SLOT_PLUS_RANGE30_PLUS_CD0_FOR_990000_990024"
    W "MODIFIES_SERVER=False"
    W "EXECUTES_SQL=False"
    W "DBC_PAYLOAD=dragon row unchanged (user-verified); beast/magic/mech/generic rows refined"
    if (Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Name -match '^(?i:wow|wow-64)$' }) {
        throw "Wow client is running; close it before upgrade"
    }
    foreach ($Required in @($Tool, $Patcher)) {
        if (-not (Test-Path -LiteralPath $Required -PathType Leaf)) { throw "required file missing: $Required" }
    }

    # --- version gate (fixed in v3: greps the REAL patcher variable) ---
    $patcherText = [IO.File]::ReadAllText($Patcher)
    if ($patcherText -notmatch 'G17C10_VERSION\s*=\s*"v1_all_archetypes_visuals"') {
        throw ("OBSOLETE_PACKAGE: patcher is not v1_all_archetypes_visuals")
    }
    W "G17C10_PATCHER_VERSION_CHECK=PASS v1_all_archetypes_visuals"

    if (-not (Test-Path -LiteralPath $ClientRoot -PathType Container)) { throw "client root missing: $ClientRoot" }
    $ClientRoot = (Resolve-Path -LiteralPath $ClientRoot).Path
    $DataDir = Join-Path $ClientRoot "Data"
    if (-not (Test-Path -LiteralPath (Join-Path $ClientRoot "Wow.exe") -PathType Leaf) -or -not (Test-Path -LiteralPath $DataDir -PathType Container)) {
        throw "invalid WoW client root"
    }
    $ToolHash = (Get-FileHash -LiteralPath $Tool -Algorithm SHA256).Hash.ToLowerInvariant()
    W "MPQCLI_SHA256=$ToolHash"
    if ($ToolHash -cne $ExpectedToolHash -and -not $MpqCliOverride) { throw "mpqcli hash mismatch" }

    $Python = Find-Python
    if (-not $Python) { throw "Python 3.12/3.10 not found" }
    W "PYTHON=$Python"

    $WorkRoot = Join-Path $UploadDir ("G17C10_Client_Work_" + (Get-Date -Format "yyyyMMdd_HHmmss") + "_" + $PID)
    New-Item -ItemType Directory -Path $WorkRoot | Out-Null

    # --- environment: state files give PATHS ONLY (C8-proven), else discovery ---
    $stateSrc = $null
    foreach ($sf in $StateFiles) {
        $st = Read-KeyValueFile $sf
        if ($st["INSTALL_STATUS"] -ceq "PASS" -and $st["ROOT_MPQ"] -and $st["LOCALE_MPQ"]) {
            $stateSrc = $st
            W ("ENV_STATE_SOURCE=" + (Split-Path -Leaf $sf))
            break
        }
    }
    if ($stateSrc) {
        W "ENV_MODE=STATE"
        $RootMpq = $stateSrc["ROOT_MPQ"]
        $Slot = $stateSrc["PATCH_SLOT"]
        if (-not $Slot -or $Slot -notmatch '^[A-Z]$') { $Slot = "Z" }
        $LocaleMpq = $stateSrc["LOCALE_MPQ"]
        if (-not (Test-Path -LiteralPath $RootMpq -PathType Leaf)) { throw "root MPQ missing: $RootMpq" }
        if (-not (Test-Path -LiteralPath $LocaleMpq -PathType Leaf)) { throw "locale MPQ missing: $LocaleMpq" }
    } else {
        W "ENV_MODE=DISCOVERY"
        $LocaleDir = Join-Path $DataDir "zhCN"
        if (-not (Test-Path -LiteralPath $LocaleDir -PathType Container)) { throw "zhCN locale directory missing" }
        $Slots = @("Z","Y","X","W","V","U","T","S","R","Q","P","O","N","M","L","K","J","I","H","G","F","E","D","C","B","A")
        $RootHits = @()
        foreach ($s in $Slots) {
            $cand = Join-Path $DataDir ("patch-" + $s + ".MPQ")
            if (-not (Test-Path -LiteralPath $cand -PathType Leaf)) { continue }
            $area = Extract-ArchiveTarget -Archive $cand -Target $AreaTarget -Tag ("root_" + $s + "_area")
            $spell = Extract-ArchiveTarget -Archive $cand -Target $SpellTarget -Tag ("root_" + $s + "_spell")
            W ("ROOT_DISCOVERY=SLOT=$s;SPELL=$($spell.State);AREA=$($area.State);PATH=$cand")
            $isChain = ($spell.State -eq "HIT" -and $area.State -eq "HIT" -and
                        $area.Hash -ceq $ExpectedAreaHash -and $area.Size -eq $ExpectedAreaSize -and
                        $spell.Size -eq $ExpectedSpellSize)
            if ($isChain) { $RootHits += [pscustomobject]@{ Path = $cand; Slot = $s } }
        }
        if ($RootHits.Count -ne 1) { throw "expected exactly one G17 chain owner (found $($RootHits.Count))" }
        $RootMpq = $RootHits[0].Path
        $Slot = $RootHits[0].Slot
        $RootHash = (Get-FileHash -LiteralPath $RootMpq -Algorithm SHA256).Hash.ToLowerInvariant()
        $LocaleMpq = $null
        foreach ($s in $Slots) {
            $cand = Join-Path $LocaleDir ("patch-zhCN-" + $s + ".MPQ")
            if (-not (Test-Path -LiteralPath $cand -PathType Leaf)) { continue }
            $h = (Get-FileHash -LiteralPath $cand -Algorithm SHA256).Hash.ToLowerInvariant()
            if ($h -ceq $RootHash) { $LocaleMpq = $cand; break }
        }
        if (-not $LocaleMpq) { throw "no byte-identical locale mirror found; report this" }
        W "ENV_DISCOVERED_ROOT_MPQ=$RootMpq"
        W "ENV_DISCOVERED_ROOT_SLOT=$Slot"
        W "ENV_DISCOVERED_LOCALE_MPQ=$LocaleMpq"
    }
    W "ROOT_MPQ=$RootMpq"
    W "PATCH_SLOT=$Slot"
    W "LOCALE_MPQ=$LocaleMpq"

    $ConfigWtf = Join-Path $ClientRoot "WTF\Config.wtf"
    $Locale = "UNKNOWN"
    if (Test-Path -LiteralPath $ConfigWtf -PathType Leaf) {
        foreach ($Line in [IO.File]::ReadAllLines($ConfigWtf)) {
            if ($Line -match '^SET\s+locale\s+"([A-Za-z]{4})"') { $Locale = $Matches[1]; break }
        }
    }
    W "CLIENT_LOCALE=$Locale"
    if ($Locale -cne "zhCN") { throw "zhCN locale required; detected $Locale" }

    # --- extract current chain-owned DBCs ---
    $OldSpell = Extract-ArchiveTarget -Archive $RootMpq -Target $SpellTarget -Tag "old_spell"
    $OldArea = Extract-ArchiveTarget -Archive $RootMpq -Target $AreaTarget -Tag "old_area"
    W ("ROOT_OWNED_SPELL_SHA256=" + $OldSpell.Hash)
    W ("ROOT_OWNED_SPELL_SIZE=" + $OldSpell.Size)
    W ("ROOT_OWNED_AREA_SHA256=" + $OldArea.Hash)
    if ($OldArea.State -ne "HIT" -or $OldArea.Hash -cne $ExpectedAreaHash -or $OldArea.Size -ne $ExpectedAreaSize) {
        throw "root owned AreaTable.dbc mismatch (not the G17 chain owner?)"
    }
    if ($OldSpell.State -ne "HIT") { throw "root owned Spell.dbc missing" }
    if ($OldSpell.Size -ne $ExpectedSpellSize) {
        throw "root owned Spell.dbc size $($OldSpell.Size) != expected $ExpectedSpellSize; report this"
    }

    # --- input state detected BY THE PATCHER (works from C3/C6/C7/C8 images) ---
    $cliOut = @(& $Python $Patcher check --input $OldSpell.Path 2>&1)
    $cliExit = $LASTEXITCODE
    foreach ($line in $cliOut) { W ("CLI_CHECK|" + $line.ToString()) }
    W "CLI_CHECK_EXIT=$cliExit"
    $cliState = @($cliOut | Where-Object { $_ -match '^G17C10_STATE=' } | Select-Object -First 1)[0]
    if (-not $cliState) { throw "client check printed no state" }
    W ("CLI_STATE=" + $cliState.ToString().Trim())

    if ($cliState -match 'G17C10_STATE=COMPLETE') {
        W "G17C10_VISUAL_STATE=ALREADY_COMPLETE"
        W "G17C10_CLIENT_VISUALS=ALREADY_CURRENT"
        W "G17C10_CLIENT_VISUALS_RESULT=PASS"
        W "NOTE=client Spell.dbc already has the v3 visuals+ranges+zero cooldowns; no write performed"
        W "RESULT_FILE=$Result"
        exit 0
    }
    if ($cliState -match 'G17C10_STATE=PARTIAL') {
        throw "client Spell.dbc carriers PARTIAL (carriers found != 25); report this result file"
    }

    # --- patch ---
    $GeneratedSpell = Join-Path $WorkRoot "generated\DBFilesClient\Spell.dbc"
    New-Item -ItemType Directory -Path (Split-Path -Parent $GeneratedSpell) -Force | Out-Null
    $rc = Invoke-NativeLogged -FilePath $Python -NativeArgs @($Patcher, "patch", "--input", $OldSpell.Path, "--output", $GeneratedSpell) -Prefix "CLI_PATCH"
    W "CLI_PATCH_EXIT=$rc"
    if ($rc -ne 0) { throw "Spell.dbc C9 v3 patch failed" }
    if ((Get-Item -LiteralPath $GeneratedSpell).Length -ne $OldSpell.Size) {
        throw "patched Spell.dbc size changed (must be in-place layout)"
    }

    # --- CONTENT verification (v3: by patcher state, not a hardcoded image hash) ---
    $verOut = @(& $Python $Patcher check --input $GeneratedSpell 2>&1)
    $verExit = $LASTEXITCODE
    foreach ($line in $verOut) { W ("VERIFY|" + $line.ToString()) }
    W "VERIFY_EXIT=$verExit"
    $verState = @($verOut | Where-Object { $_ -match '^G17C10_STATE=' } | Select-Object -First 1)[0]
    if ($verExit -ne 0 -or -not $verState -or $verState -notmatch 'G17C10_STATE=COMPLETE') {
        throw "patched Spell.dbc failed content verification (expected G17C10_STATE=COMPLETE)"
    }
    $PatchedSpellHash = (Get-FileHash -LiteralPath $GeneratedSpell -Algorithm SHA256).Hash.ToLowerInvariant()
    W "GENERATED_SPELL_SHA256=$PatchedSpellHash"

    # --- rebuild the root MPQ ---
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

    # --- round-trip verification of the BUILT archive ---
    $BuiltSpell = Extract-ArchiveTarget -Archive $BuiltArchive -Target $SpellTarget -Tag "built_spell"
    $BuiltArea = Extract-ArchiveTarget -Archive $BuiltArchive -Target $AreaTarget -Tag "built_area"
    W ("BUILT_MPQ_SPELL_SHA256=" + $BuiltSpell.Hash)
    W ("BUILT_MPQ_AREA_SHA256=" + $BuiltArea.Hash)
    if ($BuiltSpell.State -ne "HIT" -or $BuiltSpell.Hash -cne $PatchedSpellHash -or $BuiltSpell.Size -ne $ExpectedSpellSize) {
        throw "built archive Spell.dbc round-trip mismatch"
    }
    if ($BuiltArea.State -ne "HIT" -or $BuiltArea.Hash -cne $ExpectedAreaHash -or $BuiltArea.Size -ne $ExpectedAreaSize) {
        throw "built archive AreaTable.dbc mismatch"
    }
    $NewArchiveHash = (Get-FileHash -LiteralPath $BuiltArchive -Algorithm SHA256).Hash.ToLowerInvariant()
    $NewArchiveSize = (Get-Item -LiteralPath $BuiltArchive).Length
    W "NEW_MPQ_SHA256=$NewArchiveHash"
    W "NEW_MPQ_SIZE=$NewArchiveSize"

    # --- backup before swap ---
    $Stamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $BackupDir = Join-Path $UploadDir ("G17C10_Client_Backup_" + $Stamp)
    if (Test-Path -LiteralPath $BackupDir) { throw "backup directory already exists" }
    New-Item -ItemType Directory -Path $BackupDir | Out-Null
    $BackupRoot = Join-Path $BackupDir "before_G17C10_root.MPQ"
    Copy-Item -LiteralPath $RootMpq -Destination $BackupRoot
    if ((Get-FileHash -LiteralPath $BackupRoot -Algorithm SHA256).Hash.ToLowerInvariant() -cne (Get-FileHash -LiteralPath $RootMpq -Algorithm SHA256).Hash.ToLowerInvariant()) {
        throw "backup root MPQ verification failed"
    }
    $BackupLocale = Join-Path $BackupDir "before_G17C10_locale.MPQ"
    Copy-Item -LiteralPath $LocaleMpq -Destination $BackupLocale
    W "BACKUP_ROOT=$BackupRoot"
    W "BACKUP_LOCALE=$BackupLocale"
    W "BACKUP_DIR=$BackupDir"

    # --- swap root ---
    $TemporaryTarget = $RootMpq + ".g17c10.new.tmp"
    $SwapOld = $RootMpq + ".g17c10.old.tmp"
    if (Test-Path -LiteralPath $TemporaryTarget -PathType Leaf) { Remove-Item -LiteralPath $TemporaryTarget -Force -ErrorAction SilentlyContinue }
    if (Test-Path -LiteralPath $SwapOld -PathType Leaf) { Remove-Item -LiteralPath $SwapOld -Force -ErrorAction SilentlyContinue }
    Copy-Item -LiteralPath $BuiltArchive -Destination $TemporaryTarget
    if ((Get-FileHash -LiteralPath $TemporaryTarget -Algorithm SHA256).Hash.ToLowerInvariant() -cne $NewArchiveHash) { throw "temp MPQ hash mismatch" }
    Move-Item -LiteralPath $RootMpq -Destination $SwapOld
    Move-Item -LiteralPath $TemporaryTarget -Destination $RootMpq
    if ((Get-FileHash -LiteralPath $RootMpq -Algorithm SHA256).Hash.ToLowerInvariant() -cne $NewArchiveHash) { throw "installed root MPQ hash mismatch" }
    $InstalledSpell = Extract-ArchiveTarget -Archive $RootMpq -Target $SpellTarget -Tag "installed_spell"
    if ($InstalledSpell.State -ne "HIT" -or $InstalledSpell.Hash -cne $PatchedSpellHash) {
        throw "installed root MPQ Spell.dbc verification failed"
    }
    W "INSTALLED_ROOT_VERIFIED=True"

    # --- mirror locale ---
    $LocaleTmp = $LocaleMpq + ".g17c10.new.tmp"
    $LocaleSwap = $LocaleMpq + ".g17c10.old.tmp"
    if (Test-Path -LiteralPath $LocaleTmp -PathType Leaf) { Remove-Item -LiteralPath $LocaleTmp -Force -ErrorAction SilentlyContinue }
    if (Test-Path -LiteralPath $LocaleSwap -PathType Leaf) { Remove-Item -LiteralPath $LocaleSwap -Force -ErrorAction SilentlyContinue }
    Copy-Item -LiteralPath $RootMpq -Destination $LocaleTmp
    Move-Item -LiteralPath $LocaleMpq -Destination $LocaleSwap
    Move-Item -LiteralPath $LocaleTmp -Destination $LocaleMpq
    if ((Get-FileHash -LiteralPath $LocaleMpq -Algorithm SHA256).Hash.ToLowerInvariant() -cne $NewArchiveHash) { throw "locale mirror hash mismatch" }
    W "LOCALE_MIRROR_VERIFIED=True"

    # --- clear client cache ---
    $CacheDir = Join-Path $ClientRoot "Cache"
    if (Test-Path -LiteralPath $CacheDir -PathType Container) {
        Remove-Item -LiteralPath $CacheDir -Recurse -Force
        W "CLIENT_CACHE_REMOVED=True"
    } else { W "CLIENT_CACHE_REMOVED=False" }

    # --- commit state ---
    $StateLines = @(
        "STATE_FORMAT=2",
        ("BUILD=" + $BuildFingerprint),
        "INSTALL_STATUS=PASS",
        ("CLIENT_ROOT=" + $ClientRoot),
        ("ROOT_MPQ=" + $RootMpq),
        ("PATCH_SLOT=" + $Slot),
        ("LOCALE_MPQ=" + $LocaleMpq),
        ("OLD_SPELL_DBC_SHA256=" + $OldSpell.Hash),
        ("NEW_SPELL_DBC_SHA256=" + $PatchedSpellHash),
        ("NEW_MPQ_SHA256=" + $NewArchiveHash),
        ("BACKUP_DIR=" + $BackupDir),
        ("INSTALLED_AT=" + (Get-Date).ToString("o"))
    )
    $StateTemp = $ResultStateFile + ".tmp"
    [IO.File]::WriteAllText($StateTemp, (($StateLines -join [Environment]::NewLine) + [Environment]::NewLine), $Utf8NoBom)
    Move-Item -LiteralPath $StateTemp -Destination $ResultStateFile -Force
    $StateCommitted = $true

    Remove-Item -LiteralPath $SwapOld -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $LocaleSwap -Force -ErrorAction SilentlyContinue
    $SwapOld = ""

    W "CLIENT_RESTART_REQUIRED=True"
    W "G17C10_CLIENT_VISUALS=PASS"
    W "G17C10_CLIENT_VISUALS_RESULT=PASS"
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
            if ($SwapOld -and (Test-Path -LiteralPath $SwapOld -PathType Leaf) -and -not (Test-Path -LiteralPath $RootMpq)) {
                Move-Item -LiteralPath $SwapOld -Destination $RootMpq
                W "AUTO_ROLLBACK_ROOT=PASS"
            }
        } catch { W ("AUTO_ROLLBACK_ERROR=" + $_.Exception.Message) }
    }
    W ("G17C10_CLIENT_VISUALS_ERROR=" + $Message)
    W "G17C10_CLIENT_VISUALS_RESULT=FAIL"
    W "RESULT_FILE=$Result"
    exit 1
}
