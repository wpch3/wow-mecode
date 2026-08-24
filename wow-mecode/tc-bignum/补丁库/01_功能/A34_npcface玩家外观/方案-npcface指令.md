# `.npcface` —— 把 NPC 变成玩家角色外观（方案，排后面做）

> 用户：「可以做指令也做一份sql模板吧，但是排在后面」
>
> **SQL 模板已交付**（`sql/NPC变玩家外观-模板.sql`），现在就能用。
> 这份文档是**指令的设计方案**，等前面的事做完再实现。

---

## 一、SQL 模板已经能用了

`sql/NPC变玩家外观-模板.sql`，五项自检通过。

**最常用的就一段**（2.1 节），改两个变量就行：

```sql
SET @npc_entry := 70001;             -- 目标NPC
SET @char_name := '你的角色名';       -- 抄谁的样子

INSERT INTO `world`.`creature_template_outfits`
    (`entry`, `race`, `gender`, `skin`, `face`, `hair`, `haircolor`, `facialhair`)
SELECT
    @npc_entry, `race`, `gender`, `skin`, `face`, `hair`, `haircolor`, `facialstyle`
FROM `characters`.`characters`
WHERE `name` = @char_name
ON DUPLICATE KEY UPDATE ...;
```

**它直接从 characters 表抄你的外观**，不用手填数字。

**改完要重启服务端**（`LoadCreatureOutfits()` 只在启动时跑）。

---

## 二、指令要解决 SQL 解决不了的三件事

| SQL 的痛点 | 指令怎么解决 |
|---|---|
| 每次都要重启服务端 | 调 `sObjectMgr->LoadCreatureOutfits()` 热重载 |
| 要先查 entry 才能改 | 直接**选中 NPC** 就行 |
| 改完看不到效果 | 自动刷 QueryData + 提示清客户端缓存 |

---

## 三、指令设计

```
.npcface copy <角色名>        把选中的NPC变成这个角色的样子
.npcface race <种族> <性别>   只改种族性别（其它用默认值0）
.npcface set <字段> <值>      单独改一项（skin/face/hair/haircolor/facialhair）
.npcface gear <槽位> <外观ID> 穿装备（head/chest/legs/...）
.npcface show                 看当前设置
.npcface clear                还原成原本的生物模型
.npcface reload               手动重载 outfits 表

--- 中文别名 ---
.npcface 复制 / 种族 / 设置 / 装备 / 查看 / 清除 / 重载
```

---

## 四、API 已全部实查（含访问段）

### 4.1 读写 outfits

```
ObjectMgr.h:1230   void LoadCreatureOutfits()                       public(947段)
ObjectMgr.h:1488   CreatureOutfitContainer const& GetCreatureOutfitMap() const  public(947段)
ObjectMgr.h:1241   void InitializeQueriesData(QueryDataGroup mask)   public(947段)
```

### 4.2 CreatureOutfit 结构（`ObjectMgr.h:173`）

```cpp
struct CreatureOutfit
{
    uint8 race;
    uint8 gender;
    uint8 face;
    uint8 skin;
    uint8 hair;
    uint8 facialhair;
    uint8 haircolor;
    uint32 outfit[MAX_CREATURE_OUTFIT_DISPLAYS];
};
typedef std::unordered_map<uint32, CreatureOutfit> CreatureOutfitContainer;
```

> **注意字段顺序**：`face` 在 `skin` 前面，和 SQL 表的列顺序**不一样**
> （SQL 是 skin,face,hair,haircolor,facialhair）。写代码时别搞混。

### 4.3 读玩家外观（`.npcface copy` 用）

全部 `Player.h` **public(904段)**：

```
Player.h:1014   Gender GetNativeGender() const
Player.h:1016   uint8 GetSkinId() const
Player.h:1018   uint8 GetFaceId() const
Player.h:1020   uint8 GetHairStyleId() const
Player.h:1022   uint8 GetHairColorId() const
Player.h:1024   uint8 GetFacialStyle() const
```

**好处**：目标玩家**在线时**可以直接从内存读，不用查库。
不在线再走 `characters` 表。

### 4.4 运行时补标志

```
Unit.h:961   void SetUnitFlag2(UnitFlags2 flags)      public(811段)
UnitDefines.h:188   UNIT_FLAG2_MIRROR_IMAGE = 0x00000010
```

---

## 五、三个实现难点（已想好解法）

### 5.1 热重载会不会有副作用

`LoadCreatureOutfits()` 第一行是：

```cpp
_creatureOutfitStore.clear();       // ObjectMgr.cpp:8963
// 官方注释：for reload case (test only)
```

**官方自己写了 "for reload case"**，说明设计上支持重载。

**但要注意**：它同时会改 `_creatureTemplateStore[entry].Modelid1`
（ObjectMgr.cpp:9004）。重载时**已经被改过的 entry 不会自动还原** ——
因为它只对表里现存的记录做修改。

**所以 `.npcface clear` 不能只删记录**，还要：
1. 从 `creature_template` 表读回原始 `modelid1`
2. 手动写回 `_creatureTemplateStore`
3. 移除 `UNIT_FLAG2_MIRROR_IMAGE`

**这是最容易出 bug 的一步**，实现时要重点测。

### 5.2 已在世界里的 NPC 怎么刷新

和 step55 改名一个坑：

```cpp
sObjectMgr->InitializeQueriesData(QUERY_DATA_CREATURES);
```

外加对**视野内**的实例：

```cpp
creature->SetDisplayId(newModelId);
creature->SetUnitFlag2(UNIT_FLAG2_MIRROR_IMAGE);
```

### 5.3 NPCBot 有自己的一套，优先级要说清楚

实查 `SpellHandler.cpp:604-627`：

```cpp
CreatureOutfitContainer const& outfits = sObjectMgr->GetCreatureOutfitMap();
CreatureOutfitContainer::const_iterator it = outfits.find(unit->GetEntry());
if (it != outfits.end())
{
    ... 发 outfits 的数据 ...
    return;                     // <-- 【命中就返回，不再看 npcbot_appearance】
}

// 下面才是 npcbot 的分支（:640）
if (bot->IsNPCBot())
{
    NpcBotAppearanceData const* appearData = BotDataMgr::SelectNpcBotAppearance(...);
```

**结论**：`creature_template_outfits` **优先级高于** `creature_template_npcbot_appearance`。

**所以**：
- 改**普通 NPC** -> 用 `.npcface`（这个新指令）
- 改 **NPCBot** -> 用 step51 的 `换外观工具.sql` 更合适
- **两个都设了** -> outfits 赢

指令里要加提示，避免你搞混。

---

## 六、待实查（实现时补）

| # | 查什么 | 用途 |
|---|---|---|
| 1 | `MAX_CREATURE_OUTFIT_DISPLAYS` 的值 | 装备数组大小 |
| 2 | `CharSections.dbc` 有没有加载进核心 | 校验 skin/face 范围（没有就不校验，靠抄现成角色） |
| 3 | `Creature::SetDisplayId` 访问段 | 刷新视野内实例 |
| 4 | `sObjectMgr->GetCreatureTemplate(entry)->Modelid1` 原始值从哪读回 | clear 还原 |

---

## 七、排期

按你的话「排在后面」，放在这个位置：

```
1. step59/60 验证通过（手头）
2. 羁绊系统第2步（你要先做的）
3. PlayerBot 跟随 + 自动上线
4. 对话系统（羁绊第3步）
5. 【本项】.npcface 指令        <-- 在这
```

**SQL 模板现在就能用**，不用等指令。
