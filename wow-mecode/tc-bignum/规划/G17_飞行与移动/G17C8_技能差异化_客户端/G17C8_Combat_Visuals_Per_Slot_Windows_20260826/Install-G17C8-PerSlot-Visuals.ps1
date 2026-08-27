#requires -Version 5.1
# G17-C7: fix the 25 combat carriers' Effect_1 from DUMMY(3) to SCHOOL_DAMAGE(2).
# ROOT CAUSE: 3.3.5 client only renders visuals for SCHOOL_DAMAGE effects.
# DUMMY effects are completely inert client-side even with SpellVisualID set.
# This changes Effect_1 to SCHOOL_DAMAGE, sets ImplicitTargetA to 18 (current
# target), and ensures visuals/ranges (idempotent with C6/B3R4).
# Patches BOTH the server DBC and the client MPQ chain in one step.
param(
    [string]$Workspace = "C:\Users\Administrator\Downloads\workspace",
    [string]$SourceRoot = "D:\TrinityCore",
    [string]$BuildRoot = "D:\TC-Build",
    [string]$ClientRoot = "D:\WOW",
    [string]$MpqCliOverride = ""
)
$ErrorActionPreference = "Stop"
$UploadDir = Join-Path $Workspace "uploads"
$Result = Join-Path $UploadDir "G17C8_RESULT.txt"
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$Patcher = Join-Path $PSScriptRoot "tools\patch_g17c8_full.py"
$Tool = if ($MpqCliOverride) { $MpqCliOverride } else { Join-Path $PSScriptRoot "third_party\mpqcli-windows-amd64.exe" }
$ServerDbc = Join-Path $BuildRoot "bin\RelWithDebInfo\dbc\Spell.dbc"
$SpellTarget = "DBFilesClient\Spell.dbc"
$AreaTarget = "DBFilesClient\AreaTable.dbc"
$ExpectedAreaHash = "1acef997a27f844a8abee9b477c44f2097f745168c6df85ece6b1a135568c233"
$ExpectedAreaSize = 362740
$C3StateFile = Join-Path $UploadDir "G17C3_CLIENT_BAR_BUTTONS_STATE.txt"
$C6StateFile = Join-Path $UploadDir "G17C6_CLIENT_VISUALS_STATE.txt"

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
function Extract-ArchiveTarget([string]$Archive, [string]$Target, [string]$Tag, [string]$WorkRoot) {
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

$WorkRoot = ""; $BackupDir = ""; $RootMpq = ""; $LocaleMpq = ""; $Slot = "Z"
$StateCommitted = $false

try {
    W "G17C8_START"
    W "C7_BUILD=v1_perslot_cd"
    W "SCOPE=EFFECT_1_DUMMY_TO_SCHOOL_DAMAGE_FOR_990000_990024_CLIENT_AND_SERVER"
    W "ROOT_CAUSE=3.3.5 client renders visuals only for SCHOOL_DAMAGE(2), not DUMMY(3)"
    if (Get-Process worldserver -ErrorAction SilentlyContinue) { throw "worldserver is running; stop it first" }
    if (Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Name -match '^(?i:wow|wow-64)$' }) { throw "Wow client is running; close it first" }
    foreach ($Required in @($Patcher, $Tool, $ServerDbc)) {
        if (-not (Test-Path -LiteralPath $Required -PathType Leaf)) { throw "required file missing: $Required" }
    }
    $python = Find-Python
    if (-not $python) { throw "Python not found" }
    W "PYTHON=$python"

    # ==================== PART 1: SERVER DBC FIX ====================
    W ""
    W "=== PART 1: SERVER DBC ==="
    $srvOut = @(& $python $Patcher check --input $ServerDbc 2>&1)
    $srvExit = $LASTEXITCODE
    $srvState = @($srvOut | Where-Object { $_ -match '^G17C8_STATE=' } | Select-Object -First 1)[0]
    foreach ($line in $srvOut) { W ("SRV_CHECK|" + $line.ToString()) }
    W "SRV_CHECK_EXIT=$srvExit"
    if ($srvState) { W "SRV_STATE=$($srvState.ToString().Trim())" }
    if ($srvExit -ne 0) { throw "server DBC check failed" }

    if ($srvState -match 'G17C8_STATE=COMPLETE') {
        W "G17C8_SERVER_DBC=ALREADY_COMPLETE"
    } else {
        $srvBackup = Join-Path $UploadDir ("G17C8_Server_DBC_Backup_" + (Get-Date -Format "yyyyMMdd_HHmmss"))
        New-Item -ItemType Directory -Path $srvBackup -Force | Out-Null
        Copy-Item -LiteralPath $ServerDbc -Destination (Join-Path $srvBackup "Spell.dbc.before_g17c7")
        $srvTmp = $ServerDbc + ".g17c7.new"
        if (Test-Path -LiteralPath $srvTmp) { Remove-Item -LiteralPath $srvTmp -Force -ErrorAction SilentlyContinue }
        $rc = Invoke-NativeLogged -FilePath $python -NativeArgs @($Patcher, "patch", "--input", $ServerDbc, "--output", $srvTmp) -Prefix "SRV_PATCH"
        W "SRV_PATCH_EXIT=$rc"
        if ($rc -ne 0) { throw "server DBC patch failed" }
        $oldSize = (Get-Item -LiteralPath $ServerDbc).Length
        $newSize = (Get-Item -LiteralPath $srvTmp).Length
        if ($newSize -ne $oldSize) { throw "server DBC size changed (must be in-place)" }
        $srvOld = $ServerDbc + ".g17c7.old"
        Move-Item -LiteralPath $ServerDbc -Destination $srvOld
        Move-Item -LiteralPath $srvTmp -Destination $ServerDbc
        Remove-Item -LiteralPath $srvOld -Force -ErrorAction SilentlyContinue
        W "SERVER_DBC_BACKUP=$srvBackup"
        W "G17C8_SERVER_DBC=PASS"
    }

    # ==================== PART 2: CLIENT MPQ FIX ====================
    W ""
    W "=== PART 2: CLIENT MPQ ==="
    $DataDir = Join-Path $ClientRoot "Data"
    if (-not (Test-Path -LiteralPath (Join-Path $ClientRoot "Wow.exe") -PathType Leaf)) { throw "client root invalid" }

    # Find the root MPQ (prefer state files, else scan)
    $C6 = Read-KeyValueFile $C6StateFile
    $C3 = Read-KeyValueFile $C3StateFile
    $stateSrc = if ($C6["INSTALL_STATUS"] -ceq "PASS" -and $C6["ROOT_MPQ"]) { $C6 }
                elseif ($C3["INSTALL_STATUS"] -ceq "PASS" -and $C3["ROOT_MPQ"]) { $C3 }
                else { $null }

    $WorkRoot = Join-Path $UploadDir ("G17C8_Client_Work_" + (Get-Date -Format "yyyyMMdd_HHmmss") + "_" + $PID)
    New-Item -ItemType Directory -Path $WorkRoot | Out-Null

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
        $Slots = @("Z","Y","X","W","V","U","T","S","R","Q","P","O","N","M","L","K","J","I","H","G","F","E","D","C","B","A")
        $RootHits = @()
        foreach ($s in $Slots) {
            $cand = Join-Path $DataDir ("patch-" + $s + ".MPQ")
            if (-not (Test-Path -LiteralPath $cand -PathType Leaf)) { continue }
            $spell = Extract-ArchiveTarget -Archive $cand -Target $SpellTarget -Tag "root_$s" -WorkRoot $WorkRoot
            if ($spell.State -eq "HIT") { $RootHits += [pscustomobject]@{ Path = $cand; Slot = $s } }
        }
        if ($RootHits.Count -ne 1) { throw "expected 1 root MPQ owner, found $($RootHits.Count)" }
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
        if (-not $LocaleMpq) { throw "no locale mirror found" }
    }
    W "ROOT_MPQ=$RootMpq"
    W "LOCALE_MPQ=$LocaleMpq"

    # Extract and check
    $OldSpell = Extract-ArchiveTarget -Archive $RootMpq -Target $SpellTarget -Tag "old_spell" -WorkRoot $WorkRoot
    $OldArea = Extract-ArchiveTarget -Archive $RootMpq -Target $AreaTarget -Tag "old_area" -WorkRoot $WorkRoot
    if ($OldSpell.State -ne "HIT") { throw "root Spell.dbc not found in MPQ" }
    if ($OldArea.State -ne "HIT" -or $OldArea.Hash -cne $ExpectedAreaHash) { throw "AreaTable mismatch" }
    W "CLIENT_SPELL_SHA256=$($OldSpell.Hash)"

    # Check current state
    $cliOut = @(& $python $Patcher check --input $OldSpell.Path 2>&1)
    $cliState = @($cliOut | Where-Object { $_ -match '^G17C8_STATE=' } | Select-Object -First 1)[0]
    foreach ($line in $cliOut) { W ("CLI_CHECK|" + $line.ToString()) }
    if (-not $cliState) { throw "client check printed no state" }
    W "CLI_STATE=$($cliState.ToString().Trim())"

    if ($cliState -match 'G17C8_STATE=COMPLETE') {
        W "G17C8_CLIENT_MPQ=ALREADY_COMPLETE"
    } else {
        # Patch
        $GeneratedSpell = Join-Path $WorkRoot "generated\DBFilesClient\Spell.dbc"
        New-Item -ItemType Directory -Path (Split-Path -Parent $GeneratedSpell) -Force | Out-Null
        $rc = Invoke-NativeLogged -FilePath $python -NativeArgs @($Patcher, "patch", "--input", $OldSpell.Path, "--output", $GeneratedSpell) -Prefix "CLI_PATCH"
        if ($rc -ne 0) { throw "client Spell.dbc patch failed" }
        $PatchedHash = (Get-FileHash -LiteralPath $GeneratedSpell -Algorithm SHA256).Hash.ToLowerInvariant()
        W "GENERATED_SPELL_SHA256=$PatchedSpellHash"
        if ((Get-Item -LiteralPath $GeneratedSpell).Length -ne $OldSpell.Size) { throw "size mismatch" }

        # Rebuild MPQ
        $PackRoot = Join-Path $WorkRoot "pack_root"
        $PackSpell = Join-Path $PackRoot $SpellTarget
        $PackArea = Join-Path $PackRoot $AreaTarget
        New-Item -ItemType Directory -Path (Split-Path -Parent $PackSpell) -Force | Out-Null
        Copy-Item -LiteralPath $GeneratedSpell -Destination $PackSpell
        Copy-Item -LiteralPath $OldArea.Path -Destination $PackArea
        $BuiltArchive = Join-Path $WorkRoot ("patch-" + $Slot + ".MPQ")
        $rc = Invoke-NativeLogged -FilePath $Tool -NativeArgs @("create", "--game", "wow-wotlk", "--output", $BuiltArchive, $PackRoot) -Prefix "MPQ_CREATE"
        if ($rc -ne 0) { throw "MPQ creation failed" }
        $NewHash = (Get-FileHash -LiteralPath $BuiltArchive -Algorithm SHA256).Hash.ToLowerInvariant()
        W "NEW_MPQ_SHA256=$NewHash"

        # Backup
        $BackupDir = Join-Path $UploadDir ("G17C8_Client_Backup_" + (Get-Date -Format "yyyyMMdd_HHmmss"))
        New-Item -ItemType Directory -Path $BackupDir -Force | Out-Null
        Copy-Item -LiteralPath $RootMpq -Destination (Join-Path $BackupDir "before_G17C8_root.MPQ")
        Copy-Item -LiteralPath $LocaleMpq -Destination (Join-Path $BackupDir "before_G17C8_locale.MPQ")
        W "BACKUP_DIR=$BackupDir"

        # Swap root
        $Tmp = $RootMpq + ".g17c7.tmp"
        $Old = $RootMpq + ".g17c7.old"
        Copy-Item -LiteralPath $BuiltArchive -Destination $Tmp
        Move-Item -LiteralPath $RootMpq -Destination $Old
        Move-Item -LiteralPath $Tmp -Destination $RootMpq
        Remove-Item -LiteralPath $Old -Force -ErrorAction SilentlyContinue

        # Mirror locale
        Copy-Item -LiteralPath $RootMpq -Destination ($LocaleMpq + ".g17c7.tmp")
        Move-Item -LiteralPath $LocaleMpq -Destination ($LocaleMpq + ".g17c7.old") -ErrorAction SilentlyContinue
        Move-Item -LiteralPath ($LocaleMpq + ".g17c7.tmp") -Destination $LocaleMpq
        Remove-Item -LiteralPath ($LocaleMpq + ".g17c7.old") -Force -ErrorAction SilentlyContinue

        # Clear cache
        $CacheDir = Join-Path $ClientRoot "Cache"
        if (Test-Path -LiteralPath $CacheDir -PathType Container) {
            Remove-Item -LiteralPath $CacheDir -Recurse -Force
            W "CLIENT_CACHE_REMOVED=True"
        }

        # State file
        $StateFile = Join-Path $UploadDir "G17C8_STATE.txt"
        $StateTemp = $StateFile + ".tmp"
        $StateLines = @(
            "STATE_FORMAT=1", "INSTALL_STATUS=PASS",
            ("ROOT_MPQ=" + $RootMpq), ("LOCALE_MPQ=" + $LocaleMpq),
            ("NEW_MPQ_SHA256=" + $NewHash), ("BACKUP_DIR=" + $BackupDir),
            ("INSTALLED_AT=" + (Get-Date).ToString("o"))
        )
        [IO.File]::WriteAllText($StateTemp, (($StateLines -join [Environment]::NewLine) + [Environment]::NewLine), $Utf8NoBom)
        Move-Item -LiteralPath $StateTemp -Destination $StateFile -Force
        $StateCommitted = $true

        W "G17C8_CLIENT_MPQ=PASS"
    }

    W ""
    W "G17C8_RESULT=PASS"
    W "NEXT=Restart worldserver + WoW client. Mount a dragon, switch to combat page, press a skill - you should see REAL spell visuals (fire breath / claw bite / arcane blast / bomb / cleave)."
    W "RESULT_FILE=$Result"
    exit 0
} catch {
    W ("G17C8_ERROR=" + $_.Exception.Message)
    W "G17C8_RESULT=FAIL"
    W "RESULT_FILE=$Result"
    exit 1
} finally {
    if ($WorkRoot -and (Test-Path -LiteralPath $WorkRoot -PathType Container)) {
        Remove-Item -LiteralPath $WorkRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}
