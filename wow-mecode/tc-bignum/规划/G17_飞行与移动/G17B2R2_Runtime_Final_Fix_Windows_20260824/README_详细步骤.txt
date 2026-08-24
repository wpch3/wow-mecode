================================================================
G17-B2R2 三技能最终修正 — 详细安装与回传步骤
================================================================
适用环境（已锁定，无需修改）：
  源码    D:\TrinityCore
  编译    D:\TC-Build
  二进制  D:\TC-Build\bin\RelWithDebInfo\worldserver.exe
  客户端  D:\WOW
  工作区  C:\Users\Administrator\Downloads\workspace

本包内容：
  01_Install_Build_G17B2R2.cmd      一键：替换源码 + 跑两个SQL + 重链接worldserver
  02_Rollback_G17B2R2.cmd           一键：回滚到B2R1 + 重链接worldserver
  Install-Build-G17B2R2-Windows.ps1 安装逻辑（CMD调用，一般不用直接跑）
  Rollback-Build-G17B2R2-Windows.ps1回滚逻辑
  Test-G17B2R2-Package.py           包自检
  payload/                          最终源码（会被拷到 D:\TrinityCore）
  rollback_safe/                    B2R1 安全回滚源码
  sql/                              两个 world 库 SQL
  tools/ apply_g17b2r2_source.py    源码哈希校验/替换工具
  tests/                            行为测试

================================================================
第一步：装之前（必做，30秒）
================================================================
1. 完全关闭 worldserver。
   任务管理器 → 详细信息 → 确认没有 worldserver.exe 进程。
   （worldserver.exe 运行时文件被占用，MSBuild 无法覆盖它，这是
    “编译了但没变化”的头号原因。）
2. 把本 ZIP 解压到一个【没有中文、没有空格】的临时文件夹，例如：
   C:\Users\Administrator\Downloads\G17B2R2\
   解压后该目录里要能直接看到 01_Install_Build_G17B2R2.cmd。
   不要解压到 D:\TrinityCore 里面。

================================================================
第二步：双击安装
================================================================
双击：01_Install_Build_G17B2R2.cmd

它会自动按顺序做这些事（每一步都写进结果文件）：
  a) 找 Python312/310（不用 py，避开 Python314 不存在的101错误）
  b) 包自检 + 25项单元测试
  c) 记录 D:\TrinityCore\...\cs_dragonriding.cpp 替换前哈希
  d) 把源码替换成最终版（哈希 3e4590da...），原文件备份为
     cs_dragonriding.cpp.g17b2r2.b2r1-preimage
  e) 对 world 库执行：
       sql/G17B2R2_world_landing_binding_guard.sql
       sql/G17B2R2_world_spell52226_castable_override.sql
     （自动从 worldserver.conf 读数据库连接，密码不显示）
  f) 用 vswhere 找 MSBuild，重新生成 worldserver 目标
     （关键：/t:worldserver，会重新链接出新的 worldserver.exe）
  g) 校验新 obj 是刚编译的，worldserver.exe/pdb 的时间戳和哈希确实变了

看到这行才算成功：
  [G17B2R2] INSTALL/BUILD PASSED

如果看到 FAILED，不要启动服务端，直接把结果文件发我（见第四步）。

================================================================
第三步：启动并验证（这是判断“到底跑没跑新代码”的唯一标准）
================================================================
1. 启动：D:\TC-Build\bin\RelWithDebInfo\worldserver.exe
2. 看启动窗口/worldserver.log，【必须】出现这一行：
     >> G17-B2R2 dragonriding LOADED  build=2026-08-24  postimage=03dd649d
   - 有这行 = 新代码在运行。
   - 没有这行 = 跑的还是旧 exe（检查第二步是不是 FAILED，
     或是不是启动了别的目录的 worldserver.exe）。
3. 进游戏，召唤任意已学坐骑进入御龙载具。
   上马瞬间屏幕会有红字提示：
     [G17-B2R2 build 2026-08-24 已加载] ...
   看到红字也能确认新代码在跑。
4. 测三个技能：
   技能2(高速推进)：启动有双层爆发+尾流，极速和结束有冲击环。
   技能3(冲刺)：先随便转个方向再按，应朝你面对的方向直线冲，
              不会先后退/掉头。
   技能4(着陆/52226)：龙/魔法/机械/野兽各试一次，应能释放并
              分类型着陆，无降落伞。

================================================================
第四步：回传给我（只要一个文件 + 三行字）
================================================================
回传这个文件（它自动生成）：
  C:\Users\Administrator\Downloads\workspace\uploads\G17B2R2_WINDOWS_BUILD_RESULT.txt

我主要看里面这几项：
  G17B2R2_SOURCE_STATE / SOURCE_SHA256_AFTER（应为 3e4590da...）
  MYSQL|...（两个SQL是否都 GATE=PASS）
  MSBUILD_EXIT（必须 0）
  DRAGONRIDING_FRESH_OBJECTS（必须 >=1）
  AFTER_EXE_SHA256 / AFTER_EXE_UTC（确认exe真的换了）
  如果有 G17B2R2_WINDOWS_BUILD_ERROR= 那一行，把整行发我。

再用三行字告诉我：
  A 技能2：特效是否明显 / 重复按是否被拒
  B 技能3：转向后按是否还掉头
  C 技能4：能不能释放，哪种坐骑能用/不能用，报错红字是什么

如果技能4还是放不出，把按技能4时屏幕中间的红字（比如
“需要物品”“无法在骑乘时使用”等原文）发我——这能直接定位是
哪个DBC限制还没清掉。

================================================================
回滚（只有新批次出问题才用）
================================================================
双击 02_Rollback_G17B2R2.cmd：
  - 源码回到 B2R1 字节像（ff185d99...）
  - 自动重新链接 worldserver.exe
  - 注意：回滚不撤销 52226 的 spell_dbc 覆盖（那个覆盖本身是
    让任务技能可在载具上放的正向修复，保留无害）。

================================================================
这版相对上一版修了什么（为什么你之前“没变化”）
================================================================
1. 启动横幅改用 server.loading 通道，且上马时游戏内弹红字，
   不再受你屏蔽 scripts.g17 日志的影响——一眼能确认版本。
2. 技能3：spline 发射前先 SetFacingTo 锁定你当前朝向，并
   SetFacing+SetOrientationFixed，杜绝“先后退再掉头”。
3. 技能4：52226 是任务道具技能，DBC 的装备/姿态限制会在脚本
   钩子之前被核心拦下；新增 spell_dbc 覆盖清这些限制（SQL做不到
   也不阻塞编译），C++ 的 OnCheckCast 对龙放行 + AfterCast 兜底。
4. 一键脚本 /t:worldserver 强制重链接 exe，不会只编 scripts 静态库。
