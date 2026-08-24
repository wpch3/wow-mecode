# 规划 · 外观控制 + NPCBot 定制

> 立项：2026-08-01
> 状态：**已查证可行，等主线（.say）完成后开工**
> 来源：用户在 step29 期间提出的四个需求

---

## 一、四个需求汇总

| # | 需求 | 可行性 | 技术同源 |
|---|---|---|---|
| 1 | 隐藏玩家全身装备（大佬装萌新）| **已查证可行** | 外观控制 |
| 2 | 给任何生物换模型（含玩家/BOSS）| **已查证可行** | 外观控制 |
| 3 | NPCBot 用任意模型（含 HD 玩家模型）| **可行** | 同 #2 |
| 4 | NPCBot 出生裸装，只穿我给的 | 待查 botdatamgr | NPCBot |

**#1-#3 技术同源，建议合并成一批做。**

---

## 二、需求1：隐藏全身装备 `.disguise`

### 已核实 API

```
Player.h:1153      void SetVisibleItemSlot(uint8 slot, Item* pItem)   [public，904行起public段]
Player.cpp:12170   实现（12 行）
```

### 实现原理（查了实现，不是猜）

```cpp
void Player::SetVisibleItemSlot(uint8 slot, Item* pItem)
{
    if (pItem)
    {
        SetUInt32Value(PLAYER_VISIBLE_ITEM_1_ENTRYID + (slot * 2), pItem->GetEntry());
        SetUInt16Value(PLAYER_VISIBLE_ITEM_1_ENCHANTMENT + (slot * 2), 0, ...);
        SetUInt16Value(PLAYER_VISIBLE_ITEM_1_ENCHANTMENT + (slot * 2), 1, ...);
    }
    else
    {
        SetUInt32Value(PLAYER_VISIBLE_ITEM_1_ENTRYID + (slot * 2), 0);
        SetUInt32Value(PLAYER_VISIBLE_ITEM_1_ENCHANTMENT + (slot * 2), 0);
    }
}
```

**关键点**：它只改「别人看到什么」，**完全不碰 `m_items`**
（真实装备存在那儿，是 `Player.cpp:12185 VisualizeItem` 才动的）。

### 所以「大佬装萌新」完全成立

| 项 | 状态 |
|---|---|
| 外观 | **空手裸装** |
| 属性加成 | **照常生效** |
| 背包/装备栏 | 东西还在 |
| 战斗力 | 不受任何影响 |

### 设计

```
.disguise on          隐藏全身装备（19 个槽位循环设 nullptr）
.disguise off         恢复（遍历 m_items 重设回去）
.disguise weapon      只隐藏武器
.disguise armor       只隐藏护甲
.disguise <玩家名>    对指定玩家（GM 用）
```

**恢复逻辑**：`EQUIPMENT_SLOT_START` 到 `EQUIPMENT_SLOT_END` 循环，
`SetVisibleItemSlot(slot, m_items[slot])` 即可。

### 待确认

- 换装/进副本/重登后会不会被系统重置（大概率会，需要挂钩子或存标记）
- 是否要持久化（存 `character` 自定义列或单独表）

---

## 三、需求2+3：任意模型 `.model`

### 已核实 API

```
Unit.h:1595   virtual void SetDisplayId(uint32 modelId)
Unit.h:1598   void SetNativeDisplayId(uint32 displayId)
Unit.h:1594   uint32 GetDisplayId() const
Unit.h:1596   uint32 GetNativeDisplayId() const
```

**对玩家、NPC、BOSS、NPCBot 全部有效** —— 它们都是 `Unit` 的子类。

### 和已有工具的关系

`.scene`（step28）**已经在存 display + scale**，说明这条路早就验证过了。
`.dummy`（step23）也用过 `SetNativeDisplayId` 做模型兜底。

**缺的只是一个直接改模型的独立指令。**

### 官方 `.morph` 的三个不足（照 `.emote` 补 `.npc playemote` 的套路补）

| 官方 | 我们 |
|---|---|
| 只能对自己或选中的单个 | `r <半径>` / `entry <ID>` 批量 |
| 只能填数字 displayid | 常用模型中文别名表 |
| 不能持久化 | `save` 写 `creature_template.modelid` |
| 不能复位 | `reset` 回 native display |

### 设计

```
.model <ID或别名>              对选中目标
.model r <半径> <ID>           批量
.model entry <ID> <模型>       按 entry
.model me <ID>                 自己
.model reset                   复位到原生模型
.model save                    写库持久化
.model list [关键词]           查常用模型别名
```

### 关于 HD 玩家模型

**能用。** 模型在客户端侧，服务端只发 displayid，
客户端拿 ID 去 `CreatureDisplayInfo.dbc` 查路径。
你打了 HD 皮肤补丁之后，那些 displayid 指向的就是 HD 模型。

### 待确认

- 玩家换模型后，装备外观会不会错位（很可能会，和 `.disguise` 联动解决）
- `SetDisplayId` 对玩家是否需要额外发包刷新

