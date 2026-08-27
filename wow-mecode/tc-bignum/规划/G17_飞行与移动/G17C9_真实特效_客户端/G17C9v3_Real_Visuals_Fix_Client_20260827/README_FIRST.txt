G17-C9 v3 真实特效修复（Wowhead验证视觉 + 删除DBC冷却 + 修复安装器）
====================================================================

【为什么有 v3】
你运行 v1/v2 时看到的 "OBSOLETE_PACKAGE: patcher is not v1_real_visuals_no_dbc_cd"
不是你的环境问题——v1/v2 的安装器有 5 个先天缺陷，任何机器上都不可能装上：
  1. 版本门 grep 的变量名是 C6 模板遗留的 G17B3R5_VISUAL_PATCHER_VERSION，
     而 C9 补丁器变量叫 G17C9_VERSION → 永远报 OBSOLETE_PACKAGE（你撞到的就是这个）
  2. 输入门要求客户端 Spell.dbc 必须是 C3 镜像 006a892b，但你已在 C8 状态
  3. 输出门要求补丁输出等于 C6 镜像 5db5b7a5，而 C9 输出按设计就不同
  4. 环境模式要求根 MPQ 哈希仍等于 C3 时代值（C6/C7/C8 装过必然已变）
  5. 回滚 PS1 是空文件（0 字节）
v3 完全重写安装器（采用你机器上真实跑通过的 C8 安装器流程）：
  - 状态文件只提供路径，不再钉死哈希；输入状态由补丁器自己判定
    （C3/C6/C7/C8 任何一态都能直接升级，已装过则幂等 PASS）
  - 输出验证改为内容验证（补丁器 check 必须 COMPLETE）+ 打包后回读校验
  - 附带真正的回滚脚本（02_Rollback_G17C9.cmd）
DBC 内容与 v2 完全相同：25 个 Wowhead 逐条验证的真实法术视觉 + 冷却清零。

【前置】C3v2 及之后的任一客户端链状态（C3/C6/C7/C8 已装都行）。

【操作】
  1. 关闭 WoW 客户端（worldserver 无需关闭，本包只改客户端）
  2. 双击 01_Install_G17C9.cmd
  3. 看到 G17C9_CLIENT_VISUALS_RESULT=PASS 即成功
  4. 重启 WoW 客户端（安装器已自动清 Cache）

【验收四点】
  1. 龙类 5 技特效互不相同：龙息=火焰吐息 / 尾扫=扫尾 / 龙鳞=冰盾 /
     振翼=振翅冲击 / 龙威=环形爆发
  2. 其余 4 原型（兽/法/机/通）各自成族，同族 5 技互不相同
  3. 无目标按技能：不再出现冷却转圈（冷却只由服务端在成功施法后下发）
  4. 有目标施放：正常伤害 + 冷却显示正常

【回滚】双击 02_Rollback_G17C9.cmd（从 uploads\G17C9_Client_Backup_* 恢复）。

【结果文件】C:\Users\Administrator\Downloads\workspace\uploads\G17C9_CLIENT_VISUALS_RESULT.txt
