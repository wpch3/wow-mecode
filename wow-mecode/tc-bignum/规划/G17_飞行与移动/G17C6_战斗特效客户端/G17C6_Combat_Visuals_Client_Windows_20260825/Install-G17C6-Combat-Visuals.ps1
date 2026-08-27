#requires -Version 5.1
# G17-C6: patch the 25 combat carriers' SpellVisualID + RangeIndex in the
# CLIENT Spell.dbc inside the existing G17 MPQ chain.
# Prerequisites: C3 installed (client Spell.dbc == 006a892b..., the 4-button
# image) and the R4/R5 MPQ chain (patch-Z.MPQ root owner + zhCN-Y mirror).
param(
    [string]$Workspace = "C:\Users\Administrator\Downloads\workspace",
    [string]$ClientRoot = "D:\WOW",
    [string]$MpqCliOverride = ""
)
$ErrorActionPreference = "Stop"
$UploadDir = Join-Path $Workspace "uploads"
$Result = Join-Path $UploadDir "G17C6_CLIENT_VISUALS_RESULT.txt"
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$Tool = if ($MpqCliOverride) { $MpqCliOverride } else { Join-Path $PSScriptRoot "third_party\mpqcli-windows-amd64.exe" }
$Patcher = Join-Path $PSScriptRoot "tools\patch_g17b3r5_visuals.py"
$SpellTarget = "DBFilesClient\Spell.dbc"
$AreaTarget = "DBFilesClient\AreaTable.dbc"
$ExpectedToolHash = "5dc56b130432098d18e911fdf4b3464adf850432b7fbb35003a2feff7c30a79f"
# The C3 image (4 bar buttons) is the required input.
$ExpectedSpellHash = "006a892b0b3363caedc7436f907948778fe6d084759fa0fc0ddc7f7603c03997"
$ExpectedSpellSize = 48985408
# The C6 output (visuals + ranges, deterministic, same size).
$ExpectedPatchedSpellHash = "5db5b7a52a4fad0e7c05ed6127fe95a437dce158332ae9b626ec99e2b7855e9b"
$ExpectedAreaHash = "1acef997a27f844a8abee9b477c44f2097f745168c6df85ece6b1a135568c233"
$ExpectedAreaSize = 362740
# C3 state file (from the C3v2 install).
$C3StateFile = Join-Path $UploadDir "G17C3_CLIENT_BAR_BUTTONS_STATE.txt"

$WorkRoot = ""; $BackupDir = ""; $RootMpq = ""; $LocaleMpq = ""; $Slot = "Z"
$TemporaryTarget = ""; $SwapOld = ""
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
    $Spell = Extract-ArchiveTarget -Archive $Archive -Target $SpellTarget -Tag ($Prefix + "_spell")
    $Area = Extract-ArchiveTarget -Archive $Archive -Target $AreaTarget -Tag ($Prefix + "_area")
    W ($Prefix + "_SPELL_SHA256=" + $Spell.Hash)
    W ($Prefix + "_SPELL_SIZE=" + $Spell.Size)
    W ($Prefix + "_AREA_SHA256=" + $Area.Hash)
    if ($Spell.State -ne "HIT" -or $Spell.Size -ne $ExpectedSpellSize) { throw "new archive Spell.dbc missing/wrong size" }
    if ($Area.State -ne "HIT" -or $Area.Hash -cne $ExpectedAreaHash -or $Area.Size -ne $ExpectedAreaSize) { throw "new archive AreaTable.dbc mismatch" }
    if ($Spell.Hash -cne $ExpectedPatchedSpellHash) { throw "new archive Spell.dbc is not the C6 image: $($Spell.Hash)" }
}

