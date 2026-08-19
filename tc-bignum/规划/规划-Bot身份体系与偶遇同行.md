# 规划 · Bot 身份体系 与「偶遇同行」功能

> 制定：2026-08-02
> 缘起：用户澄清「我不是想要做这个开关，而是做一个功能」
> 需求：偶遇 NPC -> 一起升级同行；像 playerbot 一样不担心数量；
>       除 GM 外每人只能带 4 个；扩容出「剧情bot / 玩家bot / npcbot」多种身份

---

## 零、先纠正我上一步的理解偏差

step33 我做的是「**允许招募游荡bot**」——把一个不能对话的 NPC 变成可雇佣的。

**你要的不是这个。** 你要的是：

> 在野外**偶遇**一个 NPC，它**跟着你一起成长**，
> 像 playerbot 那样是**同伴**，不是雇佣兵。

区别在于**身份和关系**，不是权限开关。这份文档重新规划。

---

## 一、【关键发现】你要的东西，底层已经有八成了

实查 `bot_ai.cpp:SetStats()`，等级逻辑是这样的：

```cpp
// bot_ai.cpp:2152
uint8 mylevel = std::min<uint8>(master->GetLevel(), DEFAULT_MAX_LEVEL);
if (IsWanderer())
{
    // 游荡bot：自己按【杀怪数】升级
    // bot_ai.cpp:2170
    mylevel = std::max(mylevel, std::min(_baseLevel + _killsCount * XPGainMod / (mylevel*20), mapmaxlevel));
}
else
    mylevel += BotDataMgr::GetLevelBonusForBotRank(...);   // 普通bot：跟主人等级
```

### 这段代码告诉我们三件事

**第一：普通 bot 本来就「跟玩家一起升级」**

`mylevel = master->GetLevel()` —— 你 10 级它就 10 级，你 60 级它就 60 级。
**"一起升级"这个功能不用做，已经是默认行为。**

**第二：游荡 bot 已经有独立的成长系统**

```
bot_ai.cpp:15914   ++_killsCount;       <- 每杀一个记一次
bot_ai.cpp:2170    按 _killsCount 算等级
bot_ai.cpp:11636   杀得多掉落金币也多
```

源码里甚至留了注释：`//TODO: experience system for levelups`
**作者自己也想做经验系统，只是还没做。**

**第三：两套逻辑靠 `IsWanderer()` 一个开关切换**

这就是身份体系的**天然接缝** —— 我们要做的不是重写，是**在这个接缝上扩展**。

---

## 二、所以「偶遇同行」到底要做什么

拆成四件事，难度递增：

| 要素 | 现状 | 要做什么 |
|---|---|---|
| 一起升级 | **已有**（跟主人等级）| 零 |
| 数量限制 4 个 | **已有配置** | 改 conf |
| 能对话招募 | step33 已做 | 已完成 |
| **"偶遇"的仪式感** | 无 | **这才是真正要做的** |

### 「偶遇」是什么

现在的流程：走过去 -> 右键 -> 菜单 -> 付钱 -> 加入。**像买东西。**

你想要的：走过去 -> 它**主动说话** -> 有个理由 -> **同意同行**。**像遇到一个人。**

**所以核心是"交互脚本"，不是"bot 机制"。**

---

## 三、数量限制：4 个怎么设

### 现成配置（不用改代码）

```cpp
// botconfig.cpp:1172
uint8 BotCfg::GetMaxNpcBots(uint8 level)
{
    return _max_npcbots[std::min<size_t>(BRACKETS_COUNT - 1, level / 10)];
}
```

**按等级分档**，配置项是 `NpcBot.MaxBots`，要填满 BRACKETS_COUNT 个值。

**已实查确认精确值**：

```
botcommon.h:29   BRACKETS_COUNT = DEFAULT_MAX_LEVEL / 10 + 1
                 3.3.5 的 DEFAULT_MAX_LEVEL = 80  ->  80/10+1 = 9
botconfig.cpp:468  默认串 "1,1,1,1,1,1,1,1,1"   <- 正好 9 个
```

分档是：`0-9 / 10-19 / 20-29 / 30-39 / 40-49 / 50-59 / 60-69 / 70-79 / 80+`

**想让所有等级都是 4 个，填 9 个数**：

```ini
NpcBot.MaxBots = 4,4,4,4,4,4,4,4,4
```

**三条硬约束（源码实证，填错会崩服或被改）**：

| 约束 | 位置 | 后果 |
|---|---|---|
| **必须正好 9 个值** | botconfig.cpp:470 ASSERT | **直接崩服** |
| 单个值不能 > 39 | botconfig.cpp:487 | 被强制改成 39 |
| 后面的值不该比前面小 | botconfig.cpp:482 | 只警告，不阻止 |

