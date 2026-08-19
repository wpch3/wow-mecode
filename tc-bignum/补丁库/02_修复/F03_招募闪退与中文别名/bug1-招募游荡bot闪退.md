# Bug 1：招募游荡 bot 会闪退（崩服）

> 用户反馈：「游荡npc如果用npcbot指令招募会闪退」
> 2026-08-04

---

## 一、根因：bot 变成了「有主人 + 仍是游荡者」的矛盾状态

### 崩溃点（两处，无条件解引用空指针）

```cpp
// bot_ai.cpp:15873   OnBotDied / 被击杀日志
IsWanderer() ? _travel_node_cur->GetName() : "''");
//              ^^^^^^^^^^^^^^^^^^^^^^^^^^ 这里
```

```cpp
// bot_ai.cpp:15926   击杀别人的日志
_travel_node_cur->GetName());
// bot_ai.cpp:15933
IsWanderer() ? _travel_node_cur->GetName() : "''");
```

只要 `IsWanderer()` 为真，就**无条件解引用** `_travel_node_cur`。

### 为什么招募之后它会变成空

```cpp
// bot_ai.cpp:14899  只有游荡bot在【初始化时】才会赋值
_travel_node_cur = ASSERT_NOTNULL(GetClosestWanderNode());
```

而招募流程（`bot_ai.cpp:284 SetBotOwner`）里：

```cpp
bool bot_ai::SetBotOwner(Player* newowner)
{
    ...
    if (newowner->GetBotMgr()->AddBot(me) & BOT_ADD_FATAL)
        return false;

    spawned = false;
    ...
    master = newowner;
    return true;
}
```

**全程没有清除 `_wanderer` 标志。**

于是 bot 变成：
- `master` = 你（不再是自己）
- `_wanderer` = **仍然是 true**
- `_travel_node_cur` = 招募后路点体系已经不再维护它

下次这只 bot **杀死任何东西 / 被杀** -> 走到 15873 / 15926 -> **空指针 -> 崩服**。

### 为什么上游没这个问题

上游 `botcommands.cpp:4932` **本来就禁止招募游荡 bot**：

```cpp
if (!bot || !bot->IsNPCBot() || bot->GetBotAI()->GetBotOwnerGuid() ||
    bot->GetBotAI()->IsWanderer() || bot->IsSummon())
    //                ^^^^^^^^^^^^^^ 这道拦截
{
    handler->SendSysMessage("You must select uncontrolled non-wandering npcbot");
    return false;
}
```

**step33 我放开了 gossip 层的招募，但没同步处理"招募后要退出游荡状态"这件事。**
这是我的疏漏 —— 我只想着"让它能被招募"，没想"招募之后它还是不是游荡者"。

---

## 二、修复：招募时正式退出游荡状态

### 2.1 给 bot_ai 加一个"取消游荡"的方法

`SetWanderer()`（bot_ai.h:196）只能设 true，**没有反向操作**，需要新增。

**Ctrl+F 搜**（`bot_ai.h:196`）：

```cpp
    //wandering bots
    bool IsWanderer() const { return _wanderer; }
    void SetWanderer();
```

**改成**（在 `SetWanderer();` 下面加一行）：

```cpp
    //wandering bots
    bool IsWanderer() const { return _wanderer; }
    void SetWanderer();
    void UnsetWanderer();       // step44: 被招募时退出游荡状态，防止空指针崩服
```

> **访问段确认**：这几行在 `bot_ai.h:53` 起的 **public 段**，
> 新增的 `UnsetWanderer()` 也落在 public，没有 C2248 风险。

### 2.2 实现它

**Ctrl+F 搜**（`bot_ai.cpp:20412`）：

```cpp
void bot_ai::SetWanderer()
{
    if (IAmFree())
    {
        _wanderer = true;
        if (botPet)
            botPet->GetBotPetAI()->SetWanderer();
    }
}
```

**在这个函数【下面】整段插入**：

```cpp
//step44: 被玩家招募时调用，正式退出游荡状态
//
//【为什么必须有这个】
//  bot_ai.cpp:15873/15926/15933 这三处会无条件解引用 _travel_node_cur：
//      IsWanderer() ? _travel_node_cur->GetName() : "''"
//  只要 _wanderer 还是 true，招募后的 bot 一旦杀人或被杀就会空指针崩服。
//
//  所以退出游荡状态时，_wanderer 和两个路点指针要【一起】清干净。
void bot_ai::UnsetWanderer()
{
    if (!_wanderer)
        return;

    _wanderer = false;

    // 路点指针一并清空。清成 nullptr 是安全的：
    // 上面那三处日志已经被 _wanderer=false 短路，不会再解引用。
    _travel_node_last = nullptr;
    _travel_node_cur = nullptr;

    // 宠物跟着一起退出（SetWanderer 也是这么递归的）
    if (botPet)
        botPet->GetBotPetAI()->UnsetWanderer();
}
```

