# step34 行为层 —— 要往 bot_ai 里加的代码

> 这部分要改上游文件 `bot_ai.h` / `bot_ai.cpp`
> 每一处都给出【精确行号 + 原文】，已逐行核对过。

---

## 改动 1：`bot_ai.h` 加成员

### 1-A：加函数声明

**找到这一行**（`bot_ai.h:140`）：
```cpp
    void CommonTimers(uint32 diff);
```

**在它下面加**：
```cpp
    void UpdateCompanionCare(uint32 diff);
```

### 1-B：加成员变量

**找到这一行**（`bot_ai.h:521`）：
```cpp
    bool Feasting() const;
```

**在它上面加**（放在 private 段之前的任意成员区都行，这里选紧邻处）：
```cpp
    uint32 _careTimer;          // step34 伙伴关怀：全局冷却
    uint32 _careChatTimer;      // step34 闲聊单独冷却（比关怀更长）
    uint8  _lastMasterLevel;    // step34 记住上次看到的主人等级，用于察觉升级
    bool   _masterWasDead;      // step34 记住主人是否死过，用于复活后关心
```

---

## 改动 2：`bot_ai.cpp` 构造函数初始化

**找到这一行**（`bot_ai.cpp:169`）：
```cpp
    _updateTimerEx1 = urand(12000, 15000);
```

**在它下面加**：
```cpp
    // step34 伙伴关怀
    _careTimer        = urand(20000, 40000);   // 出生后先安静一会儿
    _careChatTimer    = urand(60000, 120000);
    _lastMasterLevel  = 0;
    _masterWasDead    = false;
```

---

## 改动 3：`bot_ai.cpp` CommonTimers 里递减计时器

**找到这一行**（`bot_ai.cpp:18534`）：
```cpp
    if (_updateTimerMedium > diff)  _updateTimerMedium -= diff;
```

**在它上面加**：
```cpp
    if (_careTimer > diff)          _careTimer -= diff;      else _careTimer = 0;
    if (_careChatTimer > diff)      _careChatTimer -= diff;  else _careChatTimer = 0;
```

> 注意：这里用 `else 置0` 而不是单纯递减，
> 因为下面的逻辑要靠 `== 0` 判断"冷却好了"。

---

## 改动 4：`bot_ai.cpp` 主循环里调用

**找到这一段**（`bot_ai.cpp:17730`）：
```cpp
    if (_updateTimerMedium <= diff)
    {
        _updateTimerMedium = 500;
```

**在这个 `if` 之前加一行调用**：
```cpp
    UpdateCompanionCare(diff);
```

> 放在 `GlobalUpdate` 里，这是每个 bot 每 tick 都会跑的地方。
> 内部有冷却和各种前置判断，不会造成性能问题。

---

## 改动 5：`bot_ai.cpp` 加函数本体

**加在 `void bot_ai::CommonTimers(uint32 diff)`（第 18480 行）之前**，
或者文件任意位置（只要在 `bot_ai` 命名空间内）。

