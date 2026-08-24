$ErrorActionPreference = "Stop"

$SourceRoot = "D:\TrinityCore"
$BuildRoot = "D:\TC-Build"
$Solution = Join-Path $BuildRoot "TrinityCore.sln"
$RunDir = Join-Path $BuildRoot "bin\RelWithDebInfo"
$Exe = Join-Path $RunDir "worldserver.exe"
$Pdb = Join-Path $RunDir "worldserver.pdb"
$Loader = Join-Path $SourceRoot "src\server\scripts\Commands\cs_script_loader.cpp"
$Payload = Join-Path $SourceRoot "src\server\scripts\Commands\cs_dragonriding.cpp"
$ExpectedLoader = "5502e5b4e22535957f3db81083530b048ec33f6852f4697fbe55795628cee5cc"
$ExpectedPayload = "c9535dca3390ece6735e6ff6b7418ed99ff206628b5e8febd7b78b05cba999bd"
$Workspace = "C:\Users\Administrator\Downloads\workspace"
$UploadDir = Join-Path $Workspace "uploads"
$Result = Join-Path $UploadDir "G17B0_WINDOWS_BUILD_RESULT.txt"
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)

New-Item -ItemType Directory -Path $UploadDir -Force | Out-Null
[System.IO.File]::WriteAllText($Result, "", $Utf8NoBom)

function Write-Result([string]$Line) {
    Write-Host $Line
    [System.IO.File]::AppendAllText($Result, $Line + [Environment]::NewLine, $Utf8NoBom)
}

