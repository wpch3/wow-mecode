# G17-B1R4 室内实时安全清理交付

日期：2026-08-23

用户真实确认：地面与飞行坐骑接管、起飞、降落均正常；B1R3权威seat修复通过。剩余FAIL为进入室内Vehicle不解除。

精确根因：旧实现只依赖`PlayerScript::OnUpdateZone`。同zone建筑的VMap outdoor状态变化不会稳定触发该C++钩子，因此室内判断没有执行机会。

B1R4仅在活动G17 Vehicle AI加入250ms有界安全检查；复用已有`IsBlockedArea()`，命中后单次执行坠落保护、退出Vehicle和延迟销毁。原zone钩子保留。

前像SHA=`94ff80334783e8883f0811a1a7f76595d91b729cc43684f00273abb9d955628b`

后像SHA=`e9418704731a2d9cd5119cc2024079a2326802796d00bf24e88928dd17ea7059`

离线门：GCC14/C++20零诊断、11项测试、严格SHA生命周期、R3权威seat合同保留、移动技能块逐字节未改、双PS AST、native runner、CRLF ASCII、包级、内部SHA、ZIP CRC、路径安全、确定性重建和解压复验全部PASS。

唯一ZIP：

- `G17B1R4_Continuous_Indoor_Safety_Windows_20260823.zip`
- 30086字节
- 13文件
- SHA-256=`d037912734a1bdcc1d6dc0a6a0bfc0e3668df6361b30ee6ee587fbe32ae85cd1`
- 入口=`01_Install_Build_G17B1R4.cmd`
- 结果=`C:\Users\Administrator\Downloads\workspace\uploads\G17B1R4_WINDOWS_BUILD_RESULT.txt`

第三技能反向、空中行走姿态和丢失升降控制已登记，按用户要求留到移动技能统一优化。本包无SQL、无客户端修改、不改R5。Windows构建和室内Runtime仍待用户。