---

## 三点五、【已定位】游荡 bot 无法招募 —— 不是 bug，是设计

用户反馈：野外游荡的机器人无法招募、甚至无法对话。

**源码实证**（`bot_ai.cpp:7703-7711`，`OnGossipHello`）：

```cpp
bool bot_ai::OnGossipHello(Player* player, uint32 /*option*/)
{
    if (... ||
        (!player->IsGameMaster() && (IsWanderer() || me->IsSummon())))
    {
        player->PlayerTalkClass->SendCloseGossip();   // <- 直接关掉对话框
        return true;
    }
```

**第 7707 行那个条件**：非 GM 玩家点游荡 bot，直接 `SendCloseGossip()`。

### 立即可验证

**`.gm on` 之后再去点那个 bot，应该就能对话了。**

### 设计意图

游荡 bot 是"世界背景 NPC"，让它们能被随便招募，
世界会被玩家搬空 —— 所以作者故意锁死。

### 三种改法

| 方案 | 做法 | 评价 |
|---|---|---|
| A. 直接放开 | 去掉 `IsWanderer()` 这个条件 | 简单，但世界会被搬空 |
| **B. conf 开关** | `NpcBot.Wandering.AllowHire = 0/1` | **推荐**，可控 |
| C. 限量招募 | 加冷却或数量上限 | 最平衡，工作量大 |

**推荐 B**，理由：改动小、可回退、符合 playerbot 那套
"默认行为要便宜，昂贵行为要显式开启"的设计哲学。

---

## 四、需求4：NPCBot 出生裸装

### 待查

```
botdatamgr.cpp          bot 数据初始化
4_world_generate_bot_equips.sql   装备生成 SQL
```

### 三个可能方向

| 方向 | 说明 | 风险 |
|---|---|---|
| A. 改 SQL | 让 `generate_bot_equips` 不生成 | 简单，但可能影响 bot 战力 |
| B. 加 conf 开关 | `NpcBot.StartNaked = 1` | 干净，要改源码 |
| C. 加指令 | `.npcbot strip` 一键脱光 | 最灵活 |

**推荐 C**，因为不影响默认行为，而且和 `.disguise` 技术同源。

### 注意

bot 脱光后**战力会掉**（装备属性没了）。
如果只想要「看起来裸」但保留属性，那就是 `.disguise` 那套
（`SetVisibleItemSlot(nullptr)`），**不是真脱装**。

**要先问清楚用户想要哪种。**

---

## 四点五、NPCBot 便捷指令构想（用户要求"多做点实用的"）

### 一键配置多个招募 bot（用户明确需求）

```
.bots preset tank1 heal1 dps3      按职责一键招满
.bots preset raid                  预设阵容：1T1H3D
.bots save <名字>                  存当前阵容
.bots load <名字>                  一键还原整队
.bots list                         看已存阵容
```

**技术要点**：NPCBot 的招募是逐个 gossip 的，
批量化需要绕过 gossip 直接调招募 API。要先查 `BotMgr` 的接口。

### 其他实用指令构想

| 指令 | 作用 |
|---|---|
| `.bots gear <等级>` | 一键给全队装备（复用 step9/10 的 gearset）|
| `.bots strip` | 全队脱装（**看起来裸但保属性** = `.disguise` 那套）|
| `.bots model <ID>` | 全队换模型（含 HD 玩家模型）|
| `.bots come` / `.bots stay` | 全队集合/原地待命 |
| `.bots res` | 全队复活 |
| `.bots spec <专精>` | 批量改专精 |
| `.bots info` | 一屏看全队状态 |

**设计原则**：全部支持"对整队生效"，
避免逐个点 bot —— 这是用户痛点。

### 复用资产

和 `.emote`/`.say` 一样能复用 `Tok`/别名表/`save` 模式。
另外 `.disguise` 做完后，`.bots strip` 直接调同一套逻辑。

---

## 五、排期建议

```
1. .emote 实测通过           <- 当前
2. .say                      <- 完成剧情工具链
   ────────── 主线告一段落 ──────────
3. .disguise + .model        <- 本文档 #1#2#3，一批做
4. NPCBot 定制               <- 本文档 #4 + 之前的中文化/扩容
```

**理由**：#1#2#3 都是「外观控制」，共用同一套目标选择代码
（`r 半径` / `entry` / 选中），一起做能复用大量逻辑。

---

## 六、复用资产

这三个指令能直接复用 `.emote` 的：

| 组件 | 说明 |
|---|---|
| `Tok()` | 参数切分 |
| `CollectNear()` | 半径收集（含 NPCBot/宠物保护）|
| `CollectByEntry()` | 按 entry 收集 |
| 别名表结构 | 中文/英文/数字三种写法 |
| `save` 模式 | 末尾可跟 save |
| 旧式注册语法 | `std::vector<ChatCommand>` |

**等于说 `.emote` 已经把地基打好了。**
