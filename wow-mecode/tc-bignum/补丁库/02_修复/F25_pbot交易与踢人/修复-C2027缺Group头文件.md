# 修复 C2027「使用了未定义类型 Group」

## 我的错

`.pbot kick` 的代码用了 `grp->IsMember()` / `grp->IsLeader()` / `grp->ChangeLeader()`，
但我**没让你加 `#include "Group.h"`**。

## 为什么会报错

`Player.h:64` 里 `Group` **只有前向声明**：

```cpp
class Group;
```

前向声明只够用来声明指针 `Group*`，
**不够调用它的成员函数** —— 编译器不知道 `Group` 里有什么，所以报
`C2027 使用了未定义类型`。

而 `Player.h` 本身**没有** include `Group.h`（已实查确认），
所以必须在 `cs_playerbot.cpp` 里自己加。

---

## 修法：加一行 include

文件：`D:\TrinityCore\src\server\scripts\Commands\cs_playerbot.cpp`

### 搜索原文（约 51 行）

```cpp
#include "Map.h"                // Map::IsDungeon / IsBattlegroundOrArena / GetInstanceId
```

### 替换为

```cpp
#include "Group.h"              // Group::IsMember / IsLeader / ChangeLeader (Group.h:185 public段)
#include "Map.h"                // Map::IsDungeon / IsBattlegroundOrArena / GetInstanceId
```

**就这一行，改完重编译。**

---

## 顺带确认（都已实查，不用改）

`.pbot kick` 用到的其它符号都能拿到：

```text
Group.h:210   RemoveMember                              public(185段)
Group.h:211   ChangeLeader(ObjectGuid)                  public(185段)
Group.h:228   GetLeaderGUID()                           public(185段)
Group.h:240   IsLeader(ObjectGuid)                      public(185段)
Player.h:1593 static RemoveFromGroup(Group*, ObjectGuid, RemoveMethod, ObjectGuid, char const*)  public

SharedDefines.h:3731-3737  enum RemoveMethod : uint8
    GROUP_REMOVEMETHOD_DEFAULT  = 0
    GROUP_REMOVEMETHOD_KICK     = 1     <- 我们用这个
    GROUP_REMOVEMETHOD_LEAVE    = 2
    GROUP_REMOVEMETHOD_KICK_LFG = 3
```

`GROUP_REMOVEMETHOD_KICK` 在 `src/server/shared/SharedDefines.h`，
`cs_playerbot.cpp` 已经 include 了 `Common.h`，能间接拿到，**不用额外加**。

---

## 教训（已记入坑表）

我给 `.pbot kick` 代码时，检查了**函数签名和访问段**，
但**没检查调用方文件的 include 列表**。

以后给代码前会先 `grep -n "#include" 目标文件`，
确认用到的每个类型都有对应头文件。
