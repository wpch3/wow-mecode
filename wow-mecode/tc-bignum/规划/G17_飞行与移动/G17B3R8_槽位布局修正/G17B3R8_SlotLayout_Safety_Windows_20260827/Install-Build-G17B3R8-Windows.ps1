#requires -Version 5.1
# G17-B3R8: slot-layout safety fix on top of B3R7's cooldown packets.
#   B3-R7 put the page switch at m_spells[6] (slot 7) - live testing showed the
#   3.3.5 client only fills/renders 6 vehicle bonus slots, so the switch button
#   vanished.  B3-R8: page switch ALWAYS at slot 6 on both pages; brake 990028
#   at slot 7 (G17DragonBar v6 shows it natively or via its secure
#   spell-cast fallback).  Keeps all B3R7 cooldown UI packets unchanged.
# Works from B3R6 (3fdb46e8) OR B3R7 (f2360d7e) source states.
# Source-only change, no DBC/SQL/addon steps. Payload dcfa78dd, rollback 3fdb46e8.
param(
    [string]$Workspace = "C:\Users\Administrator\Downloads\workspace",
    [string]$SourceRoot = "D:\TrinityCore",
    [string]$BuildRoot = "D:\TC-Build"
)
$ErrorActionPreference = "Stop"
$UploadDir = Join-Path $Workspace "uploads"
$Result = Join-Path $UploadDir "G17B3R8_WINDOWS_BUILD_RESULT.txt"
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$Target = Join-Path $SourceRoot "src\server\scripts\Commands\cs_dragonriding.cpp"
$Tool = Join-Path $PSScriptRoot "tools\apply_g17b3r8_source.py"
$Solution = Join-Path $BuildRoot "TrinityCore.sln"
$RunDir = Join-Path $BuildRoot "bin\RelWithDebInfo"
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

$B3R6_BUILD = "r1_perf_fix"
try {
    W "G17B3R8_WINDOWS_BUILD_START"
    W ("B3R6_BUILD=" + $B3R6_BUILD)
    W "SCOPE=REMOVE_LEARNSPELL_ON_MOUNT+TARGET_VALIDATION_IN_CHECKCAST"
    W "RUNS_SQL=False"
    W "RUNS_DBC=False"
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
    W "G17B3R8_SOURCE_APPLY_GATE=PASS"

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
    W "G17B3R8_WINDOWS_BUILD_RESULT=PASS"
    W "PROOF_OF_LOAD=start worldserver and look for 'G17-B3R6 performance fix LOADED'"
    W "RESULT_FILE=$Result"
    exit 0
} catch {
    W ("G17B3R8_WINDOWS_BUILD_ERROR=" + $_.Exception.Message)
    W "G17B3R8_WINDOWS_BUILD_RESULT=FAIL"
    W "RESULT_FILE=$Result"
    exit 1
}
