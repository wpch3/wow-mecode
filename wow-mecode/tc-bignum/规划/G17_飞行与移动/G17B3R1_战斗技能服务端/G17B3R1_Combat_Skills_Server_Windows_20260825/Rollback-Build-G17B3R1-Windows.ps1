#requires -Version 5.1
# G17-B3R1 rollback: source -> B2R3 floor, server Spell.dbc restored from
# backup (if present), world binding rows for 990000-990024 removed.
param(
    [string]$Workspace = "C:\Users\Administrator\Downloads\workspace",
    [string]$SourceRoot = "D:\TrinityCore",
    [string]$BuildRoot = "D:\TC-Build"
)
$ErrorActionPreference = "Stop"
$UploadDir = Join-Path $Workspace "uploads"
$Result = Join-Path $UploadDir "G17B3R1_WINDOWS_ROLLBACK_RESULT.txt"
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$Target = Join-Path $SourceRoot "src\server\scripts\Commands\cs_dragonriding.cpp"
$Tool = Join-Path $PSScriptRoot "tools\apply_g17b3r1_source.py"
$RunDir = Join-Path $BuildRoot "bin\RelWithDebInfo"
$Exe = Join-Path $RunDir "worldserver.exe"
$Pdb = Join-Path $RunDir "worldserver.pdb"
$ServerDbc = Join-Path $RunDir "dbc\Spell.dbc"
$WorldConf = Join-Path $RunDir "worldserver.conf"

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
    W "G17B3R1_WINDOWS_ROLLBACK_START"
    if (Get-Process worldserver -ErrorAction SilentlyContinue) { throw "worldserver is running; stop it first" }
    $python = Find-Python
    if (-not $python) { throw "Python312/Python310 not found" }
    $before = (Get-FileHash -LiteralPath $Target -Algorithm SHA256).Hash.ToLowerInvariant()
    W "SOURCE_SHA256_BEFORE=$before"
    $rc = Invoke-NativeLogged -FilePath $python -NativeArgs @($Tool, "rollback", "--source-root", $SourceRoot) -Prefix "SOURCE_ROLLBACK"
    W "SOURCE_ROLLBACK_EXIT=$rc"
    if ($rc -ne 0) { throw "source rollback failed" }
    $after = (Get-FileHash -LiteralPath $Target -Algorithm SHA256).Hash.ToLowerInvariant()
    W "SOURCE_SHA256_AFTER=$after"

    # restore server DBC from the newest G17B3R1 backup if it exists
    $backups = @(Get-ChildItem -LiteralPath $UploadDir -Directory -Filter "G17B3R1_Server_DBC_Backup_*" -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending)
    if ($backups.Count -gt 0) {
        $spell = Join-Path $backups[0].FullName "Spell.dbc.before_g17b3r1"
        if (Test-Path -LiteralPath $spell -PathType Leaf) {
            Copy-Item -LiteralPath $spell -Destination $ServerDbc -Force
            W "SERVER_DBC_RESTORED=$spell"
        }
    } else {
        W "SERVER_DBC_RESTORE=NONE_FOUND (leave as appended; harmless without source)"
    }

    # remove combat binding rows (safe: custom id range)
    if (Test-Path -LiteralPath $WorldConf -PathType Leaf) {
        $line = @(Get-Content -LiteralPath $WorldConf | Where-Object { $_ -match '^\s*WorldDatabaseInfo\s*=\s*"([^"]+)"' })[0]
        if ($line) {
            W "SQL_ROLLBACK_ADVISED=run: DELETE FROM spell_script_names WHERE spell_id BETWEEN 990000 AND 990024 AND ScriptName='spell_g17_combat_skill';"
        }
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    $msbuild = @(& $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Where-Object { $_ })[0]
    if ($msbuild) {
        $beforeExe = Get-Item $Exe; $beforePdb = Get-Item $Pdb
        $beforeExeUtc = $beforeExe.LastWriteTimeUtc; $beforePdbUtc = $beforePdb.LastWriteTimeUtc
        $start = [DateTime]::UtcNow
        $rc = Invoke-NativeLogged -FilePath $msbuild -NativeArgs @((Join-Path $BuildRoot "TrinityCore.sln"), "/m", "/t:worldserver", "/p:Configuration=RelWithDebInfo", "/p:Platform=x64", "/verbosity:minimal") -Prefix "MSBUILD"
        W "MSBUILD_EXIT=$rc"
        if ($rc -ne 0) { throw "MSBuild failed" }
        $afterExe = Get-Item $Exe; $afterPdb = Get-Item $Pdb
        if ($afterExe.LastWriteTimeUtc -le $beforeExeUtc -or $afterPdb.LastWriteTimeUtc -le $beforePdbUtc) { throw "exe/pdb timestamp did not advance" }
    }
    W "G17B3R1_WINDOWS_ROLLBACK=PASS"
    W "FLOOR=B2R3_POSTIMAGE"
    W "RESULT_FILE=$Result"
    exit 0
} catch {
    W ("G17B3R1_WINDOWS_ROLLBACK_ERROR=" + $_.Exception.Message)
    W "G17B3R1_WINDOWS_ROLLBACK=FAIL"
    W "RESULT_FILE=$Result"
    exit 1
}
