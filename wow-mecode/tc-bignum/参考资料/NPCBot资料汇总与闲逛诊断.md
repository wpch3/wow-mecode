# NPCBot 官方资料汇总 + 闲逛 bot 诊断

> 来源：https://github.com/trickerer/Trinity-Bots
> 官方手册已存档到 `参考资料/NPCBot官方手册.md`（1075 行）
> 采集日期：2026-07-31，模组版本 v5.4.653a（TC 端最后更新 2026-07-25）

---

## 一、先回答你的问题：街上没有闲逛 bot

### 结论：**几乎肯定就是 `Count = 0`，不是你装漏了东西**

我把整条链路查完了，判断依据如下。

### 链路（`botmgr.cpp:69` BotMgr::Initialize）

```cpp
BotDataMgr::LoadNpcBots();
BotDataMgr::LoadWanderMap();          // <- 加载路径点
BotDataMgr::GenerateWanderingBots();  // <- 生成闲逛bot
```

### 三道关卡，逐一排查

**关卡 1：路径点表有没有数据**

```cpp
// botdatamgr.cpp:1520-1525
QueryResult wres = WorldDatabase.Query("SELECT ... FROM creature_template_npcbot_wander_nodes ...");
if (!wres)
{
    BOT_LOG_FATAL("server.loading", "Failed to load wander points: table ... is empty!");
    ASSERT(false);        // <- 【直接崩服】
}
```

**这是 `ASSERT(false)`，表空会崩服而不是静默失败。**
你的服务器能正常启动 → **这张表一定有数据** → 关卡 1 通过。

顺带确认：`sql/base/world_npcbots.sql` 里预置了 **5098 个路径点**，
覆盖 4 张大陆地图。数据是齐的。

**关卡 2：Continents.Maps 合法性**

```cpp
// botconfig.cpp:587-592
if (_enabled_wander_node_maps.empty())
{
    BOT_LOG_ERROR(... "does not provide any valid maps! Wandering bots will not be spawned!");
    _desiredWanderingBotsCount = 0;    // <- 静默归零
}
```

你的 conf 第 4874 行：`Continents.Maps = 0,1,530,571`
（东部王国 / 卡利姆多 / 外域 / 诺森德）—— **全部合法**，关卡 2 通过。

**关卡 3：Count**

```cpp
// botdatamgr.cpp:1869-1872
const uint32 wandering_bots_desired = BotCfg::GetDesiredWanderingBotsCount();
if (wandering_bots_desired == 0)
    return;                            // <- 直接不生成，无任何提示
```

你的 conf 第 4856 行：**`NpcBot.WanderingBots.Continents.Count = 0`**

**这就是原因。** 它是唯一一个「静默什么都不做」的分支。

### 怎么确认

重启后看启动日志，正常应该有：

```
Setting up wander map...
>> Loaded 5098 bot wander nodes (0 disabled) on 4 maps ...
Spawning wandering bots...
>> Set up spawning of N wandering bots in XX ms
```

- **有前两行、没有后两行** → 确认是 Count=0
- 完全没有 → 模组没装好（但那样启动就崩了）

### 自查 SQL（保险起见）

```sql
-- 路径点数量，应该是 5098 左右
SELECT COUNT(*) FROM world.creature_template_npcbot_wander_nodes;

-- 按地图分布
SELECT mapid, COUNT(*) FROM world.creature_template_npcbot_wander_nodes GROUP BY mapid;
-- 期望看到 0 / 1 / 530 / 571 四张图

-- bot 模板数量，应该 370
SELECT COUNT(*) FROM world.creature_template WHERE entry BETWEEN 70000 AND 70999;
```

如果第一条查出来是 0，说明 `world_npcbots.sql` 没导入 —— 但那样服务器起不来。

---

## 二、改法（含防崩服警告）

`worldserver.conf` 第 **4856** 行：

```
NpcBot.WanderingBots.Continents.Count = 0     ->     = 50
```

**改完重启**（不是 reload）。

### 填太大会崩服

```cpp
// botdatamgr.cpp:1878-1886
uint32 maxbots = sBotGen->GetSpareBotsCount();
if (maxbots < wandering_bots_desired)
{
    BOT_LOG_FATAL(..., "Desired amount of wandering bots ({}) cannot be created. Aborting!");
    ASSERT(false);        // <- 崩
}
```

**可用 bot 数 = 370 个预置模板 - 已被玩家雇佣的**。

| 值 | 评价 |
|---|---|
| **50** | 建议起步值 |
| 100 | 比较热闹 |
| 200 | 仍有余量 |
| 300+ | 接近上限，雇了很多 bot 就有风险 |

---

## 三、官方手册要点摘录

### 安装步骤（对照检查你有没有漏）

官方 TrinityCore 安装流程：

1. 复制 `NPCBots.patch` 到 TrinityCore 目录
2. `patch -p1 < NPCBots.patch`（注意：**`git apply` 可能不работа**）
3. 重跑 CMake + 重新编译
4. **把 `worldserver.conf.dist` 里的 NPCBot 配置合并进你的 `worldserver.conf`**
5. 若 `Updates.AutoSetup = 0` 或库已建好，手动导入 `sql/base/` 三个文件：
   - `auth_npcbot.sql`
   - `characters_npcbot.sql`
   - `world_npcbot.sql`
6. 若 `Updates.EnableDatabases = 0`，手动逐个导入 `sql/custom/` 下的更新
7. 启动

> 你用的是**预打补丁仓库**（328950225/TrinityCore-NPCBOT-Eluna-zhCN），
> 所以 1-3 步已经由仓库作者做完了，只需要关心 **4/5/6**。

