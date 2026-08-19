# 规划 · Bot 自主行为（不当挂件）

> 制定：2026-08-02
> 用户想法：「能不能让队伍里的机器人不用一直跟着自己挂机，
> 让他也会做任务或者刷怪，或者想去做什么、邀请自己、
> 与自己分享事情和战利品；在自己挂机的时候不退队伍去做些任务。
> 不用同步等级，差异可以在 3-5 级之间智能抉择」

---

## 零、先给结论

| 你想要的 | 可行性 | 说明 |
|---|---|---|
| 队伍里的 bot 自己去刷怪 | **可行**，中等工作量 | 有硬约束要绕，见第二章 |
| bot 分享战利品 | **已有一半** | `BOT_ROLE_AUTOLOOT` 已存在 |
| bot 邀请你去做事 | **可行**，纯脚本 | 用 Whisper + Gossip |
| bot 等级差 3-5 级浮动 | **可行**，简单 | 改一行等级计算 |
| **bot 自己接任务交任务** | **做不到** | **架构墙**，见第五章 |

**核心结论**：**"自己刷怪 + 汇报 + 分赃"能做，"自己做任务"做不到。**
后者是 Creature 和 Player 的架构差异，不是工作量问题。

---

## 一、【好消息】战利品分享已经有了

实查 `botcommon.h:300-305`：

```cpp
BOT_ROLE_AUTOLOOT         = 0x00400,   //not in mask
BOT_ROLE_AUTOLOOT_POOR    = 0x00800,   // 灰色
BOT_ROLE_AUTOLOOT_COMMON  = 0x01000,   // 白色
BOT_ROLE_AUTOLOOT_UNCOMMON= 0x02000,   // 绿色
BOT_ROLE_AUTOLOOT_RARE    = 0x04000,   // 蓝色
BOT_ROLE_AUTOLOOT_EPIC    = 0x08000,   // 紫色
```

**按品质分级的自动拾取，已经内置了。** 而且会汇报：

```cpp
// bot_ai.cpp:18226
msg << LocalizedNpcText(master, BOT_TEXT_LOOTING) << ' ' << name;
BotWhisper(msg.view());          // <- 私聊告诉你它捡了什么
_autoLootCreature(un->ToCreature());
```

**所以"分享战利品"这条，只要在 Gossip 里打开对应角色就行，零开发。**
（`bot_ai.cpp:9951` 有 `GOSSIP_SENDER_ROLES_LOOTING` 菜单入口）

---

## 二、【硬约束】队伍里的 bot 为什么不能自己跑

这是本方案最关键的一处，实查得到：

```cpp
// bot_ai.cpp:4414  —— 游荡移动的唯一入口
if (IAmFree() && IsWanderer() && !me->IsInCombat() && ...)
{
    // 走向下一个路点、觅食、休息……
}
```

```cpp
// bot_ai.cpp:15379
bool bot_ai::IAmFree() const
{
    if (!_botData->owner)
        return true;       // 无主 -> 自由
    ...
    return false;          // 有主 -> 不自由
}
```

### 翻译成人话

**一旦你雇佣了它（有了 owner），整套自主行为立刻被关闭。**

游荡 bot 那套完整的路点网络（`botwanderful.h` 里定义了 20 多种路点标志，
包括阵营限制、跳跃点、BG 目标点）——**队伍里的 bot 一个都用不上**。

**这就是"bot 一直跟着你挂机"的根本原因。** 不是没做，是被 `IAmFree()` 挡住了。

### 三个绕法

| 方案 | 做法 | 风险 |
|---|---|---|
| **A. 放宽条件** | 把 `IAmFree() && IsWanderer()` 改成允许"有主但被授权自由活动" | **中**，要小心别影响正常跟随 |
| B. 临时解绑 | bot 去干活时临时清 owner，回来再绑 | **高**，owner 是持久化字段，中途崩服会丢 |
| C. 新写一套 | 给有主 bot 单独写一套简化的自主移动 | 低风险但**工作量大** |

**推荐 A**：加一个 `BOT_COMMAND_ROAM` 状态位，
条件改成 `(IAmFree() || HasBotCommandState(BOT_COMMAND_ROAM)) && ...`。

