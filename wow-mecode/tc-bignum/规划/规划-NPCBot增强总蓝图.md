# 规划 · NPCBot 增强总蓝图

> 2026-07-31 立项 · **仅规划，不动手**
> **2026-08-21状态纠偏：本文各批次仍主要是计划，不得因文件名或上游能力说明而宣称已经实现。NPCBot已有成熟战斗伙伴基础，但自主冒险、长期记忆、影子身份、背包经济、真实生活、Eluna、大规模和完整指令兼容仍未完成。权威现状见 `04-PBot与NPCBot真实完成度及后续路线_2026-08-21.md`。**
> 主线仍是「世界与副本指令」（step21），本文档是之后的路线图。

---

## 一、核心目标（用户原话）

> 「让他们绝对的真人，别人甚至看不出来的那种」
> 「上限能多大就多大，但是默认就是 39，之后再改」
> 「工程量大不怕，我们一定可以做到的」

**设计原则**：能力上无限制、默认值保守、一切由 conf/指令控制。

---

## 二、已查清的技术底牌

### 好消息（不用改上游就能做的）

| 发现 | 位置 | 意义 |
|---|---|---|
| **菜单文本走本地化表** | `bot_ai.cpp:215` `LocalizedNpcText()` | **700 条菜单纯 SQL 就能全中文**，零 C++ 改动 |
| **闲逛 bot 系统已存在** | `creature_template_npcbot_wander_nodes` 表 + `WanderNode` 类 | 不用开发，**改 conf 即可启用** |
| **公会支持离线加入** | `Guild.cpp:2188 AddMember()` | 只要 `characters` 表有记录就能入会 |
| **好友表只认 GUID** | `character_social` 两列都是 guid | 同上 |
| **13 张 bot 数据表已存在** | `characters_npcbot_*` | 装备/外观/幻化/套装/仓库/统计/日志全都有 |
| **19 个职业** | `botcommon.h` | 10 原版 + 9 魔兽3英雄 |

### 硬限制（要改上游才能突破）

| 限制 | 位置 | 现状 |
|---|---|---|
| **39 人上限强制截断** | `botconfig.cpp:486` | 填再大也被压回 39，且报错 |
| `LvlBrackets` 是 uint8 | `botconfig.h:30` | 最大 255 |
| **背包不存在** | `botcommon.h:508` `BOT_INVENTORY_SIZE=18` | 18 格是纯装备槽，无背包概念 |
| 团队上限 | `MAX_RAID_SIZE = 40` 核心层 | 超过进不了同一个团 |
| Eluna 无 bot 接口 | methods 目录搜 NPCBot = 0 | 得自己写绑定 |

---

## 三、用户已确认的决策

| # | 事项 | 决定 |
|---|---|---|
| 1 | 删 bot 的残留数据 | **同步清理**（characters + 好友 + 公会 + 13 张表） |
| 2 | bot 是否计入公会统计 | **全部计入，要真实** |
| 3 | 数量上限 | **能多大做多大，默认 39，用户可调** |
| 4 | 团队上限 `MAX_RAID_SIZE` | **也提高，能多大多大，用户可控** |
| 5 | 智能 NPC（新世界/新手引导） | **走 Eluna 另一条路**，不混进 bot 系统 |
| 6 | 原版指令对 bot 无效 | **逐个改** |
| 7 | 菜单语言 | **全中文** |
| 8 | 拟真程度 | **绝对真人，别人看不出来** |

---

## 四、执行批次（风险从低到高）

### 批次 0 · 立刻可做（改 conf，零代码）

**闲逛 bot 没出现的原因已找到**：

```
worldserver.conf:4856
NpcBot.WanderingBots.Continents.Count = 0      <- 0 = 禁用
```

用户其余配置都是好的：19 职业全开、4 张大陆地图（0,1,530,571）、
战场也开了（`BG.Enable = 1`）。**只有这一项是 0。**

改成 100 重启即可。注意事项（conf 注释原文）：
- `creature_template` 里要有足够多的未雇佣 bot 模板
- 漫游 bot 会让所在 Grid 保持加载，**吃 CPU 和内存**
- 建议从 50-100 起步，观察负载

