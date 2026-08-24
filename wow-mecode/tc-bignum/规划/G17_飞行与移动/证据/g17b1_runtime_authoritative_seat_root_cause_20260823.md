# G17-B1 Runtime FAIL：权威座位已建立但movement seat=-1被误判

日期：2026-08-23

## 用户真实结果

萨拉斯军马34767自动接管后提示“异步入座或控制权建立失败”，status恢复为`INACTIVE auto=on area_allowed=true`。59961与随后地面坐骑尝试也出现同类失败。

关键日志在多次独立Vehicle GUID上完全一致：

```text
dragonFound=true
expectedDragon=<GUID X>
actualVehicle=<同一GUID X>
expectedSeat=0
actualSeat=-1
charmer=<玩家自身GUID>
```

因此不是Vehicle未生成、不是进错Vehicle、不是控制权未建立：`actualVehicle==expectedDragon`且`charmer==player`已经证明VehicleJoinEvent完成了载具绑定和控制权建立。唯一失败条件是B1 verifier把`Player::GetTransSeat()`当成权威座位号。

## 架构根因

指定上游提交`4e8762e`的`VehicleJoinEvent::Execute`先执行：

1. `Passenger->SetVehicle(Target)`；
2. `Seat->second.Passenger.Guid = Passenger->GetGUID()`；
3. 设置movement transport seat；
4. 对可控座位执行`SetCharmedBy(Passenger, CHARM_TYPE_VEHICLE, ...)`。

`Vehicle::Seats[].Passenger`是服务器Vehicle座位占用真值。`GetTransSeat()`只是`m_movementInfo.transport.seat`，可能被后续客户端movement状态短暂回写为-1；它不能推翻已存在的Vehicle、座位占用和charmer控制链。

B1旧verifier错误要求：`GetTransSeat()==expectedSeat`。因此在真实控制权已经建立时仍主动`ExitVehicle()`并Despawn，制造功能失败。

## B1R3修复

- 新增`GetAuthoritativePassengerSeatId()`，遍历`Vehicle::Seats`并用`vehicle->GetPassenger(seat)==player`取得服务器权威座位；
- verifier只以`actualVehicle==expected`、权威seat==expected、`charmer==player`三元组决定成功；
- `movementSeat`只保留为诊断字段，不再作为清理门；
- status同时输出`seat=<权威>`和`movementSeat=<移动状态>`；
- 仍对错误Vehicle、错误权威座位、错误charmer严格fail-closed。

前像SHA=`2c7594d0f1428a767570063ac90c5f816991bf1d883fe61e45a1a28902a68199`；后像SHA=`94ff80334783e8883f0811a1a7f76595d91b729cc43684f00273abb9d955628b`。

## 同批日志边界

- `[G11-PERCEPTION]`为既有G11只读日志，与G17座位误判无关；
- `ObjectVariables.ext:43 GetGUIDLow on invalidated Creature`是独立Lua销毁事件生命周期缺陷，G17失败清理反复Despawn Vehicle会增加触发次数，但它不是座位失败根因；
- `Map::Remove<Bot>FromMap ... not in grid`是独立NPCBot地图/网格清理问题，不是G17 Vehicle控制链根因。

当前先修G17-B1阻断；两条独立Lua/NPCBot缺陷保留到对应优化线，不用错误Lua替代C++结论。
