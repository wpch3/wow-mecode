#requires -Version 5.1
param([string]$Workspace="C:\Users\Administrator\Downloads\workspace",[string]$ClientRoot="D:\WOW")
$ErrorActionPreference="Stop"
$UploadDir=Join-Path $Workspace "uploads";$StateFile=Join-Path $UploadDir "G17R5_LOCALE_MIRROR_STATE.txt";$Result=Join-Path $UploadDir "G17R5_LOCALE_MIRROR_ROLLBACK_RESULT.txt"
$Utf8NoBom=New-Object System.Text.UTF8Encoding($false);New-Item -ItemType Directory -Path $UploadDir -Force|Out-Null;[System.IO.File]::WriteAllText($Result,"",$Utf8NoBom)
function Write-Result([string]$Line){Write-Host $Line;[System.IO.File]::AppendAllText($Result,$Line+[Environment]::NewLine,$Utf8NoBom)}
function Read-KeyValueFile([string]$Path){$V=@{};foreach($L in [System.IO.File]::ReadAllLines($Path)){if($L -match '^([^=]+)=(.*)$'){$V[$Matches[1]]=$Matches[2]}};return $V}
$Target="";$Rescue="";$Moved=$false;$StateMoved=$false
try{
 Write-Result "G17R5_LOCALE_MIRROR_ROLLBACK_START"
 if(Get-Process -ErrorAction SilentlyContinue|Where-Object{$_.Name -match '^(?i:wow|wow-64)$'}){throw "Wow client is running; close it first"}
 if(-not (Test-Path -LiteralPath $StateFile -PathType Leaf)){throw "R5 state missing"}
 if(-not (Test-Path -LiteralPath $ClientRoot -PathType Container)){throw "client root missing"};$ClientRoot=(Resolve-Path -LiteralPath $ClientRoot).Path
 $S=Read-KeyValueFile $StateFile
 if($S["INSTALL_STATUS"] -cne "PASS" -or $S["CLIENT_ROOT"] -ine $ClientRoot -or $S["TARGET_PREIMAGE"] -cne "ABSENT"){throw "R5 state ownership mismatch"}
 $Target=$S["TARGET_LOCALE_MPQ"]
 if(-not (Test-Path -LiteralPath $Target -PathType Leaf)){throw "owned R5 locale MPQ missing"}
 $Hash=(Get-FileHash -LiteralPath $Target -Algorithm SHA256).Hash.ToLowerInvariant();if($Hash -cne $S["TARGET_MPQ_SHA256"]){throw "owned R5 locale MPQ changed; refusing removal"}
 $Rescue=Join-Path $UploadDir ("G17R5_ROLLBACK_RESCUE_"+(Get-Date -Format "yyyyMMdd_HHmmss")+".MPQ");if(Test-Path -LiteralPath $Rescue){throw "rescue path exists"}
 Move-Item -LiteralPath $Target -Destination $Rescue;$Moved=$true
 $StateArchive=$StateFile+".rolled_back_"+(Get-Date -Format "yyyyMMdd_HHmmss");Move-Item -LiteralPath $StateFile -Destination $StateArchive;$StateMoved=$true
 $Cache=Join-Path $ClientRoot "Cache";if(Test-Path -LiteralPath $Cache -PathType Container){Remove-Item -LiteralPath $Cache -Recurse -Force;Write-Result "CLIENT_CACHE_REMOVED=True"}else{Write-Result "CLIENT_CACHE_REMOVED=False"}
 Write-Result "TARGET_LOCALE_MPQ_REMOVED=True";Write-Result "ROOT_R4_MPQ_MODIFIED=False";Write-Result "SERVER_MODIFIED=False";Write-Result "RESCUE_COPY=$Rescue";Write-Result "G17R5_LOCALE_MIRROR_ROLLBACK_RESULT=PASS";Write-Result "RESULT_FILE=$Result";exit 0
}catch{
 if($Moved -and -not $StateMoved -and $Rescue -and (Test-Path -LiteralPath $Rescue -PathType Leaf) -and -not (Test-Path -LiteralPath $Target)){try{Move-Item -LiteralPath $Rescue -Destination $Target;Write-Result "ROLLBACK_TRANSACTION_RECOVERY=PASS"}catch{Write-Result ("ROLLBACK_TRANSACTION_RECOVERY_ERROR="+$_.Exception.Message)}}
 Write-Result ("G17R5_LOCALE_MIRROR_ROLLBACK_ERROR="+$_.Exception.Message);Write-Result "G17R5_LOCALE_MIRROR_ROLLBACK_RESULT=FAIL";Write-Result "RESULT_FILE=$Result";exit 1
}
