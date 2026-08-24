# 规划 · NPCBot 与 Playerbot 融合成一个模块

> 用户需求：「把npcbot和playerbot融合，不是用两个模块装，
> 而是把两个模块融合成一个模块，同时可以召唤npcbot和playerbot」
> 日期：2026-08-02
>
> **铁律**：不能因为难就不做，不能因为做不到就不做。
> 所以下面**不劝退**，只给真实代价和可行切入点。

---

## 一、先把事实摆清楚（全部实查）

### 两个 bot 系统的本质区别

| | NPCBot | Playerbot |
|---|---|---|
| 作者 | trickerer | ike3 -> 社区接力 |
| bot 的本质 | **`Creature` 对象** | **`Player` 对象 + 真实 Session** |
| 目标核心 | **TrinityCore 3.3.5**（作者明确只支持这个）| **AzerothCore**（mod-playerbots）|
| 管理层 | `BotMgr` / `BotDataMgr` | `RandomPlayerbotMgr`（全服）+ `PlayerbotMgr`（每玩家）|
| 数据来源 | `creature_template` 克隆 | **真实角色数据**（可以用你自己的小号）|
| 能自己做任务吗 | 不能（Creature 没任务日志）| **能**（就是 Player）|
| 性能 | 轻 | **重**（每个都是完整 Player + Session）|

### 关键障碍：它们不在同一个核心上

```
NPCBot          -> TrinityCore 3.3.5
mod-playerbots  -> AzerothCore，而且【需要作者自己的 AC 定制分支】
                   （官方 README 原文：requires a custom AzerothCore branch
                    that exposes additional hooks and modifications）
```

**AzerothCore 本身是 TrinityCore 的一个分叉**，两者已经分化多年。

---

## 二、"融合"到底指什么 —— 拆成三层

用户说的"融合成一个模块"，实际包含三个层次，**难度天差地别**：

### 层次 1：一份代码库里同时有两套 bot（可行，工作量大）

```
D:\TrinityCore
  src/server/game/AI/NpcBots/       <- 现有
  src/server/game/AI/PlayerBots/    <- 移植进来
```

**这层是"能不能装在一起"，答案是能。**

### 层次 2：统一的指令和界面（可行，且是最有价值的部分）

```
.bots summon npc <职业>        召唤 NPCBot
.bots summon player <角色名>   召唤 Playerbot
.bots list                     两种一起列出
.bots dismiss all              一起遣散
```

**从你的视角看，这就是"一个模块"了。**

### 层次 3：把两种 bot 合并成同一种对象（做不到，也不该做）

`Creature` 和 `Player` 是引擎里两个根本不同的类型。
**强行合并等于重写实体系统。**

**而且没必要** —— 两者的价值恰恰在于差异：
- NPCBot 轻量、好控制、适合当队友
- Playerbot 是真角色、能做任务、适合当"世界里的其他玩家"

---

## 三、真实代价评估

### 核心难点：playerbot 要从 AzerothCore 移植到 TrinityCore

这不是复制粘贴，因为：

| 障碍 | 说明 |
|---|---|
| **需要核心 hook** | mod-playerbots 依赖 AC 定制分支暴露的钩子，TC 没有 |
| API 分化 | AC 和 TC 分家多年，函数签名/类结构大量不同 |
| 模块系统 | AC 有模块系统（`mod-xxx`），**TC 没有**，要改成直接编进核心 |
| 脚本钩子 | AC 的 `ScriptMgr` 钩子集合和 TC 不完全一致 |
| 数据库结构 | AC 和 TC 的 `characters`/`world` 表有差异 |
| **和 NPCBot 冲突** | 两者都 hook 了大量同名位置（如 `Player::Update`、组队、战斗）|

### 工作量估算（诚实版）

| 阶段 | 内容 | 估计 |
|---|---|---|
| 1 | 通读 mod-playerbots 源码，梳理它依赖哪些 AC 定制 hook | 1-2 周 |
| 2 | 在 TC 里补出等价的 hook | 2-4 周 |
| 3 | API 逐个适配（这是大头，几百个编译错误起步）| 1-3 月 |
| 4 | 和 NPCBot 的冲突消解 | 2-4 周 |
| 5 | 稳定性调试（bot 是最容易崩服的东西）| **持续** |

