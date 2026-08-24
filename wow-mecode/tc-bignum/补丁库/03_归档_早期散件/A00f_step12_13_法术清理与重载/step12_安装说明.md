# 第 12 步：技能书清理 `.spell clean`

> 趁你测战斗节奏，我把最简单零风险的这个先做了。
> **纯新增文件，不动任何核心代码。**

---

## 解决什么问题

技能书里堆满低阶技能。学了「火球术 Rank16」，前面 15 个 Rank 全还在书里。

正常情况下学高阶会自动顶掉低阶，但这些情况会残留：
- 用 `.learn all` 或 `PlayerStart.AllSpells = 1` 批量学的 ← **你的 casual 档就是这个**
- 数据库直接塞进去的
- 跨版本升级留下的
- 天赋重置后的残留

---

## 指令

```
.spell clean              预览会清理哪些（不实际删除）
.spell clean confirm      执行清理
.spell count              技能数量统计
.spell find <关键词>      搜索自己会的技能
```

### 输出示例

```
.spell count
  技能总数    ：2847
  主动技能    ：1203
  被动技能    ：1644
  天赋技能    ：61
  带等级的技能：892
  可清理低阶：714 条

.spell clean
  火球术 (等级1)  ->  保留等级16
  火球术 (等级2)  ->  保留等级16
  ...
  当前技能总数：2847
  可清理：714 条  清理后剩余：2133 条
  这只是预览，没有实际删除。
  确认清理请输入：.spell clean confirm
```

---

## 安全设计

| 保护 | 说明 |
|---|---|
| **默认只预览** | 必须加 `confirm` 才真删 |
| **只删同链低阶** | 只删「同一条技能链上、且玩家已学更高等级」的 |
| **被动技能不动** | 很多是种族天赋、职业精通，删了会掉属性 |
| **天赋不动** | `GetTalentSpellCost() > 0` 直接跳过 |
| **不在等级链的不动** | `GetSpellRank() == 0` 跳过（唯一等级技能） |
| **防自动补回** | `RemoveSpell(id, false, false)` 第三参数 false，
避免删了又自动学回低阶 |

---

## 安装（4 步）

### 1. 放源文件

```
patches/step12_cs_spellclean.cpp
   ↓ 复制并改名
D:\TrinityCore\src\server\scripts\Commands\cs_spellclean.cpp
```

### 2. 注册脚本

`cs_script_loader.cpp` 加两行：

**声明区**（约 47 行，和其他 `void AddSC_xxx();` 放一起）：
```cpp
void AddSC_spellclean_commandscript();
```

**调用区**（`AddSC_commands()` 函数体内，约 93 行）：
```cpp
    AddSC_spellclean_commandscript();
```

### 3. RBAC 权限

`RBAC.h` 在 `RBAC_PERM_MAX` 之前加：
```cpp
    RBAC_PERM_COMMAND_SPELLCLEAN                             = 71006,
```

然后执行两个 SQL（各一条语句，哪个库执行都行）：
```
sql/14_rbac_spellclean.sql
sql/15_rbac_spellclean_link.sql
```

> **执行完必须重启 worldserver**，否则命令对玩家「不存在」。

### 4. 重跑 CMake + 编译

**新增源文件，必须重跑 CMake**（`file(GLOB)` 列表在生成时固定）。

---

## 测试清单

| # | 操作 | 预期 |
|---|---|---|
| 1 | `.spell` | 显示帮助 |
| 2 | `.spell count` | 统计数字，含「可清理」条数 |
| 3 | `.spell clean` | 列出可清理项，**不删除** |
| 4 | `.spell clean confirm` | 实际清理，报告前后数量 |
| 5 | `.spell count` 再看 | 总数减少，可清理变 0 |
| 6 | 打开技能书 | 低阶技能没了，最高阶还在 |
| 7 | 检查属性面板 | **属性没变**（被动技能没被误删） |
| 8 | `.spell find 火球` | 列出所有含「火球」的技能 |

> **第 7 项最重要** —— 如果属性掉了说明误删了被动技能，立刻告诉我。

---

## 已做的验证

```
g++ -std=c++17 -Wall -Wextra -Wunused-parameter -Wshadow   零错误零警告
```

**API 逐个对照真实源码核实**（上次栽在桩写错上，这次全查了）：

| API | 出处 |
|---|---|
| `PlayerSpellMap` / `GetSpellMap()` | Player.h:183 / 1515 |
| `PlayerSpell{state, active, dependent, disabled}` | Player.h:160 |
| `PLAYERSPELL_REMOVED = 3` | Player.h:156 |
| `RemoveSpell(id, disabled, learn_low_rank)` | Player.h:1453 |
| `GetFirstSpellInChain` / `GetNextSpellInChain` / `GetSpellRank` | SpellMgr.h:594-598 |
| `GetTalentSpellCost(uint32)` | DBCStores.h:40 |
| `SpellInfo::IsPassive()` | SpellInfo.h:425 |
| **`std::array<char const*, 16> SpellName`** | SpellInfo.h:377（这个类型最容易写错） |

---

## 一个提醒

如果你用的是 **casual 档**（`PlayerStart.AllSpells = 1`），新建角色会一次学光所有技能，
`.spell clean` 能清掉的量会非常大（可能上千条）。

**建议**：先 `.spell count` 看看数量，再决定要不要 confirm。
清理是不可逆的（除非重新 `.learn all`）。
