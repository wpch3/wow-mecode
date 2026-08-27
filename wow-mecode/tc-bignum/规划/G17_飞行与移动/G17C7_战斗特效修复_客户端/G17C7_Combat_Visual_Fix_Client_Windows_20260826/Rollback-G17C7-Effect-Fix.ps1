#requires -Version 5.1
# G17-C7 rollback: restore server DBC and client MPQ backups.
param(
    [string]$Workspace = "C:\Users\Administrator\Downloads\workspace",
    [string]$BuildRoot = "D:\TC-Build",
    [string]$ClientRoot = "D:\WOW"
)
$ErrorActionPreference = "Stop"
$UploadDir = Join-Path $Workspace "uploads"
$Result = Join-Path $UploadDir "G17C7_EFFECT_FIX_ROLLBACK_RESULT.txt"
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$ServerDbc = Join-Path $BuildRoot "bin\RelWithDebInfo\dbc\Spell.dbc"
$StateFile = Join-Path $UploadDir "G17C7_EFFECT_FIX_STATE.txt"

New-Item -ItemType Directory -Path $UploadDir -Force | Out-Null
[IO.File]::WriteAllText($Result, "", $Utf8NoBom)
function W([string]$Text) { Write-Host $Text; [IO.File]::AppendAllText($Result, $Text + [Environment]::NewLine, $Utf8NoBom) }
try {
    W "G17C7_ROLLBACK_START"
    if (Get-Process worldserver -ErrorAction SilentlyContinue) { throw "worldserver is running; stop it first" }
    if (Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Name -match '^(?i:wow|wow-64)$' }) { throw "Wow client is running; close it first" }

    # Restore server DBC from newest backup
    $srvBackups = @(Get-ChildItem -LiteralPath $UploadDir -Directory -Filter "G17C7_Server_DBC_Backup_*" -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending)
    if ($srvBackups.Count -gt 0) {
        $spell = Join-Path $srvBackups[0].FullName "Spell.dbc.before_g17c7"
        if (Test-Path -LiteralPath $spell -PathType Leaf) {
            Copy-Item -LiteralPath $spell -Destination $ServerDbc -Force
            W "SERVER_DBC_RESTORED=$spell"
        }
    } else { W "SERVER_DBC_BACKUP_NONE" }

    # Restore client MPQs from newest backup
    if (Test-Path -LiteralPath $StateFile -PathType Leaf) {
        $state = @{}
        foreach ($line in [IO.File]::ReadAllLines($StateFile)) {
            if ($line -match '^([^=]+)=(.*)$') { $state[$Matches[1]] = $Matches[2] }
        }
        $backupDir = $state["BACKUP_DIR"]
        if ($backupDir -and (Test-Path -LiteralPath $backupDir -PathType Container)) {
            $rootMpq = $state["ROOT_MPQ"]
            $localeMpq = $state["LOCALE_MPQ"]
            $backupRoot = Join-Path $backupDir "before_G17C7_root.MPQ"
            $backupLocale = Join-Path $backupDir "before_G17C7_locale.MPQ"
            if ((Test-Path -LiteralPath $backupRoot) -and $rootMpq) {
                Copy-Item -LiteralPath $backupRoot -Destination $rootMpq -Force
                W "ROOT_MPQ_RESTORED"
            }
            if ((Test-Path -LiteralPath $backupLocale) -and $localeMpq) {
                Copy-Item -LiteralPath $backupLocale -Destination $localeMpq -Force
                W "LOCALE_MPQ_RESTORED"
            }
            $CacheDir = Join-Path $ClientRoot "Cache"
            if (Test-Path -LiteralPath $CacheDir -PathType Container) { Remove-Item -LiteralPath $CacheDir -Recurse -Force }
            Remove-Item -LiteralPath $StateFile -Force
        }
    } else { W "CLIENT_STATE_NONE" }

    W "G17C7_ROLLBACK_RESULT=PASS"
    W "RESULT_FILE=$Result"
    exit 0
} catch {
    W ("G17C7_ROLLBACK_ERROR=" + $_.Exception.Message)
    W "G17C7_ROLLBACK_RESULT=FAIL"
    W "RESULT_FILE=$Result"
    exit 1
}