现有的指令状态位还有空（`botcommon.h:622-633` 用到 `0x00000800`，
上面还有大片没用的位）。

---

## 三、等级浮动 3-5 级（简单，但要想清楚）

### 现状

```cpp
// bot_ai.cpp:2152
uint8 mylevel = std::min<uint8>(master->GetLevel(), DEFAULT_MAX_LEVEL);
```

**严格等于主人等级。**

### 改成浮动

```cpp
// 伪代码
int8 offset = _levelOffset;              // 每个bot出生时随机 -5..+3
uint8 mylevel = std::clamp<int>(master->GetLevel() + offset, 1, DEFAULT_MAX_LEVEL);
```

### 但要注意三个副作用（想清楚再做）

**副作用 1：低于玩家太多会拖后腿**
- 5 级差在低等级段（1-20）影响巨大，在 70-80 段几乎无感
- **建议按等级段缩放**：低等级 ±1，高等级 ±5

**副作用 2：高于玩家会抢经验/影响组队经验分配**
- 队伍最高等级影响怪物经验计算
- **建议只允许负偏移**（bot 略低于玩家），或正偏移最多 +1~2

**副作用 3：装备等级不匹配**
- bot 的装备是按等级给的，等级低了装备也差
- 差 5 级可能导致 bot 在高难度副本里瞬间倒地

**我的建议**：
```
偏移范围     -3 ~ +1
按段缩放     1-30级用 -1~0，30-60用 -2~+1，60+用 -3~+1
可配置       NpcBot.LevelOffset.Enable / Range
```

---

## 四、"邀请你去做事" —— 这部分最有戏

这是纯脚本活，**不动引擎**，而且最能出效果。

### 能做的交互

| 场景 | 触发 | 实现 |
|---|---|---|
| bot 发现稀有怪 | 附近有 rare 标记的生物 | `BotWhisper("前面有只稀有怪，去看看？")` |
| bot 想去某地 | 随机 + 冷却 | Whisper + 地图标记 |
| bot 捡到好东西 | 拾取到蓝/紫装 | **已有**，扩展文案即可 |
| bot 报告战况 | 击杀数达标 | `"我们已经清了 20 只了"` |
| bot 提议休息 | 血蓝低 | `"要不要坐下吃点东西？"` |
| bot 闲聊 | 长时间无战斗 | 接你的 `.say` 工具链 |

### 技术底子已经有

```cpp
bot_ai.cpp:18226   BotWhisper(msg.view());          // 私聊
bot_ai.cpp:15914   ++_killsCount;                   // 击杀计数
bot_ai.cpp:9951    AddGossipItemFor(...)            // 对话菜单
```

**`BotWhisper` 现成的，`_killsCount` 现成的，Gossip 现成的。**
剩下的就是写触发条件和文案。

### 和剧情工具链的联动

你已经有 `.say` / `.emote`（step29/30），
**bot 的对白可以复用同一套表情+语气系统**。

---

## 五、【做不到】bot 自己接任务交任务

必须说清楚这条，避免期待落空。

### 为什么做不到

```
任务系统（QuestMgr）的所有接口都要求 Player*
  - CanTakeQuest(Player*)
  - AddQuest(Quest const*, Object* questGiver)
  - CompleteQuest / RewardQuest
  - 任务日志存在 Player 的 m_QuestStatus

NPCBot 是 Creature，不是 Player
```

**这不是"没实现"，是类型不匹配。** 要做的话等于：
- 给 Creature 加一套任务日志系统
- 复制整个 QuestMgr 的 bot 版本
- 处理任务物品、任务怪计数、任务链……

**这就是 playerbot 存在的意义** —— 它的 bot 就是 Player 对象，
所以天生能做任务，代价是每个 bot 都吃完整 Player 的内存和 CPU。

### 能做的"伪装版"

| 想要的效果 | 伪装做法 |
|---|---|
| bot 好像在做任务 | 让它跑到任务点、打任务怪、演个动作 |
| bot 交任务 | Whisper「我把货送到了」+ 给点金币 |
| bot 有任务目标 | 自己维护一张"bot 任务表"，纯自定义 |

