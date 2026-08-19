# 为什么 `.findmodel` 搜不到玩家模型 —— 以及怎么把 NPC 变成玩家角色

> 用户问：「模型替换的模型里为什么没有玩家的模型id？
> 我想把npc替换成我的模型或者其他的玩家角色模型」

---

## 一、直接回答：玩家模型【不在】我搜的那张表里

### 1.1 `.findmodel`（step32）搜的是什么

`cs_modelfind.cpp:495`：

```cpp
for (uint32 id = 0; id < sCreatureDisplayInfoStore.GetNumRows(); ++id)
{
    CreatureDisplayInfoEntry const* disp = sCreatureDisplayInfoStore.LookupEntry(id);
    ...
}
```

它遍历的是 **`CreatureDisplayInfo.dbc`** —— **生物**外观表。

### 1.2 玩家模型在【另一个】dbc

实查 `shared/DataStores/DBCStructure.h:397-404`：

```cpp
struct ChrRacesEntry
{
    uint32 ID;                      // 0   种族ID
    uint32 Flags;                   // 1
    uint32 FactionID;               // 2
    //uint32 ExplorationSoundID;    // 3
    uint32 MaleDisplayID;           // 4   <-- 【男性玩家模型】
    uint32 FemaleDisplayID;         // 5   <-- 【女性玩家模型】
    ...
```

**玩家模型在 `ChrRaces.dbc` 里，每个种族只有男/女两个**，
一共 10 个种族 x 2 = **20 个 displayId**。

### 1.3 为什么设计成两张表

因为**玩家模型和生物模型是两套东西**：

| | 生物模型（CreatureDisplayInfo） | 玩家模型（ChrRaces） |
|---|---|---|
| 数量 | 上万个 | **20 个** |
| 外观由什么决定 | displayId **一个数字全定了** | displayId 只定"种族+性别"，**脸/发型/肤色/装备要另外传** |
| 客户端怎么渲染 | 直接查表贴模型 | **要额外请求一个"镜像数据"包** |

**关键差别**：一个兽人男性的 displayId 是固定的，
但**同样这个 displayId 可以是光头也可以是长发、可以穿板甲也可以裸奔**。

这些细节 displayId 里没有 —— 所以不能像生物那样"填个数字就完事"。

---

## 二、好消息：核心里【已经有】这个功能

TrinityCore（以及你这个 NPCBot 分支）自带一张表专门干这事：

### `world.creature_template_outfits`

实查建表语句（`sql/base/world_npcbots.sql:5907`）：

```sql
CREATE TABLE `creature_template_outfits` (
  `entry`      int unsigned NOT NULL,          -- 哪个NPC
  `race`       tinyint unsigned DEFAULT '1',   -- 种族 1-11
  `gender`     tinyint unsigned DEFAULT '0'    COMMENT '0 for male, 1 for female',
  `skin`       tinyint unsigned DEFAULT '0',   -- 肤色
  `face`       tinyint unsigned DEFAULT '0',   -- 脸型
  `hair`       tinyint unsigned DEFAULT '0',   -- 发型
  `haircolor`  tinyint unsigned DEFAULT '0',   -- 发色
  `facialhair` tinyint unsigned DEFAULT '0',   -- 胡子/耳环等
  `head`       int unsigned DEFAULT '0',       -- 以下都是【装备显示ID】
  `shoulders`  int unsigned DEFAULT '0',
  `body`       int unsigned DEFAULT '0',       -- 衬衣
  `chest`      int unsigned DEFAULT '0',
  `waist`      int unsigned DEFAULT '0',
  `legs`       int unsigned DEFAULT '0',
  `feet`       int unsigned DEFAULT '0',
  `wrists`     int unsigned DEFAULT '0',
  `hands`      int unsigned DEFAULT '0',
  `back`       int unsigned DEFAULT '0',       -- 披风
  `tabard`     int unsigned DEFAULT '0',       -- 战袍
  PRIMARY KEY (`entry`)
);
```

**你的库里已经有两条现成范例**（`world_npcbots.sql:5935`）：

