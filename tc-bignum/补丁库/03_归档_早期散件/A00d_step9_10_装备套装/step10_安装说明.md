# 第 10 步：套装系统 v3 —— 安装说明

> v2 的问题你已经测过了：**测试清单全过、装备进背包、职业护甲全对**，
> 唯一的毛病是"散件太乱"。v3 就是来解决这个的。

---

## 一、v3 解决了什么

| 你提的问题 | v3 的做法 |
|---|---|
| 「大多有点乱，希望有对应的套装」 | 读 **ItemSet.dbc**，发的是成套的真套装，不是散件拼凑 |
| 「职业套装设成开关，太超模」 | `.gearset tier on/off`，**默认关闭**，存数据库按角色记忆 |
| 「刷副本多少次直接获得对应套装」 | 击杀**末王**计 1 次，攒够次数**必定解锁全套** |
| 「有些有声望限制有些有专业等级限制」 | 发放时**自动把声望刷到崇拜、专业点到 450**，装备本身不改 |
| 「团本和五人本次数不同，按稀有度定」 | 25人英雄 **3 次** / 10人普通 **8 次** / 英雄五人本 **15 次** / 普通五人本 **25 次** |

---

## 二、次数设计（按稀有度，越难给得越快）

```
25人英雄团本    3 次     最难的本，给最快
25人普通团本    5 次
10人英雄团本    5 次
10人普通团本    8 次
经典/TBC 团本  10 次     人数少但也算团本
英雄五人本     15 次     副本短，次数多
普通五人本     25 次
```

**为什么这样定**：一次 25 人英雄 ICC 的时间成本 ≈ 8 次英雄五人本。
按时间折算下来，每条路线拿到套装的耗时大致相当，
但**难本明显更划算** —— 这就是你要的「比无目的刷要有收益」。

数据在 `sql/09_tierset_dungeon_data.sql`，共 **68 个副本**，
不满意可以直接改表里的 `need_kills`，改完 `.reload` 或重启生效。

---

## 三、执行顺序

### 第 1 步：建表（5 个文件，逐个执行）

按你 SQL 客户端的脾气，每个文件**只有一条语句**，光标放语句里直接跑。

> ## 在哪个库执行？—— 随便哪个都行
>
> 每条语句都写死了 `库名.表名` 的完全限定形式，
> MySQL 会自己找对库，**不看你当前连的是哪个库**。
> 所以你不用切库，也不用写 `USE`。

| 文件 | 建的表 | 实际落在 |
|---|---|---|
| `sql/05_tierset_progress.sql` | `custom_raid_progress` | **characters** |
| `sql/06_tierset_unlocked.sql` | `custom_tierset_unlocked` | **characters** |
| `sql/07_tierset_config.sql` | `custom_gearset_config` | **characters** |
| `sql/08_tierset_dungeon_req.sql` | `custom_tierset_dungeon` | **world** |
| `sql/09_tierset_dungeon_data.sql` | 往上表插 68 行数据 | **world** |

**分库逻辑**：跟角色走的数据（谁刷了几次、谁解锁了什么、谁开了什么开关）
放 `characters`；全服共用的配置（副本门槛）放 `world`；权限放 `auth`。

> 全部写成不带反引号的 `库.表` 形式（如 `characters.custom_raid_progress`），
> 不会触发你客户端吞点号的问题，也不需要 `USE`。

### 第 2 步：注册 RBAC 权限（2 个文件）

| 文件 | 写入的表 | 实际落在 |
|---|---|---|
| `sql/10_rbac_gearset.sql` | `rbac_permissions`（权限 71005） | **auth** |
| `sql/11_rbac_gearset_link.sql` | `rbac_linked_permissions`（挂到 192 = 3级GM组） | **auth** |

> **这两条执行完必须重启 worldserver**。
> 未注册权限的命令，`ChatCommand.cpp:477` 的 `IsInvokerVisible` 会
> 直接把它过滤掉，表现为「命令不存在」。

### 第 2.5 步：自检（可选但建议）

执行 `sql/12_verify_gearset.sql`，一条 SELECT 查全部 7 项。

