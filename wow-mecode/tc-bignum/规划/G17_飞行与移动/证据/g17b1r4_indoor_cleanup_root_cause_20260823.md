# G17-B1R4 室内不解除根因

日期：2026-08-23

用户真实结论：地面与飞行坐骑自动接管、起飞和降落正常；进入室内Vehicle不消失。因此B1R3权威seat修复行为已PASS，但B1完整安全验收仍FAIL。

精确基础提交`4e8762ee...`的`Player::Update`只有在zone ID变化时调用`UpdateZone(newZone,newArea)`；zone不变而area变化时只调用`UpdateArea(newArea)`。C++ `PlayerScript::OnUpdateZone`仅从`Player::UpdateZone`末尾调用。建筑VMap的outdoors布尔变化不要求zone ID变化，且可能连area ID也不变，因此旧B1把实时室内清理只挂在`OnUpdateZone`上并不可靠。

B1R4由活动Vehicle AI每250ms复用`IsBlockedArea(player)`。该检查只对当前G17 Vehicle执行，无全服Player tick；命中后先锁定`_safetyCleanupStarted`，再添加坠落保护、退出Vehicle并延迟销毁，避免重复清理。`OnUpdateZone`保留作为地图/zone变化时的快速防线。

第三技能反向、空中行走姿态及施放后不能升降属于移动状态机缺陷，本批按用户要求只登记，不修改、不混入室内安全热修。