try {
    Write-Result "G17B0_WINDOWS_BUILD_START"
    Write-Result "SOURCE_ROOT=$SourceRoot"
    Write-Result "BUILD_ROOT=$BuildRoot"
    Write-Result "CONFIGURATION=RelWithDebInfo"
    Write-Result "PLATFORM=x64"

    if (Get-Process worldserver -ErrorAction SilentlyContinue) {
        throw "worldserver正在运行；请先正常关闭后再运行本包"
    }
    foreach ($Path in @($SourceRoot, $BuildRoot, $RunDir)) {
        if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
            throw "找不到目录：$Path"
        }
    }
    foreach ($Path in @($Solution, $Exe, $Pdb, $Loader, $Payload)) {
        if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
            throw "找不到编译门槛文件：$Path"
        }
    }

    $LoaderHash = (Get-FileHash -LiteralPath $Loader -Algorithm SHA256).Hash.ToLowerInvariant()
    $PayloadHash = (Get-FileHash -LiteralPath $Payload -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-Result "LOADER_SHA256=$LoaderHash"
    Write-Result "PAYLOAD_SHA256=$PayloadHash"
    if ($LoaderHash -cne $ExpectedLoader) { throw "loader后像不符：$LoaderHash" }
    if ($PayloadHash -cne $ExpectedPayload) { throw "dragonriding payload不符：$PayloadHash" }
    Write-Result "G17B0_BUILD_SOURCE_GATE=PASS"

    $CMakeCommand = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if (-not $CMakeCommand) { $CMakeCommand = Get-Command cmake -ErrorAction SilentlyContinue }
    if (-not $CMakeCommand) { throw "找不到cmake；请确认CMake已加入PATH" }
    $CMake = $CMakeCommand.Source
    Write-Result "CMAKE=$CMake"

    $VsWhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path -LiteralPath $VsWhere -PathType Leaf)) {
        throw "找不到vswhere.exe：$VsWhere"
    }
    $MSBuild = @(& $VsWhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Where-Object { $_ })[0]
    if (-not $MSBuild -or -not (Test-Path -LiteralPath $MSBuild -PathType Leaf)) {
        throw "找不到Visual Studio MSBuild.exe"
    }
    Write-Result "MSBUILD=$MSBuild"

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

    Write-Result "G17B0_CMAKE_RECONFIGURE_START"
    & $CMake -S $SourceRoot -B $BuildRoot 2>&1 | ForEach-Object { Write-Result ("CMAKE|" + $_.ToString()) }
    $CMakeExit = $LASTEXITCODE
    Write-Result "CMAKE_EXIT=$CMakeExit"
    if ($CMakeExit -ne 0) { throw "CMake重新生成失败，退出码=$CMakeExit" }

    $ProjectHits = @(Get-ChildItem -LiteralPath $BuildRoot -Filter *.vcxproj -File -Recurse |
        Select-String -SimpleMatch "cs_dragonriding.cpp")
    Write-Result "DRAGONRIDING_VCXPROJ_HITS=$($ProjectHits.Count)"
    foreach ($Hit in $ProjectHits) { Write-Result ("VCXPROJ_HIT=" + $Hit.Path) }
    if ($ProjectHits.Count -lt 1) {
        throw "重新生成后没有任何vcxproj包含cs_dragonriding.cpp"
    }
    Write-Result "G17B0_CMAKE_SOURCE_MEMBERSHIP=PASS"

    $BuildStartUtc = [DateTime]::UtcNow
    Write-Result "G17B0_MSBUILD_START_UTC=$($BuildStartUtc.ToString('o'))"
    & $MSBuild $Solution /m /t:worldserver /p:Configuration=RelWithDebInfo /p:Platform=x64 /verbosity:minimal 2>&1 |
        ForEach-Object { Write-Result ("MSBUILD|" + $_.ToString()) }
    $BuildExit = $LASTEXITCODE
    Write-Result "MSBUILD_EXIT=$BuildExit"
    if ($BuildExit -ne 0) { throw "MSBuild失败，退出码=$BuildExit" }

    $DragonObjects = @(Get-ChildItem -LiteralPath $BuildRoot -File -Recurse -Filter "*dragonriding*.obj" |
        Where-Object { $_.LastWriteTimeUtc -ge $BuildStartUtc })
    Write-Result "DRAGONRIDING_FRESH_OBJECTS=$($DragonObjects.Count)"
    foreach ($Object in $DragonObjects) {
        Write-Result ("DRAGONRIDING_OBJECT=" + $Object.FullName + ";size=" + $Object.Length + ";utc=" + $Object.LastWriteTimeUtc.ToString('o'))
    }
    if ($DragonObjects.Count -lt 1) {
        throw "未找到本轮新生成的dragonriding目标文件，不能证明新cpp进入编译"
    }

    $AfterExe = Get-Item -LiteralPath $Exe
    $AfterPdb = Get-Item -LiteralPath $Pdb
    $AfterExeHash = (Get-FileHash -LiteralPath $Exe -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-Result "AFTER_EXE_SIZE=$($AfterExe.Length)"
    Write-Result "AFTER_EXE_UTC=$($AfterExe.LastWriteTimeUtc.ToString('o'))"
    Write-Result "AFTER_EXE_SHA256=$AfterExeHash"
    Write-Result "AFTER_PDB_SIZE=$($AfterPdb.Length)"
    Write-Result "AFTER_PDB_UTC=$($AfterPdb.LastWriteTimeUtc.ToString('o'))"

    if ($AfterExe.Length -le 0 -or $AfterPdb.Length -le 0) { throw "构建产物为空" }
    if ($AfterExe.LastWriteTimeUtc -le $BeforeExeUtc) { throw "worldserver.exe时间未更新" }
    if ($AfterPdb.LastWriteTimeUtc -le $BeforePdbUtc) { throw "worldserver.pdb时间未更新" }
    if ($AfterExeHash -ceq $BeforeExeHash) { throw "worldserver.exe哈希未变化，不能证明新源码已链接" }

    Write-Result "G17B0_WINDOWS_BUILD_PASS=True"
    Write-Result "G17B0_WINDOWS_BUILD_RESULT=PASS"
    Write-Result "G17B0_WINDOWS_BUILD_COMPLETE"
    Write-Result "STOP_DO_NOT_START_WORLDSERVER"
    Write-Result "RESULT_FILE=$Result"
    exit 0
}
catch {
    Write-Result ("G17B0_WINDOWS_BUILD_ERROR=" + $_.Exception.Message)
    Write-Result "G17B0_WINDOWS_BUILD_PASS=False"
    Write-Result "G17B0_WINDOWS_BUILD_RESULT=FAIL"
    Write-Result "STOP_DO_NOT_START_WORLDSERVER"
    Write-Result "RESULT_FILE=$Result"
    exit 1
}
