#requires -Version 5.1
$ErrorActionPreference = "Stop"
$Workspace = "C:\Users\Administrator\Downloads\workspace"
$UploadDir = Join-Path $Workspace "uploads"
$StateFile = Join-Path $UploadDir "G17R3_CLIENT_MPQ_UPGRADE_STATE.txt"
$Result = Join-Path $UploadDir "G17R3_CLIENT_MPQ_ROLLBACK_RESULT.txt"
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
New-Item -ItemType Directory -Path $UploadDir -Force | Out-Null
[System.IO.File]::WriteAllText($Result,"",$Utf8NoBom)
function Write-Result([string]$Line) { Write-Host $Line; [System.IO.File]::AppendAllText($Result,$Line+[Environment]::NewLine,$Utf8NoBom) }
function Read-KeyValueFile([string]$Path) { $V=@{}; foreach($L in [System.IO.File]::ReadAllLines($Path)){if($L -match '^([^=]+)=(.*)$'){$V[$Matches[1]]=$Matches[2]}}; return $V }
try {
    Write-Result "G17R3_CLIENT_MPQ_ROLLBACK_START"
    if (Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Name -match '^(?i:wow|wow-64)$' }) { throw "Wow client is running" }
    if (-not (Test-Path -LiteralPath $StateFile -PathType Leaf)) { throw "R3 state missing: $StateFile" }
    $State=Read-KeyValueFile $StateFile
    foreach($Key in @("INSTALL_STATUS","CLIENT_ROOT","INSTALLED_MPQ","OLD_MPQ_SHA256","NEW_MPQ_SHA256","BACKUP_MPQ")){if(-not $State.ContainsKey($Key)){throw "state key missing: $Key"}}
    if($State["INSTALL_STATUS"] -cne "PASS"){throw "state is not PASS"}
    $Target=$State["INSTALLED_MPQ"]; $Backup=$State["BACKUP_MPQ"]
    if(-not (Test-Path -LiteralPath $Target -PathType Leaf)){throw "current R3 MPQ missing"}
    if(-not (Test-Path -LiteralPath $Backup -PathType Leaf)){throw "verified R1 backup MPQ missing"}
    $CurrentHash=(Get-FileHash -LiteralPath $Target -Algorithm SHA256).Hash.ToLowerInvariant()
    $BackupHash=(Get-FileHash -LiteralPath $Backup -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-Result "CURRENT_MPQ_SHA256=$CurrentHash"; Write-Result "BACKUP_MPQ_SHA256=$BackupHash"
    if($CurrentHash -cne $State["NEW_MPQ_SHA256"].ToLowerInvariant()){throw "current R3 MPQ hash changed"}
    if($BackupHash -cne $State["OLD_MPQ_SHA256"].ToLowerInvariant()){throw "R1 backup MPQ hash changed"}
    $Stamp=Get-Date -Format "yyyyMMdd_HHmmss"; $RollbackDir=Join-Path $UploadDir ("G17R3_Client_Rollback_"+$Stamp)
    if(Test-Path -LiteralPath $RollbackDir){throw "rollback directory exists"};New-Item -ItemType Directory -Path $RollbackDir|Out-Null
    $Rescue=Join-Path $RollbackDir ("removed_G17R3_"+(Split-Path -Leaf $Target)); Move-Item -LiteralPath $Target -Destination $Rescue
    $Temp=$Target+".g17r3.rollback.tmp"
    try {
        Copy-Item -LiteralPath $Backup -Destination $Temp
        if((Get-FileHash -LiteralPath $Temp -Algorithm SHA256).Hash.ToLowerInvariant() -cne $BackupHash){throw "rollback temp hash mismatch"}
        Move-Item -LiteralPath $Temp -Destination $Target
        if((Get-FileHash -LiteralPath $Target -Algorithm SHA256).Hash.ToLowerInvariant() -cne $BackupHash){throw "restored R1 MPQ hash mismatch"}
    }
    catch {
        if(Test-Path -LiteralPath $Temp){Remove-Item -LiteralPath $Temp -Force -ErrorAction SilentlyContinue}
        if(-not (Test-Path -LiteralPath $Target) -and (Test-Path -LiteralPath $Rescue)){Move-Item -LiteralPath $Rescue -Destination $Target}
        throw
    }
    Move-Item -LiteralPath $StateFile -Destination (Join-Path $RollbackDir "G17R3_CLIENT_MPQ_UPGRADE_STATE.txt")
    Write-Result "RESTORED_R1_MPQ_SHA256=$BackupHash"
    Write-Result "R3_REMOVED_MPQ_RESCUE=$Rescue"
    Write-Result "G17R3_CLIENT_MPQ_ROLLBACK_RESULT=PASS"
    Write-Result "RESULT_FILE=$Result"
    exit 0
}
catch { Write-Result ("G17R3_CLIENT_MPQ_ROLLBACK_ERROR="+$_.Exception.Message);Write-Result "G17R3_CLIENT_MPQ_ROLLBACK_RESULT=FAIL";Write-Result "RESULT_FILE=$Result";exit 1 }
