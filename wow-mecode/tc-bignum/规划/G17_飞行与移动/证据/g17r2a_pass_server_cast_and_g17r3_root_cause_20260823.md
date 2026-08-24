# G17-R2A PASS、服务端完整施法链 PASS 与 R3 根因锁定

日期：2026-08-23

## 原始只读报告

- 文件：`G17R2A_GATE_DIAGNOSTIC_RESULT_20260823.txt`
- 大小：2968字节
- SHA-256：`3dcbf440df2c4f744a4eab862c8101ffa02642e35dd5e17de214f8beeec4fe5e`
- 报告结论：`G17R2A_DIAGNOSTIC_RESULT=PASS`

该报告确认活动R2 EXE、R2源码、WorldFlight配置、R1客户端安装状态、自有MPQ和实际有效`Spell.dbc`均为锁定正确版本。

## 用户真实服务端链验收

用户随后在湿地执行未带triggered参数的：

```text
.cast self 59961
```

结果为成功召唤并上马。指定上游命令实现中未带参数的该命令使用非triggered完整施法检查，因此锁定：

```text
G17R2_END_TO_END_SERVER_CAST_CHAIN=PASS
G17_SERVER_UNTRIGGERED_CAST_RESULT=PASS_MOUNTED
G17_ADDITIONAL_SERVER_GATE_RELAXATION_REQUIRED=False
```

普通坐骑按钮仍失败不能再归因于R2 `SpellInfo.cpp`或继续放宽服务端。

## 客户端根因与R3边界

上游客户端同构数据结构确认`AreaTableEntry::IsFlyable()`要求`AREA_FLAG_OUTLAND`且无`AREA_FLAG_NO_FLY_ZONE`。锁定zhCN AreaTable中湿地及子区域缺OUTLAND位。

R3确定性补丁：

- 仅地图0/1；
- 排除硬禁飞、竞技场、实例、战场、城市和其它静态边界；
- 共948行；
- 每行只增加`0x00000400`；
- 客户端AreaTable SHA由`b0356ff4...62dd`变为`214c6935...b6a8`；
- 服务端DBC保持原样。

数据审计还证明地图0/1行和父区域组合后`AREA_FLAG_INSIDE`命中为0。因此必须在客户端门开放之前将服务端R2原像`73d52ac0...e9e2`收紧为R3后像`c3ec2237...bcbf`，在旧世界放行前实时检查`Player::IsOutdoors()`。

当前R3只完成离线代码和包验证；Windows构建、普通按钮召唤/起飞/移动/降落及室内拒绝尚待用户，不能写成最终Runtime PASS。
