# G23-P0R 真实AoE Loot路径与Eluna扩展补充取证

更新时间：2026-08-22  
状态：Windows P0R已PASS并关闭；报告`g23_p0r_20260822_003402.txt`、SHA=`7eaecd05...f8c8b`、修改计数0；禁止重跑  
当前后续：`06-G23-P0R结论与最小复现矩阵.md`  
原因：首次报告证明真实模块位于`src/server/game/Custom`，P0R已补齐真实源码、`.ext`和最新日志

## 0. 现场结果

```text
G23_P0R_CAPTURE_PASS=True
G23_P0R_SOURCE_EDIT_COUNT=0
G23_P0R_CONFIG_EDIT_COUNT=0
G23_P0R_SCRIPT_EDIT_COUNT=0
G23_P0R_DATABASE_EDIT_COUNT=0
active PID=2472
exe sha256=07a8f95259a6d46ab5d46720e5b919fc5cc60f7e81086b81e2748e7d1e1a86c1
```

完整报告已归档。以下命令只作历史审计，当前禁止重跑。

## 1. 首次执行文件（已执行）

把下面脚本下载/复制到：

```text
C:\Users\Administrator\Downloads\workspace\uploads\probe_g23_p0r_actual_paths.py
```

仓库源文件：

```text
tc-bignum\规划\G23_Lua功能与自动拾取可靠性\probe_g23_p0r_actual_paths.py
```

锁定SHA-256：

```text
b9252c68a9733b935ec44a0ffa56509537cfc49baffe0d2536d9eb7f1339fdd0
```

## 2. Windows PowerShell 5.1原子块

保持当前worldserver运行。不要执行旧`probe_g23_baseline.py`。只执行下面的新P0R脚本：

```powershell
& {
    $ErrorActionPreference = "Stop"
    $Python = "C:\Users\Administrator\AppData\Local\Programs\Python\Python312\python.exe"
    $Workspace = "C:\Users\Administrator\Downloads\workspace"
    $Uploads = Join-Path $Workspace "uploads"
    $Probe = Join-Path $Uploads "probe_g23_p0r_actual_paths.py"
    $ExpectedProbe = "b9252c68a9733b935ec44a0ffa56509537cfc49baffe0d2536d9eb7f1339fdd0"
    $Source = "D:\TrinityCore"
    $RunDir = "D:\TC-Build\bin\RelWithDebInfo"
    $Exe = Join-Path $RunDir "worldserver.exe"
    $ExpectedExe = "07a8f95259a6d46ab5d46720e5b919fc5cc60f7e81086b81e2748e7d1e1a86c1"
    $Out = Join-Path $Uploads ("g23_p0r_" + (Get-Date -Format "yyyyMMdd_HHmmss") + ".txt")

    foreach ($Path in @($Python, $Probe, $Exe)) {
        if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
            throw "找不到必需文件：$Path"
        }
    }
    foreach ($Path in @($Source, $RunDir)) {
        if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
            throw "找不到必需目录：$Path"
        }
    }

    $ProbeHash = (Get-FileHash -LiteralPath $Probe -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($ProbeHash -ne $ExpectedProbe) {
        throw "G23-P0R探针SHA不符：实际=$ProbeHash，预期=$ExpectedProbe"
    }

    $Processes = @(Get-CimInstance Win32_Process -Filter "Name='worldserver.exe'")
    if ($Processes.Count -ne 1) {
        throw "活动worldserver必须恰好1个，实际=$($Processes.Count)"
    }
    $ExeHash = (Get-FileHash -LiteralPath $Exe -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($ExeHash -ne $ExpectedExe) {
        throw "运行目录exe不是锁定F44R1二进制：实际=$ExeHash"
    }
    $ProcessPath = [string]$Processes[0].ExecutablePath
    if (-not $ProcessPath -or -not [System.IO.Path]::GetFullPath($ProcessPath).Equals(
        [System.IO.Path]::GetFullPath($Exe), [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "活动worldserver路径不等于RunDir：PID=$($Processes[0].ProcessId) PATH=$ProcessPath"
    }
    Write-Host "[OK] active PID=$($Processes[0].ProcessId); exe sha256=$ExeHash"

    & $Python $Probe --self-test
    if ($LASTEXITCODE -ne 0) {
        throw "G23-P0R self-test失败，退出码=$LASTEXITCODE"
    }

    & $Python $Probe $Source $RunDir $Out
    $ProbeExit = $LASTEXITCODE
    if (-not (Test-Path -LiteralPath $Out -PathType Leaf)) {
        throw "G23-P0R报告未生成：$Out"
    }

    $Report = Get-Content -LiteralPath $Out -Raw -Encoding UTF8
    foreach ($Marker in @(
        "G23_P0R_SCHEMA=2",
        "G23_P0R_LOOT_HANDLER_FOUND=True",
        "G23_P0R_AOE_CPP_CANDIDATE_COUNT=1",
        "G23_P0R_AOE_H_CANDIDATE_COUNT=1",
        "G23_P0R_AOE_SOURCE_PAIR_READY=True",
        "G23_P0R_AOE_HOOK_READY=True",
        "G23_P0R_SCRIPT_ROOT_READY=True",
        "G23_P0R_ELUNA_LOADER_FILE_COUNT=1",
        "G23_P0R_SOURCE_EDIT_COUNT=0",
        "G23_P0R_CONFIG_EDIT_COUNT=0",
        "G23_P0R_SCRIPT_EDIT_COUNT=0",
        "G23_P0R_DATABASE_EDIT_COUNT=0",
        "G23_P0R_CAPTURE_PASS=True"
    )) {
        if (-not $Report.Contains($Marker)) {
            throw "G23-P0R报告缺少门槛：$Marker；保留报告并停止，不要重跑"
        }
    }
    if ($ProbeExit -ne 0) {
        throw "G23-P0R退出码=$ProbeExit；虽然报告存在，但必须停止并回传，不要重跑"
    }

    $ReportHash = (Get-FileHash -LiteralPath $Out -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-Host "[OK] G23_P0R_WINDOWS_CAPTURE_PASS=True"
    Write-Host "[OK] report sha256=$ReportHash"
    Write-Host "[OK] report path=$Out"
    Write-Host "[STOP] 到此为止；只回传这份g23_p0r时间戳报告，不要复现、Apply、reload或编译。"
}
```

## 3. 成功标志

```text
[OK] G23-P0R actual Custom path + .lua/.ext positive fixture passed.
[OK] Newest-log-tail fixture retained the latest sentinel.
[OK] Duplicate-source negative fixture did not false-pass.
[OK] G23-P0R read-only self-test passed.
[OK] G23_P0R_CAPTURE_PASS=True
[OK] G23_P0R_WINDOWS_CAPTURE_PASS=True
```

只回传生成的：

```text
C:\Users\Administrator\Downloads\workspace\uploads\g23_p0r_时间戳.txt
```

若任何门槛红字，报告仍会保留；直接回传该报告，禁止再次运行。
