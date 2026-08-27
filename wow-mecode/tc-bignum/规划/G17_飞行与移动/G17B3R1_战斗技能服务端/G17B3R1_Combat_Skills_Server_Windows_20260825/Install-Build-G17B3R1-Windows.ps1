#requires -Version 5.1
# G17-B3R1: combat skill carriers server-side install.
#   1) source apply (cs_dragonriding.cpp -> B3R1 postimage)
#   2) server dbc\Spell.dbc append (25 records 990000-990024)
#   3) world SQL binding (spell_script_names -> spell_g17_combat_skill)
#   4) MSBuild /t:worldserver (fresh obj + exe/pdb proof)
# FIX5 (f2_lineage_upgrade): the locked-lineage gate now also accepts the
#   pre-FIX4 B3R1 postimage 1a96b72e... (INTERMEDIATE7) so a rerun over the
#   user's real source state (apply succeeded, MSBuild failed) upgrades
#   directly to the current postimage instead of being rejected.
# FIX6 (f3_decl_order): fixes the 5 remaining REAL MSVC compile errors in the
#   FIX4 payload (user log 2026-08-25): C3861 RevokeCombatSkills decl too
#   late, C2061/C2660/C2143/C2059 CombatStunReleaseEvent defined after use,
#   C2664 GetUnit(Player*, ...) needs *caster (const WorldObject&).  The
#   FIX4 postimage ecd307b4 is now INTERMEDIATE8 (upgradeable).
param(
    [string]$Workspace = "C:\Users\Administrator\Downloads\workspace",
    [string]$SourceRoot = "D:\TrinityCore",
    [string]$BuildRoot = "D:\TC-Build"
)
$ErrorActionPreference = "Stop"
$UploadDir = Join-Path $Workspace "uploads"
$Result = Join-Path $UploadDir "G17B3R1_WINDOWS_BUILD_RESULT.txt"
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$Target = Join-Path $SourceRoot "src\server\scripts\Commands\cs_dragonriding.cpp"
$Tool = Join-Path $PSScriptRoot "tools\apply_g17b3r1_source.py"
$Appender = Join-Path $PSScriptRoot "tools\append_g17b3_spells.py"
$Tests = Join-Path $PSScriptRoot "tests\test_g17b3r1.py"
$PackageTest = Join-Path $PSScriptRoot "Test-G17B3R1-Package.py"
$Sql = Join-Path $PSScriptRoot "sql\G17B3R1_world_combat_binding.sql"
$Solution = Join-Path $BuildRoot "TrinityCore.sln"
$RunDir = Join-Path $BuildRoot "bin\RelWithDebInfo"
$Exe = Join-Path $RunDir "worldserver.exe"
$Pdb = Join-Path $RunDir "worldserver.pdb"
$WorldConf = Join-Path $RunDir "worldserver.conf"
$ServerDbc = Join-Path $RunDir "dbc\Spell.dbc"

function Read-ToolHash([string]$Name) {
    $pattern = ('^\s*' + [regex]::Escape($Name) + '\s*=\s*"([0-9a-f]+)"')
    $line = @(Get-Content -LiteralPath $Tool | Where-Object { $_ -match $pattern })[0]
    if (-not $line) { throw "could not read $Name from $Tool" }
    if ($line -notmatch $pattern) { throw "could not parse $Name from $Tool" }
    return $Matches[1]
}
$Pre = Read-ToolHash "PRE_SHA256"
$Post = Read-ToolHash "POST_SHA256"
$SafeRollback = Read-ToolHash "SAFE_ROLLBACK_SHA256"
$Upgradeable = @(
    (Read-ToolHash "INTERMEDIATE_SHA256"), (Read-ToolHash "INTERMEDIATE2_SHA256"),
    (Read-ToolHash "INTERMEDIATE3_SHA256"), (Read-ToolHash "INTERMEDIATE4_SHA256"),
    (Read-ToolHash "INTERMEDIATE5_SHA256"), (Read-ToolHash "INTERMEDIATE6_SHA256"),
    (Read-ToolHash "INTERMEDIATE7_SHA256"), (Read-ToolHash "INTERMEDIATE8_SHA256")
)

