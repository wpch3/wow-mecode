G17-B1R3：权威Vehicle座位验证，修复控制权已建立却被movementSeat=-1误清理
日期：2026-08-23

真实根因
--------
日志同时显示actualVehicle等于expectedDragon、charmer等于玩家自身，证明Vehicle与控制权已经建立；只有GetTransSeat返回-1。GetTransSeat是movement transport状态，不是服务器Vehicle座位占用真值。旧B1因此错误退出并销毁已经成功的Vehicle。

B1R3修复
--------
- 使用Vehicle::Seats与Vehicle::GetPassenger取得服务器权威seat；
- 成功必须同时满足正确Vehicle、权威seat、玩家charmer；
- movementSeat只输出诊断，不再触发误清理；
- 错误Vehicle/权威seat/charmer继续严格失败；
- status同时显示seat和movementSeat；
- 不运行SQL、不改客户端、不改R5。

安装
----
1. 正常停止worldserver。
2. 不回滚B1，不重复R1-R5或旧B1/B1R1包。
3. 解压本包到C:\Users\Administrator\Downloads\workspace\uploads。
4. 运行01_Install_Build_G17B1R3.cmd。
5. 最终必须为G17B1R3_WINDOWS_BUILD_RESULT=PASS。
6. FAIL时不要启动worldserver，回传结果文件。

结果文件
--------
C:\Users\Administrator\Downloads\workspace\uploads\G17B1R3_WINDOWS_BUILD_RESULT.txt

PASS后Runtime
-------------
1. 正常启动worldserver，户外.dragon status应为INACTIVE auto=on。
2. 点击萨拉斯军马34767；必须保留外观、四技能、离地，status为ACTIVE且seat=0、controlled=true；movementSeat允许作为独立诊断显示-1或0。
3. .dragon dismiss后点击59961；必须保留始祖幼龙外观、四技能、起飞移动，status为ACTIVE/controlled=true。
4. 低空进入真实室内，必须安全解除且无Vehicle/异常速度残留。

只回传两条status关键行、A/B/C现象和启动G17B1R3错误。其它坐骑矩阵不转嫁用户。

ObjectVariables.ext invalidated Creature和Map::Remove<Bot>FromMap not in grid是独立Lua/NPCBot缺陷，不是本次座位失败根因；当前不混入本包。
