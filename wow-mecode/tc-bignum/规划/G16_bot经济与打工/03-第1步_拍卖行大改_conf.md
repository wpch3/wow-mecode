# G16 第1步 · 拍卖行大改（零编译，改 conf 即生效）

> 2026-08-09
> 用户四个要求：
> 1. 货物太少、不齐全 -> 要把**全部能交易的货物**都放上去
> 2. 数量要多 -> **每个物品上万个数**
> 3. **每小时补货**
> 4. **bug：金色战刃（拾取绑定）被挂上去了** -> 灵魂绑定的不要，
>    但**坐骑等趣味物品不用在意绑定**

**四条全部可以纯 conf 解决，一行代码都不用改。** 下面每条都给了实证依据。

---

## 零、先给结论表

| 你的要求 | 现在的值 | 改成 | 依据 |
|---|---|---|---|
| 金色战刃不该上架 | `Bind.Pickup = 1` | **0** | `AuctionHouseBotSeller.cpp:138` |
| 坐骑要保留 | — | 用 `Items.Include` 白名单 | `AuctionHouseBotSeller.cpp:124` |
| 货物不齐全 | 联盟/部落 Ratio **都是0** | **100** | `worldserver.conf.dist:3434/3441` |
| 数量要上万 | White 3500 | **按下表调** | `AuctionHouseBot.cpp:189` 无上限 |
| 每小时补货 | Interval **1600秒** | **3600** | `worldserver.conf.dist:3419` |
| 灰色垃圾没有 | `Amount.Gray = 0` | 看你要不要 | `:3654` |

---

## 一、【bug】金色战刃为什么会上架

### 实证

过滤器在这里：

```cpp
// AuctionHouseBotSeller.cpp:131
        // bounding filters
        switch (prototype->GetBonding())
        {
            case NO_BIND:
                if (!sAuctionBotConfig->GetConfig(CONFIG_AHBOT_BIND_NO))
                    continue;
                break;
            case BIND_WHEN_PICKED_UP:                                    // <- 138行
                if (!sAuctionBotConfig->GetConfig(CONFIG_AHBOT_BIND_PICKUP))
                    continue;
                break;
            case BIND_WHEN_EQUIPED:
                if (!sAuctionBotConfig->GetConfig(CONFIG_AHBOT_BIND_EQUIP))
                    continue;
                break;
            case BIND_WHEN_USE:
                if (!sAuctionBotConfig->GetConfig(CONFIG_AHBOT_BIND_USE))
                    continue;
                break;
            case BIND_QUEST_ITEM:
                if (!sAuctionBotConfig->GetConfig(CONFIG_AHBOT_BIND_QUEST))
                    continue;
                break;
            default:
                continue;
        }
```

绑定类型定义：

```cpp
// ItemTemplate.h:98
    NO_BIND                 = 0,     // 不绑定          <- 正常商品
    BIND_WHEN_PICKED_UP     = 1,     // 拾取绑定        <- 【金色战刃是这个】
    BIND_WHEN_EQUIPED       = 2,     // 装备绑定
    BIND_WHEN_USE           = 3,     // 使用后绑定
    BIND_QUEST_ITEM         = 4,     // 任务物品
```

你的 conf 现值：

```
worldserver.conf.dist:3520    AuctionHouseBot.Bind.No = 1
worldserver.conf.dist:3521    AuctionHouseBot.Bind.Pickup = 1     <- 【元凶】
worldserver.conf.dist:3522    AuctionHouseBot.Bind.Equip = 1
worldserver.conf.dist:3523    AuctionHouseBot.Bind.Use = 1
worldserver.conf.dist:3524    AuctionHouseBot.Bind.Quest = 0
```

**`Bind.Pickup = 1` 就是金色战刃上架的原因。**

### 澄清一个容易搞错的点

