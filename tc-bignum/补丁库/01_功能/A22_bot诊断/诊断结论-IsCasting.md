# 诊断结论：游荡 bot 无法对话 = `IsCasting()` 为真

> 用户 `.bd` 输出：**只有 `IsCasting()` 标红，其余全绿**
> 日期：2026-08-02

---

## 一、`.bd` 一屏定位，比我猜三轮强

前面我猜了三次（conf默认值 / 传送状态 / conf.d加载）全错。
做了诊断工具之后，**一条指令就定位到了**。

**教训已记**：连续两次归因失败，立刻做诊断工具，不要猜第三次。

---

## 二、为什么 `IsCasting()` 会一直为真

### 直接原因：bot 在吃东西 / 喝水

`bot_ai.cpp:6050-6061`：

```cpp
//drink
if (... && GetManaPCT(me) < 75 && urand(0, 100) < 20)
    me->CastSpell(me, GetRation(true), true);      // 喝水

//eat
if (... && GetHealthPCT(me) < 80 && urand(0, 100) < 20)
    me->CastSpell(me, GetRation(false), true);     // 吃东西
```

**这些是 channel（引导）类法术**（`bot_ai.cpp:118-145` 的法术表，
如 80级用 57073/45548），标准持续时间 **20-30 秒**。

引导期间：
```
me->GetCurrentSpell(CURRENT_CHANNELED_SPELL) != nullptr
    -> IsCasting() == true
    -> bot_ai.cpp:7704 拦截 -> SendCloseGossip() -> 对话框关闭
```

### 为什么游荡 bot 特别容易撞上

游荡 bot 一路打怪，血蓝经常不满。
`bot_ai.cpp:4414` 那条自主行为里就包含了觅食休息。

**所以你右键时，它大概率正在吃/喝。**

### 为什么"等10秒"那次没解决

我上次让你等 10 秒是为了验证**传送状态**，
而吃喝的引导是 **20-30 秒**，10 秒不够。

**这是个巧合性的误导** —— 两个假设都涉及"等待"，但时长和机制完全不同。

---

## 三、这是上游的设计，不是 bug

`IsCasting()` 拦截对话是**合理的**：
玩家在施法时也不能开对话框，bot 同理。

**但对"招募游荡bot"这个场景，它是个障碍** ——
你不可能盯着 bot 等它吃完。

---

## 四、三个解法（推荐第 1 个）

### 解法 1：`.bd fix` 增强 —— 打断施法（推荐，改动最小）

`.bd fix` 已经能清传送状态和补 GOSSIP 标记，
**再加一条：打断当前施法。**

```cpp
// 中断所有正在进行的施法（包括吃喝引导）
c->InterruptNonMeleeSpells(true);
```

用法变成：
```
.bf come        叫过来
.bd fix         打断它吃饭
右键            立刻能对话
```

**优点**：不动上游逻辑，零风险
**缺点**：每次要多敲一条指令

### 解法 2：招募场景放宽 `IsCasting` 拦截

改 `bot_ai.cpp:7704`，让**开了 AllowHire 的游荡bot**不受施法拦截：

```cpp
// 原文
IsTempBot() || me->IsInCombat() || CCed(me) || IsCasting() || IsDuringTeleport() ||

// 改为（只放宽游荡bot且开关打开的情况）
IsTempBot() || me->IsInCombat() || CCed(me) ||
(IsCasting() && !(IsWanderer() && BotCfg::IsWanderingBotHireEnabled())) ||
IsDuringTeleport() ||
```

**优点**：一劳永逸，右键就能开
**缺点**：改了上游一处判断，需要验证不影响其他场景

### 解法 3：右键时自动打断

在 `OnGossipHello` 被拦截前，先尝试打断吃喝。
**改动更深，不推荐。**

---

## 五、建议

**两个一起做**：

1. **`.bd fix` 加打断** —— 立刻可用，测试方便
2. **解法 2** —— 长期体验好

解法 2 的风险很低，因为：
- 只在 `IsWanderer() && AllowHire` 双条件下才放宽
- 普通 bot、召唤物、临时bot 完全不受影响
- 开关关闭时行为与原版一致

---

## 六、顺带确认：其他条件全绿说明什么

你的 `.bd` 显示除 `IsCasting` 外全部通过，这证明：

| 项 | 状态 | 说明 |
|---|---|---|
| GOSSIP 标记 | **有** | 第一关没问题，我之前怀疑的"死锁"**不成立** |
| `AllowHire` 读到的值 | **true(1)** | **配置生效了**，step33 的 4 处改动全对 |
| `IsDuringTeleport` | false | 传送状态正常，`.bf come` 没问题 |
| step33 条件 | false | **我改的那行逻辑是对的** |

**所以 step33 不用推倒重做。** 它一直是对的，只是被前面一条
更早的条件（`IsCasting`）挡住了。

**这也说明"从头做"是不必要的** —— 幸好先做了诊断。