**第 4 步最容易漏** —— 如果你的 `worldserver.conf` 是从旧版本继承的，
可能缺少新增的 NPCBot 配置项。不过你的 conf 有 326 处 NpcBot 配置，
看起来是完整的。

### 本地化：我之前记错了表名

**手册原文**：

> All localizable string are contained in `npc_text` table.
> If you want to make a translation you'll have to populate
> `npc_text_locale` table accordingly (`Text0_0` field)

**源码验证**（`ObjectMgr.cpp:6275`）：

```cpp
QueryResult result = WorldDatabase.Query("SELECT ID, Locale, "
    "Text0_0, Text0_1, ... FROM npc_text_locale");
```

**正确表名是 `npc_text_locale`**，不是我台账里写的 `locales_npc_text`
（那是很老的 TC 版本用的名字）。**已更正。**

调用链：

```
bot_ai.cpp:215   bot_ai::LocalizedNpcText(Player const* forPlayer, uint32 textId)
bot_ai.cpp:222       sObjectMgr->GetNpcTextLocale(textId)
ObjectMgr.cpp:6275   FROM npc_text_locale
```

**文本数据**：`sql/custom/world/npcbot_2000_00_00_00_npc_text.sql`
共 **427 条**，ID 范围 **70001 - 70700**。

中文化只需要往 `npc_text_locale` 插 427 条 `Locale='zhCN'` 的记录，
**纯 SQL，不用改代码**。这个结论仍然成立。

### 闲逛系统官方说明（手册原文）

> 1. Wandering bots in open world. Config setting `NpcBot.WanderingBots.Continents.Count`
>    controls desired amount of bots roaming world maps. Spawn points are random and
>    level is selected accordingly. These bots give small reward for kill and bonus experience.
> 2. Wandering bots generated for Battlegrounds. Enabled by `NpcBot.WanderingBots.BG.Enable`

要点：

- 闲逛 bot **不能被雇佣**，是自主单位
- 出生点**随机**，等级按区域自动选
- 杀死它们给**少量奖励和额外经验**（受 `KillReward.*` 控制，你现在全是 0）
- BG 填充是**独立开关**（你的 `BG.Enable = 1` 已开）

### 卸载时要清的表（做魔改前值得知道）

手册列出的 NPCBot 专有表：

```
characters_npcbot
characters_npcbot_group_member
characters_npcbot_transmog
characters_npcbot_gear_storage
creature_template_npcbot_extras
creature_template_npcbot_appearance
creature_template_npcbot_wander_nodes
```

外加 entry **70000-71000** 段的 `creature_template` 记录。

### 官方插件（可能对"像真人"有帮助）

| 插件 | 作者 | 用途 |
|---|---|---|
| [NetherBot](https://github.com/NetherstormX/NetherBot) | NetherstormX | 通用命令封装 |
| [NBEM](https://github.com/Torrozin/mod-nbem-scan-and-npcboteqmanager) | Torrozin | 装备管理 |
| [Lleguito 的一批](https://github.com/LleguitoWoW?tab=repositories) | Lleguito | 手册标注 "vibe coded"（慎用）|

### 其他

- **官方视频教程**：https://www.youtube.com/watch?v=fByzoyl3rCY （by QT Blue-AI）
- **Issues 区**：https://github.com/trickerer/Trinity-Bots/issues （提 bug / 问问题）
- **更新频率**：每周六 05:00 UTC
- 手册版本 0.25（2023-06-14），部分内容可能滞后于代码

### 额外职业（魔兽3英雄）官方定位

手册明确说明：

> Their main purpose is to support you and other bots.
> **They are not intended to be as effective as normal classes and/or balanced at any given level.**

即：9 个魔兽3职业**官方就没打算做平衡**。你如果要"像真人"，
可能需要自己调，或者在闲逛 bot 里限制它们出现。

其中 **剑圣（Blademaster）在最新版本是禁用状态**
（手册标注 *"Disabled in last version"*），
对应你 conf 里 `Classes.Blademaster.Enable` 的默认值是 `false`。

---

## 四、对既有台账的更正

| 台账原记录 | 更正 |
|---|---|
| 菜单本地化查 `locales_npc_text` | **错**，正确是 `npc_text_locale`（ObjectMgr.cpp:6275）|
| 700 条（70001-70700）| ID 范围对，但**实际是 427 条**，不是 700 条 |
| 「闲逛bot已存在但没开」 | 正确，且已确认唯一原因是 `Count=0`（唯一静默分支）|

---

## 五、下一步做 bot 魔改时的已知信息

| 项 | 值 | 来源 |
|---|---|---|
| 预置 bot 模板 | 370 个，entry 70000-70595 | `npcbot_2000_00_00_00_creature_template.sql` |
| 路径点 | 5098 个，4 张大陆图 | `world_npcbots.sql` |
| 可本地化文本 | 427 条，ID 70001-70700 | `npcbot_2000_00_00_00_npc_text.sql` |
| 本地化目标表 | `npc_text_locale` | `ObjectMgr.cpp:6275` |
| 39 人上限真凶 | `botconfig.cpp:486` 硬编码截断 | 之前已查 |
| bot 是 Creature 不是 Player | `bot_ai : public CreatureAI` | `bot_ai.h:51` |
| 闲逛 bot 崩服点 | `botdatamgr.cpp:1885` ASSERT | 本次查证 |
| 路径点表崩服点 | `botdatamgr.cpp:1524` ASSERT | 本次查证 |
