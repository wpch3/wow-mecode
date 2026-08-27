G17-C7 战斗特效修复（客户端+服务端）
=====================================

根因（通过真实DBC对比确认）：
  3.3.5 客户端只对 SCHOOL_DAMAGE(2) 类型效果渲染法术视觉。
  DUMMY(3) 效果完全无渲染——即使 SpellVisualID 已设置。
  火球术/火息术有特效因为 Effect_1=2(SCHOOL_DAMAGE)。
  我们的25个战斗载体无特效因为 Effect_1=3(DUMMY)。

修复：
  把 990000-990024 的 Effect_1 从 DUMMY(3) 改为 SCHOOL_DAMAGE(2)。
  同时设置 ImplicitTargetA_1=18(当前目标) 确保弹道视觉。
  BasePoints_1=0（服务端脚本通过 PreventHitDefaultEffect 处理实际伤害）。

前置条件：C3v2 和 C6 已安装。

操作：
  1. 完全关闭 worldserver 和 WoW 客户端。
  2. 双击 01_Install_G17C7_Effect_Fix.cmd
     （自动：服务端DBC修补 + 客户端MPQ修补，一步到位）
  3. 看到 [G17C7] INSTALL PASSED。
  4. 启动两端，召唤坐骑，切到战斗页，按技能——应该有火息/撕咬/奥冲等特效。

回滚：双击 02_Rollback_G17C7_Effect_Fix.cmd

结果文件：
  C:\Users\Administrator\Downloads\workspace\uploads\G17C7_EFFECT_FIX_RESULT.txt
  回传给我。