New-Item -ItemType Directory -Path $UploadDir -Force | Out-Null
[IO.File]::WriteAllText($Result, "", $Utf8NoBom)
function W([string]$Text) {
    Write-Host $Text
    [IO.File]::AppendAllText($Result, $Text + [Environment]::NewLine, $Utf8NoBom)
}
function Invoke-NativeLogged {
    param([string]$FilePath, [string[]]$NativeArgs, [string]$Prefix)
    $old = $ErrorActionPreference; $out = @(); $rc = 9009
    try {
        $ErrorActionPreference = "Continue"
        $out = @(& $FilePath @NativeArgs 2>&1)
        $rc = $LASTEXITCODE
    } finally { $ErrorActionPreference = $old }
    foreach ($line in $out) { W ($Prefix + "|" + $line.ToString()) }
    return [int]$rc
}
function Find-Python {
    $candidates = @(
        (Join-Path $env:LOCALAPPDATA "Programs\Python\Python312\python.exe"),
        (Join-Path $env:LOCALAPPDATA "Programs\Python\Python310\python.exe"),
        "C:\Python312\python.exe", "C:\Python310\python.exe")
    $python = @($candidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf })[0]
    if (-not $python) {
        $cmd = Get-Command python.exe -ErrorAction SilentlyContinue
        if ($cmd -and $cmd.Source -notmatch "\\WindowsApps\\") { $python = $cmd.Source }
    }
    return $python
}
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
    W ("G17B3R1_WORLD_SQL_GATE=PASS " + $PassMarker)
}

