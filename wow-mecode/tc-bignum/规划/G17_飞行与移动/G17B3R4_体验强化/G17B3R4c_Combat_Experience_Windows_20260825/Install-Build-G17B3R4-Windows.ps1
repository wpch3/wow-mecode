#requires -Version 5.1
# G17-B3R4: combat experience rework install.
#   1) source apply (cs_dragonriding.cpp -> B3R4 postimage)
#   2) server dbc\Spell.dbc range fix for the 25 combat carriers (1 -> 4 = 30yd;
#      fixes the user-reported "无法命中" - self-range records reject explicit
#      targets in Spell::CheckRange)
#   3) install the G17DragonBar v2 addon into the client (Interface\AddOns)
#   4) MSBuild /t:worldserver (fresh obj + exe/pdb proof)
# No SQL and no DBC steps: every binding from B3R2 is already in place
# (spell_script_names 990025-990028 + creature_template_spell rows + both
# Spell.dbc images contain all carriers).
# What B3R4 changes (user live-test feedback):
#   - damage now scales with the rider's LEVEL (+15/level, finisher +45/level)
#     so skills hit hard at endgame instead of tens of damage;
#   - combat feedback: the dragon swings at the target (melee attack packets)
#     and an audited impact kit fires on it; heals pulse a ribbon kit;
#   - the 25 combat carriers get a real 30yd range in the server Spell.dbc
#     (RangeIndex 1 -> 4): explicit-target casts no longer fail with 无法命中;
#   - the mount fights alongside the rider: assisted strikes every 3.5s while
#     the rider is in combat (rider-attributed, LOS+range gated, never drains
#     energy below 20 so the rider is never grounded);
#   - G17DragonBar v2: no IsSpellKnown dependency (3.3.5 lacks it - that is
#     why the v1 bar never showed); spellbook-name scan + hand-built secure
#     buttons; /g17bar debug reports detection state.
param(
    [string]$Workspace = "C:\Users\Administrator\Downloads\workspace",
    [string]$SourceRoot = "D:\TrinityCore",
    [string]$BuildRoot = "D:\TC-Build",
    [string]$ClientRoot = "D:\WOW"
)
$ErrorActionPreference = "Stop"
$UploadDir = Join-Path $Workspace "uploads"
$Result = Join-Path $UploadDir "G17B3R4_WINDOWS_BUILD_RESULT.txt"
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$Target = Join-Path $SourceRoot "src\server\scripts\Commands\cs_dragonriding.cpp"
$Tool = Join-Path $PSScriptRoot "tools\apply_g17b3r4_source.py"
$Tests = Join-Path $PSScriptRoot "tests\test_g17b3r4.py"
$PackageTest = Join-Path $PSScriptRoot "Test-G17B3R4-Package.py"
$Solution = Join-Path $BuildRoot "TrinityCore.sln"
$RunDir = Join-Path $BuildRoot "bin\RelWithDebInfo"
$Exe = Join-Path $RunDir "worldserver.exe"
$Pdb = Join-Path $RunDir "worldserver.pdb"
$AddonSrc = Join-Path $PSScriptRoot "addon_src\G17DragonBar"
$AddonDst = Join-Path $ClientRoot "Interface\AddOns\G17DragonBar"
$RangePatcher = Join-Path $PSScriptRoot "tools\patch_g17b3r4_ranges.py"
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
    (Read-ToolHash "INTERMEDIATE7_SHA256")
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

