G17-B2R4 服务端 Spell.dbc 解锁（技能4 52226）— 无需重新编译
================================================

目的：技能4“飞行器着陆”在客户端本地就被 Spell.dbc 里的
  RequiresSpellFocus=1553（需要飞行器焦点）和 CasterAuraSpell=52255
  （需要飞行器光环）拦下，点击按钮时客户端直接报错、封包根本没发到
  服务器。所以之前服务器端做的所有净化都“看起来没效果”。

本包只改一个文件（纯 DBC 字节修补，不重编译、不跑 SQL、不动客户端）：

  D:\TC-Build\bin\RelWithDebInfo\dbc\Spell.dbc

操作（10 秒）：
  1. 完全关闭 worldserver。
  2. 双击 01_Install_Build_G17B2R4_Server_DBC.cmd
  3. 看到 [G17B2R4] SERVER DBC UNLOCK PASSED 后，正常启动 worldserver：
       D:\TC-Build\bin\RelWithDebInfo\worldserver.exe
  4. 进游戏召唤坐骑，按技能4：现在应能释放并分类型着陆。

前提：服务器已装 G17-B2R3（R3FIX5 版或 R2FIX4 版均可，技能2/3正常）。
  如果技能2/3都正常，此包直接双击即可。

回滚：双击 02_Rollback_G17B2R4_Server_DBC.cmd 恢复原 Spell.dbc。

结果文件：
  C:\Users\Administrator\Downloads\workspace\uploads\G17B2R4_SERVER_SPELL_DBC_RESULT.txt
  回传给我。
