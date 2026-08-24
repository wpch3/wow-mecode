#requires -Version 5.1
$ErrorActionPreference = "Stop"
$Workspace = "C:\Users\Administrator\Downloads\workspace"
$UploadDir = Join-Path $Workspace "uploads"
$SourceRoot = "D:\TrinityCore"
$RunDir = "D:\TC-Build\bin\RelWithDebInfo"
$StateFile = Join-Path $UploadDir "G17R3_SERVER_BUILD_STATE.txt"
$Result = Join-Path $UploadDir "G17R3_SERVER_ROLLBACK_RESULT.txt"
$Installer = Join-Path $PSScriptRoot "tools\apply_g17r3_server_source.py"
$Exe = Join-Path $RunDir "worldserver.exe"
$Pdb = Join-Path $RunDir "worldserver.pdb"
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($Result,"",$Utf8NoBom)
function Write-Result([string]$Line) { Write-Host $Line; [System.IO.File]::AppendAllText($Result,$Line+[Environment]::NewLine,$Utf8NoBom) }
function Read-KeyValueFile([string]$Path) {
    $Values=@{}
    foreach($Line in [System.IO.File]::ReadAllLines($Path)) { if($Line -match '^([^=]+)=(.*)$') { $Values[$Matches[1]]=$Matches[2] } }
    return $Values
}
function Invoke-NativeLogged([string]$FilePath,[string[]]$NativeArgs,[string]$Prefix) {
    $Saved=$ErrorActionPreference;$Exit=9009;$Output=@()
    try { $ErrorActionPreference="Continue";$Output=@(& $FilePath @NativeArgs 2>&1);$Exit=$LASTEXITCODE }
    finally { $ErrorActionPreference=$Saved }
    foreach($Line in $Output) { Write-Result ($Prefix+"|"+$Line.ToString()) }
    return [int]$Exit
}
try {
    Write-Result "G17R3_SERVER_ROLLBACK_START"
    if(Get-Process worldserver -ErrorAction SilentlyContinue) { throw "worldserver is running" }
    if(-not(Test-Path -LiteralPath $StateFile -PathType Leaf)) { throw "build state missing" }
    $State=Read-KeyValueFile $StateFile
    if($State["BUILD_STATUS"] -cne "PASS") { throw "build state is not PASS" }
    foreach($Key in @("BACKUP_EXE","BACKUP_EXE_SHA256","BACKUP_PDB","BACKUP_PDB_SHA256","NEW_EXE_SHA256","NEW_PDB_SHA256")) {
        if(-not $State.ContainsKey($Key)) { throw "state key missing: $Key" }
    }
    foreach($Path in @($Exe,$Pdb,$State["BACKUP_EXE"],$State["BACKUP_PDB"],$Installer)) {
        if(-not(Test-Path -LiteralPath $Path -PathType Leaf)) { throw "required rollback file missing: $Path" }
    }
    if((Get-FileHash -LiteralPath $Exe -Algorithm SHA256).Hash.ToLowerInvariant() -cne $State["NEW_EXE_SHA256"]) { throw "current EXE changed" }
    if((Get-FileHash -LiteralPath $Pdb -Algorithm SHA256).Hash.ToLowerInvariant() -cne $State["NEW_PDB_SHA256"]) { throw "current PDB changed" }
    if((Get-FileHash -LiteralPath $State["BACKUP_EXE"] -Algorithm SHA256).Hash.ToLowerInvariant() -cne $State["BACKUP_EXE_SHA256"]) { throw "backup EXE changed" }
    if((Get-FileHash -LiteralPath $State["BACKUP_PDB"] -Algorithm SHA256).Hash.ToLowerInvariant() -cne $State["BACKUP_PDB_SHA256"]) { throw "backup PDB changed" }

    $Candidates=@(
        (Join-Path $env:LOCALAPPDATA "Programs\Python\Python312\python.exe"),
        (Join-Path $env:LOCALAPPDATA "Programs\Python\Python310\python.exe")
    )
    $Python=@($Candidates|Where-Object{Test-Path -LiteralPath $_ -PathType Leaf})[0]
    if(-not $Python) {
        $Command=Get-Command python.exe -ErrorAction SilentlyContinue
        if(-not $Command) { $Command=Get-Command python -ErrorAction SilentlyContinue }
        if($Command -and $Command.Source -notmatch "\\WindowsApps\\") { $Python=$Command.Source }
    }
    if(-not $Python) { throw "Python312/310 not found" }

    $Stamp=Get-Date -Format "yyyyMMdd_HHmmss"
    $Dir=Join-Path $UploadDir ("G17R3_Server_Rollback_"+$Stamp)
    if(Test-Path -LiteralPath $Dir) { throw "rollback directory already exists" }
    New-Item -ItemType Directory -Path $Dir|Out-Null
    $RescueExe=Join-Path $Dir "removed_g17r3_worldserver.exe"
    $RescuePdb=Join-Path $Dir "removed_g17r3_worldserver.pdb"
    Copy-Item -LiteralPath $Exe -Destination $RescueExe
    Copy-Item -LiteralPath $Pdb -Destination $RescuePdb
    if((Get-FileHash -LiteralPath $RescueExe -Algorithm SHA256).Hash.ToLowerInvariant() -cne $State["NEW_EXE_SHA256"] -or
       (Get-FileHash -LiteralPath $RescuePdb -Algorithm SHA256).Hash.ToLowerInvariant() -cne $State["NEW_PDB_SHA256"]) {
        throw "R3 binary rescue verification failed"
    }

    $SourceRolledBack=$false
    try {
        $RollbackExit=Invoke-NativeLogged -FilePath $Python -NativeArgs @($Installer,"rollback","--source-root",$SourceRoot) -Prefix "SOURCE_ROLLBACK"
        if($RollbackExit -ne 0) { throw "source rollback failed" }
        $SourceRolledBack=$true
        Copy-Item -LiteralPath $State["BACKUP_EXE"] -Destination $Exe -Force
        Copy-Item -LiteralPath $State["BACKUP_PDB"] -Destination $Pdb -Force
        if((Get-FileHash -LiteralPath $Exe -Algorithm SHA256).Hash.ToLowerInvariant() -cne $State["BACKUP_EXE_SHA256"]) { throw "restored EXE mismatch" }
        if((Get-FileHash -LiteralPath $Pdb -Algorithm SHA256).Hash.ToLowerInvariant() -cne $State["BACKUP_PDB_SHA256"]) { throw "restored PDB mismatch" }
        Move-Item -LiteralPath $StateFile -Destination (Join-Path $Dir "G17R3_SERVER_BUILD_STATE.txt")
    }
    catch {
        $SwapError=$_.Exception.Message
        $RecoveryErrors=@()
        try {
            Copy-Item -LiteralPath $RescueExe -Destination $Exe -Force
            Copy-Item -LiteralPath $RescuePdb -Destination $Pdb -Force
            if((Get-FileHash -LiteralPath $Exe -Algorithm SHA256).Hash.ToLowerInvariant() -cne $State["NEW_EXE_SHA256"] -or
               (Get-FileHash -LiteralPath $Pdb -Algorithm SHA256).Hash.ToLowerInvariant() -cne $State["NEW_PDB_SHA256"]) {
                throw "R3 binary recovery hash mismatch"
            }
            Write-Result "AUTO_RECOVER_R3_BINARIES=PASS"
        }
        catch { $RecoveryErrors += ("binary recovery: "+$_.Exception.Message) }
        if($SourceRolledBack) {
            try {
                $ReapplyExit=Invoke-NativeLogged -FilePath $Python -NativeArgs @($Installer,"apply","--source-root",$SourceRoot) -Prefix "SOURCE_REAPPLY"
                if($ReapplyExit -ne 0) { throw "source reapply exit=$ReapplyExit" }
                Write-Result "AUTO_RECOVER_R3_SOURCE=PASS"
            }
            catch { $RecoveryErrors += ("source recovery: "+$_.Exception.Message) }
        }
        $RecoveryText=if($RecoveryErrors.Count){$RecoveryErrors -join "; "}else{"PASS"}
        throw "rollback transaction failed: $SwapError; recovery=$RecoveryText"
    }
    Write-Result "RESTORED_R2_EXE_SHA256=$($State["BACKUP_EXE_SHA256"])"
    Write-Result "RESTORED_R2_PDB_SHA256=$($State["BACKUP_PDB_SHA256"])"
    Write-Result "G17R3_SERVER_ROLLBACK_RESULT=PASS"
    Write-Result ("RESULT_FILE="+$Result)
    exit 0
}
catch {
    Write-Result ("G17R3_SERVER_ROLLBACK_ERROR="+$_.Exception.Message)
    Write-Result "G17R3_SERVER_ROLLBACK_RESULT=FAIL"
    Write-Result ("RESULT_FILE="+$Result)
    exit 1
}
