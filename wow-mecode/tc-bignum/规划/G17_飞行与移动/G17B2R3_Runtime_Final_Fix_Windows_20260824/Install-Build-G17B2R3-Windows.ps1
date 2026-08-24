#requires -Version 5.1
param(
    [string]$Workspace = "C:\Users\Administrator\Downloads\workspace",
    [string]$SourceRoot = "D:\TrinityCore",
    [string]$BuildRoot = "D:\TC-Build"
)
$ErrorActionPreference = "Stop"
$UploadDir = Join-Path $Workspace "uploads"
$Result = Join-Path $UploadDir "G17B2R3_WINDOWS_BUILD_RESULT.txt"
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$Target = Join-Path $SourceRoot "src\server\scripts\Commands\cs_dragonriding.cpp"
$Tool = Join-Path $PSScriptRoot "tools\apply_g17b2r3_source.py"
$Tests = Join-Path $PSScriptRoot "tests\test_g17b2r3.py"
$PackageTest = Join-Path $PSScriptRoot "Test-G17B2R3-Package.py"
$Sql = Join-Path $PSScriptRoot "sql\G17B2R3_world_landing_binding_guard.sql"
$SqlCastable = Join-Path $PSScriptRoot "sql\G17B2R3_world_spell52226_castable_override.sql"
$Solution = Join-Path $BuildRoot "TrinityCore.sln"
$RunDir = Join-Path $BuildRoot "bin\RelWithDebInfo"
$Exe = Join-Path $RunDir "worldserver.exe"
$Pdb = Join-Path $RunDir "worldserver.pdb"
$WorldConf = Join-Path $RunDir "worldserver.conf"
# Hashes are READ from the Python installer tool so they can never drift from
# the payload. Hard-coding them here was the root cause of the B2R2 FAIL where
# the source was already the new postimage but this script rejected it.
function Read-ToolHash([string]$Name) {
    $pattern = ('^\s*' + [regex]::Escape($Name) + '\s*=\s*"([0-9a-f]+)"')
    $line = @(Get-Content -LiteralPath $Tool | Where-Object { $_ -match $pattern })[0]
    if (-not $line) { throw "could not read $Name from $Tool" }
    # IMPORTANT: $Matches set inside the Where-Object filter scope does not
    # survive into this function scope, so re-run the match directly here and
    # read the capture group from the function-scope $Matches (same proven
    # two-step pattern as the WorldDatabaseInfo parser below).
    if ($line -notmatch $pattern) { throw "could not parse $Name from $Tool" }
    return $Matches[1]
}
$Pre          = Read-ToolHash "PRE_SHA256"
$Post         = Read-ToolHash "POST_SHA256"
$SafeRollback = Read-ToolHash "SAFE_ROLLBACK_SHA256"
# Every valid upgrade source lives in the Python tool (single source of
# truth): B2R1 postimage, both earlier R2 drafts, the banner-less final and
# the unverified-kits intermediate.  The installer must never hard-code
# them, or the tool and PS1 drift like before.
$Upgradeable  = @(
    (Read-ToolHash "INTERMEDIATE_SHA256"),
    (Read-ToolHash "INTERMEDIATE2_SHA256"),
    (Read-ToolHash "INTERMEDIATE3_SHA256"),
    (Read-ToolHash "INTERMEDIATE4_SHA256"),
    (Read-ToolHash "INTERMEDIATE5_SHA256")
)

