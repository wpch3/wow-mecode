G17-B3R11 r1a：陆地坐骑蹬腿修复（编译错误修正版）
=================================================

r1 版真实 MSVC 编译错误（你机器上报的）：
  cs_dragonriding.cpp(1902): error C2065: "ARCHETYPE_BEAST": 未声明的标识符
  cs_dragonriding.cpp(1902): error C2065: "ARCHETYPE_GENERIC": 未声明的标识符
根因：AI 类在 G17Dragonriding 命名空间之外，UpdateFlightEmote 里的
ARCHETYPE_* 少了 G17Dragonriding:: 前缀（别处的裸用有局部 using 所以能编）。

r1a 修复：加前缀，其余与 r1 完全一致（蹬腿修复逻辑不变）。
兼容：你当前源码就是 r1（520696ee，已应用未编译）——直接装 r1a 即可，
安装器识别 r1 状态为可升级；B3R6-R10 任一状态也可直装。

附带说明：编译日志里 CustomAoELoot.cpp(161) 的 warning C4100（'player'
未引用参数）是 F45R1 静默移除播报语句后的无害残留，不是错误，可忽略。

操作：关 worldserver → 双击 01_Install_Build_G17B3R11.cmd →
      首行 G17B3R11_BUILD=c5c4c332 → PASSED → 重启。
验收：骑马/狼类上天腿不再蹬动；龙/机械/魔法不受影响。
