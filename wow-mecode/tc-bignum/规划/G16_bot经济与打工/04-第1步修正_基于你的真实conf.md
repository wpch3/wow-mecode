# G16 第1步（修正版）· 基于你的真实 conf

> 2026-08-09
> 我读了你上传的 `worldserver.conf.txt`，发现**和我上一版假设的不一样**。
> 这份是按你的**实际现值**重写的，直接照抄即可。

---

## 零、先纠正我上一版的两个错误

| 我上次说 | 你的实际值 | 结论 |
|---|---|---|
| `Buyer.Enabled = 0`（买家关着） | **= 1（早就开着）** | 我错了，收购功能一直在跑 |
| 建议你配 `forceIncludeItems` 保留坐骑 | **你已经配了 359 项** | 已经做好了，不用动 |

**所以"卖东西没人接盘"这个判断是错的** —— 你的 bot 一直在收购。

---

## 一、金色战刃的真凶（重新定位）

上一版我说是 `Bind.Pickup = 1`。**这个判断对，但不完整。**

实查过滤链（`AuctionHouseBotSeller.cpp:109-156`）：

```cpp
:116    if (prototype->GetQuality() >= MAX_AUCTION_QUALITY) continue;   // 品质>=7 排除
:120    if (excludeItems.count(itemId)) continue;                       // 黑名单
:124    if (includeItems.count(itemId))
:126    {   _itemPool[...].push_back(itemId);
:128        continue;                        // <<< 白名单直接入池，【跳过下面所有过滤】
:129    }
:132    switch (prototype->GetBonding())     // <<< 绑定过滤在这，白名单根本走不到
:138        case BIND_WHEN_PICKED_UP:
:139            if (!GetConfig(CONFIG_AHBOT_BIND_PICKUP)) continue;
```

**两条路都能让装备上架**：

```
路径1  白名单（你配的359项）  -> 无视绑定，一定上架
路径2  Bind.Pickup = 1        -> 所有拾取绑定装备都上架   <- 金色战刃走这条
```

**你的白名单 359 项里，38766-39006 段有 233 项**，
这一段是 WLK 的坐骑/趣味物品，**是你要保留的**。

所以修法是：**只关 `Bind.Pickup`，白名单一个字不动。**

---

## 二、真正要改的（按你的现值，只有 6 项）

### 改动清单

| 配置项 | 你的现值 | 改成 | 为什么 |
|---|---|---|---|
| `Bind.Pickup` | 1 | **0** | 金色战刃元凶；白名单里的坐骑不受影响 |
| `Alliance.Items.Amount.Ratio` | 0 | **100** | 联盟拍卖行是空的 |
| `Horde.Items.Amount.Ratio` | 0 | **100** | 部落拍卖行是空的 |
| `Items.Misc` | 0 | **1** | 漏掉一大批物品种类 |
| `ItemsPerCycle.Boost` | 200 | **5000** | 补货太慢 |
| `ItemsPerCycle.Normal` | 20 | **500** | 同上 |

### 数量（你要"上万"）

| 配置项 | 现值 | 改成 |
|---|---|---|
| `Items.Amount.Gray` | 0 | **2000** |
| `Items.Amount.White` | 3500 | **12000** |
| `Items.Amount.Green` | 3500 | **12000** |
| `Items.Amount.Blue` | 2500 | **8000** |
| `Items.Amount.Purple` | 1500 | **5000** |
| `Items.Amount.Orange` | 50 | **500** |
| `Items.Amount.Yellow` | 50 | **200** |

### 堆叠（材料成堆的关键）

| 配置项 | 现值 | 改成 |
|---|---|---|
| `RandomStackRatio.Consumable` | 20 | **100** |
| `RandomStackRatio.Gem` | 20 | **100** |
| `RandomStackRatio.Reagent` | 10 | **100** |
| `RandomStackRatio.Projectile` | 10 | **100** |
| `RandomStackRatio.TradeGood` | 10 | **100** |
| `RandomStackRatio.Generic` | 10 | **100** |
| `RandomStackRatio.Quest` | 10 | **100** |
| `RandomStackRatio.Key` | 10 | **100** |
| `RandomStackRatio.Misc` | 10 | **100** |

> **Weapon / Armor / Container / Recipe / Quiver / Glyph 保持 0**
> —— 它们 `MaxStackSize` 本来就是 1，改成 100 只是白跑一次 urand。

### 补货频率

```
AuctionHouseBot.Update.Interval = 1600   ->   3600
```

你要"每小时补货"，单位是秒，3600 = 1 小时。

---

## 三、直接照抄（复制粘贴替换）

打开 `D:\TC-Build\bin\RelWithDebInfo\worldserver.conf`，
用 Ctrl+H 逐条替换（**左边是你现在的行，右边是改后的**）：