**预期**：返回 1 行 7 列数字，不报错就是装全了。
其中 `08_09_副本门槛_应为68` 这一列应该是 **68**，
`10_权限_应为1` 和 `11_权限组_应为1` 应该都是 **1**，
其余几列刚装完是 0 很正常（还没产生数据）。

**如果报错** `Table 'xxx' doesn't exist`，错误信息里的表名就告诉你缺哪个文件：

```
custom_gearset          -> 04_gearset_table.sql
custom_raid_progress    -> 05_tierset_progress.sql
custom_tierset_unlocked -> 06_tierset_unlocked.sql
custom_gearset_config   -> 07_tierset_config.sql
custom_tierset_dungeon  -> 08_tierset_dungeon_req.sql
```

### 第 3 步：放源文件

把 `patches/step10_cs_gearset.cpp` 复制到：

```
D:\TrinityCore\src\server\scripts\Commands\cs_gearset.cpp
```

> 如果你 v2 已经放过 `cs_gearset.cpp`，**直接覆盖**。

### 第 4 步：注册脚本（改 2 处）

打开 `D:\TrinityCore\src\server\scripts\Commands\cs_script_loader.cpp`

**第 1 处** —— 在其他 `void AddSC_xxx_commandscript();` 声明附近（约 47 行）加一行：

```cpp
void AddSC_gearset_commandscript();
```

**第 2 处** —— 在 `AddSC_commands()` 函数体里（约 93 行）加一行：

```cpp
    AddSC_gearset_commandscript();
```

> v2 如果已经加过，**不用重复加**。

### 第 5 步：RBAC 权限枚举

打开 `D:\TrinityCore\src\server\game\Accounts\RBAC.h`，
找到 `RBAC_PERM_MAX` 之前，确认有这一行（v2 加过就不用重复）：

```cpp
    RBAC_PERM_COMMAND_GEARSET                                = 71005,
```

### 第 6 步：重跑 CMake + 编译

> **只有 v2 没放过 cs_gearset.cpp 时才需要重跑 CMake。**
> 源文件是 `file(GLOB)` 扫描的，列表在生成时固定，新增文件必须重新生成。
> 如果只是覆盖已有的 .cpp，直接编译即可。

```
CMake GUI -> Configure -> Generate
VS2022 打开 D:\TC-Build\TrinityCore.sln -> 生成解决方案
```

### 第 7 步：重启 worldserver

启动时留意这一行日志，说明末王数据载入成功：

```
>> 套装系统：已载入 NNN 个副本的末王信息
```

---

## 四、测试清单

按顺序做，每一步都该有预期输出：

| # | 指令 | 预期 |
|---|---|---|
| 1 | `.gearset` | 弹出 9 项主菜单，可点击 |
| 2 | `.gearset help` | 打印完整帮助 |
| 3 | `.gearset 战士 264 tank` | 发 14 件板甲坦克装到背包 |
| 4 | `.gearset preview 法师 264` | **只列清单不发**，显示装等和平均装等 |
| 5 | `.gearset 盗贼 264 dps equip` | 发装备并**直接穿上** |
| 6 | `.gearset weapon 战士 264 tank` | 发单手剑 + 盾牌 |
| 7 | `.gearset trinket 法师 264` | 发 4 个饰品 |
| 8 | `.gearset config` | 显示 4 个开关状态，tier 应为**关** |
| 9 | `.gearset tier` | 提示「职业套装当前已关闭」 |
| 10 | `.gearset tier on` | 提示已开启 |
| 11 | `.gearset tier` | 提示「还没解锁任何套装」+ 引导看 progress |
| 12 | `.gearset progress` | 显示「还没有任何副本记录」+ 门槛参考 |
| 13 | 进副本杀末王 | 聊天框出现「通关 1/N 次」提示 |
| 14 | `.gearset progress` | 该副本进度显示 1/N，带百分比 |
| 15 | 刷够次数 | 出现「[套装解锁]」绿字 + 套装名 |
| 16 | `.gearset tier` | 弹出已解锁套装菜单（可点击领取） |
| 17 | `.gearset book` | 收藏册列出已解锁套装及件数 |
| 18 | 领一套有声望要求的 | 提示「[前置] 声望已补足：xxx -> 崇拜」 |
| 19 | `.gearset strip` | 全身装备卸到背包 |
| 20 | `.gearset bot 战士 264` | 选中 bot 后发装备到自己背包 |