```sql
INSERT INTO `creature_template_outfits` VALUES
(70551,2,0,0,14,9,7,5,0,0,0,0,59194,64674,0,36248,0,0,0),
(70552,2,0,0,14,9,7,5,0,0,0,0,59194,64674,0,36248,0,0,0);
--     ^ ^ ^  ^ ^ ^ ^
--     | | |  | | | +-- facialhair=5
--     | | |  | | +---- haircolor=7
--     | | |  | +------ hair=9
--     | | |  +-------- face=14
--     | | +----------- skin=0
--     | +------------- gender=0 (男)
--     +--------------- race=2 (兽人)
```

---

## 三、它是怎么工作的（三步，全部实查）

### 第1步：服务端启动时把 displayId 换成玩家模型

`ObjectMgr.cpp:9000-9016`：

```cpp
co.gender = fields[i++].GetUInt8();
switch (co.gender)
{
    case GENDER_FEMALE:
        _creatureTemplateStore[entry].Modelid1 = rEntry->FemaleDisplayID;   // 从ChrRaces取
        break;
    case GENDER_MALE:
        _creatureTemplateStore[entry].Modelid1 = rEntry->MaleDisplayID;
        break;
}
_creatureTemplateStore[entry].Modelid2 = 0;
_creatureTemplateStore[entry].Modelid3 = 0;
_creatureTemplateStore[entry].Modelid4 = 0;
_creatureTemplateStore[entry].unit_flags2 |= UNIT_FLAG2_MIRROR_IMAGE;  // <-- 【关键】
```

**注意最后一行**：打上 `UNIT_FLAG2_MIRROR_IMAGE` 标志。

官方注释写得很清楚：`// Needed so client requests mirror packet`

### 第2步：客户端看到这个标志，主动来要"镜像数据"

客户端发 `CMSG_GET_MIRRORIMAGE_DATA`。

### 第3步：服务端回复完整外观

`SpellHandler.cpp:604-627`：

```cpp
CreatureOutfitContainer const& outfits = sObjectMgr->GetCreatureOutfitMap();
CreatureOutfitContainer::const_iterator it = outfits.find(unit->GetEntry());
if (it != outfits.end())
{
    WorldPacket data(SMSG_MIRRORIMAGE_DATA, 68);
    data << guid;
    data << uint32(unit->GetNativeDisplayId());
    data << uint8(it->second.race);             // 种族
    data << uint8(it->second.gender);           // 性别
    data << uint8(unit->GetClass());            // 职业
    data << uint8(it->second.skin);             // 肤色
    data << uint8(it->second.face);             // 脸
    data << uint8(it->second.hair);             // 发型
    data << uint8(it->second.haircolor);        // 发色
    data << uint8(it->second.facialhair);       // 胡子
    data << uint32(0);                          // 公会（战袍纹章用）
    for (uint8 i = 0; i != MAX_CREATURE_OUTFIT_DISPLAYS; ++i)
        data << uint32(it->second.outfit[i]);   // 12件装备
    SendPacket(&data);
    return;
}
```

**这就是为什么玩家模型需要"两段式"** —— displayId 只是个壳，
真正的外观靠这个包。

> **NPCBot 自己也在用这套机制**（`bot_ai.cpp:447`）：
> ```cpp
> (const_cast<CreatureTemplate*>(me->GetCreatureTemplate()))->unit_flags2 |= UNIT_FLAG2_MIRROR_IMAGE;
> ```
> 所以你的 NPCBot 能显示成玩家外观，走的就是这条路。

---

## 四、你现在就能做的：把某个 NPC 变成玩家角色

### 4.1 最简单：抄你自己的角色

**第1步：查你自己的外观数据**

```sql
SELECT
    `name`       AS `角色名`,
    `race`       AS `种族`,
    `gender`     AS `性别`,
    `skin`       AS `肤色`,
    `face`       AS `脸型`,
    `hair`       AS `发型`,
    `haircolor`  AS `发色`,
    `facialstyle` AS `胡子`
FROM `characters`.`characters`
WHERE `name` = '你的角色名';
```

**第2步：把这些值填进 outfits 表**

