G17-B3R9 服务端：自定义御龙 UI 的全玩家施法支持
================================================

目的：配合新的客户端自定义 UI（G17DragonRide），所有御龙技能都能由玩家
按钮直接施放（CastSpellByID 安全路径），彻底摆脱原版 6 格载具条限制。

改动（源码-only，无 SQL/DBC 文件改动）：
  1. 三个真实移动技能（55215 推进 / 52197 冲刺 / 52226 着陆）的脚本改用
     ResolveDragonFromCaster——玩家施法同样触发龙的 AI 动作并扣龙能量
  2. CheckEnergyCast 双施法者能量门（玩家路径也不能白嫖能量）
  3. 启动时给 55215/52197 运行时补 CASTABLE_WHILE_MOUNTED 属性
     （沿用 B2R3 已验证的 const_cast 手法，磁盘 DBC 不动；52226 本来就有）
  4. 战斗技能与 4 个 G17 载体本就支持玩家施法（B3R3 双施法路径）

兼容：B3R6/B3R7/B3R8 任一源码状态可直接安装（14 镜像白名单）。
操作：关 worldserver → 双击 01_Install_Build_G17B3R9.cmd →
      首行显示 G17B3R9_BUILD=f0564c5a → PASSED → 重启 worldserver。
结果文件：uploads\G17B3R9_WINDOWS_BUILD_RESULT.txt
回滚：02_Rollback_G17B3R9.cmd（回 B3R6）。
配套客户端：G17DragonRide_FINAL.zip（全新自定义御龙界面）。
