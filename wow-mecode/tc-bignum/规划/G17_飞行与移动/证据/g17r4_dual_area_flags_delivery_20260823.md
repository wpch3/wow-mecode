# G17-R4 双Area飞行标志Windows包交付证据

日期：2026-08-23

## 触发证据

- R3真实Windows服务端构建与客户端MPQ安装：PASS；
- 59961普通按钮旧世界：FAIL；
- 湿地`IsFlyableArea()`：用户实测`nil`；
- 无头骑士坐骑外层48025可召唤，但它是按位置/骑术选择隐藏内层法术的混合包装，不能证明纯飞行直接法术可用。

## R3确定缺陷

R3只为地图0/1的948行增加`0x00000400`，而3.3.5a扩展区域的正常可飞行行通常同时具有`0x00000400 | 0x00004000`。

本地原始DBC统计：

- 地图0：499行全部不含两位；
- 地图1：473行全部不含两位；
- 地图530：342行同时含两位；
- 地图571：524行同时含两位，另有少量特殊组合。

湿地Area ID 11：

```text
stock 0x00000040
R3    0x00000440
R4    0x00004440
```

## R4改动边界

- 仍是R3选定的同一948行；
- R3到R4只给这些行的AreaTable字段4 OR `0x00004000`；
- 所有未选中行、所有其它字段、DBC头和字符串块不变；
- R1客户端Spell.dbc保持`dd250911...64ea`；
- 不修改Spell Effect/Aura/MiscValueB；
- 不修改服务端源码、服务端DBC或SQL；
- 保留R3服务端实时`Player::IsOutdoors()`门。

## 离线验证

```text
G17R4_DBC_TESTS=PASS
R3_TO_R4_CHANGED_ROWS=948
WETLANDS_FLAGS=0x00000040->0x00000440->0x00004440
POWERSHELL_AST_FILES=2
POWERSHELL_AST_ERRORS=0
REAL_MPQ_FORMAT_VERSION=2
REAL_MPQ_FILE_COUNT=4
REAL_MPQ_SPELL_SHA256=dd25091167f671764735ce88c78b66485c6d661fadf05d322574c261f6e464ea
REAL_MPQ_AREA_SHA256=1acef997a27f844a8abee9b477c44f2097f745168c6df85ece6b1a135568c233
FULL_INSTALL_LIFECYCLE_SIMULATION=PASS
IDEMPOTENT_RERUN=PASS
ROLLBACK_TO_R3_MPQ=PASS
ZHCN_CUSTOM_COLLISION_REFUSAL=PASS
CLIENT_SPELL_EFFECT_DISGUISE=False
```

## 交付

- 目录：`G17R4_Pure_Flight_Dual_Area_Flags_Windows_20260823/`
- ZIP：`G17R4_Pure_Flight_Dual_Area_Flags_Windows_20260823.zip`
- ZIP大小：`588095`字节
- ZIP文件数：`17`
- ZIP SHA-256：`fbf4a9adf5ada31e659a1b5f12572532959e3f45d79955ff6f79bb760f4935d3`
- 安装：`Run-G17R4-Client-Fix.cmd`
- 回滚：`Run-G17R4-Client-Rollback.cmd`
- R4客户端Area SHA-256：`1acef997a27f844a8abee9b477c44f2097f745168c6df85ece6b1a135568c233`

## 交付后真实Windows结果

用户随后回传`G17R4_CLIENT_MPQ_UPGRADE_RESULT=PASS`：根MPQ v2/4文件，内部Spell/Area哈希正确，Cache已删，服务端DBC未改，已知碰撞为0；但重启后湿地`IsFlyableArea()=nil`。因此最终状态为：

```text
G17R4_REAL_WINDOWS_CLIENT_RUNTIME=FAIL_API_STILL_NIL
G17R4_ACTUAL_DBC_LOAD_PROVEN=False
```

正确MPQ内容不能冒充`Wow.exe`实际加载证明。后续唯一入口已转G17-R5有效zhCN locale封装槽；R4禁止重复执行。
