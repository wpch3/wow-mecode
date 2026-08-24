G17-R4：旧世界客户端双飞行标志修正（仅客户端，不重编服务端）
日期：2026-08-23

一、为什么R3安装PASS但湿地仍返回nil

R3只给旧世界AreaTable行增加了0x00000400。
湿地（Area ID 11）的变化是：

  原版：0x00000040
  R3：  0x00000440
  R4：  0x00004440

3.3.5a扩展区域的正常可飞行行同时具有：

  0x00000400  AREA_FLAG_FLYING / OUTLAND
  0x00004000  AREA_FLAG_ENABLE_FLIGHT_BOUNDS_ON_MAP / OUTLAND2

用户在湿地实测IsFlyableArea()返回nil，证明R3单标志没有形成客户端可飞行结果。
R4在完全相同的948个安全候选行上补齐第二个0x00004000，不扩大行集合。

二、本包没有使用“无头骑士式客户端伪装”

没有修改Spell.dbc的Effect、Aura、MiscValueB、图标或提示。
没有把始祖幼龙伪装成地面坐骑。
没有修改服务端Spell.dbc或AreaTable.dbc。
没有SQL、没有源码覆盖、没有服务端重新编译。

保留的客户端Spell.dbc必须仍为R1精确哈希：
  dd25091167f671764735ce88c78b66485c6d661fadf05d322574c261f6e464ea

R4客户端AreaTable.dbc哈希：
  1acef997a27f844a8abee9b477c44f2097f745168c6df85ece6b1a135568c233

三、安装

1. 完全关闭Wow.exe。
2. 保持R3服务端与客户端状态，不要回滚、不要重新执行R1/R2/R3。
3. 双击：Run-G17R4-Client-Fix.cmd
4. 读取：
   C:\Users\Administrator\Downloads\workspace\uploads\G17R4_CLIENT_MPQ_UPGRADE_RESULT.txt
5. 只有末尾出现以下内容才算安装成功：
   G17R4_CLIENT_MPQ_UPGRADE_RESULT=PASS

安装器会：
- 验证R1和R3状态；
- 验证当前patch-Z.MPQ内Spell和R3 Area的精确哈希；
- 扫描Data根目录及Data\zhCN的自定义字母MPQ是否碰撞Spell/Area；
- 从服务端原始AreaTable只读重生成R4客户端Area；
- 回读验证新MPQ内两个DBC；
- 备份R3 MPQ后原子交换；
- 删除客户端Cache目录；
- 再次确认服务端两个DBC完全未改。

如果报告显示custom MPQ collision，请不要手工删除任何MPQ，直接回传报告。

四、真实游戏验收（湿地安全户外）

A. 输入：
/run DEFAULT_CHAT_FRAME:AddMessage("G17R4 IsFlyableArea="..tostring(IsFlyableArea()))

预期：G17R4 IsFlyableArea=1

B. 从坐骑技能页/法术书普通按钮点击红色始祖幼龙（59961），预期：
- 能正常召唤；
- 能起飞；
- 能水平移动；
- 能降落。

C. 到真实室内再点同一普通按钮，预期服务端拒绝。

回传最小结果：
  G17R4_INSTALL=PASS/FAIL
  G17R4_WETLANDS_ISFLYABLEAREA=1/nil
  G17R4_59961_NORMAL_BUTTON=PASS/FAIL
  G17R4_59961_TAKEOFF_MOVE_LAND=PASS/FAIL
  G17R4_REAL_INDOOR_REJECT=PASS/FAIL

五、回滚

完全关闭Wow.exe，然后双击：
  Run-G17R4-Client-Rollback.cmd

回滚只恢复R3客户端MPQ，不回滚R3服务端安全门，也不破坏R1/R3状态。