---

### 批次 A · 中文化（纯 SQL，零风险）

**427 条文本**（npc_text ID 70001-70700）灌 **`npc_text_locale`** 的 zhCN 行。
（2026-07-31 核实更正：表名不是 `locales_npc_text`；条数是 427 不是 700。
 依据 `ObjectMgr.cpp:6275 FROM npc_text_locale` + 官方手册本地化章节）

技术依据：
```cpp
// bot_ai.cpp:215
NpcTextLocale const* ntl = sObjectMgr->GetNpcTextLocale(textId);
if (loc != DEFAULT_LOCALE && ntl && ...)
    return ntl->Text_0[0][loc];     // 有 zhCN 就用 zhCN
```

分两部分：
- **功能性文本**（菜单/按钮/提示）—— 优先，量小见效快
- **背景故事**（19 个职业介绍，单条几百字）—— 后续

外加 **114 个指令 / ~100 个子命令**的中文别名
（`.bot 招募` = `.npcbot hire`），新建文件不动上游。

---

### 批次 B · 数量上限解放

改 4 处：

| 文件:行 | 改动 |
|---|---|
| `botconfig.h:30` | `LvlBrackets = BotBrackets<uint8>` -> `<uint32>` |
| `botconfig.h:121` | `static uint8 GetMaxNpcBots(uint8)` -> `uint32` |
| `botconfig.cpp:1172` | 同上 |
| `botconfig.cpp:474-490` | `StringTo<uint8>` -> `<uint32>`，**删掉 39 强制截断** |

`botmgr.cpp:890` 靠类型提升自动兼容。

**团队上限** `MAX_RAID_SIZE` 是核心层改动，影响组队界面、
副本人数校验、战场平衡，单独评估。

> 提醒：改完「能招 200 个」≠「200 个能一起打本」，
> 后者受核心团队上限约束。

---

### 批次 C · Eluna 绑定

新建 `LuaBotMethods.h`，约 30 个方法：

```
Bot:GetOwner()      Bot:GetClass()     Bot:GetRole()
Bot:SetRole()       Bot:GetSpec()      Bot:CastSpell()
Bot:Say()           Bot:Follow()       Bot:Attack()
Bot:GetEquip()      Bot:SetEquip()     Bot:GetStats()
Player:GetBots()    Player:HireBot()   Player:DismissBot()
事件：OnBotHire / OnBotDismiss / OnBotKill / OnBotDeath / OnBotLevelUp
```

纯加法，不改上游。

---

### 批次 D · 影子角色（拟真核心）

给每个 bot 在 `characters` 表插一条记录，占真实 GUID。

**换来的能力**：
- 加好友（`character_social` 认 GUID）
- 加公会（`Guild::AddMember` 离线分支）
- `/who` 能查到
- 公会名册、好友列表正常显示
- 邮件系统可用

**配套必做**：
- 删 bot 时同步清理（用户已确认）：characters + character_social
  + guild_member + 13 张 bot 表
- 影子角色不能被真人登录（account 指向系统账号）
- `.bot 清理孤儿` 指令，扫描并修复残留

---

### 批次 E · 拟真社交（"看不出是 bot"）

| 功能 | 说明 |
|---|---|
| **上下线播报** | "XXX 进入了游戏"，按真实作息随机 |
| **主动交友** | 大世界 bot 遇到玩家有几率发好友申请 |
| **战斗喊话** | 嘲讽/治疗告急/BOSS技能预警/残血求救 |
| **闲聊系统** | 组队时互相搭话、评论环境、抱怨装备 |
| **公会活跃** | 公会频道聊天、上线打招呼、下线道别 |
| **作息模拟** | 不同 bot 有不同活跃时段，不是 24 小时在线 |
| **打字延迟** | 回复消息前有 1-3 秒"打字"延迟 |
| **情绪状态** | 连续死亡会"沮丧"，打出高伤害会"兴奋" |
| **社交记忆** | 记住和谁组过队、谁救过自己，态度不同 |
| **称号/成就** | 影子角色可挂称号 |

