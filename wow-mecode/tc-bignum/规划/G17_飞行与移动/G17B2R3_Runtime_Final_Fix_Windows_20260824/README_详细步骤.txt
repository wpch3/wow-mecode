================================================================
G17-B2R3 技能4施放修复 + 技能3防倒车 — 详细安装与回传步骤
================================================================
适用环境（已锁定，无需修改）：
  源码    D:\TrinityCore
  编译    D:\TC-Build
  二进制  D:\TC-Build\bin\RelWithDebInfo\worldserver.exe
  客户端  D:\WOW
  工作区  C:\Users\Administrator\Downloads\workspace

本包内容：
  01_Install_Build_G17B2R3.cmd      一键：替换源码 + 跑两个SQL + 重链接worldserver
  02_Rollback_G17B2R3.cmd           一键：回滚到R2 + 重链接worldserver
  Install-Build-G17B2R3-Windows.ps1 安装逻辑（CMD调用，一般不用直接跑）
  Rollback-Build-G17B2R3-Windows.ps1回滚逻辑
  Test-G17B2R3-Package.py           包自检
  payload/                          最终源码（会被拷到 D:\TrinityCore）
  rollback_safe/                    R2 安全回滚源码（=你上一轮编译运行的版本）
  sql/                              两个 world 库 SQL
  tools/ apply_g17b2r3_source.py    源码哈希校验/替换工具
  tests/                            行为测试

================================================================
第一步：装之前（必做，30秒）
================================================================
1. 完全关闭 worldserver。
   任务管理器 → 详细信息 → 确认没有 worldserver.exe 进程。
2. 把本 ZIP 解压到一个【没有中文、没有空格】的临时文件夹，例如：
   C:\Users\Administrator\Downloads\G17B2R3\
   解压后该目录里要能直接看到 01_Install_Build_G17B2R3.cmd。
   不要解压到 D:\TrinityCore 里面。

================================================================
第二步：双击安装
================================================================
双击：01_Install_Build_G17B2R3.cmd

它会自动按顺序做这些事（每一步都写进结果文件）：
  a) 找 Python312/310（不用 py，避开 Python314 不存在的101错误）
  b) 包自检 + 单元测试（含 R3 新增：52226施放门槛净化、技能3先停旧运动、
     哈希状态分类纯函数等回归）
  c) 记录 D:\TrinityCore\...\cs_dragonriding.cpp 替换前哈希
     （预期是 R2 后像 3b92e815...，也接受全部旧版 R2 血缘）
  d) 把源码替换成最终版（哈希 98446106...），原文件备份为
     cs_dragonriding.cpp.g17b2r3.b2r2-preimage
  e) 对 world 库执行：
       sql/G17B2R3_world_landing_binding_guard.sql
       sql/G17B2R3_world_spell52226_castable_override.sql
     （自动从 worldserver.conf 读数据库连接，密码不显示）
  f) 用 vswhere 找 MSBuild，重新生成 worldserver 目标
     （关键：/t:worldserver，会重新链接出新的 worldserver.exe）
  g) 校验新 obj 是刚编译的，worldserver.exe/pdb 的时间戳和哈希确实变了

看到这行才算成功：
  [G17B2R3] INSTALL/BUILD PASSED

如果你的 D:\TrinityCore 源码哈希是下面任一值，都是预期可升级来源
（本包自动识别并替换，绝不覆盖未知文件）：
  3b92e815...  (R2 后像 —— 你上一轮编译运行的版本，最常见)
  ff185d99...  (B2R1 后像)
  3e4590da...  (旧版 R2 草稿)
  61342067...  (更早的 R2 草稿壳)
  03dd649d...  (无横幅版 R2 终稿)
  adedfc58...  (含未验证 Kit 的 R2 中间版)
替换后 SOURCE_SHA256_AFTER 必须等于 98446106...10f9。

如果看到 FAILED，不要启动服务端，直接把结果文件发我（见第四步）。

