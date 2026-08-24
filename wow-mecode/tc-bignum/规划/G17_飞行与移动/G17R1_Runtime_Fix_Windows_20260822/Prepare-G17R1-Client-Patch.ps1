#requires -Version 5.1
param(
    [string]$InputSpellDbc = "D:\TC-Build\bin\RelWithDebInfo\dbc\Spell.dbc",
    [string]$ClientRoot = "",
    [ValidatePattern("^[4-9A-Z]$")][string]$PatchSlot = "Z"
)

$ErrorActionPreference = "Stop"
$Workspace = "C:\Users\Administrator\Downloads\workspace"
$UploadDir = Join-Path $Workspace "uploads"
$StageRoot = Join-Path $UploadDir "G17R1_Client_Patch_Staging"
$OutputDbc = Join-Path $StageRoot "DBFilesClient\Spell.dbc"
$PatchReport = Join-Path $UploadDir "G17R1_CLIENT_DBC_PATCH_RESULT.txt"
$RunReport = Join-Path $UploadDir "G17R1_CLIENT_PREPARE_RESULT.txt"
$Patcher = Join-Path $PSScriptRoot "tools\patch_g17r1_client_spell_dbc.py"
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)

New-Item -ItemType Directory -Path $UploadDir -Force | Out-Null
[System.IO.File]::WriteAllText($RunReport, "", $Utf8NoBom)

function Write-Result([string]$Line) {
    Write-Host $Line
    [System.IO.File]::AppendAllText($RunReport, $Line + [Environment]::NewLine, $Utf8NoBom)
}

function Invoke-NativeLogged {
    param(
        [Parameter(Mandatory=$true)][string]$FilePath,
        [Parameter(Mandatory=$true)][string[]]$NativeArgs,
        [Parameter(Mandatory=$true)][string]$Prefix
    )
    $SavedErrorActionPreference = $ErrorActionPreference
    $NativeExit = 9009
    $NativeOutput = @()
    try {
        $ErrorActionPreference = "Continue"
        $NativeOutput = @(& $FilePath @NativeArgs 2>&1)
        $NativeExit = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $SavedErrorActionPreference
    }
    foreach ($NativeLine in $NativeOutput) {
        Write-Result ($Prefix + "|" + $NativeLine.ToString())
    }
    return [int]$NativeExit
}