**本质是"演出来"，不是真的走任务系统。**
对单机/小服体验来说，**演得像就够了**。

---

## 六、"挂机时 bot 不退队去做事" —— 怎么设计

这是你描述里最具体的一个场景，单独说。

### 设计

```
玩家 30 秒无操作（或手动开启"自由活动"）
   -> bot 进入 ROAM 状态（第二章方案A）
   -> 在玩家周围 N 码范围内自主刷怪
   -> 玩家一动 / 进战斗 / 手动召回
   -> bot 立刻回归跟随
```

### 关键参数

| 参数 | 建议值 | 理由 |
|---|---|---|
| 触发延迟 | 30 秒 | 太短会误触发 |
| 活动半径 | 100-150 码 | 太远召不回来 |
| 自动回归距离 | 玩家移动 > 40 码 | 别让 bot 掉队 |
| 战斗中 | 立即回归 | 玩家打架 bot 必须在 |

### 实现要点

```cpp
// 伪代码
void bot_ai::UpdateRoamState(uint32 diff)
{
    if (!HasBotCommandState(BOT_COMMAND_ROAM))
        return;

    if (master->IsInCombat() || me->IsInCombat())
    {
        RemoveBotCommandState(BOT_COMMAND_ROAM);   // 打架了，回来
        return;
    }

    if (me->GetDistance(master) > MAX_ROAM_DIST)
    {
        // 太远了，往回走
        return;
    }

    // 找附近的怪打
}
```

**这块可以完全独立开发，不影响现有逻辑。**

---

## 七、执行建议（按性价比排）

### 第 1 档：零开发，立刻能用

```
[ ] 打开 BOT_ROLE_AUTOLOOT 系列（Gossip 里配）
    -> 战利品自动拾取 + 私聊汇报，已内置
```

**先试这个** —— 可能你想要的"分享战利品"已经满足了。

### 第 2 档：小改动，效果明显

```
[ ] 等级浮动（改 SetStats 一处 + 加 conf）
[ ] bot 主动搭话（纯 Eluna 或小段 C++，接 BotWhisper）
```

### 第 3 档：中等工程，最核心

```
[ ] BOT_COMMAND_ROAM 状态位
[ ] 挂机时自主刷怪（第六章）
[ ] 放宽 bot_ai.cpp:4414 的 IAmFree() 约束
```

### 第 4 档：大工程，做完前三档再评估

```
[ ] 完整的 bot 自主目标系统（想去哪、想做什么）
[ ] 伪任务系统
```

---

## 八、和前面几个规划的关系

```
规划-Bot身份体系与偶遇同行.md    <- 身份（是谁）
规划-Bot自主行为.md（本文档）     <- 行为（做什么）
规划-NPCBot增强总蓝图.md         <- 批次E「拟真社交」和本文档重叠
```

**建议合并执行**：身份体系定了之后，
不同身份可以有不同的自主行为策略：

| 身份 | 自主行为 |
|---|---|
| 普通雇佣 bot | 挂机时刷怪（第六章）|
| 偶遇同伴 | 会主动提议去哪 |
| 剧情 bot | 按剧情脚本行动 |
| 玩家 bot | 完全自主（就是现在的游荡 bot）|

---

## 附：本文档实查的源码位置

```
bot_ai.cpp:4414     if (IAmFree() && IsWanderer() && ...)   <- 自主移动唯一入口
bot_ai.cpp:15379    bool bot_ai::IAmFree()  有 owner 就返回 false
bot_ai.cpp:2152     uint8 mylevel = std::min(master->GetLevel(), DEFAULT_MAX_LEVEL);
bot_ai.cpp:18226    BotWhisper(msg.view());   拾取汇报
bot_ai.cpp:18229    _autoLootCreature(un->ToCreature());
bot_ai.cpp:15914    ++_killsCount;
bot_ai.cpp:9951     GOSSIP_SENDER_ROLES_LOOTING  拾取角色菜单
botcommon.h:300-305 BOT_ROLE_AUTOLOOT 系列（按品质分级）
botcommon.h:622-637 BOT_COMMAND_* 状态位（还有空位可加 ROAM）
botwanderful.h:21+  BotWPFlags 路点系统（20+种标志，队伍bot用不上）
```
