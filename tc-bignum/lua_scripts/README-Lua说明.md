# Lua 脚本说明

## ⚠️ 最重要的一条规则

**文件名不能以数字开头。**

Eluna 会把文件名开头的数字当成 mapId（`ElunaLoader.cpp:225` 的 `std::from_chars`）：

| 文件名 | 解析成 | 后果 |
|---|---|---|
| `01_welcome.lua` | mapId **1** | 只在卡利姆多加载 |
| `02_announce.lua` | mapId **2** | **map 2 不存在 → 永不加载** |
| `custom_welcome.lua` | -1 | 全地图加载 ✓ |

而且**执行失败是静默的**（`LuaEngine.cpp:250` 只在 DEBUG 级别记日志），
所以脚本没生效时你看不到任何报错。

**判断方法**：看启动日志的 `Executed N Lua scripts`，
如果每张地图的 N 不一样，说明有脚本被 mapId 过滤了。

---

## 当前脚本

| 文件 | 功能 | 命令 |
|---|---|---|
| `custom_welcome.lua` | 登录欢迎 + 指令导览 | `.help2` |
| `custom_announce.lua` | 公告轮播（8条，10分钟一轮） | 自动 |
| `custom_daily_reward.lua` | 每日登录奖励（连续签到递增） | 自动 |
| `custom_teleport.lua` | **传送系统** | `.tp` |
| `custom_diag.lua` | 环境诊断（确认没问题后可删） | 自动 |
| `bignum_selftest.lua` | 大数值自检 | `.bigtest` |

---

## 传送系统 `custom_teleport.lua`

### 用法

```
.tp              打开分类菜单（可点击）
.tp 暴风         中文搜索，多结果弹菜单
.tp 雷霆崖       唯一结果时【直接传送】，不用再点
.tp stormwind    英文也能搜
```

### 需要先执行两个 SQL

```
sql/20_tele_cn.sql        建中文名映射表
sql/21_tele_cn_data.sql   导入 112 个常用地点的中文名
```

**为什么要这张表**：`game_tele` 表**只有一列 name，是英文，官方没有本地化表**
（`cs_lookup.cpp:1141` 的 `.lookup tele` 也是直接搜那一列）。
所以要自己做中英映射。

导入方式是**按英文名模糊匹配**写入，不写死 id ——
不管你的 `game_tele` 表 id 怎么编排都能对上。

执行后核对：
```sql
SELECT COUNT(*) FROM world.custom_tele_cn
```

### 显示效果

```
暴风城  Stormwind          ← 有中文的：中文为主，英文灰色小字
Stormwindjail              ← 没中文的：显示英文
```

想加中文名，往 `custom_tele_cn` 表里加一行就行。

### 数据来源

用你数据库里现成的 **`game_tele` 表**（TrinityCore 自带 1000+ 个点），
LEFT JOIN 中文表显示。

### ⚠️ Eluna Gossip 回调的两个大坑（都踩过了）

#### 坑1：回调签名和文档不一致

文档（`GlobalMethods.h:996`）写的是 7 个参数：
```
(event, player, object, sender, intid, code, menu_id)
```

但看源码 `hooks/GossipHooks.cpp:81-88` 的**实际推参**只有 6 个：
```cpp
HookPush(pPlayer);   // 参数2 = player
HookPush(pPlayer);   // 参数3 = 又是 player（源码注释：just not to mess up the amount of args）
HookPush(sender);    // 参数4 = sender
HookPush(action);    // 参数5 = intid
HookPush(code);      // 参数6 = code
                     // 没有第7个，menu_id 根本不推！
```

**结论**：
- 第 3 个参数是 **object 不是 menuId**
- **`menu_id` 永远是 nil**，不能用它做过滤
- 只能用 **sender 区间**判断是不是自己的菜单

正确写法：
```lua
local function OnGossip(event, player, object, sender, intid, code)
    if sender ~= SENDER_CAT and sender ~= SENDER_TELE and sender ~= SENDER_NAV then
        return
    end
    ...
end
```

#### 坑2：Gossip sender 冲突

套装系统 `cs_gearset.cpp` 的 `OnGossipSelect` **只按 sender 过滤（1~11），
不检查 menuId**。所以任何 Lua 脚本用 sender 1~11 都会被它抢走。