---

### 批次 F · 背包与银行（新做）

现状：`BOT_INVENTORY_SIZE = 18` 是纯装备槽，**没有背包**。

新建：
- `bot_bag`（16-100 格可配）
- `bot_bank`（28-200 格可配）
- Gossip 界面：查看/存取/整理/自动分类
- 支持 bot 自己拾取战利品放进背包
- 玩家可从 bot 背包取物品

---

### 批次 G · 指令兼容（逐个改）

**原因**：大量指令写死
```cpp
Player* target = handler->getSelectedPlayerOrSelf();   // 选中 Creature 返回 nullptr
```

**做法**：新增辅助函数
```cpp
Unit* GetTargetPlayerOrBot(ChatHandler* handler);
```
让指令同时接受玩家和 bot。**每个指令都要改一遍**，工作量取决于要覆盖多少。

优先级：
1. 我们自制的（`.set` / `.bar` / `.combo` / `.gear` / `.setup`）
2. 常用原版（`.modify` / `.additem` / `.learn` / `.tele`）
3. 其余按需

---

### 批次 H · 自定义 bot 创建

不依赖现有模板：
- `.bot 创建 <名字> <种族> <职业> <外观>`
- entry 段规划：**950000-959999 给自造 bot**
- 支持自定义装备、技能组、AI 档位
- AI 档位：菜鸟/普通/精英/大师（影响反应速度、技能使用率、走位）

---

### 批次 I · 智能 NPC（独立路线）

**不用 bot 系统做** —— bot 是"雇佣战斗单位"，
而新世界 NPC 是"有对话有剧情的世界居民"，需求不同。

技术路线：**Eluna Lua + Gossip + SmartAI**，更轻更好改。

- 新世界真人感 NPC（会走动、会聊天、有日程）
- 旧世界新手引导 NPC（教学、答疑、给新手装备）

---

## 五、还能加什么（新想法）

| 想法 | 说明 |
|---|---|
| **bot 图鉴** | 一览所有 bot 的属性/装备/战绩/好感度 |
| **战绩排行** | DPS/治疗/承伤/死亡次数榜 |
| **bot professions** | 让 bot 会采集和制造，能给你打材料 |
| **bot 拍卖行** | bot 会挂单和买东西，让经济系统活起来 |
| **bot 组队招募** | bot 会在频道发"求组"，玩家可应征 |
| **师徒系统** | 高级 bot 带新玩家，给指导和奖励 |
| **bot 阵营战** | 敌对阵营 bot 会主动来打你 |
| **动态难度** | bot 强度随玩家表现自动调整 |
| **录像回放** | 记录 bot 战斗过程，用于调 AI |
| **AI 学习** | 记录哪套循环 DPS 高，自动优化 |

---

## 六、依赖关系

```
批次0 (conf)          <- 独立，随时可做
批次A (中文化)         <- 独立
批次B (上限)          <- 独立
批次C (Eluna)         <- 独立
批次D (影子角色)       <- 批次 E 的前提
批次E (拟真社交)       <- 依赖 D
批次F (背包银行)       <- 独立
批次G (指令兼容)       <- 独立
批次H (自定义bot)      <- 建议在 B 之后
批次I (智能NPC)        <- 完全独立，可并行
```

**建议顺序**：0 -> A -> B -> C -> D -> E -> F -> G -> H -> I

---

## 七、风险登记

| 风险 | 说明 | 对策 |
|---|---|---|
| **上游 merge 冲突** | 批次 B/F/G 要改 NPCBot 源码 | 改动集中、注释标记、留 patch 记录 |
| **性能** | 每个 bot 是完整 Creature+AI | 分批测试，先 50 再 100 |
| **影子角色脏数据** | 删 bot 没清干净 | 配套清理指令 + 启动时自检 |
| **团队上限** | 超 39 进不了同一个团 | 要动核心层，单独评估 |
| **Grid 常驻** | 漫游 bot 保持地图加载 | conf 控制数量 |

---

## 八、当前状态

**主线未变**：step21 世界与副本指令。
本蓝图待主线完成后按批次推进。
