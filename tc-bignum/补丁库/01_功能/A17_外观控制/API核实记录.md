# step31 `.disguise` + `.model` —— 开工前 API 核实记录

> 核实日期：2026-08-01
> 源码：`328950225/TrinityCore-NPCBOT-Eluna-zhCN` 分支 `NPCBOT-Eluna-zhCN-2026`
> 原则：全部实查，不凭记忆

---

## 一、`.disguise` 隐藏装备（保留属性）

### 核心 API

```
Player.h:1153      void SetVisibleItemSlot(uint8 slot, Item* pItem)   [public，904行起public段]
Player.cpp:12170   实现
Player.h:1082      Item* GetItemByPos(uint8 bag, uint8 slot) const    [public]
Player.h:552       #define INVENTORY_SLOT_BAG_0    255
```

### 实现（Player.cpp:12170，全文 14 行）

```cpp
void Player::SetVisibleItemSlot(uint8 slot, Item* pItem)
{
    if (pItem)
    {
        SetUInt32Value(PLAYER_VISIBLE_ITEM_1_ENTRYID + (slot * 2), pItem->GetEntry());
        SetUInt16Value(PLAYER_VISIBLE_ITEM_1_ENCHANTMENT + (slot * 2), 0, pItem->GetEnchantmentId(PERM_ENCHANTMENT_SLOT));
        SetUInt16Value(PLAYER_VISIBLE_ITEM_1_ENCHANTMENT + (slot * 2), 1, pItem->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT));
    }
    else
    {
        SetUInt32Value(PLAYER_VISIBLE_ITEM_1_ENTRYID + (slot * 2), 0);
        SetUInt32Value(PLAYER_VISIBLE_ITEM_1_ENCHANTMENT + (slot * 2), 0);
    }
}
```

### 【关键】为什么能"保留属性"

**它只写 `PLAYER_VISIBLE_ITEM_*` 这组【外观广播字段】，
完全不碰 `m_items`。**

对比 `Player.cpp:12185 VisualizeItem`，那个才动真实装备：

```cpp
m_items[slot] = pItem;                                    // <- 真实装备数组
SetGuidValue(PLAYER_FIELD_INV_SLOT_HEAD + (slot * 2), pItem->GetGUID());
pItem->SetOwnerGUID(GetGUID());
```

**结论**：

| 项 | 状态 |
|---|---|
| 别人看到的外观 | **空手裸装** |
| 属性加成 | **照常生效**（属性走 `m_items` 和光环，不走外观字段）|
| 背包/装备栏 | 东西还在 |
| 战斗力 | **完全不受影响** |

### 恢复方案

```cpp
Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);   // Player.h:1082
player->SetVisibleItemSlot(slot, item);
```

`GetItemByPos` 从真实装备栏取，所以能原样还原（含附魔）。

### 19 个装备槽（Player.h:554-577，实查）

```
0  HEAD        1  NECK       2  SHOULDERS   3  BODY(衬衣)
4  CHEST       5  WAIST      6  LEGS        7  FEET
8  WRISTS      9  HANDS     10  FINGER1    11  FINGER2
12 TRINKET1   13  TRINKET2  14  BACK       15  MAINHAND
16 OFFHAND    17  RANGED    18  TABARD
EQUIPMENT_SLOT_START = 0,  EQUIPMENT_SLOT_END = 19
```

**注意**：项链(1)、戒指(10/11)、饰品(12/13) **本来就不显示外观**，
设不设都一样。但为了逻辑统一，还是全槽处理。

**武器槽**：MAINHAND(15) / OFFHAND(16) / RANGED(17)

---

## 二、`.model` 任意生物换模型

### 核心 API

```
Unit.h:1595   virtual void SetDisplayId(uint32 modelId)
Unit.h:1598   void SetNativeDisplayId(uint32 displayId)
Unit.h:1594   uint32 GetDisplayId() const
Unit.h:1596   uint32 GetNativeDisplayId() const
Unit.h:1191   void DeMorph()
```

