# 规划 · 在 NPCBot 里加入"真实 Player 型 Bot"

> 用户需求：「npcbot也有，playerbot这种真实的bot也有，
> 就是说 Creature 和 Player 都同时做到 npcbot 里」
> 「我还要一个真实属性的bot，可以调用背包和接任务，以及做更多的事情」
>
> 日期：2026-08-02
> **铁律**：不能因为难就不做。下面全部是【实查源码后的结论】，不是猜测。

---

## 一、结论先行：**可行，而且比移植 playerbot 简单得多**

我原本以为这要从 AzerothCore 移植整套 mod-playerbots（3-6个月）。

**实查 TrinityCore 源码后，结论变了：**

> **TrinityCore 核心【本身就支持】没有网络连接的 Player 对象。**
> 不需要移植任何东西，用核心自带的机制就能造出真实 Player bot。

---

## 二、决定性证据（三条，全部实查）

### 证据 1：`WorldSession` 接受空 socket

```cpp
// WorldSession.h:495
WorldSession(uint32 id, std::string&& name, std::shared_ptr<WorldSocket> sock, ...);
                                            ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
// 是 shared_ptr，可以传 nullptr
```

### 证据 2：核心【明确设计了】"无连接会话"这个状态

```cpp
// WorldSession.h:503
bool PlayerDisconnected() const { return !m_Socket; }
```

**核心自己就有"这个会话没有 socket"的概念，并且到处在用。**

### 证据 3：无 socket 时发包被安全丢弃，不崩

```cpp
// WorldSession.cpp:211-216
void WorldSession::SendPacket(WorldPacket const* packet)
{
    ASSERT(packet->GetOpcode() != NULL_OPCODE);

    if (!m_Socket)
        return;              // <- 直接返回，不崩
    ...
}
```

**这是最关键的一条。** 意味着：
- 可以创建一个没有网络连接的 `WorldSession`
- 用它加载一个真实的 `Player` 对象
- 服务端所有发给"客户端"的包都会被静默丢弃
- **但 Player 对象本身完全正常** —— 有背包、有任务日志、有天赋、有技能

**这正是 playerbot 的核心原理。** ike3 当年就是这么做的。

---

## 三、为什么这比移植 mod-playerbots 简单

| | 移植 mod-playerbots | 自己造 Player bot |
|---|---|---|
| 来源 | AzerothCore（需要它的定制分支）| **TrinityCore 自带机制** |
| API 适配 | 几百个编译错误起步 | **不需要**，用的就是 TC 的 API |
| 模块系统 | AC 有，TC 没有，要改造 | 不涉及 |
| 和 NPCBot 冲突 | 两套 hook 打架 | **可控**，我们自己设计 |
| AI 逻辑 | 它自带一套庞大的 AI 树 | **要自己写**（这是主要工作量）|

**结论：难点从"移植适配"变成了"写AI"，而后者是可以渐进的。**

---

## 四、【重要】已发现的一个崩溃点

```cpp
// WorldSession.cpp:298
if (IsConnectionIdle() && !HasPermission(rbac::RBAC_PERM_IGNORE_IDLE_CONNECTION))
    m_Socket->CloseSocket();      // <- 【没有判空！】

// WorldSession.cpp:764
bool WorldSession::IsConnectionIdle() const
{
    return m_timeOutTime < GameTime::GetGameTime() && !m_inQueue;
}
```

**问题**：bot 的会话没有 socket，而且永远不会有"网络活动"来刷新
`m_timeOutTime`，所以 `IsConnectionIdle()` 迟早为真
-> `m_Socket->CloseSocket()` -> **空指针崩服**。

**两个解法（二选一）**：

**解法 1（推荐）**：给 bot 会话授予 `RBAC_PERM_IGNORE_IDLE_CONNECTION` 权限，
条件短路，永远不会走到那行。

**解法 2**：改核心加判空
```cpp
if (m_Socket && IsConnectionIdle() && !HasPermission(...))
    m_Socket->CloseSocket();
```

**解法1更干净，不动核心。**

> 这个坑如果不提前发现，会表现为"跑一段时间随机崩服"，极难排查。

---

## 五、实现路线（分四步，每步可独立验证）

