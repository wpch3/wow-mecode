# G17-B1R1 PowerShell原生参数卡死修复交付（2026-08-23）

## 真实失败边界

用户旧B1输出停在`PYTHON=...Python312\python.exe`之后，未出现`NATIVE_SELFTEST`、`SOURCE_APPLY`或`MSBUILD`。`SOURCE_SHA256_BEFORE`仍为R1前像：

`10a7002db4c173e441836870da01dd3009a7ba470369ca1a89bdb399ee9b2f45`

因此旧运行没有进入Python source apply或MSBuild；源码写入、构建、SQL和客户端写入均为0。无需源码回滚。

## 根因与修复

旧函数使用`[string[]]$Args`，与PowerShell大小写不敏感的自动变量`$args`冲突。真实PowerShell探针显示传入三个原生参数后函数内部`$Args.Count=0`；Windows上的`cmd.exe`可能因此无`/d /c`启动并等待交互输入。

B1R1安装与回滚改为：

- `[string[]]$NativeArgs`；
- 原生展开使用`@NativeArgs`；
- 所有调用使用`-FilePath/-NativeArgs/-Prefix`显式命名绑定；
- Python优先直接使用Python312/Python310，不依赖失效的`py.exe`；
- 接受R1前像或B1后像，保持exact-hash与幂等生命周期。

## 离线验证

- 安装器与回滚器PowerShell AST：PASS；
- 真实native stdout/stderr/exit code：PASS；
- 含空格参数与含引号参数保持：PASS；
- B1 8项源码测试：PASS；
- source check/apply/幂等apply/rollback：PASS；
- 包级自测：PASS；
- Windows脚本CRLF/无BOM：PASS；
- ZIP路径安全、CRC、14项内文件SHA、解压后包级自测和双AST：PASS；
- 确定性重建ZIP逐字节SHA一致：PASS。

## 唯一新交付

- ZIP：`G17B1R1_Native_Runner_Fix_Windows_20260823.zip`
- 大小：`32052` bytes
- 文件数：`15`
- SHA-256：`c94394177b188843c7ed79a989fe46f7397233bf9103d2fc622ecf79f4c77cc8`
- 安装入口：`01_Install_Build_G17B1R1.cmd`
- 结果：`C:\Users\Administrator\Downloads\workspace\uploads\G17B1R1_WINDOWS_BUILD_RESULT.txt`

旧`G17B1_All_Mounts_Auto_Intercept_Windows_20260823.zip`和旧`01_Install_Build_G17B1.cmd`永久废弃，禁止重跑。

## 用户当前唯一动作

1. 若旧窗口仍卡住，按Ctrl+C；无响应直接关闭。
2. 不运行旧回滚，不重复R1–R5。
3. 保持R5客户端Y槽不动并正常停止worldserver。
4. 校验新ZIP SHA后解压，运行`01_Install_Build_G17B1R1.cmd`。
5. 回传新结果文件。Windows构建和B1游戏Runtime在回传前仍为PENDING，不得提前标PASS。
