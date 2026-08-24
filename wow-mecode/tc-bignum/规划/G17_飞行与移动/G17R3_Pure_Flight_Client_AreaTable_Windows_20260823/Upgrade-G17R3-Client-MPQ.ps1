#requires -Version 5.1
$ErrorActionPreference = "Stop"

$Workspace = "C:\Users\Administrator\Downloads\workspace"
$UploadDir = Join-Path $Workspace "uploads"
$ClientRoot = "D:\WOW"
$RunDir = "D:\TC-Build\bin\RelWithDebInfo"
$ServerSpellDbc = Join-Path $RunDir "dbc\Spell.dbc"
$ServerAreaDbc = Join-Path $RunDir "dbc\AreaTable.dbc"
$R1Report = Join-Path $UploadDir "G17R1_CLIENT_MPQ_INSTALL_RESULT.txt"
$R1StateFile = Join-Path $UploadDir "G17R1_CLIENT_MPQ_INSTALL_STATE.txt"
$R3StateFile = Join-Path $UploadDir "G17R3_CLIENT_MPQ_UPGRADE_STATE.txt"
$Result = Join-Path $UploadDir "G17R3_CLIENT_MPQ_UPGRADE_RESULT.txt"
$Tool = Join-Path $PSScriptRoot "third_party\mpqcli-windows-amd64.exe"
$Patcher = Join-Path $PSScriptRoot "tools\patch_g17r3_client_areatable_dbc.py"
$PackageOriginalArea = Join-Path $PSScriptRoot "original\DBFilesClient\AreaTable.dbc"
$PackagePatchedArea = Join-Path $PSScriptRoot "payload\DBFilesClient\AreaTable.dbc"
$SpellTarget = "DBFilesClient\Spell.dbc"
$AreaTarget = "DBFilesClient\AreaTable.dbc"
$ExpectedToolHash = "5dc56b130432098d18e911fdf4b3464adf850432b7fbb35003a2feff7c30a79f"
$ExpectedSpellHash = "dd25091167f671764735ce88c78b66485c6d661fadf05d322574c261f6e464ea"
$ExpectedSpellSize = 48956359
$ExpectedServerSpellHash = "df44e75ef1730e363dc06f1bc5ae064299b08d2d0047e663c0a1782ed4c8d10f"
$ExpectedAreaOriginalHash = "b0356ff41e5777896509ec52bc68af516b67d82a659dbc47757960aef98b62dd"
$ExpectedAreaPatchedHash = "214c6935d11b784f0bf5e4855fb756126d9d667d622a346c3124ae748812b6a8"
$ExpectedAreaSize = 362740
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$WorkRoot = ""
$BackupDir = ""
$TargetMpq = ""
$TemporaryTarget = ""
$SwapOld = ""
$StateCommitted = $false
$NewArchiveHash = ""

New-Item -ItemType Directory -Path $UploadDir -Force | Out-Null
[System.IO.File]::WriteAllText($Result, "", $Utf8NoBom)

function Write-Result([string]$Line) {
    Write-Host $Line
    [System.IO.File]::AppendAllText($Result, $Line + [Environment]::NewLine, $Utf8NoBom)
}

function Read-KeyValueFile([string]$Path) {
    $Values = @{}
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $Values }
    foreach ($Line in [System.IO.File]::ReadAllLines($Path)) {
        if ($Line -match '^([^=]+)=(.*)$') { $Values[$Matches[1]] = $Matches[2] }
    }
    return $Values
}

function Invoke-NativeCapture {
    param([string]$FilePath, [string[]]$NativeArgs)
    $Saved = $ErrorActionPreference
    $Exit = 9009
    $Output = @()
    try {
        $ErrorActionPreference = "Continue"
        $Output = @(& $FilePath @NativeArgs 2>&1)
        $Exit = $LASTEXITCODE
    }
    finally { $ErrorActionPreference = $Saved }
    return [pscustomobject]@{ ExitCode=[int]$Exit; Lines=@($Output) }
}

function Invoke-NativeLogged {
    param([string]$FilePath, [string[]]$NativeArgs, [string]$Prefix)
    $Native = Invoke-NativeCapture -FilePath $FilePath -NativeArgs $NativeArgs
    foreach ($Line in $Native.Lines) { Write-Result ($Prefix + "|" + $Line.ToString()) }
    return [int]$Native.ExitCode
}

