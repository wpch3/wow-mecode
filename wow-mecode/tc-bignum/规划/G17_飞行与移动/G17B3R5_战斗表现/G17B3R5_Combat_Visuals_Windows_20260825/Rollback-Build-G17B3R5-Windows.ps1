#requires -Version 5.1
# G17-B3R4 rollback: source -> B3R3 floor (29f3e554), restore the server
# Spell.dbc range backup if present (the 30yd patch is harmless to keep but
# we restore for exactness), remove the G17DragonBar addon (restoring a
# backed-up copy if one exists), rebuild.
param(
    [string]$Workspace = "C:\Users\Administrator\Downloads\workspace",
    [string]$SourceRoot = "D:\TrinityCore",
    [string]$BuildRoot = "D:\TC-Build",
    [string]$ClientRoot = "D:\WOW"
)
$ErrorActionPreference = "Stop"
$UploadDir = Join-Path $Workspace "uploads"
$Result = Join-Path $UploadDir "G17B3R5_WINDOWS_ROLLBACK_RESULT.txt"
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$Target = Join-Path $SourceRoot "src\server\scripts\Commands\cs_dragonriding.cpp"
$Tool = Join-Path $PSScriptRoot "tools\apply_g17b3r5_source.py"
$RunDir = Join-Path $BuildRoot "bin\RelWithDebInfo"
$AddonDst = Join-Path $ClientRoot "Interface\AddOns\G17DragonBar"

New-Item -ItemType Directory -Path $UploadDir -Force | Out-Null
[IO.File]::WriteAllText($Result, "", $Utf8NoBom)
function W([string]$Text) { Write-Host $Text; [IO.File]::AppendAllText($Result, $Text + [Environment]::NewLine, $Utf8NoBom) }
function Invoke-NativeLogged {
    param([string]$FilePath, [string[]]$NativeArgs, [string]$Prefix)
    $old = $ErrorActionPreference; $out = @(); $rc = 9009
    try { $ErrorActionPreference = "Continue"; $out = @(& $FilePath @NativeArgs 2>&1); $rc = $LASTEXITCODE }
    finally { $ErrorActionPreference = $old }
    foreach ($line in $out) { W ($Prefix + "|" + $line.ToString()) }
    return [int]$rc
}
function Find-Python {
    $candidates = @(
        (Join-Path $env:LOCALAPPDATA "Programs\Python\Python312\python.exe"),
        (Join-Path $env:LOCALAPPDATA "Programs\Python\Python310\python.exe"),
        "C:\Python312\python.exe", "C:\Python310\python.exe")
    $python = @($candidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf })[0]
    if (-not $python) { $cmd = Get-Command python.exe -ErrorAction SilentlyContinue; if ($cmd -and $cmd.Source -notmatch "\\WindowsApps\\") { $python = $cmd.Source } }
    return $python
}
try {
    W "G17B3R5_WINDOWS_ROLLBACK_START"
    if (Get-Process worldserver -ErrorAction SilentlyContinue) { throw "worldserver is running; stop it first" }
    if (Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Name -match '^(?i:wow|wow-64)$' }) {
        throw "Wow client is running; close it first"
    }
    $python = Find-Python
    if (-not $python) { throw "Python312/Python310 not found" }
    $before = (Get-FileHash -LiteralPath $Target -Algorithm SHA256).Hash.ToLowerInvariant()
    W "SOURCE_SHA256_BEFORE=$before"
    $rc = Invoke-NativeLogged -FilePath $python -NativeArgs @($Tool, "rollback", "--source-root", $SourceRoot) -Prefix "SOURCE_ROLLBACK"
    W "SOURCE_ROLLBACK_EXIT=$rc"
    if ($rc -ne 0) { throw "source rollback failed" }
    $after = (Get-FileHash -LiteralPath $Target -Algorithm SHA256).Hash.ToLowerInvariant()
    W "SOURCE_SHA256_AFTER=$after"

    # restore the server DBC range backup if present
    $ServerDbc = Join-Path $RunDir "dbc\Spell.dbc"
    $rangeBackups = @(Get-ChildItem -LiteralPath $UploadDir -Directory -Filter "G17B3R5_Server_DBC_Backup_*" -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending)
    if ($rangeBackups.Count -gt 0) {
        $spell = Join-Path $rangeBackups[0].FullName "Spell.dbc.before_g17b3r5"
        if (Test-Path -LiteralPath $spell -PathType Leaf) {
            Copy-Item -LiteralPath $spell -Destination $ServerDbc -Force
            W "SERVER_DBC_VISUAL_RESTORED=$spell"
        } else { W "SERVER_DBC_VISUAL_BACKUP_FILE_MISSING (kept current DBC)" }
    } else { W "SERVER_DBC_VISUAL_BACKUP_NONE (kept current DBC)" }

    # remove the addon (or restore the newest backed-up copy)
    if (Test-Path -LiteralPath $AddonDst -PathType Container) {
        Remove-Item -LiteralPath $AddonDst -Recurse -Force
        W "ADDON_REMOVED=$AddonDst"
    }
    $backups = @(Get-ChildItem -LiteralPath $UploadDir -Directory -Filter "G17B3R5_Addon_Backup_*" -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending)
    if ($backups.Count -gt 0) {
        $saved = Join-Path $backups[0].FullName "G17DragonBar"
        if (Test-Path -LiteralPath $saved -PathType Container) {
            Copy-Item -LiteralPath $saved -Destination $AddonDst -Recurse
            W "ADDON_RESTORED=$saved"
        }
    } else { W "ADDON_BACKUP_NONE" }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) { throw "vswhere missing" }
    $msbuild = @(& $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Where-Object { $_ })[0]
    if (-not $msbuild) { throw "MSBuild not found" }
    [IO.File]::SetLastWriteTimeUtc($Target, [DateTime]::UtcNow)
    $rc = Invoke-NativeLogged -FilePath $msbuild -NativeArgs @((Join-Path $BuildRoot "TrinityCore.sln"), "/m", "/t:worldserver", "/p:Configuration=RelWithDebInfo", "/p:Platform=x64", "/verbosity:minimal") -Prefix "MSBUILD"
    W "MSBUILD_EXIT=$rc"
    if ($rc -ne 0) { throw "MSBuild failed" }
    W "G17B3R5_WINDOWS_ROLLBACK_RESULT=PASS"
    W "RESULT_FILE=$Result"
    exit 0
} catch {
    W ("G17B3R5_WINDOWS_ROLLBACK_ERROR=" + $_.Exception.Message)
    W "G17B3R5_WINDOWS_ROLLBACK_RESULT=FAIL"
    W "RESULT_FILE=$Result"
    exit 1
}
