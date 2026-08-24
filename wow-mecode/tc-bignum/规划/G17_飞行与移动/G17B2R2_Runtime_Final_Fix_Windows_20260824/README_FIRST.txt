G17-B2R2 三技能 Runtime 最终修正 — 一键安装
================================================

只需要做一件事：

  1. 完全关闭 worldserver（任务管理器确认没有 worldserver.exe）。
  2. 双击 01_Install_Build_G17B2R2.cmd
     （会自动：用 Python312 替换源码 -> 对 world 库执行 52226 绑定守护 SQL ->
       用 MSBuild 重新生成 worldserver 目标 -> 校验新 exe/pdb 时间戳和哈希）
  3. 看到 [G17B2R2] INSTALL/BUILD PASSED 后，正常启动：
       D:\TC-Build\bin\RelWithDebInfo\worldserver.exe
  4. 在 worldserver.log 启动处确认出现（逐字一致）：
       >> G17-B2R2 dragonriding LOADED  build=20260824-r2 (skill2/3/4 fixes active)
     如果没有这行，说明跑的不是新 exe。
     源码后像 SHA256（结果文件 SOURCE_SHA256_AFTER 应等于）：
       3b92e815dc81ade4aa9927c19716dabddb8e8f93a6d0aff8b32c80dfbcbfc7f1
  5. 进游戏召唤坐骑，依次按一次技能2、技能3、技能4。
  6. 把这个结果文件发回给代理：
       C:\Users\Administrator\Downloads\workspace\uploads\G17B2R2_WINDOWS_BUILD_RESULT.txt
     （里面包含源码前后 SHA、SQL 结果、MSBuild 输出、新 exe 哈希/时间戳）
     并在 worldserver.log 里搜 "G17-B2R2"，把按技能时打印的几行一并发回。

注意（如果你上一轮已经跑过一次失败）：
  你 D:\TrinityCore 的 cs_dragonriding.cpp 现在是旧版 R2 草稿
  （SHA 3e4590da...），属于 R2 血缘，本包已把它列入可升级来源，
  直接重新双击本 CMD 即可；结果文件里 SOURCE_SHA256_BEFORE=3e4590da...
  是预期值，SOURCE_SHA256_AFTER 必须变成 3b92e815...。

本批修复：
  技能2：只用已审计 Kit（44/696/13709/13481），双层启动+交替气浪+极速/结束双冲击环。
  技能3：路径锚定骑乘者施法朝向，水平零偏航，根除固定方向转弯/掉头。
  技能4：保留 52226“飞行器着陆”，OnCheckCast 放行龙 + OnEffectHit 抑制 Dummy +
         AfterCast 幂等兜底，全类型坐骑可用。

回滚（仅在新批次异常时）：双击 02_Rollback_G17B2R2.cmd，回到 B2R1 字节像并重新编译。

路径固定：
  源码 D:\TrinityCore
  编译 D:\TC-Build
  二进制 D:\TC-Build\bin\RelWithDebInfo\worldserver.exe
  客户端 D:\WOW
  工作区 C:\Users\Administrator\Downloads\workspace

不需要手动改任何东西、不需要手动跑 SQL、不需要手动开 VS。