$B3R1_BUILD = "f3_decl_order"
try {
    W "G17B3R1_WINDOWS_BUILD_START"
    W ("B3R1_BUILD=" + $B3R1_BUILD)
    W "SCOPE=B3_COMBAT_SKILLS_25_CARRIERS+RIDER_EXIT_NORMALIZE+8000210_APPEND"
    W "RUNS_DBC_APPEND=True"
    W "RUNS_SQL=True"
    W "DATABASE=world"
    W "POST_SHA256=$Post"
    if (Get-Process worldserver -ErrorAction SilentlyContinue) { throw "worldserver is running; stop it first" }
    foreach ($dir in @($SourceRoot, $BuildRoot, $RunDir)) {
        if (-not (Test-Path -LiteralPath $dir -PathType Container)) { throw "directory missing: $dir" }
    }
    foreach ($file in @($Target, $Tool, $Appender, $Tests, $PackageTest, $Sql, $Solution, $Exe, $Pdb, $WorldConf, $ServerDbc)) {
        if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw "file missing: $file" }
    }
    $python = Find-Python
    if (-not $python) { throw "Python312/Python310 not found" }
    W "PYTHON=$python"

    $rc = Invoke-NativeLogged -FilePath $python -NativeArgs @($PackageTest, "--package-root", $PSScriptRoot) -Prefix "PACKAGE_TEST"
    W "PACKAGE_TEST_EXIT=$rc"
    if ($rc -ne 0) { throw "package self-test failed" }
    $rc = Invoke-NativeLogged -FilePath $python -NativeArgs @($Tests) -Prefix "UNIT_TEST"
    W "UNIT_TEST_EXIT=$rc"
    if ($rc -ne 0) { throw "B3R1 unit tests failed" }

    $before = (Get-FileHash -LiteralPath $Target -Algorithm SHA256).Hash.ToLowerInvariant()
    W "SOURCE_SHA256_BEFORE=$before"
    $recognized = @($Pre, $Post, $SafeRollback) + $Upgradeable
    if ($before -cnotin $recognized) { throw "dragonriding source is not a locked lineage image: $before" }
    $rc = Invoke-NativeLogged -FilePath $python -NativeArgs @($Tool, "apply", "--source-root", $SourceRoot) -Prefix "SOURCE_APPLY"
    W "SOURCE_APPLY_EXIT=$rc"
    if ($rc -ne 0) { throw "source apply failed" }
    $after = (Get-FileHash -LiteralPath $Target -Algorithm SHA256).Hash.ToLowerInvariant()
    W "SOURCE_SHA256_AFTER=$after"
    if ($after -cne $Post) { throw "B3R1 postimage SHA mismatch" }
    W "G17B3R1_SOURCE_APPLY_GATE=PASS"

    # --- server Spell.dbc append (semantic, no fixed postimage; preimage is
    #     the user's zhCN DBC = 49839 records / 234 fields, gates already 0/0) ---
    # The B3 appender's `check` prints to STDOUT (it does not write a report
    # file, unlike the C1 patch_g17c1 checker).  Capture stdout and parse the
    # state line directly; never read a file that may not exist.
    $dbcOut = @(& $python $Appender check --input $ServerDbc 2>&1)
    $dbcExit = $LASTEXITCODE
    $dbcStateLine = @($dbcOut | Where-Object { $_ -match '^G17B3_SPELL_DBC_STATE=' } | Select-Object -First 1)[0]
    foreach ($line in $dbcOut) { W ("DBC_CHECK|" + $line.ToString()) }
    W "DBC_CHECK_EXIT=$dbcExit"
    if ($dbcStateLine) { W "DBC_CHECK_STATE=$($dbcStateLine.ToString().Trim())" }
    if ($dbcExit -ne 0) { throw "server Spell.dbc check failed" }
    if (-not $dbcStateLine) { throw "server Spell.dbc check printed no state line" }
    if ($dbcStateLine -match 'G17B3_SPELL_DBC_STATE=ALREADY_APPENDED') {
        W "G17B3R1_SERVER_DBC_APPEND=ALREADY_APPENDED"
    } elseif ($dbcStateLine -match 'G17B3_SPELL_DBC_STATE=PARTIAL') {
        throw "server Spell.dbc has PARTIAL 990000-990024 presence; refusing"
    } elseif ($dbcStateLine -notmatch 'G17B3_SPELL_DBC_STATE=MISSING') {
        throw ("Unexpected server Spell.dbc state: " + $dbcStateLine)
    } else {
        $dbBackup = Join-Path $UploadDir ("G17B3R1_Server_DBC_Backup_" + (Get-Date -Format "yyyyMMdd_HHmmss"))
        New-Item -ItemType Directory -Path $dbBackup -Force | Out-Null
        Copy-Item -LiteralPath $ServerDbc -Destination (Join-Path $dbBackup "Spell.dbc.before_g17b3r1")
        $dbcTmp = $ServerDbc + ".g17b3r1.new"
        if (Test-Path -LiteralPath $dbcTmp -PathType Leaf) { Remove-Item -LiteralPath $dbcTmp -Force -ErrorAction SilentlyContinue }
        $rc = Invoke-NativeLogged -FilePath $python -NativeArgs @($Appender, "append", "--input", $ServerDbc, "--output", $dbcTmp) -Prefix "DBC_APPEND"
        W "DBC_APPEND_EXIT=$rc"
        if ($rc -ne 0) { throw "server Spell.dbc append failed" }
        $oldSize = (Get-Item -LiteralPath $ServerDbc).Length
        $newSize = (Get-Item -LiteralPath $dbcTmp).Length
        W "SERVER_DBC_SIZE_BEFORE=$oldSize"
        W "SERVER_DBC_SIZE_AFTER=$newSize"
        if ($newSize -le $oldSize) { throw "appended server DBC did not grow" }
        $dbcOld = $ServerDbc + ".g17b3r1.old"
        Move-Item -LiteralPath $ServerDbc -Destination $dbcOld
        Move-Item -LiteralPath $dbcTmp -Destination $ServerDbc
        Remove-Item -LiteralPath $dbcOld -Force -ErrorAction SilentlyContinue
        W "SERVER_DBC_BACKUP=$dbBackup"
        W "G17B3R1_SERVER_DBC_APPEND=PASS"
    }

    # --- world SQL binding ---
    Invoke-WorldSql -SqlFile $Sql -PassMarker "G17B3R1_WORLD_COMBAT_BINDING=PASS"

    # --- build ---
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) { throw "vswhere missing" }
    $msbuild = @(& $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Where-Object { $_ })[0]
    if (-not $msbuild) { throw "MSBuild not found" }
    W "MSBUILD=$msbuild"
    $hits = @(Get-ChildItem -LiteralPath $BuildRoot -Filter *.vcxproj -File -Recurse | Select-String -SimpleMatch "cs_dragonriding.cpp")
    W "DRAGONRIDING_VCXPROJ_HITS=$($hits.Count)"
    if ($hits.Count -lt 1) { throw "cs_dragonriding.cpp absent from generated projects" }
    $beforeExe = Get-Item $Exe; $beforePdb = Get-Item $Pdb
    $beforeExeUtc = $beforeExe.LastWriteTimeUtc; $beforePdbUtc = $beforePdb.LastWriteTimeUtc
    $beforeExeHash = (Get-FileHash $Exe -Algorithm SHA256).Hash.ToLowerInvariant()
    W "BEFORE_EXE_SHA256=$beforeExeHash"
    W "BEFORE_EXE_UTC=$($beforeExeUtc.ToString('o'))"
    W "BEFORE_PDB_UTC=$($beforePdbUtc.ToString('o'))"
    [IO.File]::SetLastWriteTimeUtc($Target, [DateTime]::UtcNow)
    Start-Sleep -Milliseconds 150
    $start = [DateTime]::UtcNow
    W "BUILD_START_UTC=$($start.ToString('o'))"
    $buildArgs = @($Solution, "/m", "/t:worldserver", "/p:Configuration=RelWithDebInfo", "/p:Platform=x64", "/verbosity:minimal")
    $rc = Invoke-NativeLogged -FilePath $msbuild -NativeArgs $buildArgs -Prefix "MSBUILD"
    W "MSBUILD_EXIT=$rc"
    if ($rc -ne 0) { throw "MSBuild failed" }
    $objects = @(Get-ChildItem -LiteralPath $BuildRoot -File -Recurse -Filter '*dragonriding*.obj' | Where-Object { $_.LastWriteTimeUtc -ge $start })
    W "DRAGONRIDING_FRESH_OBJECTS=$($objects.Count)"
    foreach ($object in $objects) { W ("FRESH_OBJECT=" + $object.FullName + ";size=" + $object.Length) }
    if ($objects.Count -lt 1) { throw "fresh dragonriding object not proven" }
    $afterExe = Get-Item $Exe; $afterPdb = Get-Item $Pdb
    $afterExeHash = (Get-FileHash $Exe -Algorithm SHA256).Hash.ToLowerInvariant()
    W "AFTER_EXE_SHA256=$afterExeHash"
    W "AFTER_EXE_SIZE=$($afterExe.Length)"
    W "AFTER_EXE_UTC=$($afterExe.LastWriteTimeUtc.ToString('o'))"
    W "AFTER_PDB_UTC=$($afterPdb.LastWriteTimeUtc.ToString('o'))"
    if ($afterExe.LastWriteTimeUtc -le $beforeExeUtc -or $afterPdb.LastWriteTimeUtc -le $beforePdbUtc) {
        throw "exe/pdb timestamp did not advance"
    }
    if ($afterExe.LastWriteTimeUtc -lt $start -or $afterPdb.LastWriteTimeUtc -lt $start) { throw "exe/pdb predates build start" }
    if ($before -cne $Post -and $afterExeHash -ceq $beforeExeHash) { throw "first B3R1 build did not change worldserver.exe SHA" }
    W "G17B3R1_WINDOWS_BUILD_RESULT=PASS"
    W "PROOF_OF_LOAD=start worldserver and confirm 'G17-B3R1 combat skills LOADED' in worldserver.log"
    W "NEXT=Start worldserver, then test in game: mount dragon -> spellbook shows 5 G17 combat skills per archetype -> direct leave vehicle restores normal flight-off state."
    W "RESULT_FILE=$Result"
    exit 0
} catch {
    W ("G17B3R1_WINDOWS_BUILD_ERROR=" + $_.Exception.Message)
    W "G17B3R1_WINDOWS_BUILD_RESULT=FAIL"
    W "STOP_DO_NOT_START_WORLDSERVER"
    W "RESULT_FILE=$Result"
    exit 1
}
