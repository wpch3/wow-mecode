G17-B0 Windows构建v3：ASCII + Windows PowerShell native stderr安全版

旧包状态：
- v1：UTF-8无BOM中文PS1在Windows PowerShell 5.1加载阶段ParserError，脚本主体未执行。
- v2：纯ASCII修复了解码，但全局ErrorActionPreference=Stop与native命令的“2>&1管道”组合，使CMake stderr被包装为ErrorRecord并提前进入catch。
- v2日志没有CMAKE_EXIT、MSBuild开始或MSBUILD_EXIT；MSBuild没有运行，不能算真实编译PASS或FAIL。
- v1和v2均禁止重跑。

v3修复：
- PS1与CMD继续保持逐字节纯ASCII；
- 新增Invoke-NativeLogged；
- 仅在运行native命令的局部把ErrorActionPreference改为Continue；
- 完整收集stdout/stderr后立即捕获LASTEXITCODE，再恢复全局Stop；
- 不再把CMake/MSBuild输出直接连接到ForEach管道；
- 在CMake前先用cmd.exe同时输出stdout/stderr并返回0，真实自测native runner；只有自测PASS才继续。

执行：
1. 停用并删除旧v1/v2解压目录，避免点错。
2. 确认worldserver已关闭。
3. 完整解压本ZIP到新目录。
4. 双击Run-G17B0-Windows-Build.cmd。
5. 不要启动worldserver。
6. 上传C:\Users\Administrator\Downloads\workspace\uploads\G17B0_WINDOWS_BUILD_RESULT.txt。

本包不执行SQL、不修改源码、不启动服务器、不做Runtime。