---

## 五、v3 相对 v2 的技术改动

### 5-1 真套装从哪来

```cpp
// DBCStructure.h:960
struct ItemSetEntry {
    char const* Name[16];
    uint32 ItemID[MAX_ITEM_SET_ITEMS];        // 10 件
    uint32 SetSpellID[MAX_ITEM_SET_SPELLS];   // 8 个套装效果
    uint32 SetThreshold[MAX_ITEM_SET_SPELLS]; // 几件触发
};
```

游戏里所有 T 套的数据本来就在 `ItemSet.dbc` 里，
`sItemSetStore`（`DBCStores.h:152`）可以直接读。

启动后第一次用到时构建缓存（`BuildTierCache`），
按平均装等排序，之后查询是内存操作，不查库。

### 5-2 「末王」怎么判定

数据源是官方的 `instance_encounters` 表 + `DungeonEncounter.dbc`：

```
instance_encounters.entry  -> DungeonEncounterEntry.ID
DungeonEncounterEntry.Bit  -> 该 BOSS 在副本里的序号
```

取每张地图 **Bit 最大** 的那条，就是最后一个 BOSS。

这样做的好处：**不用手工维护 BOSS 列表**，
所有副本（包括你以后自己加的）自动适配。

> 你选的是「击杀最终BOSS才算1次」，
> 所以反复进出、只杀前面几个 BOSS 都**不会**刷计数。

### 5-3 声望/专业怎么解除

你选的是**补足前置**方案：

```cpp
// 声望：直接给到崇拜 42000
player->SetReputation(proto->RequiredReputationFaction, 42000);

// 专业：直接顶到 450
player->SetSkill(proto->RequiredSkill, 0, target, target);
```

**为什么不改装备**：
- 装备 entry 不动 → 其他玩家、其他途径拿到同款，行为完全一致
- 不新增物品条目 → 不污染 item_template
- 副作用只有「你的声望变高了」，这在 GM 端完全可接受

检查点已核实：
- `Player.cpp:11617` —— 声望检查（`CanUseItem`）
- `Player.cpp:11642` —— 专业检查（`CanUseItem`）
- `Player.cpp:11650` —— 法术要求

`FactionEntry.ReputationIndex < 0` 的阵营不参与声望系统，
代码里做了跳过（参考 `cs_modify.cpp:717` 官方写法）。

### 5-4 v2 的崩溃根因已规避

```cpp
// 数据库接口是 fmt 库，占位符是 {} 不是 %u
CharacterDatabase.PQuery("... WHERE owner_guid = {}", guid);   // 正确
```

v3 的**全部 12 处数据库调用**已用脚本逐一校验：
- 无 printf 式占位符
- `{}` 个数与参数个数完全匹配

`PSendSysMessage` 的 **全部 80+ 处格式串**也做了同样校验
（含跨行字符串拼接的情况），格式符与参数一一对应。

---

## 六、已做的验证

代码不是写完就给你的，做了这些验证：

**1. 真实编译**（不是"应该能编译"）

```
g++ -std=c++17 -Wall -Wextra   零错误零警告
```

把 GearSet 命名空间（962 行核心逻辑）抽出来，
配 134 行 TrinityCore 类型桩，真实编译通过。

**2. 25 项单元测试全过**

```
职业名解析      5 项   中文/英文/简称/未知值
护甲类型映射    4 项   板甲/锁甲/皮甲/布甲
定位评分        3 项   耐力装坦克分更高、力量装输出分更高、空指针保护
部位需求        2 项   12 部位 / 共 14 件
武器需求        3 项   盗贼双持、战士坦克带盾、战士输出双手
宝石选择        3 项   meta孔/治疗红孔/坦克黄孔
副本Key编码     3 项   同图不同难度、不同图、编码正确性
Gossip上限      2 项   29+3=32 不超限
```

**3. 崩溃点静态扫描**

- fmt 占位符：12 处数据库调用全部正确
- 格式串参数：80+ 处 `PSendSysMessage` 全部匹配
  （扫描时发现并修掉了 1 处「收藏册」缺参数的真实崩溃点）
- Gossip 32 条硬上限：`GossipDef.cpp:42` 的 ASSERT，
  分页设计为 29 内容 + 3 导航

