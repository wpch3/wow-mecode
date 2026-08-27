#requires -Version 5.1
# G17-C6 rollback: restore root patch MPQ and zhCN mirror backups.
param(
    [string]$Workspace = "C:\Users\Administrator\Downloads\workspace",
    [string]$ClientRoot = "D:\WOW"
)
$ErrorActionPreference = "Stop"
$UploadDir = Join-Path $Workspace "uploads"
$Result = Join-Path $UploadDir "G17C6_CLIENT_VISUALS_ROLLBACK_RESULT.txt"
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$StateFile = Join-Path $UploadDir "G17C6_CLIENT_VISUALS_STATE.txt"

New-Item -ItemType Directory -Path $UploadDir -Force | Out-Null
[IO.File]::WriteAllText($Result, "", $Utf8NoBom)
function W([string]$Text) { Write-Host $Text; [IO.File]::AppendAllText($Result, $Text + [Environment]::NewLine, $Utf8NoBom) }
try {
    W "G17C6_CLIENT_VISUALS_ROLLBACK_START"
    if (Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Name -match '^(?i:wow|wow-64)$' }) {
        throw "Wow client is running; close it first"
    }
    if (-not (Test-Path -LiteralPath $StateFile -PathType Leaf)) { throw "G17C6 state file missing" }
    $state = @{}
    foreach ($line in [IO.File]::ReadAllLines($StateFile)) {
        if ($line -match '^([^=]+)=(.*)$') { $state[$Matches[1]] = $Matches[2] }
    }
    if ($state["INSTALL_STATUS"] -cne "PASS") { throw "state is not PASS" }
    $rootMpq = $state["ROOT_MPQ"]; $localeMpq = $state["LOCALE_MPQ"]
    $backupDir = $state["BACKUP_DIR"]
    $newHash = $state["NEW_MPQ_SHA256"]
    if (-not $rootMpq -or -not $localeMpq -or -not $backupDir -or -not $newHash) { throw "state incomplete" }
    $backupRoot = Join-Path $backupDir ("before_G17C6_" + (Split-Path -Leaf $rootMpq))
    $backupLocale = Join-Path $backupDir ("before_G17C6_" + (Split-Path -Leaf $localeMpq))
    if (-not (Test-Path -LiteralPath $backupRoot)) { throw "backup missing" }
    if (-not (Test-Path -LiteralPath $backupLocale)) { throw "backup missing" }
    Copy-Item -LiteralPath $backupRoot -Destination $rootMpq -Force
    Copy-Item -LiteralPath $backupLocale -Destination $localeMpq -Force
    $CacheDir = Join-Path $ClientRoot "Cache"
    if (Test-Path -LiteralPath $CacheDir -PathType Container) { Remove-Item -LiteralPath $CacheDir -Recurse -Force; W "CLIENT_CACHE_REMOVED=True" }
    Remove-Item -LiteralPath $StateFile -Force
    W "G17C6_CLIENT_VISUALS_ROLLBACK=PASS"
    W "G17C6_CLIENT_VISUALS_ROLLBACK_RESULT=PASS"
    W "RESULT_FILE=$Result"
    exit 0
} catch {
    W ("G17C6_CLIENT_VISUALS_ROLLBACK_ERROR=" + $_.Exception.Message)
    W "G17C6_CLIENT_VISUALS_ROLLBACK_RESULT=FAIL"
    W "RESULT_FILE=$Result"
    exit 1
}
