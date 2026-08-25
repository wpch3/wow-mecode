G17-C2 客户端战斗技能实体（B3 第一步，25 个技能写入 Spell.dbc）
================================================

目的：B3 "四类战斗 + 独立技能页" 的第一步。25 个技能
（990000-990024，龙/兽/魔法/机械/通用各 5 个）作为客户端可见的
DUMMY 载体写入 Spell.dbc 并打进 patch-Z.MPQ + zhCN Y 镜像。
真实伤害/治疗/控制由后续 G17-B3-R1 服务端 SpellScript 实现，
本包不改服务端、不跑 SQL。

前提：
  1. G17-C1 必须已 PASS（客户端 Spell.dbc 当前应为 03bf11fd）
  2. WoW 完全关闭

操作（和 C1 一样）：
  1. 解压 ZIP，双击 01_Install_G17C2_Combat_Skills.cmd
  2. 看到 [G17C2] CLIENT COMBAT SKILLS PASSED
  3. 结果文件：
     C:\Users\Administrator\Downloads\workspace\uploads\G17C2_CLIENT_COMBAT_SKILLS_RESULT.txt

回滚：双击 02_Rollback_G17C2_Combat_Skills.cmd

注意：装完本包后游戏内还看不到这些技能（服务端绑定在 B3-R1），
不要现在进游戏找技能，等 B3-R1 一起验收。
