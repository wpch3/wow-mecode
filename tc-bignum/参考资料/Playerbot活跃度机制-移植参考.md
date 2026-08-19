# Playerbot 活跃度调度机制 —— NPCBot 扩容的移植参考

> 官方项目：https://github.com/mod-playerbots/mod-playerbots
> （AzerothCore Playerbots Module，900 star / 515 fork / 2692 commits，
>   最后更新 2026-07-31，仍在高频维护）
> 配置文件已存档：本文档摘录自 `conf/playerbots.conf.dist`（2366 行）
> 采集日期：2026-07-31

---

## 一、为什么要看这个

用户指出：AC playerbot 能稳跑 1000-5000 bot，而且**不是"附近没人就休眠"**
——加了好友的 bot 每天都在升级做任务，内存也没爆。

我一开始判断"NPCBot 上千会拖垮服务器"，**这个判断是错的**。
实测案例：

| 硬件 | bot 数 | 结果 |
|---|---|---|
| Beelink N100 迷你主机（4核16G，$150）| 1000 | "no issues at all" |
| AMD 5700x（6核20G）| **5000** | 作者日常配置 |
| Oracle 免费 ARM 实例（4核24G）| 1000-2000 | 只用 8GB |

**关键不在硬件，在于 playerbot 有一套活跃度调度，NPCBot 没有。**

---

## 二、核心机制：BotActiveAlone

### 基本思路

```
AiPlayerbot.BotActiveAlone = 10                 # 约 10% 的 bot 活跃
AiPlayerbot.BotActiveAloneDurationSeconds = 30  # 每 30 秒轮换一批
```

官方注释原文（重点）：

> - Think of it as a rough percentage: 10 means approximately 10% of bots will be active.
> - **The active bots rotate**: every `<DurationSeconds>` a different set of bots takes a turn.
> - The real number of active bots will always be higher than this value, because bots in
>   combat, dungeons, battlegrounds, LFG queue, groups with real players, etc.
>   **are always forced active on top of this**.
> - Set to 100 (with SmartScale off) = all bots always active. Maximum server load.

**这解释了用户观察到的现象**：
5000 只 bot 都"存在"，但同一时刻只有约 500 只在跑完整 AI，
且**每 30 秒轮换**——所以每只 bot 长期看都在推进（做任务、升级），
但瞬时负载只有 1/10。

### 强制活跃规则（override 百分比）

```
AiPlayerbot.BotActiveAloneForceWhenInRadius = 150   # 真人在 150 码内
AiPlayerbot.BotActiveAloneForceWhenInZone   = 1     # 真人在同一区域
AiPlayerbot.BotActiveAloneForceWhenInMap    = 0     # 真人在同一大陆
AiPlayerbot.BotActiveAloneForceWhenIsFriend = 0     # 真人好友列表里有它
AiPlayerbot.BotActiveAloneForceWhenInGuild  = 1     # 和真人同公会
```

**不可配置的强制活跃条件**（官方写死）：

> in combat, inside a dungeon/raid/BG, in a BG or LFG queue,
> grouped with a real player, or controlled by a real player.

这就是为什么用户"去刷怪做任务附近都会出现 bot 一起打" ——
`ForceWhenInRadius = 150` 保证玩家周围 150 码内的 bot **一定活跃**。

而"加好友那只每天升级" —— 如果开了 `ForceWhenIsFriend`，
或者它只是在轮换中被调度到，长期看都在推进。

### SmartScale 自适应降载

```
AiPlayerbot.botActiveAloneSmartScale = 1
AiPlayerbot.botActiveAloneSmartScaleDiffLimitfloor   = 50    # ms
AiPlayerbot.botActiveAloneSmartScaleDiffLimitCeiling = 200   # ms
AiPlayerbot.botActiveAloneSmartScaleWhenMinLevel = 1
AiPlayerbot.botActiveAloneSmartScaleWhenMaxLevel = 80
```

监控**服务器 tick 耗时**，动态调整活跃比例：

| 服务器 tick | 活跃比例（BotActiveAlone=10 时）|
|---|---|
| 50ms（流畅）| ~10%（不降）|
| 125ms | ~5%（减半）|
| 200ms（吃力）| 0%（只剩强制活跃的）|

**这是自我保护机制** —— 服务器扛不住时自动少跑 bot，而不是卡死。

---

## 三、对比 NPCBot：差在哪

| 能力 | playerbot | NPCBot 现状 | 依据 |
|---|---|---|---|
| 数量上限 | 配置项，无硬限 | **370 原型池** | 每生成一只 `_spareBotIdsPerClassMap.erase(orig_entry)`（botdatamgr.cpp:474）|
| 活跃度调度 | 10%~100% 可调 + 轮换 | **无，全 100% 活跃** | `Object.cpp:1008` bot 永远不能设为非活跃 |
| 格子加载 | 按活跃状态 | **每只强制 LoadGrid** | `botdatamgr.cpp SpawnWandererBot` |
| 自适应降载 | SmartScale 按 tick 调节 | 无 | — |
| AI 节流 | `RandomBotUpdateInterval` | 无 | — |