> **botPet 的 UnsetWanderer**：`bpet_ai` 继承自 `bot_ai`，
> 所以加在 `bot_ai` 上它自动就有，不用另外改 bpet_ai.h。

### 2.3 在招募流程里调用它

**Ctrl+F 搜**（`bot_ai.cpp:284` 起的 `SetBotOwner` 里）：

```cpp
    if (newowner->GetBotMgr()->AddBot(me) & BOT_ADD_FATAL)
    {
        _checkMasterTimer += 30000;
        return false;
    }

    spawned = false;
```

**改成**（在 `spawned = false;` 上面插入）：

```cpp
    if (newowner->GetBotMgr()->AddBot(me) & BOT_ADD_FATAL)
    {
        _checkMasterTimer += 30000;
        return false;
    }

    //step44: 【必须在这里】退出游荡状态。
    //  放在 AddBot 成功【之后】—— AddBot 失败时 bot 还是自由身，
    //  这时候清掉游荡状态会让它变成"既不游荡也没主人"的僵尸。
    //  放在 master 赋值【之前】—— UnsetWanderer 内部不依赖 master。
    UnsetWanderer();

    spawned = false;
```

---

## 三、为什么这样改是安全的

### 3.1 时机选择

```
AddBot 失败  -> 直接 return，【不】动游荡状态   （bot 保持原样继续游荡）
AddBot 成功  -> UnsetWanderer()                 （正式转为你的随从）
```

放在 `AddBot` 之后是关键。如果放前面，AddBot 失败时 bot 会变成
「不游荡也没主人」的僵尸状态 —— 站在原地不动，也没法再招募。

### 3.2 清空指针不会引入新崩溃

那三处日志的写法都是：

```cpp
IsWanderer() ? _travel_node_cur->GetName() : "''"
```

`IsWanderer()` 已经是 false，**三目运算符短路**，根本走不到解引用。

`bot_ai.cpp:15926` 那处在 `if (IsWanderer())` 块内，同理进不去。

### 3.3 其余引用 `_travel_node_cur` 的地方

```
bot_ai.cpp:7398/7399/7400   炉石传送 —— 外层有 IsWanderer() 判断
bot_ai.cpp:11065            .npcbot info 调试输出 —— 有 if (_travel_node_cur) 判空
bot_ai.cpp:18565/18575/18576  路点推进 —— 整段在游荡逻辑里
bot_ai.cpp:14899            初始化赋值 —— 只在 spawn 时走
```

**全部都在 `IsWanderer()` 保护之下**，清空后不会被访问。

---

## 四、验证清单

```
[ ] 找一个野外游荡bot
[ ] 招募它                            应该成功，不闪退
[ ] .npcbot info                      确认它现在有主人
[ ] 【关键】带它去打个怪，让它杀死目标   <- 原来这一步会崩
[ ] 【关键】让它被怪打死               <- 原来这一步也会崩
[ ] 复活它，正常跟随
[ ] 解雇它（.npcbot remove）
[ ] 观察它是否恢复游荡（预期：不会自动恢复，变成普通自由bot）
[ ] 服务端挂10分钟不崩
```

**重点是那两个"关键"步骤** —— 崩溃只在击杀/死亡时触发，
招募当下不会立刻崩，容易误判成"修好了"。

---

## 五、附带说明：解雇后不会自动变回游荡

`UnsetWanderer()` 是单向的。解雇（`.npcbot remove`）后
bot 会变成**普通的无主 bot**（站在原地等人招募），而不是继续游荡。

这是有意为之：
- 恢复游荡需要重新分配路点（`GetClosestWanderNode()`），
  而那套逻辑只在 spawn 流程里跑过一遍
- 硬塞回去容易再制造新的空指针问题

如果你要「解雇后恢复游荡」，说一声，那需要在 `RemoveBot` 里
重新走一遍 `SetWanderer()` + 路点分配，我单独做。

---

## 六、API 核实记录

```
bot_ai.h:195    IsWanderer() const               public(53段)
bot_ai.h:196    SetWanderer()                    public(53段)
                UnsetWanderer()  <- 新增，同样在 public(53段)
bot_ai.h:762    _wanderer                        private(590段)
bot_ai.h:764    _travel_node_last                private(590段)
bot_ai.h:765    _travel_node_cur                 private(590段)
                （在 bot_ai 成员函数内访问私有成员，没问题）

bot_ai.cpp:284    SetBotOwner()      <- 修复插入点
bot_ai.cpp:20412  SetWanderer()      <- 新函数插在它下面
bot_ai.cpp:14899  _travel_node_cur = ASSERT_NOTNULL(GetClosestWanderNode())
bot_ai.cpp:15873  【崩溃点1】IsWanderer() ? _travel_node_cur->GetName() : "''"
bot_ai.cpp:15926  【崩溃点2】_travel_node_cur->GetName()
bot_ai.cpp:15933  【崩溃点3】IsWanderer() ? _travel_node_cur->GetName() : "''"

botcommands.cpp:4932  上游原生拦截（IsWanderer 那条）
```
