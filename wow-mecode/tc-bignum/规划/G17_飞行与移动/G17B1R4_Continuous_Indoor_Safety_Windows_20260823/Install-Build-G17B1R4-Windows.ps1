#requires -Version 5.1
param([string]$Workspace="C:\Users\Administrator\Downloads\workspace",[string]$SourceRoot="D:\TrinityCore",[string]$BuildRoot="D:\TC-Build")
$ErrorActionPreference="Stop"
$UploadDir=Join-Path $Workspace "uploads";$Result=Join-Path $UploadDir "G17B1R4_WINDOWS_BUILD_RESULT.txt";$Utf8NoBom=New-Object System.Text.UTF8Encoding($false)
$Target=Join-Path $SourceRoot "src\server\scripts\Commands\cs_dragonriding.cpp";$Tool=Join-Path $PSScriptRoot "tools\apply_g17b1r4_source.py"
$Solution=Join-Path $BuildRoot "TrinityCore.sln";$RunDir=Join-Path $BuildRoot "bin\RelWithDebInfo";$Exe=Join-Path $RunDir "worldserver.exe";$Pdb=Join-Path $RunDir "worldserver.pdb"
$Pre="94ff80334783e8883f0811a1a7f76595d91b729cc43684f00273abb9d955628b";$Post="e9418704731a2d9cd5119cc2024079a2326802796d00bf24e88928dd17ea7059"
New-Item -ItemType Directory -Path $UploadDir -Force|Out-Null;[IO.File]::WriteAllText($Result,"",$Utf8NoBom)
function W([string]$x){Write-Host $x;[IO.File]::AppendAllText($Result,$x+[Environment]::NewLine,$Utf8NoBom)}
function Invoke-NativeLogged {
 param([Parameter(Mandatory=$true)][string]$FilePath,[Parameter(Mandatory=$true)][string[]]$NativeArgs,[Parameter(Mandatory=$true)][string]$Prefix)
 $old=$ErrorActionPreference;$out=@();$rc=9009
 try{$ErrorActionPreference="Continue";$out=@(& $FilePath @NativeArgs 2>&1);$rc=$LASTEXITCODE}finally{$ErrorActionPreference=$old}
 foreach($line in $out){W ($Prefix+"|"+$line.ToString())};return [int]$rc
}
function FindPython(){
 $c=@((Join-Path $env:LOCALAPPDATA "Programs\Python\Python312\python.exe"),(Join-Path $env:LOCALAPPDATA "Programs\Python\Python310\python.exe"));$p=@($c|Where-Object{Test-Path -LiteralPath $_ -PathType Leaf})[0]
 if(-not $p){$cmd=Get-Command python.exe -ErrorAction SilentlyContinue;if(-not $cmd){$cmd=Get-Command python -ErrorAction SilentlyContinue};if($cmd -and $cmd.Source -notmatch "\\WindowsApps\\"){$p=$cmd.Source}};return $p
}
try{
 W "G17B1R4_WINDOWS_BUILD_START";W "SCOPE=CONTINUOUS_INDOOR_SAFETY_ENFORCEMENT";W "RUNS_SQL=False";W "MODIFIES_CLIENT=False";W "R5_CLIENT_STATE_MODIFIED=False"
 if(Get-Process worldserver -ErrorAction SilentlyContinue){throw "worldserver is running; stop it normally first"}
 foreach($d in @($SourceRoot,$BuildRoot,$RunDir)){if(-not(Test-Path -LiteralPath $d -PathType Container)){throw "directory missing: $d"}}
 foreach($f in @($Target,$Tool,$Solution,$Exe,$Pdb)){if(-not(Test-Path -LiteralPath $f -PathType Leaf)){throw "file missing: $f"}}
 $before=(Get-FileHash -LiteralPath $Target -Algorithm SHA256).Hash.ToLowerInvariant();W "SOURCE_SHA256_BEFORE=$before";if($before-cne $Pre -and $before-cne $Post){throw "dragonriding source is not locked B1R3 or B1R4 image"};$first=($before-ceq $Pre)
 $python=FindPython;if(-not $python){throw "Python312/Python310 not found; py.exe and WindowsApps aliases are not used"};W "PYTHON=$python"
 $com=$env:ComSpec;if(-not $com){throw "ComSpec missing"};$NativeSelfTestArgs=@('/d','/c','echo G17B1R4_NATIVE_STDOUT& echo G17B1R4_NATIVE_STDERR 1>&2& exit /b 0');$rc=Invoke-NativeLogged -FilePath $com -NativeArgs $NativeSelfTestArgs -Prefix 'NATIVE_SELFTEST';W "NATIVE_SELFTEST_EXIT=$rc";if($rc-ne 0){throw "native runner selftest failed"}
 $SourceApplyArgs=@($Tool,'apply','--source-root',$SourceRoot);$rc=Invoke-NativeLogged -FilePath $python -NativeArgs $SourceApplyArgs -Prefix 'SOURCE_APPLY';W "SOURCE_APPLY_EXIT=$rc";if($rc-ne 0){throw "source apply failed"}
 $after=(Get-FileHash -LiteralPath $Target -Algorithm SHA256).Hash.ToLowerInvariant();W "SOURCE_SHA256_AFTER=$after";if($after-cne $Post){throw "B1R4 postimage SHA mismatch"};W "G17B1R4_SOURCE_APPLY_GATE=PASS"
 $vswhere=Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe";if(-not(Test-Path -LiteralPath $vswhere -PathType Leaf)){throw "vswhere missing"}
 $msbuild=@(& $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe"|Where-Object{$_})[0];if(-not $msbuild){throw "MSBuild not found"};W "MSBUILD=$msbuild"
 $hits=@(Get-ChildItem -LiteralPath $BuildRoot -Filter *.vcxproj -File -Recurse|Select-String -SimpleMatch "cs_dragonriding.cpp");W "DRAGONRIDING_VCXPROJ_HITS=$($hits.Count)";if($hits.Count-lt 1){throw "cs_dragonriding.cpp absent from generated projects"}
 $be=Get-Item $Exe;$bp=Get-Item $Pdb;$BeforeExeUtc=$be.LastWriteTimeUtc;$BeforePdbUtc=$bp.LastWriteTimeUtc;$beh=(Get-FileHash $Exe -Algorithm SHA256).Hash.ToLowerInvariant();W "BEFORE_EXE_SHA256=$beh";W "BEFORE_EXE_UTC=$($BeforeExeUtc.ToString('o'))";W "BEFORE_PDB_UTC=$($BeforePdbUtc.ToString('o'))"
 [IO.File]::SetLastWriteTimeUtc($Target,[DateTime]::UtcNow);Start-Sleep -Milliseconds 150;$start=[DateTime]::UtcNow;W "BUILD_START_UTC=$($start.ToString('o'))"
 $MSBuildArgs=@($Solution,'/m','/t:worldserver','/p:Configuration=RelWithDebInfo','/p:Platform=x64','/verbosity:minimal');$rc=Invoke-NativeLogged -FilePath $msbuild -NativeArgs $MSBuildArgs -Prefix 'MSBUILD';W "MSBUILD_EXIT=$rc";if($rc-ne 0){throw "MSBuild failed"}
 $objs=@(Get-ChildItem -LiteralPath $BuildRoot -File -Recurse -Filter '*dragonriding*.obj'|Where-Object{$_.LastWriteTimeUtc-ge $start});W "DRAGONRIDING_FRESH_OBJECTS=$($objs.Count)";foreach($o in $objs){W ("FRESH_OBJECT="+$o.FullName+";size="+$o.Length)};if($objs.Count-lt 1){throw "fresh dragonriding object not proven"}
 $ae=Get-Item $Exe;$ap=Get-Item $Pdb;$AfterExeUtc=$ae.LastWriteTimeUtc;$AfterPdbUtc=$ap.LastWriteTimeUtc;$aeh=(Get-FileHash $Exe -Algorithm SHA256).Hash.ToLowerInvariant();W "AFTER_EXE_SHA256=$aeh";W "AFTER_EXE_SIZE=$($ae.Length)";W "AFTER_EXE_UTC=$($AfterExeUtc.ToString('o'))";W "AFTER_PDB_UTC=$($AfterPdbUtc.ToString('o'))"
 if($AfterExeUtc-le $BeforeExeUtc -or $AfterPdbUtc-le $BeforePdbUtc){throw "exe/pdb timestamp did not advance"};if($AfterExeUtc-lt $start -or $AfterPdbUtc-lt $start){throw "exe/pdb predates build start"};if($first -and $aeh-ceq $beh){throw "first B1R4 build did not change worldserver.exe SHA"}
 W "G17B1R4_WINDOWS_BUILD_RESULT=PASS";W "STOP_DO_NOT_RUN_SQL";W "NEXT=Start worldserver normally; enter the same real indoor location and verify cleanup";W "RESULT_FILE=$Result";exit 0
}catch{W ("G17B1R4_WINDOWS_BUILD_ERROR="+$_.Exception.Message);W "G17B1R4_WINDOWS_BUILD_RESULT=FAIL";W "STOP_DO_NOT_START_WORLDSERVER";W "RESULT_FILE=$Result";exit 1}