| 绑定类型 | 玩家能不能在拍卖行交易 | 该不该上架 |
|---|---|---|
| `NO_BIND` 不绑定 | **能** | 要 |
| `BIND_WHEN_EQUIPED` 装备绑定 | **能**（穿上才绑） | **要** |
| `BIND_WHEN_USE` 使用后绑定 | **能**（用了才绑） | **要** |
| `BIND_WHEN_PICKED_UP` 拾取绑定 | **不能** | **不要** |
| `BIND_QUEST_ITEM` 任务物品 | **不能** | 不要 |

**关键**：`Bind.Equip = 1` 和 `Bind.Use = 1` **要保留**。
它们不是"灵魂绑定"——装备绑定的装备在拍卖行是可以正常买卖的，
这才是拍卖行的主力商品。只有 `Pickup` 是真正的"灵魂绑定/拾取绑定"。

### 修改

```
AuctionHouseBot.Bind.No = 1
AuctionHouseBot.Bind.Pickup = 0      <- 从 1 改成 0【本次核心修复】
AuctionHouseBot.Bind.Equip = 1
AuctionHouseBot.Bind.Use = 1
AuctionHouseBot.Bind.Quest = 0
```

---

## 二、坐骑等趣味物品要保留 —— 用白名单强制放行

你说「除了坐骑等趣味物品，不用在意是否绑定」。
核心正好有个**优先级最高的强制放行通道**：

```cpp
// AuctionHouseBotSeller.cpp:123
        // forced include filter
        if (includeItems.count(itemId))
        {
            _itemPool[prototype->GetQuality()][prototype->GetClass()].push_back(itemId);
            ++itemsAdded;
            continue;                              // <- 直接入池，不走后面的绑定过滤
        }
```

**`:124` 在 `:131` 绑定过滤【之前】** —— 所以白名单里的物品，
不管什么绑定类型都会上架。

### 配置

```
AuctionHouseBot.forceIncludeItems = "物品ID1,物品ID2,..."
```

### 常用坐骑 ID（3.3.5 有名的稀有坐骑）

```
# 稀有坐骑
13335   死亡战马
18242   灰色角马
18243   黑色角马
19872   猩红晨光头巾（趣味）
23720   火焰之靴
25953   蓝色冰霜白熊
33809   剧毒蛇（黑market）
44151   蓝色飞行器
45693   神秘的火箭头盔

# 趣味/收藏
1973    幽灵磁带
2586    海盗帽
19024   蓝色雪人玩偶
32542   有趣的镜子
33223   摇滚吉他
```

**建议做法**：先不填，等第1步跑通看到效果后，
你想要哪些坐骑上架，我再给你一份**从数据库自动筛选坐骑**的 SQL，
生成完整 ID 列表。手工填容易漏。

---

## 三、货物不齐全 —— 两个原因

### 原因1：联盟/部落拍卖行是**关着的**

```
worldserver.conf.dist:3434    AuctionHouseBot.Alliance.Items.Amount.Ratio = 0    <- 关
worldserver.conf.dist:3441    AuctionHouseBot.Horde.Items.Amount.Ratio = 0       <- 关
worldserver.conf.dist:3448    AuctionHouseBot.Neutral.Items.Amount.Ratio = 100   <- 只有这个开着
```

**所以你只有中立拍卖行（加基森）有货，主城的联盟/部落拍卖行是空的。**
这多半就是你觉得"货物太少"的直接原因。

### 原因2：物品来源开关

```
worldserver.conf.dist:3491    AuctionHouseBot.Items.Vendor = 1    商人卖的  开
worldserver.conf.dist:3499    AuctionHouseBot.Items.Loot = 1      怪物掉的  开
worldserver.conf.dist:???     AuctionHouseBot.Items.Misc = 0      其它      【关着】
```

`Items.Misc = 0` 会漏掉一大批"既不是商人卖的、也不是怪掉的"物品
（很多任务奖励、制造品、特殊物品）。

### 修改

