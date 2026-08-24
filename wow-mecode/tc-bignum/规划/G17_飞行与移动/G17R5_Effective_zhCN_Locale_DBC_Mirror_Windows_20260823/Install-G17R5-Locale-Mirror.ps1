#requires -Version 5.1
param(
    [string]$Workspace = "C:\Users\Administrator\Downloads\workspace",
    [string]$ClientRoot = "D:\WOW",
    [string]$MpqCliOverride = ""
)
$ErrorActionPreference = "Stop"
$UploadDir = Join-Path $Workspace "uploads"
$R4StateFile = Join-Path $UploadDir "G17R4_CLIENT_MPQ_UPGRADE_STATE.txt"
$R5StateFile = Join-Path $UploadDir "G17R5_LOCALE_MIRROR_STATE.txt"
$Result = Join-Path $UploadDir "G17R5_LOCALE_MIRROR_RESULT.txt"
$Tool = if ($MpqCliOverride) { $MpqCliOverride } else { Join-Path $PSScriptRoot "third_party\mpqcli-windows-amd64.exe" }
$SpellTarget = "DBFilesClient\Spell.dbc"
$AreaTarget = "DBFilesClient\AreaTable.dbc"
$ExpectedToolHash = "5dc56b130432098d18e911fdf4b3464adf850432b7fbb35003a2feff7c30a79f"
$ExpectedLinuxTestToolHash = "d2f97ee5b5a7473d8318238d3fc7238a76c35727c4e0b55275516b6ad325b2e7"
$ExpectedSpellHash = "dd25091167f671764735ce88c78b66485c6d661fadf05d322574c261f6e464ea"
$ExpectedSpellSize = 48956359
$ExpectedAreaHash = "1acef997a27f844a8abee9b477c44f2097f745168c6df85ece6b1a135568c233"
$ExpectedAreaSize = 362740
$LocaleSlot = "Y"
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$WorkRoot = ""
$Target = ""
$TemporaryTarget = ""
$SourceArchiveHash = ""
$StateCommitted = $false

New-Item -ItemType Directory -Path $UploadDir -Force | Out-Null
[System.IO.File]::WriteAllText($Result,"",$Utf8NoBom)
function Write-Result([string]$Line) { Write-Host $Line; [System.IO.File]::AppendAllText($Result,$Line+[Environment]::NewLine,$Utf8NoBom) }
function Read-KeyValueFile([string]$Path) {
    $Values=@{}
    if(-not (Test-Path -LiteralPath $Path -PathType Leaf)){return $Values}
    foreach($Line in [System.IO.File]::ReadAllLines($Path)){if($Line -match '^([^=]+)=(.*)$'){$Values[$Matches[1]]=$Matches[2]}}
    return $Values
}
function Invoke-NativeCapture {
    param([string]$FilePath,[string[]]$NativeArgs)
    $Saved=$ErrorActionPreference;$Exit=9009;$Output=@()
    try{$ErrorActionPreference="Continue";$Output=@(& $FilePath @NativeArgs 2>&1);$Exit=$LASTEXITCODE}
    finally{$ErrorActionPreference=$Saved}
    return [pscustomobject]@{ExitCode=[int]$Exit;Lines=@($Output)}
}
function Extract-ArchiveTarget([string]$Archive,[string]$ArchiveTarget,[string]$Tag) {
    $Root=Join-Path $WorkRoot ("extract_"+$Tag+"_"+[Guid]::NewGuid().ToString("N"));New-Item -ItemType Directory -Path $Root|Out-Null
    $Native=Invoke-NativeCapture -FilePath $Tool -NativeArgs @("extract","--output",$Root,"--keep","--file",$ArchiveTarget,$Archive)
    $Extracted=Join-Path $Root $ArchiveTarget
    if($Native.ExitCode -eq 0 -and (Test-Path -LiteralPath $Extracted -PathType Leaf)){
        return [pscustomobject]@{State="HIT";Hash=(Get-FileHash -LiteralPath $Extracted -Algorithm SHA256).Hash.ToLowerInvariant();Size=[int64](Get-Item -LiteralPath $Extracted).Length;Path=$Extracted;Detail=""}
    }
    $Text=(($Native.Lines|ForEach-Object{$_.ToString()}) -join " | ")
    if($Native.ExitCode -ne 0 -and -not (Test-Path -LiteralPath $Extracted) -and ($Text.Contains("File doesn't exist") -or $Text.Contains("does not exist"))){
        return [pscustomobject]@{State="NO_HIT";Hash="";Size=0;Path="";Detail=""}
    }
    if($Text.Length -gt 600){$Text=$Text.Substring(0,600)}
    return [pscustomobject]@{State="ERROR";Hash="";Size=0;Path="";Detail=$Text}
}
function Assert-R4Archive([string]$Archive,[string]$Prefix) {
    $Format=Invoke-NativeCapture -FilePath $Tool -NativeArgs @("info","--property","format-version",$Archive)
    $Count=Invoke-NativeCapture -FilePath $Tool -NativeArgs @("info","--property","file-count",$Archive)
    if($Format.ExitCode -ne 0 -or $Count.ExitCode -ne 0){throw "MPQ info probe failed: $Archive"}
    $FormatText=(($Format.Lines|ForEach-Object{$_.ToString().Trim()}) -join "")
    $CountText=(($Count.Lines|ForEach-Object{$_.ToString().Trim()}) -join "")
    Write-Result ($Prefix+"_FORMAT_VERSION="+$FormatText);Write-Result ($Prefix+"_FILE_COUNT="+$CountText)
    if($FormatText -ne "2" -or $CountText -ne "4"){throw "unexpected R4 MPQ structure"}
    $Spell=Extract-ArchiveTarget -Archive $Archive -ArchiveTarget $SpellTarget -Tag ($Prefix+"_spell")
    $Area=Extract-ArchiveTarget -Archive $Archive -ArchiveTarget $AreaTarget -Tag ($Prefix+"_area")
    Write-Result ($Prefix+"_SPELL_SHA256="+$Spell.Hash);Write-Result ($Prefix+"_AREA_SHA256="+$Area.Hash)
    if($Spell.State -ne "HIT" -or $Spell.Hash -cne $ExpectedSpellHash -or $Spell.Size -ne $ExpectedSpellSize){throw "R4 Spell.dbc mismatch"}
    if($Area.State -ne "HIT" -or $Area.Hash -cne $ExpectedAreaHash -or $Area.Size -ne $ExpectedAreaSize){throw "R4 AreaTable.dbc mismatch"}
}

