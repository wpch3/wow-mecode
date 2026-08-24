G17-R3 纯飞行坐骑普通按钮修复（Windows 组合包）
====================================================
日期：2026-08-23
状态：离线验证完成；等待本机 Windows 构建和游戏内验收

一、这次修什么
--------------
R2 的服务端完整非 triggered 施法链已经通过：在湿地执行
.cast self 59961
可以成功召唤并上马。普通坐骑按钮失败来自客户端 AreaTable 本地飞行区判断，
所以本包不再放宽服务端规则，也不重复安装 R1/R2。

本包只做两件互相依赖的事：
1. 服务端：在旧世界放行前增加实时 Player::IsOutdoors() 安全门并重新编译。
2. 客户端：在 R1 自己拥有的 patch-字母.MPQ 内加入锁定的 zhCN AreaTable.dbc，
   仅对安全候选增加 AREA_FLAG_OUTLAND；同时保留 R1 的 Spell.dbc。

重要事实：锁定的旧世界 AreaTable 行和父区域组合后，INSIDE 标志行数为 0。
因此客户端静态数据不能承担建筑室内保护；本包必须先编译服务端实时户外安全门，
然后才允许安装客户端 AreaTable。

二、运行前必须满足
------------------
1. 路径仍为：
   源码 D:\TrinityCore
   构建 D:\TC-Build
   客户端 D:\WOW
   工作区 C:\Users\Administrator\Downloads\workspace
2. workspace\uploads 内必须保留已经 PASS 的 R1 安装结果和状态文件。
3. R2 的源码、worldserver.exe、worldserver.pdb 必须仍是已验证版本。
4. 完全关闭 worldserver 和 Wow.exe；不要只退到角色选择界面。
5. Visual Studio/MSBuild、CMake 构建树和 Python 3.12 或 3.10 保持可用。
6. 不要再运行旧 Source Apply、SQL、v1/v2/v3、R1 或 R2 包。

三、唯一安装入口
----------------
双击：Run-G17R3-Windows-Fix.cmd

不要单独跳过第一阶段去运行客户端升级脚本。入口会按固定顺序：
1. 校验 R2/R3 源码状态、R1 前置文件和构建成员关系；
2. 首次执行时持久备份 R2 worldserver.exe/pdb；
3. 精确哈希安装 R3 SpellInfo.cpp 并增量重编 worldserver；
4. 校验 zhCN 客户端、R1 自有 MPQ、服务器原始 DBC 和其它自定义槽冲突；
5. 生成并回读验证同时含 Spell.dbc 与 AreaTable.dbc 的新 MPQ；
6. 原子交换客户端 MPQ并保存 R1 备份和 R3 状态。

不会执行 SQL；不会改服务端 Spell.dbc 或 AreaTable.dbc；不会重复 R1。

四、执行后先回传结果，不要立即开服
----------------------------------
主结果：
C:\Users\Administrator\Downloads\workspace\uploads\G17R3_WINDOWS_FIX_RESULT.txt

另外保留并可回传：
- G17R3_SERVER_WINDOWS_FIX_RESULT.txt
- G17R3_SERVER_BUILD_STATE.txt
- G17R3_CLIENT_MPQ_UPGRADE_RESULT.txt
- G17R3_CLIENT_MPQ_UPGRADE_STATE.txt

主结果必须包含：
G17R3_WINDOWS_FIX_RESULT=PASS

如果出现 FAIL：不要启动 worldserver，不要手工复制文件，不要运行旧包；回传结果文件。
同一个 R3 入口支持安全重复执行：构建失败后修好环境可重跑，已成功的客户端阶段会报告 ALREADY_CURRENT。

五、PASS 后的唯一游戏内验收
--------------------------
确认主结果 PASS 后，正常启动 worldserver 和 Wow。不要用 .cast 做最终验收。
在湿地使用法术书/坐骑页的普通红色始祖幼龙按钮（59961）验证：
1. 普通按钮可召唤并上马；
2. 可起飞；
3. 可水平移动；
4. 可正常降落；
5. 进入真实建筑室内后普通按钮必须被服务端拒绝；
6. 城市、显式禁飞区、副本、战场、竞技场及配置黑名单继续拒绝。

只有以上真实验收通过，R3 才能最终 PASS，之后才进入 G17-B1 通用坐骑阶段。

六、全量回滚
------------
仅在需要撤销且 worldserver/Wow 都关闭时，双击：
Run-G17R3-Full-Rollback.cmd
输入大写 ROLLBACK 后继续。

它先恢复 R1 自有客户端 MPQ，再恢复 R2 SpellInfo.cpp 和 R2 worldserver.exe/pdb。
所有当前文件和备份都要通过状态哈希校验；篡改或缺失会严格拒绝，不会盲目覆盖。

七、锁定哈希摘要
----------------
原 AreaTable.dbc：b0356ff41e5777896509ec52bc68af516b67d82a659dbc47757960aef98b62dd
R3 AreaTable.dbc：214c6935d11b784f0bf5e4855fb756126d9d667d622a346c3124ae748812b6a8
R2 SpellInfo.cpp：73d52ac0feb67a32822fc0bf086a9174ba7ef0bc186223cdc8a690f48fccb9e2
R3 SpellInfo.cpp：c3ec2237ed6da8831662a8b7a5d45cf88f8efc7798cdd35c52a07700fa9cbcbf
R3 AreaTable 修改行：948（只增加 0x00000400）
