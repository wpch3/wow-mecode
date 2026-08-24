G17-B1：所有已拥有直接坐骑按钮自动接管 + 原模型 + 类型会话
日期：2026-08-23

前置已经关闭
------------
G17-R5真实Runtime已PASS：59961普通按钮可召唤、上马、起飞、水平飞行；进入室内自动解除且无问题。
不要回滚或重复R1-R5，也不要改客户端locale Y槽。

B1本批实现
----------
1. 默认`.dragon auto on`：玩家正常点击自己已经学会的直接坐骑按钮；普通上马成功后100ms安全转换为G17原生可控Vehicle。
2. 精确读取普通坐骑当时的`UNIT_FIELD_MOUNTDISPLAYID`，新Vehicle同时设置display/native display，保留原坐骑外观，不把全部坐骑替换成红龙。
3. 对飞行、纯飞行和地面坐骑使用同一接管链；地面模型没有飞行动画时允许空中奔跑，不以模型为由拒绝。
4. 只接管`player->HasSpell()`确认拥有且含`SPELL_AURA_MOUNTED`的直接坐骑法术；出租、任务临时载具、未知触发内层法术不会被冒充为拥有坐骑。
5. 语言无关类型：DRAGON/BEAST/MECHANICAL/MAGIC/GENERIC。无法分类进入GENERIC，不拒绝。
6. 会话记录sourceSpell/sourceCreature/display/type；`.dragon status`可见。
7. 普通坐骑Aura只在替代Vehicle、可控座位和会话数据全部准备完毕后移除，失败转换保留原普通坐骑。
8. 保留R1异步入座、4技能动作条、energy、慢落、死亡/换图/城市/室内/副本/BG/竞技场清理。
9. `.dragon auto off`可关闭本次登录的自动接管；下次登录默认重新开启。永久偏好属于后续持久化批次。

本批不冒充完成
--------------
B1是完整御龙工程的第一实施批。5档速度、动量/俯冲/滑翔、1200%极速、独立攻击页、玩家骑乘施法、自动寻路和固定模式仍分别在B2-B5继续实现；本包未把这些项目标成PASS。

安装和构建
----------
1. 保持当前R5客户端不动，正常关闭worldserver.exe。
2. 解压本包到C:\Users\Administrator\Downloads\workspace\uploads。
3. 运行：01_Install_Build_G17B1.cmd
4. 结果必须含：G17B1_WINDOWS_BUILD_RESULT=PASS
5. 若FAIL，不启动worldserver，直接回传结果文件；不要手工复制源码。
6. PASS后正常启动worldserver。

最小Runtime验收
---------------
先输入：
.dragon status

应看到INACTIVE auto=on。

A. 点击任意已拥有的地面坐骑普通按钮：
- 外观仍是该地面坐骑；
- 自动进入可控载具并出现R1四技能动作条；
- 可以离地飞行；
- `.dragon status`显示sourceSpell/display/type和controlled=true。

B. 点击59961普通按钮：
- 同样自动接管且仍为红色始祖幼龙；
- 可起飞和移动。

C. 进入真实室内：
- 会话安全解除。

如拥有机械坐骑，可再点一次机械坐骑并确认status类型；这不是强制回传项。

回传
----
C:\Users\Administrator\Downloads\workspace\uploads\G17B1_WINDOWS_BUILD_RESULT.txt
并说明A/B/C是否通过。B1 Runtime通过后直接进入B2五档动量和1200%安全极速，不转去Bot；完整御龙B1-B6完成后才按用户顺序继续Bot。

回滚
----
02_Rollback_Build_G17B1.cmd 会核对B1后像和R1备份后恢复并重新构建；不改数据库或客户端。