$B3R4_BUILD = "r1c_lineage_gate"
try {
    W "G17B3R4_WINDOWS_BUILD_START"
    W ("B3R4_BUILD=" + $B3R4_BUILD)
    W "SCOPE=B3R4_LEVEL_SCALED_DAMAGE+COMBAT_VISUALS+RANGE30_FIX+MOUNT_AUTOCOMBAT+ADDON_V2"
    W "RUNS_SQL=False"
    W "RUNS_DBC=True"
    W "DBC_STEP=range_patch_990000_990024_to_30yd"
    W "POST_SHA256=$Post"
    if (Get-Process worldserver -ErrorAction SilentlyContinue) { throw "worldserver is running; stop it first" }
    if (Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Name -match '^(?i:wow|wow-64)$' }) {
        throw "Wow client is running; close it first (the addon folder must be installed cleanly)"
    }
    foreach ($dir in @($SourceRoot, $BuildRoot, $RunDir, $ClientRoot)) {
        if (-not (Test-Path -LiteralPath $dir -PathType Container)) { throw "directory missing: $dir" }
    }
    foreach ($file in @($Target, $Tool, $RangePatcher, $Tests, $PackageTest, $Solution, $Exe, $Pdb, $ServerDbc,
            (Join-Path $AddonSrc "G17DragonBar.toc"), (Join-Path $AddonSrc "G17DragonBar.lua"))) {
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
    if ($rc -ne 0) { throw "B3R4 unit tests failed" }

    $before = (Get-FileHash -LiteralPath $Target -Algorithm SHA256).Hash.ToLowerInvariant()
    W "SOURCE_SHA256_BEFORE=$before"
    $recognized = @($Pre, $Post, $SafeRollback) + $Upgradeable
    if ($before -cnotin $recognized) { throw "dragonriding source is not a locked lineage image: $before" }
    $rc = Invoke-NativeLogged -FilePath $python -NativeArgs @($Tool, "apply", "--source-root", $SourceRoot) -Prefix "SOURCE_APPLY"
    W "SOURCE_APPLY_EXIT=$rc"
    if ($rc -ne 0) { throw "source apply failed" }
    $after = (Get-FileHash -LiteralPath $Target -Algorithm SHA256).Hash.ToLowerInvariant()
    W "SOURCE_SHA256_AFTER=$after"
    if ($after -cne $Post) { throw "B3R4 postimage SHA mismatch" }
    W "G17B3R4_SOURCE_APPLY_GATE=PASS"

    # --- server Spell.dbc range fix for the 25 combat carriers ---
    $dbcOut = @(& $python $RangePatcher check --input $ServerDbc 2>&1)
    $dbcExit = $LASTEXITCODE
    $dbcStateLine = @($dbcOut | Where-Object { $_ -match '^G17B3R4_RANGE_STATE=' } | Select-Object -First 1)[0]
    foreach ($line in $dbcOut) { W ("RANGE_CHECK|" + $line.ToString()) }
    W "RANGE_CHECK_EXIT=$dbcExit"
    if ($dbcStateLine) { W "RANGE_CHECK_STATE=$($dbcStateLine.ToString().Trim())" }
    if ($dbcExit -ne 0) { throw "server Spell.dbc range check failed" }
    if (-not $dbcStateLine) { throw "range patcher printed no state line" }
    if ($dbcStateLine -match 'G17B3R4_RANGE_STATE=PATCHED') {
        W "G17B3R4_RANGE_PATCH=ALREADY_PATCHED"
    } elseif ($dbcStateLine -match 'G17B3R4_RANGE_STATE=MISSING') {
        $rangeBackup = Join-Path $UploadDir ("G17B3R4_Server_DBC_Backup_" + (Get-Date -Format "yyyyMMdd_HHmmss"))
        New-Item -ItemType Directory -Path $rangeBackup -Force | Out-Null
        Copy-Item -LiteralPath $ServerDbc -Destination (Join-Path $rangeBackup "Spell.dbc.before_g17b3r4")
        $rangeTmp = $ServerDbc + ".g17b3r4.new"
        if (Test-Path -LiteralPath $rangeTmp -PathType Leaf) { Remove-Item -LiteralPath $rangeTmp -Force -ErrorAction SilentlyContinue }
        $rc = Invoke-NativeLogged -FilePath $python -NativeArgs @($RangePatcher, "patch", "--input", $ServerDbc, "--output", $rangeTmp) -Prefix "RANGE_PATCH"
        W "RANGE_PATCH_EXIT=$rc"
        if ($rc -ne 0) { throw "server Spell.dbc range patch failed" }
        $oldSize = (Get-Item -LiteralPath $ServerDbc).Length
        $newSize = (Get-Item -LiteralPath $rangeTmp).Length
        W "SERVER_DBC_SIZE_BEFORE=$oldSize"
        W "SERVER_DBC_SIZE_AFTER=$newSize"
        if ($newSize -ne $oldSize) { throw "range patch changed the file size (must be in-place)" }
        $rangeOld = $ServerDbc + ".g17b3r4.old"
        Move-Item -LiteralPath $ServerDbc -Destination $rangeOld
        Move-Item -LiteralPath $rangeTmp -Destination $ServerDbc
        Remove-Item -LiteralPath $rangeOld -Force -ErrorAction SilentlyContinue
        W "SERVER_DBC_RANGE_BACKUP=$rangeBackup"
        W "G17B3R4_RANGE_PATCH=PASS"
    } else {
        throw ("Unexpected server Spell.dbc range state: " + $dbcStateLine)
    }

    # --- addon install (backup any existing copy, then replace) ---
    if (Test-Path -LiteralPath $AddonDst -PathType Container) {
        $stamp = Get-Date -Format "yyyyMMdd_HHmmss"
        $addonBackup = Join-Path $UploadDir ("G17B3R4_Addon_Backup_" + $stamp)
        New-Item -ItemType Directory -Path $addonBackup -Force | Out-Null
        Copy-Item -LiteralPath $AddonDst -Destination (Join-Path $addonBackup "G17DragonBar") -Recurse
        W "ADDON_BACKUP=$addonBackup"
        Remove-Item -LiteralPath $AddonDst -Recurse -Force
    }
    $addonParent = Split-Path -Parent $AddonDst
    New-Item -ItemType Directory -Path $addonParent -Force | Out-Null
    Copy-Item -LiteralPath $AddonSrc -Destination $AddonDst -Recurse
    foreach ($rel in @("G17DragonBar.toc", "G17DragonBar.lua")) {
        if (-not (Test-Path -LiteralPath (Join-Path $AddonDst $rel) -PathType Leaf)) { throw "addon install incomplete: $rel" }
    }
    W "ADDON_INSTALLED=$AddonDst"
    W "G17B3R4_ADDON_INSTALL=PASS"

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
    if ($before -cne $Post -and $afterExeHash -ceq $beforeExeHash) { throw "first B3R4 build did not change worldserver.exe SHA" }
    W "G17B3R4_WINDOWS_BUILD_RESULT=PASS"
    W "PROOF_OF_LOAD=start worldserver and confirm 'G17-B3R4 combat experience LOADED' in worldserver.log"
    W "NEXT=Start worldserver + the WoW client; summon a mount: the G17DragonBar v2 appears with 11 buttons; combat skills now hit for level-scaled damage with visuals; enter combat and the mount auto-strikes alongside you."
    W "RESULT_FILE=$Result"
    exit 0
} catch {
    W ("G17B3R4_WINDOWS_BUILD_ERROR=" + $_.Exception.Message)
    W "G17B3R4_WINDOWS_BUILD_RESULT=FAIL"
    W "STOP_DO_NOT_START_WORLDSERVER"
    W "RESULT_FILE=$Result"
    exit 1
}
