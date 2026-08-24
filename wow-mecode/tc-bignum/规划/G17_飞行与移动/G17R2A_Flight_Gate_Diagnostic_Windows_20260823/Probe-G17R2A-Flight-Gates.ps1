#requires -Version 5.1
$ErrorActionPreference = "Stop"

$Workspace = "C:\Users\Administrator\Downloads\workspace"
$UploadDir = Join-Path $Workspace "uploads"
$Result = Join-Path $UploadDir "G17R2A_GATE_DIAGNOSTIC_RESULT.txt"
$ClientRoot = "D:\WOW"
$BuildRoot = "D:\TC-Build"
$RunDir = Join-Path $BuildRoot "bin\RelWithDebInfo"
$SourceRoot = "D:\TrinityCore"
$WorldExe = Join-Path $RunDir "worldserver.exe"
$SpellInfoSource = Join-Path $SourceRoot "src\server\game\Spells\SpellInfo.cpp"
$SpellSource = Join-Path $SourceRoot "src\server\game\Spells\Spell.cpp"
$R2BuildResult = Join-Path $UploadDir "G17R2_WINDOWS_FIX_RESULT.txt"
$ClientInstallReport = Join-Path $UploadDir "G17R1_CLIENT_MPQ_INSTALL_RESULT.txt"
$ClientStateFile = Join-Path $UploadDir "G17R1_CLIENT_MPQ_INSTALL_STATE.txt"
$Tool = Join-Path $PSScriptRoot "third_party\mpqcli-windows-amd64.exe"
$ArchiveTarget = "DBFilesClient\Spell.dbc"
$ExpectedToolHash = "5dc56b130432098d18e911fdf4b3464adf850432b7fbb35003a2feff7c30a79f"
$ExpectedPatchedDbcHash = "dd25091167f671764735ce88c78b66485c6d661fadf05d322574c261f6e464ea"
$ExpectedPatchedDbcSize = 48956359
$ExpectedR2SpellInfoHash = "73d52ac0feb67a32822fc0bf086a9174ba7ef0bc186223cdc8a690f48fccb9e2"
$KnownUpstreamSpellHash = "75b8db7d175499d9add2c70be66eb03c97a9476a3a0daeadcc7af601c1287476"
$MarkerText = "G17R2 old-world pure-flight location allowed: spell=59961"
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$WorkRoot = ""

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
    param(
        [Parameter(Mandatory=$true)][string]$FilePath,
        [Parameter(Mandatory=$true)][string[]]$NativeArgs
    )
    $SavedPreference = $ErrorActionPreference
    $NativeExit = 9009
    $NativeOutput = @()
    try {
        $ErrorActionPreference = "Continue"
        $NativeOutput = @(& $FilePath @NativeArgs 2>&1)
        $NativeExit = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $SavedPreference
    }
    return [pscustomobject]@{ ExitCode = [int]$NativeExit; Lines = @($NativeOutput) }
}