================================================================
第三步：启动并验证（这是判断“到底跑没跑新代码”的唯一标准）
================================================================
1. 启动：D:\TC-Build\bin\RelWithDebInfo\worldserver.exe
2. 看启动窗口/worldserver.log，【必须】出现这两行（逐字一致）：
     >> G17-B2R3 landing command 52226 cast-gates cleared (focus/aura/item/stance); ...
     >> G17-B2R3 dragonriding LOADED  build=20260824-r3 (skill4 castable + skill3 anti-reverse)
   - 有这两行 = 新代码在运行（技能4的焦点/光环门槛已被清除）。
   - 没有 = 跑的还是旧 exe（检查第二步是不是 FAILED，
     或是不是启动了别的目录的 worldserver.exe）。
3. 进游戏，召唤任意已学坐骑进入御龙载具。
   上马瞬间屏幕会有红字提示：
     [G17-B2R3 build 2026-08-24 已加载] ...
   看到红字也能确认新代码在跑。
4. 测三个技能：
   技能2(高速推进)：启动有双层爆发+尾流，极速和结束有冲击环。
   技能3(冲刺)：先随便转个方向、最好在按住后退/刹车时按，
               应朝你面对的方向直接冲，不再有先倒一段再冲的现象。
   技能4(着陆/52226)：龙/魔法/机械/野兽各试一次，应能释放并
               分类型着陆，无降落伞。

================================================================
第四步：回传给我（只要一个文件 + 三行字）
================================================================
回传这个文件（它自动生成）：
  C:\Users\Administrator\Downloads\workspace\uploads\G17B2R3_WINDOWS_BUILD_RESULT.txt

我主要看里面这几项：
  G17B2R3_SOURCE_STATE / SOURCE_SHA256_AFTER（应为 98446106...10f9）
  MYSQL|...（两个SQL是否都 GATE=PASS）
  MSBUILD_EXIT（必须 0）
  DRAGONRIDING_FRESH_OBJECTS（必须 >=1）
  AFTER_EXE_SHA256 / AFTER_EXE_UTC（确认exe真的换了）
  如果有 G17B2R3_WINDOWS_BUILD_ERROR= 那一行，把整行发我。

再用三行字告诉我：
  A 技能2：特效是否明显 / 重复按是否被拒
  B 技能3：后退中/刹车中按，是否还有“先倒车再冲刺”
  C 技能4：能不能释放，哪种坐骑能用/不能用，报错红字是什么

如果技能4还是放不出，把按技能4时屏幕中间的红字原文发我——但请注意：
R3 已在启动日志证明“52226 cast-gates cleared”，如果仍失败，请确认跑的
确实是新 exe（启动日志两行都必须在）。

================================================================
回滚（只有新批次出问题才用）
================================================================
双击 02_Rollback_G17B2R3.cmd：
  - 源码回到 R2 字节像（3b92e815...）
  - 自动重新链接 worldserver.exe

================================================================
这版修了什么（为什么技能4“完全不能用”）
================================================================
1. 52226 是“飞行器着陆”任务道具技能，DBC 要求法术焦点 1553 和
   施法者光环 52255；龙没有这两样，核心在 CheckCast 最前面就拒绝，
   OnCheckCast 钩子根本不会执行（这就是为什么之前所有脚本修补都没用）。
2. 世界库 spell_dbc 覆盖表没有 RequiresSpellFocus / CasterAuraSpell
   两列，SQL 覆盖永远清不掉这两个限制——只能从 C++ 运行时净化。
3. B2R3 在服务启动时直接清零 52226 的焦点/光环/姿态/物品门槛
   （SpellInfo 运行时修改，按钮名称与客户端 DBC 完全不动），
   同时保留 OnCheckCast 放行 + OnEffectHit 抑制原生Dummy +
   AfterCast 幂等兜底。启动日志打印“cast-gates cleared”证明已生效。
4. 技能3：先 StopMoving 结算旧运动再从真实位置取样，杜绝先倒车。