```sql
-- 把 70001 换成你想改的那个 NPC 的 entry
-- 后面 8 个值用上一步查到的
INSERT INTO `world`.`creature_template_outfits`
    (`entry`, `race`, `gender`, `skin`, `face`, `hair`, `haircolor`, `facialhair`)
VALUES
    (70001,    1,      0,        5,      3,      2,      7,           1)
ON DUPLICATE KEY UPDATE
    `race`=VALUES(`race`), `gender`=VALUES(`gender`),
    `skin`=VALUES(`skin`), `face`=VALUES(`face`),
    `hair`=VALUES(`hair`), `haircolor`=VALUES(`haircolor`),
    `facialhair`=VALUES(`facialhair`);
```

**第3步：重启服务端**

`LoadCreatureOutfits()` 只在启动时跑，**没有单表 reload 指令**。

### 4.2 想让它穿装备

`head` / `shoulders` / ... 这些字段填的是**装备的 displayId**，
**不是物品ID**。

查装备 displayId：

```sql
SELECT `entry` AS `物品ID`, `name` AS `名字`, `displayid` AS `外观ID`
FROM `world`.`item_template`
WHERE `name` LIKE '%霜之哀伤%';
```

拿 `displayid` 填进对应槽位。

> 范例里 `(70551,2,0,0,14,9,7,5, 0,0,0,0,59194,64674,0,36248,0,0,0)`
> 后面那串就是：waist=59194, legs=64674, wrists=36248

---

## 五、三个种族/性别的取值参考

### 种族 ID（`race` 字段）

```
1  人类      2  兽人      3  矮人      4  暗夜精灵   5  亡灵
6  牛头人    7  侏儒      8  巨魔      10 血精灵     11 德莱尼
```

**注意没有 9**（那是被砍掉的种族）。

### 性别（`gender`）

```
0 = 男    1 = 女
```

### 肤色/脸型/发型的取值范围

**每个种族不一样**，而且是 `CharSections.dbc` 定的。

**最省事的办法**：**照抄现有角色的数值**（4.1 那个 SQL），
不要自己瞎试 —— 填了超范围的值客户端会显示成默认或者出错。

---

## 六、要不要我做个指令？

现在这套要手动跑 SQL + 重启，比较麻烦。

**我可以做一个 `.npcface` 指令**（名字待定）：

```
.npcface copy <你的角色名>     把选中NPC变成这个角色的样子
.npcface race <种族> <性别>    只改种族性别
.npcface show                  看当前设置
.npcface clear                 还原成原本的生物模型
```

**技术上要解决的点**（我先想到这些）：

| 问题 | 解法 |
|---|---|
| `LoadCreatureOutfits()` 没有单表 reload | 调 `sObjectMgr->LoadCreatureOutfits()` 全表重载（表很小，代价可接受） |
| 改完 `Modelid1` 要刷新已在世界的 NPC | 和 step55 一样，`InitializeQueriesData(QUERY_DATA_CREATURES)` |
| 客户端缓存 | 让它走出视野再回来，或清 `creaturecache.wdb` |
| `UNIT_FLAG2_MIRROR_IMAGE` 要在运行时加上 | `unit->SetUnitFlag2(UNIT_FLAG2_MIRROR_IMAGE)` |

**待实查**（做之前要确认）：

```
1. ObjectMgr.h  LoadCreatureOutfits()      访问段
2. ObjectMgr.h  GetCreatureOutfitMap()     访问段
3. Unit.h       SetUnitFlag2()             访问段
4. 运行时改 outfits 后，已生成的 Creature 要怎么刷新
```

**要做的话告诉我**，我按老规矩：先实查 API + 出方案，你确认了我再写代码。

---

## 七、一句话总结

**`.findmodel` 搜不到玩家模型，是因为玩家模型不在 `CreatureDisplayInfo.dbc` 里，
而在 `ChrRaces.dbc`（每种族只有男女2个）。**

**但你想要的功能核心已经有了** —— `world.creature_template_outfits` 表，
填好种族/性别/脸型/装备，重启即可。你的库里还有两条现成范例可以照抄。
