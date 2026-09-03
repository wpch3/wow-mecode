#requires -Version 5.1
# G17-B3R11: land-mount flight animation fix (user report: 陆地坐骑飞行时
# 双脚蹬个不停).  BEAST/GENERIC mounts lack Fly-tier animations so the client
# plays their run cycle in flight; a server EMOTE STATE (stand pose) overrides
# the movement animation of non-self units - legs freeze while airborne and
# restore on the ground.  Flying-model archetypes untouched.  Includes every
# B3R7-R10 improvement (one install brings any lineage state current).
# B3-R12: (1) no-target hidden-cooldown fix - combat-skill CheckCast now
# pre-validates the target (full resolution chain, triggered exempt), so a
# targetless press fails CLEANLY instead of executing + silently eating the
# core GCD ("not ready" with no UI swirl).  (2) Skill variety #1/#2: 990029
# swoop strike (combat slot 7, first AoE) + 990030 wind stance (movement slot
# 7, toggle) - slot 7 is now page-pure.  Steps: source apply -> server DBC
# append (990029/990030) -> world SQL binding -> MSBuild.
# Works from B3R6-R11r1a states. Payload 3d501d9b, rollback 3fdb46e8.
param(
    [string]$Workspace = "C:\Users\Administrator\Downloads\workspace",
    [string]$SourceRoot = "D:\TrinityCore",
    [string]$BuildRoot = "D:\TC-Build"
)
$ErrorActionPreference = "Stop"
$UploadDir = Join-Path $Workspace "uploads"
$Result = Join-Path $UploadDir "G17B3R12_WINDOWS_BUILD_RESULT.txt"
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$Target = Join-Path $SourceRoot "src\server\scripts\Commands\cs_dragonriding.cpp"
$Tool = Join-Path $PSScriptRoot "tools\apply_g17b3r12_source.py"
$Appender = Join-Path $PSScriptRoot "tools\append_g17b3r12_spells.py"
$Sql = Join-Path $PSScriptRoot "sql\G17B3R12_world_variety_binding.sql"
$Solution = Join-Path $BuildRoot "TrinityCore.sln"
$RunDir = Join-Path $BuildRoot "bin\RelWithDebInfo"
$ServerDbc = Join-Path $RunDir "dbc\Spell.dbc"
$WorldConf = Join-Path $RunDir "worldserver.conf"
$Exe = Join-Path $RunDir "worldserver.exe"
$Pdb = Join-Path $RunDir "worldserver.pdb"

function Read-ToolHash([string]$Name) {
    $pattern = ('^\s*' + [regex]::Escape($Name) + '\s*=\s*"([0-9a-f]+)"')
    $line = @(Get-Content -LiteralPath $Tool | Where-Object { $_ -match $pattern })[0]
    if (-not $line) { throw "could not read $Name from $Tool" }
    return $Matches[1]
}
$Pre = Read-ToolHash "PRE_SHA256"
$Post = Read-ToolHash "POST_SHA256"
$SafeRollback = Read-ToolHash "SAFE_ROLLBACK_SHA256"


