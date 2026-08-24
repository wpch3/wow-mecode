G17-B2R1 三技能 Runtime 体验重制（Windows，2026-08-24）

目标
- 技能2：启动、加速中、进入极速、结束四阶段均有一次性视觉/声音反馈；250%-1200%硬上限不变，持续期间禁止重复叠加。
- 技能3：用7节点 Catmull-Rom 向前爬升，转角上限40度；高速度转向率由1.00平滑降至0.82，正常结束按末端切线交还控制。
- 技能4：动作条迁移到客户端无SpellVisual、无Aura的“飞行器着陆”；魔法平姿、龙类长斜坡、机械反推、猛兽无火扑落均使用前向多点/多段样条。

安装（唯一推荐路径）
1. 正常关闭 worldserver。
2. 双击 01_Install_Build_G17B2R1.cmd。
3. 脚本自动执行：包自检 -> 52项测试 -> 严格SHA源文件应用 -> world数据库安全迁移 -> RelWithDebInfo worldserver增量编译 -> EXE/PDB/OBJ新鲜度检查。
4. 只在 C:\Users\Administrator\Downloads\workspace\uploads\G17B2R1_WINDOWS_BUILD_RESULT.txt 出现：
   G17B2R1_WINDOWS_BUILD_RESULT=PASS
   后才启动 worldserver。

固定环境
- Workspace: C:\Users\Administrator\Downloads\workspace
- SourceRoot默认: D:\TrinityCore
- BuildRoot默认: D:\TC-Build
- 只使用Python312/310；不使用py.exe、Python314或WindowsApps别名。
- SQL从 D:\TC-Build\bin\RelWithDebInfo\worldserver.conf 读取 WorldDatabaseInfo，并明确只操作 world。
- 自动寻找 mysql.exe/mariadb.exe；密码仅临时放入MYSQL_PWD，不写结果文件。
- 本包不改客户端文件，不触碰已关闭的R5 zhCN Y槽。

最短Runtime验收（无需完整矩阵）
1. 任一坐骑把速度拉到1000%以上，按技能2：看启动冲刺、持续尾流、极速风爆、结束风爆/尾流停止。
2. 1000%以上转向并按技能3：应连续弯曲向前上升，结束不应锐角或跳角，控制立即可用。
3. 动作条第4格应显示“飞行器着陆”，施放全过程不得出现降落伞。
4. 各选一个魔法/风、龙、机械/火箭、猛兽坐骑着陆：分别检查平姿风托、长斜坡、反推拉平、无火扑落。
5. 面向墙或低空尝试技能3/4：路径不安全时应取消并恢复控制，不穿墙、不直坠。

回滚
- 正常关闭 worldserver，双击 02_Rollback_Build_G17B2R1.cmd。
- 回滚故意使用“安全底线”源码和同一world迁移：移除R1复杂运动，但绝不恢复已知降落伞动作或猛兽喷火Kit。
- 仅在 G17B2R1_WINDOWS_ROLLBACK_RESULT=PASS_SAFE_FLOOR 后启动服务。

注意
- 自动编译与48项测试已通过；真实模型姿态/视觉仍必须以上述短Runtime验收为准，交付文件不会伪称Runtime已通过。
- 若结果为FAIL，不要启动worldserver；只需回传uploads中的小型结果TXT，无需回传完整ZIP。