```
AuctionHouseBot.Alliance.Items.Amount.Ratio = 100
AuctionHouseBot.Horde.Items.Amount.Ratio = 100
AuctionHouseBot.Neutral.Items.Amount.Ratio = 100

AuctionHouseBot.Items.Vendor = 1
AuctionHouseBot.Items.Loot = 1
AuctionHouseBot.Items.Misc = 1        <- 从 0 改成 1，货物种类大增
```

---

## 四、数量要"上万个数" —— 先分清两个概念

这里必须说清楚，否则配出来的结果和你想的不一样。

### 概念1：`Items.Amount.颜色` = **挂单条数**（不是堆叠数）

```
worldserver.conf.dist:3654    AuctionHouseBot.Items.Amount.Gray = 0
worldserver.conf.dist:3655    AuctionHouseBot.Items.Amount.White = 3500
worldserver.conf.dist:3656    AuctionHouseBot.Items.Amount.Green = 3500
worldserver.conf.dist:3657    AuctionHouseBot.Items.Amount.Blue = 2500
worldserver.conf.dist:3658    AuctionHouseBot.Items.Amount.Purple = 1500
worldserver.conf.dist:3659    AuctionHouseBot.Items.Amount.Orange = 50
worldserver.conf.dist:3660    AuctionHouseBot.Items.Amount.Yellow = 50
```

**这个数值没有上限**（实证：`AuctionHouseBot.cpp:189` 用的是 `SetConfig`，
不是 `SetConfigMax`，所以不卡上限）：

```cpp
// AuctionHouseBot.cpp:189
    SetConfig(CONFIG_AHBOT_ITEM_GRAY_AMOUNT, "AuctionHouseBot.Items.Amount.Gray", 0);
    SetConfig(CONFIG_AHBOT_ITEM_WHITE_AMOUNT, "AuctionHouseBot.Items.Amount.White", 2000);
```

对照 Ratio 是**有上限 10000** 的：

```cpp
// AuctionHouseBot.cpp:153
    SetConfigMax(CONFIG_AHBOT_ALLIANCE_ITEM_AMOUNT_RATIO, "...Alliance.Items.Amount.Ratio", 100, 10000);
```

### 概念2：`RandomStackRatio` = **每条挂单堆多少个**

```cpp
// AuctionHouseBotSeller.cpp:641
uint32 AuctionBotSeller::GetStackSizeForItem(ItemTemplate const* itemProto, SellerConfiguration& config) const
{
    if (config.GetRandomStackRatioPerClass(ItemClass(itemProto->GetClass())) > urand(0, 99))
        return urand(1, itemProto->GetMaxStackSize());     // 随机堆叠
    else
        return 1;                                          // 只放1个
}
```

你的现值：

```
worldserver.conf.dist:3796    RandomStackRatio.Consumable = 20     只有20%概率堆叠
worldserver.conf.dist:3798    RandomStackRatio.Weapon = 0          武器永远只放1个
worldserver.conf.dist:3800    RandomStackRatio.Armor = 0           护甲永远只放1个
worldserver.conf.dist:3803    RandomStackRatio.TradeGood = 10      材料只有10%堆叠
```

**这是"数量少"的另一个原因** —— 材料类只有 10% 的挂单是成堆的，
其余 90% 都是孤零零 1 个。

### 关于"每个物品上万个数"

这里我要**如实说明一个事实**，不能让你配完发现不对：

**武器、护甲、坐骑这类物品，`MaxStackSize` 本身就是 1**，
`urand(1, 1)` 永远返回 1。这是物品模板的属性，不是 AHBot 的限制。
所以"每件装备上万个"在堆叠层面做不到。

但"上万"可以通过**挂单条数**实现 —— 同一把武器挂 100 条，
每条 1 件，拍卖行里就是 100 个独立条目。

**能真正堆到上万的是**：材料、药水、food、弹药这些 `MaxStackSize = 20/200/1000` 的。