**NPCBot 的 `Object.cpp:1008`**：

```cpp
//npcbot: bots should never be removed from active
if (on == false && IsNPCBotOrPet())
    return;
```

这是 NPCBot 作者**故意加的保护** —— 防止 bot 被卸载。
但副作用就是"没有休眠档"，1000 只 = 1000 只全速跑。

---

## 四、移植方案（两步）

### 第一步：扩原型池（纯 SQL，零风险）

往 `creature_template` + `creature_template_npcbot_extras` 加更多原型记录。
现有 370 条（entry 70000-70595），可扩到 1000+。

**这步没有技术障碍**，做完立刻能把 Count 拉高。

同时能测出 **NPCBot 在"无调度"下的真实上限** —— 这是做第二步的依据。

### 第二步：移植活跃度调度（核心）

需要做的：

1. **放开 `Object.cpp:1008` 的 bot 保护**
   改成「允许闲逛 bot 设为非活跃，但雇佣中/副本中的不能」

2. **做一个轮换调度器**
   - 维护活跃名单，每 N 秒轮换
   - 按 `BotActiveAlone` 百分比决定名单大小

3. **实现强制活跃规则**
   - 玩家半径内 / 同区域 / 同公会 / 战斗中 / 副本中 → 强制活跃
   - 这部分逻辑 NPCBot 已有部分基础（它有 `IsWanderer()` 判断）

4. **SmartScale 自适应**
   - 读 `sWorld->GetUpdateTime()` 或类似接口
   - 按 floor/ceiling 线性降载

### 配套的 worldserver.conf 优化（官方推荐）

```
PreloadAllNonInstancedMapGrids = 0
SetAllCreaturesWithWaypointMovementActive = 0
DontCacheRandomMovementPaths = 0
MapUpdate.Threads = 4        # 约 CPU 核数 - 2
MapUpdateInterval = 10
MinWorldUpdateTime = 1
PlayerLimit = 0
Quests.IgnoreAutoAccept = 1
LeaveGroupOnLogout.Enabled = 1
```

官方 wiki 还提到：**内存会持续增长，建议定期重启**（cronjob）。

---

## 五、playerbot 项目的设计哲学（值得抄）

从 PR 模板里摘的，这段对我们做 bot 增强很有参考价值：

> **DESIGN PHILOSOPHY:** We prioritize **STABILITY, PERFORMANCE, AND PREDICTABILITY**
> over behavioral realism.
>
> Every action and decision executes **PER BOT AND PER TRIGGER**.
> Small increases in logic complexity **scale poorly across thousands of bots**
> and negatively affect all. We prioritize a stable system over a smarter one.
>
> **Bots don't need to behave perfectly; believable behavior is the goal,
> not human simulation.**
>
> Default behavior must be cheap in processing; **expensive behavior must be opt-in.**

翻译成人话：

- **可信 > 真实**。不追求完美模拟真人，只要看起来像就行
- **每个决策都要乘以 bot 数量**。加一点逻辑复杂度，×5000 就是灾难
- **默认要便宜，贵的功能做成开关**

这和用户「让他们绝对的真人，别人甚至看不出来」的诉求有张力 ——
真实感和性能是**此消彼长**的。做 NPCBot 增强时要在具体功能上取舍：

- 便宜且效果好的：名字/外观/公会/好友（静态数据，零 tick 成本）
- 贵的：智能对话、复杂决策（应该做成 opt-in，或只对玩家附近的 bot 开）

他们的 PR 模板甚至要求提交者回答：

> - Describe the **minimum logic** required to achieve the intended behavior.
> - Describe the **processing cost** when this logic executes across many bots.
> - Does this change increase per-bot/per-tick processing or **risk scaling poorly
>   with thousands of bots**?

---

## 六、其他可参考的官方数据

- **总量 vs 在线比**：`MaxRandomBots` 是**在线数**，
  总 bot 数默认是它的 2 倍（`EnablePeriodicOnlineOffline` 相关）
  —— 即 500 在线 = 1000 个角色轮换上下线
- **账号数**：`RandomBotAccountCount = 0` 表示自动算
- 官方默认值是 **500/500**，不是 1000

---

## 七、下一步

用户决定：**先做主线（.scene 场景快照），bot 增强稍后。**
用户表示可以提供 playerbot 项目源码用于深度整合。

做 bot 增强时，本文档就是移植的技术依据。
