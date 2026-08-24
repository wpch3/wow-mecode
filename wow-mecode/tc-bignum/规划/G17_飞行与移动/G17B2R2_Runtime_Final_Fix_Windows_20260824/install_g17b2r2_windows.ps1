# G17-B2R2 Windows source apply + self-test.
# Usage (from an unzipped package directory):
#   powershell -ExecutionPolicy Bypass -File .\install_g17b2r2_windows.ps1 -SourceRoot D:\TrinityCore
# This only replaces the single G17 source file.  Run the world SQL separately
# (see sql\G17B2R2_world_landing_binding_guard.sql), then rebuild scripts.

param(
    [Parameter(Mandatory=$true)][string]$SourceRoot,
    [switch]$Rollback
)

$ErrorActionPreference = "Stop"
$PackageRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$Python = "py"

function Invoke-Step($label, $argumentList) {
    Write-Host "==> $label"
    & $Python -3 (Join-Path $PackageRoot "tools\apply_g17b2r2_source.py") @argumentList
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
Write-Host "G17B2R2_WINDOWS_SOURCE_STEP=PASS"
Write-Host "Next: run sql\G17B2R2_world_landing_binding_guard.sql against world, then rebuild."