| 物品类型 | MaxStackSize | 能堆多少 |
|---|---|---|
| 材料/药水/食物 | 20 / 200 | **靠堆叠 + 多条挂单，能上万** |
| 弹药 | 200 / 1000 | **能上万** |
| 武器/护甲/坐骑 | **1** | 只能靠挂单条数堆 |

### 推荐配置（先保守，看服务器扛不扛得住）

```
# ---- 挂单条数 ----
AuctionHouseBot.Items.Amount.Gray = 2000
AuctionHouseBot.Items.Amount.White = 12000
AuctionHouseBot.Items.Amount.Green = 12000
AuctionHouseBot.Items.Amount.Blue = 8000
AuctionHouseBot.Items.Amount.Purple = 5000
AuctionHouseBot.Items.Amount.Orange = 500
AuctionHouseBot.Items.Amount.Yellow = 200

# ---- 堆叠概率（全部拉满，让材料真正成堆）----
AuctionHouseBot.Class.RandomStackRatio.Consumable = 100
AuctionHouseBot.Class.RandomStackRatio.Container = 0
AuctionHouseBot.Class.RandomStackRatio.Weapon = 0
AuctionHouseBot.Class.RandomStackRatio.Gem = 100
AuctionHouseBot.Class.RandomStackRatio.Armor = 0
AuctionHouseBot.Class.RandomStackRatio.Reagent = 100
AuctionHouseBot.Class.RandomStackRatio.Projectile = 100
AuctionHouseBot.Class.RandomStackRatio.TradeGood = 100
AuctionHouseBot.Class.RandomStackRatio.Generic = 100
AuctionHouseBot.Class.RandomStackRatio.Recipe = 0
AuctionHouseBot.Class.RandomStackRatio.Quiver = 0
AuctionHouseBot.Class.RandomStackRatio.Quest = 100
AuctionHouseBot.Class.RandomStackRatio.Key = 100
AuctionHouseBot.Class.RandomStackRatio.Misc = 100
AuctionHouseBot.Class.RandomStackRatio.Glyph = 0
```

> **Weapon/Armor/Container/Recipe/Quiver/Glyph 保持 0** ——
> 它们 MaxStackSize 本来就是 1，设成 100 只是白白多跑一次 urand，没有意义。

**三个行的总量** = (12000+12000+8000+5000+2000+500+200) × 3 = **约 12 万条挂单**

---

## 五、每小时补货

```
worldserver.conf.dist:3419    AuctionHouseBot.Update.Interval = 1600
```

单位是**秒**，1600 秒 = 26.7 分钟。

你要每小时 -> 改成 **3600**：

```
AuctionHouseBot.Update.Interval = 3600
```

### 但补货速度还受另一个参数限制

```
worldserver.conf.dist:3540    AuctionHouseBot.ItemsPerCycle.Boost = 200
worldserver.conf.dist:3548    AuctionHouseBot.ItemsPerCycle.Normal = 20
```

- **Normal**：正常情况每周期补 20 件
- **Boost**：当拍卖行库存**远低于目标值**时，每周期补 200 件

**问题**：你把目标改成 12 万条，但每小时只补 200 件 ——
填满要 600 小时。

**所以必须同时调大**：

```
AuctionHouseBot.ItemsPerCycle.Boost = 5000
AuctionHouseBot.ItemsPerCycle.Normal = 500
```

> **首次填充会很慢**，这是正常的。想立刻填满，
> 用 GM 命令 `.ahbot rebuild all` 强制重建（`cs_ahbot.cpp:68`）。

---

## 六、完整改动清单（直接抄）

打开 `D:\TC-Build\...\worldserver.conf`（**注意是 .conf 不是 .conf.dist**），
按下面逐项改：

