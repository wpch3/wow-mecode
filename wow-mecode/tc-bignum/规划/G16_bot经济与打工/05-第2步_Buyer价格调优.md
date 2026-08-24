# G16 第2步 · Buyer 价格调优（零编译）

> 2026-08-09
> 你的 `AuctionHouseBot.Buyer.Enabled = 1` 一直开着。
> 这一步把"bot 收购"调到合理，既能接盘又不会被刷钱。

---

## 零、好消息：刷钱风险比我上次说的小得多

我上一版警告"Buyer 开着会被无限刷金"。**实查后要修正这个说法。**

核心判定在 `AuctionHouseBotBuyer.cpp:162 RollBuyChance`：

```cpp
float itemBuyPrice = float(auction->buyout / item->GetCount());     // 你的挂单价
float itemPrice = float(item->GetTemplate()->GetSellPrice()         // 【物品自身的商店回收价】
                        ? item->GetTemplate()->GetSellPrice()
                        : GetVendorPrice(item->GetTemplate()->GetQuality()));
itemPrice *= 1.4f;                                                   // 加 40% 容差

// 挂单价越高于 itemPrice，chance 越低
float chance = std::min(100.f, std::pow(100.f,
    1.f + (1.f - itemBuyPrice / itemPrice) / GetConfig(CONFIG_AHBOT_BUYER_CHANCE_FACTOR)));
```

**关键：参照物是物品自己的 `SellPrice`（商店回收价），不是 `Baseprice`。**

所以：

```
你挂 亚麻布 1金一组   ->  亚麻布 SellPrice 才几铜  ->  itemBuyPrice 远高于 itemPrice
                      ->  chance 趋近 0  ->  bot 不买
```

**bot 只会买"接近或低于商店价"的东西**，天然不接受高价。

### 那 Baseprice 是干嘛的

```cpp
// AuctionHouseBotBuyer.cpp:348-360  GetVendorPrice
return sAuctionBotConfig->GetConfig(CONFIG_AHBOT_BUYER_BASEPRICE_GRAY);   // 等等
```

**只在物品 `SellPrice == 0` 时兜底**（有些物品没定义回收价）。
所以它影响的是少数特殊物品，不是全局定价。

### 真正的风险点

```cpp
// AuctionHouseBotBuyer.cpp:187
if (ahInfo->BuyItemCount > 5)
    chance *= 1.f / std::sqrt(itemBuyPrice / avgBuyPrice);
```

**同一物品挂超过 5 件时，会参考"市场均价"。**
如果你自己刷屏挂 100 件高价货把均价拉高，理论上能提高成交率。
——但这需要大量操作，且每件仍受 `SellPrice` 约束，**收益极低**。

**结论：默认配置下刷钱不划算，可以放心开着。**

---

## 一、你现在的 Buyer 配置

我在你上传的 conf 里没找到 Buyer 的细项，说明**全部走默认值**：

```
AuctionHouseBot.Buyer.Recheck.Interval        = 20      （分钟）
AuctionHouseBot.Buyer.Baseprice.Gray          = 3504
AuctionHouseBot.Buyer.Baseprice.White         = 5429
AuctionHouseBot.Buyer.Baseprice.Green         = 21752
AuctionHouseBot.Buyer.Baseprice.Blue          = 36463
AuctionHouseBot.Buyer.Baseprice.Purple        = 87124
AuctionHouseBot.Buyer.Baseprice.Orange        = 214347
AuctionHouseBot.Buyer.Baseprice.Yellow        = 407406
AuctionHouseBot.Buyer.ChanceMultiplier.*      = 100     （七个颜色都是）
AuctionHouseBot.Buyer.Chance.Factor           = 2.5
AuctionHouseBot.BidPrice.Min                  = 0.6
AuctionHouseBot.BidPrice.Max                  = 0.9
```

出处：`AuctionHouseBot.cpp:259-273` / `:307-308`

---

## 二、推荐调整（三项）

### 1. 收购频率：20 分钟 -> 5 分钟

```ini
AuctionHouseBot.Buyer.Recheck.Interval = 5
```

**理由**：你挂个东西要等 20 分钟才可能被看到，体验差。
5 分钟比较接近"有人在盯着拍卖行"的感觉。

> 上限是 `DAY / MINUTE` = 1440（`AuctionHouseBot.cpp:259 SetConfigMinMax`），
> 最小 1。别设 1，那样每分钟扫一遍全部挂单，费 CPU。

### 2. 三个拍卖行都要开

你之前只有中立行有货，改完 conf 后三个行都有了，
**买家也要三个都开**，否则主城拍卖行只进不出：

```ini
AuctionHouseBot.Buyer.Alliance.Enabled = 1
AuctionHouseBot.Buyer.Horde.Enabled = 1
AuctionHouseBot.Buyer.Neutral.Enabled = 1
```

> 如果 conf 里没有这三行，就加在 `AuctionHouseBot.Buyer.Enabled` 附近。

### 3. 提高绿蓝紫的收购意愿

```ini
AuctionHouseBot.Buyer.ChanceMultiplier.Gray   = 60
AuctionHouseBot.Buyer.ChanceMultiplier.White  = 80
AuctionHouseBot.Buyer.ChanceMultiplier.Green  = 130
AuctionHouseBot.Buyer.ChanceMultiplier.Blue   = 150
AuctionHouseBot.Buyer.ChanceMultiplier.Purple = 150
AuctionHouseBot.Buyer.ChanceMultiplier.Orange = 100
AuctionHouseBot.Buyer.ChanceMultiplier.Yellow = 100
```

