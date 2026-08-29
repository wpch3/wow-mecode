G17-B3R7 服务端：7 槽载具技能页 ＋ 冷却 UI 封包
================================================

两个修复（源码-only，无 SQL/DBC/插件步骤）：

1. 【更多格数·第一步】两个技能页都用满 7 个 m_spells 槽：
   移动页 = 拉升/俯冲/推进/冲刺/着陆/制动/切页
   战斗页 = 5 类型技能/制动/切页
   制动(990028)回到动作条（原 B3R6 注释预留的"future client-UI extension"）。
   默认载具条仍显示前 6 格；第 7 格由 G17DragonBar 专用条显示（装 G17-C10
   里的 addon\G17DragonBar 即可）。m_spells[7] 仍为 0（MAX_CREATURE_SPELLS=8）。

2. 【冷却 UI 小 bug 修复·服务端半边】施法成功后向骑手客户端发送
   SMSG_SPELL_COOLDOWN（龙 GUID + 玩家 GUID 双变体，复用核心
   SpellHistory::BuildCooldownPacket，封包格式零漂移）。
   根因：C9 清零 DBC RecoveryTime 后客户端没有本地冷却预测，而载具生物
   施法永远不会走进 SpellHistory::StartCooldown 的玩家会话发包分支。
   （客户端半边由 G17DragonBar 的施法成功监听兜底，两边互为保险。）

前置：当前源码为 B3R6（3fdb46e8）或任一历史谱系镜像（安装器自动识别；
未知现场状态零写入拒绝）。
操作：关闭 worldserver → 双击 01_Install_Build_G17B3R7.cmd →
      首行显示 G17B3R7_BUILD=f2360d7e → PASSED → 重启 worldserver。
结果文件：uploads\G17B3R7_WINDOWS_BUILD_RESULT.txt
回滚：双击 02_Rollback_G17B3R7.cmd（回滚到 B3R6 3fdb46e8）。

游戏内验收：
  1. 装了 G17DragonBar：上龙看到 7 个技能（含制动），默认条 6 个
  2. 任一战斗技能施放成功 → 按钮出现冷却圈（4/6/20/10/60 秒按槽位）
  3. 切页按钮 → 1 秒冷却圈
  4. 未装 G17DragonBar：冷却圈也应出现（服务端封包路径）