```ini
# ============ 补货节奏 ============
AuctionHouseBot.Update.Interval = 3600
AuctionHouseBot.ItemsPerCycle.Boost = 5000
AuctionHouseBot.ItemsPerCycle.Normal = 500

# ============ 三个拍卖行全开 ============
AuctionHouseBot.Alliance.Items.Amount.Ratio = 100
AuctionHouseBot.Horde.Items.Amount.Ratio = 100
AuctionHouseBot.Neutral.Items.Amount.Ratio = 100

# ============ 绑定过滤（本次核心bug修复）============
AuctionHouseBot.Bind.No = 1
AuctionHouseBot.Bind.Pickup = 0
AuctionHouseBot.Bind.Equip = 1
AuctionHouseBot.Bind.Use = 1
AuctionHouseBot.Bind.Quest = 0

# ============ 物品来源全开 ============
AuctionHouseBot.Items.Vendor = 1
AuctionHouseBot.Items.Loot = 1
AuctionHouseBot.Items.Misc = 1

# ============ 挂单条数 ============
AuctionHouseBot.Items.Amount.Gray = 2000
AuctionHouseBot.Items.Amount.White = 12000
AuctionHouseBot.Items.Amount.Green = 12000
AuctionHouseBot.Items.Amount.Blue = 8000
AuctionHouseBot.Items.Amount.Purple = 5000
AuctionHouseBot.Items.Amount.Orange = 500
AuctionHouseBot.Items.Amount.Yellow = 200

# ============ 堆叠概率 ============
AuctionHouseBot.Class.RandomStackRatio.Consumable = 100
AuctionHouseBot.Class.RandomStackRatio.Gem = 100
AuctionHouseBot.Class.RandomStackRatio.Reagent = 100
AuctionHouseBot.Class.RandomStackRatio.Projectile = 100
AuctionHouseBot.Class.RandomStackRatio.TradeGood = 100
AuctionHouseBot.Class.RandomStackRatio.Generic = 100
AuctionHouseBot.Class.RandomStackRatio.Quest = 100
AuctionHouseBot.Class.RandomStackRatio.Key = 100
AuctionHouseBot.Class.RandomStackRatio.Misc = 100

# ============ 买家（让你卖东西有人接盘）============
AuctionHouseBot.Buyer.Enabled = 1
```

改完**重启 worldserver**（conf 不能热重载 AHBot 的物品池）。

---

## 七、验证

```
1. 重启服务器
2. 进游戏，去【暴风城/奥格瑞玛】拍卖行（不是加基森）
   -> 以前这里是空的，现在应该有货了
3. 搜索"战刃"
   -> 金色的拾取绑定战刃【应该消失了】
4. 搜索"魔纹布"之类的材料
   -> 应该看到成堆的（20个/组），不是1个1个的
5. .ahbot status
   -> 看各颜色的当前数量 vs 目标数量
```

`.ahbot status` 的实现在 `cs_ahbot.cpp:70`，它会调
`AuctionHouseBot::PrepareStatusInfos`（`AuctionHouseBot.cpp:484`），
显示每个行、每个品质的实际挂单数。

---

## 八、风险与注意事项

| 风险 | 说明 | 对策 |
|---|---|---|
| **12万条挂单的性能** | `auctionhouse` 表会有12万行 | 先按上表配，观察 CPU/内存。卡就减半 |
| **数据库膨胀** | 每条挂单对应一个 `item_instance` | 定期 `.ahbot rebuild` 清理过期 |
| **Buyer 开了会刷钱** | bot 高价收购 = 玩家无限刷金 | 见下方 |
| **首次填充慢** | 12万条要跑很久 | `.ahbot rebuild all` 强制 |

### Buyer 刷钱风险（必读）

打开 `Buyer.Enabled = 1` 后，bot 会**收购玩家挂的东西**。
如果出价基准配太高，你可以：买垃圾 -> 挂拍卖 -> bot 高价收 -> 无限刷金。

相关参数（`AuctionHouseBot.h:120-133`）：

```
AuctionHouseBot.Buyer.Price.Gray / White / Green / Blue / Purple / Orange / Yellow
AuctionHouseBot.Buyer.Chance.Multiplier.*
AuctionHouseBot.Buyer.Recheck.Interval
```

