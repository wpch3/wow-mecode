G17-B0 完整Windows编译包（2026-08-22）

已通过且禁止重复：
- Windows Source Apply
- live world前像
- world v3安装与独立只读后检：G17B0_WORLD_CHECK_PASS

本包只做：
- 确认worldserver未运行；
- 锁定D:\TrinityCore内loader和cs_dragonriding.cpp的精确SHA；
- 对D:\TC-Build显式执行CMake重新生成，确保新cpp进入vcxproj；
- 验证vcxproj确实包含cs_dragonriding.cpp；
- 使用Visual Studio MSBuild构建worldserver，配置RelWithDebInfo、平台x64；
- 验证本轮新dragonriding obj、worldserver.exe/PDB时间前进及exe哈希变化；
- 输出一个结果TXT。

本包不会：
- 不改源码；
- 不执行SQL；
- 不启动worldserver/authserver；
- 不reload；
- 不执行游戏Runtime验收。

执行前：
1. 正常关闭worldserver，确认它不在运行。
2. 保持源码D:\TrinityCore和构建目录D:\TC-Build不手工改动。
3. 完整解压本ZIP。

执行：
- 双击 Run-G17B0-Windows-Build.cmd
- 等待窗口明确PASS或FAIL，不要中途关闭。

结果文件：
C:\Users\Administrator\Downloads\workspace\uploads\G17B0_WINDOWS_BUILD_RESULT.txt

成功末尾必须包含：
G17B0_WINDOWS_BUILD_PASS=True
G17B0_WINDOWS_BUILD_RESULT=PASS
G17B0_WINDOWS_BUILD_COMPLETE
STOP_DO_NOT_START_WORLDSERVER

无论PASS或FAIL，都上传该TXT。FAIL时不要启动worldserver；PASS后也先不要启动，必须等结果验收。
