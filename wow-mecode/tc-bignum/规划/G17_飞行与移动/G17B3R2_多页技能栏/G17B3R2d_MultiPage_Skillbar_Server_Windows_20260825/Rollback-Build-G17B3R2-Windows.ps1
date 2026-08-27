#requires -Version 5.1
# G17-B3R2 rollback: source -> B3R1 floor (2ddf54a6), server Spell.dbc
# restored from the newest B3R2 backup (if present), the 4 B3R2 script
# bindings removed, creature_template bar restored to the B3R1 layout,
# then MSBuild rebuild.
param(
    [string]$Workspace = "C:\Users\Administrator\Downloads\workspace",
    [string]$SourceRoot = "D:\TrinityCore",
    [string]$BuildRoot = "D:\TC-Build"
)
$ErrorActionPreference = "Stop"
$UploadDir = Join-Path $Workspace "uploads"
$Result = Join-Path $UploadDir "G17B3R2_WINDOWS_ROLLBACK_RESULT.txt"
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$Target = Join-Path $SourceRoot "src\server\scripts\Commands\cs_dragonriding.cpp"
$Tool = Join-Path $PSScriptRoot "tools\apply_g17b3r2_source.py"
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
function Find-MySqlClient {
    foreach ($name in @("mysql.exe", "mariadb.exe")) {
        $cmd = Get-Command $name -ErrorAction SilentlyContinue
        if ($cmd -and (Test-Path -LiteralPath $cmd.Source -PathType Leaf)) { return $cmd.Source }
    }
    foreach ($pattern in @(
        "C:\Program Files\MySQL\MySQL Server *\bin\mysql.exe",
        "D:\Program Files\MySQL\MySQL Server *\bin\mysql.exe",
        "C:\Program Files\MariaDB *\bin\mariadb.exe",
        "D:\Program Files\MariaDB *\bin\mariadb.exe",
        "C:\xampp\mysql\bin\mysql.exe", "D:\xampp\mysql\bin\mysql.exe")) {
        $hit = @(Get-Item -Path $pattern -ErrorAction SilentlyContinue | Where-Object { -not $_.PSIsContainer })[0]
        if ($hit) { return $hit.FullName }
    }
    return $null
}
try {
    W "G17B3R2_WINDOWS_ROLLBACK_START"
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

    # restore server DBC from the newest G17B3R2 backup if it exists
    $backups = @(Get-ChildItem -LiteralPath $UploadDir -Directory -Filter "G17B3R2_Server_DBC_Backup_*" -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending)
    if ($backups.Count -gt 0) {
        $spell = Join-Path $backups[0].FullName "Spell.dbc.before_g17b3r2"
        if (Test-Path -LiteralPath $spell -PathType Leaf) {
            Copy-Item -LiteralPath $spell -Destination $ServerDbc -Force
            W "SERVER_DBC_RESTORED=$spell"
        } else { W "SERVER_DBC_BACKUP_FILE_MISSING (kept current DBC; appended records are inert once scripts are unbound)" }
    } else { W "SERVER_DBC_BACKUP_NONE (kept current DBC; appended records are inert once scripts are unbound)" }

    # remove the 4 B3R2 script bindings and restore the template bar layout
    if (Test-Path -LiteralPath $WorldConf -PathType Leaf) {
        $line = @(Get-Content -LiteralPath $WorldConf | Where-Object { $_ -match '^\s*WorldDatabaseInfo\s*=\s*"([^"]+)"' })[0]
        if ($line) {
            $parts = $Matches[1].Split(';')
            if ($parts.Count -ge 5 -and $parts[4] -ceq "world") {
                $mysql = Find-MySqlClient
                if ($mysql) {
                    $sqlText = @"
USE `world`;
SET NAMES utf8mb4 COLLATE utf8mb4_unicode_ci;
DELETE FROM `spell_script_names` WHERE `spell_id` BETWEEN 990025 AND 990028;
DELETE FROM `creature_template_spell` WHERE `CreatureID`=1000171 AND `Index` BETWEEN 0 AND 7;
INSERT INTO `creature_template_spell` (`CreatureID`, `Index`, `Spell`) VALUES
  (1000171, 0, 9573), (1000171, 1, 55215), (1000171, 2, 52197), (1000171, 3, 52226);
SELECT 'G17B3R2_ROLLBACK_SQL=PASS';
"@
                    $tmpSql = Join-Path $env:TEMP ("g17b3r2_rollback_" + [Guid]::NewGuid().ToString("N") + ".sql")
                    [IO.File]::WriteAllText($tmpSql, $sqlText, $Utf8NoBom)
                    $oldPwd = $env:MYSQL_PWD; $oldPref = $ErrorActionPreference
                    $out = @(); $rc = 9009
                    try {
                        $env:MYSQL_PWD = $parts[3]; $ErrorActionPreference = "Continue"
                        $out = @(Get-Content -LiteralPath $tmpSql | & $mysql @("--protocol=TCP", ("--host=" + $parts[0]), ("--port=" + $parts[1]), ("--user=" + $parts[2]), "--default-character-set=utf8mb4", "--database=world", "--batch", "--raw", "--skip-column-names") 2>&1)
                        $rc = $LASTEXITCODE
                    } finally { $ErrorActionPreference = $oldPref; $env:MYSQL_PWD = $oldPwd }
                    foreach ($entry in $out) { W ("MYSQL|" + $entry.ToString()) }
                    Remove-Item -LiteralPath $tmpSql -Force -ErrorAction SilentlyContinue
                    W "MYSQL_EXIT=$rc"
                    if ($rc -ne 0) { throw "rollback SQL failed" }
                } else { W "MYSQL_CLIENT_NOT_FOUND (bindings left in place; they are inert with the B3R1 source)" }
            } else { W "WORLDCONF_UNEXPECTED (bindings left in place)" }
        } else { W "WORLDCONF_NO_DSN (bindings left in place)" }
    } else { W "WORLDCONF_MISSING (bindings left in place)" }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) { throw "vswhere missing" }
    $msbuild = @(& $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Where-Object { $_ })[0]
    if (-not $msbuild) { throw "MSBuild not found" }
    [IO.File]::SetLastWriteTimeUtc($Target, [DateTime]::UtcNow)
    $rc = Invoke-NativeLogged -FilePath $msbuild -NativeArgs @((Join-Path $BuildRoot "TrinityCore.sln"), "/m", "/t:worldserver", "/p:Configuration=RelWithDebInfo", "/p:Platform=x64", "/verbosity:minimal") -Prefix "MSBUILD"
    W "MSBUILD_EXIT=$rc"
    if ($rc -ne 0) { throw "MSBuild failed" }
    W "G17B3R2_WINDOWS_ROLLBACK_RESULT=PASS"
    W "RESULT_FILE=$Result"
    exit 0
} catch {
    W ("G17B3R2_WINDOWS_ROLLBACK_ERROR=" + $_.Exception.Message)
    W "G17B3R2_WINDOWS_ROLLBACK_RESULT=FAIL"
    W "RESULT_FILE=$Result"
    exit 1
}
