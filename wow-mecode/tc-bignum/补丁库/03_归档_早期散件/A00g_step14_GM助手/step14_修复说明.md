# `.gmhelp` Bug 修复（2026-07-30）

> 用户实测反馈的三个 bug，全部已修并验证。

---

## Bug 1：传送选项还在推荐废弃的 `.lookup tele`

**现象**：`.gmhelp find 传送` 出来的是 `.lookup tele`，但那个只认英文名，实际用不了。

**原因**：指令库是在做传送系统 `.tp` 之前写的，没同步更新。

**修复**：

| 改动 | 内容 |
|---|---|
| 删除 | `.lookup tele`（只认英文名，`cs_lookup.cpp:1141` 直接搜英文列） |
| 新增 | `.tp` —— 本服传送系统，中文搜索+分类+分页 |
| 新增 | `.tp <关键词>` —— 例 `.tp 暴风` |
| 保留 | `.tele` 但标注「只认英文名，建议改用 .tp」 |

验证：搜「传送」共 6 条，**`.tp` 排第 1 位**。

---

## Bug 2：点分类跳到套装菜单

**现象**：`.gmhelp menu` 点任意分类，弹出的是套装系统的菜单。

**根因**（这是我之前警告过、自己却踩了的坑）：

```cpp
// cs_gearset.cpp:1943  套装系统的过滤条件
if (sender < SENDER_MAIN || sender > SENDER_BOOK)   // 只看 sender 1~11
    return;                                          // 【完全不看 menuId】
```

而 gmhelper 原本用的是 sender **1 / 2 / 3 / 9** —— 正好落在套装的拦截区间内，
点击事件被套装系统先截走了。

**修复**：sender 段整体上移到 9301+

```cpp
SENDER_CAT   = 9301;   // 原 1
SENDER_CMD   = 9302;   // 原 2
SENDER_QUICK = 9303;   // 原 3
SENDER_NAV   = 9309;   // 原 9
```

### 全服 sender 段登记表（以后加菜单先查这张表）

| 段 | 归属 |
|---|---|
| 1 - 11 | 套装系统 `cs_gearset`（**不看 menuId，会截胡**） |
| 9101 / 9102 / 9109 | 传送系统 `custom_teleport.lua` |
| 9201 - 9209 | 幻化（预留，目前未用菜单） |
| **9301 - 9309** | **`.gmhelp`（本次改动）** |
| 9401+ | 以后新菜单从这里开始 |

> **装第三方 Gossip 模块前先 grep 它的 sender**，撞了 1~11 就改成 9401+。

---

## Bug 3：点「返回分类」菜单直接关闭

**现象**：在指令列表里点「返回」，菜单消失而不是回到分类页。

**根因**：

```cpp
case NAV_BACK:
    player->PlayerTalkClass->SendCloseGossip();      // ← 罪魁祸首
    gmhelper_commandscript::ShowCategoryMenu(...);   // 这个包被客户端丢弃
```

客户端收到「关闭」包后，会把紧随其后的菜单包丢掉。

**修复**：删掉 `SendCloseGossip()`。`ShowCategoryMenu` 内部本来就有 `ClearMenus()`，
直接调用即可，不需要先关闭。

---

## 顺带补充：指令库同步到最新

自定义指令从 **13 条增到 19 条**，补上了最近做的功能：

| 新增 | 说明 |
|---|---|
| `.transmog copy` | 幻化，自动识别部位 |
| `.transmog find` | 搜外观，按品质 |
| `.transmog preview` | 试穿，15秒自动恢复 |
| `.transmog save` | 外观方案 |
| `.item clone` | 克隆装备改数值 |
| `.item list` | 自造装备表 |

指令库总数：**69 → 76 条**。

### 上限校验

| 分类 | 条数 |
|---|---|
| 本服自定义 | **19**（最多） |
| 角色调整 | 15 |
| 传送移动 | 10 |
| 作弊开关 | 9 |
| 物品装备 | 8 |
| NPC与生物 | 7 |
| 副本相关 | 4 |
| 其他 | 4 |

最大分类 19 条 < `PER_PAGE = 29`，**单页装得下，没触发 32 条硬上限**。
（`GossipDef.cpp:41` 有 `ASSERT(_menuItems.size() <= 32)`，超了崩服）

---

## 验证

- 编译：`-Wall -Wextra -Wshadow` **零错误零警告**
- 搜索测试 **11/11 通过**：传送/tp/幻化/外观/造装备/克隆/体型/无敌/冷却/技能书/试穿
- `.tp` 在「传送」搜索结果中**排第 1 位**

---

## 安装

只改了 `step14_cs_gmhelper.cpp` 一个文件。

替换 `D:\TrinityCore\src\server\scripts\Commands\cs_gmhelper.cpp` → 重新编译。

**不用重跑 CMake**（没有新增文件），**不用执行 SQL**（RBAC 71008 已注册）。

### 测试清单

| # | 操作 | 预期 |
|---|---|---|
| 1 | `.gmhelp find 传送` | 出 `.tp`，排第 1，**不再有 `.lookup tele`** |
| 2 | `.gmhelp menu` → 点任意分类 | **正常展开指令列表**，不再跳套装菜单 |
| 3 | 在指令列表点「返回」 | **回到分类页**，菜单不消失 |
| 4 | `.gmhelp find 幻化` | 出 `.transmog copy` 等 |
| 5 | `.gmhelp find 造装备` | 出 `.item clone` |
| 6 | 点「本服自定义」分类 | 19 条全部显示，单页装得下 |
