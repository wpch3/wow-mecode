# step37 解法A —— 给 bot_ai 加"重设路点"接口

> 目的：修复 `.bf come` 把游荡bot拽过来后，它念炉石跑回去的死循环
> 改上游文件：`bot_ai.h` / `bot_ai.cpp`
> 每处都给【精确行号 + 原文】

---

## 一、为什么必须改上游

`.bf come` 之后 bot 念炉石，是因为它发现"离目标路点太远"：

```cpp
// bot_ai.cpp:18632
if (mapid != me->GetMap()->GetId() || _evadeCount >= 50 ||
    me->GetExactDist2d(pos) > MAX_WANDER_NODE_DISTANCE || ...)
{
    evadeDelayTimer = 12000;
    me->CastSpell(me, WANDERER_HEARTHSTONE);   // 10秒读条，期间无法对话
    return;
}
```

而这个"目标路点"是三个 **private** 成员决定的（scripts 里够不着）：

| 成员 | 位置 | 访问段 |
|---|---|---|
| `_travel_node_cur` | bot_ai.h:765 | **private**(590) |
| `homepos` | bot_ai.h:697 | **private**(590) |
| `evadeDelayTimer` | bot_ai.h:723 | **private**(590) |

所以要加一个 public 方法，让外部能把 bot 的"家"改成它现在的位置。

---

## 二、改动 1：`bot_ai.h` 加方法声明

**找到这一行**（`bot_ai.h:198`）：

```cpp
    WanderNode const* GetClosestWanderNode() const;
```

**在它下面加**：

```cpp
    // step37: 把游荡bot的"家"重设到它当前位置附近，防止被传送后念炉石跑回去
    bool ResetWanderHomeToCurrent();
```

---

## 三、改动 2：`bot_ai.cpp` 加实现

**找到这个函数的结尾**（`bot_ai.cpp:19054` 开始的 `GetHomePosition`）：

```cpp
void bot_ai::GetHomePosition(uint16& mapid, Position* pos) const
{
    if (IsWanderer())
    {
        mapid = _travel_node_cur->GetMapId();
        pos->Relocate(homepos);
    }
    else
    {
        CreatureData const* data = me->GetCreatureData();
        mapid = data->mapId;
        pos->Relocate(data->spawnPoint);
    }
}
```

**在这个函数【后面】加一个新函数**：

```cpp
// step37: 把游荡bot的"家"重设到当前位置
//
// 背景：游荡bot的"家"是 _travel_node_cur（当前目标路点），不是出生点。
//       用 GM 指令把它传送过来后，它会发现自己离目标路点极远
//       （bot_ai.cpp:18632 的 MAX_WANDER_NODE_DISTANCE 判定），
//       于是念炉石（WANDERER_HEARTHSTONE，10秒读条）传回去。
//       读条期间 IsCasting()==true，会被 bot_ai.cpp:7704 拦截，无法对话。
//
// 做法：把目标路点改成离当前位置最近的那个，并把 homepos 挪到脚下，
//       同时清掉 evade 计数和延时，让它认为"已经到家了"。
//
// 返回：true = 成功重设；false = 不是游荡bot（无需处理）
bool bot_ai::ResetWanderHomeToCurrent()
{
    if (!IsWanderer())
        return false;

    // 找离当前位置最近的路点。找不到就保持原样，不要置空（会 ASSERT 崩）
    if (WanderNode const* node = GetClosestWanderNode())
        _travel_node_cur = node;

    // 把"家"挪到脚下 —— GetHomePosition 对游荡bot返回的就是 homepos
    homepos.Relocate(me);

    // 清掉 evade 状态，否则 _evadeCount >= 50 那条依然会触发炉石
    _evadeCount = 0;
    evadeDelayTimer = 0;

    // 如果正在念炉石，打断它
    if (me->HasUnitState(UNIT_STATE_CASTING))
        me->InterruptNonMeleeSpells(true);

    return true;
}
```

---

## 四、注意：`_evadeCount` 也要能访问

上面用到了 `_evadeCount`。它同样是 private，
**但因为我们把函数写在 `bot_ai.cpp` 内部（是类的成员函数），所以能直接访问。**

这就是为什么解法A必须在 bot_ai.cpp 里加函数，
而不能在 scripts 里想办法绕。

---

## 五、验证改动是否正确

改完编译前，确认三件事：

```
[ ] bot_ai.h:199 附近有 ResetWanderHomeToCurrent 声明
[ ] bot_ai.cpp 的 GetHomePosition 函数【后面】有实现
[ ] 实现里的 4 个成员都能编译通过：
      _travel_node_cur / homepos / _evadeCount / evadeDelayTimer
```

**如果报 `_evadeCount 未声明`**，搜一下它的真实名字：
```
grep -n "_evadeCount" bot_ai.h
```

---

## 六、编译

改的是 `.h` 和 `.cpp` 的内容，**没有新增文件**：
- **不用重跑 CMake**
- 但 `bot_ai.h` 被很多文件 include，**改它会触发大范围重编译**，会比较慢
