#requires -Version 5.1
$ErrorActionPreference = "Stop"

$SourceRoot = "D:\TrinityCore"
$BuildRoot = "D:\TC-Build"
$Solution = Join-Path $BuildRoot "TrinityCore.sln"
$RunDir = Join-Path $BuildRoot "bin\RelWithDebInfo"
$Exe = Join-Path $RunDir "worldserver.exe"
$Pdb = Join-Path $RunDir "worldserver.pdb"
$SpellInfo = Join-Path $SourceRoot "src\server\game\Spells\SpellInfo.cpp"
$DragonRiding = Join-Path $SourceRoot "src\server\scripts\Commands\cs_dragonriding.cpp"
$ExpectedPreimage = "73d52ac0feb67a32822fc0bf086a9174ba7ef0bc186223cdc8a690f48fccb9e2"
$ExpectedPostimage = "c3ec2237ed6da8831662a8b7a5d45cf88f8efc7798cdd35c52a07700fa9cbcbf"
$ExpectedDragonRidingR1 = "10a7002db4c173e441836870da01dd3009a7ba470369ca1a89bdb399ee9b2f45"
$Workspace = "C:\Users\Administrator\Downloads\workspace"
$UploadDir = Join-Path $Workspace "uploads"
$Result = Join-Path $UploadDir "G17R3_SERVER_WINDOWS_FIX_RESULT.txt"
$BuildState = Join-Path $UploadDir "G17R3_SERVER_BUILD_STATE.txt"
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$SourceInstaller = Join-Path $PSScriptRoot "tools\apply_g17r3_server_source.py"

New-Item -ItemType Directory -Path $UploadDir -Force | Out-Null
[System.IO.File]::WriteAllText($Result, "", $Utf8NoBom)

function Write-Result([string]$Line) {
    Write-Host $Line
    [System.IO.File]::AppendAllText($Result, $Line + [Environment]::NewLine, $Utf8NoBom)
}

function Read-KeyValueFile([string]$Path) {
    $Values = @{}
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $Values }
    foreach ($Line in [System.IO.File]::ReadAllLines($Path)) {
        if ($Line -match '^([^=]+)=(.*)$') { $Values[$Matches[1]] = $Matches[2] }
    }
    return $Values
}

function Invoke-NativeLogged {
    param(
        [Parameter(Mandatory=$true)][string]$FilePath,
        [Parameter(Mandatory=$true)][string[]]$NativeArgs,
        [Parameter(Mandatory=$true)][string]$Prefix
    )

    $SavedErrorActionPreference = $ErrorActionPreference
    $NativeExit = 9009
    $NativeOutput = @()
    try {
        # Windows PowerShell 5.1 can wrap native stderr as ErrorRecord.
        $ErrorActionPreference = "Continue"
        $NativeOutput = @(& $FilePath @NativeArgs 2>&1)
        $NativeExit = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $SavedErrorActionPreference
    }
    foreach ($NativeLine in $NativeOutput) {
        Write-Result ($Prefix + "|" + $NativeLine.ToString())
    }
    return [int]$NativeExit
}

