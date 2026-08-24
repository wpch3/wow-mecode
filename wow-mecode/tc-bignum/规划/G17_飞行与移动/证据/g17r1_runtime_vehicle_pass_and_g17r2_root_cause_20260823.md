# G17-R1车辆Runtime PASS与G17-R2根因（2026-08-23）

## 用户真实验收回报

用户明确回报：

- `.dragon summon`没有问题；
- 4个载具技能全部可以使用；
- 始祖幼龙在湿地仍提示“这里无法召唤坐骑”；
- 当前御龙体验飞行过慢，没有分段加速/俯冲驰骋感，攻击与御龙技能混在同一组。

据此可以记录：

```text
G17R1_RUNTIME_SUMMON=PASS
G17R1_RUNTIME_ALL_4_VEHICLE_SKILLS=PASS
G17R1_RUNTIME_VEHICLE_CONTROL_AND_ACTIONBAR=PASS
G17R1_RUNTIME_EXPERIENCE=SLOW_NO_MOMENTUM_REQUIRES_REDESIGN
G17R1_PROTO_DRAKE_OLD_WORLD_RUNTIME=FAIL_IN_WETLANDS_INCORRECT_AREA
```

用户没有提供`G17R1_CLIENT_MPQ_INSTALL_RESULT.txt`，所以客户端MPQ实际安装状态继续为UNKNOWN；车辆Runtime不能证明客户端DBC补丁已安装。

## 确定性服务端根因

目标文件：`G17A_安全全世界飞行/源文件/SpellInfo.cpp`

函数：`SpellInfo::CheckLocation()`

旧严格分支先计算`g17OldWorldAllowed`，但条件结构只让它绕过`AreaTableEntry::IsFlyable()`，随后仍无条件执行：

```cpp
!player->CanFlyInZone(map_id, zone_id, this)
```

在地图0/1，该原版调用返回false，因此即使G17旧世界安全策略已经放行，仍返回`SPELL_FAILED_INCORRECT_AREA`。非严格分支已使用`originalContinentAllowed || g17OldWorldAllowed`，严格/非严格逻辑不一致。

这是一条独立于客户端DBC/MPQ状态的确定性服务端失败路径，足以解释当前59961失败；不应再重复R1或把问题无证据归咎于MPQ locale。

## R2处理

G17-R2只修改严格分支：当`g17OldWorldAllowed=true`时同时替代原版`IsFlyable()`和`CanFlyInZone()`两个谓词；否则保留原版成对检查。安全策略函数未改，室内、城市、禁飞、竞技场、实例地图和配置黑名单仍受控。

```text
G17R2_PREIMAGE_SHA256=537e5c350baa5f4a90bd0ec38c6b6858360e287aeabd75ab54050b4432e50755
G17R2_POSTIMAGE_SHA256=73d52ac0feb67a32822fc0bf086a9174ba7ef0bc186223cdc8a690f48fccb9e2
G17R2_LOCAL_AUTOTESTS=10/10_PASS
G17R2_WINDOWS_BUILD=PASS_USER_CONFIRMED
G17R2_59961_RUNTIME=FAIL_NOT_HERE
G17R2A_GATE_DIAGNOSTIC=NOT_RUN
```