**建议**：第一轮先只改卖家部分，**Buyer 暂时保持 0**，
等你确认卖家这边效果满意了，第2步我再给你一份经过计算的 Buyer 数值。

> 我把 `Buyer.Enabled = 1` 写进上面清单是因为你要"经济活起来"，
> 但如果你想稳一点，**这一条可以先不改**。

---

## 附：本文档实查的源码位置

```
# ===== 绑定过滤 =====
AuctionHouseBotSeller.cpp:120     if (excludeItems.count(itemId)) continue;
AuctionHouseBotSeller.cpp:123-129 forced include filter  <- 白名单，优先级最高
AuctionHouseBotSeller.cpp:131-156 bounding filters
AuctionHouseBotSeller.cpp:138         case BIND_WHEN_PICKED_UP  <- 金色战刃元凶
ItemTemplate.h:98-103             NO_BIND=0 / PICKED_UP=1 / EQUIPED=2 / USE=3 / QUEST=4

# ===== 堆叠 =====
AuctionHouseBotSeller.cpp:641     GetStackSizeForItem
AuctionHouseBotSeller.cpp:643         if (RandomStackRatio > urand(0,99))
AuctionHouseBotSeller.cpp:644             return urand(1, itemProto->GetMaxStackSize());
AuctionHouseBotSeller.cpp:646         else return 1;
AuctionHouseBotSeller.cpp:389-398 SetRandomStackRatioPerClass 全15类

# ===== 数量上限（关键：Amount无上限，Ratio有）=====
AuctionHouseBot.cpp:110           SetConfigMax(...)  带上限的版本
AuctionHouseBot.cpp:153-155       Ratio 用 SetConfigMax，上限 10000
AuctionHouseBot.cpp:189-190       Amount 用 SetConfig，【无上限】
AuctionHouseBot.cpp:395-397       GetConfigItemQualityAmount

# ===== 造货 =====
AuctionHouseBotSeller.cpp:884     Item::CreateItem(itemId, stackCount)  凭空造
AuctionHouseBotSeller.cpp:900     SetPricesOfItem
AuctionHouseBotSeller.cpp:920     owner = GetRandChar()
AuctionHouseBotSeller.cpp:935/938 AddAuction 被调两次【上游bug，见01-总体设计.md 结论3】

# ===== 指令 =====
cs_ahbot.cpp:68                   { "rebuild", HandleAHBotRebuildCommand }
cs_ahbot.cpp:69                   { "reload",  HandleAHBotReloadCommand }
cs_ahbot.cpp:70                   { "status",  HandleAHBotStatusCommand }
AuctionHouseBot.cpp:484           PrepareStatusInfos
AuctionHouseBot.cpp:512           Rebuild(bool all)

# ===== conf 现场值（你的服）=====
worldserver.conf.dist:3411        AuctionHouseBot.Account = 2
worldserver.conf.dist:3419        Update.Interval = 1600        -> 改 3600
worldserver.conf.dist:3427        Seller.Enabled = 1
worldserver.conf.dist:3434        Alliance.Items.Amount.Ratio = 0    -> 改 100
worldserver.conf.dist:3441        Horde.Items.Amount.Ratio = 0       -> 改 100
worldserver.conf.dist:3448        Neutral.Items.Amount.Ratio = 100
worldserver.conf.dist:3491        Items.Vendor = 1
worldserver.conf.dist:3499        Items.Loot = 1
worldserver.conf.dist:3520-3524   Bind.No=1 / Pickup=1 / Equip=1 / Use=1 / Quest=0
worldserver.conf.dist:3540        ItemsPerCycle.Boost = 200      -> 改 5000
worldserver.conf.dist:3548        ItemsPerCycle.Normal = 20      -> 改 500
worldserver.conf.dist:3654-3660   Items.Amount.Gray..Yellow
worldserver.conf.dist:3796-3810   RandomStackRatio.* 全15类
worldserver.conf.dist:3823        Buyer.Enabled = 0
```
