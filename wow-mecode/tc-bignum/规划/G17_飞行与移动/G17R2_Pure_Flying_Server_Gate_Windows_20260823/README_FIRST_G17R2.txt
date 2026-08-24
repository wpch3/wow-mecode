G17-R2 纯飞行坐骑严格区域门修复（Windows，2026-08-23）
========================================================

用途
----
修复红色始祖幼龙（59961）等纯飞行坐骑在湿地等旧世界安全户外仍提示“这里无法召唤坐骑”的确定性服务端错误。

R1 的异步入座、载具控制权、4 个载具技能已由真实运行验收通过。本包不会重复 R1，不执行 SQL，也不安装或覆盖客户端 MPQ。

唯一执行入口
------------
1. 正常停止 worldserver。
2. 确认本目录位于本机任意可读位置；工作区仍使用：
   C:\Users\Administrator\Downloads\workspace
3. 双击：Run-G17R2-Windows-Fix.cmd
4. 看到 G17R2_WINDOWS_BUILD_RESULT=PASS 后，不要再次执行旧 R1 包。
5. 将以下小文件交回：
   C:\Users\Administrator\Downloads\workspace\uploads\G17R2_WINDOWS_FIX_RESULT.txt

脚本固定使用：
- 源码：D:\TrinityCore
- 构建：D:\TC-Build
- 配置：RelWithDebInfo / x64
- Python：优先 Python 3.12，其次 3.10；不调用 py.exe，不接受 WindowsApps 别名。

本包的严格门禁
--------------
- G17-R1 的 cs_dragonriding.cpp SHA 必须保持为：
  10a7002db4c173e441836870da01dd3009a7ba470369ca1a89bdb399ee9b2f45
- SpellInfo.cpp 前镜像 SHA：
  537e5c350baa5f4a90bd0ec38c6b6858360e287aeabd75ab54050b4432e50755
- SpellInfo.cpp 后镜像 SHA：
  73d52ac0feb67a32822fc0bf086a9174ba7ef0bc186223cdc8a690f48fccb9e2
- 安装器先备份、原子写入、写后复验；未知源码拒绝覆盖。
- 构建后必须出现 fresh SpellInfo object、新 worldserver.exe/PDB；首次应用时 EXE SHA 必须变化。
- worldserver 运行时拒绝执行，构建完成也不会替你启动服务器。

修复内容
--------
旧严格分支虽已算出 g17OldWorldAllowed=true，仍无条件调用
player->CanFlyInZone(...)；旧世界会返回 false，最终仍得到
SPELL_FAILED_INCORRECT_AREA。

R2 让 G17 旧世界安全策略在成立时同时替代原版的 IsFlyable 与
CanFlyInZone 两个区域谓词。G17 关闭时、外域/诺森德时仍走原版规则。
室内、城市、禁飞区、竞技场、实例地图和配置黑名单边界没有被移除。

PASS 后的最小运行复测
--------------------
1. 确认真实 worldserver 配置仍有 WorldFlight.Enable=1、WorldFlight.AllowOldWorld=1。
2. 正常启动新 worldserver.exe。
3. 在湿地安全户外使用红色始祖幼龙（法术 59961）。
4. 验收：可以召唤、离地、水平移动、降落。
5. 服务端预期日志包含：
   G17R2 old-world pure-flight location allowed: spell=59961

只需回复两项即可：
- G17R2_WINDOWS_BUILD_RESULT=PASS/FAIL
- 59961 在湿地：可召唤/仍无法召唤（若仍失败，再附当时服务端相关日志）

若仍失败
--------
不要重复 R1 或 R2，不要重做 SQL。保留结果文件，并补交之前客户端安装器生成的：
G17R1_CLIENT_MPQ_INSTALL_RESULT.txt
客户端 DBC 门和本次服务端严格门是两个独立门，必须分开取证。

回滚（仅在明确要求时）
----------------------
使用本包 tools\apply_g17r2_source.py 的 rollback 模式，源码根为 D:\TrinityCore。
安装器只接受本包锁定的后镜像及已验证备份，拒绝回滚未知文件。