**设计意图**：

| 品质 | 倍率 | 为什么 |
|---|---|---|
| 灰/白 | 60 / 80 | 垃圾就该卖商店，不该占拍卖行 |
| 绿/蓝/紫 | 130-150 | **这些才是玩家想出手的**，提高成交感 |
| 橙/黄 | 100 | 极稀有，保持默认，别让 bot 抢走 |

这个乘数直接乘在 chance 上（`:192`），150 = 意愿提高 50%。

---

## 三、完整改动（照抄）

```ini
AuctionHouseBot.Buyer.Enabled = 1
AuctionHouseBot.Buyer.Alliance.Enabled = 1
AuctionHouseBot.Buyer.Horde.Enabled = 1
AuctionHouseBot.Buyer.Neutral.Enabled = 1

AuctionHouseBot.Buyer.Recheck.Interval = 5

AuctionHouseBot.Buyer.ChanceMultiplier.Gray   = 60
AuctionHouseBot.Buyer.ChanceMultiplier.White  = 80
AuctionHouseBot.Buyer.ChanceMultiplier.Green  = 130
AuctionHouseBot.Buyer.ChanceMultiplier.Blue   = 150
AuctionHouseBot.Buyer.ChanceMultiplier.Purple = 150
AuctionHouseBot.Buyer.ChanceMultiplier.Orange = 100
AuctionHouseBot.Buyer.ChanceMultiplier.Yellow = 100
```

**Baseprice 七项和 Chance.Factor 不用动** —— 它们只在
物品没有 SellPrice 时兜底，默认值是合理的。

---

## 四、验证

```
[ ] 1. 重启服务器
[ ] 2. 拿一件绿装，按【略低于商店回收价的1.4倍】挂上去
[ ] 3. 等 5-10 分钟
[ ] 4. 收到"你的物品已售出"的邮件 -> 成功
```

**怎么算合理价格**：

```sql
-- 查你要挂的物品的商店回收价
SELECT `entry`, `name`, `SellPrice`, `BuyPrice`, `Quality`
FROM `world`.`item_template`
WHERE `entry` = 【物品ID】;
```

`SellPrice × 1.4` 就是 bot 眼里的"合理价"，
挂这个价以下**大概率被收**，挂 2 倍以上基本不会成交。

---

## 五、如果想测刷钱风险

```
1. 买 100 组亚麻布（成本很低）
2. 全部按 10 倍价格挂上去
3. 等一小时看有没有成交
```

按 `:174` 的公式，`itemBuyPrice / itemPrice = 10/1.4 ≈ 7.1`，
`chance = 100^(1 + (1-7.1)/2.5) = 100^(-1.44)` ≈ **0.0013%**，
基本不可能成交。

**如果实测发现能刷**，把 `Chance.Factor` 调小（默认 2.5，改成 1.5），
指数会更陡峭，高价成交率进一步降低。

---

## 附：实查位置

```
# ===== Buyer 决策逻辑 =====
AuctionHouseBotBuyer.cpp:76    Update(AuctionHouseType)
AuctionHouseBotBuyer.cpp:162   RollBuyChance   <- 决定买不买
AuctionHouseBotBuyer.cpp:164       if (!auction->buyout) return false;    没一口价不买
AuctionHouseBotBuyer.cpp:167       itemBuyPrice = buyout / count
AuctionHouseBotBuyer.cpp:168       itemPrice = SellPrice ? SellPrice : GetVendorPrice(quality)
                                   ^^^ 参照物是【商店回收价】，不是 Baseprice
AuctionHouseBotBuyer.cpp:170       itemPrice *= 1.4f    加40%容差
AuctionHouseBotBuyer.cpp:174       chance = min(100, 100^(1 + (1 - buyPrice/price) / CHANCE_FACTOR))
AuctionHouseBotBuyer.cpp:177-178   有人出价过 -> chance / 5
AuctionHouseBotBuyer.cpp:187-188   同物品>5件 -> 参考市场均价
AuctionHouseBotBuyer.cpp:192       chance *= GetChanceMultiplier(quality) / 100
AuctionHouseBotBuyer.cpp:201   RollBidChance   竞价逻辑（同理）
AuctionHouseBotBuyer.cpp:348-360   GetVendorPrice -> Baseprice.*（仅 SellPrice=0 时用）
AuctionHouseBotBuyer.cpp:371-383   GetChanceMultiplier -> ChanceMultiplier.*
AuctionHouseBotBuyer.cpp:390   BuyEntry
AuctionHouseBotBuyer.cpp:421   PlaceBidToEntry

# ===== 默认值 =====
AuctionHouseBot.cpp:259    Buyer.Recheck.Interval  默认20  范围[1, 1440]
AuctionHouseBot.cpp:260-266    Buyer.Baseprice.Gray..Yellow
AuctionHouseBot.cpp:267-273    Buyer.ChanceMultiplier.*  默认100
AuctionHouseBot.cpp:307-308    BidPrice.Min 0.6 / Max 0.9
AuctionHouseBot.h:119-133      对应的枚举
```
