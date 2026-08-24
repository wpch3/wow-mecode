#requires -Version 5.1
# G17-B2R4 rollback: restore the server Spell.dbc backup taken at install.
param(
    [string]$Workspace = "C:\Users\Administrator\Downloads\workspace",
    [string]$RunDir = "D:\TC-Build\bin\RelWithDebInfo"
)
$ErrorActionPreference = "Stop"
$UploadDir = Join-Path $Workspace "uploads"
$Result = Join-Path $UploadDir "G17B2R4_SERVER_SPELL_DBC_ROLLBACK_RESULT.txt"
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$Target = Join-Path $RunDir "dbc\Spell.dbc"
$StateFile = Join-Path $UploadDir "G17B2R4_SERVER_SPELL_DBC_STATE.txt"

New-Item -ItemType Directory -Path $UploadDir -Force | Out-Null
[IO.File]::WriteAllText($Result, "", $Utf8NoBom)
function W([string]$Text) {
    Write-Host $Text
    [IO.File]::AppendAllText($Result, $Text + [Environment]::NewLine, $Utf8NoBom)
}
try {
    W "G17B2R4_SERVER_DBC_ROLLBACK_START"
    if (Get-Process worldserver -ErrorAction SilentlyContinue) {
        throw "worldserver is running; stop it first"
    }
    if (-not (Test-Path -LiteralPath $StateFile -PathType Leaf)) {
        throw "G17B2R4 state file missing; nothing to roll back"
    }
    $state = @{}
    foreach ($line in [IO.File]::ReadAllLines($StateFile)) {
        if ($line -match '^([^=]+)=(.*)$') { $state[$Matches[1]] = $Matches[2] }
    }
    if ($state["INSTALL_STATUS"] -cne "PASS") { throw "state is not PASS" }
    $backup = $state["BACKUP_SPELL_DBC"]
    $before = $state["SERVER_SPELL_DBC_SHA256_BEFORE"]
    if (-not $backup -or -not (Test-Path -LiteralPath $backup -PathType Leaf)) {
        throw "backup missing: $backup"
    }
    $current = (Get-FileHash -LiteralPath $Target -Algorithm SHA256).Hash.ToLowerInvariant()
    W "CURRENT_SHA256=$current"
    W "EXPECTED_AFTER=$($state["SERVER_SPELL_DBC_SHA256_AFTER"])"
    Copy-Item -LiteralPath $backup -Destination $Target
    $restored = (Get-FileHash -LiteralPath $Target -Algorithm SHA256).Hash.ToLowerInvariant()
    W "RESTORED_SHA256=$restored"
    if ($restored -cne $before) {
        throw "restored DBC does not match recorded preimage hash"
    }
    Remove-Item -LiteralPath $StateFile -Force
    W "G17B2R4_SERVER_SPELL_DBC_ROLLBACK=PASS"
    W "G17B2R4_SERVER_SPELL_DBC_ROLLBACK_RESULT=PASS"
    W "RESULT_FILE=$Result"
    exit 0
} catch {
    W ("G17B2R4_SERVER_SPELL_DBC_ROLLBACK_ERROR=" + $_.Exception.Message)
    W "G17B2R4_SERVER_SPELL_DBC_ROLLBACK_RESULT=FAIL"
    W "RESULT_FILE=$Result"
    exit 1
}
