#requires -Version 5.1
# G17-C12: CLIENT MOD v2 - make the eight C11 buttons FIT the stock skin art
# (user report: ui frame too small for 8 buttons).  One tunable line in
# VehicleMenuBar_UpdateActionBars: scale the whole action-button frame to
# 0.75 so eight 62px buttons fit the original six-button footprint.
# The xml (buttons 7/8) and the DBC files pass through unchanged from C11.
param(
    [string]$Workspace = "C:\Users\Administrator\Downloads\workspace",
    [string]$ClientRoot = "D:\WOW",
    [string]$MpqCliOverride = ""
)
$ErrorActionPreference = "Stop"
$UploadDir = Join-Path $Workspace "uploads"
$Result = Join-Path $UploadDir "G17C12_CLIENTMOD_RESULT.txt"
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$Patcher = Join-Path $PSScriptRoot "tools\patch_g17c12.py"
$Tool = if ($MpqCliOverride) { $MpqCliOverride } else { Join-Path $PSScriptRoot "third_party\mpqcli-windows-amd64.exe" }
$PayloadLua = Join-Path $PSScriptRoot "payload\Interface\FrameXML\VehicleMenuBar.lua"
$PayloadXml = Join-Path $PSScriptRoot "payload\Interface\FrameXML\VehicleMenuBar.xml"
$SpellTarget = "DBFilesClient\Spell.dbc"
$AreaTarget = "DBFilesClient\AreaTable.dbc"
$LuaTarget = "Interface\FrameXML\VehicleMenuBar.lua"
$XmlTarget = "Interface\FrameXML\VehicleMenuBar.xml"
$ExpectedToolHash = "5dc56b130432098d18e911fdf4b3464adf850432b7fbb35003a2feff7c30a79f"
$ExpectedAreaHash = "1acef997a27f844a8abee9b477c44f2097f745168c6df85ece6b1a135568c233"
$ExpectedAreaSize = 362740
$ExpectedLuaHash = "071e688725661511d8471b6c5534c7abbadaf0538e0e0fb34750a75ecf848ef6"
$ExpectedXmlHash = "31563ecf8787054408bab4049f212b2b77aa56c12dd264d60aa03b1494f1e628"
$BuildFingerprint = "v1_8slot_bar_fits"
$StateFiles = @(
    (Join-Path $UploadDir "G17C12_CLIENTMOD_STATE.txt"),
    (Join-Path $UploadDir "G17C10_CLIENT_VISUALS_STATE.txt"),
    (Join-Path $UploadDir "G17C9_CLIENT_VISUALS_STATE.txt"),
    (Join-Path $UploadDir "G17C8_STATE.txt"),
    (Join-Path $UploadDir "G17C6_CLIENT_VISUALS_STATE.txt"),
    (Join-Path $UploadDir "G17C3_CLIENT_BAR_BUTTONS_STATE.txt")
)
$ResultStateFile = Join-Path $UploadDir "G17C12_CLIENTMOD_STATE.txt"

