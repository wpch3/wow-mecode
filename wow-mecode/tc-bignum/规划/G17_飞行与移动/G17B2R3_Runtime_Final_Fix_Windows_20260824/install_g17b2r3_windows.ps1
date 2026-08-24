# G17-B2R3 Windows source apply + self-test.
# Usage (from an unzipped package directory):
#   powershell -ExecutionPolicy Bypass -File .\install_g17b2r3_windows.ps1 -SourceRoot D:\TrinityCore
# This only replaces the single G17 source file.  Run the world SQL separately
# (see sql\G17B2R3_world_landing_binding_guard.sql), then rebuild scripts.
#
# B2R2 note: the `py -3` launcher on this machine points at a missing
# Python314 install (exit 101 "Unable to create process").  This script now
# auto-detects a real python.exe and never uses the WindowsApps placeholder.

param(
    [Parameter(Mandatory=$true)][string]$SourceRoot,
    [switch]$Rollback
)

$ErrorActionPreference = "Stop"
$PackageRoot = Split-Path -Parent $MyInvocation.MyCommand.Path

function Resolve-Python {
    # 1) Explicit override if the user set $env:G17_PYTHON.
    if ($env:G17_PYTHON -and (Test-Path $env:G17_PYTHON)) { return $env:G17_PYTHON }

    $candidates = @(
        "C:\Python312\python.exe",
        "C:\Python310\python.exe",
        "$env:LOCALAPPDATA\Programs\Python\Python312\python.exe",
        "$env:LOCALAPPDATA\Programs\Python\Python310\python.exe",
        "$env:LOCALAPPDATA\Programs\Python\Python313\python.exe",
        "C:\Program Files\Python312\python.exe",
        "C:\Program Files\Python310\python.exe"
    )
    foreach ($c in $candidates) {
        if (Test-Path $c) { return $c }
    }

    # 2) Try a bare `python` (reject the WindowsApps stub).
    $cmd = Get-Command python.exe -ErrorAction SilentlyContinue
    if ($cmd -and $cmd.Source -notlike "*WindowsApps*") { return $cmd.Source }

    throw "No usable Python found. Set `$env:G17_PYTHON to a real python.exe (3.10/3.12)."
}

$Python = Resolve-Python
Write-Host "Using Python: $Python"

function Invoke-Step($label, $argumentList) {
    Write-Host "==> $label"
    & $Python (Join-Path $PackageRoot "tools\apply_g17b2r3_source.py") @argumentList
    if ($LASTEXITCODE -ne 0) { throw "$label failed (exit $LASTEXITCODE)" }
}

if ($Rollback) {
    Invoke-Step "rollback" @("rollback", "--source-root", $SourceRoot)
} else {
    Invoke-Step "check" @("check", "--source-root", $SourceRoot)
    Invoke-Step "apply" @("apply", "--source-root", $SourceRoot)
    Invoke-Step "post-check" @("check", "--source-root", $SourceRoot)
}

Write-Host ""
Write-Host "G17B2R3_WINDOWS_SOURCE_STEP=PASS"
Write-Host "Next: run sql\G17B2R3_world_landing_binding_guard.sql against world, then rebuild."
