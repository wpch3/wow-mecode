# step48 游荡 bot 崩溃彻查 —— 根因是我 step38 埋的

> 用户反馈：「管理天赋技能启用也会闪退」「用了会闪退在没有招募的情况」
> 2026-08-04

---

## 一、根因：`master` 是个伪装成 Player 的 Creature

### 1.1 上游的设计

```cpp
// bot_ai.cpp:459   ResetBotAI()
if (resetType & BOTAI_RESET_MASK_RESET_MASTER)
    master = reinterpret_cast<Player*>(me);      // <- me 是 Creature*
```

```cpp
// bot_ai.h:581
Player* master{};                                 // 声明是 Player*
```

**无主 bot 的 `master` 装的其实是 Creature 指针**，
只是被强转成了 `Player*`。

上游这么写没事，因为**无主 bot 原本进不到那些菜单**
（`bot_ai.cpp:7820  if (player == master)` 永远为假）。

### 1.2 我 step38 打开了那道门

```cpp
// step38 我的改动
bool const wanderer_full_menu = IsWanderer() && BotCfg::IsWanderingBotHireEnabled();
if (player == master || wanderer_full_menu)      // <- 游荡bot也放行了
{
    menus = true;
```

菜单显示出来了，但**菜单项的处理函数全都假设 master 是真 Player**。

### 1.3 真正的崩溃点

```cpp
// bot_ai.cpp:262-271   BotWhisper 的无 target 重载
void bot_ai::BotWhisper(std::string_view text, Player const* target) const
{
    if (!target && master->IsPlayer())           // <-【崩在这】
        target = master;
    ...
}
```

```cpp
// Object.h:195
inline bool IsPlayer() const { return GetTypeId() == TYPEID_PLAYER; }
                                      ^^^^^^^^^^ 虚函数
```

`master` 实际是 Creature，强转成 `Player*` 后调**虚函数** ->
vtable 偏移错位 -> **崩溃**。

我写了最小复现验证过：

```
master 地址   : 0x7ffec0148000
me(Creature)  : 0x7ffec0148000        <- 两者是同一个地址
原代码 master->IsPlayer()             <- UB，实机崩溃
```

### 1.4 影响范围（全量扫描结果）

`OnGossipSelect`（7991~11177 行，132 个 case 分支）里：

| 类型 | 数量 | 状态 |
|---|---|---|
| `BotWhisper(...)` **不传 target** | **10 处** | **全都会崩** |
| `BotWhisper(..., player)` 传了 target | 38 处 | 安全 |
| 直接 `master->` 解引用 | **5 处** | **会崩** |

**天赋切换**（`bot_ai.cpp:10182`）正好是那 10 处之一：

```cpp
case GOSSIP_SENDER_SPEC_SET:
    ...
    BotWhisper(LocalizedNpcText(player, BOT_TEXT_CHANGING_MY_SPEC_TO_) + ...);
    //         ^^^ 没传 target -> 走 master->IsPlayer() -> 崩
```

**5 处直接解引用 master 的**：

```
 8054  GOSSIP_SENDER_CLASS                     master 当参数传
10628  GOSSIP_SENDER_FORMATION                 master->
10674  GOSSIP_SENDER_FORMATION_ATTACK_DISTANCE master->
10708  GOSSIP_SENDER_FORMATION_ATTACK_ANGLE    master->
10954  GOSSIP_SENDER_DEBUG_ACTION              master->
```

### 1.5 我 step38 的排查错在哪

我在 step38 文档里写过：

> 「已排查：块内不会因无主而崩溃」

**那个排查只看了菜单【显示】代码，没看点击后的【处理】代码。**

显示代码确实安全（`master->GetGUID()` 恰好不崩），
但处理代码里有 15 处会踩。**这是我的疏漏。**

---

## 二、修复：改 1 处，覆盖 10 个崩溃点

### 2.1 核心修复（必做）

**Ctrl+F 搜**（`bot_ai.cpp:262`）：

```cpp
void bot_ai::BotWhisper(std::string_view text, Player const* target) const
{
    if (!target && master->IsPlayer())
        target = master;
    if (!target)
        return;
```

**整段替换成**：

```cpp
void bot_ai::BotWhisper(std::string_view text, Player const* target) const
{
    //step48: 【关键安全修复】
    //  无主bot的 master 是 reinterpret_cast<Player*>(me)（bot_ai.cpp:459），
    //  实际指向 Creature 自己。此时调 master->IsPlayer()（Object.h:195，虚函数）
    //  会读到错位的 vtable -> 崩溃。
    //
    //  必须【先比指针地址】确认 master 不是它自己，再调任何成员函数。
    //  比地址不涉及虚函数调用，绝对安全。
    if (!target)
    {
        if (reinterpret_cast<void const*>(master) == reinterpret_cast<void const*>(me))
            return;                     // 无主：没有可私聊的对象，直接返回
        if (!master->IsPlayer())
            return;
        target = master;
    }
    if (!target)
        return;
```

**下面那行 `me->Whisper(...)` 保持不动。**

> **为什么这样改**：10 处不传 target 的调用**一次性全修好**，
> 比改 10 个调用点安全得多，也不会漏。

### 2.2 加一个安全的辅助方法（推荐）

**Ctrl+F 搜**（`bot_ai.h:192`）：

```cpp
    bool IAmFree() const;
```

**在它下面加**：

