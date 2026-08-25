#requires -Version 5.1
# G17-B2R4: unlock spell 52226 in the SERVER Spell.dbc.
# Pure DBC-file patch - NO C++ rebuild, NO SQL, NO client change.
#   Backup dbc\Spell.dbc -> patch 52226 (RequiresSpellFocus=1553 ->
#   0, CasterAuraSpell=52255 -> 0) -> verify -> restart worldserver.
param(
    [string]$Workspace = "C:\Users\Administrator\Downloads\workspace",
    [string]$RunDir = "D:\TC-Build\bin\RelWithDebInfo"
)
$ErrorActionPreference = "Stop"
$UploadDir = Join-Path $Workspace "uploads"
$Result = Join-Path $UploadDir "G17B2R4_SERVER_SPELL_DBC_RESULT.txt"
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$Patcher = Join-Path $PSScriptRoot "tools\patch_g17c1_spell_dbc.py"
$Target = Join-Path $RunDir "dbc\Spell.dbc"
# R4 install-time server Spell.dbc (the zhCN original this fork ships;
# semantic guards below are the real safety net).
$KnownPreimage = "df44e75ef1730e363dc06f1bc5ae064299b08d2d0047e663c0a1782ed4c8d10f"
$ExpectedSize = 48956359

New-Item -ItemType Directory -Path $UploadDir -Force | Out-Null
[IO.File]::WriteAllText($Result, "", $Utf8NoBom)
function W([string]$Text) {
    Write-Host $Text
    [IO.File]::AppendAllText($Result, $Text + [Environment]::NewLine, $Utf8NoBom)
}
function Find-Python {
    $candidates = @(
        (Join-Path $env:LOCALAPPDATA "Programs\Python\Python312\python.exe"),
        (Join-Path $env:LOCALAPPDATA "Programs\Python\Python310\python.exe"),
        "C:\Python312\python.exe", "C:\Python310\python.exe"
    )
    $python = @($candidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf })[0]
    if (-not $python) {
        $cmd = Get-Command python.exe -ErrorAction SilentlyContinue
        if ($cmd -and $cmd.Source -notmatch "\\WindowsApps\\") { $python = $cmd.Source }
    }
    return $python
}
function Invoke-NativeLogged {
    param([string]$FilePath, [string[]]$NativeArgs, [string]$Prefix)
    $old = $ErrorActionPreference
    $out = @(); $rc = 9009
    try {
        $ErrorActionPreference = "Continue"
        $out = @(& $FilePath @NativeArgs 2>&1)
        $rc = $LASTEXITCODE
    } finally { $ErrorActionPreference = $old }
    foreach ($line in $out) { W ($Prefix + "|" + $line.ToString()) }
    return [int]$rc
}
try {
    W "G17B2R4_SERVER_SPELL_DBC_START"
    W "SCOPE=SPELL_52226_FOCUS_1553_AND_AURA_52255_CLEARED_IN_SERVER_DBC_FILE"
    W "REBUILDS_CPP=False"
    W "EXECUTES_SQL=False"
    W "MODIFIES_CLIENT=False"
    if (Get-Process worldserver -ErrorAction SilentlyContinue) {
        throw "worldserver is running; stop it before patching dbc\Spell.dbc"
    }
    foreach ($file in @($Patcher, $Target)) {
        if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw "file missing: $file" }
    }
    $python = Find-Python
    if (-not $python) { throw "Python312/Python310 not found" }
    W "PYTHON=$python"
    $before = (Get-FileHash -LiteralPath $Target -Algorithm SHA256).Hash.ToLowerInvariant()
    $size = (Get-Item -LiteralPath $Target).Length
    W "TARGET=$Target"
    W "SERVER_SPELL_DBC_SHA256_BEFORE=$before"
    W "SERVER_SPELL_DBC_SIZE=$size"
    if ($size -ne $ExpectedSize) {
        throw "unexpected server Spell.dbc size: $size (expected $ExpectedSize)"
    }
    if ($before -cne $KnownPreimage) {
        # Known image check is informational - the patcher's semantic guards
        # (record 52226, focus=1553, aura=52255, name=飞行器着陆, dummy effect)
        # are authoritative.  Report the actual hash for our records.
        W "SERVER_SPELL_DBC_PREIMAGE_NOTE=not the R4-known hash; semantic guards will decide"
    }
    $checkReport = Join-Path $UploadDir "G17B2R4_SPELL_DBC_CHECK_BEFORE.txt"
    $rc = Invoke-NativeLogged -FilePath $python -NativeArgs @($Patcher, "check", "--input", $Target, "--report", $checkReport) -Prefix "CHECK_BEFORE"
    W "CHECK_BEFORE_EXIT=$rc"
    if ($rc -ne 0) { throw "Spell.dbc semantic check failed - refusing to patch a foreign DBC" }
    $checkText = [IO.File]::ReadAllText($checkReport)
    $stateLine = @($checkText -split "`r?`n" | Where-Object { $_ -match '^G17C1_SPELL_DBC_STATE=' })[0]
    $dbState = if ($stateLine -match '^G17C1_SPELL_DBC_STATE=(.+)$') { $Matches[1] } else { "" }
    W "G17B2R4_PRE_PATCH_STATE=$dbState"
    if ($dbState -ceq "LAYOUT_UNKNOWN") {
        throw "server Spell.dbc layout unknown (not a 234-field Spell.dbc); refusing to interpret column positions"
    }
    if ($dbState -ceq "ALREADY_CLEAN") {
        # The gates are already 0/0 on the server DBC - the target state.
        # No bytes are written; we still record state + evidence so the run
        # is idempotent and auditable.
        $stateFile = Join-Path $UploadDir "G17B2R4_SERVER_SPELL_DBC_STATE.txt"
        $alreadyLine = @(
            "STATE_FORMAT=1",
            "INSTALL_STATUS=PASS",
            "PRE_PATCH_STATE=ALREADY_CLEAN",
            "WROTE_FILE=False",
            ("RUN_DIR=" + $RunDir),
            ("SERVER_SPELL_DBC=" + $Target),
            ("SERVER_SPELL_DBC_SHA256=" + $before),
            ("SERVER_SPELL_DBC_SIZE=" + $size),
            ("INSTALLED_AT=" + (Get-Date).ToString("o"))
        )
        $stateTemp = $stateFile + ".tmp"
        # Idempotent: a previous PASS (or crashed) run may have left the
        # state file / temp.  Remove stale temp, then overwrite with -Force.
        if (Test-Path -LiteralPath $stateTemp -PathType Leaf) {
            Remove-Item -LiteralPath $stateTemp -Force -ErrorAction SilentlyContinue
        }
        [IO.File]::WriteAllText($stateTemp, (($alreadyLine -join [Environment]::NewLine) + [Environment]::NewLine), $Utf8NoBom)
        Move-Item -LiteralPath $stateTemp -Destination $stateFile -Force
        W "G17B2R4_SERVER_SPELL_DBC_WROTE_FILE=False"
        W "G17B2R4_SERVER_SPELL_DBC=ALREADY_CLEAN"
        W "G17B2R4_SERVER_SPELL_DBC_RESULT=PASS"
        W "NOTE=server Spell.dbc already has 52226 focus/aura=0; nothing to write. Restart worldserver and run the client unlock (G17-C1)."
        W "RESULT_FILE=$Result"
        exit 0
    }
    if ($dbState -cne "PATCHED") {
        throw "server Spell.dbc is not in the expected 1553/52255 state (state=$dbState); not patched"
    }

    $stamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $backupDir = Join-Path $UploadDir ("G17B2R4_Server_DBC_Backup_" + $stamp)
    if (Test-Path -LiteralPath $backupDir) { throw "backup dir already exists" }
    New-Item -ItemType Directory -Path $backupDir | Out-Null
    $backup = Join-Path $backupDir "Spell.dbc.before_g17b2r4"
    Copy-Item -LiteralPath $Target -Destination $backup
    if ((Get-FileHash -LiteralPath $backup -Algorithm SHA256).Hash.ToLowerInvariant() -cne $before) {
        throw "backup verification failed"
    }
    W "BACKUP=$backup"

    $tmpTarget = $Target + ".g17b2r4.new"
    $tmpReport = Join-Path $UploadDir "G17B2R4_SPELL_DBC_PATCH_REPORT.txt"
    if (Test-Path -LiteralPath $tmpTarget -PathType Leaf) {
        Remove-Item -LiteralPath $tmpTarget -Force -ErrorAction SilentlyContinue
    }
    $rc = Invoke-NativeLogged -FilePath $python -NativeArgs @($Patcher, "patch", "--input", $Target, "--output", $tmpTarget, "--report", $tmpReport) -Prefix "DBC_PATCH"
    W "DBC_PATCH_EXIT=$rc"
    if ($rc -ne 0) { throw "Spell.dbc patch failed" }
    $after = (Get-FileHash -LiteralPath $tmpTarget -Algorithm SHA256).Hash.ToLowerInvariant()
    W "SERVER_SPELL_DBC_SHA256_AFTER=$after"
    if ($after -ceq $before) { throw "patched file is byte-identical to preimage" }
    if ((Get-Item -LiteralPath $tmpTarget).Length -ne $size) { throw "patched file size changed unexpectedly" }

    $targetTmp = $Target + ".g17b2r4.old"
    if (Test-Path -LiteralPath $targetTmp -PathType Leaf) {
        # A previous run may have crashed between the two moves; the real
        # target is authoritative.  Remove only our .old artifact.
        Remove-Item -LiteralPath $targetTmp -Force -ErrorAction SilentlyContinue
    }
    Move-Item -LiteralPath $Target -Destination $targetTmp
    Move-Item -LiteralPath $tmpTarget -Destination $Target
    if ((Get-FileHash -LiteralPath $Target -Algorithm SHA256).Hash.ToLowerInvariant() -cne $after) {
        # roll back immediately
        Move-Item -LiteralPath $Target -Destination ($Target + ".g17b2r4.failed")
        Move-Item -LiteralPath $targetTmp -Destination $Target
        throw "installed patched DBC verification failed; auto-rolled back"
    }
    Remove-Item -LiteralPath $targetTmp -Force -ErrorAction SilentlyContinue

    $stateLine = @(
        "STATE_FORMAT=1",
        "INSTALL_STATUS=PASS",
        ("RUN_DIR=" + $RunDir),
        ("SERVER_SPELL_DBC=" + $Target),
        ("SERVER_SPELL_DBC_SHA256_BEFORE=" + $before),
        ("SERVER_SPELL_DBC_SHA256_AFTER=" + $after),
        ("BACKUP_SPELL_DBC=" + $backup),
        ("INSTALLED_AT=" + (Get-Date).ToString("o"))
    )
    $stateFile = Join-Path $UploadDir "G17B2R4_SERVER_SPELL_DBC_STATE.txt"
    [IO.File]::WriteAllText($stateFile, (($stateLine -join [Environment]::NewLine) + [Environment]::NewLine), $Utf8NoBom)

    W "NEXT=Restart D:\TC-Build\bin\RelWithDebInfo\worldserver.exe; startup log will show the sanitizer line as before; skill 4 packet now reaches spell_g17_dragon_safe_landing."
    W "G17B2R4_SERVER_SPELL_DBC=PASS"
    W "G17B2R4_SERVER_SPELL_DBC_RESULT=PASS"
    W "RESULT_FILE=$Result"
    exit 0
} catch {
    if ($tmpTarget -and (Test-Path -LiteralPath $tmpTarget -PathType Leaf)) {
        Remove-Item -LiteralPath $tmpTarget -Force -ErrorAction SilentlyContinue
    }
    try {
        if ($Target -and $targetTmp -and (Test-Path -LiteralPath $targetTmp -PathType Leaf) -and
            -not (Test-Path -LiteralPath $Target -PathType Leaf)) {
            Move-Item -LiteralPath $targetTmp -Destination $Target
            W "AUTO_RESTORE_SERVER_DBC=PASS"
        }
    } catch { W ("AUTO_RESTORE_ERROR=" + $_.Exception.Message) }
    W ("G17B2R4_SERVER_SPELL_DBC_ERROR=" + $_.Exception.Message)
    W "G17B2R4_SERVER_SPELL_DBC_RESULT=FAIL"
    W "STOP_DO_NOT_START_WORLDSERVER_UNTIL_RESOLVED"
    W "RESULT_FILE=$Result"
    exit 1
}
