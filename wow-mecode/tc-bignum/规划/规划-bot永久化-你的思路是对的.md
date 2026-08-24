# bot 永久化 —— 你的思路是对的，而且就是官方机制本身

> 用户提出：
> 「可不可以让自己添加的游荡npc可以同时拥有官方的招募npc表和游荡npc表？
> 这样可以固定吗？毕竟用指令召唤的npc都是可以永久的」
>
> 「关键是可以同时对上百上千个bot生效吗」

---

## 一、结论：**可行，而且比我之前设想的简单得多**

**你说对了一件我没说清楚的事**：

游荡bot 和 `.npcbot spawn` 出来的永久bot，**用的是【同一批模板】**。
区别不在模板，而在**这个 entry 有没有进 `characters_npcbot` 表**。

**而且不需要"复制到新entry段"** —— 直接让你的 bot 模板走
`.npcbot spawn` 那条路就行。

---

## 二、实证：游荡池怎么挑 bot

`botdatamgr.cpp:316-326`（服务器启动时）：

```cpp
for (auto const& [id, extras] : _botsExtras)          // 遍历 creature_template_npcbot_extras
{
    uint8 c = extras.bclass;
    if (c != BOT_CLASS_NONE && BotCfg::IsWanderingClassEnabled(c))
    {
        ++enabledBotsCount;
        if (!_botsData.contains(id))                  // <-- 【关键这一行】
        {
            ASSERT(_spareBotIdsPerClassMap.contains(c));
            _spareBotIdsPerClassMap.at(c).insert(id);  // 放进游荡池
        }
    }
}
```

**`_botsData` 就是 `characters_npcbot` 表的内存镜像。**

所以规则是：

```
在 creature_template_npcbot_extras 里  +  【不在】 characters_npcbot 里
    -> 进游荡池，每次重启随机洗牌

在 creature_template_npcbot_extras 里  +  【在】 characters_npcbot 里
    -> 【不进游荡池】，是永久bot
```

**这正是你说的"同时拥有两张表"** —— 只要往 `characters_npcbot` 写一条，
它就自动退出游荡池，变成永久的。

---

## 三、那我之前说的"必须复制到新entry"是怎么回事

**我说错了一半。** 分清两种情况：

| 情况 | entry 是什么 | 能否持久化 |
|---|---|---|
| **你库里的 bot 模板**（如 70001-70500） | `creature_template` 表里的**真实记录** | **能** |
| **运行时生成的游荡实例**（如 80770） | `_botsExtraCreatureTemplates` **内存临时对象** | **不能** |

`botdatamgr.cpp:379-383`：

```cpp
while (all_templates.contains(++next_bot_id));         // 找一个没用过的id，如 80770

const auto [bot_class, orig_entry] = spareBotPair;     // orig_entry = 你库里的真实模板
CreatureTemplate const* orig_template = ASSERT_NOTNULL(sObjectMgr->GetCreatureTemplate(orig_entry));
CreatureTemplate& bot_template = _botsExtraCreatureTemplates[next_bot_id];
bot_template = *orig_template;                          // 复制
bot_template.KillCredit[0] = orig_entry;                // 记住原始entry
```

**你在游戏里看到的那个 `Llane (80770)` 是【临时副本】**，
它的"本体"是你库里某个真实 entry（比如 70123）。

**`.pin` 错在试图固定那个临时副本 80770。
正确做法是固定它的本体 70123。**

---

## 四、批量能不能对上百上千个 bot 生效

**能，而且纯 SQL 就够，不需要写代码。**

### 4.1 核心就一句 INSERT

```sql
INSERT INTO `characters`.`characters_npcbot` (`entry`, `owner`, `roles`, `spec`, `faction`)
SELECT
    e.`entry`,
    0,                          -- owner=0 表示无主（不是被玩家招募）
    <默认roles>,
    <默认spec>,
    <阵营>
FROM `world`.`creature_template_npcbot_extras` e
WHERE e.`entry` BETWEEN 70001 AND 70500;
```

**一次几百上千条都没问题**，MySQL 批量插入很快。

### 4.2 但还要在 `creature` 表 spawn 它们

光写 `characters_npcbot` 只是"退出游荡池"，
bot 还需要一个**实际站立的位置**（`world.creature` 记录）。

```sql
INSERT INTO `world`.`creature`
  (`guid`,`id`,`map`,`spawnMask`,`phaseMask`,`modelid`,`equipment_id`,
   `position_x`,`position_y`,`position_z`,`orientation`,
   `spawntimesecs`,`wander_distance`,`currentwaypoint`,
   `curhealth`,`curmana`,`MovementType`,`npcflag`,`unit_flags`,`dynamicflags`)
VALUES ...
```