$BuildFingerprint = "v1_visuals_range"
try {
    W "G17C6_CLIENT_VISUALS_START"
    W ("C6_BUILD=" + $BuildFingerprint)
    W "SCOPE=PATCH_SPELLVISUAL_PER_BLOCK_PLUS_RANGE30_FOR_990000_990024"
    W "MODIFIES_SERVER=False"
    W "EXECUTES_SQL=False"
    if (Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Name -match '^(?i:wow|wow-64)$' }) {
        throw "Wow client is running; close it before upgrade"
    }
    foreach ($Required in @($Tool, $Patcher)) {
        if (-not (Test-Path -LiteralPath $Required -PathType Leaf)) { throw "required file missing: $Required" }
    }
    $patcherText = [IO.File]::ReadAllText($Patcher)
    if ($patcherText -notmatch 'G17B3R5_VISUAL_PATCHER_VERSION\s*=\s*"v1_visuals_range"') {
        throw ("OBSOLETE_PACKAGE: patcher is not v1_visuals_range")
    }
    W "C6_PATCHER_VERSION_CHECK=PASS v1_visuals_range"
    if (-not (Test-Path -LiteralPath $ClientRoot -PathType Container)) { throw "client root missing: $ClientRoot" }
    $ClientRoot = (Resolve-Path -LiteralPath $ClientRoot).Path
    $DataDir = Join-Path $ClientRoot "Data"
    if (-not (Test-Path -LiteralPath (Join-Path $ClientRoot "Wow.exe") -PathType Leaf) -or -not (Test-Path -LiteralPath $DataDir -PathType Container)) {
        throw "invalid WoW client root"
    }
    $ToolHash = (Get-FileHash -LiteralPath $Tool -Algorithm SHA256).Hash.ToLowerInvariant()
    W "MPQCLI_SHA256=$ToolHash"
    if ($ToolHash -cne $ExpectedToolHash -and -not $MpqCliOverride) { throw "mpqcli hash mismatch" }

    # --- environment: prefer the C3 state file, else content discovery ---
    $C3 = Read-KeyValueFile $C3StateFile
    if ($C3["INSTALL_STATUS"] -ceq "PASS" -and $C3["ROOT_MPQ"] -and $C3["LOCALE_MPQ"] -and $C3["NEW_MPQ_SHA256"]) {
        W "ENV_MODE=C3_STATE"
        $RootMpq = $C3["ROOT_MPQ"]
        $Slot = $C3["PATCH_SLOT"]
        if (-not $Slot -or $Slot -notmatch '^[A-Z]$') { $Slot = "Z" }
        $LocaleMpq = $C3["LOCALE_MPQ"]
        if (-not (Test-Path -LiteralPath $RootMpq -PathType Leaf)) { throw "C3 root MPQ missing: $RootMpq" }
        if (-not (Test-Path -LiteralPath $LocaleMpq -PathType Leaf)) { throw "C3 locale MPQ missing: $LocaleMpq" }
        $RootHash = (Get-FileHash -LiteralPath $RootMpq -Algorithm SHA256).Hash.ToLowerInvariant()
        $LocaleHash = (Get-FileHash -LiteralPath $LocaleMpq -Algorithm SHA256).Hash.ToLowerInvariant()
        W "C3_ROOT_MPQ=$RootMpq"
        W "C3_ROOT_MPQ_SHA256=$RootHash"
        W "C3_LOCALE_MPQ=$LocaleMpq"
        W "C3_LOCALE_MPQ_SHA256=$LocaleHash"
        if ($RootHash -cne $C3["NEW_MPQ_SHA256"].ToLowerInvariant()) { throw "root MPQ hash no longer matches the C3 state" }
        if ($RootHash -cne $LocaleHash) { throw "locale mirror no longer byte-identical to root" }
    } else {
        W "ENV_MODE=DISCOVERY"
        W ("ENV_REASON=c3_state_missing=" + [bool](Test-Path -LiteralPath $C3StateFile -PathType Leaf))
        $WorkRoot = Join-Path $UploadDir ("G17C6_Client_Work_" + (Get-Date -Format "yyyyMMdd_HHmmss") + "_" + $PID)
        New-Item -ItemType Directory -Path $WorkRoot | Out-Null
        $LocaleDir = Join-Path $DataDir "zhCN"
        if (-not (Test-Path -LiteralPath $LocaleDir -PathType Container)) { throw "zhCN locale directory missing" }
        $Slots = @("Z","Y","X","W","V","U","T","S","R","Q","P","O","N","M","L","K","J","I","H","G","F","E","D","C","B","A")
        $RootHits = @()
        foreach ($s in $Slots) {
            $cand = Join-Path $DataDir ("patch-" + $s + ".MPQ")
            if (-not (Test-Path -LiteralPath $cand)) { continue }
            if (Test-Path -LiteralPath $cand -PathType Container) { continue }
            $spell = Extract-ArchiveTarget -Archive $cand -Target $SpellTarget -Tag ("root_" + $s + "_spell")
            $area  = Extract-ArchiveTarget -Archive $cand -Target $AreaTarget -Tag ("root_" + $s + "_area")
            W ("ROOT_DISCOVERY=SLOT=$s;SPELL=$($spell.State);SPELL_SHA256=$($spell.Hash);AREA=$($area.State);AREA_SHA256=$($area.Hash);PATH=$cand")
            $isChain = ($spell.State -eq "HIT" -and $area.State -eq "HIT" -and
                        $spell.Size -eq $ExpectedSpellSize -and $area.Size -eq $ExpectedAreaSize -and
                        $area.Hash -ceq $ExpectedAreaHash -and
                        ($spell.Hash -ceq $ExpectedSpellHash -or $spell.Hash -ceq $ExpectedPatchedSpellHash))
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

    $ConfigWtf = Join-Path $ClientRoot "WTF\Config.wtf"
    $Locale = "UNKNOWN"
    if (Test-Path -LiteralPath $ConfigWtf -PathType Leaf) {
        foreach ($Line in [IO.File]::ReadAllLines($ConfigWtf)) {
            if ($Line -match '^SET\s+locale\s+"([A-Za-z]{4})"') { $Locale = $Matches[1]; break }
        }
    }
    W "CLIENT_LOCALE=$Locale"
    if ($Locale -cne "zhCN") { throw "zhCN locale required; detected $Locale" }

    if (-not $WorkRoot -or -not (Test-Path -LiteralPath $WorkRoot -PathType Container)) {
        $WorkRoot = Join-Path $UploadDir ("G17C6_Client_Work_" + (Get-Date -Format "yyyyMMdd_HHmmss") + "_" + $PID)
        New-Item -ItemType Directory -Path $WorkRoot | Out-Null
    }

    $OldSpell = Extract-ArchiveTarget -Archive $RootMpq -Target $SpellTarget -Tag "old_spell"
    $OldArea = Extract-ArchiveTarget -Archive $RootMpq -Target $AreaTarget -Tag "old_area"
    W "ROOT_OWNED_SPELL_SHA256=$($OldSpell.Hash)"
    W "ROOT_OWNED_SPELL_SIZE=$($OldSpell.Size)"
    W "ROOT_OWNED_AREA_SHA256=$($OldArea.Hash)"
    if ($OldArea.State -ne "HIT" -or $OldArea.Hash -cne $ExpectedAreaHash -or $OldArea.Size -ne $ExpectedAreaSize) {
        throw "root owned AreaTable.dbc mismatch"
    }
    if ($OldSpell.State -ne "HIT") { throw "root owned Spell.dbc missing" }
    if ($OldSpell.Hash -ceq $ExpectedPatchedSpellHash -and $OldSpell.Size -eq $ExpectedSpellSize) {
        W "G17C6_VISUAL_STATE=ALREADY_COMPLETE"
        W "G17C6_CLIENT_VISUALS=ALREADY_CURRENT"
        W "G17C6_CLIENT_VISUALS_RESULT=PASS"
        W "NOTE=client Spell.dbc already has the visuals+ranges; no write performed"
        W "RESULT_FILE=$Result"
        exit 0
    }
    if ($OldSpell.Hash -cne $ExpectedSpellHash -or $OldSpell.Size -ne $ExpectedSpellSize) {
        throw ("root owned Spell.dbc must be the C3 image 006a892b/$ExpectedSpellSize; found $($OldSpell.Hash)/$($OldSpell.Size) - run G17-C3v2 first if not")
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

    $GeneratedSpell = Join-Path $WorkRoot "generated\DBFilesClient\Spell.dbc"
    New-Item -ItemType Directory -Path (Split-Path -Parent $GeneratedSpell) -Force | Out-Null
    $rc = Invoke-NativeLogged -FilePath $Python -NativeArgs @($Patcher, "patch", "--input", $OldSpell.Path, "--output", $GeneratedSpell) -Prefix "SPELL_DBC_VISUAL_PATCH"
    W "SPELL_DBC_VISUAL_PATCH_EXIT=$rc"
    if ($rc -ne 0) { throw "Spell.dbc C6 visual patch failed" }
    $PatchedSpellHash = (Get-FileHash -LiteralPath $GeneratedSpell -Algorithm SHA256).Hash.ToLowerInvariant()
    W "GENERATED_SPELL_SHA256=$PatchedSpellHash"
    $generatedSize = (Get-Item -LiteralPath $GeneratedSpell).Length
    if ($generatedSize -ne $ExpectedSpellSize) { throw "patched Spell.dbc size mismatch: $generatedSize" }
    if ($PatchedSpellHash -cne $ExpectedPatchedSpellHash) { throw "patched Spell.dbc hash mismatch: $PatchedSpellHash" }

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

    $Stamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $BackupDir = Join-Path $UploadDir ("G17C6_Client_Visuals_Backup_" + $Stamp)
    if (Test-Path -LiteralPath $BackupDir) { throw "backup directory already exists" }
    New-Item -ItemType Directory -Path $BackupDir | Out-Null
    $BackupRoot = Join-Path $BackupDir ("before_G17C6_" + (Split-Path -Leaf $RootMpq))
    Copy-Item -LiteralPath $RootMpq -Destination $BackupRoot
    if ((Get-FileHash -LiteralPath $BackupRoot -Algorithm SHA256).Hash.ToLowerInvariant() -cne (Get-FileHash -LiteralPath $RootMpq -Algorithm SHA256).Hash.ToLowerInvariant()) { throw "backup root MPQ verification failed" }
    W "BACKUP_ROOT=$BackupRoot"
    $BackupLocale = Join-Path $BackupDir ("before_G17C6_" + (Split-Path -Leaf $LocaleMpq))
    Copy-Item -LiteralPath $LocaleMpq -Destination $BackupLocale
    W "BACKUP_LOCALE=$BackupLocale"
    W "BACKUP_DIR=$BackupDir"

    $TemporaryTarget = $RootMpq + ".g17c6.new.tmp"
    $SwapOld = $RootMpq + ".g17c6.old.tmp"
    if (Test-Path -LiteralPath $TemporaryTarget -PathType Leaf) { Remove-Item -LiteralPath $TemporaryTarget -Force -ErrorAction SilentlyContinue }
    if (Test-Path -LiteralPath $SwapOld -PathType Leaf) { Remove-Item -LiteralPath $SwapOld -Force -ErrorAction SilentlyContinue }
    Copy-Item -LiteralPath $BuiltArchive -Destination $TemporaryTarget
    if ((Get-FileHash -LiteralPath $TemporaryTarget -Algorithm SHA256).Hash.ToLowerInvariant() -cne $NewArchiveHash) { throw "temp MPQ hash mismatch" }
    Move-Item -LiteralPath $RootMpq -Destination $SwapOld
    Move-Item -LiteralPath $TemporaryTarget -Destination $RootMpq
    Assert-NewArchive -Archive $RootMpq -Prefix "INSTALLED_ROOT"
    if ((Get-FileHash -LiteralPath $RootMpq -Algorithm SHA256).Hash.ToLowerInvariant() -cne $NewArchiveHash) { throw "installed root MPQ hash mismatch" }

    $LocaleTmp = $LocaleMpq + ".g17c6.new.tmp"
    $LocaleSwap = $LocaleMpq + ".g17c6.old.tmp"
    if (Test-Path -LiteralPath $LocaleTmp -PathType Leaf) { Remove-Item -LiteralPath $LocaleTmp -Force -ErrorAction SilentlyContinue }
    if (Test-Path -LiteralPath $LocaleSwap -PathType Leaf) { Remove-Item -LiteralPath $LocaleSwap -Force -ErrorAction SilentlyContinue }
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
        ("NEW_MPQ_SHA256=" + $NewArchiveHash),
        ("BACKUP_DIR=" + $BackupDir),
        ("INSTALLED_AT=" + (Get-Date).ToString("o"))
    )
    $StateFile = Join-Path $UploadDir "G17C6_CLIENT_VISUALS_STATE.txt"
    $StateTemp = $StateFile + ".tmp"
    [IO.File]::WriteAllText($StateTemp, (($StateLines -join [Environment]::NewLine) + [Environment]::NewLine), $Utf8NoBom)
    Move-Item -LiteralPath $StateTemp -Destination $StateFile -Force
    $StateCommitted = $true
    Remove-Item -LiteralPath $SwapOld -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $LocaleSwap -Force -ErrorAction SilentlyContinue
    $SwapOld = ""

    W "CLIENT_RESTART_REQUIRED=True"
    W "G17C6_CLIENT_VISUALS=PASS"
    W "G17C6_CLIENT_VISUALS_RESULT=PASS"
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
        } catch { W ("AUTO_ROLLBACK_ERROR=" + $_.Exception.Message) }
    }
    W ("G17C6_CLIENT_VISUALS_ERROR=" + $Message)
    W "G17C6_CLIENT_VISUALS_RESULT=FAIL"
    W "RESULT_FILE=$Result"
    exit 1
} finally {
    if ($TemporaryTarget -and (Test-Path -LiteralPath $TemporaryTarget -PathType Leaf)) { Remove-Item -LiteralPath $TemporaryTarget -Force -ErrorAction SilentlyContinue }
    if ($WorkRoot -and (Test-Path -LiteralPath $WorkRoot -PathType Container)) { Remove-Item -LiteralPath $WorkRoot -Recurse -Force -ErrorAction SilentlyContinue }
}
