#requires -Version 5.1
# G17-C10 rollback: restore the pre-C10 root MPQ + locale mirror from the
# backup recorded in G17C11_CLIENTMOD_STATE.txt (or the newest
# G17C11_Client_Backup_* directory in uploads), then clear the client cache.
param(
    [string]$Workspace = "C:\Users\Administrator\Downloads\workspace",
    [string]$ClientRoot = "D:\WOW"
)
$ErrorActionPreference = "Stop"
$UploadDir = Join-Path $Workspace "uploads"
$Result = Join-Path $UploadDir "G17C11_ROLLBACK_RESULT.txt"
$StateFile = Join-Path $UploadDir "G17C11_CLIENTMOD_STATE.txt"
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)

New-Item -ItemType Directory -Path $UploadDir -Force | Out-Null
[IO.File]::WriteAllText($Result, "", $Utf8NoBom)
function W([string]$Text) { Write-Host $Text; [IO.File]::AppendAllText($Result, $Text + [Environment]::NewLine, $Utf8NoBom) }
function Read-KeyValueFile([string]$Path) {
    $Values = @{}
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $Values }
    foreach ($Line in [IO.File]::ReadAllLines($Path)) { if ($Line -match '^([^=]+)=(.*)$') { $Values[$Matches[1]] = $Matches[2] } }
    return $Values
}

