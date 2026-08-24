# G17-B1 Windows安装器原生运行器卡死（2026-08-23）

用户运行旧`01_Install_Build_G17B1.cmd`后，结果停在：

```text
G17B1_WINDOWS_BUILD_START
SCOPE=G17B1_ALL_OWNED_DIRECT_MOUNT_AUTO_INTERCEPT_AND_TYPED_SESSION
RUNS_SQL=False
MODIFIES_CLIENT=False
R5_CLIENT_STATE_MODIFIED=False
SOURCE_SHA256_BEFORE=10a7002db4c173e441836870da01dd3009a7ba470369ca1a89bdb399ee9b2f45
PYTHON=C:\Users\Administrator\AppData\Local\Programs\Python\Python312\python.exe
```

没有出现`NATIVE_SELFTEST`或`SOURCE_APPLY`输出。

## 根因

旧脚本把原生参数形参命名为`$Args`：

```powershell
function Native([string]$File,[string[]]$Args,[string]$Prefix)
```

PowerShell的`$args`是自动变量，名称大小写不敏感；该命名与自动变量冲突，`& $File @Args`可能没有向`cmd.exe`传入预期`/d /c ...`参数。结果是`cmd.exe`以交互方式启动并等待输入，表现为永久卡在Python路径之后。

## 写入边界

原生自测位于Python source apply之前；日志又确认当前源码仍是R1前像SHA `10a7002...9b2f45`。因此本次卡死发生在源码写入、MSBuild、数据库和客户端操作之前：

```text
G17B1_HANG_SOURCE_EDITS=0
G17B1_HANG_BUILD_STARTED=False
G17B1_HANG_SQL_WRITES=0
G17B1_HANG_CLIENT_WRITES=0
```

用户应按Ctrl+C或关闭旧CMD窗口，不需要回滚源码，也不得重跑旧包。

## 修复

G17-B1R1将形参改为`$NativeArgs`，全部调用改为显式命名参数，并复用此前R2真实Windows已通过的native stderr安全捕获结构；加入PowerShell真实native stdout/stderr/exit语义测试。