try {
    Write-Result "G17R1_CLIENT_PREPARE_START"
    Write-Result "INPUT_SPELL_DBC=$InputSpellDbc"
    Write-Result "STAGE_OUTPUT=$OutputDbc"
    Write-Result "PATCH_SLOT=$PatchSlot"

    if (Get-Process Wow -ErrorAction SilentlyContinue) {
        throw "Wow.exe is running; close the client before preparing or installing a DBC patch"
    }
    foreach ($Path in @($InputSpellDbc, $Patcher)) {
        if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
            throw "required file missing: $Path"
        }
    }

    $PythonCandidates = @(
        (Join-Path $env:LOCALAPPDATA "Programs\Python\Python312\python.exe"),
        (Join-Path $env:LOCALAPPDATA "Programs\Python\Python310\python.exe")
    )
    $Python = @($PythonCandidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf })[0]
    if (-not $Python) {
        $PythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
        if (-not $PythonCommand) { $PythonCommand = Get-Command python -ErrorAction SilentlyContinue }
        if ($PythonCommand -and $PythonCommand.Source -notmatch "\\WindowsApps\\") {
            $Python = $PythonCommand.Source
        }
    }
    if (-not $Python) { throw "Python 3.12/3.10 was not found; WindowsApps aliases are rejected" }
    $PrefixArgs = @()
    Write-Result "PYTHON=$Python"

    $PatchArgs = @($PrefixArgs + @($Patcher, "patch", "--input", $InputSpellDbc, "--output", $OutputDbc, "--report", $PatchReport))
    $PatchExit = Invoke-NativeLogged -FilePath $Python -NativeArgs $PatchArgs -Prefix "DBC_PATCH"
    Write-Result "DBC_PATCH_EXIT=$PatchExit"
    if ($PatchExit -ne 0) { throw "client Spell.dbc patch failed; exit code=$PatchExit" }

    $VerifyArgs = @($PrefixArgs + @($Patcher, "verify", "--original", $InputSpellDbc, "--patched", $OutputDbc))
    $VerifyExit = Invoke-NativeLogged -FilePath $Python -NativeArgs $VerifyArgs -Prefix "DBC_VERIFY"
    Write-Result "DBC_VERIFY_EXIT=$VerifyExit"
    if ($VerifyExit -ne 0) { throw "client Spell.dbc verification failed; exit code=$VerifyExit" }

    $InputHash = (Get-FileHash -LiteralPath $InputSpellDbc -Algorithm SHA256).Hash.ToLowerInvariant()
    $OutputHash = (Get-FileHash -LiteralPath $OutputDbc -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-Result "INPUT_SHA256=$InputHash"
    Write-Result "OUTPUT_SHA256=$OutputHash"
    if ($InputHash -ceq $OutputHash) { throw "patched client DBC did not change" }
    Write-Result "SERVER_DBC_MODIFIED=False"
    Write-Result "G17R1_CLIENT_DBC_STAGE=PASS"

    if ($ClientRoot) {
        $ClientRoot = (Resolve-Path -LiteralPath $ClientRoot).Path
        $WowExe = Join-Path $ClientRoot "Wow.exe"
        $DataDir = Join-Path $ClientRoot "Data"
        if (-not (Test-Path -LiteralPath $WowExe -PathType Leaf)) { throw "Wow.exe missing: $WowExe" }
        if (-not (Test-Path -LiteralPath $DataDir -PathType Container)) { throw "Data directory missing: $DataDir" }

        $PatchRoot = Join-Path $DataDir ("patch-" + $PatchSlot + ".MPQ")
        if (Test-Path -LiteralPath $PatchRoot -PathType Leaf) {
            throw "patch slot is an existing packed MPQ and cannot be edited safely: $PatchRoot"
        }
        $ClientDbc = Join-Path $PatchRoot "DBFilesClient\Spell.dbc"
        if (Test-Path -LiteralPath $ClientDbc -PathType Leaf) {
            $ExistingHash = (Get-FileHash -LiteralPath $ClientDbc -Algorithm SHA256).Hash.ToLowerInvariant()
            if ($ExistingHash -cne $OutputHash) {
                throw "refusing to overwrite a different existing client Spell.dbc: $ClientDbc"
            }
            Write-Result "G17R1_CLIENT_LOOSE_PATCH=ALREADY_CURRENT"
        }
        else {
            New-Item -ItemType Directory -Path (Split-Path -Parent $ClientDbc) -Force | Out-Null
            $Temporary = $ClientDbc + ".g17r1.tmp"
            Copy-Item -LiteralPath $OutputDbc -Destination $Temporary -Force
            if ((Get-FileHash -LiteralPath $Temporary -Algorithm SHA256).Hash.ToLowerInvariant() -cne $OutputHash) {
                Remove-Item -LiteralPath $Temporary -Force -ErrorAction SilentlyContinue
                throw "temporary client DBC copy verification failed"
            }
            Move-Item -LiteralPath $Temporary -Destination $ClientDbc
            Write-Result "G17R1_CLIENT_LOOSE_PATCH=CREATED"
        }
        Write-Result "CLIENT_PATCH_ROOT=$PatchRoot"
        Write-Result "CLIENT_SPELL_DBC=$ClientDbc"
        Write-Result "CLIENT_CACHE_MUST_BE_DELETED=True"
        Write-Result "LOOSE_PATCH_LOADER_SUPPORT_MUST_BE_RUNTIME_VERIFIED=True"
    }
    else {
        Write-Result "G17R1_CLIENT_LOOSE_PATCH=NOT_INSTALLED_NO_CLIENT_ROOT"
    }

    Write-Result "G17R1_CLIENT_PREPARE_RESULT=PASS"
    Write-Result "RESULT_FILE=$RunReport"
    exit 0
}
catch {
    Write-Result ("G17R1_CLIENT_PREPARE_ERROR=" + $_.Exception.Message)
    Write-Result "G17R1_CLIENT_PREPARE_RESULT=FAIL"
    Write-Result "RESULT_FILE=$RunReport"
    exit 1
}