```ini
AuctionHouseBot.Update.Interval = 1600
->
AuctionHouseBot.Update.Interval = 3600

AuctionHouseBot.Alliance.Items.Amount.Ratio = 0
->
AuctionHouseBot.Alliance.Items.Amount.Ratio = 100

AuctionHouseBot.Horde.Items.Amount.Ratio = 0
->
AuctionHouseBot.Horde.Items.Amount.Ratio = 100

AuctionHouseBot.Items.Misc = 0
->
AuctionHouseBot.Items.Misc = 1

AuctionHouseBot.Bind.Pickup = 1
->
AuctionHouseBot.Bind.Pickup = 0

AuctionHouseBot.ItemsPerCycle.Boost = 200
->
AuctionHouseBot.ItemsPerCycle.Boost = 5000

AuctionHouseBot.ItemsPerCycle.Normal = 20
->
AuctionHouseBot.ItemsPerCycle.Normal = 500

AuctionHouseBot.Items.Amount.Gray = 0
->
AuctionHouseBot.Items.Amount.Gray = 2000

AuctionHouseBot.Items.Amount.White = 3500
->
AuctionHouseBot.Items.Amount.White = 12000

AuctionHouseBot.Items.Amount.Green = 3500
->
AuctionHouseBot.Items.Amount.Green = 12000

AuctionHouseBot.Items.Amount.Blue = 2500
->
AuctionHouseBot.Items.Amount.Blue = 8000

AuctionHouseBot.Items.Amount.Purple = 1500
->
AuctionHouseBot.Items.Amount.Purple = 5000

AuctionHouseBot.Items.Amount.Orange = 50
->
AuctionHouseBot.Items.Amount.Orange = 500

AuctionHouseBot.Items.Amount.Yellow = 50
->
AuctionHouseBot.Items.Amount.Yellow = 200

AuctionHouseBot.Class.RandomStackRatio.Consumable = 20
->
AuctionHouseBot.Class.RandomStackRatio.Consumable = 100

AuctionHouseBot.Class.RandomStackRatio.Gem = 20
->
AuctionHouseBot.Class.RandomStackRatio.Gem = 100

AuctionHouseBot.Class.RandomStackRatio.Reagent = 10
->
AuctionHouseBot.Class.RandomStackRatio.Reagent = 100

AuctionHouseBot.Class.RandomStackRatio.Projectile = 10
->
AuctionHouseBot.Class.RandomStackRatio.Projectile = 100

AuctionHouseBot.Class.RandomStackRatio.TradeGood = 10
->
AuctionHouseBot.Class.RandomStackRatio.TradeGood = 100

AuctionHouseBot.Class.RandomStackRatio.Generic = 10
->
AuctionHouseBot.Class.RandomStackRatio.Generic = 100

AuctionHouseBot.Class.RandomStackRatio.Quest = 10
->
AuctionHouseBot.Class.RandomStackRatio.Quest = 100

AuctionHouseBot.Class.RandomStackRatio.Key = 10
->
AuctionHouseBot.Class.RandomStackRatio.Key = 100

AuctionHouseBot.Class.RandomStackRatio.Misc = 10
->
AuctionHouseBot.Class.RandomStackRatio.Misc = 100
```

**共 23 处。** `forceIncludeItems` / `forceExcludeItems` / `Buyer.Enabled`
**一个字都不要动**（你已经配好了）。

---

## 四、改完必做

### 1. 重启 worldserver

conf 不能热重载 AHBot 的物品池。

### 2. 强制重建（否则要等很久才填满）

进游戏或控制台执行：

```
.ahbot rebuild all
```

出处：`cs_ahbot.cpp:68`，实现 `AuctionHouseBot.cpp:512 Rebuild(bool all)`。

> 12 万条挂单不会瞬间填满，`rebuild` 只是把旧货标记过期。
> 真正填充靠每个周期补 5000 件，**大约 1-2 小时铺满**。

### 3. 验证

```
.ahbot status
```

会显示每个行、每个品质的当前数量 vs 目标
（`AuctionHouseBot.cpp:484 PrepareStatusInfos`）。

**进游戏检查三件事**：

```
[ ] 去【暴风城】拍卖行（不是加基森）-> 以前是空的，现在应该有货
[ ] 搜"战刃" -> 金色的拾取绑定战刃应该消失了
[ ] 搜"魔纹布" -> 应该是成堆的（20个/组），不是1个1个
[ ] 搜"坐骑"或看你白名单里的物品 -> 应该还在（白名单没动）
```

---

## 五、风险提示（务必看）

### 你的 Buyer 已经开着，这是刷钱风险

