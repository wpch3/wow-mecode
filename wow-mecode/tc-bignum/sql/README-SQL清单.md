# SQL 文件清单与执行状态

> 编号只是顺序号，**不代表要一次全执行**。做一个功能配一套 SQL。
> 最后更新：2026-07-30

---

## 一屏总表

| 编号 | 属于哪个功能 | 状态 |
|---|---|---|
| 00 ~ 03 | 大数值改造（扩列/耐久/测试物品） | ✅ 阶段一已执行 |
| 04 ~ 12 | 套装系统 v2/v3 + RBAC 71005 | ✅ 已执行 |
| 13 | 每日登录奖励（Lua 用表） | ✅ 已执行 |
| **14 ~ 19** | `.spell clean` / `.reloaditem` / `.gmhelp` 的 RBAC | ✅ **已执行**（你说三个指令都能用） |
| 20 ~ 22 | 传送系统中文名表 | ✅ 已执行（v2 精确匹配版） |
| **23 ~ 26** | **幻化**：2 张表 + RBAC 71009 | ✅ 已执行（幻化在用） |
| **27 ~ 28** | **装备锻造** RBAC 71010 | ✅ 已执行（`.item` 能用） |
| **29** | **修耐久溢出** | ⬜ **只在耐久真的异常时才需要** |

---

## 怎么判断某个 SQL 跑没跑过

看功能能不能用最直接：

| 能用的功能 | 说明对应 SQL 已执行 |
|---|---|
| `.spell clean` / `.reloaditem` / `.gmhelp` | 14 ~ 19 ✅ |
| `.transmog` 全套 | 23 ~ 26 ✅ |
| `.item clone` / `.item type` 等 | 27 ~ 28 ✅ |

**RBAC 类 SQL 没跑的话，指令会直接「不存在」**（`ChatCommand.cpp:477` 的
`IsInvokerVisible` 会把没权限的命令对你隐藏），不会是「能用但有问题」。

所以：**指令能用 = 它的 SQL 一定跑过了。**

---

## 29 号要不要跑

`29_fix_forge_durability.sql` 是修「克隆装备耐久溢出」的。

**先查一下再决定**：

```sql
SELECT entry, name, MaxDurability FROM world.item_template
WHERE entry BETWEEN 800000 AND 899999;
```

- `MaxDurability` 是正常值（如 100、120）或 0 → **不用跑**
- 出现 65535 以上的怪值，或明明该有耐久却是 0 → **跑一下**

游戏内也能查：`.item info 800000` 或 `.item why <法术ID>`（会显示耐久）。

---

## 暴风城传到监狱的 bug

### 原因

21 号 v1 用了**模糊匹配**：

```sql
WHERE LOWER(REPLACE(name,' ','')) LIKE '%stormwind%'
```

`game_tele` 表里含 stormwind 的记录不止一条：

| 英文名 | 实际是 | v1 标成了 |
|---|---|---|
| `Stormwind` | 暴风城 | 暴风城 ✓ |
| **`StormwindJail`** | **暴风城监狱（副本）** | **暴风城** ✗ |
| `StormwindVault` | 暴风城地窖 | 暴风城 ✗ |

三条都叫"暴风城"，排序时挑中了监狱那条 → 传到副本出口，卡在栅栏门里。

### 修复

v2 改成**精确匹配**：

```sql
WHERE LOWER(REPLACE(name,' ','')) = 'stormwind'      -- 完全相等
```

并且监狱单独一条：
```sql
WHERE LOWER(REPLACE(name,' ','')) = 'stormwindjail'  -- '暴风城监狱'，归到副本分类
```

现在 110 条映射**全部是精确匹配**，不会再串。

### 你被卡住了怎么脱困

游戏内输入任意一个：

```
.unstuck              官方自救命令，传回墓地
.tele stormwind       原版传送命令（用英文名）
.recall               回到传送前的位置
```

`.recall` 最直接 —— 我的传送脚本调用的 `player:Teleport()` 内部会
`SaveRecallPosition()`，所以传送前的位置是存着的。

---

## 执行顺序（现在做）

```
第1步  22_tele_cn_clear.sql    清空
第2步  21_tele_cn_data.sql     重新导入 v2
第3步  游戏内 .reload eluna
第4步  .tp 暴风城  验证传送正确
```

核对导入结果：
```sql
SELECT COUNT(*) FROM world.custom_tele_cn
```

v2 是精确匹配，条数会比 v1 少一些（v1 有误匹配的重复项），
大概 80-110 条，取决于你的 `game_tele` 表里有多少标准命名的点。

看看有没有重名（正常应该返回 0 行）：
```sql
SELECT name_cn, COUNT(*) FROM world.custom_tele_cn GROUP BY name_cn HAVING COUNT(*) > 1
```