try {
    Write-Result "G17R5_LOCALE_MIRROR_START"
    Write-Result "SCOPE=CLIENT_LOCALE_ARCHIVE_ONLY"
    Write-Result "SERVER_WRITE_ATTEMPTED=False"
    Write-Result "ROOT_R4_MPQ_WRITE_ATTEMPTED=False"
    if(Get-Process -ErrorAction SilentlyContinue|Where-Object{$_.Name -match '^(?i:wow|wow-64)$'}){throw "Wow client is running; close it first"}
    foreach($Required in @($Tool,$R4StateFile)){if(-not (Test-Path -LiteralPath $Required -PathType Leaf)){throw "required file missing: $Required"}}
    if(-not (Test-Path -LiteralPath $ClientRoot -PathType Container)){throw "client root missing"}
    $ClientRoot=(Resolve-Path -LiteralPath $ClientRoot).Path
    $DataDir=Join-Path $ClientRoot "Data";$LocaleDir=Join-Path $DataDir "zhCN"
    if(-not (Test-Path -LiteralPath (Join-Path $ClientRoot "Wow.exe") -PathType Leaf) -or -not (Test-Path -LiteralPath $LocaleDir -PathType Container)){throw "invalid zhCN client root"}
    $Target=Join-Path $LocaleDir ("patch-zhCN-"+$LocaleSlot+".MPQ")
    $ToolHash=(Get-FileHash -LiteralPath $Tool -Algorithm SHA256).Hash.ToLowerInvariant();Write-Result "MPQCLI_SHA256=$ToolHash"
    if(-not (($ToolHash -ceq $ExpectedToolHash) -or ($MpqCliOverride -and $ToolHash -ceq $ExpectedLinuxTestToolHash))){throw "mpqcli hash mismatch"}
    Write-Result ("MPQCLI_OVERRIDE="+[bool]$MpqCliOverride)
    $WorkRoot=Join-Path $UploadDir ("G17R5_Locale_Work_"+(Get-Date -Format "yyyyMMdd_HHmmss")+"_"+$PID);New-Item -ItemType Directory -Path $WorkRoot|Out-Null

    if(Test-Path -LiteralPath $R5StateFile -PathType Leaf){
        $R5=Read-KeyValueFile $R5StateFile
        if($R5["INSTALL_STATUS"] -cne "PASS" -or $R5["CLIENT_ROOT"] -ine $ClientRoot -or $R5["TARGET_LOCALE_MPQ"] -ine $Target){throw "existing R5 state ownership mismatch"}
        if(-not (Test-Path -LiteralPath $Target -PathType Leaf)){throw "R5 state exists but locale MPQ is missing"}
        $Current=(Get-FileHash -LiteralPath $Target -Algorithm SHA256).Hash.ToLowerInvariant()
        if($Current -cne $R5["TARGET_MPQ_SHA256"]){throw "R5 locale MPQ changed"}
        Assert-R4Archive -Archive $Target -Prefix "ALREADY_CURRENT"
        Write-Result "G17R5_LOCALE_MIRROR=ALREADY_CURRENT";Write-Result "G17R5_LOCALE_MIRROR_RESULT=PASS";Write-Result "RESULT_FILE=$Result";exit 0
    }

    $R4=Read-KeyValueFile $R4StateFile
    if($R4["INSTALL_STATUS"] -cne "PASS" -or $R4["CLIENT_ROOT"] -ine $ClientRoot){throw "R4 prerequisite state is not PASS for this client"}
    $Source=$R4["INSTALLED_MPQ"]
    if(-not (Test-Path -LiteralPath $Source -PathType Leaf)){throw "R4 root MPQ missing"}
    $SourceArchiveHash=(Get-FileHash -LiteralPath $Source -Algorithm SHA256).Hash.ToLowerInvariant()
    if($SourceArchiveHash -cne $R4["NEW_MPQ_SHA256"]){throw "R4 root MPQ hash no longer matches state"}
    Write-Result "SOURCE_ROOT_MPQ=$Source";Write-Result "SOURCE_ROOT_MPQ_SHA256=$SourceArchiveHash"
    Assert-R4Archive -Archive $Source -Prefix "SOURCE_R4_MPQ"
    if(Test-Path -LiteralPath $Target){throw "target locale slot Y is no longer absent; refusing overwrite"}

    $Slots=@("Z","X","W","V","U","T","S","R","Q","P","O","N","M","L","K","J","I","H","G","F","E","D","C","B","A")
    $Collisions=0
    foreach($Slot in $Slots){
        $Candidate=Join-Path $LocaleDir ("patch-zhCN-"+$Slot+".MPQ")
        if(-not (Test-Path -LiteralPath $Candidate)){continue}
        if(Test-Path -LiteralPath $Candidate -PathType Container){
            $AreaHit=[int](Test-Path -LiteralPath (Join-Path $Candidate $AreaTarget) -PathType Leaf);$SpellHit=[int](Test-Path -LiteralPath (Join-Path $Candidate $SpellTarget) -PathType Leaf)
            Write-Result "LOCALE_SCAN=SLOT=$Slot;TYPE=DIRECTORY;AREA_HIT=$AreaHit;SPELL_HIT=$SpellHit;PATH=$Candidate"
            if($AreaHit -or $SpellHit){$Collisions++}
        } else {
            $AreaProbe=Extract-ArchiveTarget -Archive $Candidate -ArchiveTarget $AreaTarget -Tag ("scan_area_"+$Slot)
            $SpellProbe=Extract-ArchiveTarget -Archive $Candidate -ArchiveTarget $SpellTarget -Tag ("scan_spell_"+$Slot)
            Write-Result "LOCALE_SCAN=SLOT=$Slot;TYPE=PACKED_MPQ;AREA=$($AreaProbe.State);SPELL=$($SpellProbe.State);PATH=$Candidate"
            if($AreaProbe.State -eq "ERROR" -or $SpellProbe.State -eq "ERROR"){throw "cannot safely inspect locale slot $Slot"}
            if($AreaProbe.State -eq "HIT" -or $SpellProbe.State -eq "HIT"){$Collisions++}
        }
    }
    Write-Result "OTHER_LOCALE_DBC_COLLISIONS=$Collisions"
    if($Collisions -ne 0){throw "another locale custom MPQ owns Spell/Area; refusing ambiguous override"}

    $TemporaryTarget=$Target+".g17r5.new.tmp";if(Test-Path -LiteralPath $TemporaryTarget){throw "temporary target already exists"}
    Copy-Item -LiteralPath $Source -Destination $TemporaryTarget
    if((Get-FileHash -LiteralPath $TemporaryTarget -Algorithm SHA256).Hash.ToLowerInvariant() -cne $SourceArchiveHash){throw "locale mirror temp hash mismatch"}
    Move-Item -LiteralPath $TemporaryTarget -Destination $Target
    if((Get-FileHash -LiteralPath $Target -Algorithm SHA256).Hash.ToLowerInvariant() -cne $SourceArchiveHash){throw "locale mirror installed hash mismatch"}
    Assert-R4Archive -Archive $Target -Prefix "INSTALLED_LOCALE_MPQ"
    $CacheDir=Join-Path $ClientRoot "Cache";if(Test-Path -LiteralPath $CacheDir -PathType Container){Remove-Item -LiteralPath $CacheDir -Recurse -Force;Write-Result "CLIENT_CACHE_REMOVED=True"}else{Write-Result "CLIENT_CACHE_REMOVED=False"}
    $StateLines=@(
        "STATE_FORMAT=1","INSTALL_STATUS=PASS",("CLIENT_ROOT="+$ClientRoot),("SOURCE_ROOT_MPQ="+$Source),("SOURCE_ROOT_MPQ_SHA256="+$SourceArchiveHash),
        ("TARGET_LOCALE_MPQ="+$Target),("TARGET_MPQ_SHA256="+$SourceArchiveHash),"TARGET_PREIMAGE=ABSENT",("LOCALE_SLOT="+$LocaleSlot),
        ("SPELL_DBC_SHA256="+$ExpectedSpellHash),("AREA_DBC_SHA256="+$ExpectedAreaHash),("INSTALLED_AT="+(Get-Date).ToString("o"))
    )
    $StateTemp=$R5StateFile+".tmp";if(Test-Path -LiteralPath $StateTemp){throw "R5 state temp exists"}
    [System.IO.File]::WriteAllText($StateTemp,(($StateLines -join [Environment]::NewLine)+[Environment]::NewLine),$Utf8NoBom);Move-Item -LiteralPath $StateTemp -Destination $R5StateFile;$StateCommitted=$true
    Write-Result "ROOT_R4_MPQ_MODIFIED=False";Write-Result "SERVER_MODIFIED=False";Write-Result "CLIENT_RESTART_REQUIRED=True"
    Write-Result "G17R5_LOCALE_MIRROR=PASS";Write-Result "G17R5_LOCALE_MIRROR_RESULT=PASS";Write-Result "RESULT_FILE=$Result";exit 0
}
catch {
    if(-not $StateCommitted -and $Target -and $SourceArchiveHash -and (Test-Path -LiteralPath $Target -PathType Leaf)){
        try{if((Get-FileHash -LiteralPath $Target -Algorithm SHA256).Hash.ToLowerInvariant() -ceq $SourceArchiveHash){$Rescue=Join-Path $UploadDir ("G17R5_FAILED_LOCALE_RESCUE_"+(Get-Date -Format "yyyyMMdd_HHmmss")+".MPQ");Move-Item -LiteralPath $Target -Destination $Rescue;Write-Result "AUTO_RESCUE_NEW_LOCALE_MPQ=PASS"}}catch{Write-Result ("AUTO_RESCUE_ERROR="+$_.Exception.Message)}
    }
    Write-Result ("G17R5_LOCALE_MIRROR_ERROR="+$_.Exception.Message);Write-Result "ROOT_R4_MPQ_WRITE_ATTEMPTED=False";Write-Result "SERVER_WRITE_ATTEMPTED=False";Write-Result "G17R5_LOCALE_MIRROR_RESULT=FAIL";Write-Result "RESULT_FILE=$Result";exit 1
}
finally {
    if($TemporaryTarget -and (Test-Path -LiteralPath $TemporaryTarget)){Remove-Item -LiteralPath $TemporaryTarget -Force -ErrorAction SilentlyContinue}
    if($WorkRoot -and (Test-Path -LiteralPath $WorkRoot -PathType Container)){Remove-Item -LiteralPath $WorkRoot -Recurse -Force -ErrorAction SilentlyContinue}
}