### 第 1 步：造出一个能站着的 Player bot（1-2 轮）

```
1. 准备一个专用账号（如 "BOTACC1"）和角色
2. 创建 WorldSession(accountId, name, nullptr, SEC_PLAYER, ...)
3. 授予 RBAC_PERM_IGNORE_IDLE_CONNECTION（避开第四章的崩溃点）
4. sWorld->AddSession(session)
5. 走正常登录流程加载 Player（HandlePlayerLogin / Player::LoadFromDB）
6. 验证：世界里出现一个真实角色，/who 能看到，可以组队
```

**验收标准**：能看到它站在那，且服务端不崩。

**这一步做完，"真实属性的bot"就已经存在了** ——
它有背包、有任务日志、有天赋，因为它就是个真 Player。

### 第 2 步：接管控制（2-3 轮）

给它加一个 AI 更新循环：
```
跟随主人 / 攻击目标 / 施放技能
```

**可以复用 NPCBot 的战斗逻辑思路**，但要用 Player 的 API。

### 第 3 步：任务与背包（这是用户最想要的）

因为它是真 Player，**这些是白送的**：
```cpp
bot->CanTakeQuest(quest, false)       // 能接任务
bot->AddQuest(quest, questGiver)
bot->CompleteQuest(questId)
bot->StoreNewItem(...)                // 背包
bot->GetQuestStatus(questId)
```

**不需要"模拟"，直接调 Player 的方法。**

### 第 4 步：和 NPCBot 统一管理

```cpp
enum BotKind : uint8
{
    BOT_KIND_NPC    = 0,   // Creature 型，轻量
    BOT_KIND_PLAYER = 1,   // Player 型，全功能
};

.bots summon npc <职业>        召唤 NPCBot
.bots summon player <角色名>   召唤 Player bot
.bots list                     两种一起列
```

---

## 六、两种 bot 的定位（都要，各有用途）

| | NPCBot（Creature）| Player bot |
|---|---|---|
| 性能 | **轻**，几十个没问题 | **重**，每个是完整Player |
| 背包 | 模拟的（step34虚拟背包）| **真的** |
| 任务 | 做不到 | **能接能交** |
| 天赋技能 | 简化版 | **完整玩家系统** |
| 适合 | 日常队友、填充世界 | 少量精英同伴、剧情角色 |

**建议用法**：
- 日常带 3-4 个 NPCBot（省性能）
- 关键剧情/需要做任务时召唤 1-2 个 Player bot

**这正是用户要的"两种都有"。**

---

## 七、诚实的风险提示

| 风险 | 说明 |
|---|---|
| **崩服** | Player bot 走的是完整玩家代码路径，任何空 socket 假设不成立的地方都可能崩。第四章那个只是我发现的第一个 |
| 性能 | 每个 Player bot = 一个完整 Player + Session，10个就相当于10个真人在线 |
| 存档污染 | bot 角色会写 characters 库，要用独立账号隔离 |
| AI 工作量 | 让它"聪明地打架"是长期工程，第1步只能让它站着 |

**但这些都是工程问题，不是"做不到"。**

---

## 八、下一步

**建议先做第 1 步的最小验证**：

> 造出一个能站在世界里、不崩服的 Player bot。

这一步做完，后面全是加法。做不成，说明我漏了什么，及时止损。

**我可以现在就写第 1 步的代码。** 要开始吗？

---

## 附：本文实查的源码位置

```
WorldSession.h:495    构造函数收 std::shared_ptr<WorldSocket>（可为空）
WorldSession.h:503    bool PlayerDisconnected() const { return !m_Socket; }
WorldSession.cpp:211  SendPacket: if (!m_Socket) return;      <- 关键
WorldSession.cpp:298  m_Socket->CloseSocket();                <- 崩溃点
WorldSession.cpp:764  IsConnectionIdle()
World.cpp:334         void World::AddSession(WorldSession* s)
CharacterHandler.cpp:720  WorldSession::HandlePlayerLogin(LoginQueryHolder const&)
Player.cpp:17149      bool Player::LoadFromDB(ObjectGuid, CharacterDatabaseQueryHolder const&)
```