> 想低等级少带、高等级多带，可以写 `1,1,2,2,3,3,4,4,4`。
> 全填 4 就是任何等级都能带 4 个。

### GM 不受限？

源码里 `GetMaxNpcBots()` 没有 GM 例外。要做的话，
在 `bot_ai.cpp:7770` 那个 `reason = 2` 的判断加 `!player->IsGameMaster()`。

**这是个小改动，可以并进 step33。**

---

## 四、Bot 身份体系设计（你要的"扩容"）

### 现有的身份（源码已有）

| 身份 | 判定 | 特征 |
|---|---|---|
| 普通 bot | 默认 | 跟主人等级，可雇佣，落库持久 |
| 游荡 bot | `IsWanderer()` | 自己按杀怪升级，四处走，不可雇佣 |
| 临时 bot | `IsTempBot()` | 不落库，用完消失 |
| 召唤物 | `me->IsSummon()` | 跟随召唤者 |
| bot 宠物 | `IsNPCBotPet()` | 属于某个 bot |

**已经是一套身份体系了**，只是没有你要的那几种。

### 建议新增的身份

#### 身份 A：剧情 bot（Story Bot）

```
用途：真龙纪元剧情里的同行 NPC，六卷主线的伙伴
特征：
  - 等级跟随玩家（复用普通bot逻辑）
  - 不可解雇（或解雇要确认）
  - 不占 4 个名额（剧情强制的不该挤掉玩家的选择）
  - 有专属对话（接 .say / .emote 工具链）
  - 剧情阶段推进时自动更换/离队
实现：加一个 flag，在 GetMaxNpcBots 判断时跳过
```

**这个和你的剧情规划直接对接** —— 六卷主线里那七位凡人主角，
就可以做成剧情 bot。

#### 身份 B：同伴 bot（Companion / 偶遇的那种）

```
用途：野外偶遇，愿意跟你走的
特征：
  - 等级跟随玩家
  - 占 4 个名额
  - 有"好感度"（可选，后期）
  - 招募方式是【对话触发】不是【付钱】
实现：本质就是普通bot + 特殊的招募入口
```

#### 身份 C：玩家 bot（Player-like）

```
用途：模拟真实玩家，在世界里活动
特征：
  - 有名字、有装备、会打招呼
  - 不可招募（它是"别的玩家"）
  - 会自己做事（游荡bot已有这个基础）
实现：游荡bot + 拟真社交（规划-NPCBot增强总蓝图 批次E）
```

### 身份怎么落地：加一个字段

现有的 `IsWanderer()` 是 bool，不够用了。建议：

```cpp
enum BotIdentity : uint8
{
    BOT_IDENTITY_NORMAL    = 0,   // 雇佣的普通bot
    BOT_IDENTITY_WANDERER  = 1,   // 游荡（已有）
    BOT_IDENTITY_STORY     = 2,   // 剧情bot
    BOT_IDENTITY_COMPANION = 3,   // 偶遇同伴
    BOT_IDENTITY_PLAYERLIKE= 4,   // 模拟玩家
};
```

存在哪：`characters` 库的 `characters_npcbot` 表加一列。

> **注意**：不要动 `_wanderer` 这个变量本身。
> 30+ 处代码在判断它，动了就是大手术。
> **新加一个字段，让 `IsWanderer()` 保持原样。**

---

## 五、「偶遇」怎么实现 —— 三个方案

### 方案 1：Eluna 脚本触发（推荐，零 C++ 改动）

```lua
-- 玩家靠近游荡bot时触发
-- 钩子：OnUpdate 或 OnAreaTrigger
function OnPlayerNear(player, bot)
    if 已触发过 then return end
    if 玩家等级 与 bot等级 相差 > 5 then return end
    if 随机数 > 触发概率 then return end

    bot:SendUnitSay("你也是要去" .. 地名 .. "吗？路上不太平...", 0)
    -- 给玩家一个 Gossip 选项："要一起走吗？"
end
```

**优点**：
- 完全不动 C++
- 剧情文案随便改，不用重编译
- 可以做得很丰富（不同地区不同对白）

**缺点**：
- Eluna 的 bot 绑定有限（见 `规划-NPCBot增强总蓝图` 批次C）
- 要先确认 Eluna 能不能拿到 bot 对象

**这是最该先试的方案。**

### 方案 2：改 bot_ai 的 OnGossipHello（中等）

在 step33 的基础上，把游荡 bot 的招募菜单改成"同行邀请"：