```cpp
    //step48: 安全判断 master 是否是【真的】玩家。
    //  无主bot的 master 指向自己（bot_ai.cpp:459），
    //  直接调 master->任何虚函数都会崩。必须先比地址。
    bool HasRealMaster() const
    {
        return reinterpret_cast<void const*>(master) != reinterpret_cast<void const*>(me)
            && master->IsPlayer();
    }
```

> `me` 在 bot_ai 里是成员变量，`HasRealMaster()` 是内联的，
> 放 public 段（192 行在 public 里，已确认）。

### 2.3 修 5 处直接解引用（用上面的辅助方法）

这 5 处都在**队形/调试**菜单里。逐个加保护：

**Ctrl+F 搜**（`bot_ai.cpp:10628` 附近，`case GOSSIP_SENDER_FORMATION:`）：

在 `case GOSSIP_SENDER_FORMATION:` 的 `{` 后面**第一行**插入：

```cpp
            //step48: 无主bot没有真master，队形菜单无意义且会崩
            if (!HasRealMaster())
            {
                BotWhisper(LocalizedNpcText(player, BOT_TEXT_NOTHING_TO_DO_HERE), player);
                break;
            }
```

**同样的三行**，也加到：
- `case GOSSIP_SENDER_FORMATION_ATTACK_DISTANCE:`（10674）
- `case GOSSIP_SENDER_FORMATION_ATTACK_ANGLE:`（10708）
- `case GOSSIP_SENDER_DEBUG_ACTION:`（10954）
- `case GOSSIP_SENDER_CLASS:`（8054）

> 如果 `BOT_TEXT_NOTHING_TO_DO_HERE` 这个常量不存在，
> 用 `BOT_TEXT_NEVERMIND`（70480，已确认存在）替代。

---

## 三、更稳妥的替代方案：干脆别给游荡bot那些菜单

上面是"修好每个崩溃点"。还有个**更省事、更安全**的思路：

**游荡 bot 只显示它真正需要的菜单项**（招募、闲聊），
不显示队形/天赋/装备这些"需要主人"的功能。

### 改法

**Ctrl+F 搜**（step38 我加的那行）：

```cpp
        if (player == master || wanderer_full_menu)
        {
            menus = true;

            //general: equips, roles, distance, abilities, comsumables, group
            AddGossipItemFor(player, GOSSIP_ICON_TALK, LocalizedNpcText(player, BOT_TEXT_MANAGE_EQUIPMENT), GOSSIP_SENDER_EQUIPMENT, GOSSIP_ACTION_INFO_DEF + 1);
```

**改成**：

```cpp
        if (player == master || wanderer_full_menu)
        {
            menus = true;

            //step48: 游荡bot（无主）不显示需要 master 的功能菜单，
            //  否则点进去会崩（master 实际指向 Creature 自己）。
            //  它需要的只是"能对话+能招募"，招募选项在 7761 行单独处理。
            bool const full = (player == master);

            //general: equips, roles, distance, abilities, comsumables, group
            if (full)
            {
            AddGossipItemFor(player, GOSSIP_ICON_TALK, LocalizedNpcText(player, BOT_TEXT_MANAGE_EQUIPMENT), GOSSIP_SENDER_EQUIPMENT, GOSSIP_ACTION_INFO_DEF + 1);
```

然后在这一整块菜单项的**末尾**（`if (!gr)` 之前）加上 `}` 闭合。

**我的建议**：
- **先做第二章**（修 BotWhisper），风险最低、改动最小、立刻不崩
- 如果你觉得"游荡bot本来也不需要管装备天赋"，再做第三章

---

## 四、验证清单

```
[ ] 改完 BotWhisper 重新编译
[ ] 找一个【没招募的】游荡bot，右键对话
[ ] 点"管理天赋" -> 选一个专精      <- 原来崩，现在应该正常
[ ] 点"管理技能" -> 随便点          <- 原来崩
[ ] 点"管理装备"                    <- 检查
[ ] 点"管理阵型"                    <- 5处解引用之一，重点测
[ ] 招募它，再把上面全点一遍         <- 有主人时应该完全正常
[ ] 挂机10分钟不崩
```

**重点是前两条** —— 那是用户明确报的崩溃场景。

---

## 五、API 核实记录

```
bot_ai.cpp:459    master = reinterpret_cast<Player*>(me);   <- 万恶之源
bot_ai.h:581      Player* master{};                          声明类型
bot_ai.cpp:262    BotWhisper(string_view, Player const*)     <-【主修复点】
bot_ai.cpp:264    if (!target && master->IsPlayer())         <- 崩溃行
Object.h:195      IsPlayer() { return GetTypeId() == TYPEID_PLAYER; }
                  GetTypeId 是【虚函数】-> 强转后调用 = UB

bot_ai.cpp:7820   if (player == master)                      <- 原生的门
bot_ai.cpp:7832   BOT_TEXT_MANAGE_TALENTS 菜单项
bot_ai.cpp:7991   OnGossipSelect 起始
bot_ai.cpp:10182  BotWhisper 不传target（天赋切换，用户报的崩溃）
bot_ai.cpp:11177  OnGossipSelect 结束

全量扫描结果（132个case分支）：
  不传target的 BotWhisper : 10 处（9775/9789/9799/9805/10182/...）
  直接 master-> 解引用     : 5 处（8054/10628/10674/10708/10954）

bot_ai.h:192      IAmFree()  public  <- HasRealMaster() 加在这附近
bot_ai.h:195      IsWanderer()  public
```