function Extract-ArchiveTarget([string]$Archive, [string]$Target, [string]$Tag) {
    $Root = Join-Path $WorkRoot ("extract_" + $Tag + "_" + [Guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Path $Root | Out-Null
    $Native = Invoke-NativeCapture -FilePath $Tool -NativeArgs @("extract", "--output", $Root, "--keep", "--file", $Target, $Archive)
    $Extracted = Join-Path $Root $Target
    if ($Native.ExitCode -eq 0 -and (Test-Path -LiteralPath $Extracted -PathType Leaf)) {
        $Hash = (Get-FileHash -LiteralPath $Extracted -Algorithm SHA256).Hash.ToLowerInvariant()
        $Size = (Get-Item -LiteralPath $Extracted).Length
        return [pscustomobject]@{ State="HIT"; Path=$Extracted; Hash=$Hash; Size=[int64]$Size; ExitCode=0; Detail="" }
    }
    $Text = (($Native.Lines | ForEach-Object { $_.ToString() }) -join " | ")
    if ($Native.ExitCode -ne 0 -and -not (Test-Path -LiteralPath $Extracted) -and
        ($Text.Contains("File doesn't exist") -or $Text.Contains("does not exist"))) {
        return [pscustomobject]@{ State="NO_HIT"; Path=""; Hash=""; Size=0; ExitCode=[int]$Native.ExitCode; Detail="" }
    }
    if ($Text.Length -gt 600) { $Text = $Text.Substring(0,600) }
    return [pscustomobject]@{ State="ERROR"; Path=""; Hash=""; Size=0; ExitCode=[int]$Native.ExitCode; Detail=$Text }
}

function Assert-NewArchive([string]$Archive, [string]$Prefix) {
    $Format = Invoke-NativeCapture -FilePath $Tool -NativeArgs @("info", "--property", "format-version", $Archive)
    if ($Format.ExitCode -ne 0) { throw "mpq format probe failed: $Archive" }
    $FormatText = (($Format.Lines | ForEach-Object { $_.ToString().Trim() }) -join "")
    Write-Result ($Prefix + "_FORMAT_VERSION=" + $FormatText)
    if ($FormatText -ne "2") { throw "unexpected MPQ format version: $FormatText" }
    $Count = Invoke-NativeCapture -FilePath $Tool -NativeArgs @("info", "--property", "file-count", $Archive)
    if ($Count.ExitCode -ne 0) { throw "mpq file-count probe failed: $Archive" }
    $CountText = (($Count.Lines | ForEach-Object { $_.ToString().Trim() }) -join "")
    Write-Result ($Prefix + "_FILE_COUNT=" + $CountText)
    if ($CountText -ne "4") { throw "new archive must contain 2 targets plus listfile/attributes; count=$CountText" }
    $Spell = Extract-ArchiveTarget -Archive $Archive -Target $SpellTarget -Tag ($Prefix + "_spell")
    $Area = Extract-ArchiveTarget -Archive $Archive -Target $AreaTarget -Tag ($Prefix + "_area")
    Write-Result ($Prefix + "_SPELL_SHA256=" + $Spell.Hash)
    Write-Result ($Prefix + "_SPELL_SIZE=" + $Spell.Size)
    Write-Result ($Prefix + "_AREA_SHA256=" + $Area.Hash)
    Write-Result ($Prefix + "_AREA_SIZE=" + $Area.Size)
    if ($Spell.State -ne "HIT" -or $Spell.Hash -cne $ExpectedSpellHash -or $Spell.Size -ne $ExpectedSpellSize) {
        throw "new archive Spell.dbc mismatch"
    }
    if ($Area.State -ne "HIT" -or $Area.Hash -cne $ExpectedAreaPatchedHash -or $Area.Size -ne $ExpectedAreaSize) {
        throw "new archive AreaTable.dbc mismatch"
    }
}

try {
    Write-Result "G17R3_CLIENT_MPQ_UPGRADE_START"
    Write-Result "READ_ONLY_SERVER_DBC=True"
    Write-Result "SQL_EXECUTED=False"
    if (Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Name -match '^(?i:wow|wow-64)$' }) {
        throw "Wow client is running; close it before upgrade"
    }
    foreach ($Required in @($Tool,$Patcher,$PackageOriginalArea,$PackagePatchedArea,$ServerSpellDbc,$ServerAreaDbc,$R1StateFile,$R1Report)) {
        if (-not (Test-Path -LiteralPath $Required -PathType Leaf)) { throw "required file missing: $Required" }
    }
    if (-not (Test-Path -LiteralPath $ClientRoot -PathType Container)) { throw "client root missing: $ClientRoot" }
    $ClientRoot = (Resolve-Path -LiteralPath $ClientRoot).Path
    $DataDir = Join-Path $ClientRoot "Data"
    if (-not (Test-Path -LiteralPath (Join-Path $ClientRoot "Wow.exe") -PathType Leaf) -or
        -not (Test-Path -LiteralPath $DataDir -PathType Container)) { throw "invalid WoW client root" }

    $ToolHash = (Get-FileHash -LiteralPath $Tool -Algorithm SHA256).Hash.ToLowerInvariant()
    $OriginalAreaHash = (Get-FileHash -LiteralPath $PackageOriginalArea -Algorithm SHA256).Hash.ToLowerInvariant()
    $PatchedAreaHash = (Get-FileHash -LiteralPath $PackagePatchedArea -Algorithm SHA256).Hash.ToLowerInvariant()
    $ServerSpellBefore = (Get-FileHash -LiteralPath $ServerSpellDbc -Algorithm SHA256).Hash.ToLowerInvariant()
    $ServerAreaBefore = (Get-FileHash -LiteralPath $ServerAreaDbc -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-Result "MPQCLI_SHA256=$ToolHash"
    Write-Result "PACKAGE_ORIGINAL_AREA_SHA256=$OriginalAreaHash"
    Write-Result "PACKAGE_PATCHED_AREA_SHA256=$PatchedAreaHash"
    Write-Result "SERVER_SPELL_DBC_SHA256_BEFORE=$ServerSpellBefore"
    Write-Result "SERVER_AREA_DBC_SHA256_BEFORE=$ServerAreaBefore"
    if ($ToolHash -cne $ExpectedToolHash) { throw "mpqcli hash mismatch" }
    if ($OriginalAreaHash -cne $ExpectedAreaOriginalHash -or (Get-Item $PackageOriginalArea).Length -ne $ExpectedAreaSize) { throw "packaged original AreaTable mismatch" }
    if ($PatchedAreaHash -cne $ExpectedAreaPatchedHash -or (Get-Item $PackagePatchedArea).Length -ne $ExpectedAreaSize) { throw "packaged patched AreaTable mismatch" }
    if ($ServerSpellBefore -cne $ExpectedServerSpellHash) { throw "server Spell.dbc is not original" }
    if ($ServerAreaBefore -cne $ExpectedAreaOriginalHash) { throw "server AreaTable.dbc differs from locked zhCN original" }
    if ((Get-FileHash -LiteralPath $PackageOriginalArea -Algorithm SHA256).Hash -cne
        (Get-FileHash -LiteralPath $ServerAreaDbc -Algorithm SHA256).Hash) { throw "packaged/client source AreaTable differs from server original" }

    $ConfigWtf = Join-Path $ClientRoot "WTF\Config.wtf"
    $Locale = "UNKNOWN"
    if (Test-Path -LiteralPath $ConfigWtf -PathType Leaf) {
        foreach ($Line in [System.IO.File]::ReadAllLines($ConfigWtf)) {
            if ($Line -match '^SET\s+locale\s+"([A-Za-z]{4})"') { $Locale=$Matches[1];break }
        }
    }
    Write-Result "CLIENT_LOCALE=$Locale"
    if ($Locale -cne "zhCN") { throw "this locked AreaTable payload is zhCN; detected locale=$Locale" }

    $R1ReportValues = Read-KeyValueFile $R1Report
    $R1State = Read-KeyValueFile $R1StateFile
    if (-not $R1ReportValues.ContainsKey("G17R1_CLIENT_MPQ_INSTALL_RESULT") -or
        $R1ReportValues["G17R1_CLIENT_MPQ_INSTALL_RESULT"] -cne "PASS") { throw "R1 client install report is not PASS" }
    if (-not $R1State.ContainsKey("INSTALL_STATUS") -or $R1State["INSTALL_STATUS"] -cne "PASS") { throw "R1 install state is not PASS" }
    if ($R1State["CLIENT_ROOT"] -ine $ClientRoot) { throw "R1 state belongs to another client root" }
    $TargetMpq = $R1State["INSTALLED_MPQ"]
    $Slot = $R1State["PATCH_SLOT"]
    if (-not $TargetMpq -or -not $Slot -or $Slot -notmatch '^[A-Z]$') { throw "R1 state target/slot missing" }
    $ExpectedTargetPath = Join-Path $DataDir ("patch-" + $Slot + ".MPQ")
    if ($TargetMpq -ine $ExpectedTargetPath) { throw "R1 state target is outside its owned patch slot" }

    $WorkRoot = Join-Path $UploadDir ("G17R3_Client_Work_" + (Get-Date -Format "yyyyMMdd_HHmmss") + "_" + $PID)
    if (Test-Path -LiteralPath $WorkRoot) { throw "work directory already exists" }
    New-Item -ItemType Directory -Path $WorkRoot | Out-Null

    if (Test-Path -LiteralPath $R3StateFile -PathType Leaf) {
        $R3State = Read-KeyValueFile $R3StateFile
        if ($R3State["INSTALL_STATUS"] -cne "PASS" -or $R3State["CLIENT_ROOT"] -ine $ClientRoot -or
            $R3State["INSTALLED_MPQ"] -ine $TargetMpq) { throw "existing R3 state ownership mismatch" }
        if (-not (Test-Path -LiteralPath $TargetMpq -PathType Leaf)) { throw "R3 state exists but target MPQ is missing" }
        $CurrentHash = (Get-FileHash -LiteralPath $TargetMpq -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($CurrentHash -cne $R3State["NEW_MPQ_SHA256"]) { throw "R3 target hash changed" }
        Assert-NewArchive -Archive $TargetMpq -Prefix "ALREADY_CURRENT"
        Write-Result "SERVER_DBC_MODIFIED=False"
        Write-Result "G17R3_CLIENT_MPQ_UPGRADE=ALREADY_CURRENT"
        Write-Result "G17R3_CLIENT_MPQ_UPGRADE_RESULT=PASS"
        Write-Result "RESULT_FILE=$Result"
        exit 0
    }

    if (-not (Test-Path -LiteralPath $TargetMpq -PathType Leaf)) { throw "R1 installed MPQ missing: $TargetMpq" }
    $OldArchiveHash = (Get-FileHash -LiteralPath $TargetMpq -Algorithm SHA256).Hash.ToLowerInvariant()
    $OldArchiveSize = (Get-Item -LiteralPath $TargetMpq).Length
    Write-Result "R1_OWNED_MPQ=$TargetMpq"
    Write-Result "R1_OWNED_MPQ_SHA256=$OldArchiveHash"
    Write-Result "R1_OWNED_MPQ_SIZE=$OldArchiveSize"
    if ($OldArchiveHash -cne $R1State["INSTALLED_MPQ_SHA256"].ToLowerInvariant()) { throw "R1 owned MPQ hash no longer matches state" }
    $OldSpell = Extract-ArchiveTarget -Archive $TargetMpq -Target $SpellTarget -Tag "old_spell"
    $OldArea = Extract-ArchiveTarget -Archive $TargetMpq -Target $AreaTarget -Tag "old_area"
    Write-Result "R1_OWNED_SPELL_SHA256=$($OldSpell.Hash)"
    Write-Result "R1_OWNED_AREA_STATE=$($OldArea.State)"
    if ($OldSpell.State -ne "HIT" -or $OldSpell.Hash -cne $ExpectedSpellHash -or $OldSpell.Size -ne $ExpectedSpellSize) { throw "R1 owned Spell.dbc mismatch" }
    if ($OldArea.State -ne "NO_HIT") { throw "R1 owned MPQ unexpectedly already contains AreaTable.dbc" }

    $Slots = @("Z","Y","X","W","V","U","T","S","R","Q","P","O","N","M","L","K","J","I","H","G","F","E","D","C","B","A")
    $AreaCollisions = 0
    foreach ($OtherSlot in $Slots) {
        if ($OtherSlot -ceq $Slot) { continue }
        $Candidate = Join-Path $DataDir ("patch-" + $OtherSlot + ".MPQ")
        if (-not (Test-Path -LiteralPath $Candidate)) { continue }
        if (Test-Path -LiteralPath $Candidate -PathType Container) {
            $Loose = Join-Path $Candidate $AreaTarget
            $Hit = [int](Test-Path -LiteralPath $Loose -PathType Leaf)
            Write-Result "OTHER_SLOT_AREA=SLOT=$OtherSlot;TYPE=DIRECTORY;HITS=$Hit;PATH=$Candidate"
            if ($Hit) { $AreaCollisions++ }
        }
        elseif (Test-Path -LiteralPath $Candidate -PathType Leaf) {
            $Probe = Extract-ArchiveTarget -Archive $Candidate -Target $AreaTarget -Tag ("slot_" + $OtherSlot)
            Write-Result "OTHER_SLOT_AREA=SLOT=$OtherSlot;TYPE=PACKED_MPQ;STATE=$($Probe.State);PATH=$Candidate"
            if ($Probe.State -eq "HIT") { $AreaCollisions++ }
            elseif ($Probe.State -eq "ERROR") { throw "cannot safely inspect AreaTable in custom slot ${OtherSlot}: $($Probe.Detail)" }
        }
        else { throw "unknown filesystem type in custom slot $OtherSlot" }
    }
    Write-Result "OTHER_CUSTOM_AREATABLE_COLLISIONS=$AreaCollisions"
    if ($AreaCollisions -ne 0) { throw "another custom letter MPQ already owns AreaTable.dbc; refusing override" }

    $PythonCandidates = @(
        (Join-Path $env:LOCALAPPDATA "Programs\Python\Python312\python.exe"),
        (Join-Path $env:LOCALAPPDATA "Programs\Python\Python310\python.exe")
    )
    $Python = @($PythonCandidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf })[0]
    if (-not $Python) {
        $PythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
        if (-not $PythonCommand) { $PythonCommand = Get-Command python -ErrorAction SilentlyContinue }
        if ($PythonCommand -and $PythonCommand.Source -notmatch "\\WindowsApps\\") { $Python=$PythonCommand.Source }
    }
    if (-not $Python) { throw "Python 3.12/3.10 not found; py.exe and WindowsApps aliases are not used" }
    Write-Result "PYTHON=$Python"
    $GeneratedArea = Join-Path $WorkRoot "generated\DBFilesClient\AreaTable.dbc"
    $PatchReport = Join-Path $WorkRoot "G17R3_AreaTable_Patch_Report.txt"
    $PatchArgs = @($Patcher,"patch","--input",$ServerAreaDbc,"--output",$GeneratedArea,"--report",$PatchReport)
    $PatchExit = Invoke-NativeLogged -FilePath $Python -NativeArgs $PatchArgs -Prefix "AREATABLE_PATCH"
    Write-Result "AREATABLE_PATCH_EXIT=$PatchExit"
    if ($PatchExit -ne 0) { throw "AreaTable patcher failed" }
    $GeneratedHash = (Get-FileHash -LiteralPath $GeneratedArea -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-Result "GENERATED_AREA_SHA256=$GeneratedHash"
    if ($GeneratedHash -cne $ExpectedAreaPatchedHash -or
        (Get-Item -LiteralPath $GeneratedArea).Length -ne $ExpectedAreaSize) { throw "generated AreaTable mismatch" }
    if ((Get-FileHash -LiteralPath $GeneratedArea -Algorithm SHA256).Hash -cne
        (Get-FileHash -LiteralPath $PackagePatchedArea -Algorithm SHA256).Hash) { throw "generated AreaTable differs from packaged payload" }

    $PackRoot = Join-Path $WorkRoot "pack_root"
    $PackSpell = Join-Path $PackRoot $SpellTarget
    $PackArea = Join-Path $PackRoot $AreaTarget
    New-Item -ItemType Directory -Path (Split-Path -Parent $PackSpell) -Force | Out-Null
    Copy-Item -LiteralPath $OldSpell.Path -Destination $PackSpell
    Copy-Item -LiteralPath $GeneratedArea -Destination $PackArea
    $BuiltArchive = Join-Path $WorkRoot ("patch-" + $Slot + ".MPQ")
    $CreateExit = Invoke-NativeLogged -FilePath $Tool -NativeArgs @("create","--game","wow-wotlk","--output",$BuiltArchive,$PackRoot) -Prefix "MPQ_CREATE"
    Write-Result "MPQ_CREATE_EXIT=$CreateExit"
    if ($CreateExit -ne 0 -or -not (Test-Path -LiteralPath $BuiltArchive -PathType Leaf)) { throw "new MPQ creation failed" }
    Assert-NewArchive -Archive $BuiltArchive -Prefix "BUILT_MPQ"
    $NewArchiveHash = (Get-FileHash -LiteralPath $BuiltArchive -Algorithm SHA256).Hash.ToLowerInvariant()
    $NewArchiveSize = (Get-Item -LiteralPath $BuiltArchive).Length
    Write-Result "NEW_MPQ_SHA256=$NewArchiveHash"
    Write-Result "NEW_MPQ_SIZE=$NewArchiveSize"
    if ($NewArchiveHash -ceq $OldArchiveHash) { throw "new MPQ hash unexpectedly equals old MPQ" }

    $Stamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $BackupDir = Join-Path $UploadDir ("G17R3_Client_AreaTable_Backup_" + $Stamp)
    if (Test-Path -LiteralPath $BackupDir) { throw "backup directory already exists" }
    New-Item -ItemType Directory -Path $BackupDir | Out-Null
    $BackupMpq = Join-Path $BackupDir ("before_G17R3_" + (Split-Path -Leaf $TargetMpq))
    $BackupR1State = Join-Path $BackupDir "G17R1_CLIENT_MPQ_INSTALL_STATE.txt"
    Copy-Item -LiteralPath $TargetMpq -Destination $BackupMpq
    Copy-Item -LiteralPath $R1StateFile -Destination $BackupR1State
    Copy-Item -LiteralPath $R1Report -Destination (Join-Path $BackupDir "G17R1_CLIENT_MPQ_INSTALL_RESULT.txt")
    if ((Get-FileHash -LiteralPath $BackupMpq -Algorithm SHA256).Hash.ToLowerInvariant() -cne $OldArchiveHash) { throw "backup MPQ verification failed" }

    $TemporaryTarget = $TargetMpq + ".g17r3.new.tmp"
    $SwapOld = $TargetMpq + ".g17r3.old.tmp"
    if ((Test-Path -LiteralPath $TemporaryTarget) -or (Test-Path -LiteralPath $SwapOld)) { throw "client swap temp path already exists" }
    Copy-Item -LiteralPath $BuiltArchive -Destination $TemporaryTarget
    if ((Get-FileHash -LiteralPath $TemporaryTarget -Algorithm SHA256).Hash.ToLowerInvariant() -cne $NewArchiveHash) { throw "new client temp MPQ hash mismatch" }
    Move-Item -LiteralPath $TargetMpq -Destination $SwapOld
    Move-Item -LiteralPath $TemporaryTarget -Destination $TargetMpq
    Assert-NewArchive -Archive $TargetMpq -Prefix "INSTALLED_MPQ"
    if ((Get-FileHash -LiteralPath $TargetMpq -Algorithm SHA256).Hash.ToLowerInvariant() -cne $NewArchiveHash) { throw "installed new MPQ hash mismatch" }

    $StateLines = @(
        "STATE_FORMAT=1",
        "INSTALL_STATUS=PASS",
        ("CLIENT_ROOT=" + $ClientRoot),
        ("INSTALLED_MPQ=" + $TargetMpq),
        ("PATCH_SLOT=" + $Slot),
        ("OLD_MPQ_SHA256=" + $OldArchiveHash),
        ("OLD_MPQ_SIZE=" + $OldArchiveSize),
        ("NEW_MPQ_SHA256=" + $NewArchiveHash),
        ("NEW_MPQ_SIZE=" + $NewArchiveSize),
        ("BACKUP_MPQ=" + $BackupMpq),
        ("BACKUP_R1_STATE=" + $BackupR1State),
        ("SPELL_DBC_SHA256=" + $ExpectedSpellHash),
        ("AREA_ORIGINAL_SHA256=" + $ExpectedAreaOriginalHash),
        ("AREA_PATCHED_SHA256=" + $ExpectedAreaPatchedHash),
        ("SERVER_SPELL_DBC_SHA256=" + $ExpectedServerSpellHash),
        ("SERVER_AREA_DBC_SHA256=" + $ExpectedAreaOriginalHash),
        ("INSTALLED_AT=" + (Get-Date).ToString("o"))
    )
    $StateTemp = $R3StateFile + ".tmp"
    if (Test-Path -LiteralPath $StateTemp) { throw "R3 state temp already exists" }
    [System.IO.File]::WriteAllText($StateTemp,(($StateLines -join [Environment]::NewLine)+[Environment]::NewLine),$Utf8NoBom)
    Move-Item -LiteralPath $StateTemp -Destination $R3StateFile
    $StateCommitted = $true
    Remove-Item -LiteralPath $SwapOld -Force -ErrorAction SilentlyContinue
    $SwapOld = ""

    $ServerSpellAfter = (Get-FileHash -LiteralPath $ServerSpellDbc -Algorithm SHA256).Hash.ToLowerInvariant()
    $ServerAreaAfter = (Get-FileHash -LiteralPath $ServerAreaDbc -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-Result "SERVER_SPELL_DBC_SHA256_AFTER=$ServerSpellAfter"
    Write-Result "SERVER_AREA_DBC_SHA256_AFTER=$ServerAreaAfter"
    if ($ServerSpellAfter -cne $ServerSpellBefore -or $ServerAreaAfter -cne $ServerAreaBefore) { throw "server DBC changed during client upgrade" }
    Write-Result "SERVER_DBC_MODIFIED=False"
    Write-Result "R1_STATE_MODIFIED=False"
    Write-Result "CLIENT_RESTART_REQUIRED=True"
    Write-Result "G17R3_CLIENT_MPQ_UPGRADE=PASS"
    Write-Result "G17R3_CLIENT_MPQ_UPGRADE_RESULT=PASS"
    Write-Result "RESULT_FILE=$Result"
    exit 0
}
catch {
    $Message = $_.Exception.Message
    if (-not $StateCommitted) {
        try {
            if ($TargetMpq -and $NewArchiveHash -and (Test-Path -LiteralPath $TargetMpq -PathType Leaf)) {
                $Current = (Get-FileHash -LiteralPath $TargetMpq -Algorithm SHA256).Hash.ToLowerInvariant()
                if ($Current -ceq $NewArchiveHash) {
                    $Rescue = if ($BackupDir) { Join-Path $BackupDir "FAILED_NEW_MPQ_RESCUE.MPQ" } else { Join-Path $WorkRoot "FAILED_NEW_MPQ_RESCUE.MPQ" }
                    Move-Item -LiteralPath $TargetMpq -Destination $Rescue
                    Write-Result "FAILED_NEW_MPQ_RESCUED=True"
                }
            }
            if ($SwapOld -and (Test-Path -LiteralPath $SwapOld -PathType Leaf) -and -not (Test-Path -LiteralPath $TargetMpq)) {
                Move-Item -LiteralPath $SwapOld -Destination $TargetMpq
                Write-Result "AUTO_ROLLBACK_OLD_MPQ=PASS"
            }
            elseif ($BackupDir -and $TargetMpq -and -not (Test-Path -LiteralPath $TargetMpq)) {
                $BackupCandidate = @(Get-ChildItem -LiteralPath $BackupDir -File -Filter "before_G17R3_patch-*.MPQ" -ErrorAction SilentlyContinue)[0]
                if ($BackupCandidate) { Copy-Item -LiteralPath $BackupCandidate.FullName -Destination $TargetMpq; Write-Result "AUTO_ROLLBACK_FROM_BACKUP=PASS" }
            }
        }
        catch { Write-Result ("AUTO_ROLLBACK_ERROR=" + $_.Exception.Message) }
    }
    Write-Result ("G17R3_CLIENT_MPQ_UPGRADE_ERROR=" + $Message)
    Write-Result "SERVER_DBC_WRITE_ATTEMPTED=False"
    Write-Result "R1_STATE_WRITE_ATTEMPTED=False"
    Write-Result "G17R3_CLIENT_MPQ_UPGRADE_RESULT=FAIL"
    Write-Result "RESULT_FILE=$Result"
    exit 1
}
finally {
    if ($TemporaryTarget -and (Test-Path -LiteralPath $TemporaryTarget -PathType Leaf)) { Remove-Item -LiteralPath $TemporaryTarget -Force -ErrorAction SilentlyContinue }
    if ($WorkRoot -and (Test-Path -LiteralPath $WorkRoot -PathType Container)) { Remove-Item -LiteralPath $WorkRoot -Recurse -Force -ErrorAction SilentlyContinue }
}
