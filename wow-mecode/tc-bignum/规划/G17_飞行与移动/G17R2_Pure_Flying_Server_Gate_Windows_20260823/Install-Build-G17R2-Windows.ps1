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
$ExpectedPreimage = "537e5c350baa5f4a90bd0ec38c6b6858360e287aeabd75ab54050b4432e50755"
$ExpectedPostimage = "73d52ac0feb67a32822fc0bf086a9174ba7ef0bc186223cdc8a690f48fccb9e2"
$ExpectedDragonRidingR1 = "10a7002db4c173e441836870da01dd3009a7ba470369ca1a89bdb399ee9b2f45"
$Workspace = "C:\Users\Administrator\Downloads\workspace"
$UploadDir = Join-Path $Workspace "uploads"
$Result = Join-Path $UploadDir "G17R2_WINDOWS_FIX_RESULT.txt"
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$SourceInstaller = Join-Path $PSScriptRoot "tools\apply_g17r2_source.py"

New-Item -ItemType Directory -Path $UploadDir -Force | Out-Null
[System.IO.File]::WriteAllText($Result, "", $Utf8NoBom)

function Write-Result([string]$Line) {
    Write-Host $Line
    [System.IO.File]::AppendAllText($Result, $Line + [Environment]::NewLine, $Utf8NoBom)
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
    Write-Result "G17R2_WINDOWS_FIX_START"
    Write-Result "SOURCE_ROOT=$SourceRoot"
    Write-Result "BUILD_ROOT=$BuildRoot"
    Write-Result "CONFIGURATION=RelWithDebInfo"
    Write-Result "PLATFORM=x64"
    Write-Result "G17R2_SCOPE=SPELLINFO_STRICT_LOCATION_ONLY"
    Write-Result "G17R2_REPEATS_R1=False"
    Write-Result "G17R2_RUNS_SQL=False"
    Write-Result "G17R2_INSTALLS_CLIENT_MPQ=False"

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
        throw "SpellInfo.cpp is neither locked G17-A preimage nor G17-R2 postimage: $SpellHashBeforeApply"
    }
    if ($DragonRidingHash -cne $ExpectedDragonRidingR1) {
        throw "G17-R1 dragonriding prerequisite SHA mismatch; this R2 package will not overwrite or repeat R1"
    }
    $WasPreimage = ($SpellHashBeforeApply -ceq $ExpectedPreimage)
    Write-Result "G17R2_PRE_APPLY_SOURCE_GATE=PASS"
    Write-Result "G17R1_PREREQUISITE_GATE=PASS"

    $ComSpec = $env:ComSpec
    if (-not $ComSpec -or -not (Test-Path -LiteralPath $ComSpec -PathType Leaf)) {
        throw "ComSpec command processor not found"
    }
    $NativeSelfTestArgs = @("/d", "/c", "echo G17R2_NATIVE_STDOUT& echo G17R2_NATIVE_STDERR 1>&2& exit /b 0")
    $NativeSelfTestExit = Invoke-NativeLogged -FilePath $ComSpec -NativeArgs $NativeSelfTestArgs -Prefix "NATIVE_SELFTEST"
    Write-Result "NATIVE_RUNNER_SELFTEST_EXIT=$NativeSelfTestExit"
    if ($NativeSelfTestExit -ne 0) { throw "native runner self-test failed" }
    Write-Result "G17R2_NATIVE_RUNNER_SELFTEST=PASS"

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
    if ($ApplyExit -ne 0) { throw "G17-R2 exact-hash source apply failed; exit code=$ApplyExit" }

    $SpellHashAfterApply = (Get-FileHash -LiteralPath $SpellInfo -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-Result "SPELLINFO_SHA256_AFTER_APPLY=$SpellHashAfterApply"
    if ($SpellHashAfterApply -cne $ExpectedPostimage) {
        throw "SpellInfo.cpp G17-R2 postimage SHA mismatch after apply: $SpellHashAfterApply"
    }
    Write-Result "G17R2_SOURCE_APPLY_GATE=PASS"

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
    Write-Result "G17R2_CMAKE_SOURCE_MEMBERSHIP=PASS"

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
    Write-Result "G17R2_MSBUILD_START_UTC=$($BuildStartUtc.ToString('o'))"
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
        throw "first G17-R2 build did not change worldserver.exe SHA"
    }
    Write-Result "G17R2_BUILD_SOURCE_GATE=PASS"

    Write-Result "G17R2_WINDOWS_BUILD_PASS=True"
    Write-Result "G17R2_WINDOWS_BUILD_RESULT=PASS"
    Write-Result "G17R2_WINDOWS_BUILD_COMPLETE"
    Write-Result "STOP_DO_NOT_START_WORLDSERVER"
    Write-Result "NEXT=Start worldserver normally, then test Red Proto-Drake spell 59961 in safe outdoor Wetlands"
    Write-Result "EXPECTED_LOG=G17R2 old-world pure-flight location allowed: spell=59961"
    Write-Result "RESULT_FILE=$Result"
    exit 0
}
catch {
    Write-Result ("G17R2_WINDOWS_BUILD_ERROR=" + $_.Exception.Message)
    Write-Result "G17R2_WINDOWS_BUILD_PASS=False"
    Write-Result "G17R2_WINDOWS_BUILD_RESULT=FAIL"
    Write-Result "STOP_DO_NOT_START_WORLDSERVER"
    Write-Result "RESULT_FILE=$Result"
    exit 1
}
