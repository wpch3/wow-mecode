#requires -Version 5.1
param([string]$Workspace = "C:\Users\Administrator\Downloads\workspace", [string]$SourceRoot = "D:\TrinityCore", [string]$BuildRoot = "D:\TC-Build")
$ErrorActionPreference = "Stop"
$UploadDir = Join-Path $Workspace "uploads"
$Result = Join-Path $UploadDir "G17B3R8_ROLLBACK_RESULT.txt"
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$Tool = Join-Path $PSScriptRoot "tools\apply_g17b3r8_source.py"
$Target = Join-Path $SourceRoot "src\server\scripts\Commands\cs_dragonriding.cpp"
New-Item -ItemType Directory -Path $UploadDir -Force | Out-Null
[IO.File]::WriteAllText($Result, "", $Utf8NoBom)
function W([string]$Text) { Write-Host $Text; [IO.File]::AppendAllText($Result, $Text + [Environment]::NewLine, $Utf8NoBom) }
try {
    W "G17B3R8_ROLLBACK_START"
    if (Get-Process worldserver -ErrorAction SilentlyContinue) { throw "stop worldserver first" }
    $python = Join-Path $env:LOCALAPPDATA "Programs\Python\Python312\python.exe"
    if (-not (Test-Path $python)) { $python = Join-Path $env:LOCALAPPDATA "Programs\Python\Python310\python.exe" }
    $rc = & $python $Tool rollback --source-root $SourceRoot 2>&1
    foreach ($line in $rc) { W ("ROLLBACK|" + $line.ToString()) }
    if ($LASTEXITCODE -ne 0) { throw "rollback failed" }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    $msbuild = @(& $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Where-Object { $_ })[0]
    [IO.File]::SetLastWriteTimeUtc($Target, [DateTime]::UtcNow)
    $rc2 = & $msbuild (Join-Path $BuildRoot "TrinityCore.sln") "/m" "/t:worldserver" "/p:Configuration=RelWithDebInfo" "/p:Platform=x64" "/verbosity:minimal" 2>&1
    W "MSBUILD_EXIT=$LASTEXITCODE"
    W "G17B3R8_ROLLBACK_RESULT=PASS"
    exit 0
} catch {
    W ("G17B3R8_ROLLBACK_ERROR=" + $_.Exception.Message)
    W "G17B3R8_ROLLBACK_RESULT=FAIL"
    exit 1
}
