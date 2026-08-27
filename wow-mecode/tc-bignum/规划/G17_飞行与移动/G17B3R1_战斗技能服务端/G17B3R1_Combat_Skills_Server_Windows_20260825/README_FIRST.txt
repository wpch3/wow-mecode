G17-B3R1 服务端战斗技能 + 离开载具飞行清理修复（FIX6 / f3_decl_order）
==========================================================================

本包做两件事（一个服务端包，含源码替换 + 服务端 DBC 追加 + world SQL + 重编译）：

 1) 修复你发现的 bug：直接点“离开载具”（不按技能4降落）后，
    飞行效果/飞行状态没有消失。现在任何退出路径（点离开、死亡、
    禁飞清理、换图）都会完整归一玩家：取消飞行/重力/悬停、重算全部
    速度、清除移动状态。
 2) B3-R1 战斗系统（配合你已装好的 C2 客户端技能实体）：
    - 25 个战斗技能（990000-990024，龙/兽/魔法/机械/通用各5）
      进入载具时自动出现在玩家法术书（按坐骑类型只给对应的5个），
      退出载具自动移除；
    - 每个技能真实有效：伤害/治疗+净化/打断+短眩晕/终结爆发，
      消耗龙能量、带冷却、伤害归属=玩家、PvE/PvP区分（战场/竞技场禁用）；
    - 移动页（载具动作条）与战斗页（玩家法术书）独立、可往返切换。

FIX6 修复了什么（相对你上次那个 FAIL 的包）：
  - 上次（f2_lineage_upgrade）谱系门已经放行：源码成功从 1a96b72e
    升级到 ecd307b4、DBC 识别为已追加、SQL 0/25/0 PASS，但 MSBuild
    在编译 ecd307b4 时报了 5 个真实 C++ 错误：
      1) C3861  RevokeCombatSkills 前向声明放在了首次调用之后
      2) C2061/C2660/C2143/C2059  CombatStunReleaseEvent 类定义在
         new 之后（`new` 需要完整类型）
      3) C2664  ObjectAccessor::GetUnit 第1参数要 const WorldObject&，
         传了 Player* 没解引用
  - 本包（指纹 B3R1_BUILD=f3_decl_order）：
      1) 前向声明移到所有调用之前；
      2) CombatStunReleaseEvent 类整体上移到 HandleCombatSkillSpell 之前；
      3) GetUnit(*caster, ...) 正确解引用。
  - 你的源码现在正处于 ecd307b4（上次 apply 已成功），本包已把它列为
    合法可升级中间像：直接重跑即可平滑升级到 2ddf54a6。
    DBC 已追加会自动跳过；world SQL 幂等可重复执行。

操作：
  1. 完全关闭 worldserver。
  2. 解压 ZIP，双击 01_Install_Build_G17B3R1.cmd
     （自动：包自检+30项单测 -> 源码替换 -> 服务端 Spell.dbc 追加 ->
       world 库绑定 SQL -> MSBuild 重编 worldserver -> 校验新 exe）
     首行必须显示 B3R1_BUILD=f3_decl_order（旧包是 f1/f2）。
  3. 看到 [G17B3R1] INSTALL/BUILD PASSED。
  4. 启动 worldserver，worldserver.log 应出现：
     >> G17-B3R1 combat skills LOADED: 25 carriers 990000-990024 + rider-exit normalize active
  5. 进游戏召唤坐骑：
     - 法术书应出现当前类型的 5 个 G17 战斗技能（拖动到动作条即可用）；
     - 直接点离开载具，落地后应恢复正常步行（不再持续飞行）；
     - 按技能4降落仍正常。

回滚：双击 02_Rollback_G17B3R1.cmd（源码回 B2R3 + 恢复服务端 DBC 备份）。

结果文件：
  C:\Users\Administrator\Downloads\workspace\uploads\G17B3R1_WINDOWS_BUILD_RESULT.txt
  回传给我。
