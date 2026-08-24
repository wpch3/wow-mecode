G17-B2R3 技能4施放修复 + 技能3防倒车 — 一键安装
================================================

只需要做一件事：

  1. 完全关闭 worldserver（任务管理器确认没有 worldserver.exe）。
  2. 双击 01_Install_Build_G17B2R3.cmd
     （会自动：用 Python312 替换源码 -> 对 world 库执行两个守护/门 SQL ->
       用 MSBuild 重新生成 worldserver 目标 -> 校验新 exe/pdb 时间戳和哈希）
  3. 看到 [G17B2R3] INSTALL/BUILD PASSED 后，正常启动：
       D:\TC-Build\bin\RelWithDebInfo\worldserver.exe
  4. 在 worldserver.log 启动处确认出现（逐字一致）：
       >> G17-B2R3 dragonriding LOADED  build=20260824-r3 (skill4 castable + skill3 anti-reverse)
       >> G17-B2R3 landing command 52226 cast-gates cleared (focus/aura/item/stance); ...
     如果没有这两行，说明跑的不是新 exe。
     源码后像 SHA256（结果文件 SOURCE_SHA256_AFTER 应等于）：
       98446106309b45371f138d9c7bc707ee608d9a3db347e13d61cfd68cc97810f9
  5. 进游戏召唤坐骑，依次按一次技能2、技能3、技能4。
  6. 把这个结果文件发回给代理：
       C:\Users\Administrator\Downloads\workspace\uploads\G17B2R3_WINDOWS_BUILD_RESULT.txt
     （里面包含源码前后 SHA、SQL 结果、MSBuild 输出、新 exe 哈希/时间戳）
     并在 worldserver.log 里搜 "G17-B2R3"，把按技能时打印的几行一并发回。

本批修复（相对上一版 B2R2，你实测：安装成功、技能2/3基本OK、技能4完全不能用）：

  技能4（根因已定位）：52226“飞行器着陆”是原版飞行器任务道具技能，DBC 里
    RequiresSpellFocus=1553（需要法术焦点）且 CasterAuraSpell=52255（需要
    “飞艇”光环）。龙载具两者都没有，核心在 CheckCast 最前面就返回
    “需要法术焦点/施法者光环”，我们的 OnCheckCast 钩子根本没机会运行；
    而且世界库 spell_dbc 表没有这两列，SQL 永远清不掉。B2R3 在服务端
    启动时直接把 52226 的焦点/光环/姿态/物品门槛全部清零（C++ 运行时
    净化，无需改客户端 DBC），按钮和名称不变，OnCheckCast/AfterCast
    兜底保留。
  技能3（小尾巴）：上一版在冲刺开始时先建路径再停旧运动，旧的后退/刹车
    spline 被清除时会先“结算”到最远点，客户端因此先倒一小段再冲。B2R3
    改为：先停旧运动（StopMoving 结算当前位置）并锁定朝向，再从真实
    当前位置取样建路径，冲刺从静止开始，无倒车。

回滚（仅在新批次异常时）：双击 02_Rollback_G17B2R3.cmd，回到 R2 字节像
  （3b92e815...，即你上一轮编译运行通过的版本）并重新编译。

路径固定：
  源码 D:\TrinityCore
  编译 D:\TC-Build
  二进制 D:\TC-Build\bin\RelWithDebInfo\worldserver.exe
  客户端 D:\WOW
  工作区 C:\Users\Administrator\Downloads\workspace

不需要手动改任何东西、不需要手动跑 SQL、不需要手动开 VS。