New-Item -ItemType Directory -Path $UploadDir -Force | Out-Null
[IO.File]::WriteAllText($Result, "", $Utf8NoBom)
function W([string]$Text) {
    Write-Host $Text
    [IO.File]::AppendAllText($Result, $Text + [Environment]::NewLine, $Utf8NoBom)
}
function Invoke-NativeLogged {
    param(
        [Parameter(Mandatory=$true)][string]$FilePath,
        [Parameter(Mandatory=$true)][string[]]$NativeArgs,
        [Parameter(Mandatory=$true)][string]$Prefix
    )
    $old = $ErrorActionPreference
    $out = @()
    $rc = 9009
    try {
        $ErrorActionPreference = "Continue"
        $out = @(& $FilePath @NativeArgs 2>&1)
        $rc = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $old
    }
    foreach ($line in $out) { W ($Prefix + "|" + $line.ToString()) }
    return [int]$rc
}
function Find-Python {
    $candidates = @(
        (Join-Path $env:LOCALAPPDATA "Programs\Python\Python312\python.exe"),
        (Join-Path $env:LOCALAPPDATA "Programs\Python\Python310\python.exe"),
        "C:\Python312\python.exe",
        "C:\Python310\python.exe"
    )
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
        "C:\xampp\mysql\bin\mysql.exe",
        "D:\xampp\mysql\bin\mysql.exe",
        "C:\mysql\bin\mysql.exe",
        "D:\mysql\bin\mysql.exe",
        "C:\Program Files\MySQL\MySQL Server 8.0\bin\mysql.exe",
        "C:\Program Files\MariaDB 11.4\bin\mariadb.exe",
        "C:\Program Files\MariaDB 10.11\bin\mariadb.exe"
    )
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
function Invoke-WorldMigration {
    param(
        [Parameter(Mandatory=$true)][string]$SqlFile,
        [Parameter(Mandatory=$true)][string]$PassMarker,
        [Parameter(Mandatory=$true)][string]$Label
    )
    if (-not (Test-Path -LiteralPath $WorldConf -PathType Leaf)) {
        throw "worldserver.conf missing: $WorldConf"
    }
    if (-not (Test-Path -LiteralPath $SqlFile -PathType Leaf)) {
        throw "SQL file missing: $SqlFile"
    }
    $line = @(Get-Content -LiteralPath $WorldConf | Where-Object { $_ -match '^\s*WorldDatabaseInfo\s*=\s*"([^"]+)"' })[0]
    if (-not $line) { throw "WorldDatabaseInfo not found in worldserver.conf" }
    if ($line -notmatch '^\s*WorldDatabaseInfo\s*=\s*"([^"]+)"') { throw "WorldDatabaseInfo parse failed" }
    $parts = $Matches[1].Split(';')
    if ($parts.Count -lt 5) { throw "WorldDatabaseInfo has fewer than five fields" }
    $hostName = $parts[0]; $port = $parts[1]; $user = $parts[2]; $password = $parts[3]; $database = $parts[4]
    if ($database -cne "world") { throw "WorldDatabaseInfo database is not explicit world: $database" }
    if ($port -notmatch '^\d+$') { throw "invalid WorldDatabaseInfo port" }
    $mysql = Find-MySqlClient
    if (-not $mysql) { throw "mysql.exe or mariadb.exe not found" }
    W "WORLD_CONF=$WorldConf"
    W "MYSQL_CLIENT=$mysql"
    W "WORLD_DATABASE=$database"
    $args = @(
        "--protocol=TCP", ("--host=" + $hostName), ("--port=" + $port),
        ("--user=" + $user), "--default-character-set=utf8mb4", "--database=world",
        "--batch", "--raw", "--skip-column-names"
    )
    $oldPwd = $env:MYSQL_PWD
    $oldPreference = $ErrorActionPreference
    $out = @(); $rc = 9009
    try {
        $env:MYSQL_PWD = $password
        $ErrorActionPreference = "Continue"
        $out = @(Get-Content -LiteralPath $SqlFile | & $mysql @args 2>&1)
        $rc = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $oldPreference
        $env:MYSQL_PWD = $oldPwd
    }
    foreach ($entry in $out) { W ("MYSQL|" + $entry.ToString()) }
    W "MYSQL_EXIT=$rc"
    if ($rc -ne 0) { throw ("World SQL failed: " + $Label) }
    if (-not (@($out | Where-Object { $_.ToString() -match [regex]::Escape($PassMarker) }).Count)) {
        throw ("World SQL did not emit PASS gate: " + $PassMarker)
    }
    W ("G17B2R3_WORLD_SQL_GATE=PASS " + $Label)
}
try {
    W "G17B2R3_WINDOWS_BUILD_START"
    W "SCOPE=SKILL2_LAYERED_AUDITED_VISUALS_SKILL3_ANTI_REVERSE_SKILL4_52226_SANITIZED"
    W "RUNS_SQL=True"
    W "DATABASE=world"
    W "MODIFIES_CLIENT=False"
    W "BANNER=G17-B2R3_LOADED"
    W "POST_SHA256=$Post"
    if (Get-Process worldserver -ErrorAction SilentlyContinue) { throw "worldserver is running; stop it normally first" }
    foreach ($dir in @($SourceRoot, $BuildRoot, $RunDir)) {
        if (-not (Test-Path -LiteralPath $dir -PathType Container)) { throw "directory missing: $dir" }
    }
    foreach ($file in @($Target, $Tool, $Tests, $PackageTest, $Sql, $SqlCastable, $Solution, $Exe, $Pdb, $WorldConf)) {
        if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw "file missing: $file" }
    }
    $python = Find-Python
    if (-not $python) { throw "Python312/Python310 not found; py.exe/WindowsApps aliases are not used" }
    W "PYTHON=$python"
    $rc = Invoke-NativeLogged -FilePath $python -NativeArgs @($PackageTest, "--package-root", $PSScriptRoot) -Prefix "PACKAGE_TEST"
    W "PACKAGE_TEST_EXIT=$rc"
    if ($rc -ne 0) { throw "package self-test failed" }
    $rc = Invoke-NativeLogged -FilePath $python -NativeArgs @($Tests) -Prefix "UNIT_TEST"
    W "UNIT_TEST_EXIT=$rc"
    if ($rc -ne 0) { throw "B2R3 unit tests failed" }
    $before = (Get-FileHash -LiteralPath $Target -Algorithm SHA256).Hash.ToLowerInvariant()
    W "SOURCE_SHA256_BEFORE=$before"
    $recognized = @($Pre, $Post, $SafeRollback) + $Upgradeable
    if ($before -cnotin $recognized) {
        throw ("dragonriding source is not a locked R2-lineage image: " + $before)
    }
    $first = ($before -cne $Post)
    $rc = Invoke-NativeLogged -FilePath $python -NativeArgs @($Tool, "apply", "--source-root", $SourceRoot) -Prefix "SOURCE_APPLY"
    W "SOURCE_APPLY_EXIT=$rc"
    if ($rc -ne 0) { throw "source apply failed" }
    $after = (Get-FileHash -LiteralPath $Target -Algorithm SHA256).Hash.ToLowerInvariant()
    W "SOURCE_SHA256_AFTER=$after"
    if ($after -cne $Post) { throw "B2R3 postimage SHA mismatch" }
    W "G17B2R3_SOURCE_APPLY_GATE=PASS"
    Invoke-WorldMigration -SqlFile $Sql -PassMarker "G17B2R3_WORLD_BINDING_GUARD=PASS" -Label "52226_BINDING"
    Invoke-WorldMigration -SqlFile $SqlCastable -PassMarker "G17B2R3_SPELL_52226_CASTABLE=PASS" -Label "52226_CASTABLE"
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) { throw "vswhere missing" }
    $msbuild = @(& $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Where-Object { $_ })[0]
    if (-not $msbuild) { throw "MSBuild not found" }
    W "MSBUILD=$msbuild"
    $hits = @(Get-ChildItem -LiteralPath $BuildRoot -Filter *.vcxproj -File -Recurse | Select-String -SimpleMatch "cs_dragonriding.cpp")
    W "DRAGONRIDING_VCXPROJ_HITS=$($hits.Count)"
    if ($hits.Count -lt 1) { throw "cs_dragonriding.cpp absent from generated projects" }
    $beforeExe = Get-Item $Exe
    $beforePdb = Get-Item $Pdb
    $beforeExeUtc = $beforeExe.LastWriteTimeUtc
    $beforePdbUtc = $beforePdb.LastWriteTimeUtc
    $beforeExeHash = (Get-FileHash $Exe -Algorithm SHA256).Hash.ToLowerInvariant()
    W "BEFORE_EXE_SHA256=$beforeExeHash"
    W "BEFORE_EXE_UTC=$($beforeExeUtc.ToString('o'))"
    W "BEFORE_PDB_UTC=$($beforePdbUtc.ToString('o'))"
    [IO.File]::SetLastWriteTimeUtc($Target, [DateTime]::UtcNow)
    Start-Sleep -Milliseconds 150
    $start = [DateTime]::UtcNow
    W "BUILD_START_UTC=$($start.ToString('o'))"
    # IMPORTANT: build the worldserver TARGET, not only the scripts static lib,
    # so worldserver.exe is relinked and the new code actually loads.
    $buildArgs = @($Solution, "/m", "/t:worldserver", "/p:Configuration=RelWithDebInfo", "/p:Platform=x64", "/verbosity:minimal")
    $rc = Invoke-NativeLogged -FilePath $msbuild -NativeArgs $buildArgs -Prefix "MSBUILD"
    W "MSBUILD_EXIT=$rc"
    if ($rc -ne 0) { throw "MSBuild failed" }
    $objects = @(Get-ChildItem -LiteralPath $BuildRoot -File -Recurse -Filter '*dragonriding*.obj' | Where-Object { $_.LastWriteTimeUtc -ge $start })
    W "DRAGONRIDING_FRESH_OBJECTS=$($objects.Count)"
    foreach ($object in $objects) { W ("FRESH_OBJECT=" + $object.FullName + ";size=" + $object.Length) }
    if ($objects.Count -lt 1) { throw "fresh dragonriding object not proven" }
    $afterExe = Get-Item $Exe
    $afterPdb = Get-Item $Pdb
    $afterExeHash = (Get-FileHash $Exe -Algorithm SHA256).Hash.ToLowerInvariant()
    W "AFTER_EXE_SHA256=$afterExeHash"
    W "AFTER_EXE_SIZE=$($afterExe.Length)"
    W "AFTER_EXE_UTC=$($afterExe.LastWriteTimeUtc.ToString('o'))"
    W "AFTER_PDB_UTC=$($afterPdb.LastWriteTimeUtc.ToString('o'))"
    if ($afterExe.LastWriteTimeUtc -le $beforeExeUtc -or $afterPdb.LastWriteTimeUtc -le $beforePdbUtc) {
        throw "exe/pdb timestamp did not advance (worldserver target was not relinked?)"
    }
    if ($afterExe.LastWriteTimeUtc -lt $start -or $afterPdb.LastWriteTimeUtc -lt $start) {
        throw "exe/pdb predates build start"
    }
    if ($first -and $afterExeHash -ceq $beforeExeHash) {
        throw "first B2R3 build did not change worldserver.exe SHA"
    }
    W "G17B2R3_WINDOWS_BUILD_RESULT=PASS"
    W "PROOF_OF_LOAD=start worldserver and confirm 'G17-B2R3 LOADED' appears in worldserver.log"
    W "NEXT=Start worldserver normally; press skills 2/3/4 once, then send back this result file plus the G17-B2R3 log lines."
    W "RESULT_FILE=$Result"
    exit 0
} catch {
    W ("G17B2R3_WINDOWS_BUILD_ERROR=" + $_.Exception.Message)
    W "G17B2R3_WINDOWS_BUILD_RESULT=FAIL"
    W "STOP_DO_NOT_START_WORLDSERVER"
    W "RESULT_FILE=$Result"
    exit 1
}