```
原：  [雇佣我 - 500金]
改：  [看起来你也在赶路，要一起吗？]
```

**优点**：体验统一，不依赖 Eluna
**缺点**：要改 C++，文案写死在代码里

### 方案 3：专门的"旅人"NPC 池（工程量大）

单独做一批不是游荡 bot 的"旅人"，在特定路点刷新，
有背景故事，可招募可拒绝。

**优点**：最有沉浸感，可以和剧情深度结合
**缺点**：要做数据表 + 刷新逻辑 + 对话树，是个大工程

---

## 六、和 playerbot 的差距（说实话）

你提到"就跟 playerbot 一样"，这里要说清楚区别：

| 能力 | playerbot | NPCBot | 差距原因 |
|---|---|---|---|
| bot 数量 | 几百个同时在线 | 受 conf 限制 | **架构不同** |
| bot 本质 | **真实的 Player 对象** | Creature 对象 | **这是根本差异** |
| 能不能进副本 | 能 | 能 | 平 |
| 能不能自己做任务 | 能 | 不能 | Player vs Creature |
| 装备/天赋系统 | 完整（就是玩家系统）| 简化版 | 同上 |
| 性能开销 | **大**（每个都是完整Player）| 小 | NPCBot 更轻 |

**核心差异**：playerbot 的 bot 是 `Player` 对象，NPCBot 的是 `Creature` 对象。

- playerbot 能做玩家能做的一切，但**吃性能**
- NPCBot 轻量，但**做不了玩家专属的事**（比如自己接任务）

**NPCBot 无法变成 playerbot**，这是架构决定的。
但"偶遇同行"这个需求，**NPCBot 完全能做**，因为它不需要 bot 自己接任务。

---

## 七、执行建议（分三步，先小后大）

### 第一步：验证 Eluna 能不能操作 bot（半天，关键）

**这一步决定后面所有方案的走向。**

```lua
-- 测试脚本：能不能拿到 bot 对象并让它说话
local function TestBotAccess(event, player)
    local creatures = player:GetCreaturesInRange(30)
    for _, c in pairs(creatures) do
        -- 能不能判断它是不是 bot？
        -- 能不能让它说话？
        c:SendUnitSay("测试", 0)
    end
end
```

- **能** -> 走方案 1（Eluna），后续开发极快
- **不能** -> 走方案 2（改 C++），慢但可控

### 第二步：最小可用版（1-2 轮）

```
[ ] 数量限制改成 4（改 conf，先确认 BRACKETS_COUNT）
[ ] GM 不受限（bot_ai.cpp:7770 加判断，并进 step33）
[ ] 偶遇触发：玩家靠近 -> bot 说一句话 -> 出现"同行"选项
[ ] 同行后：等级自动跟随（已有，验证即可）
```

### 第三步：身份体系（3-5 轮，做完前两步再评估）

```
[ ] characters_npcbot 加 identity 字段
[ ] 剧情bot（不占名额、不可解雇）
[ ] 同伴bot 好感度（可选）
[ ] 对接真龙剧情的七位凡人主角
```

---

## 八、我的判断

**你这个需求比想象中容易，因为：**

1. **"一起升级"已经是默认行为**（`mylevel = master->GetLevel()`）
2. **数量限制有现成配置**，改 conf 就行
3. **游荡bot已有独立成长系统**（`_killsCount`），作者还留了 TODO
4. 真正要做的只是**"偶遇"的交互体验**，这是脚本活不是引擎活

**唯一的不确定性是 Eluna 能不能操作 bot** —— 这决定是"写 Lua"还是"改 C++"。

**所以第一步必须是那个半天的验证。** 别的都先别动。

---

## 附：本文档实查的源码位置

```
bot_ai.cpp:2152    uint8 mylevel = std::min(master->GetLevel(), DEFAULT_MAX_LEVEL);
                   <- 普通bot自动跟随主人等级
bot_ai.cpp:2170    游荡bot按 _killsCount 计算等级
bot_ai.cpp:2170    //TODO: experience system for levelups   <- 作者留的坑
bot_ai.cpp:15914   ++_killsCount;
bot_ai.cpp:11636   杀怪数影响掉落金币
bot_ai.cpp:2187    me->SetLevel(mylevel);
botconfig.cpp:1172 GetMaxNpcBots(level) 按等级分档
botconfig.cpp:470  ASSERT(toks0.size() == BRACKETS_COUNT)  <- 配置填错会崩服
bot_ai.cpp:7770    reason = 2  数量超限的判断点（GM例外要加这）
bot_ai.h:195       bool IsWanderer() const { return _wanderer; }
bot_ai.cpp:20412   SetWanderer()  只置true不置false
```