function Probe-ArchiveTarget([string]$ArchivePath, [string]$Tag) {
    $ProbeRoot = Join-Path $WorkRoot ("probe_" + $Tag + "_" + [Guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Path $ProbeRoot | Out-Null
    try {
        $Native = Invoke-NativeCapture -FilePath $Tool -NativeArgs @("extract", "--output", $ProbeRoot, "--keep", "--file", $ArchiveTarget, $ArchivePath)
        $Extracted = Join-Path $ProbeRoot "DBFilesClient\Spell.dbc"
        if ($Native.ExitCode -eq 0 -and (Test-Path -LiteralPath $Extracted -PathType Leaf)) {
            $Hash = (Get-FileHash -LiteralPath $Extracted -Algorithm SHA256).Hash.ToLowerInvariant()
            $Size = (Get-Item -LiteralPath $Extracted).Length
            return [pscustomobject]@{ State = "HIT"; Hash = $Hash; Size = [int64]$Size; ExitCode = 0; Detail = "" }
        }
        $Text = (($Native.Lines | ForEach-Object { $_.ToString() }) -join " | ")
        if ($Native.ExitCode -ne 0 -and -not (Test-Path -LiteralPath $Extracted) -and
            ($Text.Contains("File doesn't exist") -or $Text.Contains("does not exist"))) {
            return [pscustomobject]@{ State = "NO_HIT"; Hash = ""; Size = 0; ExitCode = [int]$Native.ExitCode; Detail = "" }
        }
        if ($Text.Length -gt 600) { $Text = $Text.Substring(0, 600) }
        return [pscustomobject]@{ State = "ERROR"; Hash = ""; Size = 0; ExitCode = [int]$Native.ExitCode; Detail = $Text }
    }
    finally {
        Remove-Item -LiteralPath $ProbeRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}

try {
    Write-Result "G17R2A_GATE_DIAGNOSTIC_START"
    Write-Result ("DIAGNOSTIC_UTC=" + [DateTime]::UtcNow.ToString("o"))
    Write-Result "READ_ONLY=True"
    Write-Result "DATABASE_WRITE_ATTEMPTED=False"
    Write-Result "SOURCE_WRITE_ATTEMPTED=False"
    Write-Result "CLIENT_WRITE_ATTEMPTED=False"

    foreach ($Required in @($Tool, $RunDir, $WorldExe, $SpellInfoSource, $SpellSource)) {
        if (-not (Test-Path -LiteralPath $Required)) { throw "required diagnostic target missing: $Required" }
    }
    $ToolHash = (Get-FileHash -LiteralPath $Tool -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-Result "MPQCLI_SHA256=$ToolHash"
    if ($ToolHash -cne $ExpectedToolHash) { throw "bundled mpqcli hash mismatch" }

    $WorkRoot = Join-Path $UploadDir ("G17R2A_ReadOnly_Work_" + $PID)
    if (Test-Path -LiteralPath $WorkRoot) { throw "diagnostic work directory already exists: $WorkRoot" }
    New-Item -ItemType Directory -Path $WorkRoot | Out-Null

    $R2Values = Read-KeyValueFile $R2BuildResult
    Write-Result ("R2_BUILD_RESULT_FILE_EXISTS=" + (Test-Path -LiteralPath $R2BuildResult -PathType Leaf))
    if ($R2Values.ContainsKey("G17R2_WINDOWS_BUILD_RESULT")) {
        Write-Result ("R2_BUILD_RESULT=" + $R2Values["G17R2_WINDOWS_BUILD_RESULT"])
    }
    else { Write-Result "R2_BUILD_RESULT=UNKNOWN" }

    $ExeHash = (Get-FileHash -LiteralPath $WorldExe -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-Result "ACTIVE_WORLDSERVER_EXE_SHA256=$ExeHash"
    if ($R2Values.ContainsKey("AFTER_EXE_SHA256")) {
        $ExpectedExeHash = $R2Values["AFTER_EXE_SHA256"].ToLowerInvariant()
        Write-Result "R2_REPORTED_AFTER_EXE_SHA256=$ExpectedExeHash"
        Write-Result ("ACTIVE_EXE_MATCHES_R2_BUILD=" + ($ExeHash -ceq $ExpectedExeHash))
    }
    else { Write-Result "ACTIVE_EXE_MATCHES_R2_BUILD=UNKNOWN" }

    $SpellInfoHash = (Get-FileHash -LiteralPath $SpellInfoSource -Algorithm SHA256).Hash.ToLowerInvariant()
    $SpellHash = (Get-FileHash -LiteralPath $SpellSource -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-Result "SPELLINFO_SOURCE_SHA256=$SpellInfoHash"
    Write-Result ("SPELLINFO_R2_POSTIMAGE=" + ($SpellInfoHash -ceq $ExpectedR2SpellInfoHash))
    Write-Result "SPELL_SOURCE_SHA256=$SpellHash"
    Write-Result ("SPELL_SOURCE_MATCHES_KNOWN_UPSTREAM=" + ($SpellHash -ceq $KnownUpstreamSpellHash))

    $ConfigCandidates = @()
    $MainConfig = Join-Path $RunDir "worldserver.conf"
    if (Test-Path -LiteralPath $MainConfig -PathType Leaf) { $ConfigCandidates += Get-Item -LiteralPath $MainConfig }
    $ConfigDir = Join-Path $RunDir "worldserver.conf.d"
    if (Test-Path -LiteralPath $ConfigDir -PathType Container) {
        $ConfigCandidates += @(Get-ChildItem -LiteralPath $ConfigDir -File -Filter "*.conf" -ErrorAction SilentlyContinue)
    }
    $ConfigHits = @()
    foreach ($ConfigFile in $ConfigCandidates) {
        $Hits = @(Select-String -LiteralPath $ConfigFile.FullName -Pattern '^\s*WorldFlight\.' -ErrorAction SilentlyContinue)
        foreach ($Hit in $Hits) {
            $Value = $Hit.Line.Trim()
            if ($Value.Length -gt 500) { $Value = $Value.Substring(0, 500) }
            $ConfigHits += ($ConfigFile.FullName + ":" + $Hit.LineNumber + ":" + $Value)
        }
    }
    Write-Result "WORLD_FLIGHT_CONFIG_LINES=$($ConfigHits.Count)"
    foreach ($Hit in $ConfigHits) { Write-Result ("WORLD_FLIGHT_CONFIG=" + $Hit) }

    $LogFiles = @()
    try {
        $LogFiles = @(Get-ChildItem -LiteralPath $RunDir -File -Recurse -Filter "*.log" -ErrorAction SilentlyContinue |
            Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 40)
    }
    catch { Write-Result ("LOG_DISCOVERY_WARNING=" + $_.Exception.Message) }
    Write-Result "LOG_FILES_SCANNED=$($LogFiles.Count)"
    $MarkerHits = 0
    $RelatedLines = @()
    foreach ($Log in $LogFiles) {
        try {
            $Tail = @(Get-Content -LiteralPath $Log.FullName -Tail 60000 -ErrorAction Stop)
            foreach ($LineObject in $Tail) {
                $Line = $LineObject.ToString()
                if ($Line.Contains($MarkerText)) { $MarkerHits++ }
                if ($Line -match 'G17R2|spell=59961|spell 59961|59961.*Spell') {
                    if ($Line.Length -gt 1000) { $Line = $Line.Substring(0, 1000) }
                    $RelatedLines += ($Log.FullName + "|" + $Line)
                }
            }
        }
        catch { Write-Result ("LOG_READ_WARNING=" + $Log.FullName + "|" + $_.Exception.Message) }
    }
    Write-Result "G17R2_59961_SERVER_MARKER_HITS=$MarkerHits"
    if ($MarkerHits -gt 0) { Write-Result "G17R2_59961_SERVER_LOCATION_GATE=PASS_CONFIRMED" }
    else { Write-Result "G17R2_59961_SERVER_LOCATION_GATE=NOT_CONFIRMED_FROM_DISCOVERED_LOGS" }
    $RelatedTail = @($RelatedLines | Select-Object -Last 30)
    Write-Result "RELATED_LOG_LINES_REPORTED=$($RelatedTail.Count)"
    foreach ($Line in $RelatedTail) { Write-Result ("RELATED_LOG=" + $Line) }

    Write-Result ("CLIENT_ROOT_EXISTS=" + (Test-Path -LiteralPath $ClientRoot -PathType Container))
    if (-not (Test-Path -LiteralPath $ClientRoot -PathType Container)) { throw "client root missing: $ClientRoot" }
    $DataDir = Join-Path $ClientRoot "Data"
    if (-not (Test-Path -LiteralPath $DataDir -PathType Container)) { throw "client Data directory missing" }

    $InstallValues = Read-KeyValueFile $ClientInstallReport
    $StateValues = Read-KeyValueFile $ClientStateFile
    Write-Result ("CLIENT_INSTALL_REPORT_EXISTS=" + (Test-Path -LiteralPath $ClientInstallReport -PathType Leaf))
    if ($InstallValues.ContainsKey("G17R1_CLIENT_MPQ_INSTALL_RESULT")) {
        Write-Result ("CLIENT_INSTALL_REPORT_RESULT=" + $InstallValues["G17R1_CLIENT_MPQ_INSTALL_RESULT"])
    }
    else { Write-Result "CLIENT_INSTALL_REPORT_RESULT=UNKNOWN" }
    Write-Result ("CLIENT_INSTALL_STATE_EXISTS=" + (Test-Path -LiteralPath $ClientStateFile -PathType Leaf))
    if ($StateValues.ContainsKey("INSTALL_STATUS")) { Write-Result ("CLIENT_INSTALL_STATE_STATUS=" + $StateValues["INSTALL_STATUS"]) }
    else { Write-Result "CLIENT_INSTALL_STATE_STATUS=UNKNOWN" }

    if ($StateValues.ContainsKey("INSTALLED_MPQ")) {
        $StateTarget = $StateValues["INSTALLED_MPQ"]
        Write-Result "STATE_INSTALLED_MPQ=$StateTarget"
        $StateTargetExists = Test-Path -LiteralPath $StateTarget -PathType Leaf
        Write-Result "STATE_INSTALLED_MPQ_EXISTS=$StateTargetExists"
        if ($StateTargetExists) {
            $StateTargetHash = (Get-FileHash -LiteralPath $StateTarget -Algorithm SHA256).Hash.ToLowerInvariant()
            Write-Result "STATE_INSTALLED_MPQ_ACTUAL_SHA256=$StateTargetHash"
            if ($StateValues.ContainsKey("INSTALLED_MPQ_SHA256")) {
                Write-Result ("STATE_INSTALLED_MPQ_HASH_MATCH=" + ($StateTargetHash -ceq $StateValues["INSTALLED_MPQ_SHA256"].ToLowerInvariant()))
            }
        }
    }

    $Slots = @("Z","Y","X","W","V","U","T","S","R","Q","P","O","N","M","L","K","J","I","H","G","F","E","D","C","B","A")
    $RootHits = @()
    $RootErrors = @()
    foreach ($Slot in $Slots) {
        $CurrentSlotIndex = [array]::IndexOf($Slots, $Slot)
        $Candidate = Join-Path $DataDir ("patch-" + $Slot + ".MPQ")
        if (-not (Test-Path -LiteralPath $Candidate)) { continue }
        if (Test-Path -LiteralPath $Candidate -PathType Container) {
            $LooseTarget = Join-Path $Candidate "DBFilesClient\Spell.dbc"
            if (Test-Path -LiteralPath $LooseTarget -PathType Leaf) {
                $Hash = (Get-FileHash -LiteralPath $LooseTarget -Algorithm SHA256).Hash.ToLowerInvariant()
                $Size = (Get-Item -LiteralPath $LooseTarget).Length
                $RootHits += [pscustomobject]@{ Slot=$Slot; Index=$CurrentSlotIndex; Path=$Candidate; Hash=$Hash; Size=[int64]$Size; Type="DIRECTORY" }
                Write-Result "ROOT_SLOT_TARGET=HIT;SLOT=$Slot;TYPE=DIRECTORY;SHA256=$Hash;SIZE=$Size;PATH=$Candidate"
            }
            else { Write-Result "ROOT_SLOT_TARGET=NO_HIT;SLOT=$Slot;TYPE=DIRECTORY;PATH=$Candidate" }
        }
        elseif (Test-Path -LiteralPath $Candidate -PathType Leaf) {
            $Probe = Probe-ArchiveTarget -ArchivePath $Candidate -Tag ("root_" + $Slot)
            if ($Probe.State -eq "HIT") {
                $RootHits += [pscustomobject]@{ Slot=$Slot; Index=$CurrentSlotIndex; Path=$Candidate; Hash=$Probe.Hash; Size=[int64]$Probe.Size; Type="PACKED_MPQ" }
                Write-Result "ROOT_SLOT_TARGET=HIT;SLOT=$Slot;TYPE=PACKED_MPQ;SHA256=$($Probe.Hash);SIZE=$($Probe.Size);PATH=$Candidate"
            }
            elseif ($Probe.State -eq "NO_HIT") { Write-Result "ROOT_SLOT_TARGET=NO_HIT;SLOT=$Slot;TYPE=PACKED_MPQ;PATH=$Candidate" }
            else {
                $RootErrors += [pscustomobject]@{ Slot=$Slot; Index=$CurrentSlotIndex }
                Write-Result "ROOT_SLOT_TARGET=ERROR;SLOT=$Slot;EXIT=$($Probe.ExitCode);DETAIL=$($Probe.Detail);PATH=$Candidate"
            }
        }
        else { $RootErrors += [pscustomobject]@{ Slot=$Slot; Index=$CurrentSlotIndex }; Write-Result "ROOT_SLOT_TARGET=ERROR;SLOT=$Slot;DETAIL=UNKNOWN_FILESYSTEM_TYPE;PATH=$Candidate" }
    }

    Write-Result "ROOT_SPELL_DBC_HITS=$($RootHits.Count)"
    Write-Result "ROOT_SLOT_PROBE_ERRORS=$($RootErrors.Count)"
    $ClientGate = "FAIL_NO_ROOT_CUSTOM_SPELL_DBC"
    $BlockingErrors = @()
    if ($RootHits.Count -gt 0) {
        $Highest = $RootHits[0]
        $BlockingErrors = @($RootErrors | Where-Object { $_.Index -lt $Highest.Index })
        Write-Result "EFFECTIVE_ROOT_SPELL_DBC_SLOT=$($Highest.Slot)"
        Write-Result "EFFECTIVE_ROOT_SPELL_DBC_SHA256=$($Highest.Hash)"
        Write-Result "EFFECTIVE_ROOT_SPELL_DBC_SIZE=$($Highest.Size)"
        if ($BlockingErrors.Count -gt 0) { $ClientGate = "UNKNOWN_UNINSPECTABLE_HIGHER_ROOT_SLOT" }
        elseif ($Highest.Hash -ceq $ExpectedPatchedDbcHash -and $Highest.Size -eq $ExpectedPatchedDbcSize) { $ClientGate = "PASS_EXPECTED_PATCHED_DBC" }
        else { $ClientGate = "FAIL_EFFECTIVE_ROOT_DBC_NOT_EXPECTED_PATCH" }
    }
    else {
        $BlockingErrors = @($RootErrors)
        Write-Result "EFFECTIVE_ROOT_SPELL_DBC_SLOT=NONE"
        Write-Result "EFFECTIVE_ROOT_SPELL_DBC_SHA256=NONE"
        if ($BlockingErrors.Count -gt 0) { $ClientGate = "UNKNOWN_UNINSPECTABLE_ROOT_SLOT" }
    }
    Write-Result "ROOT_SLOT_BLOCKING_ERRORS=$($BlockingErrors.Count)"
    Write-Result "G17R1_CLIENT_EFFECTIVE_DBC_GATE=$ClientGate"

    $Classification = "INSUFFICIENT_EVIDENCE"
    if ($MarkerHits -gt 0) {
        $Classification = "SERVER_LOCATION_GATE_PASSED_CHECK_LATER_SERVER_GATE"
    }
    elseif ($ClientGate -eq "FAIL_NO_ROOT_CUSTOM_SPELL_DBC" -or $ClientGate -eq "FAIL_EFFECTIVE_ROOT_DBC_NOT_EXPECTED_PATCH") {
        $Classification = "CLIENT_DBC_GATE_NOT_INSTALLED_OR_OVERRIDDEN"
    }
    elseif ($ClientGate -eq "PASS_EXPECTED_PATCHED_DBC") {
        $Classification = "CLIENT_PATCH_PRESENT_BUT_SERVER_LOCATION_MARKER_NOT_FOUND"
    }
    elseif ($ClientGate -like "UNKNOWN_UNINSPECTABLE*") {
        $Classification = "CLIENT_DBC_PRIORITY_UNKNOWN"
    }
    Write-Result "G17R2A_CLASSIFICATION=$Classification"
    Write-Result "G17R2A_DIAGNOSTIC_RESULT=PASS"
    Write-Result "RESULT_FILE=$Result"
    exit 0
}
catch {
    Write-Result ("G17R2A_DIAGNOSTIC_ERROR=" + $_.Exception.Message)
    Write-Result "G17R2A_DIAGNOSTIC_RESULT=FAIL"
    Write-Result "RESULT_FILE=$Result"
    exit 1
}
finally {
    if ($WorkRoot -and (Test-Path -LiteralPath $WorkRoot -PathType Container)) {
        Remove-Item -LiteralPath $WorkRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}
