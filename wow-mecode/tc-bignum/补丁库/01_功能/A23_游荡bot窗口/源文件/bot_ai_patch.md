# step38 让游荡 bot 拥有完整窗口 —— 改动清单

> 用户需求：「游荡的npc完全没有召唤的npc的谈话窗口」
> 「没有窗口，什么都做不了」
> 改上游：`bot_ai.cpp`（2 处）

---

## 一、根因（实查确认）

### 第一层：菜单项一个都没有 -> 窗口自动关

`bot_ai.cpp:7979`：

```cpp
if (!menus)
{
    player->PlayerTalkClass->SendCloseGossip();   // 没有任何菜单项就关窗
    return true;
}
```

### 第二层：`menus` 的 7 个赋值点，游荡bot一个都碰不到

| 行 | 条件 | 游荡bot |
|---|---|---|
| 7758 | `player->IsGameMaster()` | 只有GM |
| 7813 | `... && IAmFree() && !IsWanderer()` | **被 `!IsWanderer()` 挡住** |
| 7822 | `if (_botData->owner)` | **无主，进不去** |
| 7870/7882/7891 | 在 7822 块内 | 同上 |
| 7975 | `_botclass >= BOT_CLASS_EX_START` | 只有特殊职业 |

### 第三层：`player == master` 也不成立

```cpp
// bot_ai.cpp:460  无主bot的 master 指向【它自己】
master = reinterpret_cast<Player*>(me);
```

所以 `bot_ai.cpp:7820 if (player == master)` 对游荡bot**永远为假**。

**三层叠加 = 完全没有窗口。**

---

## 二、改动 1：让游荡bot进入"完整菜单"块

**Ctrl+F 搜**（`bot_ai.cpp:7816`）：

```cpp
    if (_botData->owner)
    {
        Group const* gr = player->GetGroup();

        if (player == master)
        {
            menus = true;
```

**改为**：

```cpp
    // step38: 游荡bot（无主）原本进不来这个块，导致完全没有控制菜单。
    //         开关打开时让它也能进，这样才有装备/角色/阵型/技能/天赋等窗口。
    bool const wanderer_full_menu = IsWanderer() && BotCfg::IsWanderingBotHireEnabled();

    if (_botData->owner || wanderer_full_menu)
    {
        Group const* gr = player->GetGroup();

        // step38: 无主bot的 master 指向它自己（bot_ai.cpp:460），
        //         所以 player == master 永远为假。游荡bot要单独放行。
        if (player == master || wanderer_full_menu)
        {
            menus = true;
```

---

## 三、改动 2：招募选项也要对游荡bot开放

**Ctrl+F 搜**（`bot_ai.cpp:7761`，step33 已改过一次）：

```cpp
    if (player_guidlow != _botData->owner && IAmFree() && (!IsWanderer() || BotCfg::IsWanderingBotHireEnabled()))
```

**如果你 step33 已经改成上面这样，这处就不用动。**

如果还是原文 `&& !IsWanderer())`，改成上面那样。

---

## 三点五、已排查：块内不会因无主而崩溃

我逐行扫了 `7816-7971` 整个块，只有一处解引用 `master`：

```cpp
// bot_ai.cpp:7858
if (player == master || (gr && gr->IsMember(master->GetGUID())))
```

**这里安全**，因为无主bot的 master 不是 nullptr，
而是指向它自己（`bot_ai.cpp:460  master = reinterpret_cast<Player*>(me);`）。

所以 `master->GetGUID()` 返回的是 bot 自己的 GUID，
`gr->IsMember()` 查不到，条件为假，只是不显示"职业专属"那几个选项。
**不崩，只是少两个菜单项。**

---

## 四、注意：块内可能有依赖 owner 的代码

`if (_botData->owner)` 块很长（7816 到 7975 左右），
里面可能有假设"一定有主人"的代码。

**已知的一处**（约 7957）：

```cpp
if (!shared_owner)
{
    ... BOT_TEXT_UR_DISMISSED ...   // "解雇"选项
}
```

游荡bot还没被雇佣，显示"解雇"没意义但**不会崩**
（只是多一个无效选项，点了会走解雇逻辑，对无主bot无影响）。

**如果编译后发现某个选项点了报错，把报错发我，我单独加条件屏蔽。**

---

## 五、这样改之后能得到什么

游荡bot右键将出现**和雇佣bot一样的完整窗口**：

```
管理装备        GOSSIP_SENDER_EQUIPMENT
管理角色        GOSSIP_SENDER_ROLES_MAIN
管理阵型        GOSSIP_SENDER_FORMATION
管理技能        GOSSIP_SENDER_ABILITIES
管理天赋        GOSSIP_SENDER_SPEC        <- 10级以上，天赋已内置！
雇佣我          GOSSIP_SENDER_HIRE
...
```

**用户要的"背包/技能/天赋UI"，NPCBot 上游其实已经用 Gossip 做好了**，
只是游荡bot看不到而已。

---

## 六、编译

只改 `bot_ai.cpp` 内容，**不用重跑 CMake**。

---

## 七、验证

```
[ ] .tome                 传送到游荡bot旁边（不要用 .bf come，更干净）
[ ] 右键它
    预期：弹出完整窗口，有装备/角色/阵型/技能/天赋等选项
[ ] 点"管理装备"          能进子菜单
[ ] 点"管理天赋"          能看到天赋选项（需要bot >= 10级）
[ ] 点"雇佣我"            能招募
[ ] 招募后再右键          菜单应该完全一样（因为已经有owner了）
```

**如果窗口出来了但某个选项点了没反应或报错**，
把选项名和报错发我，单独处理那一个。