function Find-MySqlClient {
    foreach ($name in @("mysql.exe", "mariadb.exe")) {
        $cmd = Get-Command $name -ErrorAction SilentlyContinue
        if ($cmd -and (Test-Path -LiteralPath $cmd.Source -PathType Leaf)) { return $cmd.Source }
    }
    $candidates = @(
        "C:\xampp\mysql\bin\mysql.exe", "D:\xampp\mysql\bin\mysql.exe",
        "C:\mysql\bin\mysql.exe", "D:\mysql\bin\mysql.exe",
        "C:\Program Files\MySQL\MySQL Server 8.0\bin\mysql.exe",
        "C:\Program Files\MariaDB 11.4\bin\mariadb.exe",
        "C:\Program Files\MariaDB 10.11\bin\mariadb.exe")
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) { return $candidate }
    }
    foreach ($pattern in @(
        "C:\Program Files\MySQL\MySQL Server *\bin\mysql.exe",
        "D:\Program Files\MySQL\MySQL Server *\bin\mysql.exe",
        "C:\Program Files\MariaDB *\bin\mariadb.exe",
        "D:\Program Files\MariaDB *\bin\mariadb.exe")) {
        $hit = @(Get-Item -Path $pattern -ErrorAction SilentlyContinue | Where-Object { -not $_.PSIsContainer })[0]
        if ($hit) { return $hit.FullName }
    }
    return $null
}
function Invoke-WorldSql {
    param([string]$SqlFile, [string]$PassMarker)
    if (-not (Test-Path -LiteralPath $WorldConf -PathType Leaf)) { throw "worldserver.conf missing" }
    $line = @(Get-Content -LiteralPath $WorldConf | Where-Object { $_ -match '^\s*WorldDatabaseInfo\s*=\s*"([^"]+)"' })[0]
    if (-not $line) { throw "WorldDatabaseInfo not found" }
    $parts = $Matches[1].Split(';')
    if ($parts.Count -lt 5) { throw "WorldDatabaseInfo has fewer than five fields" }
    $hostName = $parts[0]; $port = $parts[1]; $user = $parts[2]; $password = $parts[3]; $database = $parts[4]
    if ($database -cne "world") { throw "WorldDatabaseInfo database is not explicit world" }
    if ($port -notmatch '^\d+$') { throw "invalid port" }
    $mysql = Find-MySqlClient
    if (-not $mysql) { throw "mysql.exe/mariadb.exe not found" }
    $args = @("--protocol=TCP", ("--host=" + $hostName), ("--port=" + $port),
              ("--user=" + $user), "--default-character-set=utf8mb4", "--database=world",
              "--batch", "--raw", "--skip-column-names")
    $oldPwd = $env:MYSQL_PWD
    $oldPref = $ErrorActionPreference
    $out = @(); $rc = 9009
    try {
        $env:MYSQL_PWD = $password
        $ErrorActionPreference = "Continue"
        $out = @(Get-Content -LiteralPath $SqlFile | & $mysql @args 2>&1)
        $rc = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $oldPref
        $env:MYSQL_PWD = $oldPwd
    }
    foreach ($entry in $out) { W ("MYSQL|" + $entry.ToString()) }
    W "MYSQL_EXIT=$rc"
    if ($rc -ne 0) { throw "World SQL failed" }
    if (-not (@($out | Where-Object { $_.ToString() -match [regex]::Escape($PassMarker) }).Count)) {
        throw "World SQL did not emit PASS gate: $PassMarker"
    }
    W ("G17B3R12_WORLD_SQL_GATE=PASS " + $PassMarker)
}

New-Item -ItemType Directory -Path $UploadDir -Force | Out-Null
[IO.File]::WriteAllText($Result, "", $Utf8NoBom)
function W([string]$Text) {
    Write-Host $Text
    [IO.File]::AppendAllText($Result, $Text + [Environment]::NewLine, $Utf8NoBom)
}
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
        (Join-Path $env:LOCALAPPDATA "Programs\Python\Python310\python.exe"))
    $python = @($candidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf })[0]
    if (-not $python) { $cmd = Get-Command python.exe -ErrorAction SilentlyContinue; if ($cmd -and $cmd.Source -notmatch "\\WindowsApps\\") { $python = $cmd.Source } }
    return $python
}

