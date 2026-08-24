# 修复 C2011「BotGiftKind 类型重定义」

## 原因

```text
C2011 "BotGiftKind":   类型重定义   bot_ai.cpp 79
C2011 "BotGiftReject": 类型重定义   bot_ai.cpp 90
```

**枚举现在有两份**：

```text
bot_companion.h        你刚加的（正确位置）
bot_ai.cpp:79 和 :90   当初 C2065 修复时加的（要删掉）
```

`bot_ai.cpp` 里 `#include "bot_companion.h"` 之后，
两份定义撞车 -> C2011。

**上一份修复文档我写了"第1步：从 bot_ai.cpp 删掉"，
但你可能只做了"第2步：加到 bot_companion.h"。**

---

## 修法：删掉 `bot_ai.cpp` 里的那两个 enum

### 打开文件

```text
D:\TrinityCore\src\server\game\AI\NpcBots\bot_ai.cpp
```

### 直接跳到第 79 行

VS 里按 `Ctrl+G`，输入 `75`，回车。

### 你会看到这样一段（大约 74-98 行）

```cpp
static constexpr GossipOptionIcon BOT_ICON_ON = GOSSIP_ICON_BATTLE;
static constexpr GossipOptionIcon BOT_ICON_OFF = GOSSIP_ICON_CHAT;

enum BotGiftKind : uint8
{
    BOT_GIFT_KIND_FOOD                  = 1,
    BOT_GIFT_KIND_DRINK                 = 2,
    BOT_GIFT_KIND_POTION                = 3,
    BOT_GIFT_KIND_EQUIP                 = 4,
    BOT_GIFT_KIND_RARE                  = 5,
    BOT_GIFT_KIND_JUNK                  = 6,
    BOT_GIFT_KIND_GENERIC               = 7
};

enum BotGiftReject : uint8
{
    BOT_GIFT_OK                         = 0,
    BOT_GIFT_REJECT_QUEST               = 1,
    BOT_GIFT_REJECT_BOUND               = 2,
    BOT_GIFT_REJECT_CONJURED            = 3,
    BOT_GIFT_REJECT_BAG                 = 4,
    BOT_GIFT_REJECT_LIMITED             = 5
};

static constexpr uint32 MAX_AMMO_LEVEL = 13;
```

### 把中间那两个 enum 整段删掉，变成

```cpp
static constexpr GossipOptionIcon BOT_ICON_ON = GOSSIP_ICON_BATTLE;
static constexpr GossipOptionIcon BOT_ICON_OFF = GOSSIP_ICON_CHAT;

static constexpr uint32 MAX_AMMO_LEVEL = 13;
```

**删除范围**：从 `enum BotGiftKind : uint8` 那一行，
一直到 `BOT_GIFT_REJECT_LIMITED = 5` 下面那个 `};`（含）。

> 上下两行 `BOT_ICON_OFF` 和 `MAX_AMMO_LEVEL` **保留不动**。
> 它们是上游原有的代码。

---

## 删完立刻自检（不用编译）

在 `bot_ai.cpp` 里 `Ctrl+F`：

| 搜索词 | 应有次数 |
|---|---|
| `enum BotGiftKind` | **0** |
| `enum BotGiftReject` | **0** |
| `BOT_GIFT_KIND_FOOD` | **3** |
| `#include "bot_companion.h"` | **1** |

在 `bot_companion.h` 里：

| 搜索词 | 应有次数 |
|---|---|
| `enum BotGiftKind` | **1** |
| `enum BotGiftReject` | **1** |
| `enum BotRequestType` | **1** |

**关键**：`enum BotGiftKind` 在两个文件里加起来必须**正好 1 次**。

---

## 关于 `E0065 应输入";"`

```text
E0065 应输入";"   MapDefines.h 48   game
E0065 应输入";"   MapDefines.h 48   scripts
```

**这条不用管。**

`MapDefines.h` 是 TrinityCore 的原始文件，我们从没碰过它。
`E0065` 前缀是 **"错误(活动)"** —— 这是 **IntelliSense** 报的，不是编译器。

IntelliSense 在解析被 C2011 打断的头文件链时经常误报，
**修好 C2011 后它会自己消失**。

判断方法：错误列表里
- **"错误"**（无括号）= 真编译错误，必须修
- **"错误(活动)"** = IntelliSense 提示，常常是误报

---

## 我的问题

上一份文档我把"删旧的"和"加新的"写成了第1步、第2步两个小节，
中间还隔了一大段代码，**很容易只做后半段**。

**改进**：以后"移动代码"类的操作，我会写成一句话的检查清单放最前面：

```text
[ ] 从 A 文件删除
[ ] 加到 B 文件
[ ] 搜索确认 A 文件已是 0 个
```