try {
    W "G17C11_ROLLBACK_START"
    if (Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Name -match '^(?i:wow|wow-64)$' }) {
        throw "Wow client is running; close it before rollback"
    }

    $State = Read-KeyValueFile $StateFile
    $BackupDir = $State["BACKUP_DIR"]
    if (-not $BackupDir -or -not (Test-Path -LiteralPath $BackupDir -PathType Container)) {
        # fallback: newest G17C11_Client_Backup_* directory
        $candidates = @(Get-ChildItem -LiteralPath $UploadDir -Directory -Filter "G17C11_Client_Backup_*" -ErrorAction SilentlyContinue | Sort-Object Name -Descending)
        if ($candidates.Count -ge 1) { $BackupDir = $candidates[0].FullName }
        W ("BACKUP_DIR_FALLBACK=" + ($BackupDir -ne $State["BACKUP_DIR"]))
    }
    if (-not $BackupDir -or -not (Test-Path -LiteralPath $BackupDir -PathType Container)) {
        throw "no G17C9 backup directory found; nothing to roll back"
    }
    W "BACKUP_DIR=$BackupDir"

    $BackupRoot = Join-Path $BackupDir "before_G17C11_root.MPQ"
    $BackupLocale = Join-Path $BackupDir "before_G17C11_locale.MPQ"
    if (-not (Test-Path -LiteralPath $BackupRoot -PathType Leaf)) { throw "backup root MPQ missing: $BackupRoot" }
    if (-not (Test-Path -LiteralPath $BackupLocale -PathType Leaf)) { throw "backup locale MPQ missing: $BackupLocale" }
    $BackupRootHash = (Get-FileHash -LiteralPath $BackupRoot -Algorithm SHA256).Hash.ToLowerInvariant()
    $BackupLocaleHash = (Get-FileHash -LiteralPath $BackupLocale -Algorithm SHA256).Hash.ToLowerInvariant()
    W "BACKUP_ROOT_SHA256=$BackupRootHash"
    W "BACKUP_LOCALE_SHA256=$BackupLocaleHash"

    # restore targets: from state file, else discover via locale-mirror scan
    $RootMpq = $State["ROOT_MPQ"]
    $LocaleMpq = $State["LOCALE_MPQ"]
    if (-not $RootMpq -or -not (Test-Path -LiteralPath $RootMpq -PathType Leaf)) {
        $DataDir = Join-Path $ClientRoot "Data"
        if (-not (Test-Path -LiteralPath $DataDir -PathType Container)) { throw "client Data directory missing and state ROOT_MPQ invalid" }
        $Slots = @("Z","Y","X","W","V","U","T","S","R","Q","P","O","N","M","L","K","J","I","H","G","F","E","D","C","B","A")
        $found = @()
        foreach ($s in $Slots) {
            $cand = Join-Path $DataDir ("patch-" + $s + ".MPQ")
            if (Test-Path -LiteralPath $cand -PathType Leaf) { $found += $cand }
        }
        if ($found.Count -ne 1) { throw "cannot determine root patch MPQ (found $($found.Count) candidates); run with explicit state" }
        $RootMpq = $found[0]
        $RootHash = (Get-FileHash -LiteralPath $RootMpq -Algorithm SHA256).Hash.ToLowerInvariant()
        $LocaleDir = Join-Path $DataDir "zhCN"
        $LocaleMpq = $null
        foreach ($s in $Slots) {
            $cand = Join-Path $LocaleDir ("patch-zhCN-" + $s + ".MPQ")
            if (Test-Path -LiteralPath $cand -PathType Leaf) {
                $h = (Get-FileHash -LiteralPath $cand -Algorithm SHA256).Hash.ToLowerInvariant()
                if ($h -ceq $RootHash) { $LocaleMpq = $cand; break }
            }
        }
        if (-not $LocaleMpq) { throw "no locale mirror found for rollback" }
    }
    W "RESTORE_ROOT=$RootMpq"
    W "RESTORE_LOCALE=$LocaleMpq"

    # restore root via temp swap
    $Tmp = $RootMpq + ".g17c11rb.tmp"
    if (Test-Path -LiteralPath $Tmp -PathType Leaf) { Remove-Item -LiteralPath $Tmp -Force -ErrorAction SilentlyContinue }
    Copy-Item -LiteralPath $BackupRoot -Destination $Tmp
    if ((Get-FileHash -LiteralPath $Tmp -Algorithm SHA256).Hash.ToLowerInvariant() -cne $BackupRootHash) { throw "restore temp root hash mismatch" }
    Move-Item -LiteralPath $Tmp -Destination $RootMpq -Force
    if ((Get-FileHash -LiteralPath $RootMpq -Algorithm SHA256).Hash.ToLowerInvariant() -cne $BackupRootHash) { throw "restored root hash mismatch" }
    W "ROOT_RESTORED=True"

    # restore locale mirror
    $TmpL = $LocaleMpq + ".g17c11rb.tmp"
    if (Test-Path -LiteralPath $TmpL -PathType Leaf) { Remove-Item -LiteralPath $TmpL -Force -ErrorAction SilentlyContinue }
    Copy-Item -LiteralPath $BackupLocale -Destination $TmpL
    if ((Get-FileHash -LiteralPath $TmpL -Algorithm SHA256).Hash.ToLowerInvariant() -cne $BackupLocaleHash) { throw "restore temp locale hash mismatch" }
    Move-Item -LiteralPath $TmpL -Destination $LocaleMpq -Force
    if ((Get-FileHash -LiteralPath $LocaleMpq -Algorithm SHA256).Hash.ToLowerInvariant() -cne $BackupLocaleHash) { throw "restored locale hash mismatch" }
    W "LOCALE_RESTORED=True"

    $CacheDir = Join-Path $ClientRoot "Cache"
    if (Test-Path -LiteralPath $CacheDir -PathType Container) {
        Remove-Item -LiteralPath $CacheDir -Recurse -Force
        W "CLIENT_CACHE_REMOVED=True"
    } else { W "CLIENT_CACHE_REMOVED=False" }

    # mark state rolled back
    if (Test-Path -LiteralPath $StateFile -PathType Leaf) {
        $State["INSTALL_STATUS"] = "ROLLBACK"
        $Lines = @()
        foreach ($k in @("STATE_FORMAT","BUILD","INSTALL_STATUS","CLIENT_ROOT","ROOT_MPQ","PATCH_SLOT","LOCALE_MPQ","OLD_SPELL_DBC_SHA256","NEW_SPELL_DBC_SHA256","NEW_MPQ_SHA256","BACKUP_DIR","INSTALLED_AT")) {
            if ($State.ContainsKey($k) -and $State[$k]) { $Lines += ($k + "=" + $State[$k]) }
        }
        $Lines += ("ROLLED_BACK_AT=" + (Get-Date).ToString("o"))
        [IO.File]::WriteAllText($StateFile, (($Lines -join [Environment]::NewLine) + [Environment]::NewLine), $Utf8NoBom)
    }

    W "CLIENT_RESTART_REQUIRED=True"
    W "G17C11_ROLLBACK_RESULT=PASS"
    W "RESULT_FILE=$Result"
    exit 0
} catch {
    W ("G17C11_ROLLBACK_ERROR=" + $_.Exception.Message)
    W "G17C11_ROLLBACK_RESULT=FAIL"
    W "RESULT_FILE=$Result"
    exit 1
}
