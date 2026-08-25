================================================================
G17-C2 客户端战斗技能实体 — 详细步骤
================================================================
适用环境（已锁定）：
  客户端  D:\WOW
  工作区  C:\Users\Administrator\Downloads\workspace
  前提    G17-C1 必须已经 PASS（Spell.dbc == 03bf11fd）
         G17-R4/R5 客户端链已装

本包内容：
  01_Install_G17C2_Combat_Skills.cmd  一键：追加25技能到 Spell.dbc -> 重建 patch-Z.MPQ -> 镜像 zhCN-Y -> 清缓存
  02_Rollback_G17C2_Combat_Skills.cmd 回滚
  tools/append_g17b3_spells.py         B3 技能追加器（含 --version/幂等/守卫）
  tools/G17B3_skill_table.py          技能表（25 条，990000-990024）
  third_party/mpqcli-windows-amd64.exe

步骤：
  1. 完全关闭 WoW（任务管理器无 wow.exe/wow-64.exe）。
  2. 解压 ZIP（无中文/空格路径），双击 01_Install_G17C2_Combat_Skills.cmd。
  3. 看到 [G17C2] CLIENT COMBAT SKILLS PASSED。
  4. 回传：
     C:\Users\Administrator\Downloads\workspace\uploads\G17C2_CLIENT_COMBAT_SKILLS_RESULT.txt

验收点（日志应出现）：
  C2_PATCHER_VERSION_CHECK=PASS v1_append25
  ENV_MODE=DISCOVERY（或 STATE）
  R4_OWNED_SPELL_SHA256=03bf11fd...
  SPELL_DBC_APPEND|G17B3_SPELL_DBC_STATE=APPENDED
  GENERATED_SPELL_SHA256=760d3f27...
  NEW_MPQ_SHA256=...
  G17C2_CLIENT_COMBAT_SKILLS_RESULT=PASS

回滚：双击 02_Rollback_G17C2_Combat_Skills.cmd。

注意：装完先不要进游戏找战斗技能——服务端绑定在 G17-B3-R1，
R1 完成后再一起验收（设计文档 §5.2/cannot只播放视觉）。
