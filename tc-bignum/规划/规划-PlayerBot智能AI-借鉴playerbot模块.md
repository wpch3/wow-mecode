# PlayerBot 智能AI —— 借鉴 mod-playerbots 的实证调研

> 2026-08-05
> 用户提醒：「playerbot模块有对应的方案和文件可以借鉴啊」
>
> **你说得对，我之前的判断是错的。这份文档是实查 mod-playerbots 源码后的结论。**

---

## 零、先纠正我之前的错误判断

我在 `规划-PlayerBot生活化四件套.md` 里写过：

> **Level 2（后做，好看）：真实寻路移动**
> 缺点：**PlayerBot 是 Player 不是 Creature**，Player 的移动要靠客户端发包驱动。
> 服务端主动推 Player 走路需要伪造 `MSG_MOVE_*` 包，**这是个大工程**
> **风险点**：Player 的 `MotionMaster` 在 3.3.5 里对无客户端的会话行为未验证

**这段话是错的。** 我没查就下了结论。

### 实证 1：mod-playerbots 对 Player 直接用 MotionMaster

`src/Ai/Base/Actions/MovementActions.cpp:1273`：

```cpp
    if (bot->GetMotionMaster()->GetCurrentMovementGeneratorType() != FOLLOW_MOTION_TYPE)
        bot->GetMotionMaster()->Clear();

    bot->GetMotionMaster()->MoveFollow(target, distance, angle);
    return true;
```

**`bot` 在 mod-playerbots 里就是 `Player*`。** 它直接调 `MoveFollow`，
没有任何伪造包的操作。

### 实证 2：Player 的 MotionMaster 确实被服务端驱动

我在**我们自己的仓库**里查到了驱动链：

```cpp
// Entities/Unit/Unit.cpp:494-495   Unit::Update() 内
    UpdateSplineMovement(p_time);
    i_motionMaster->Update(p_time);        // <-- 【服务端每tick驱动】
```

```cpp
// Entities/Player/Player.cpp:970    Player::Update() 内
    Unit::Update(p_time);                  // <-- Player 调用了它
```

**结论**：`MotionMaster` 是**纯服务端**的移动状态机，
每个 tick 由 `Unit::Update` 驱动，**和有没有客户端无关**。

客户端只是**接收**移动广播包（`SMSG_MONSTER_MOVE` / spline），
**不参与驱动**。所以 PlayerBot 完全可以用。

> **这也解释了为什么 NPCBot 能走路** —— 它用的是同一套 MotionMaster。
> 我早该想到这一点。

### 我错在哪

我把两件事搞混了：

| | 传送（TeleportTo） | 移动（MotionMaster） |
|---|---|---|
| 需要客户端 ACK 吗 | **需要**（step40 踩过） | **不需要** |
| 驱动方 | 客户端回包 | 服务端 Unit::Update |

**传送要 ACK 是真的**（`WorldSession.h:829 HandleMoveWorldportAck` 就是为此存在），
**但我错误地把这个结论推广到了所有移动上。**

---

## 一、mod-playerbots 的架构（值得借鉴的部分）

### 1.1 目录结构

```
src/
  Ai/
    Base/
      Strategy/      策略：决定"现在该干什么"
      Actions/       动作：具体怎么做
      Value/         值：缓存计算结果
      Triggers/      触发器：什么条件下激活策略
    Class/           各职业专属AI
    Dungeon/ Raid/   副本/团本专属
  Bot/
    Engine/          决策引擎
    Factory/         装备/天赋生成
  Mgr/
    Move/            移动管理
    Travel/          长距离旅行
```

### 1.2 核心设计：Strategy - Trigger - Action 三层

```
Trigger（触发器）  "血量低于30%"
      |
      v
Strategy（策略）   "生存策略" -> 决定用哪个 Action
      |
      v
Action（动作）     "喝药水" / "逃跑" / "求救"
```

**优点**：每个职业只要写自己的 Strategy 和 Action，框架复用。

**对我们的意义**：这是个**成熟的框架**，但**整套搬过来工作量巨大**
（mod-playerbots 有 1000+ 个文件）。

### 1.3 关键差异：它是 AzerothCore，我们是 TrinityCore

| | mod-playerbots | 我们 |
|---|---|---|
| 核心 | AzerothCore | TrinityCore 3.3.5 |
| `MoveFollow` 签名 | `(Unit*, float dist, float angle)` | `(Unit*, float dist, ChaseAngle angle, MovementSlot)` |
| 模块系统 | 有（可插拔模块） | 无（要直接改核心/脚本） |

**实查 TC 的签名**（`Movement/MotionMaster.h:156`，public 102段）：