```
AuctionHouseBot.Buyer.Enabled = 1     <- 你的现值
```

bot 会**收购玩家挂的东西**。如果出价基准配得高，
可以：买垃圾 -> 挂拍卖 -> bot 高价收 -> 无限刷金。

**这次改动会放大这个风险** —— 因为 `Items.Amount` 调高了 3-4 倍，
bot 手里的"预算"会更多。

**建议**：改完先自己试一次
```
1. 买一组便宜材料（比如亚麻布）
2. 用略高于市价挂上去
3. 看 bot 会不会秒收
```

如果秒收且价格离谱，把这几项调低（在 conf 里搜 `Buyer.Price`）：

```
AuctionHouseBot.Buyer.Price.Gray / White / Green / Blue / Purple / Orange / Yellow
```

### 12 万条挂单的性能

`auctionhouse` 表会有 12 万行，每条对应一个 `item_instance`。

**先按这套配，观察 CPU 和内存**。如果卡：
- 把 `Items.Amount.*` 全部减半
- 或者只开中立行（把 Alliance/Horde 改回 0）

---

## 六、关于"每个物品上万个"

必须如实说明一个限制：

**武器/护甲/坐骑的 `MaxStackSize` 本身就是 1**：

```cpp
// AuctionHouseBotSeller.cpp:641
uint32 AuctionBotSeller::GetStackSizeForItem(ItemTemplate const* itemProto, SellerConfiguration& config) const
{
    if (config.GetRandomStackRatioPerClass(ItemClass(itemProto->GetClass())) > urand(0, 99))
        return urand(1, itemProto->GetMaxStackSize());     // 装备类 MaxStackSize=1
    else
        return 1;
}
```

`urand(1, 1)` 永远返回 1，**这是物品模板属性，不是 AHBot 限制**。

| 物品类型 | MaxStackSize | 能不能上万 |
|---|---|---|
| 材料/药水/食物 | 20 / 200 | **能**（堆叠 × 多条挂单） |
| 弹药 | 200 / 1000 | **能** |
| 武器/护甲/坐骑 | **1** | 只能靠挂单条数堆 |

改完后三个行合计约 **12 万条挂单**，装备类是"很多条各1件"，
材料类是"很多条各20个" —— 这已经是 3.3.5 能做到的上限。

---

## 附：实查位置

```
# ===== 过滤链（决定什么能上架）=====
AuctionHouseBotSeller.cpp:109   for (uint32 itemId = 0; itemId < sItemStore.GetNumRows(); ++itemId)
AuctionHouseBotSeller.cpp:116       品质 >= MAX_AUCTION_QUALITY(7) 排除
AuctionHouseBotSeller.cpp:120       excludeItems 黑名单
AuctionHouseBotSeller.cpp:123-129   includeItems 白名单 -> push_back + continue
                                    【关键：白名单跳过下面所有过滤】
AuctionHouseBotSeller.cpp:131-156   绑定过滤（白名单走不到这）
AuctionHouseBotSeller.cpp:138           case BIND_WHEN_PICKED_UP  <- 金色战刃
AuctionHouseBotSeller.cpp:641   GetStackSizeForItem
AuctionHouseBotSeller.cpp:644       return urand(1, itemProto->GetMaxStackSize());

# ===== 你的实际 conf 值（已核对）=====
AuctionHouseBot.Account = 2
AuctionHouseBot.Update.Interval = 1600          -> 3600
AuctionHouseBot.Seller.Enabled = 1
AuctionHouseBot.Buyer.Enabled = 1               <- 早就开着，我上次说错了
AuctionHouseBot.Alliance.Items.Amount.Ratio = 0 -> 100
AuctionHouseBot.Horde.Items.Amount.Ratio = 0    -> 100
AuctionHouseBot.Neutral.Items.Amount.Ratio = 100
AuctionHouseBot.Items.Vendor = 1
AuctionHouseBot.Items.Loot = 1
AuctionHouseBot.Items.Misc = 0                  -> 1
AuctionHouseBot.ItemsPerCycle.Boost = 200       -> 5000
AuctionHouseBot.ItemsPerCycle.Normal = 20       -> 500
AuctionHouseBot.Bind.Pickup = 1                 -> 0  【核心修复】
AuctionHouseBot.forceIncludeItems = 359 项      <- 不要动（含233项WLK坐骑段）
AuctionHouseBot.forceExcludeItems = 9 项        <- 不要动

# ===== 指令 =====
cs_ahbot.cpp:68    { "rebuild", HandleAHBotRebuildCommand }
cs_ahbot.cpp:70    { "status",  HandleAHBotStatusCommand }
AuctionHouseBot.cpp:484   PrepareStatusInfos
AuctionHouseBot.cpp:512   Rebuild(bool all)
```
