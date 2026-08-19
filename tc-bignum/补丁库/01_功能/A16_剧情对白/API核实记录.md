# step30 `.say` —— 开工前 API 核实记录

> 核实日期：2026-08-01
> 源码：`328950225/TrinityCore-NPCBOT-Eluna-zhCN` 分支 `NPCBOT-Eluna-zhCN-2026`
> 原则：全部实查，不凭记忆

---

## 一、四种说话方式（全 public，已确认访问权限）

```
Unit.h:1823   virtual void Say(std::string_view text, Language language, WorldObject const* target = nullptr);
Unit.h:1824   virtual void Yell(std::string_view text, Language language, WorldObject const* target = nullptr);
Unit.h:1825   virtual void TextEmote(std::string_view text, WorldObject const* target = nullptr, bool isBossEmote = false);
Unit.h:1826   virtual void Whisper(std::string_view text, Language language, Player* target, bool isBossWhisper = false);
```

**访问权限确认**：位于 `Unit.h:811` 开始的 `public:` 段
（下一个 `private:` 在 1669 行），可直接调用。

### 实现差异（Unit.cpp:14773 起，查了实现）

四个都是 `Talk()` 的薄封装，区别只在**频道**和**距离配置**：

| API | 频道 | 距离配置 | 表现 |
|---|---|---|---|
| `Say` | `CHAT_MSG_MONSTER_SAY` | `CONFIG_LISTEN_RANGE_SAY` | 白字，近距离 |
| `Yell` | `CHAT_MSG_MONSTER_YELL` | `CONFIG_LISTEN_RANGE_YELL` | 红字，远距离 |
| `TextEmote` | `CHAT_MSG_MONSTER_EMOTE` | `CONFIG_LISTEN_RANGE_TEXTEMOTE` | 橙字，第三人称 |
| `TextEmote(isBoss=true)` | `CHAT_MSG_RAID_BOSS_EMOTE` | 同上 | **全屏 BOSS 提示** |
| `Whisper` | `CHAT_MSG_MONSTER_WHISPER` | 定向 | 只有目标能看见 |
| `Whisper(isBoss=true)` | `CHAT_MSG_RAID_BOSS_WHISPER` | 定向 | BOSS 密语 |

**`isBossEmote` / `isBossWhisper` 这两个 bool 很值钱** ——
它们能做出「全屏红字警告」那种演出效果，官方 `.npc` 系列没暴露这个参数。

### 还有 textId 重载（走 creature_text 表）

```
Unit.h:1828-1831   Say/Yell/TextEmote/Whisper(uint32 textId, ...)
```

走 `CreatureTextMgr`（`src/server/game/Texts/CreatureTextMgr.cpp`），
支持本地化（`creature_text_locale` 表，CreatureTextMgr.cpp:196）。

**本期不用**，但记下来 —— 将来剧情要多语言时是这条路。

---

## 二、官方 `.npc say` 的可借鉴点

`cs_npc.cpp:910-918`：

```cpp
creature->Say(text, LANG_UNIVERSAL);

// make some emotes
switch (text.back())
{
    case '?':   creature->HandleEmoteCommand(EMOTE_ONESHOT_QUESTION);      break;
    case '!':   creature->HandleEmoteCommand(EMOTE_ONESHOT_EXCLAMATION);   break;
    default:    creature->HandleEmoteCommand(EMOTE_ONESHOT_TALK);          break;
}
```

**按末尾标点自动配表情。** 这个思路直接抄，而且能扩展：

| 标点 | 表情 |
|---|---|
| `?` | `EMOTE_ONESHOT_QUESTION` (6) |
| `!` | `EMOTE_ONESHOT_EXCLAMATION` (5) |
| `...` | 无（沉思，不播）|
| 默认 | `EMOTE_ONESHOT_TALK` (1) |

**和 step29 `.emote` 天然联动** —— 说话自带动作，不用两条指令。

### 中文标点要一起处理

官方只判 ASCII 的 `?` `!`。中文剧情会用 `？` `！` `。` `……`，
这些是 UTF-8 多字节，`text.back()` 取最后一个**字节**会判错。
**必须按 UTF-8 尾部多字节比较，不能用 `back()`。**

---

## 三、Language 枚举

```
SharedDefines.h:829   LANG_UNIVERSAL  = 0     所有人都懂（剧情用这个）
SharedDefines.h:830   LANG_ORCISH     = 1
SharedDefines.h:834   LANG_COMMON     = 7
```

剧情固定用 `LANG_UNIVERSAL`，避免出现「你看不懂对方说什么」。

---

## 四、【关键约束】延时台词做不了自动播放

### 查证结果

| 方案 | 结论 |
|---|---|
| `TaskScheduler` | 存在（`TaskScheduler.h:225 Schedule`），**但需要宿主对象持续调 Update** |
| `Creature` 内置 scheduler | **没有**（grep Creature.h 无 TaskScheduler 成员）|
| `events.ScheduleEvent` | 是 `EventMap`，**只在 CreatureAI 内部可用**（zone_undercity.cpp:129 那种）|
| `World::m_timers` | 是固定用途的 IntervalTimer（World.h:819），不能塞任务 |

**结论**：GM 指令是「一次性调用」，执行完就返回，
**没有一个持续 Update 的宿主**能承载延时任务。

### 三条可选路线

| 路线 | 做法 | 代价 |
|---|---|---|
| **A. 不做延时** | `.say` 只管说一句，多句由 GM 手动敲 | 最简单，够用 |
| **B. 注入 AI** | 像 `.dummy`（step23）那样 `AIM_Initialize` 挂个带 EventMap 的 AI | 复杂，且会顶掉原 AI |
| **C. 存台词本** | 存进表，配合将来的 `.timeline` 播放 | 需要先做 timeline |

**建议走 A**，理由：
- 你做剧情时是**手动导演**，不是自动过场
- 真要自动序列，那是 `.timeline` 的活（已在待办里）
- B 会顶掉 NPC 原有 AI，副作用大

---

## 五、`.say` 设计方案

### 指令形态

```
.say <文本>                     选中目标说话（白字）
.say yell <文本>                喊话（红字，远距离）
.say emote <文本>               第三人称描述（橙字）
.say boss <文本>                全屏 BOSS 提示 <- 官方没暴露
.say whisper <文本>             对自己密语
.say r <半径> <文本>            周围所有 NPC 一起说
.say entry <ID> <文本>          指定 entry 的 NPC 说
.say me <文本>                  自己说
.say noemote <文本>             不自动配表情
```

### 自动表情（可关）

默认按末尾标点配 `.emote` 的一次性动作，`noemote` 关掉。

### 和 `.emote` 的复用

| 组件 | 复用 |
|---|---|
| `Tok()` | 直接抄 |
| `CollectNear()` | 直接抄（含 NPCBot/宠物保护）|
| `CollectByEntry()` | 直接抄 |
| 旧式注册语法 | 一致 |
| 权限 `RBAC_PERM_COMMAND_WORLDTOOLS` | 一致 |

### 文本参数的特殊处理

**台词含空格，不能用 `Tok()` 切完就丢。**
需要「前 N 段当参数，剩下全部拼回原文」的取法。

---

## 六、待确认（写代码时验证）

| 项 | 说明 |
|---|---|
| UTF-8 中文标点判断 | 要按字节序列比对 `？`(EF BC 9F) `！`(EF BC 81) |
| `Whisper` 需要 `Player*` | 只能对玩家密语，NPC 之间不行 |
| 空文本保护 | `text.back()` 对空串是 UB，必须先判空 |