$WorkRoot = ""; $BackupDir = ""; $RootMpq = ""; $LocaleMpq = ""; $Slot = "Z"
$SwapOld = ""; $NewArchiveHash = ""; $StateCommitted = $false

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
    W "G17C12_CLIENTMOD_START"
    W ("G17C12_BUILD=" + $BuildFingerprint)
    W "SCOPE=ADD_INTERFACE_FRAMEXML_VEHICLEMENUBAR_8SLOT_TO_G17_MPQ_CHAIN"
    W "MODIFIES_SERVER=False"
    W "EXECUTES_SQL=False"
    W "CLIENT_MOD=True"
    if (Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Name -match '^(?i:wow|wow-64)$' }) {
        throw "Wow client is running; close it before upgrade"
    }
    foreach ($Required in @($Tool, $Patcher, $PayloadLua, $PayloadXml)) {
        if (-not (Test-Path -LiteralPath $Required -PathType Leaf)) { throw "required file missing: $Required" }
    }

    # --- version gate (the C9-v1 lesson: grep the REAL variable name) ---
    $patcherText = [IO.File]::ReadAllText($Patcher)
    if ($patcherText -notmatch 'G17C12_VERSION\s*=\s*"v1_8slot_bar_fits"') {
        throw ("OBSOLETE_PACKAGE: patcher is not v1_8slot_bar_fits")
    }
    W "G17C12_PATCHER_VERSION_CHECK=PASS v1_8slot_bar_fits"

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

    $rc = Invoke-NativeLogged -FilePath $Python -NativeArgs @($Patcher, "verify") -Prefix "PAYLOAD_VERIFY"
    if ($rc -ne 0) { throw "payload verify failed" }

    $WorkRoot = Join-Path $UploadDir ("G17C12_Client_Work_" + (Get-Date -Format "yyyyMMdd_HHmmss") + "_" + $PID)
    New-Item -ItemType Directory -Path $WorkRoot | Out-Null

    # --- environment: state files give PATHS ONLY (C8-proven) ---
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
                        $area.Hash -ceq $ExpectedAreaHash -and $area.Size -eq $ExpectedAreaSize)
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

    # --- current chain contents (DBC passthrough + UI state) ---
    $OldSpell = Extract-ArchiveTarget -Archive $RootMpq -Target $SpellTarget -Tag "old_spell"
    $OldArea = Extract-ArchiveTarget -Archive $RootMpq -Target $AreaTarget -Tag "old_area"
    $OldLua = Extract-ArchiveTarget -Archive $RootMpq -Target $LuaTarget -Tag "old_lua"
    W ("ROOT_OWNED_SPELL_SHA256=" + $OldSpell.Hash)
    W ("ROOT_OWNED_AREA_SHA256=" + $OldArea.Hash)
    W ("ROOT_OWNED_LUA_STATE=" + $OldLua.State)
    if ($OldArea.State -ne "HIT" -or $OldArea.Hash -cne $ExpectedAreaHash -or $OldArea.Size -ne $ExpectedAreaSize) {
        throw "root owned AreaTable.dbc mismatch (not the G17 chain owner?)"
    }
    if ($OldSpell.State -ne "HIT") { throw "root owned Spell.dbc missing" }

    if ($OldLua.State -eq "HIT" -and $OldLua.Hash -ceq $ExpectedLuaHash) {
        W "G17C12_STATE=ALREADY_COMPLETE"
        W "G17C12_CLIENTMOD=ALREADY_CURRENT"
        W "G17C12_CLIENTMOD_RESULT=PASS"
        W "NOTE=chain MPQ already carries the 8-slot VehicleMenuBar; no write performed"
        W "RESULT_FILE=$Result"
        exit 0
    }

    # --- rebuild the chain MPQ with the FrameXML files added ---
    $PackRoot = Join-Path $WorkRoot "pack_root"
    $PackSpell = Join-Path $PackRoot $SpellTarget
    $PackArea = Join-Path $PackRoot $AreaTarget
    $PackLua = Join-Path $PackRoot $LuaTarget
    $PackXml = Join-Path $PackRoot $XmlTarget
    New-Item -ItemType Directory -Path (Split-Path -Parent $PackSpell) -Force | Out-Null
    New-Item -ItemType Directory -Path (Split-Path -Parent $PackLua) -Force | Out-Null
    Copy-Item -LiteralPath $OldSpell.Path -Destination $PackSpell
    Copy-Item -LiteralPath $OldArea.Path -Destination $PackArea
    Copy-Item -LiteralPath $PayloadLua -Destination $PackLua
    Copy-Item -LiteralPath $PayloadXml -Destination $PackXml
    $BuiltArchive = Join-Path $WorkRoot ("patch-" + $Slot + ".MPQ")
    $CreateExit = Invoke-NativeLogged -FilePath $Tool -NativeArgs @("create", "--game", "wow-wotlk", "--output", $BuiltArchive, $PackRoot) -Prefix "MPQ_CREATE"
    W "MPQ_CREATE_EXIT=$CreateExit"
    if ($CreateExit -ne 0 -or -not (Test-Path -LiteralPath $BuiltArchive -PathType Leaf)) { throw "new MPQ creation failed" }

    # --- round-trip verification of ALL FOUR files ---
    $BuiltSpell = Extract-ArchiveTarget -Archive $BuiltArchive -Target $SpellTarget -Tag "built_spell"
    $BuiltArea = Extract-ArchiveTarget -Archive $BuiltArchive -Target $AreaTarget -Tag "built_area"
    $BuiltLua = Extract-ArchiveTarget -Archive $BuiltArchive -Target $LuaTarget -Tag "built_lua"
    $BuiltXml = Extract-ArchiveTarget -Archive $BuiltArchive -Target $XmlTarget -Tag "built_xml"
    W ("BUILT_MPQ_SPELL_SHA256=" + $BuiltSpell.Hash)
    W ("BUILT_MPQ_AREA_SHA256=" + $BuiltArea.Hash)
    W ("BUILT_MPQ_LUA_SHA256=" + $BuiltLua.Hash)
    W ("BUILT_MPQ_XML_SHA256=" + $BuiltXml.Hash)
    if ($BuiltSpell.State -ne "HIT" -or $BuiltSpell.Hash -cne $OldSpell.Hash) { throw "built archive Spell.dbc passthrough mismatch" }
    if ($BuiltArea.State -ne "HIT" -or $BuiltArea.Hash -cne $ExpectedAreaHash -or $BuiltArea.Size -ne $ExpectedAreaSize) { throw "built archive AreaTable.dbc mismatch" }
    if ($BuiltLua.State -ne "HIT" -or $BuiltLua.Hash -cne $ExpectedLuaHash) { throw "built archive VehicleMenuBar.lua mismatch" }
    if ($BuiltXml.State -ne "HIT" -or $BuiltXml.Hash -cne $ExpectedXmlHash) { throw "built archive VehicleMenuBar.xml mismatch" }
    $NewArchiveHash = (Get-FileHash -LiteralPath $BuiltArchive -Algorithm SHA256).Hash.ToLowerInvariant()
    W "NEW_MPQ_SHA256=$NewArchiveHash"
    W ("NEW_MPQ_SIZE=" + (Get-Item -LiteralPath $BuiltArchive).Length)

    # --- backup before swap ---
    $Stamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $BackupDir = Join-Path $UploadDir ("G17C12_Client_Backup_" + $Stamp)
    if (Test-Path -LiteralPath $BackupDir) { throw "backup directory already exists" }
    New-Item -ItemType Directory -Path $BackupDir | Out-Null
    $BackupRoot = Join-Path $BackupDir "before_G17C12_root.MPQ"
    Copy-Item -LiteralPath $RootMpq -Destination $BackupRoot
    if ((Get-FileHash -LiteralPath $BackupRoot -Algorithm SHA256).Hash.ToLowerInvariant() -cne (Get-FileHash -LiteralPath $RootMpq -Algorithm SHA256).Hash.ToLowerInvariant()) { throw "backup root MPQ verification failed" }
    $BackupLocale = Join-Path $BackupDir "before_G17C12_locale.MPQ"
    Copy-Item -LiteralPath $LocaleMpq -Destination $BackupLocale
    W "BACKUP_ROOT=$BackupRoot"
    W "BACKUP_LOCALE=$BackupLocale"
    W "BACKUP_DIR=$BackupDir"

    # --- swap root ---
    $TemporaryTarget = $RootMpq + ".g17c12.new.tmp"
    $SwapOld = $RootMpq + ".g17c12.old.tmp"
    if (Test-Path -LiteralPath $TemporaryTarget -PathType Leaf) { Remove-Item -LiteralPath $TemporaryTarget -Force -ErrorAction SilentlyContinue }
    if (Test-Path -LiteralPath $SwapOld -PathType Leaf) { Remove-Item -LiteralPath $SwapOld -Force -ErrorAction SilentlyContinue }
    Copy-Item -LiteralPath $BuiltArchive -Destination $TemporaryTarget
    if ((Get-FileHash -LiteralPath $TemporaryTarget -Algorithm SHA256).Hash.ToLowerInvariant() -cne $NewArchiveHash) { throw "temp MPQ hash mismatch" }
    Move-Item -LiteralPath $RootMpq -Destination $SwapOld
    Move-Item -LiteralPath $TemporaryTarget -Destination $RootMpq
    if ((Get-FileHash -LiteralPath $RootMpq -Algorithm SHA256).Hash.ToLowerInvariant() -cne $NewArchiveHash) { throw "installed root MPQ hash mismatch" }
    $InstalledLua = Extract-ArchiveTarget -Archive $RootMpq -Target $LuaTarget -Tag "installed_lua"
    if ($InstalledLua.State -ne "HIT" -or $InstalledLua.Hash -cne $ExpectedLuaHash) { throw "installed root MPQ VehicleMenuBar.lua verification failed" }
    W "INSTALLED_ROOT_VERIFIED=True"

    # --- mirror locale ---
    $LocaleTmp = $LocaleMpq + ".g17c12.new.tmp"
    $LocaleSwap = $LocaleMpq + ".g17c12.old.tmp"
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
        "STATE_FORMAT=1",
        ("BUILD=" + $BuildFingerprint),
        "INSTALL_STATUS=PASS",
        ("CLIENT_ROOT=" + $ClientRoot),
        ("ROOT_MPQ=" + $RootMpq),
        ("PATCH_SLOT=" + $Slot),
        ("LOCALE_MPQ=" + $LocaleMpq),
        ("OLD_SPELL_DBC_SHA256=" + $OldSpell.Hash),
        ("VEHICLEMENUBAR_LUA_SHA256=" + $ExpectedLuaHash),
        ("VEHICLEMENUBAR_XML_SHA256=" + $ExpectedXmlHash),
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
    W "G17C12_CLIENTMOD=PASS"
    W "G17C12_CLIENTMOD_RESULT=PASS"
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
    W ("G17C12_CLIENTMOD_ERROR=" + $Message)
    W "G17C12_CLIENTMOD_RESULT=FAIL"
    W "RESULT_FILE=$Result"
    exit 1
}
