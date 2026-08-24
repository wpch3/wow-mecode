#requires -Version 5.1
param([string]$Workspace="C:\Users\Administrator\Downloads\workspace",[string]$SourceRoot="D:\TrinityCore",[string]$BuildRoot="D:\TC-Build")
$ErrorActionPreference='Stop';$U=Join-Path $Workspace 'uploads';$R=Join-Path $U 'G17B2_ROLLBACK_BUILD_RESULT.txt';$E=New-Object Text.UTF8Encoding($false);New-Item -ItemType Directory $U -Force|Out-Null;[IO.File]::WriteAllText($R,'',$E)
function W([string]$x){Write-Host $x;[IO.File]::AppendAllText($R,$x+[Environment]::NewLine,$E)}
function Invoke-NativeLogged{param([string]$FilePath,[string[]]$NativeArgs,[string]$Prefix)$old=$ErrorActionPreference;$o=@();$rc=9009;try{$ErrorActionPreference='Continue';$o=@(& $FilePath @NativeArgs 2>&1);$rc=$LASTEXITCODE}finally{$ErrorActionPreference=$old};foreach($l in $o){W ($Prefix+'|'+$l)};return [int]$rc}
try{
 W 'G17B2_ROLLBACK_BUILD_START';if(Get-Process worldserver -ErrorAction SilentlyContinue){throw 'stop worldserver first'}
 $t=Join-Path $SourceRoot 'src\server\scripts\Commands\cs_dragonriding.cpp';$tool=Join-Path $PSScriptRoot 'tools\apply_g17b2_source.py';$sol=Join-Path $BuildRoot 'TrinityCore.sln';$exe=Join-Path $BuildRoot 'bin\RelWithDebInfo\worldserver.exe';$pdb=Join-Path $BuildRoot 'bin\RelWithDebInfo\worldserver.pdb';foreach($f in @($t,$tool,$sol,$exe,$pdb)){if(-not(Test-Path $f -PathType Leaf)){throw "missing: $f"}}
 $c=@((Join-Path $env:LOCALAPPDATA 'Programs\Python\Python312\python.exe'),(Join-Path $env:LOCALAPPDATA 'Programs\Python\Python310\python.exe'));$py=@($c|Where-Object{Test-Path $_ -PathType Leaf})[0];if(-not $py){throw 'Python312/310 missing'}
 $a=@($tool,'rollback','--source-root',$SourceRoot);$rc=Invoke-NativeLogged -FilePath $py -NativeArgs $a -Prefix 'SOURCE_ROLLBACK';if($rc-ne 0){throw 'source rollback failed'}
 $pre='35af002b09b5d8112bbc1aaa1750f4a6245adec8b7c91a7852d69bdd283668b8';if((Get-FileHash $t -Algorithm SHA256).Hash.ToLowerInvariant()-cne $pre){throw 'B1R5 preimage not restored'}
 $vw=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe';$ms=@(& $vw -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe'|Where-Object{$_})[0];if(-not $ms){throw 'MSBuild missing'}
 $BeforeExeUtc=(Get-Item $exe).LastWriteTimeUtc;$BeforePdbUtc=(Get-Item $pdb).LastWriteTimeUtc;[IO.File]::SetLastWriteTimeUtc($t,[DateTime]::UtcNow);Start-Sleep -Milliseconds 150;$a=@($sol,'/m','/t:worldserver','/p:Configuration=RelWithDebInfo','/p:Platform=x64','/verbosity:minimal');$rc=Invoke-NativeLogged -FilePath $ms -NativeArgs $a -Prefix 'MSBUILD';if($rc-ne 0){throw 'rollback MSBuild failed'};if((Get-Item $exe).LastWriteTimeUtc-le $BeforeExeUtc -or (Get-Item $pdb).LastWriteTimeUtc-le $BeforePdbUtc){throw 'rollback exe/pdb not updated'}
 W 'G17B2_SOURCE_ROLLBACK=PASS';W 'G17B2_ROLLBACK_BUILD_RESULT=PASS';W ('RESULT_FILE='+$R);exit 0
}catch{W ('G17B2_ROLLBACK_ERROR='+$_.Exception.Message);W 'G17B2_ROLLBACK_BUILD_RESULT=FAIL';W 'STOP_DO_NOT_START_WORLDSERVER';exit 1}
