# step29 `.emote` —— 开工前 API 核实记录

> 核实日期：2026-08-01
> 源码：`328950225/TrinityCore-NPCBOT-Eluna-zhCN` 分支 `NPCBOT-Eluna-zhCN-2026`
> 原则：**全部实查，不凭记忆**

---

## 一、【最关键】两个 API 的本质区别

这一条决定整个指令的设计，必须先说清楚。

### `HandleEmoteCommand` —— 一次性动作

```
Unit.h:1037          void HandleEmoteCommand(Emote emoteId);
Unit.cpp:1712        实现
```

实现只有 5 行，本质是**发一个网络包**：

```cpp
void Unit::HandleEmoteCommand(Emote emoteId)
{
    WorldPackets::Chat::Emote packet;
    packet.Guid = GetGUID();
    packet.EmoteID = emoteId;
    SendMessageToSet(packet.Write(), true);
}
```

**特点**：
- 播完就没了，**不改任何字段**
- 服务器重启、玩家重新进视野 -> **动作消失**
- 适合 `EMOTE_ONESHOT_*`（挥手、鞠躬、大笑）

### `SetEmoteState` —— 持续状态

```
Unit.h:967   Emote GetEmoteState() const { return Emote(GetUInt32Value(UNIT_NPC_EMOTESTATE)); }
Unit.h:968   void SetEmoteState(Emote emote) { SetUInt32Value(UNIT_NPC_EMOTESTATE, emote); }
```

**特点**：
- 写进 `UNIT_NPC_EMOTESTATE` 字段，**会一直保持**
- 新玩家进视野也能看到
- **`.scene` 已经在存这个字段**（step28 存了 emote）
- 适合 `EMOTE_STATE_*`（跳舞、睡觉、坐着、工作）

### 对比表

| | HandleEmoteCommand | SetEmoteState |
|---|---|---|
| 持续性 | **一次性** | **永久** |
| 改字段 | 否 | `UNIT_NPC_EMOTESTATE` |
| 新玩家可见 | 否 | **是** |
| 适合 | ONESHOT 类 | STATE 类 |
| `.scene` 能存 | 否 | **能**（已实现）|

**结论：`.emote` 必须同时支持两种，否则一半场景做不了。**

---

## 二、Emote 枚举

```
SharedDefines.h:1998    enum Emote : uint32
SharedDefines.h:2175    };            <- 结束行
```

**总计 174 个**：
- `EMOTE_ONESHOT_*` **101 个**
- `EMOTE_STATE_*` **73 个**

### 剧情常用 STATE（实查节选）

| 值 | 名称 | 用途 |
|---|---|---|
| 10 | `EMOTE_STATE_DANCE` | 跳舞 |
| 12 | `EMOTE_STATE_SLEEP` | 睡觉 |
| 13 | `EMOTE_STATE_SIT` | 坐着 |
| 26 | `EMOTE_STATE_STAND` | 站立（复位）|
| 30 | `EMOTE_STATE_NONE` | 无（清除）|
| 64 | `EMOTE_STATE_STUN` | 昏迷 |
| 65 | `EMOTE_STATE_DEAD` | 假死 |
| 68 | `EMOTE_STATE_KNEEL` | 跪 |
| 173 | `EMOTE_STATE_WORK` | 工作 |
| 233 | `EMOTE_STATE_WORK_MINING` | 挖矿 |
| 234 | `EMOTE_STATE_WORK_CHOPWOOD` | 砍柴 |
| 353 | `EMOTE_STATE_SPELL_KNEEL_START` | 施法跪姿 |
| 378 | `EMOTE_STATE_TALK` | 持续说话 |
| 379 | `EMOTE_STATE_FISHING` | 钓鱼 |
| 383 | `EMOTE_STATE_DROWNED` | 溺水 |

### 剧情常用 ONESHOT（实查节选）

| 值 | 名称 |
|---|---|
| 1 | `EMOTE_ONESHOT_TALK` |
| 2 | `EMOTE_ONESHOT_BOW` |
| 3 | `EMOTE_ONESHOT_WAVE` |
| 4 | `EMOTE_ONESHOT_CHEER` |
| 5 | `EMOTE_ONESHOT_EXCLAMATION` |
| 6 | `EMOTE_ONESHOT_QUESTION` |
| 11 | `EMOTE_ONESHOT_LAUGH` |
| 15 | `EMOTE_ONESHOT_ROAR` |
| 16 | `EMOTE_ONESHOT_KNEEL` |
| 18 | `EMOTE_ONESHOT_CRY` |
| 21 | `EMOTE_ONESHOT_APPLAUD` |
| 22 | `EMOTE_ONESHOT_SHOUT` |
| 25 | `EMOTE_ONESHOT_POINT` |