```cpp
void MoveFollow(Unit* target, float dist, ChaseAngle angle, MovementSlot slot = MOTION_SLOT_ACTIVE);
//                                        ^^^^^^^^^^ 【注意】不是 float
```

**所以不能直接抄代码，但可以抄思路。**

---

## 二、我的建议：分三步走，不整套搬

### 第1步：跟随（最小可用，1个文件）

**这一步现在就能做，而且很简单。**

```cpp
// 核心就这三行
if (bot->GetMotionMaster()->GetCurrentMovementGeneratorType() != FOLLOW_MOTION_TYPE)
    bot->GetMotionMaster()->Clear();
bot->GetMotionMaster()->MoveFollow(master, 3.0f, ChaseAngle(float(M_PI)));
```

**配套要做的**（从 mod-playerbots 学来的经验）：

| 要点 | 为什么 | 出处 |
|---|---|---|
| 移动前检查 `IsRooted()` / `IsPolymorphed()` | 被控时别下移动指令 | `MovementActions.cpp:937` |
| 站起来 `SetStandState(UNIT_STAND_STATE_STAND)` | 坐着不能走 | `MovementActions.cpp:1297` |
| 处理游泳/飞行标志 | 不然会在水里"走路" | `MovementActions.cpp:959-979` |
| 距离太远直接传送 | 卡地形时的兜底 | 我们自己加 |

**指令**：`.pbot follow <名字|all>` / `.pbot stay`

### 第2步：战斗AI（中等，2-3个文件）

**不做完整的职业AI，先做通用的**：

```
选目标  ->  跟着主人的目标打
接近    ->  近战贴脸(5码) / 远程保持(25码)
攻击    ->  Attack(target) + 自动平A
技能    ->  从 spell 表里挑能用的，按冷却轮着放
```

**这一步的关键实现**（TC API，待实查确认访问段）：

```
Unit::Attack(Unit* victim, bool meleeAttack)
Unit::CastSpell(Unit* target, uint32 spellId, ...)
Player::HasSpell(uint32 spellId)
```

**先做"能打"，再做"打得好"。**

### 第3步：职业专属（大工程，长期）

每个职业一个文件，写技能优先级。**这一步可以慢慢来，一次一个职业。**

---

## 三、为什么不整套移植 mod-playerbots

| 理由 | 说明 |
|---|---|
| **核心不同** | AC 和 TC 的 API 差异遍布每个文件，改不完 |
| **它依赖 AC 的模块系统** | 我们没有，要重写所有挂钩点 |
| **1000+ 文件** | 移植工作量以月计，且很难调试 |
| **我们已有 NPCBot** | NPCBot 的战斗AI已经很成熟，PlayerBot 可以借鉴它 |

**更好的路**：**借鉴 NPCBot 的战斗AI**（同一个仓库，同一套 API）。

```
bot_ai.cpp 里已经有完整的：
  目标选择、技能循环、走位、团队配合
```

**PlayerBot 可以复用这些逻辑**，只是把 `Creature*` 换成 `Player*`。

---

## 四、执行计划（等你确认优先级）

```
现在 --> 修完手头的bug（step59/60）
          |
          v
       羁绊系统第2步（你说要先做的）
          |
          v
       【新】PlayerBot 跟随 —— 用 MotionMaster，1个文件
          |
          v
       【新】PlayerBot 自动上线
          |
          v
       【新】PlayerBot 基础战斗AI
          |
          v
       对话系统（羁绊第3步）
```

---

## 五、待实查清单（做的时候要查）

| # | 查什么 | 为什么 |
|---|---|---|
| 1 | `ChaseAngle` 构造函数 | TC 的 MoveFollow 第3参数类型 |
| 2 | `FOLLOW_MOTION_TYPE` 枚举值 | 判断当前是不是在跟随 |
| 3 | `Unit::Attack` 访问段 | 战斗AI |
| 4 | `Player::GetSpellMap()` 访问段 | 拿bot会的技能 |
| 5 | `Unit::SetStandState` 访问段 | 移动前站起来 |
| 6 | NPCBot 的 `bot_ai::Attack` 逻辑 | 借鉴目标选择 |

---

## 六、给你的一句话总结

**你提醒得对，我之前说"Player移动是大工程"是错的。**

实证：`Unit.cpp:495 i_motionMaster->Update(p_time)` 每tick驱动，
Player 继承 Unit，**MotionMaster 对 PlayerBot 直接可用**。

跟随功能**不需要伪造任何包**，三行代码就能跑起来。
我之前把"传送要ACK"错误地推广成了"所有移动都要客户端"。

已记入坑表。
