G17-B0 Windows构建脚本v2 ASCII修正版（2026-08-22）

旧包问题：
- 旧Build-G17B0-Windows.ps1是UTF-8无BOM，并含中文字符串。
- Windows PowerShell 5.1按本机ANSI代码页读取后出现乱码和ParserError。
- 错误发生在脚本解析阶段，旧脚本主体没有开始，CMake/MSBuild都没有运行。
- 旧包G17B0_Windows_Build_20260822.zip禁止重跑。

本修正版：
- Build-G17B0-Windows.ps1和Run-G17B0-Windows-Build.cmd均为纯ASCII字节；
- 离线测试强制检查两个可执行脚本的每个字节都小于0x80；
- 不再依赖BOM或Windows系统代码页；
- 构建逻辑、源码SHA门、CMake成员门、fresh obj门和不启动服务器边界保持不变。

执行：
1. 确认worldserver已经正常关闭。
2. 完整解压本ZIP。
3. 双击Run-G17B0-Windows-Build.cmd。
4. 等待窗口完成，不要中途关闭。
5. 不要启动worldserver。
6. 上传：
   C:\Users\Administrator\Downloads\workspace\uploads\G17B0_WINDOWS_BUILD_RESULT.txt

本轮不要执行SQL、不要运行旧构建包、不要启动服务器或做Runtime。