字段清单实查自 `WorldDatabase.cpp:86` 的 `WORLD_INS_CREATURE`。

### 4.3 两张表必须同时写（硬约束）

`botdatamgr.cpp:1170-1176` 启动时交叉校验：

```cpp
for (auto const& [_, cdata] : sObjectMgr->GetAllCreatureData())
    if (cdata.id >= BOT_ENTRY_BEGIN && ...IsNPCBot() && 找不到对应characters_npcbot记录)
        invalid_ids.push_back(cdata.id);

if (!invalid_ids.empty())
    report_inavlid_ids("Invalid NPCBot spawns found in `creature` table having no data in `characters_npcbot` table!");
    // -> ABORT_MSG 崩服
```

**creature 表有、characters_npcbot 没有 -> 崩服。**
反过来（characters_npcbot 有、creature 没有）不崩，只是 bot 不出现。

---

## 五、三种方案对比（你选）

### 方案A：纯 SQL 批量（最快，推荐先试）

```
优点：不用编译，一条 SQL 搞定几百个
缺点：位置要自己指定（可以按坐标网格自动铺开）
适合：「我想要 200 个 bot 固定在主城/某个区域」
```

### 方案B：指令批量（`.pin range 70001 70100`）

```
优点：在游戏里操作，能看到效果
缺点：要写代码 + 编译
适合：一个个挑着固定
```

### 方案C：改配置让游荡bot"不洗牌"（如果只是想让它们别变）

```
优点：改个配置就行
缺点：位置还是随机的，而且要确认核心支持
待查：botconfig 里有没有"固定随机种子"的选项
```

---

## 六、方案A 的位置怎么定（批量的核心难点）

上千个 bot 不能都堆在一个点。三个办法：

### 6.1 沿用游荡bot的路点表（推荐）

`creature_template_npcbot_wander_nodes` 里已经有全地图的合法坐标点，
**直接从里面挑**：

```sql
-- 给每个 bot 分配一个路点作为固定位置
SELECT
    e.`entry`,
    w.`mapid`, w.`x`, w.`y`, w.`z`, w.`o`
FROM `world`.`creature_template_npcbot_extras` e
JOIN (SELECT @r := 0) r
JOIN `world`.`creature_template_npcbot_wander_nodes` w
  ON w.`id` = (e.`entry` % (SELECT COUNT(*) FROM `world`.`creature_template_npcbot_wander_nodes`)) + 1
WHERE e.`entry` BETWEEN 70001 AND 70500;
```

**好处**：这些点本来就是给 bot 用的，保证不卡地形、不在水里。

### 6.2 围绕某个中心点铺开

```
以主城为中心，按螺旋/网格算法生成 N 个坐标
```

### 6.3 手动指定几个"驻地"

```
比如：暴风城 50 个、奥格瑞玛 50 个、达拉然 100 个
```

---

## 七、我建议的路线

```
第1步  先用 SQL 试 10 个        <- 验证机制通不通，风险最小
       重启 -> 看它们在不在、有没有崩服
              |
              v
第2步  确认可行后，批量 200-500  <- 用路点表分配位置
              |
              v
第3步  如果需要，再做指令封装     <- .pin range / .pin batch
```

**第1步我可以现在就给你 SQL。**

---

## 八、需要你确认的三件事

1. **你的 bot 模板 entry 范围是多少？**
   （比如 70001-70500？用 `.pin status` 或查 `creature_template_npcbot_extras`）

2. **想固定多少个？固定在哪？**
   - 全部固定 / 固定一部分
   - 用路点表随机分布 / 集中在某几个城市

3. **固定后还要不要保留一部分游荡bot？**
   （建议保留，不然世界太静态）

---

## 九、待实查（写 SQL 前要确认）

| # | 查什么 | 为什么 |
|---|---|---|
| 1 | `characters_npcbot` 的 `roles`/`spec` 默认值怎么算 | 代码里是 `DefaultRolesForClass()` / `SelectSpecForClass()`，SQL 里要复现 |
| 2 | `faction` 怎么取 | `GetDefaultFactionForBotRaceClass()` 依赖种族 |
| 3 | `creature.guid` 从哪开始分配 | 不能和现有 spawn 冲突 |
| 4 | `curhealth`/`curmana` 填什么 | 可能填 1 让核心自己算 |
| 5 | 固定后的 bot 会不会自己走动 | `MovementType` 字段 |

**这些我在写 SQL 之前会逐条查清楚，不猜。**