**4. 3.3.5 分支 API 核对**

- `Map::GetDifficulty()` —— 3.3.5 用这个，**不是** master 的 `GetDifficultyID()`
- `ChatHandler::GetSessionDbcLocale()` —— 只在 ChatHandler 上，
  WorldSession 上没有（这里踩过，已修）

---

## 七、常见问题

**Q：刷了本但没有计数？**
A：三个可能：
1. 杀的不是**末王**（你选的规则是只有末王算）
2. 该副本没在 `custom_tierset_dungeon` 表里（68 个常见本已配，
   自定义本要自己加一行）
3. 击杀时人不在本里（组队时会给**本里的所有队友**计数，
   不在本里的不算）

**Q：解锁了但 `.gearset tier` 说没解锁？**
A：套装开关是**按角色**存的，换角色要重新 `.gearset tier on`。

**Q：想调整次数？**
A：直接改 `world.custom_tierset_dungeon` 表的 `need_kills`，
重启后生效（数据是启动时载入缓存的）。

**Q：想让某个副本掉指定套装？**
A：填该行的 `set_ids` 字段，逗号分隔的 ItemSet ID。
留空则按职业 + `ilvl_min`/`ilvl_max` 自动匹配。

---

## 八、出错怎么办

编译报错的话，**只发第一条 error** 给我，
带上文件名和行号，我来定位。

运行时崩服的话，看 `Server.log` 最后 20 行，
特别注意有没有 `fmt::format_error` 或 `ASSERT`。

---

## 九、v3.1 编译错误修复记录（2026-07-29）

你第一次编译报的 4 个错 + 2 个警告，全部已修。根因是我用桩代码验证时，
桩里的字段名是**按记忆写的**，和真实源码有出入 —— 桩写错了，验证自然过。

| VS 报错 | 行号 | 根因 | 修复 |
|---|---|---|---|
| C2039 `HolidayID` 不是成员 | 836, 883 | 真名是 **`HolidayId`**（小写 d） | 2 处已改 |
| C2065 `INVTYPE_WAND` 未声明 | 797 | 3.3.5 **没有这个枚举** | 改用 `INVTYPE_RANGEDRIGHT` |
| C2027 使用了未定义类型 `Group` | 1062 | `Player.h` 只有 `class Group;` 前向声明 | 补 `#include "Group.h"` |
| C4100 `role` 未引用 | 593 | 参数确实没用到 | 标注为 `/*role*/` |
| C4100 `player` 未引用 | 691 | 同上 | 标注为 `/*player*/` |

### 源码依据（这次都查了真实文件）

```cpp
// ItemTemplate.h:670   —— 是 HolidayId 不是 HolidayID
uint32 HolidayId;                    // id from Holidays.dbc

// ItemTemplate.h:287   —— 3.3.5 的魔杖用这个，没有 INVTYPE_WAND
INVTYPE_RANGEDRIGHT              = 26,

// Player.cpp:9516      —— 官方代码佐证：RANGEDRIGHT 映射到远程槽
case INVTYPE_RANGEDRIGHT:
    slots[0] = EQUIPMENT_SLOT_RANGED;

// Player.h:64          —— 只有前向声明，用 Group 的方法必须 include
class Group;

// cs_misc.cpp:27       —— 官方 cs_ 文件的标准做法
#include "Group.h"
```

### 修复后的重新验证

```
g++ -std=c++17 -Wall -Wextra -Wunused-parameter -Wshadow    零错误零警告
25 项单元测试                                                 全过
ItemTemplate 字段引用全量扫描                                 无错误引用
枚举常量全量扫描                                              全部确认存在
```

**同时校正了验证用的桩文件**，让它和真实源码一致：
- `HolidayID` → `HolidayId`
- 删掉不存在的 `INVTYPE_WAND`，补全真实的 `INVTYPE_RANGEDRIGHT` 等
- 补上 `Group` / `GroupReference` 桩

这样下次再验证就不会出现"桩过了但真实编译不过"的情况。

### 你要做的

**直接用新版覆盖** `D:\TrinityCore\src\server\scripts\Commands\cs_gearset.cpp`，
重新编译即可。**不用重跑 CMake**（文件名没变，只改了内容）。