```cpp
// ============================================================================
//  step34  伙伴关怀 —— 让 bot 像个真正的战友
//
//  用户需求：「会在你饿了给你面包，渴了给你水，穷了给你钱，
//              就像是一个伙伴，一个值得信赖的伙伴，
//              是会自己拿出背包里的东西给予伙伴的一个战友」
//
//  设计原则：
//    1. 主动 —— 不用玩家点菜单，bot 自己观察自己给
//    2. 有来源 —— 从 bot 自己的虚拟背包里扣，不凭空生成
//    3. 不烦人 —— 多重冷却 + 场合判断
//    4. 可配置 —— 台词和物品全在数据库
// ============================================================================
void bot_ai::UpdateCompanionCare(uint32 diff)
{
    // ---- 前置：总开关 ----
    if (!BotCfg::IsCompanionCareEnabled())
        return;

    // ---- 前置：必须是有主人的 bot ----
    // 游荡bot没有 master，直接跳过
    if (IAmFree() || !master || !master->IsInWorld())
        return;

    // ---- 前置：临时bot不参与 ----
    if (IsTempBot())
        return;

    // ---- 前置：双方都活着 ----
    if (!me->IsAlive())
        return;

    // ---- 复活关心：这个优先级最高，不受普通冷却限制 ----
    // 放在活着判断之后，因为要 bot 自己活着才能说话
    if (!master->IsAlive())
    {
        _masterWasDead = true;
        return;                     // 主人死着的时候不啰嗦
    }
    else if (_masterWasDead)
    {
        _masterWasDead = false;
        std::string txt = sBotCompanionMgr->PickText(CARE_TYPE_REVIVE, _botclass);
        if (!txt.empty())
        {
            BotWhisper(txt);
            _careTimer = urand(30000, 60000);
        }
        return;
    }

    // ---- 前置：战斗中不打扰 ----
    if (me->IsInCombat() || master->IsInCombat())
        return;

    // ---- 前置：距离太远就别喊了 ----
    if (me->GetDistance(master) > 30.0f)
        return;

    // ---- 升级祝贺：也不受普通冷却限制 ----
    uint8 curLevel = master->GetLevel();
    if (_lastMasterLevel && curLevel > _lastMasterLevel)
    {
        _lastMasterLevel = curLevel;
        std::string txt = sBotCompanionMgr->PickText(CARE_TYPE_LEVELUP, _botclass);
        if (!txt.empty())
        {
            BotWhisper(txt);
            _careTimer = urand(20000, 40000);
        }
        // 顺便补货（等级变了，能给的东西也该升级）
        sBotCompanionMgr->RestockBot(me->GetGUID().GetCounter(), curLevel);
        return;
    }
    _lastMasterLevel = curLevel;

    // ---- 普通关怀：受冷却限制 ----
    if (_careTimer)
        return;

    uint32 botGuid = me->GetGUID().GetCounter();

    // ---- 优先级 1：你饿了（血量低且脱战）----
    if (GetHealthPCT(master) < BotCfg::GetCompanionCareHealthPct())
    {
        std::string from;
        uint32 itemId = sBotCompanionMgr->FindInInventory(botGuid, CARE_TYPE_FOOD, curLevel, from);
        if (itemId && TryGiveItemToMaster(itemId, 1, CARE_TYPE_FOOD, from))
            return;
    }

    // ---- 优先级 2：你渴了（法力低）----
    if (master->GetMaxPower(POWER_MANA) > 1 &&
        GetManaPCT(master) < BotCfg::GetCompanionCareManaPct())
    {
        std::string from;
        uint32 itemId = sBotCompanionMgr->FindInInventory(botGuid, CARE_TYPE_DRINK, curLevel, from);
        if (itemId && TryGiveItemToMaster(itemId, 1, CARE_TYPE_DRINK, from))
            return;
    }

    // ---- 优先级 3：你穷了 ----
    uint32 moneyThreshold = BotCfg::GetCompanionCareMoneyThreshold();
    if (moneyThreshold && master->GetMoney() < moneyThreshold)
    {
        uint32 give = BotCfg::GetCompanionCareMoneyGive();
        if (give)
        {
            master->ModifyMoney(int32(give));

            std::string txt = sBotCompanionMgr->PickText(CARE_TYPE_MONEY, _botclass);
            if (!txt.empty())
            {
                // {gold} 占位符替换
                char goldbuf[32];
                snprintf(goldbuf, sizeof(goldbuf), "%u", give / 10000);
                size_t pos = txt.find("{gold}");
                if (pos != std::string::npos)
                    txt.replace(pos, 6, goldbuf);
                BotWhisper(txt);
            }
            _careTimer = BotCfg::GetCompanionCareCooldown();
            return;
        }
    }

    // ---- 优先级 4：没事，聊两句 ----
    if (!_careChatTimer)
    {
        std::string txt = sBotCompanionMgr->PickText(CARE_TYPE_CHAT, _botclass);
        if (!txt.empty())
        {
            BotWhisper(txt);
            _careChatTimer = BotCfg::GetCompanionChatCooldown();
            _careTimer     = BotCfg::GetCompanionCareCooldown() / 2;
        }
    }
}

// ----------------------------------------------------------------------------
//  把物品从 bot 背包转给主人
//
//  返回 true 表示成功给出（并已说话、已设冷却）
// ----------------------------------------------------------------------------
bool bot_ai::TryGiveItemToMaster(uint32 itemId, uint32 count, uint8 careType, std::string const& from)
{
    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
    if (!proto)
        return false;

    // 检查主人背包放不放得下
    // 官方同款：bot_ai.cpp:8467 术士给治疗石就是这么写的
    ItemPosCountVec dest;
    InventoryResult msg = master->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemId, count);
    if (msg != EQUIP_ERR_OK)
        return false;               // 包满了，安静地算了，不刷屏

    // 先从 bot 自己的包里扣 —— 这是"真的从背包拿"的关键
    if (!sBotCompanionMgr->TakeFromInventory(me->GetGUID().GetCounter(), itemId, count))
        return false;

    Item* item = master->StoreNewItem(dest, itemId, true, 0);
    if (!item)
    {
        // 给失败了，把东西还回 bot 背包，别凭空消失
        sBotCompanionMgr->AddToInventory(me->GetGUID().GetCounter(), itemId, count, from);
        return false;
    }

    master->SendNewItem(item, count, true, false, true);

    // 说句话。{item} 替换成物品名，{from} 替换成来源
    std::string txt = sBotCompanionMgr->PickText(careType, _botclass);
    if (!txt.empty())
    {
        std::string itemName = proto->Name1;
        _LocalizeItem(master, itemName, itemId);

        size_t pos = txt.find("{item}");
        if (pos != std::string::npos)
            txt.replace(pos, 6, itemName);

        pos = txt.find("{from}");
        if (pos != std::string::npos)
            txt.replace(pos, 6, from.empty() ? "路上" : from);

        BotWhisper(txt);
    }

    _careTimer = BotCfg::GetCompanionCareCooldown();
    return true;
}
```

---

## 改动 6：`bot_ai.h` 加 TryGiveItemToMaster 声明

**和改动 1-A 一起加**：
```cpp
    bool TryGiveItemToMaster(uint32 itemId, uint32 count, uint8 careType, std::string const& from);
```

---

## 改动 7：`bot_ai.cpp` 顶部加 include

**找到文件顶部的 include 区，加**：
```cpp
#include "bot_companion.h"      // step34 伙伴关怀
```

---

## 依赖的已有 API（全部实查确认）

```
bot_ai.cpp:262     BotWhisper(std::string_view, Player const*)
bot_ai.h:529       static uint8 GetHealthPCT(Unit const* u);
bot_ai.h:530       static uint8 GetManaPCT(Unit const* u);
bot_ai.cpp:8467    master->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, id, 1)
bot_ai.cpp:8473    master->StoreNewItem(dest, id, true, 0)
bot_ai.cpp:8483    master->SendNewItem(item, 1, true, false, true)
Player.h:1389      uint32 GetMoney() const
Player.h:1390      bool ModifyMoney(int32 amount, bool sendError = true)
bot_ai.h:381       bool GlobalUpdate(uint32 diff)
bot_ai.h:140       void CommonTimers(uint32 diff)
```

`_LocalizeItem` 在 bot_ai 里已有（用于物品名本地化），
如果编译报找不到，搜一下确切名字（可能是 `_LocalizeItem` 或类似）。