$B3R12_BUILD = "r12_targetfix_variety"
try {
    W "G17B3R12_WINDOWS_BUILD_START"
    W ("G17B3R12_BUILD=" + $B3R12_BUILD)
    W "SCOPE=CHECKCAST_TARGET_PREVALIDATION+SWOOP_STRIKE_990029+WIND_STANCE_990030+SLOT7_PAGE_PURE"
    W "RUNS_SQL=True_G17B3R12_WORLD_VARIETY_BINDING"
    W "RUNS_DBC=True_SERVER_DBC_APPEND_990029_990030"
    W "POST_SHA256=$Post"
    if (Get-Process worldserver -ErrorAction SilentlyContinue) { throw "worldserver is running; stop it first" }
    foreach ($file in @($Target, $Tool, $Solution, $Exe, $Pdb)) {
        if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw "file missing: $file" }
    }
    $python = Find-Python
    if (-not $python) { throw "Python not found" }
    W "PYTHON=$python"

    $before = (Get-FileHash -LiteralPath $Target -Algorithm SHA256).Hash.ToLowerInvariant()
    W "SOURCE_SHA256_BEFORE=$before"
    $recognized = @($Pre, $Post, $SafeRollback, "98446106309b45371f138d9c7bc707ee608d9a3db347e13d61cfd68cc97810f9",
        "1a96b72eb28ffa2c0ac0d3e0c07e26c30f25bcd8525babd15efad02a041825d6",
        "ecd307b472cb2c49f68607a8b0afe5dcf5f87a7a8eb6f087a4717f4cd8fa1bbb",
        "feb3dad467188052c7b189478cea7060b14f8e13eb5bd7082d9f81b4ca3ab9ce",
        "a65b0ddcd06a66cfbdf04a91cd4114295615f9ee0c014f92bd742cb6c245b24d",
        "175e5a122765691448738c7db7a25b32535f1fc29d7781e297e10614d4173975",
        "29f3e55470f3ceaab79c8c5a6145ece76a8743c99999adec505a446239c32b3a",
        "f49fd955ec27f2336bfcc6ed8e84f995abaf1d98a1136cf1eb0daefecf563a14",
        "7cb417b3cec7c6d93002c35c96a17748583d412308ac019bf2830fd496afa936", "cd05b8369b42d1176ff674c5eeb1fe49c2f57ebc0e7229034d660b00eabe7d1f", "ddcaa119650510e4b4699ff3a96a6369601e6deb44bd8f6d2ab3683164455c42", "726d403254b6328b05ebdbaf594e8946b6d21e920c5597d568a42f8d7bc339df")
    if ($before -cnotin $recognized) { throw "source not a locked lineage image: $before" }
    $rc = Invoke-NativeLogged -FilePath $python -NativeArgs @($Tool, "apply", "--source-root", $SourceRoot) -Prefix "SOURCE_APPLY"
    W "SOURCE_APPLY_EXIT=$rc"
    if ($rc -ne 0) { throw "source apply failed" }
    $after = (Get-FileHash -LiteralPath $Target -Algorithm SHA256).Hash.ToLowerInvariant()
    W "SOURCE_SHA256_AFTER=$after"
    if ($after -cne $Post) { throw "postimage mismatch" }
    W "G17B3R12_SOURCE_APPLY_GATE=PASS"

    # --- B3-R12 server Spell.dbc append (990029/990030) ---
    $dbcOut = @(& $Python $Appender check --input $ServerDbc 2>&1)
    $dbcExit = $LASTEXITCODE
    $dbcStateLine = @($dbcOut | Where-Object { $_ -match '^G17B3R12_SPELL_DBC_STATE=' } | Select-Object -First 1)[0]
    foreach ($line in $dbcOut) { W ("DBC_CHECK|" + $line.ToString()) }
    W "DBC_CHECK_EXIT=$dbcExit"
    if ($dbcExit -ne 0) { throw "server Spell.dbc check failed" }
    if (-not $dbcStateLine) { throw "server Spell.dbc check printed no state line" }
    if ($dbcStateLine -match 'G17B3R12_SPELL_DBC_STATE=ALREADY_APPENDED') {
        W "G17B3R12_SERVER_DBC_APPEND=ALREADY_APPENDED"
    } elseif ($dbcStateLine -match 'G17B3R12_SPELL_DBC_STATE=PARTIAL') {
        throw "server Spell.dbc has PARTIAL 990029/990030 presence; refusing"
    } elseif ($dbcStateLine -notmatch 'G17B3R12_SPELL_DBC_STATE=MISSING') {
        throw ("Unexpected server Spell.dbc state: " + $dbcStateLine)
    } else {
        $dbBackup = Join-Path $UploadDir ("G17B3R12_Server_DBC_Backup_" + (Get-Date -Format "yyyyMMdd_HHmmss"))
        New-Item -ItemType Directory -Path $dbBackup -Force | Out-Null
        Copy-Item -LiteralPath $ServerDbc -Destination (Join-Path $dbBackup "Spell.dbc.before_g17b3r12")
        $dbcTmp = $ServerDbc + ".g17b3r12.new"
        if (Test-Path -LiteralPath $dbcTmp -PathType Leaf) { Remove-Item -LiteralPath $dbcTmp -Force -ErrorAction SilentlyContinue }
        $rc2 = Invoke-NativeLogged -FilePath $Python -NativeArgs @($Appender, "append", "--input", $ServerDbc, "--output", $dbcTmp) -Prefix "DBC_APPEND"
        W "DBC_APPEND_EXIT=$rc2"
        if ($rc2 -ne 0) { throw "server Spell.dbc append failed" }
        $oldSize = (Get-Item -LiteralPath $ServerDbc).Length
        $newSize = (Get-Item -LiteralPath $dbcTmp).Length
        W "SERVER_DBC_SIZE_BEFORE=$oldSize"
        W "SERVER_DBC_SIZE_AFTER=$newSize"
        if ($newSize -le $oldSize) { throw "appended server DBC did not grow" }
        $dbcOld = $ServerDbc + ".g17b3r12.old"
        Move-Item -LiteralPath $ServerDbc -Destination $dbcOld
        Move-Item -LiteralPath $dbcTmp -Destination $ServerDbc
        Remove-Item -LiteralPath $dbcOld -Force -ErrorAction SilentlyContinue
        W "SERVER_DBC_BACKUP=$dbBackup"
        W "G17B3R12_SERVER_DBC_APPEND=PASS"
    }

    # --- B3-R12 world SQL binding ---
    Invoke-WorldSql -SqlFile $Sql -PassMarker "G17B3R12_WORLD_VARIETY_BINDING=PASS"

    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    $msbuild = @(& $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Where-Object { $_ })[0]
    if (-not $msbuild) { throw "MSBuild not found" }
    [IO.File]::SetLastWriteTimeUtc($Target, [DateTime]::UtcNow)
    Start-Sleep -Milliseconds 150
    $start = [DateTime]::UtcNow
    $beforeExeHash = (Get-FileHash $Exe -Algorithm SHA256).Hash.ToLowerInvariant()
    W "BEFORE_EXE_SHA256=$beforeExeHash"
    $rc = Invoke-NativeLogged -FilePath $msbuild -NativeArgs @($Solution, "/m", "/t:worldserver", "/p:Configuration=RelWithDebInfo", "/p:Platform=x64", "/verbosity:minimal") -Prefix "MSBUILD"
    W "MSBUILD_EXIT=$rc"
    if ($rc -ne 0) { throw "MSBuild failed" }
    $objects = @(Get-ChildItem -LiteralPath $BuildRoot -File -Recurse -Filter '*dragonriding*.obj' | Where-Object { $_.LastWriteTimeUtc -ge $start })
    W "DRAGONRIDING_FRESH_OBJECTS=$($objects.Count)"
    if ($objects.Count -lt 1) { throw "no fresh object" }
    $afterExeHash = (Get-FileHash $Exe -Algorithm SHA256).Hash.ToLowerInvariant()
    W "AFTER_EXE_SHA256=$afterExeHash"
    if ($before -cne $Post -and $afterExeHash -ceq $beforeExeHash) { throw "exe SHA unchanged" }
    W "G17B3R12_WINDOWS_BUILD_RESULT=PASS"
    W "PROOF_OF_LOAD=start worldserver and look for 'G17-B3R6 performance fix LOADED'"
    W "RESULT_FILE=$Result"
    exit 0
} catch {
    W ("G17B3R12_WINDOWS_BUILD_ERROR=" + $_.Exception.Message)
    W "G17B3R12_WINDOWS_BUILD_RESULT=FAIL"
    W "RESULT_FILE=$Result"
    exit 1
}