---

## 三、官方已有实现（参考 + 不足）

### `.npc playemote`

```
cs_npc.cpp:660-673
```

```cpp
static bool HandleNpcPlayEmoteCommand(ChatHandler* handler, Emote emote)
{
    Creature* target = handler->getSelectedCreature();
    if (!target) { ...报错... }
    target->SetEmoteState(emote);
    return true;
}
```

**三个不足，正是我们要补的**：

| 不足 | 我们的改进 |
|---|---|
| 只能 `SetEmoteState`，**没有一次性播放** | 两种都支持 |
| **只能选中单个目标** | 加 `r <半径>` / `entry <ID>` 批量 |
| **必须填数字**，174 个记不住 | 中文别名 |

### 其他参考点

```
cs_modify.cpp:830    玩家自己：GetSession()->GetPlayer()->SetEmoteState(Emote(anim_id))
cs_debug.cpp:1105    unit->HandleEmoteCommand(emote)
cs_npc.cpp:915-917   官方用 ONESHOT 做说话动作（? ! 分别对应问号/感叹号）
```

**`cs_npc.cpp:915-917` 特别有价值** —— 官方 `.npc say` 会根据
标点自动播对应表情，这个思路 `.say` 可以直接借鉴。

---

## 四、【重要】指令注册语法必须用旧式

### 仓库现状：两种语法并存

```
ChatCommand.h:50    using ChatCommandTable = std::vector<ChatCommandBuilder>;   <- 新式
ChatCommand.h:281   using ChatCommand [[deprecated(...)]] = ...ChatCommandBuilder;  <- 旧式，仍可用
```

旧式虽然标了 `deprecated`，但**仓库里仍有多个官方文件在用**：
`cs_wp.cpp` / `cs_ticket.cpp` / `cs_server.cpp` / `cs_reset.cpp` / `cs_reload.cpp`

### 我们自己的既有代码用的是旧式

```
cs_npcstate.cpp:350   static std::vector<ChatCommand> commandTable =
cs_npcstate.cpp:354   { "nst", rbac::RBAC_PERM_COMMAND_WORLDTOOLS, false, &HandleNst, "" },
```

**`.emote` 必须沿用旧式**，理由：
1. 和 `.nst` / `.scene` / `.dummy` 保持一致
2. 旧式接 `char const* args`，**方便自己解析多参数**
   （新式是强类型自动解析，做不了 `r 30 跳舞 save` 这种混合参数）
3. 改语法风格会引入不必要的编译风险

### 权限沿用

```
rbac::RBAC_PERM_COMMAND_WORLDTOOLS
```
和 `.nst` / `.scene` 用同一个，不用动 RBAC 表。

---

## 五、其他已核实 API

```
Chat.h:104              Creature* getSelectedCreature();
Unit.h:967/968          GetEmoteState / SetEmoteState          [public]
Unit.h:1037             HandleEmoteCommand                     [public, 811行起的public段]
SharedDefines.h:1998    enum Emote : uint32
UpdateFields.h:140      UNIT_NPC_EMOTESTATE
Creature.h:394          bool IsNPCBotOrPet() const
```

**访问权限已逐个确认**：`HandleEmoteCommand` 位于 `Unit.h:811` 开始的
`public:` 段内（下一个 `protected:` 在更后面），可以直接调用。

---

## 六、`.emote` 设计方案

### 指令形态

```
.emote <表情>              对选中目标
.emote r <半径> <表情>      对周围所有 NPC
.emote entry <ID> <表情>    对指定 entry 的所有 NPC
.emote list [关键词]        查表情（解决 174 个记不住的问题）
.emote clear               清除状态表情（= STATE_NONE）
.emote save                写库持久化
```

### 表情参数三种写法

```
.emote 跳舞          中文别名
.emote dance         英文别名
.emote 10            原始数字
```

### 自动判断 ONESHOT / STATE

```
值在 EMOTE_STATE_* 列表里  -> SetEmoteState（持续）
其他                       -> HandleEmoteCommand（一次性）
强制指定：.emote once 跳舞 / .emote state 挥手
```

### 和 `.scene` 打通

`.scene` 已经在存 `emote` 字段（step28），
`.emote save` 写库后，`.scene save` 能一起存下来。

---

## 七、待确认（写代码时验证）

| 项 | 说明 |
|---|---|
| `creature_template.unit_flags` 附近有无 emote 持久化字段 | 决定 `save` 怎么写 |
| NPCBot 是否需要特殊处理 | `.nst` 里有 `IsNPCBotOrPet()` 分支 |
| 半径搜索用哪个 API | 参考 `.nst` 的实现 |