### 【重要】`DeMorph()` 就是现成的复位入口

`Unit.cpp:3461`，全文 4 行：

```cpp
void Unit::DeMorph()
{
    SetDisplayId(GetNativeDisplayId());
}
```

**`.model reset` 直接调它即可**，不用自己存原始 displayid。

### 【注意】Creature 覆写了 SetDisplayId，Player 没有

```
Unit.h:1595        virtual void SetDisplayId(uint32 modelId);
Creature.h:86      void SetDisplayId(uint32 modelId) override;    <- 覆写了
（Player.h 无覆写）
```

**含义**：
- 对 Creature 调 `SetDisplayId` 会走 Creature 的版本（可能额外处理碰撞体积等）
- 对 Player 调走 Unit 基类版本

**都能用**，但要知道行为可能有细微差异。

### 对玩家/BOSS 都有效

`SetDisplayId` 是 `Unit` 的方法，Player / Creature / NPCBot 都是 Unit 子类。

### 和已有工具的关系

- `.scene`（step28）**已经在存 display + scale**，路早就验证过
- `.dummy`（step23）用过 `SetNativeDisplayId` 做模型兜底

---

## 三、复用 step29/30 的现成资产

| 组件 | 来源 | 说明 |
|---|---|---|
| `Tok()` | cs_emote/cs_say | 参数切分 |
| `CollectNear()` | cs_emote | 半径收集（含 NPCBot/宠物保护）|
| `CollectByEntry()` | cs_emote | 按 entry 收集 |
| 别名表结构 | cs_emote | 中文/英文/数字三种写法 |
| 旧式注册语法 | 全部 | `std::vector<ChatCommand>` |
| 权限 | 全部 | `RBAC_PERM_COMMAND_WORLDTOOLS` |

---

## 四、【血泪教训】必须遵守的三条

### 1. SQL 占位符用 `{}` 不是 `%u`

```
DatabaseWorkerPool.h:99
void DirectPExecute(Trinity::FormatString<Args...> sql, Args&&... args)
    -> Trinity::StringFormat(...)     // fmt 库
```

**step29 因为写 `%u` 崩服一次。** 本批次凡写库一律 `{}`。
（`snprintf` 的 `%u` 是对的，那是标准 C，不要混淆）

### 2. 绝不在运行时调 `sObjectMgr->LoadXxx()`

`ObjectMgr` 的各种 `LoadXxx()` 设计上只在启动时调一次。
运行时调会让容器 rehash，**活着的对象持有的指针全部失效 -> 崩溃**。

**step29 v4 踩过这个坑。** 持久化只写库，用 `.respawn` 或重启生效。

### 3. 注册要改两处

```
1. ScriptLoader.h            加 void AddSC_xxx_commandscript();
2. AddCommandsScripts() 函数体   加 AddSC_xxx_commandscript();
```

**只加声明不加调用 -> 编译过但命令不存在。** step29 踩过。

---

## 五、设计方案

### `.disguise`

```
.disguise on              隐藏全身（19槽）
.disguise off             恢复
.disguise weapon          只隐藏武器（15/16/17）
.disguise armor           只隐藏护甲（其余槽）
.disguise toggle          切换
.disguise status          看当前状态
```

**保留属性** —— 这是用户明确要求的核心。

### `.model`

```
.model <ID或别名>          对选中目标
.model r <半径> <ID>       批量
.model entry <ID> <模型>   按 entry
.model me <ID>             自己
.model reset               复位（调 DeMorph）
.model reset r <半径>      批量复位
.model save                写库（creature_template.modelid1）
.model list [关键词]       查常用模型别名
```

---

## 六、待确认（写代码时验证）

| 项 | 说明 |
|---|---|
| 隐藏后换装/进副本会否被重置 | 大概率会（`VisualizeItem` 会重设）—— 先做成会话内有效 |
| Player 换模型后装备外观 | 可能错位，和 `.disguise` 联动可解 |
| `.model save` 写哪张表 | Creature 走 `creature_template.modelid1`；玩家无法持久化 |