本脚本的 sender 用 **9101 / 9102 / 9109**，避开这个区间。

**以后写新的 Gossip 脚本，sender 一律从 9000 起。**

### 分类

| 分类 | mapId |
|---|---|
| 东部王国 | 0 |
| 卡利姆多 | 1 |
| 外域 | 530 |
| 诺森德 | 571 |
| 副本与团本 | 其余全部 |

改分类只要编辑文件里的 `CATEGORIES` 表。

### 分页

Gossip 有 **32 条硬上限**（`GossipDef.cpp:42` 有 `ASSERT`，超了崩服）。

设计成 **28 内容 + 最多 4 导航 = 32**，正好卡住。
传送点再多也不会超限。

### 没做的功能

`.tp home` 和 `.tp back` **只给提示，不自己实现**。

原因：这版 Eluna **没有读取炉石点的 API**（只有 `SetBindPoint` 能写不能读）。
所以引导你用原版的 `.recall` 和炉石。

---

## 每日奖励要建表

```
sql/13_daily_reward.sql
```

一条语句，哪个库执行都行（已写完全限定名）。

---

## 安装

1. 把 `custom_*.lua` 复制到 `D:\TC-Build\bin\RelWithDebInfo\lua_scripts\`
2. **删掉旧的 `00_diag.lua` / `01_welcome.lua` / `02_announce.lua` / `03_daily_reward.lua`**
3. 游戏内 `.reload eluna`

### 预期日志

每张地图都会打印一次（Eluna 是多状态的，每张地图一个独立 Lua state）：

```
[Eluna] custom_welcome.lua 已加载 -- 登录欢迎 + .help2 命令导览
[Eluna] custom_announce.lua 已加载 -- 共 8 条公告...
[Eluna] custom_daily_reward.lua 已加载 -- 每日登录奖励
[Eluna] custom_teleport.lua 已加载 -- 输入 .tp 打开传送菜单
```

**关键**：`Executed N Lua scripts` 每张地图应该是**同一个数字**。
如果不一样，说明还有脚本被 mapId 过滤了。

---

## 已做的验证

### API 逐个对照 Eluna 真实源码

| API | 出处 |
|---|---|
| `player:GetGUIDLow` | ObjectMethods.h:183 |
| `player:Teleport` | PlayerMethods.h:3138 |
| `player:GossipMenuAddItem` | PlayerMethods.h:3459 |
| `player:GossipComplete` | PlayerMethods.h:3478 |
| `player:GossipSendMenu` | PlayerMethods.h:3498 |
| `player:GossipClearMenu` | PlayerMethods.h:3520 |
| `player:SendBroadcastMessage` | PlayerMethods.h:3290 |
| `query:GetFloat/GetString/GetUInt32/NextRow` | ElunaQueryMethods.h:205/233/121/251 |
| `RegisterPlayerEvent` | GlobalMethods.h:726 |
| `RegisterPlayerGossipEvent` | GlobalMethods.h:1009 |
| `WorldDBQuery` | GlobalMethods.h:1248 |
| 事件 3=登录 4=登出 42=命令 | hooks/Hooks.h:142/143/181 |
| Gossip 事件 2=on_select | hooks/Hooks.h:431 |

> **踩过的坑**：`GetBindMapId` / `GetBindX` 这些**不存在**，
> 我一开始用了，核实时发现并移除。

### 真 Lua 解释器执行测试

7 个场景全过：
- 加载 + 事件注册
- `.tp` 打开菜单（7 项）
- `.tp 暴风` 多结果弹菜单
- `.tp 雷霆崖` **唯一结果直接传送**
- 点分类正确过滤（东部王国 3 个 / 副本 1 个）
- 搜索无结果的提示
- **无关命令 `.gearset` 不被拦截**（返回 nil 交给 C++ 处理）

### 沙箱兼容

模拟 Eluna 沙箱（**把 os 库整个拿掉**）测试，全部脚本正常加载。
`custom_daily_reward.lua` 已改用 MySQL 的 `CURDATE()`/`DATEDIFF()`，不依赖 Lua os 库。