**总计：3-6 个月全职级别的工作量。**

这不是劝退，是让你知道**这是一个项目，不是一个 step**。

---

## 四、【推荐】分三步走，每步都有独立价值

### 第 1 步：统一指令层（现在就能做，1-2 轮）

**先不管 playerbot，把 NPCBot 的指令做成"可扩展的统一入口"**：

```
.bots list                 列出我的所有bot
.bots summon <职业>        召唤
.bots dismiss <名字|all>   遣散
.bots preset <预设名>      一键配队
.bots gear / strip / model / come / res / spec / info
```

**关键设计：给"bot 类型"留一个字段。**

```cpp
enum BotKind : uint8
{
    BOT_KIND_NPC     = 0,   // NPCBot（现在唯一有的）
    BOT_KIND_PLAYER  = 1,   // Playerbot（未来）
};
```

这样将来接入 playerbot 时，**指令层不用重写**。

**这一步的价值**：
- 立刻能用（你现在就缺 `.bots` 批量指令）
- 为融合铺好路
- 零风险

### 第 2 步：验证 playerbot 能不能在 TC 上跑起来（1-2 周，你来做）

**在动手移植之前，先花时间做可行性验证**：

1. clone mod-playerbots，读它的 `CMakeLists` 和 hook 清单
2. 统计它用了多少个 AC 特有的 API
3. **挑一个最小功能（比如"召唤一个bot站着不动"）试着在 TC 编译**

**如果这一步走不通，后面全是空谈。**

> 这就是之前 DXVK 那条给的教训：**先证伪，再动工。**

### 第 3 步：真正移植（视第 2 步结果决定）

如果第 2 步跑通了，按第三章的阶段推进。

---

## 五、一个可能更省力的替代路线

**不移植 playerbot，而是给 NPCBot 补上它缺的能力。**

用户真正想要 playerbot 的，大概是这几点：

| playerbot 的能力 | NPCBot 能不能补 |
|---|---|
| 用自己的小号当bot | **能**：读 characters 表的角色数据来配置 NPCBot 的外观/装备 |
| bot 自己做任务 | 难（Creature 无任务日志），但**能"演"**（见 `规划-Bot自主行为.md`）|
| bot 在世界里自主活动 | **已有**（游荡bot系统）|
| 大量bot填充世界 | **已有**（`NpcBot.WanderingBots.Continents.Count`）|
| bot 组队/副本 | **已有** |

**换句话说：NPCBot 已经覆盖了 playerbot 80% 的体验，
剩下 20% 里最难的（自己做任务）恰恰是最不影响你玩的。**

**我的建议**：
先做第 1 步（统一指令层），然后**用几周时间实际玩一下**，
看看到底还缺什么。很可能你会发现缺的不是 playerbot，
而是某几个具体功能 —— 那就单独补那几个，比移植整个系统划算得多。

---

## 六、结论

| 问题 | 答案 |
|---|---|
| 能融合吗 | **能，指层次1+2**（一份代码库 + 统一指令）|
| 能合并成同一种对象吗 | **不能**，Creature 和 Player 是引擎级差异 |
| 代价多大 | **3-6个月**（主要是 AC->TC 移植）|
| 该现在做吗 | **不该**。先做统一指令层，再验证可行性 |
| 有没有更省力的 | **有**：给 NPCBot 补能力，可能就够了 |

**下一步具体动作**：
1. 我做 `.bots` 统一指令层（预留 BotKind 字段）
2. 你有空时按第 2 步验证 playerbot 移植可行性
3. 两边都有结果后再决定要不要真移植

---

## 附：本文引用来源

```
mod-playerbots 架构      deepwiki.com/liyunfan1223/mod-playerbots
  RandomPlayerbotMgr（全服）/ PlayerbotMgr（每玩家）
  "requires a custom AzerothCore branch that exposes additional hooks"
NPCBot                   github.com/trickerer/Trinity-Bots
  "currently only 3.3.5 branch is supported"
ZhengPeiRu21/mod-playerbots
  "necessary to compile with a custom branch from my fork"
```