try {
    Write-Result "G17R3_SERVER_WINDOWS_FIX_START"
    Write-Result "SOURCE_ROOT=$SourceRoot"
    Write-Result "BUILD_ROOT=$BuildRoot"
    Write-Result "CONFIGURATION=RelWithDebInfo"
    Write-Result "PLATFORM=x64"
    Write-Result "G17R3_SERVER_SCOPE=LIVE_OUTDOOR_HARDENING_BEFORE_CLIENT_AREATABLE"
    Write-Result "G17R3_SERVER_REPEATS_R1=False"
    Write-Result "G17R3_SERVER_RUNS_SQL=False"
    Write-Result "G17R3_SERVER_INSTALLS_CLIENT_MPQ=False"

    if (Get-Process worldserver -ErrorAction SilentlyContinue) {
        throw "worldserver is running; stop it normally before running this package"
    }
    foreach ($Path in @($SourceRoot, $BuildRoot, $RunDir)) {
        if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
            throw "required directory not found: $Path"
        }
    }
    foreach ($Path in @($Solution, $Exe, $Pdb, $SpellInfo, $DragonRiding, $SourceInstaller)) {
        if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
            throw "required build-gate file not found: $Path"
        }
    }

    $SpellHashBeforeApply = (Get-FileHash -LiteralPath $SpellInfo -Algorithm SHA256).Hash.ToLowerInvariant()
    $DragonRidingHash = (Get-FileHash -LiteralPath $DragonRiding -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-Result "SPELLINFO_SHA256_BEFORE_APPLY=$SpellHashBeforeApply"
    Write-Result "DRAGONRIDING_R1_SHA256=$DragonRidingHash"
    if (($SpellHashBeforeApply -cne $ExpectedPreimage) -and ($SpellHashBeforeApply -cne $ExpectedPostimage)) {
        throw "SpellInfo.cpp is neither locked G17-R2 preimage nor G17-R3 server postimage: $SpellHashBeforeApply"
    }
    if ($DragonRidingHash -cne $ExpectedDragonRidingR1) {
        throw "G17-R1 dragonriding prerequisite SHA mismatch; R3 will not overwrite or repeat R1"
    }
    $WasPreimage = ($SpellHashBeforeApply -ceq $ExpectedPreimage)
    Write-Result "G17R3_SERVER_PRE_APPLY_SOURCE_GATE=PASS"
    Write-Result "G17R1_PREREQUISITE_GATE=PASS"

    $PreparedState = Read-KeyValueFile $BuildState
    if ($WasPreimage) {
        if (Test-Path -LiteralPath $BuildState -PathType Leaf) {
            if ($PreparedState["BUILD_STATUS"] -cne "PREPARED") {
                throw "R2 source is preimage but existing R3 build state is not PREPARED"
            }
            foreach ($Key in @("BACKUP_EXE","BACKUP_EXE_SHA256","BACKUP_PDB","BACKUP_PDB_SHA256")) {
                if (-not $PreparedState.ContainsKey($Key)) { throw "prepared R3 build state key missing: $Key" }
            }
            foreach ($BackupKey in @("BACKUP_EXE","BACKUP_PDB")) {
                if (-not (Test-Path -LiteralPath $PreparedState[$BackupKey] -PathType Leaf)) { throw "prepared R3 binary backup missing: $BackupKey" }
            }
            $PreparedBackupExeHash = (Get-FileHash -LiteralPath $PreparedState["BACKUP_EXE"] -Algorithm SHA256).Hash.ToLowerInvariant()
            $PreparedBackupPdbHash = (Get-FileHash -LiteralPath $PreparedState["BACKUP_PDB"] -Algorithm SHA256).Hash.ToLowerInvariant()
            if ($PreparedBackupExeHash -cne $PreparedState["BACKUP_EXE_SHA256"] -or
                $PreparedBackupPdbHash -cne $PreparedState["BACKUP_PDB_SHA256"]) { throw "prepared R3 binary backup hash changed" }
            if ((Get-FileHash -LiteralPath $Exe -Algorithm SHA256).Hash.ToLowerInvariant() -cne $PreparedBackupExeHash -or
                (Get-FileHash -LiteralPath $Pdb -Algorithm SHA256).Hash.ToLowerInvariant() -cne $PreparedBackupPdbHash) {
                throw "R2 source is preimage but active binaries no longer match prepared R2 backups"
            }
            Write-Result "R3_PREPARED_STATE=REUSED_BEFORE_APPLY"
        }
        else {
            $BinaryBackupDir = Join-Path $UploadDir ("G17R3_Server_Binary_Backup_" + (Get-Date -Format "yyyyMMdd_HHmmss"))
            New-Item -ItemType Directory -Path $BinaryBackupDir | Out-Null
            $BackupExe = Join-Path $BinaryBackupDir "worldserver.exe.before_g17r3"
            $BackupPdb = Join-Path $BinaryBackupDir "worldserver.pdb.before_g17r3"
            Copy-Item -LiteralPath $Exe -Destination $BackupExe
            Copy-Item -LiteralPath $Pdb -Destination $BackupPdb
            $OldExeHash = (Get-FileHash -LiteralPath $BackupExe -Algorithm SHA256).Hash.ToLowerInvariant()
            $OldPdbHash = (Get-FileHash -LiteralPath $BackupPdb -Algorithm SHA256).Hash.ToLowerInvariant()
            $PreparedLines = @(
                "STATE_FORMAT=1","BUILD_STATUS=PREPARED",
                ("SOURCE_PRE_SHA256=" + $ExpectedPreimage),("SOURCE_POST_SHA256=" + $ExpectedPostimage),
                ("BACKUP_EXE=" + $BackupExe),("BACKUP_EXE_SHA256=" + $OldExeHash),
                ("BACKUP_PDB=" + $BackupPdb),("BACKUP_PDB_SHA256=" + $OldPdbHash)
            )
            [System.IO.File]::WriteAllText($BuildState,(($PreparedLines -join [Environment]::NewLine)+[Environment]::NewLine),$Utf8NoBom)
            Write-Result "R2_BINARY_BACKUP_DIR=$BinaryBackupDir"
            Write-Result "R2_BACKUP_EXE_SHA256=$OldExeHash"
            Write-Result "R2_BACKUP_PDB_SHA256=$OldPdbHash"
            $PreparedState = Read-KeyValueFile $BuildState
        }
    }
    else {
        if (-not (Test-Path -LiteralPath $BuildState -PathType Leaf)) { throw "R3 source is postimage but prepared binary backup state is missing" }
        if (($PreparedState["BUILD_STATUS"] -cne "PREPARED") -and ($PreparedState["BUILD_STATUS"] -cne "PASS")) {
            throw "R3 build state status is neither PREPARED nor PASS"
        }
        foreach ($Key in @("BACKUP_EXE","BACKUP_EXE_SHA256","BACKUP_PDB","BACKUP_PDB_SHA256")) {
            if (-not $PreparedState.ContainsKey($Key)) { throw "R3 build state key missing: $Key" }
        }
        foreach ($BackupKey in @("BACKUP_EXE","BACKUP_PDB")) {
            if (-not (Test-Path -LiteralPath $PreparedState[$BackupKey] -PathType Leaf)) { throw "R3 binary backup missing: $BackupKey" }
        }
        if ((Get-FileHash -LiteralPath $PreparedState["BACKUP_EXE"] -Algorithm SHA256).Hash.ToLowerInvariant() -cne $PreparedState["BACKUP_EXE_SHA256"]) { throw "R3 EXE backup hash changed" }
        if ((Get-FileHash -LiteralPath $PreparedState["BACKUP_PDB"] -Algorithm SHA256).Hash.ToLowerInvariant() -cne $PreparedState["BACKUP_PDB_SHA256"]) { throw "R3 PDB backup hash changed" }
        if ($PreparedState["BUILD_STATUS"] -ceq "PASS") {
            foreach ($Key in @("NEW_EXE","NEW_EXE_SHA256","NEW_PDB","NEW_PDB_SHA256")) {
                if (-not $PreparedState.ContainsKey($Key)) { throw "PASS R3 build state key missing: $Key" }
            }
            if ($PreparedState["NEW_EXE"] -ine $Exe -or $PreparedState["NEW_PDB"] -ine $Pdb) { throw "PASS R3 build state output path mismatch" }
            if ((Get-FileHash -LiteralPath $Exe -Algorithm SHA256).Hash.ToLowerInvariant() -cne $PreparedState["NEW_EXE_SHA256"] -or
                (Get-FileHash -LiteralPath $Pdb -Algorithm SHA256).Hash.ToLowerInvariant() -cne $PreparedState["NEW_PDB_SHA256"]) {
                throw "active R3 binary hash changed since PASS state"
            }
        }
        Write-Result "R3_BINARY_BACKUP_STATE=REUSED"
        Write-Result ("R3_EXISTING_BUILD_STATUS=" + $PreparedState["BUILD_STATUS"])
    }

    $ComSpec = $env:ComSpec
    if (-not $ComSpec -or -not (Test-Path -LiteralPath $ComSpec -PathType Leaf)) {
        throw "ComSpec command processor not found"
    }
    $NativeSelfTestArgs = @("/d", "/c", "echo G17R3_SERVER_NATIVE_STDOUT& echo G17R3_SERVER_NATIVE_STDERR 1>&2& exit /b 0")
    $NativeSelfTestExit = Invoke-NativeLogged -FilePath $ComSpec -NativeArgs $NativeSelfTestArgs -Prefix "NATIVE_SELFTEST"
    Write-Result "NATIVE_RUNNER_SELFTEST_EXIT=$NativeSelfTestExit"
    if ($NativeSelfTestExit -ne 0) { throw "native runner self-test failed" }
    Write-Result "G17R3_SERVER_NATIVE_RUNNER_SELFTEST=PASS"

    $PythonCandidates = @(
        (Join-Path $env:LOCALAPPDATA "Programs\Python\Python312\python.exe"),
        (Join-Path $env:LOCALAPPDATA "Programs\Python\Python310\python.exe")
    )
    $Python = @($PythonCandidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf })[0]
    if (-not $Python) {
        $PythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
        if (-not $PythonCommand) { $PythonCommand = Get-Command python -ErrorAction SilentlyContinue }
        if ($PythonCommand -and $PythonCommand.Source -notmatch "\\WindowsApps\\") {
            $Python = $PythonCommand.Source
        }
    }
    if (-not $Python) { throw "Python 3.12/3.10 was not found; py.exe and WindowsApps aliases are not used" }
    Write-Result "PYTHON=$Python"
    $PythonArgs = @($SourceInstaller, "apply", "--source-root", $SourceRoot)
    $ApplyExit = Invoke-NativeLogged -FilePath $Python -NativeArgs $PythonArgs -Prefix "SOURCE_APPLY"
    Write-Result "SOURCE_APPLY_EXIT=$ApplyExit"
    if ($ApplyExit -ne 0) { throw "G17-R3 server exact-hash source apply failed; exit code=$ApplyExit" }

    $SpellHashAfterApply = (Get-FileHash -LiteralPath $SpellInfo -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-Result "SPELLINFO_SHA256_AFTER_APPLY=$SpellHashAfterApply"
    if ($SpellHashAfterApply -cne $ExpectedPostimage) {
        throw "SpellInfo.cpp G17-R3 server postimage SHA mismatch after apply: $SpellHashAfterApply"
    }
    Write-Result "G17R3_SERVER_SOURCE_APPLY_GATE=PASS"

    $VsWhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path -LiteralPath $VsWhere -PathType Leaf)) {
        throw "vswhere.exe not found: $VsWhere"
    }
    $MSBuild = @(& $VsWhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Where-Object { $_ })[0]
    if (-not $MSBuild -or -not (Test-Path -LiteralPath $MSBuild -PathType Leaf)) {
        throw "Visual Studio MSBuild.exe not found"
    }
    Write-Result "MSBUILD=$MSBuild"

    $ProjectHits = @(Get-ChildItem -LiteralPath $BuildRoot -Filter *.vcxproj -File -Recurse |
        Select-String -SimpleMatch "SpellInfo.cpp")
    Write-Result "SPELLINFO_VCXPROJ_HITS=$($ProjectHits.Count)"
    foreach ($Hit in $ProjectHits) { Write-Result ("VCXPROJ_HIT=" + $Hit.Path) }
    if ($ProjectHits.Count -lt 1) { throw "no vcxproj contains SpellInfo.cpp" }
    Write-Result "G17R3_SERVER_CMAKE_SOURCE_MEMBERSHIP=PASS"

    $BeforeExe = Get-Item -LiteralPath $Exe
    $BeforePdb = Get-Item -LiteralPath $Pdb
    $BeforeExeHash = (Get-FileHash -LiteralPath $Exe -Algorithm SHA256).Hash.ToLowerInvariant()
    $BeforeExeUtc = $BeforeExe.LastWriteTimeUtc
    $BeforePdbUtc = $BeforePdb.LastWriteTimeUtc
    Write-Result "BEFORE_EXE_SIZE=$($BeforeExe.Length)"
    Write-Result "BEFORE_EXE_UTC=$($BeforeExeUtc.ToString('o'))"
    Write-Result "BEFORE_EXE_SHA256=$BeforeExeHash"
    Write-Result "BEFORE_PDB_SIZE=$($BeforePdb.Length)"
    Write-Result "BEFORE_PDB_UTC=$($BeforePdbUtc.ToString('o'))"

    # Touch without changing bytes so an idempotent rerun still recompiles this TU.
    [System.IO.File]::SetLastWriteTimeUtc($SpellInfo, [DateTime]::UtcNow)
    Start-Sleep -Milliseconds 150
    $BuildStartUtc = [DateTime]::UtcNow
    Write-Result "G17R3_SERVER_MSBUILD_START_UTC=$($BuildStartUtc.ToString('o'))"
    $MSBuildArgs = @($Solution, "/m", "/t:worldserver", "/p:Configuration=RelWithDebInfo", "/p:Platform=x64", "/verbosity:minimal")
    $BuildExit = Invoke-NativeLogged -FilePath $MSBuild -NativeArgs $MSBuildArgs -Prefix "MSBUILD"
    Write-Result "MSBUILD_EXIT=$BuildExit"
    if ($BuildExit -ne 0) { throw "MSBuild failed; exit code=$BuildExit" }

    $SpellObjects = @(Get-ChildItem -LiteralPath $BuildRoot -File -Recurse -Filter "*SpellInfo*.obj" |
        Where-Object { $_.LastWriteTimeUtc -ge $BuildStartUtc })
    Write-Result "SPELLINFO_FRESH_OBJECTS=$($SpellObjects.Count)"
    foreach ($Object in $SpellObjects) {
        Write-Result ("SPELLINFO_OBJECT=" + $Object.FullName + ";size=" + $Object.Length + ";utc=" + $Object.LastWriteTimeUtc.ToString('o'))
    }
    if ($SpellObjects.Count -lt 1) {
        throw "no fresh SpellInfo object was produced; new C++ compilation is not proven"
    }

    $AfterExe = Get-Item -LiteralPath $Exe
    $AfterPdb = Get-Item -LiteralPath $Pdb
    $AfterExeHash = (Get-FileHash -LiteralPath $Exe -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-Result "AFTER_EXE_SIZE=$($AfterExe.Length)"
    Write-Result "AFTER_EXE_UTC=$($AfterExe.LastWriteTimeUtc.ToString('o'))"
    Write-Result "AFTER_EXE_SHA256=$AfterExeHash"
    Write-Result "AFTER_PDB_SIZE=$($AfterPdb.Length)"
    Write-Result "AFTER_PDB_UTC=$($AfterPdb.LastWriteTimeUtc.ToString('o'))"

    if ($AfterExe.Length -le 0 -or $AfterPdb.Length -le 0) { throw "build output is empty" }
    if ($AfterExe.LastWriteTimeUtc -le $BeforeExeUtc) { throw "worldserver.exe timestamp was not updated" }
    if ($AfterPdb.LastWriteTimeUtc -le $BeforePdbUtc) { throw "worldserver.pdb timestamp was not updated" }
    if ($WasPreimage -and ($AfterExeHash -ceq $BeforeExeHash)) {
        throw "first G17-R3 server build did not change worldserver.exe SHA"
    }
    $AfterPdbHash = (Get-FileHash -LiteralPath $Pdb -Algorithm SHA256).Hash.ToLowerInvariant()
    $FinalStateLines = @(
        "STATE_FORMAT=1","BUILD_STATUS=PASS",
        ("SOURCE_PRE_SHA256=" + $ExpectedPreimage),("SOURCE_POST_SHA256=" + $ExpectedPostimage),
        ("BACKUP_EXE=" + $PreparedState["BACKUP_EXE"]),("BACKUP_EXE_SHA256=" + $PreparedState["BACKUP_EXE_SHA256"]),
        ("BACKUP_PDB=" + $PreparedState["BACKUP_PDB"]),("BACKUP_PDB_SHA256=" + $PreparedState["BACKUP_PDB_SHA256"]),
        ("NEW_EXE=" + $Exe),("NEW_EXE_SHA256=" + $AfterExeHash),
        ("NEW_PDB=" + $Pdb),("NEW_PDB_SHA256=" + $AfterPdbHash),
        ("BUILT_AT=" + (Get-Date).ToString("o"))
    )
    $BuildStateTemp = $BuildState + ".tmp"
    $BuildStatePrevious = $BuildState + ".previous.tmp"
    if ((Test-Path -LiteralPath $BuildStateTemp) -or (Test-Path -LiteralPath $BuildStatePrevious)) { throw "R3 build state swap temp already exists" }
    [System.IO.File]::WriteAllText($BuildStateTemp,(($FinalStateLines -join [Environment]::NewLine)+[Environment]::NewLine),$Utf8NoBom)
    try {
        [System.IO.File]::Replace($BuildStateTemp,$BuildState,$BuildStatePrevious)
    }
    catch {
        if (-not (Test-Path -LiteralPath $BuildState -PathType Leaf) -and (Test-Path -LiteralPath $BuildStatePrevious -PathType Leaf)) {
            Move-Item -LiteralPath $BuildStatePrevious -Destination $BuildState
        }
        throw
    }
    Remove-Item -LiteralPath $BuildStatePrevious -Force
    Write-Result "G17R3_SERVER_BUILD_STATE=$BuildState"
    Write-Result "AFTER_PDB_SHA256=$AfterPdbHash"
    Write-Result "G17R3_SERVER_BUILD_SOURCE_GATE=PASS"

    Write-Result "G17R3_SERVER_WINDOWS_BUILD_PASS=True"
    Write-Result "G17R3_SERVER_WINDOWS_BUILD_RESULT=PASS"
    Write-Result "G17R3_SERVER_WINDOWS_BUILD_COMPLETE"
    Write-Result "STOP_DO_NOT_START_WORLDSERVER"
    Write-Result "NEXT=Run the G17-R3 client MPQ upgrade; do not start worldserver yet"
    Write-Result "CLIENT_UPGRADE_REQUIRED=True"
    Write-Result "RESULT_FILE=$Result"
    exit 0
}
catch {
    Write-Result ("G17R3_SERVER_WINDOWS_BUILD_ERROR=" + $_.Exception.Message)
    Write-Result "G17R3_SERVER_WINDOWS_BUILD_PASS=False"
    Write-Result "G17R3_SERVER_WINDOWS_BUILD_RESULT=FAIL"
    Write-Result "STOP_DO_NOT_START_WORLDSERVER"
    Write-Result "RESULT_FILE=$Result"
    exit 1
}
