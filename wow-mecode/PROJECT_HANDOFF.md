# PROJECT_HANDOFF.md · 魔兽世界 3.3.5a 客户端与 TrinityCore 服务端魔改项目交接

> 更新日期：2026-08-24（Asia/Shanghai）
> 当前工作仓库：`https://github.com/wpch3/wow-mecode`
> 当前本地分支/基线：`main` / `d1ab4bed387dcc2fdbaaea05add9ddd94cbe8769`
> 上游参考仓库：`https://github.com/328950225/TrinityCore-NPCBOT-Eluna-zhCN`
> 上游参考分支/取证提交：`NPCBOT-Eluna-zhCN-2026` / `4e8762ee2b00948fa103d0cd1afd78ccdf4364fb`
> 注意：本次成果尚未提交或推送，须以当前工作树和 `HANDOFF_GIT_STATE.txt` 为准。

---

## 0. 给下一位代理的 30 秒摘要

1. **架构基线必须牢记：** 项目是在 `328950225/TrinityCore-NPCBOT-Eluna-zhCN` 的 `NPCBOT-Eluna-zhCN-2026` 架构上改进和优化；`wpch3/wow-mecode` 是用户成果同步仓库，不是另一套TC。
2. G11第2步human最小验收已通过：同一PID 40秒得到 `28/28/0/0/True`，用户确认28条human原始日志的 `kind=human`、`readonly=1` 全齐。禁止重复Apply、编译和探针；完整T1–T10/10/100/500性能后补。
3. **G17-P0已经完成。** schema-2真实报告 `g17_source_probe_v2_20260821_125214.txt` 已原样归档；83044字节、UTF-8-BOM/CRLF、SHA-256=`13dc5682...107d0`，42个上下文，required/optional/conflict均0，A/B READY均True。
4. **G17-A代码交付、用户真实Apply、VS2022编译和基础运行均已完成。** 完整后镜像 `SpellInfo.cpp`=`537e5c35...0755`，新worldserver已由用户真实MSBuild链接；schema-2、第17/18/18R/19A/19B/20A/20B-S全部关闭，禁止重复。
5. G17-A第20C-1也已关闭：用户明确确认主城内召唤拒绝、从外飞入暴风城会落地、PVP战场表现相同，并声明“执行完成”。证据只按该摘要归档；没有单独提供的A4室内地点/GPS、精确世界竞技场地点和逐项前后检行不得补造，也不得要求重复主城/PVP测试。
6. G17-A只改 `src/server/game/Spells/SpellInfo.cpp`，conf默认关闭；G17旧世界安全策略保留NO_FLY/竞技场/城市/室内等边界。R2服务端链PASS；R5把同一R4归档置于有效zhCN Y槽后，用户确认59961普通按钮召唤、上马、起飞、水平移动及室内自动解除全部成功，纯飞行门关闭。R3实时`Player::IsOutdoors()`安全门继续保留；R1–R5禁止重复。
7. 御龙术不能缩成学几个技能。R1–R5纯飞行门与B1全坐骑接管均已关闭；B2已真实Windows部署，技能3向前/向上与控制恢复PASS，但技能2反馈、高速转向和技能4类型化着陆真实体验FAIL。B2R1三技能重制已由用户真实Windows报告证明源码应用、World迁移、MSBuild、新鲜OBJ/EXE/PDB及新EXE哈希全部PASS；当前只待重启新worldserver并做客户端A–E Runtime。B3玩家/移动/类型攻击独立页→B4骑乘施法/吃喝→B5自动寻路/固定→B6客户端反馈和压力加固仍未实现。
8. **完美客户端最终目标已锁定。** 不能永远停在纯服务端绕行；人物模型、真实ADT/WMO城市与建筑、新种族/职业/世界、NPC种族和真龙主线都要双端落地。权威总纲已加入官方12.x持续差分账（截至2026-08-21为12.1）及“改造完成后的城市/新世界户外空域默认可飞”终局口径；当前主城落地只属G17-A安全一期，不是最终上限。
9. **PBot完成度已按用户纠正重置：只有基础，其它完善完全没有。** 旧A25/A27/A39/A41勾选只算基础文件/命令，不能宣称完整AI。NPCBot有较成熟Creature战斗伙伴基础，但自主冒险、长期记忆、身份、背包经济、生活、Eluna和规模仍未完成。权威清单：`tc-bignum/规划/04-PBot与NPCBot真实完成度及后续路线_2026-08-21.md`。
10. G19第3步已编译、数据库及启动加载通过；只剩游戏内场景/900秒冷却矩阵，不重跑SQL。
11. G16永久硬需求：每个eligible item至少10条有效独立拍卖行；堆叠件数只作诊断；传说装备禁止，传说坐骑/宠物例外。约99,250总量不能作为完成。
12. 每批除原始源码/config/SQL/probe外，必须同步 `tc-bignum/00-当前整体安装步骤_单文件入口.md`；它是用户唯一操作入口。当前工作树未提交/推送；以manifest/git-state为准。
13. **旧F44真人功能FAIL；F44R1已运行并获用户定性烟雾认可。** 本地回归、Windows Check/Apply、VS2022增量编译及正确哈希新二进制启动均PASS；PID=`16556`，exe=`36734464`字节/SHA `07a8f952...a86c1`。用户真人反馈“没什么太大的问题”并授权转下一步；A至D全职业结构化矩阵未回传，不得补造。Check/Apply/编译/启动禁止重复。
14. **当前开发主线是G17-B2R1三技能Runtime体验重制。** B1已关闭；B2旧包真实部署且技能3向前/向上和控制恢复PASS，但整体体验FAIL。B2R1以B2后像`8b47a5b...c1d5`为前像，后像=`ff185d99...c4fc`，安全回滚=`e298a856...0203`：技能2四阶段反馈；技能3七节点曲线、0.70弧度偏航上限、三级LOS回退和平滑交还；技能4迁移到项目真实DBC中无视觉/无Aura的52226，按魔法平姿、龙45码斜坡、机械反推、猛兽无火扑落、通用避障处理。payload/安全回滚双GCC14/C++20零诊断，52/52、未知SHA零写入拒绝、World迁移三状态模型、双AST、18文件包与ZIP解压复验PASS。唯一ZIP=`G17B2R1_Runtime_Experience_Rework_Windows_20260824.zip`（73526字节/SHA `94822d39...8daa`）；统一CMD已真实执行PASS，当前只待新二进制启动和A–E五行Runtime。
15. **超大团队一键组团没有完成。** 固定状态为`BOT_MASS_RAID_ONE_CLICK=NOT_IMPLEMENTED`；40/100/250/500/1000规模路线已持久化，但按用户决定排在G17-B0/B1后，不打断御龙术。
16. **游荡NPCBot跟随入本/团本的装备适配也未完成。** 固定状态为`NPCBOT_CONTEXT_GEAR_ADAPTATION=NOT_IMPLEMENTED`。上游证据显示装备只在firstspawn初始化，游荡优先走WANDERING类别，DUNGEON仅面向临时5人LFG bot且断言非团本。GEAR-P0到P5的场景分类、职责配装、临时覆盖/恢复、防复制、诊断、性能和UI路线已写入04号权威规划；排在G17-B0/B1后、规模线前。
17. **Bot职责自动选择和缺口补充没有完成。** 固定状态为`BOT_AUTO_ROLE_SELECTION_AND_FILL=NOT_IMPLEMENTED`（实现域别名`BOT_ROLE_AUTO_ASSIGN_AND_FILL=NOT_IMPLEMENTED`）。后续必须区分CAPABLE/ASSIGNED/ACTIVE，尊重玩家锁定，按真实技能/装备/AI选择主副坦、治疗、近远程和关键能力，从PBot/NPCBot混合池补缺并设置备援；不得只按人数或role标签盲目加入。
18. **超大团队BOSS可玩性缩放没有完成。** 固定状态为`MASS_RAID_BOSS_SCALING_PLAYABILITY=NOT_IMPLEMENTED`（实现域别名`BOT_INSTANCE_SCALING_CONTROL=NOT_IMPLEMENTED`）。用户已观察到人数增加后BOSS血量直接上亿且不可击杀；后续必须锁定真实乘法链，以有效参战人数和目标TTK做分段递减，限制生命/伤害倍率，使用uint64中间值、防重复缩放和开怪快照，并将40/100/250/500/1000职责、装备、机制、战斗时长、奖励与性能一起验收。
19. **G17完整通用坐骑范围当前到B2R1待Runtime。** `G17_通用御龙与战斗坐骑总设计_20260823.md`中的B1已真实Runtime关闭；B2已真实部署但体验FAIL；B2R1源码/SQL/自动测试/Windows源码应用、World迁移、MSBuild和新鲜产物门均已真实PASS；尚未做新二进制下的操控/视觉验收。B3四类型攻击与独立页、B4骑乘施法、B5寻路/固定、B6客户端反馈/压力均未实现。

---

## 1. 项目身份、目的与仓库关系

### 1.1 项目目的

这是 World of Warcraft 3.3.5a 的客户端与服务端联合魔改项目，服务端基础是 TrinityCore + NPCBots + Eluna 的中文整合分支。工作范围不仅是配置和数据库，也包括：

- TrinityCore/NPCBot/PlayerBot C++ 修改；
- 世界数据库、角色数据库和自定义数据表；
- WoW 3.3.5a 客户端、DBC/MPQ/界面及后续客户端能力扩展；
- 高清人物模型、真实城市/建筑、新可玩种族、新职业、新地图/新世界和NPC专用种族；
- bot 羁绊、情境对话、自主行为、经济和社交系统；
- 剧情《真龙纪元》的任务、场景、世界变化与多结局落地。

用户的最终目标同时包括：让 bot 能“自主冒险、偶遇玩家并逐步形成队伍”；以及突破3.3.5客户端原有限制，形成真正完整的客户端/服务端定制产品。纯服务端相位、模型复用和`.story`只能作原型，不能冒充最终完成。

### 1.2 两个仓库不可混淆

| 仓库 | 角色 | 当前事实 |
|---|---|---|
| `wpch3/wow-mecode` | 用户上传的改进成果、规划、配置、SQL、安装器和交接同步仓库 | 已克隆到 `/home/user/wow-mecode`；`main` HEAD=`d1ab4be` |
| `328950225/TrinityCore-NPCBOT-Eluna-zhCN` | **项目实际 TC/NPCBot/Eluna 架构基础**，所有服务端改进都建立在该架构上 | 参考分支 `NPCBOT-Eluna-zhCN-2026`；取证提交=`4e8762e` |

用户正在 Windows 上实际编译的完整改进源码位于 `D:\TrinityCore`，分支 `bignum-mod`、当前探针 HEAD=`ae60f5b6f6ffe9a0426dfeb6e227712de7d6c8b7`。它不在 `wow-mecode` 工作树内。上游 `4e8762e` 用于 API/机制取证，真实安装锚点必须以用户源码为准。

### 1.3 用户环境

- 源码：`D:\TrinityCore`
- 编译目录：`D:\TC-Build`
- 客户端：`D:\WOW`
- Windows导入的Arena工作区：`C:\Users\Administrator\Downloads\workspace`；仓库为其内`wow-mecode`，上传目录为其内`uploads`。后续命令统一用该简化路径，不再使用旧长UUID目录名。
- 常用工具：Visual Studio 2022、CMake GUI、Git Bash/PowerShell、DBeaver
- 用户源码探针已确认：分支 `bignum-mod`，HEAD `ae60f5b6f6ffe9a0426dfeb6e227712de7d6c8b7`；存在大量已跟踪修改和自定义未跟踪源码，禁止 reset/clean。
- Windows Python 现状：`Python314\python.exe` 不存在，`py -3` 记录已失效；可用路径已找到 `Python312\python.exe` 与 `Python310\python.exe`，当前优先直接调用 Python 3.12；不得使用 `WindowsApps\python.exe` 占位符。

---

## 2. 证据与状态口径

本交接严格区分以下状态：

| 口径 | 含义 |
|---|---|
| 用户确认编译成功 | 只证明 C++ 构建完成，不自动证明新二进制已部署 |
| 仓库配置已修正 | 只证明 `/home/user/wow-mecode` 文件正确，不自动证明 Windows 运行目录已复制 |
| SQL 已写好 | 不等于已在真实数据库执行 |
| 上游源码取证 | 证明机制/API，但不证明用户修改源码的行号和作用域相同 |
| 游戏验收通过 | 必须有用户游戏内观察、原始日志或数据库结果；G11 human最小门槛已有正式判定和用户字段确认，但完整T1–T10/性能未完成；G19场景/冷却与AHBot新阶段也未完成 |

---

## 3. 当前优先级与唯一主线

### P0（已完成）：G17-A用户侧self-test与真实源码Check

- G17 schema-2探针已经完成并归档，禁止重复v1/v2。
- G17-A完整交付包已生成：完整后镜像源码、默认关闭conf、审计patch、安装器、SHA256SUMS、专项说明和ZIP齐全。
- 用户第17步完整回传已通过：五个包内哈希、普通与`-O` self-test、`CHECK_READY_TO_APPLY=True`、`Source/config/database edits: 0`及真实源码前后哈希不变全部齐全。
- 聊天转录归档：`tc-bignum/规划/G17_飞行与移动/证据/g17a_step17_selftest_check_pass_20260821.txt`，SHA=`d2595db5...0592`；第17步禁止重复。

### P1（已关闭并转线）：F44R1 `.combo`增益生命周期与治疗优先

- 旧F44的真实探针、三原件、Check、Apply、VS2022编译及新exe启动均已完成并关闭；活动旧exe哈希曾正确，但这不等于功能PASS。
- F44-A真人只通过“道标首次及稳定主坦”单项；王者印记约32秒`100→83`并继续到`80`，且不治疗自己、坦克和DPS，故旧F44-A整体FAIL。
- 已确认直接根因：神圣圣骑同时规划强效力量/智慧/王者，五人队可预排约15次互斥祝福；旧`.buff now`队列仅在自动buff小段让路，攻击/治疗仍争GCD；普通5分钟和稳定10秒提前刷新也违反用户要求。
- F44R1将受控基础数据审计为31专精/493条；新增`SF_EXCLUSIVE_BUFF`、`SF_MANUAL`、`SF_DEPLOYABLE`、`SF_COMBAT_UTILITY`，系统分类姿态、形态、守护、护甲、圣印、祝福、领域、护盾、武器附魔、毒药和图腾。
- 圣骑祝福按目标职业统一只选一种（坦克庇护/王者、法系智慧、物理力量）；所有正常Aura只有`AuraRemainMs==0`才补，不存在提前刷新窗口；全专精计划每个互斥家族只保留职责排序首选。
- 图腾每次进战斗只成功部署一次；毒药/武器附魔保留栏位但禁止错误Unit Aura自动重放；误导/嫁祸改为稳定坦克战斗工具；错误的无条件回蓝/宠物专属/防御分类已清理。
- `.buff now`队列现在是tick入口全局GCD闸；进战斗或治疗线不安全会取消余队列；`.buff off`立即递增generation并清零队列。治疗职责血线不安全时只治疗、必要回蓝或等待，绝不下落攻击。
- F44R1本地门槛、Windows Check/Apply、VS2022增量编译及正确哈希新二进制启动全部PASS；PID=`16556`，exe SHA=`07a8f952...a86c1`。构建末尾中文为UTF-8被GBK误解码，只影响输出可读性；0警告/0错误且不得为此重编。用户真人定性反馈“没什么太大的问题”并授权转下一步；未回传A至D结构化数量/时序，不能写成全职业矩阵PASS。
- 治疗默认值保持按场景递进，不全局抬高；现有游戏内入口为`.buff scene`。任意自奶/队友/救命阈值和其它辅助策略另立可回滚批次，不混入G23-P0。

### P1A（安装/编译关闭，被动观察）：G23-F45自动群拾可靠性

- 用户随机完整/不完整/只剩一具和混合怪物更明显的定性已接受，禁止再要求R1–R4计数。
- F45包：`tc-bignum/补丁库/02_修复/F45_AoE拾取随机漏尸与组队金币语义/`；不可变ZIP=`F45_delivery_20260822.zip`，38016字节，SHA=`469e55b0...82a8`。
- 本地门槛PASS；用户明确确认Windows Check、Apply、VS2022编译成功，并进一步确认正常游戏中播报可见、观察到的拾取数量一致；禁止重复。
- F45只随正常游戏被动观察；运行确认仅限用户回传粒度，不扩写组队roll/邮件等未测试矩阵。

### P1B（P2/P2R1关闭，P3A已交付待Windows）：G23 Lua功能

- P2完成共享核心、原子奖励、安全传送、诊断和ObjectVariables隔离；P2R1把传送菜单改为无状态sender/intid分页，用户确认“可以了”，运行PASS仅限修后菜单粒度。
- P3A新增`.server status/commands/daily/health`统一助手；`G23.PASS_THROUGH`保证原生`.server info/restart/shutdown`、未知未来子命令及控制台命令继续由核心处理。
- 新版`.gmhelp`含153条去重中文项目/常用核心目录、11类无状态菜单，并按需联合搜索真实`world.command`；旧C++文件不修改、不编译，回滚后自然恢复旧行为。
- 最终ZIP=`G23P3A_delivery_20260822.zip`，34756字节，SHA=`fc9d850f...415270`，20文件；Lua5.2/mock/完整P2R1集成/安装器/ZIP门槛PASS。
- 当前只按10号执行P3A Check→停服→Apply→正常启动；无SQL/编译，禁止`.reload eluna`。

### P2（当前开发主线）：G17-B2R1客户端Runtime与G22客户端终局

- B0参考实现已经写入`tc-bignum/补丁库/01_功能/G17B0_原生御龙载具/`：`.dragon summon/dismiss/status/help`、原生VehicleAI、4格VehicleActionBar、100 Energy、龙息/加速扣能、离散爬升、安全着陆和PlayerScript清理。
- C++已在上游`4e8762e`头文件/API上通过GCC 14.2/C++20单翻译单元`-fsyntax-only`；完整CMake因本地缺Boost开发依赖而未完成，不能写成构建PASS。
- world v2离线合同曾PASS，但真实01返回1267 collation错误，证明离线门不能代替真实库执行。v3保留精确克隆/动作条/移动/SpellScript、事务门和owned-only回滚，新增01显式`USE world`与全量`utf8mb4_unicode_ci`锁；02仍是单语句只读且全限定`world`.`table`。
- 本地静态合同现已PASS：`G17B0_STATIC_TESTS=PASS`、4个注册SpellScript、4个动作条槽、未声称B2。
- 用户完整结果`G17B0_LOCK_RESULT_20260822_120243.zip`已从GitHub提交`cc3c219`下载验收：73265字节、SHA `ece3d768...aee0c`、10项CRC/路径安全PASS；真实分支/HEAD=`bignum-mod`/`ae60f5b...`，loader=PRE、新cpp缺席。
- 真实loader前像、7个上下文文件、结构后像及双门控安装器已保存；CMake自动收集Commands，无需改CMake或ScriptLoader.h。用户捕获头文件覆盖参考树后的C++翻译单元语法PASS。
- 当前预检ZIP=`G17B0_Preflight_20260822.zip`，95950字节、SHA `4affab73...8132c`；只含SelfTest/Check及world只读SQL，不含Apply wrapper/审批标志，Apply硬锁。
- 用户真实源码预检文件已从GitHub提交`db131492aef1fde77f731c08fe046205071b3b16`下载验收：1914字节、SHA `724d58ee...14b0`；`G17B0_INSTALLER_SELF_TEST_PASS=True`、2个负例、SelfTest/Check rc均0、最终`READY_TO_APPLY`且`SOURCE_EDITS=0`。源码阶段PASS，禁止重复CMD。
- live world v2结果已从提交`abd3c2179136f19231112d6177f0f42c2d1e6285`原样归档：17472字节、SHA `af4a6031...470e`、50数据行、首尾marker齐全；离线验收器锁定表格、7表、30列、source/target/动作条/移动/summary，PASS。
- 冻结DB前像：`world`、MySQL 8.0.46、source entry 27756唯一且VehicleId70/model25854、4条原动作、Flight=1；target 1000171为0行，候选自定义ScriptName绑定0，状态`G17B0_DB_PREIMAGE_READY`。v1的1046/1271均关闭，禁止重跑探针。
- world表无冲突不等于四个基础DBC技能已Runtime验证；实际技能条/施法仍须经过worldserver加载、编译和游戏验收。
- 受控源码Apply包=`G17B0_Source_Apply_20260822.zip`离线FINAL_PASS；用户真实结果1668字节、SHA `772f4007...5ca4`，PRE→POST改动2项、备份及7上下文正确，最终ALREADY_APPLIED/wrapper PASS，验收器输出`G17B0_WINDOWS_SOURCE_APPLY_ACCEPTANCE=PASS`，禁止重复。
- world v2真实1267失败历史已关闭；v3真实02结果492字节、SHA `182a1878...f32a`，不可变验收器确认唯一行、11列和所有精确计数，`G17B0_WORLD_POSTCHECK_V3_ACCEPTANCE=PASS`。world SQL禁止重复。
- 构建v3真实结果6396字节、SHA `58c40872...952e`：native自测/CMake0/vcxproj命中1/MSBuild0/fresh obj1；明确编译dragonriding和loader、生成scripts.lib并链接worldserver。新EXE=36759040字节/SHA `59491e97...5881`，PDB=433442816字节。
- 旧v3的唯一`warning C4018`位于正数energy cost的signed/unsigned比较，非致命且不影响旧链接；G17-R1已将cost改为`uint32`并在负扣能处显式转`int32`。用户真实新Windows构建报告`DRAGONRIDING_C4018_HITS=0`，该告警门已PASS并关闭。
- 用户真实最小Runtime：`.dragon help/status`正常，`INACTIVE area_allowed=true`，但`.dragon summon`回报进入可控座位失败；完整worldserver日志无G17命令期Vehicle细节。精确状态为`G17B0_RUNTIME=FAIL_ENTER_CONTROLLABLE_SEAT`。
- 根因已由指定上游实现链确认：`Unit::EnterVehicle`施放46598，`Vehicle::AddPassenger`把`VehicleJoinEvent`加入玩家事件队列；旧B0在事件前立刻判断`GetVehicleBase()!=dragon`并销毁龙。R1按实际`VEHICLE_SEAT_FLAG_CAN_CONTROL`选座，在同一玩家事件队列250ms后核验vehicle/seat/charmer，并记录VehicleId、seat flags、PassengerBoarded和结果。
- G17-R1后源码SHA=`10a7002db4c173e441836870da01dd3009a7ba470369ca1a89bdb399ee9b2f45`；上游HEAD `4e8762e`真实头文件GCC14 C++20 `-fsyntax-only -Wall -Wextra -Wsign-compare`为exit0/0输出。8项自动测试和包自检PASS。
- 纯飞行坐骑客户端门：仅G17-A服务端`CheckLocation`不足以解开客户端`Spell.dbc AttributesExD 0x04000000`。R1补丁器只对Aura 78/201/207结构目标清该bit，服务器DBC原样保留。真实build-12340 zhCN Spell.dbc为49839行，精确修改110行，10个始祖幼龙ID全命中。
- 独立服务端包=`G17R1_Runtime_Fix_Windows_20260822.zip`（31645字节，SHA=`f4eb4e20...e4d8c`）已由用户真实执行：Apply/CMake/MSBuild/fresh OBJ/C4018=0/新EXE全部PASS。R2A随后确认R1客户端安装报告、状态、自有MPQ和有效Spell.dbc全部PASS。
- 用户真实R1 Runtime已确认：`.dragon summon`无问题，4个载具技能全可用；因此`G17R1_RUNTIME_VEHICLE_CONTROL_AND_ACTIONBAR=PASS`。同时体验确认为慢、无动量/分档且移动与攻击混页，完整御龙未完成。
- 59961首次湿地失败的确定性根因是G17-A strict分支无条件`CanFlyInZone()`；R2后镜像SHA=`73d52ac0...cb9e2`，本地10/10测试和Windows构建PASS。用户在湿地执行未带triggered参数的`.cast self 59961`已成功召唤并上马，因此`G17R2_END_TO_END_SERVER_CAST_CHAIN=PASS`，禁止继续放宽服务端。
- R2A只读包真实结果已PASS；R2完整服务端施法链关闭，禁止继续放宽服务端。
- R3锁定zhCN AreaTable原像`b0356ff4...62dd`，对地图0/1的948行只增加`0x00000400`，后像`214c6935...b6a8`；服务端后像`c3ec2237...bcbf`加入实时`Player::IsOutdoors()`。真实Windows构建/MPQ安装PASS，新EXE SHA=`15005e8f...2a9`、安装MPQ SHA=`c9013624...15f`，服务端DBC未改。
- R3真实游戏失败：59961普通按钮仍只在外域/诺森德可用，无头骑士外层坐骑可召唤；湿地`IsFlyableArea()=nil`。这否定了“单个OUTLAND位已完整关闭客户端门”。
- R4审计确认R3未带`0x00004000`（EnableFlightBoundsonMap/OUTLAND2），因此曾在相同948行补位；湿地Area ID 11为原版`0x00000040`、R3`0x00000440`、R4`0x00004440`，R4 Area SHA=`1acef997...c233`，R1 Spell仍为`dd250911...64ea`。但R4真实Runtime仍nil，已证明“遗漏第二位”不是充分根因；禁止继续把它写成已解决根因。
- R4 DBC逐行测试、两个PS1 AST、真实MPQ、完整安装/幂等/回滚与Data/zhCN碰撞拒绝均PASS。ZIP=`G17R4_Pure_Flight_Dual_Area_Flags_Windows_20260823.zip`，588095字节、17文件、SHA=`fbf4a9ad...935d3`。用户真实R4安装也PASS：根MPQ v2/4文件、内部Spell/Area哈希正确、Cache已删、已知碰撞0；但湿地Runtime仍`IsFlyableArea()=nil`，所以R4运行验收FAIL。
- R4 payload中湿地父区和全部26个直接子区都已含`0x400`；R3/R4 Area均未改变API表现，且根MPQ内R1 Spell也未让普通59961按钮生效。当前根因收紧为“根Data自定义MPQ不是zhCN客户端最后采用的DBC来源”，不再盲加Flag。真实报告中的`Data\zhCN\patch-zhCN-Z.MPQ`是无Spell/Area的目录，R5不覆盖它。
- R5把已验证的根R4 MPQ逐字节镜像到空闲、有效封装的`Data\zhCN\patch-zhCN-Y.MPQ`；用户真实安装结果PASS，并确认59961普通按钮召唤/上马/起飞/水平移动和进入室内自动解除全部成功。`G17R5_REAL_WINDOWS_RUNTIME=PASS`，纯飞行门关闭；多参数`print`只显示首项不作为API值，但不推翻完整行为PASS。
- B1源码已实现通用拥有坐骑拦截、源display保留、地面/飞行统一Vehicle、DRAGON/BEAST/MECHANICAL/MAGIC/GENERIC类型会话、`.dragon auto on|off`和扩展status；替代Vehicle和座位准备完成后才移除普通Aura。前像SHA=`10a7002d...9b2f45`，后像=`2c7594d0...a68199`；GCC C++20零输出、8测试、安装/幂等/回滚、PS AST、包/ZIP门PASS。
- 旧B1包=`G17B1_All_Mounts_Auto_Intercept_Windows_20260823.zip`已永久废弃：PowerShell函数形参`$Args`覆盖自动变量`$args`，真实运行停在Python路径后、native selftest前；用户源码仍为R1前像，因此无源码写入、无构建、无SQL/客户端修改。
- B1R1包=`G17B1R1_Native_Runner_Fix_Windows_20260823.zip`已执行且禁止重跑。真实报告SHA=`aa5f7521...f7bd7`：source apply、MSBuild、fresh OBJ和新EXE/PDB通过；EXE从`15005e8f...172a9`变为`dc85bd93...a2581`。末尾FAIL属于PDB前时间戳未快照的后检假阴性，接受器输出`G17B1R1_WINDOWS_BUILD_ACCEPTANCE=PASS`。
- B1R2规范修复批次保存B1R1原件，并将构建前EXE/PDB时间立即复制到`BeforeExeUtc/BeforePdbUtc`值类型后再比较；静态测试和PS AST PASS。本轮不交用户执行。
- B1首次真实Runtime为FAIL：34767/59961均已匹配VehicleBase与charmer，只有`GetTransSeat()==-1`；movement transport seat不是Vehicle占用权威，旧verifier据此误清理。
- B1R3以`Vehicle::Seats/GetPassenger()`取得权威seat，同时严格要求VehicleBase/权威seat/charmer；用户真实确认地面/飞行接管、起飞与降落均正常，座位修复行为PASS。
- B1R3后的室内FAIL由B1R4活动Vehicle AI每250ms复用`IsBlockedArea()`和单次锁修复；用户确认进入真实室内会自动退出，`G17B1R4_REAL_INDOOR_RUNTIME=PASS_USER_CONFIRMED`，旧B1R4包禁止重复。其降落伞被用户明确拒绝。
- 爱情火箭71342与无头骑士48025的真实DBC外层均为Effect `[6,6,77]`、Aura `[78,4,0]`、TriggerSpell `[0,0,0]`；内层关系由C++ `spell_gen_mount`按骑术/地点选择。`SpellMgr`把外层前两个Effect置为NONE但保留ApplyAuraName metadata，旧同ID门因而漏判。
- B1R5用`ApplyAuraName == SPELL_AURA_MOUNTED`通用覆盖直接/包装外层，玩家所有权锚定外层，活动内层Aura提供creature/display；禁区清理无降落伞。用户已确认室内无降落伞、无头骑士及其他包装坐骑正常，B1关闭，旧包禁止重跑。
- B2源码=`35af002b...668b8 -> 8b47a5b...c1d5`，1575行/60897字节。七档250%–1200%硬上限；动量由前进、俯冲、拉升、转向、后退制动和停滞连续更新，技能2是4秒有界推进；低动量失速启用真实重力并可由动量阈值或推进恢复。
- 技能3使用沿Vehicle当前朝向向前14码+向上8码的碰撞感知`MoveJump`；反向/障碍不足1码安全取消且不扣能量。MovementInform或1500ms超时均显式恢复Vehicle飞行、重力、`AnimTier::Fly`、当前速度、MotionMaster和客户端完整控制。
- 技能4通过`PreventHitDefaultEffect`阻止53208降落伞Aura；魔法/风、野兽两段扑跃、机械火箭、龙类收翼和通用着陆分别处理，触地后统一恢复Vehicle/玩家状态。B1R3权威seat、B1R4 250ms室内检查、B1R5包装Aura识别均有回归门。
- B2 GCC14/C++20严格零诊断、33/33及旧ZIP离线门曾PASS；随后真实Windows部署成功、技能3向前/向上与控制恢复PASS，但技能2反馈、高速转向和技能4着陆体验被用户否决。旧包只保留历史，禁止重跑。
- B2R1锁定`8b47a5b...c1d5 -> ff185d99...c4fc`，安全回滚`e298a856...0203`；World SQL SHA=`b4526c24...e88b66`。项目真实DBC确认52226 Visual 0/0、Dummy、无Aura；World前像8文件零碰撞。
- 技能2新增启动/持续/极速/结束反馈并拒绝重复推进；技能3改7节点Catmull-Rom、偏航/LOS/平滑交还；技能4按魔法/龙/机械/猛兽/通用类型分段，移除53208与14475并完整归一落地状态。
- payload与安全回滚均GCC14/C++20严格零诊断；52/52含未知SHA check/apply/rollback零写入拒绝、World迁移成功/重复/外来冲突三状态模型；PowerShell 7.6.5双AST 0错误；18文件包、CRC、解压自检均PASS。
- 用户回传`G17B2R1_WINDOWS_BUILD_RESULT.txt`已证明52项、源码`8b47... -> ff185...`、World迁移、MSBuild、新鲜OBJ/EXE/PDB和新EXE哈希全部PASS；禁止重跑统一CMD。当前只启动`D:\TC-Build\bin\RelWithDebInfo\worldserver.exe`并做A–E客户端Runtime。B3–B6仍NOT_IMPLEMENTED；完整御龙完成后才恢复Bot优化。

### P3（已排到G17-B0/B1后）：NPCBot场景装备、PBot/NPCBot与超大团队

- 用户已明确纠正PBot“只有基础，其它完全没有”；后续状态必须据此写，不得沿用旧百分比或旧勾选。
- 新增硬需求：游荡NPCBot被招募并跟随进入5人/团本时必须按地图、难度、职责和专精配备合理装备，离本恢复且不可复制利用。固定结论为`NPCBOT_CONTEXT_GEAR_ADAPTATION=NOT_IMPLEMENTED`。
- 上游`4e8762e`窄审计显示`InitEquips()`只在firstspawn调用；`IsWanderer()`优先选择WANDERING装备，DUNGEON分支仅面向临时LFG bot并断言`IsNonRaidDungeon()`，不能覆盖用户场景。真实Windows源仍须GEAR-P0探针。
- GEAR-P0到P5已经冻结：只读audit/preview→5人临时覆盖/恢复/防复制→10/25普通/英雄职责模板/附魔宝石→批量缓存压测→客户端UI。它排在G17-B0/B1后、超大团队线前，因为再多弱装bot也不构成可用团队。
- 职责自动选择/补缺固定为`BOT_ROLE_AUTO_ASSIGN_AND_FILL=NOT_IMPLEMENTED`：真实能力审计、玩家锁定、CAPABLE/ASSIGNED/ACTIVE、约束编成、混合补缺池、有限转职/临时装备、主副坦/治疗/关键技能备援和诊断均已加入N13。
- BOSS可玩性缩放固定为`BOT_INSTANCE_SCALING_CONTROL=NOT_IMPLEMENTED`：用户确认的人数增加后血量上亿不可击杀属于明确缺陷；真实缩放链探针、有效参战人数、分段递减、目标TTK、倍率硬上限、防重复、uint64数值安全、开怪快照和重置幂等已加入N14。
- 专项审计已确认`.pbot invite all`仍受`Group::IsFull()`和传统Group上限约束，现有A30/A39/A41/NPCBot全选不能证明40/100/1000一键副本团队完成。
- 固定结论：`BOT_MASS_RAID_ONE_CLICK=NOT_IMPLEMENTED`。后续统一`.bots raid`、自动职责补缺、PBot/NPCBot混编、装备/缩放可玩性门槛、原子预检/回滚、Group/封包/持久化扩展、分片调度和40/100/250/500/1000压测路线已经持久化。
- 按用户决定，Bot各线不能提前打断御龙术，也不能因为路线文件存在宣称完成。
- 完整清单与工期见 `tc-bignum/规划/04-PBot与NPCBot真实完成度及后续路线_2026-08-21.md`。

### 保留待办（不与P0混跑）

- G11 human最小门槛已过；完整T1–T10与10/100/500性能后补，禁止重复Apply/编译/探针。
- G19只剩游戏内主城/野外/副本与900秒冷却验收，不重跑SQL。
- G16等待SQL `12`/`14`与真实源码探针；最终验收必须按每entry至少10条独立活动行和传说例外规则。

---

## 4. G19 第3步到底为下一步准备了什么

G19 第3步不是单纯“再加一些台词”。它为对话选择链补上环境维度：

1. `scene`：主城、野外、副本、战场等场景类别；
2. `zone_id`：指定区域上下文；
3. `map_id`：指定地图上下文；
4. 匹配评分：地图/区域/种族/场景/职业按权重选择；
5. 冷却池轮换：避免短时间重复同一句话，并提高语料覆盖率。

这正好成为 G11 的“环境输入”。G11 后续判断附近玩家时，不只知道“有一个玩家”，还可以结合：

- bot 在哪张地图、哪个区域、哪种场景；
- 玩家是否处于战斗、低生命或已有队伍；
- bot 与玩家等级差、距离和阵营/可见性；
- 后续决策是否适合在该环境发生。

这条历史依赖链中的G11只读安装和human最小验收已经完成。当前用户要求先转G17；G11完整矩阵以及之后的“决策→行为→组队→记忆”仍保留，不得因插入G17而误写完成。G19运行验收仍须与G11分别记录。

G19 文档版本关系：

- `04-第3步_场景感知实现.md`：通用旧方案，已作废，禁止重装；
- `05`：当时的真实源码探针；
- `06`：基于用户源码重写的精确补丁；
- `07` 到 `11`：编译错误诊断与最终修复历史；
- `11` 的修复已完成且编译通过，不要重复执行；
- `12`：当前唯一执行入口，负责编译后的部署与验收。

---

## 5. G11 第2步交付与安全边界

### 5.1 已创建文件

| 文件 | 用途 |
|---|---|
| `tc-bignum/规划/G11_bot自主冒险/06-第2步_感知层实施规格.md` | 第2步功能、配置、候选事实、性能保护和验收定义 |
| `tc-bignum/规划/G11_bot自主冒险/07-第2步_装机前探针.md` | 探针历史入口；用户真实探针已完成 |
| `tc-bignum/规划/G11_bot自主冒险/probe_g11_step2.py` | 只读读取 Git、文件元数据、源码锚点、API 和冲突符号 |
| `tc-bignum/规划/G11_bot自主冒险/08-第2步_基于真实源码的安装与验收.md` | G11 第2步专项技术细节；总体操作仍以 `00-当前整体安装步骤_单文件入口.md` 为唯一入口 |
| `tc-bignum/规划/G11_bot自主冒险/install_g11_step2.py` | 锁定四文件哈希，提供 check/apply/rollback |
| `tc-bignum/规划/G11_bot自主冒险/g11_step2_windows.cmd` | 可选Windows包装；本次未使用且无需补跑 |
| `tc-bignum/规划/G11_bot自主冒险/09-配置已加载但无感知日志_只读取证.md` | 749行启动截取的历史结论；顶部已标明被后续日志证据纠正 |
| `tc-bignum/规划/G11_bot自主冒险/probe_g11_runtime_v2.py` | 唯一运行探针；稳定纯文本、正/负fixture、human最小PASS |
| `tc-bignum/规划/G11_bot自主冒险/10-运行探针v1格式缺陷_复盘与v2自检.md` | v1/v2测试缺陷根因、状态纠正和强制质量门槛 |
| `tc-bignum/规划/G11_bot自主冒险/11-v2真实报告首次结果_新进程短窗口无日志.md` | PID 14980约24秒首次报告、0命中解释及同PID 40秒最小刺激 |
| `tc-bignum/规划/G11_bot自主冒险/probe_g11_runtime.ps1` | 已fail-closed停用；只提示改用v2，不得用于验收 |
| `tc-bignum/conf/worldserver.conf.d/g11_perception.conf` | 独立配置，默认关闭、强制只读 |

### 5.2 本阶段只允许做什么

- 定时、限量扫描附近真实玩家；
- 读取距离、生命、战斗、等级、队伍、在线/断线等事实；
- 生成限频诊断日志；
- 为下一阶段保存只在内存中的候选摘要；
- 提供配置开关、半径、随机周期和日志冷却。

### 5.3 本阶段明确禁止什么

- 不移动或追随；
- 不说话/密语/表情；
- 不攻击或施法；
- 不帮助玩家；
- 不发组队邀请，也不直接 `AddMember`；
- 不改变羁绊；
- 不写数据库；
- 不让每个 bot 每 tick 全地图扫描。

### 5.4 真实探针结论

用户已经完整回传真实源码探针：

- 分支/HEAD：`bignum-mod` / `ae60f5b6f6ffe9a0426dfeb6e227712de7d6c8b7`；
- `bot_ai.h/.cpp` 和 `botconfig.h/.cpp` 的大小、编码、SHA256 已记录；
- G19 `GetCurrentScene` 声明1处、定义1处，源码括号平衡；
- `GlobalUpdate` 存活/在世界检查、现有玩家网格扫描位置已确认；
- `PlayerDisconnected` 和项目自定义 `IsBotSession` 均存在；
- G11 配置键与 `UpdateAutonomyPerception` 当前无冲突。

因此已允许生成源码专用 C++ 安装器，但安装器仍必须验证哈希，不能取消防护。

### 5.5 当前状态与禁止事项

`--apply`、编译、conf部署和worldserver启动均已完成。安装后哈希保留如下：

```text
bot_ai.h       f2e85fe100db83dcaae0ce3c4ecb6b72a19e19eb7e7de80089cfdfa675fe80f6
bot_ai.cpp     d34ad76b1fcfdbacfe0099fb791119993309d2030db0940feddc5b7bdb75e2cf
botconfig.h    6b09154b3566da76bf8aea08ebec791fd38709cd13c11386eb57f8bb55aae58e
botconfig.cpp  6ca80f8b201c90585cf5fd3fcc65eefeda1e02452ae07b07c4902bf12082bcf6
```

同一PID 40秒真人刺激结果：

```text
G11_LOG_MATCHES=28
G11_HUMAN_MATCHES=28
G11_PLAYERBOT_MATCHES=0
G11_LINES_WITH_MISSING_FIELDS=0
G11_HUMAN_MINIMUM_PASS=True
```

用户确认28条原始human行的 `kind=human`、`readonly=1` 全齐。human最小验收关闭；禁止重复 `--apply`、重复编译或重复探针。只保留完整报告归档、T1–T10和性能阶梯未完成。

---
## 6. AHBot 37,500 条的源码级根因

### 6.1 已确认数字

用户给出的 13 类挂单数量总和：

```text
37,500
```

仓库七个品质基础预算总和：

```text
2,000 + 12,000 + 12,000 + 8,000 + 5,000 + 500 + 200 = 39,700
```

旧配置的单共享池目标约为 39,700，因此数据库实际 37,500 与旧单池目标处于同一数量级。37,500 与 39,700 的差额可能来自过期、补货时序、空物品池和权重四舍五入，但这不是缺失约六万条的主因。

### 6.2 共享拍卖行机制

上游 `AuctionHouseMgr.cpp` 证明：开启 `AllowTwoSide.Interaction.Auction = 1` 后，不管请求 Alliance、Horde 还是 Neutral，都返回同一个 `mNeutralAuctions`。

后果：

1. Alliance/Horde/Neutral 不是三个独立容器；
2. `.ahbot status` 三行可能读取同一张 map 三次；
3. 三行数量不能求和；
4. 旧 `100/100/100` 不能稳定表达“3 x 39,700”；
5. 等待只会把同一个目标补满，不会凭空产生三个独立市场。

### 6.3 已安装的共享池配置

用户已确认 Windows 运行目录安装：

```ini
AuctionHouseBot.Alliance.Items.Amount.Ratio = 0
AuctionHouseBot.Horde.Items.Amount.Ratio = 0
AuctionHouseBot.Neutral.Items.Amount.Ratio = 250
AuctionHouseBot.Class.Misc = 2
AuctionHouseBot.Update.Interval = 60
```

理论总目标仍为 `39,700 x 250% = 99,250`，但这只是总量配置，不是最终验收。安装前数据库基线为：39,498唯一行、37,307有效、2,191过期；全为 GUID 5 `鲤鱼`（account 2），没有玩家挂单。

### 6.4 为什么保留 `Class.Misc = 2`

用户明确将 Misc 调回 2，因为坐骑过多。本参数影响类别分配，不决定总目标。把它改成 8 只能提高坐骑/宠物在总预算中的份额，不能解决共享池容量问题。

不要静默恢复旧文档里的 `Class.Misc = 8`。

### 6.5 总量以外的新硬需求

用户明确不接受只达到约 99,250 条：

1. 每个符合条件的 `itemEntry` 至少有10条有效独立 AHBot 挂单；
2. 传说装备不得进入拍卖行；
3. 传说坐骑和宠物必须作为唯一例外保留。

上游 seller 当前按权重随机选 entry，不能保证每 entry 下限；`Orange=500` 也不能做选择性传说过滤。必须新增按 entry 统计活动 AHBot 拍卖、优先补缺口和源代码级 quality/class/subclass 过滤。当前可见 8,267 个 entry，最低 5/10/12 条分别约需 41,335/82,670/99,204 条。用户已确认 `MinCopiesPerEntry=10`，按有效独立挂单条数计；当前可见池保底约需82,670条，堆叠总件数不替代此下限。

### 6.6 当前文件与验收

| 文件 | 状态 |
|---|---|
| `tc-bignum/conf/worldserver.conf.d/ahbot.conf` | 0/0/250、Misc=2、临时 interval=60；用户已确认安装 |
| `tc-bignum/规划/G16_bot经济与打工/11-共享拍卖行只有37500条_根因与十万挂单修复.md` | 已记录安装前数据库基线与部署状态 |
| `tc-bignum/规划/G16_bot经济与打工/12-共享拍卖行_只读诊断.sql` | 总量/归属/house/类别/品质只读诊断 |
| `tc-bignum/规划/G16_bot经济与打工/13-每种物品多库存与传说过滤_新增硬需求.md` | per-entry 下限与选择性传说过滤设计 |
| `tc-bignum/规划/G16_bot经济与打工/14-拍卖行每物品库存与传说过滤_只读诊断.sql` | 每 entry 库存分布和传说例外分类，只含 SELECT |
| `tc-bignum/规划/G16_bot经济与打工/15-每entry十条与传说过滤_源码探针和回传清单.md` | SQL+真实源码完整回传入口 |
| `tc-bignum/规划/G16_bot经济与打工/probe_g16_min_stock.py` | 只读采集六个 AHBot/AuctionHouse 文件、哈希、锚点及 Git 状态 |

仍待回传三组证据：新配置补货后的 `12` 号 SQL、当前活动橙色468行/3 entries与库存分布的 `14` 号 SQL、以及 `probe_g16_min_stock.py` 生成的完整真实源码报告。最终验收必须同时通过总量、每 entry 至少10条、传说过滤、AHBot/玩家归属和类别/品质分布。

---

## 7. 本轮修改文件

### 7.1 新增

- `tc-bignum/00-当前整体安装步骤_单文件入口.md`
- `tc-bignum/规划/G19_情境对话系统/12-第3步编译通过后_部署与验收.md`
- `tc-bignum/规划/G11_bot自主冒险/06-第2步_感知层实施规格.md`
- `tc-bignum/规划/G11_bot自主冒险/07-第2步_装机前探针.md`
- `tc-bignum/规划/G11_bot自主冒险/probe_g11_step2.py`
- `tc-bignum/规划/G16_bot经济与打工/11-共享拍卖行只有37500条_根因与十万挂单修复.md`
- `tc-bignum/规划/G16_bot经济与打工/12-共享拍卖行_只读诊断.sql`
- `tc-bignum/规划/G16_bot经济与打工/13-每种物品多库存与传说过滤_新增硬需求.md`
- `tc-bignum/规划/G16_bot经济与打工/14-拍卖行每物品库存与传说过滤_只读诊断.sql`
- `tc-bignum/规划/G16_bot经济与打工/15-每entry十条与传说过滤_源码探针和回传清单.md`
- `tc-bignum/规划/G16_bot经济与打工/probe_g16_min_stock.py`
- `tc-bignum/规划/G11_bot自主冒险/08-第2步_基于真实源码的安装与验收.md`
- `tc-bignum/规划/G11_bot自主冒险/install_g11_step2.py`
- `tc-bignum/规划/G11_bot自主冒险/g11_step2_windows.cmd`
- `tc-bignum/规划/G11_bot自主冒险/09-配置已加载但无感知日志_只读取证.md`
- `tc-bignum/规划/G11_bot自主冒险/probe_g11_runtime_v2.py`
- `tc-bignum/规划/G11_bot自主冒险/10-运行探针v1格式缺陷_复盘与v2自检.md`
- `tc-bignum/规划/G11_bot自主冒险/11-v2真实报告首次结果_新进程短窗口无日志.md`
- `tc-bignum/规划/G11_bot自主冒险/probe_g11_runtime.ps1`（fail-closed停用占位）
- `tc-bignum/conf/worldserver.conf.d/g11_perception.conf`
- `tc-bignum/规划/G17_飞行与移动/02-重新审计_全世界飞行与御龙术分阶段实施计划.md`
- `tc-bignum/规划/G17_飞行与移动/03-v1真实报告诊断与v2探针更正.md`
- `tc-bignum/规划/G17_飞行与移动/证据/g17_source_probe_v1_20260821_123340.txt`
- `tc-bignum/规划/G17_飞行与移动/证据/g17_source_probe_v2_20260821_125214.txt`
- `tc-bignum/规划/G17_飞行与移动/证据/g17a_step17_selftest_check_pass_20260821.txt`
- `tc-bignum/规划/G17_飞行与移动/证据/g17a_step18_apply_conf_validator_failure_20260821_151724.txt`
- `tc-bignum/规划/G17_飞行与移动/probe_g17_source.py`
- `tc-bignum/规划/G17_飞行与移动/G17A_安全全世界飞行/`（完整源码、安装器、conf、patch、专项说明、SHA256SUMS、ZIP）
- `tc-bignum/conf/worldserver.conf.d/g17_world_flight.conf`
- `tc-bignum/补丁库/02_修复/G17B2_完整御空术与特色降落/`（历史B2；真实部署后体验FAIL）
- `tc-bignum/规划/G17_飞行与移动/G17B2_Complete_Flight_Typed_Landing_Windows_20260823.zip`（历史失败基线；禁止重跑）
- `tc-bignum/补丁库/02_修复/G17B2R1_三技能体验重制/`（B2前像、后像、安全回滚、World迁移、52项测试与证据）
- `tc-bignum/规划/G17_飞行与移动/G17B2R1_Runtime_Experience_Rework_Windows_20260824/`（18文件Windows目录）
- `tc-bignum/规划/G17_飞行与移动/G17B2R1_Runtime_Experience_Rework_Windows_20260824.zip`（73526字节，SHA `94822d39...8daa`）
- `tc-bignum/规划/G17_飞行与移动/证据/G17B2R1_WINDOWS_BUILD_RESULT_20260824.txt`（用户真实报告；源码应用、World迁移、MSBuild、新鲜OBJ/EXE/PDB与新EXE哈希PASS）
- `tc-bignum/规划/04-PBot与NPCBot真实完成度及后续路线_2026-08-21.md`

### 7.2 修改

- `tc-bignum/conf/worldserver.conf.d/ahbot.conf`
- `tc-bignum/README.md`
- `tc-bignum/待办总表.md`
- `tc-bignum/未完成想法-总清单.md`
- `tc-bignum/规划/G11_bot自主冒险/01-总体设计.md`
- `tc-bignum/规划/G17_飞行与移动/01-全世界飞行方案.md`（已标历史取证/禁止施工）
- `tc-bignum/规划/总路线图-2026-08-09.md`
- `tc-bignum/规划/总路线图-v2-含世界线.md`
- `tc-bignum/规划/G16_bot经济与打工/07-坐骑变少的根因与修正.md`
- `tc-bignum/规划/G16_bot经济与打工/08-坐骑只有三种_真因与修法.md`
- `tc-bignum/规划/G16_bot经济与打工/09-坐骑修复_最终方案B.md`
- `tc-bignum/规划/G16_bot经济与打工/10-三个疑问的答复_补货慢与改动清单.md`
- `tc-bignum/规划/G19_情境对话系统/11-最终修复_两个真因.md`
- `PROJECT_HANDOFF.md`
- `HANDOFF_GIT_STATE.txt`
- `HANDOFF_FILE_MANIFEST.tsv`

旧 G16 文档 `07-10`、G19文档 `04-11` 和G17 `01` 仍保留为演变/取证历史，但其旧状态、数值和方案不能覆盖2026-08-21权威检查点。

---

## 8. 本轮实际验证

### 8.1 G11 真实探针与第2步安装包

用户完整探针已确认：

- `D:\TrinityCore` 分支 `bignum-mod`，HEAD `ae60f5b6f6ffe9a0426dfeb6e227712de7d6c8b7`；
- 四个目标文件编码、大小、SHA256及唯一插入锚点均已取得；
- G19 `GetCurrentScene` 声明/定义各1处，源码大括号平衡；
- `GlobalUpdate`、玩家网格搜索、`PlayerDisconnected`、`IsBotSession` 等所需 API 存在；
- G11 新符号和配置键当前无冲突。

本地安装包验证通过：

- `install_g11_step2.py` 的 `py_compile`；
- `g11_step2_windows.cmd` 的标签/GOTO、三模式、日志文件和 CRLF 静态检查；
- `--self-test` 合成源码安装/重复检测/回滚；
- 真实多行 accessor 锚点自测；
- 生成 C++ 大括号和关键符号数量检查；
- conf UTF-8 无 BOM、六项配置和值检查；
- 尾随空白和 `git diff --check`。

本地验证证明安装包内部一致；用户侧 Python 3.12.10 的 `--check`/`--apply` 又确认真实四文件匹配并成功安装。四份原始备份已创建；之后RelWithDebInfo/x64编译、实际运行目录启动均已成功。同一PID 40秒结果为28总/28 human/0 PBot/0缺字段/PASS=True，用户确认全部human原始行均含 `kind=human`、`readonly=1`，最小验收关闭。

### 8.2 G17重新审计、schema-2报告与G17-A交付

- 递归检索飞行、载具、御龙和客户端物理资料；确认真实mask=`0x04000000`，排除旧`0x80000000`、全局恒真和纯SQL方案；
- schema-2用户真实报告已归档：83044字节、SHA=`13dc5682...107d0`、42上下文、0缺失、0冲突、A/B READY；探针关闭，禁止重复；
- 用户真实`SpellInfo.cpp`前镜像：180195字节、CRLF、SHA=`78dbc810...89753`；
- 生成完整后镜像：182275字节、CRLF、SHA=`537e5c35...0755`；
- 安装器SHA=`926585da...93c4`；普通和`python -O --self-test`、Apply/幂等Apply/Rollback、重复/变更锚点负例全通过；
- 用G++17 `-Wall -Wextra -Werror` mock编译运行，验证默认关闭、map0/1放行、其它地图拒绝、NO_FLY/竞技场硬拒绝、主城/室内开关、Zone/Area flags合并和BlockedAreaIds；
- 生成完整交付目录、`SHA256SUMS.txt`和ZIP；ZIP 42209字节、SHA=`f33d4851...2275`，CRC、包内逐字节和解压后`-O --self-test`通过；
- 用户第17步完整回传通过并转录归档：五文件哈希、普通/-O self-test、只读check、0修改与前镜像哈希不变均齐；
- 用户第18步真实Apply、源码备份、exe/PDB备份成功；conf SHA检查通过后文本行断言红字，输出已转录归档；
- 用户第18R步真实只读恢复验证已通过并归档：post/pre/conf SHA、两份二进制备份、无BOM和唯一默认关闭行全部正确；
- 用户第19A步真实VS2022/MSBuild已编译`SpellInfo.cpp`、`game.lib`并链接新worldserver，exe/PDB更新与新exe哈希均PASS；
- 第19B默认关闭验收已关闭：预/后检PASS，主世界拒飞/外域可飞；不重复启动；
- 第20A默认关闭备份与Enable=1精确切换PASS并归档；
- 第20B-S A1 map0/A2 map1/A9 map530启用烟雾与停服后检PASS；第20C-1随后由用户声明执行完成并确认主城禁召、外飞入暴风城落地、PVP战场相同。摘要证据已归档，未提供的A4/精确A6/逐项前后检不补造，不要求重复。

### 8.2R G17-R1入座与纯飞行坐骑修复验证

- 审阅指定上游`Unit.cpp/Vehicle.cpp`确认`EnterVehicle→46598→_EnterVehicle→AddPassenger→VehicleJoinEvent`异步链，旧B0立即检查并销毁龙是确定根因，不把空`vehicle_template`统计或无关Eluna/NPCBot错误误判为根因；
- R1完整后文件SHA=`10a7002...9b2f45`，当前B0原件SHA=`c9535dca...99bd`，精确Apply/幂等/rollback测试PASS；
- 上游HEAD `4e8762e`真实头文件树上GCC14 C++20 `-fsyntax-only -Wall -Wextra -Wsign-compare` exit0、stdout/stderr 0；
- 8项自动测试覆盖异步误判消失、实际可控座位、延迟vehicle/seat/charmer验收、日志、C4018、源码安装器和DBC窄diff；
- 客户端DBC补丁器用真实build-12340 zhCN Spell.dbc（49839行）运行并反向验证：只改110个目标行的AttributesExD bit，输出等长，字符串块及其它记录不变；10个标准始祖幼龙ID全部命中；
- Windows自包含包13文件、31645字节、SHA=`f4eb4e20...e4d8c`，包内哈希/ASCII+CRLF/NativeSafe/旧包不调用/服务器DBC不覆盖门全部PASS；
- 用户真实Windows结果已归档：源码精确Apply、CMake exit0、vcxproj成员PASS、MSBuild exit0、fresh OBJ=1、C4018=0；EXE由`59491e97...b5881`变为`e74d9304...08148`，`G17R1_WINDOWS_BUILD_RESULT=PASS`；
- 同一回传确认客户端DBC输入`df44e75e...d10f`、输出`dd250911...64ea`、110行、10个始祖幼龙、verify PASS、`SERVER_DBC_MODIFIED=False`；但`NOT_INSTALLED_NO_CLIENT_ROOT`，只在staging；
- 新packed-MPQ包13文件、454641字节、SHA=`f59513e9...0e5ec`；绑定mpqcli v0.10.2 Windows PE SHA、无覆盖空槽策略、state精确回滚、PS解析0错误。用真实48956359字节patched DBC完成MPQ create/list/info/extract往返：format2、file count3、抽出SHA仍为`dd250911...64ea`；
- 用户真实Runtime已确认`.dragon summon`和4个载具技能均可使用；异步入座、控制权、原生动作条和4技能正式PASS。R2A后续确认客户端R1安装报告/状态/自有MPQ/有效Spell.dbc全部PASS。
- 59961湿地真实失败，确定性服务端根因是严格分支无条件调用`CanFlyInZone()`；R2只改这一分支。前/后镜像SHA=`537e5c35...e50755`/`73d52ac0...cb9e2`，10项自动测试和C++策略编译PASS。
- R2 Windows包为`G17R2_Pure_Flying_Server_Gate_Windows_20260823.zip`，75105字节、SHA=`e63724d3...c003d`；用户确认`G17R2_WINDOWS_BUILD_RESULT=PASS`。随后湿地未triggered的`.cast self 59961`真实成功召唤上马，完整服务端链PASS。
- R2A真实报告PASS；R3真实Windows安装也PASS，但普通按钮Runtime FAIL、湿地`IsFlyableArea()=nil`。R4在R3相同948行补齐`0x00004000`，保留实时`Player::IsOutdoors()`，当前等待运行唯一R4客户端入口和游戏验收。

### 8.2B G17-B1R1原生参数修复验证

- 旧安装器真实停点为Python路径后/native selftest前；源文件仍是R1前像`10a7002...9b2f45`，确认源码修改0、构建未开始、SQL/客户端修改0；
- 旧函数`[string[]]$Args`在真实PowerShell探针中收到三个原生参数后计数为0，证明与自动变量`$args`冲突；
- 新安装/回滚统一改为`[string[]]$NativeArgs`、`@NativeArgs`和显式`-FilePath/-NativeArgs/-Prefix`调用；
- PowerShell 7.6.5双AST、真实native stdout/stderr/exit、含空格/引号参数、B1 8项测试、source check/apply/幂等/rollback、包级自测、Windows脚本CRLF/无BOM均PASS；
- ZIP路径安全、CRC、14项内文件SHA、解压后包测/双AST与确定性重建均PASS；最终ZIP=32052字节/15文件/SHA `c94394177b188843c7ed79a989fe46f7397233bf9103d2fc622ecf79f4c77cc8`；
- B1R1真实Windows报告随后已回传；本离线包门与真实结果分开记录。

### 8.2C G17-B1R1真实Windows构建接受与B1R2规范修复

- 原始报告3152字节/SHA=`aa5f7521...f7bd7`；native selftest/source apply/MSBuild均exit0，源码精确变为B1后像；
- 明确编译`cs_dragonriding.cpp`、fresh OBJ=1/2197559字节并链接worldserver；
- EXE从`15005e8f...172a9`变为`dc85bd93...a2581`，新EXE=36783616字节；EXE/PDB时间均晚于BUILD_START；
- 最终FAIL是B1R1未在构建前物化`$bp.LastWriteTimeUtc`，构建后首次访问得到新PDB时间并与新`$ap`自比较；接受器逐项复验输出`G17B1R1_WINDOWS_BUILD_ACCEPTANCE=PASS`；
- 不回滚、不重编、不重跑B1R1；规范B1R2原件/后文件已把EXE/PDB前时间立即保存为值类型快照，静态和AST PASS，用户无需执行；
- 此后B1首次Runtime实际FAIL：VehicleBase和charmer正确，但旧verifier因非权威`GetTransSeat()==-1`误清理；已由后续B1R3源码修复取代。

### 8.2D G17-B1R3权威座位占用验证修复

- 精确根因：`GetTransSeat()`是movement transport状态，不能代表`Vehicle::Seats`中的乘员占用；真实失败日志同时证明VehicleBase与charmer已经匹配；
- 新实现遍历`Vehicle::Seats`并调用`GetPassenger(seatId)`取得权威seat；成功要求VehicleBase、权威seat与charmer三重匹配，错误任一项仍fail-closed；
- `.dragon status`输出权威`seat`和独立`movementSeat`，后者只诊断，不决定成功或误清理；
- original/payload SHA=`2c7594d0...68199`/`94ff8033...5628b`；GCC14/C++20零stdout/stderr、B1R3 9/9、B1回归8/8和严格SHA生命周期PASS；
- Windows包双AST、native stdout/stderr/exit/空格/引号、CRLF ASCII无BOM、包级、13文件SHA、ZIP CRC、解压复验与确定性重建PASS；ZIP=29614字节/SHA `69a3cd9cda242012422f775fc44b14cd0807269b4c4182d35ad2ed02decfad70`；
- 用户随后确认地面/飞行坐骑接管、起飞和降落均正常，B1R3权威seat行为PASS；室内不解除是独立后续FAIL。

### 8.2E G17-B1R4真实室内PASS与B1R5包装坐骑/无降落伞修复

- B1R4活动Vehicle 250ms检查已由用户真实验证：进入同一室内会自动退出，`G17B1R4_REAL_INDOOR_RUNTIME=PASS_USER_CONFIRMED`；运行行为证明新二进制部署成功，虽未回传独立build结果文件；
- 用户明确拒绝室内退出附加53208降落伞，并报告军马/始祖龙接管PASS、爱情火箭/无头骑士接管FAIL；
- 只读解析真实Spell.dbc（SHA=`dd250911...64ea`）11条记录，并核对锁定服务端源码：两外层的DBC TriggerSpell均0，C++ `spell_gen_mount`根据骑术/地点施放内层；`SpellMgr`只把外层Effect置NONE而保留ApplyAuraName；
- B1R5候选门读取保留Mounted metadata，所有权锚定已学习外层，活动内层Mounted Aura的`MiscValue`和玩家当前display为模型权威；Runtime源码不写死两坐骑名称或ID；
- 禁区VehicleAI/zone钩均调用`CleanupPlayer(player, false, true)`；unboard不隐式施放降落伞；Vehicle speed/can-fly/gravity归一，退出Vehicle并重算玩家移动速率；
- 高空/边界用100ms、最多200次、检测落地即停的NonVisualFallGuard，只重置fall-damage accounting；无Aura/模型、无缓降、无瞬移、无重力/轨迹/控制改变；
- original/payload SHA=`e9418704...a7059`/`35af002b...668b8`；GCC14/C++20零诊断、DBC 11条、18/18、严格SHA生命周期、B1R3座位/B1R4 250ms合同和移动技能块未改PASS；
- Windows包双AST、native runner、CRLF ASCII、14项内SHA、包级、ZIP CRC/路径安全、解压复验与确定性重建PASS；ZIP=37888字节/15文件/SHA `77cded6bb996945b9ad3230d21d5455eb566c3a7a93429ca79cd26cd9afe9158`；
- 爱情火箭、无头骑士及同一室内无降落伞已由用户真实确认PASS，B1关闭；技能3缺陷随后进入B2并已完成方向/控制修复。

### 8.3 G19 数据库

用户回传的只读结果已通过：

- 四个必需字段存在；
- `npcbot_text` 共179条；
- 54条带场景标签；
- G19 14个目标场景 tag 全部存在，14/14对应台词存在；
- 冷却范围 900–900。

编译、数据库检查、新二进制启动和179条care texts加载均已确认；只剩游戏内场景/冷却验收。

### 8.4 AHBot / G16

源代码取证已确认：

- 跨阵营共享拍卖行三种 house 类型别名到同一 `mNeutralAuctions`；
- seller 采用加权随机 entry，不能保证每 entry 最低库存；
- `ITEM_SUBCLASS_JUNK_PET=2`、`ITEM_SUBCLASS_JUNK_MOUNT=5`，可作为传说例外分类依据；
- 全局 `Orange=0` 会误删允许的传说坐骑/宠物，现有 `Orange=500` 又不能排除传说装备。

用户回传的安装前数据库基线已记录：39,498唯一行、37,307有效、2,191过期；全部属于 AHBot GUID 5 `鲤鱼`（account 2），玩家挂单为0。当前活动覆盖8,267个不同 entry，平均约4.51行/entry；活动橙色为468行、3个 entries，仍待 `14` 号 SQL 分类。用户随后锁定 `MinCopiesPerEntry=10`，口径为有效独立挂单行。

`probe_g16_min_stock.py` 已通过 `py_compile`，并对上游取证提交的六个真实 AuctionHouseBot/AuctionHouse 文件实跑；生成约168 KB UTF-8-BOM/CRLF完整报告，六个 BEGIN/END 源码段齐全，运行前后 Git 状态一致，未修改源码。用户 `D:\TrinityCore` 尚未运行该探针。

### 8.5 F44 `.combo`真实源码确认与本地实施

仓库版三个文件哈希：

```text
cs_combathelper.cpp  2805de88e7a51bd20db1511b8b7e344d5abbe4216aaee357360ca29ad566e72d
CombatSpecData.cpp   2fae4c259e40fe94671efe01428d5e71546128b194ddb0e17f407b3d7266c571
CombatSpecData.h     c01ab4cbe40b12cfd7f4ddf9916998ec43e6ea7bebf8f836f06dd14c4651ca9d
```

`workspace_static_probe_20260821.txt`显示`BEACON_RECAST_ROOT_PATTERN_PRESENT=True`、generation=False、NPCBot heal=False、SELF误目标=True、dispel结果忽略=True、稳定维护flag=False。用户随后回传真实报告并通过：cs/h与上述仓库SHA一致；真实数据cpp为62974字节/1743行/SHA `cd0d172eb3546b36665174ea155bd8a99e5f245bb6ed3059ce7cec3a87c1ebfd`，与仓库不同；所有缺陷标志同样命中。原报告为PowerShell UTF-16LE，已原样及UTF-8标准化双归档；探针修改计数为0。

真实原件已归档，旧仓库生成物的`EXTRA_BUFF`增强已保留。F44后文件锁定值：

```text
cs_combathelper.cpp  5f31a5b97fa0ffe99d1472b370660dc86164ddf3f402bc16b030ee47418f31da
CombatSpecData.h     af3e9c2575b725fa50e7bee921d1f7612546e04b967cb2da149e4fe6a2bdd4a7
CombatSpecData.cpp   1cdd6d3a8b07a4915de60746c04848f00bae2608a9c9cd25982d6f6e6a14272e
```

`tests/run_f44_tests.sh`当时输出`F44_ALL_TESTS=PASS`；随后Windows真实Check/Apply/VS2022编译/启动均PASS，但真人F44-A发现互斥祝福耗材循环和治疗不工作，证明旧本地mock门槛不充分。旧F44不得标记功能PASS，也不得重复旧步骤。

F44R1锁定旧F44三文件为preimage，postimage为：

```text
cs_combathelper.cpp  209cd709d3f649261032a22f54407601dd30953984c23c8464cea6a939c4d4fc
CombatSpecData.h     2f471943f67b4188ddee240114d0b9d6056225f1a81c45e73618098a3b49b74d
CombatSpecData.cpp   d7bc0532ba16f2ead2ff5af81ee12329d70e172a0005afac302fa575cf224e56
```

F44R1安装器/ZIP及本地总回归哈希保持锁定。Windows Check/Apply均PASS；随后VS2022真实增量编译PASS：`cs_combathelper.cpp=True`、`CombatSpecData.cpp=True`，exe=`36734464`字节/SHA `07a8f95259a6d46ab5d46720e5b919fc5cc60f7e81086b81e2748e7d1e1a86c1`，PDB=`433344512`字节。该exe已以PID `16556`真实运行，`F44R1_NEW_BINARY_RUNNING=True`。用户真人定性反馈“没什么太大的问题”并授权转下一步；证据只支持“无重大问题报告”，不支持补写A至D全职业次数、目标和时序。

治疗场景当前值为自动`70/75/30`、任务`60/65/25`、刷材料`65/70/30`、5人副本`75/80/35`、团本`80/85/40`、高级团本`85/90/45`（自奶/队友/救命）。决定不全局抬高，现用游戏内`.buff scene`选择档位；个性阈值与辅助策略见`规划/G23_Lua功能与自动拾取可靠性/03-治疗血线与辅助策略后续增强决策.md`，另立批次。

### 8.6 仓库一致性

- 未执行 commit/push/merge/rebase/reset/clean；
- 未连接或修改真实数据库；
- 未访问或改写用户 Windows 源码和客户端；
- 没有把编译、数据库、部署、启动、游戏验收混写成同一状态；
- `_dt/p`、`_dt/sub`、`_dt2` 的删除是接手时已有工作树状态，未擅自恢复。

---

## 9. 已知状态与“不要重复安装”

以下是已有仓库/用户回报的重要状态，继续工作时必须保留：

- F40 剑圣镜像导致登出闪退：用户此前确认已装；不要重复安装。
- F41 游荡 bot 中立怪/发呆修复：此前已装并验证；不要重复安装。
- F42 招募游荡 bot 闪退修复：此前已装并验证；不要重复安装。
- F43 近战 bot 锁定却不前进修复：此前已装并验证；不要重复安装。
- A42 `PickGiftText` 种族权重修复：此前探针证实已存在；不要重复安装。
- G19 第3步 `11` 号最终修复：完整编译已通过；不要重复运行安装脚本。
- G19 数据库四字段与14条场景台词：已通过；不要再写成“待执行 SQL”。
- G19 `04` 号通用实现：已作废；不要安装。
- G11 真实探针：已完成；只在源码哈希变化时重新探针。
- G11 human最小验收已经28/28/0/0/True通过，且原始字段已由用户确认；**不得再次应用、编译或运行探针**。只保留T1–T10/性能后续。
- G17旧 `01-全世界飞行方案.md` 已禁止直接施工；不得改DBC掩码、将`CanFlyInZone`改恒真或用学三技能冒充御龙术。
- G17 v1、schema-2真实探针和G17-A第17步均已归档并关闭，禁止重复。
- 第18/18R/19A/19B/20A/20B-S及20C-1均已关闭，禁止重复。20C-1只按用户摘要证明主城禁召/飞入暴风城落地/PVP相同；A4和精确A6原始字段保持未提供。
- 旧F44真实探针、原件、Check/Apply、编译和启动均完成且禁止重复；真人F44-A整体FAIL。当前必须走F44R1锁定安装器，禁止手工散改、禁止用旧cpp覆盖增强，也不得在F44R1真人验收前宣称运行PASS。
- AHBot `0/0/250`、Misc=2 配置：用户已确认安装；下一步是查补货后数据，不是重复复制。
- AHBot 旧 `100/100/100`、三行求和口径和 `Class.Misc=8`：均非当前权威方案。

如用户实际文件与这些记录冲突，以用户真实 Git diff、启动日志、数据库和游戏结果为最高证据，但不要在没取证前回滚。

---

## 10. 下一位代理的精确执行顺序

### 步骤1（P0，已完成）：G17-A用户侧self-test与真实源码Check

统一入口第17步完整输出已通过并归档；禁止重复。真实源码仍为锁定前镜像，真实源码/config/数据库修改均为0。

### 步骤2（已执行但最终断言红字）：受控Apply与默认关闭conf

真实Apply、源码备份、exe/PDB备份成功；conf复制及精确SHA检查通过，随后文本行断言红字。禁止重跑原第18步，不要回滚。

### 步骤2R（已PASS并关闭）：第18R步只读恢复验证

用户完整回传已确认postimage、源码备份、两份二进制备份及conf的哈希/BOM/唯一Enable行，`STEP18_RECOVERY_VERIFY_PASS=True`；禁止重复。

### 步骤2A（已PASS并关闭）：第19A步VS2022编译

真实输出已确认`SpellInfo.cpp`、`game.lib`与新worldserver链接链，exe/PDB更新和`STEP19A_BUILD_PASS=True`齐全；禁止重复编译。

### 步骤2B（已PASS并关闭）：第19B默认关闭启动

预/后检、可连接游戏测试、主世界拒飞和外域可飞均通过；不再为已关闭控制台重复启动。原始启动/定位字段转为第20B强制捕获。

### 步骤2C（已PASS并关闭）：第20A启用配置切换

默认关闭备份及Enable=1运行conf精确SHA、无BOM、唯一键均PASS；禁止重复。

### 步骤2D（已PASS并关闭）：第20B-S启用烟雾

map0/map1/map530真实飞行与后检均通过；禁止重复。

### 步骤2E（已按用户摘要关闭）：第20C-1安全边界

用户确认主城内召唤拒绝、外飞入暴风城落地、PVP战场表现相同，并声明执行完成。禁止重复；未提供的A4室内地点/GPS、精确A6世界竞技场和前后检行不补造。

### 步骤2F（已PASS，禁止重复）：F44 `.combo`只读源码探针

真实报告已归档，三个文件SHA/字节/锚点齐全，`SOURCE_OR_CONFIG_EDITS=0`。执行逻辑和头文件一致，数据cpp漂移。

### 步骤2G（已PASS，禁止重复）：真实数据cpp原件

真实`CombatSpecData.cpp`已以62974字节、SHA `cd0d172eb3546b36665174ea155bd8a99e5f245bb6ed3059ce7cec3a87c1ebfd`通过校验并归档；三个真实原件齐全，不再复制或上传。

### 步骤3：G17-A其余A0–A12矩阵（后续另批）

开启后检查东部王国/卡利姆多户外允许，主城/室内/副本/BG/竞技场/NO_FLY拒绝，BlockedAreaIds有效，外域/北裂境与54197规则保留。任何失败先按锁定安装器和conf回滚，不手工散改。

### 步骤4（已完成到用户摘要粒度）：G23-F45安装与编译

P0R Windows报告`g23_p0r_20260822_003402.txt`已以SHA `7eaecd05...f8c8b`PASS并归档；R1–R4计数取消。用户明确确认F45 Check、Apply、VS2022编译成功，并确认正常运行中播报可见、观察到的拾取数量一致。F45转被动观察，禁止重复。

### 步骤5（Windows待安装项）：G23-P3A Lua服务器助手与GM帮助

P2R1用户确认修复成功。P3A独立包=`G23P3A_delivery_20260822.zip`，34756字节，SHA=`fc9d850f...415270`。它仍按10号执行Check→停服→Apply→正常启动，并简短确认`.server`、原生`.server info`、`.gmhelp find pbot`及`.gmhelp core reload`；无需SQL/编译、不reload。未回传前保持`WINDOWS_NOT_RUN`，但不阻塞B0的源码锁定取证。

### 步骤6（当前唯一执行）：G17-B2R1客户端Runtime短验收

1. 不重跑安装CMD；用户真实`G17B2R1_WINDOWS_BUILD_RESULT.txt`已PASS并归档，实际结果文件名/成功标志是`G17B2R1_WINDOWS_BUILD_RESULT=PASS`。
2. 确认旧worldserver进程已退出，启动新生成的`D:\TC-Build\bin\RelWithDebInfo\worldserver.exe`；保持R5有效zhCN Y槽不动，客户端无需安装新MPQ。
3. 完全重启现有R5客户端，在户外空旷处正常召唤任一已学坐骑进入G17动作条；第4格必须显示52226“飞行器着陆”，不是旧53208降落伞动作。
4. Runtime只回A–E五行：技能2四阶段反馈；把普通高速转向与技能3曲线/交还分开观察；魔法/风平姿；龙长斜坡；机械反推或猛兽无火扑落，全部无降落伞并完整恢复控制。
5. 面墙/低空补一项fail-safe；任一失败只报`A/B/C/D/E + 坐骑名 + 现象`。新回归时才执行`02_Rollback_Build_G17B2R1.cmd`；该安全回滚不恢复53208或14475。

当前精确状态：`G17B1_OVERALL_STATUS=CLOSED_PASS`、`G17B2_WINDOWS_DEPLOYMENT=PASS_PROVEN_BY_RUNTIME`、`G17B2_OVERALL_RUNTIME=FAIL_EXPERIENCE_REFINEMENT_REQUIRED`、`G17B2R1_TESTS=PASS_52_OF_52`、`G17B2R1_ZIP_SHA256=94822d393236db850b82972062281ec12cb709b75d70b8f56885b9a018aa8daa`、`G17B2R1_WINDOWS_BUILD=PASS_USER_REPORT`、`G17B2R1_RUNTIME=PENDING_USER`。

B2R1通过后进入B3独立技能页；B3–B6完成后才继续Bot制作优化。

### 步骤7（后续保留）：NPCBot装备、PBot/NPCBot规模、G11/G19/G16

- G17-B0/B1后先执行NPCBot GEAR-P0真实源码/配置/装备池探针，再做只读audit；没有证据前不直接改装备；
- NPCBot场景装备适配当前为`NOT_IMPLEMENTED`，顺序为5人本临时覆盖/恢复/防复制，再到10/25普通/英雄和批量性能；
- 自动职责选择/补缺当前为`NOT_IMPLEMENTED`，必须以真实技能/装备/AI能力而非role标签编成，并尊重玩家锁定和设置战斗备援；
- BOSS人数缩放可玩性当前为`NOT_IMPLEMENTED`，先查实际乘法链，再做有效人数、目标TTK、递减/封顶、防重复和数值安全；
- 超大团队一键组团当前为`NOT_IMPLEMENTED`，GEAR、ROLE和SCALING硬门槛通过后再进入规模线；
- G11 human最小门槛已关闭，只补T1–T10与10/100/500性能；
- G19只做主城/野外/副本与900秒冷却游戏内矩阵；
- G16回传SQL `12`/`14`及 `probe_g16_min_stock.py`，再实现“先补每entry到10条独立活动行+禁止传说装备、允许传说坐骑/宠物”。

---

## 11. 开发与交付约束

1. 中文沟通，给一条明确主线，不把多套互斥方案同时丢给用户。
2. `wpch3/wow-mecode` 是工作成果仓库；`328950225/TrinityCore-NPCBOT-Eluna-zhCN` 是项目实际 TC/NPCBot/Eluna 架构基础，不能遗忘或降格成无关示例。
3. 客户端与服务端都属于项目范围，不得把项目缩成单一服务端配置工程。
4. 涉及用户真实源码时先探针/哈希，后补丁；上游行号只能作 API 和机制参考。
5. 删除/替换代码块必须给精确边界、完整前后块、assert、自动备份和回滚。
6. “搜 X 改 N 处”必须列出哪些命中要改、哪些相似名字绝对不能改。
7. 声明与实现必须成对计数，防止 LNK2019；优先检查第一条真实编译错误。
8. 用户报崩溃先要崩溃日志/调用栈，不靠猜。
9. SQL 默认兼容 DBeaver 只执行当前语句，诊断脚本尽量只读且不依赖会话变量。
10. AHBot 配置总量、类别权重、eligible item 池、每 entry 库存下限和品质过滤是不同层次，不得混为一谈。
11. 共享 AH 模式禁止把 status 三行求和；约99,250总量不能替代 per-entry 和传说过滤验收。
12. 不执行破坏性 Git 命令，不覆盖用户 `D:\TrinityCore`，不自行推送远端。
13. 每完成一批都同步 README、待办总表、未完成想法和三个 handoff 文件。
14. 每批交付除原始源码、配置、SQL和专项说明外，必须同步更新 `tc-bignum/00-当前整体安装步骤_单文件入口.md`；该文件必须包含唯一执行顺序、精确命令、预期成功标志、部署、验收、回滚和完整回传清单。

---

## 12. 当前 Git 事实

- 仓库根：`/home/user/wow-mecode`
- 已知来源仓库：`https://github.com/wpch3/wow-mecode.git`
- 当前本地 `git remote -v`：无输出（Arena 快照不保留 `.git/config`，不要据此误判来源仓库）
- 分支：`main`
- HEAD：`d1ab4bed387dcc2fdbaaea05add9ddd94cbe8769`
- 当前 upstream：未配置
- 当前修改尚未提交；准确文件列表、哈希与 diff 摘要见：
  - `HANDOFF_GIT_STATE.txt`
  - `HANDOFF_FILE_MANIFEST.tsv`（1635项；本次归档Windows报告后最终完整校验`unlisted=0 / missing=0 / bad=0`）

本轮没有提交或推送。下一位代理在用户没有明确要求时，也不要自行 commit/push。

---

## 13. 当前未知项

- 第20C-1消息未单独提供A4明确室内地点/GPS、精确A6世界竞技场、预检/后检原始标志；已完成的主城/PVP现象不得因此要求重复；
- 第20C其余副本/BG/NO_FLY/北裂境/BlockedAreaIds矩阵结果；
- 用户是否已执行`.combo off`、`.buff off`完成旧二进制止损；
- F44R1-A至D全职业结构化数量/时序结果未回传；正确哈希新二进制启动和用户定性烟雾已确认，禁止为了缺失字段重复Check、Apply、编译或启动；
- F45 Check/Apply/VS2022编译及正常使用烟雾均已用户确认；组队roll、背包满、邮件等未回传边界不得补造，也不得据此要求重复；
- G23-P2R1 Check/Apply及修后`.tp`菜单由用户确认通过；未回传的全部分类/页码/安全门不补造。P3A Windows Check/Apply/运行尚未执行；P2原子奖励SQL末尾结果及ObjectVariables历史错误是否消失仍未单独回传；
- R1–R5与B1均已关闭；B2已真实部署且技能3向前/向上与控制恢复PASS，但整体体验FAIL。B2R1真实源码应用、World迁移、MSBuild及新鲜产物门均由用户报告PASS；当前未知仅为新worldserver是否已实际启动及A–E客户端Runtime体验；
- world v2的1267保留为失败历史，v3已经替代并关闭；不得再次运行任一旧B0 world安装/后检/回滚SQL。B2R1统一CMD与安全迁移已真实PASS，禁止重复执行；
- 真正客户端预测/封包级动态飞行仍未完成；B2R1是服务端体验重制，不冒充客户端终局；
- G22-CP0对真实Wow.exe/MPQ/DBC/GlueXML、人物/种族/职业槽和地图工具链的能力结论；
- PBot真实源码中基础命令、autoaccept、roster、更新钩子和注册的当前精确状态；除基础外按未完成处理；40/100/1000一键副本团队固定为未实现；
- PBot/NPCBot真实职责能力接口、玩家锁定和混合候选池仍未知；自动职责选择/补缺固定未实现，不能用现有role位或邀请人数冒充；
- 用户实例中BOSS血量上亿的具体缩放代码链、配置和重复倍率来源仍未知；症状已确认但不得在探针前武断归因。可玩性缩放固定未实现；
- 用户Windows真实NPCBot源码中的`InitEquips/DefaultInit/IsWanderer/TeleportBot/FinishTeleport`是否与上游一致、手工装备和游荡装备的实际持久化状态、5人/团本当前平均装等与缺槽分布；场景装备适配固定为未实现，不能只靠上游推断直接施工；
- G11完整时间戳报告归档、T1–T10和10/100/500性能结果；
- G19游戏内场景/900秒冷却结果；
- AHBot补货后的唯一活动行、entry覆盖与库存分布；
- 当前活动橙色3个entries是否全为允许的传说坐骑/宠物；
- G16最终C++所需用户真实seller源码哈希/锚点；
- 服务器满足每entry下限后的性能和容量上限。

这些都必须写成“未确认”，不能从编译成功、总行数、旧源码快照或仓库文件存在推断。

---

## 14. 关键文件速查

```text
PROJECT_HANDOFF.md
HANDOFF_GIT_STATE.txt
HANDOFF_FILE_MANIFEST.tsv
F44R1_delivery_20260821.zip

tc-bignum/README.md
tc-bignum/00-当前整体安装步骤_单文件入口.md
tc-bignum/待办总表.md
tc-bignum/未完成想法-总清单.md

tc-bignum/规划/G19_情境对话系统/12-第3步编译通过后_部署与验收.md

tc-bignum/规划/G11_bot自主冒险/06-第2步_感知层实施规格.md
tc-bignum/规划/G11_bot自主冒险/07-第2步_装机前探针.md
tc-bignum/规划/G11_bot自主冒险/probe_g11_step2.py
tc-bignum/规划/G11_bot自主冒险/08-第2步_基于真实源码的安装与验收.md
tc-bignum/规划/G11_bot自主冒险/install_g11_step2.py
tc-bignum/规划/G11_bot自主冒险/g11_step2_windows.cmd
tc-bignum/规划/G11_bot自主冒险/09-配置已加载但无感知日志_只读取证.md
tc-bignum/规划/G11_bot自主冒险/probe_g11_runtime_v2.py
tc-bignum/规划/G11_bot自主冒险/10-运行探针v1格式缺陷_复盘与v2自检.md
tc-bignum/规划/G11_bot自主冒险/11-v2真实报告首次结果_新进程短窗口无日志.md
tc-bignum/规划/G11_bot自主冒险/probe_g11_runtime.ps1  # fail-closed deprecated stub
tc-bignum/conf/worldserver.conf.d/g11_perception.conf

tc-bignum/规划/G17_飞行与移动/01-全世界飞行方案.md  # 历史取证，禁止施工
tc-bignum/规划/G17_飞行与移动/02-重新审计_全世界飞行与御龙术分阶段实施计划.md
tc-bignum/规划/G17_飞行与移动/03-v1真实报告诊断与v2探针更正.md
tc-bignum/规划/G17_飞行与移动/证据/g17_source_probe_v1_20260821_123340.txt
tc-bignum/规划/G17_飞行与移动/证据/g17_source_probe_v2_20260821_125214.txt
tc-bignum/规划/G17_飞行与移动/证据/g17a_step17_selftest_check_pass_20260821.txt
tc-bignum/规划/G17_飞行与移动/证据/g17a_step18_apply_conf_validator_failure_20260821_151724.txt
tc-bignum/规划/G17_飞行与移动/证据/g17a_step18r_recovery_verify_pass_20260821.txt
tc-bignum/规划/G17_飞行与移动/证据/g17a_step19a_build_pass_20260821.txt
tc-bignum/规划/G17_飞行与移动/证据/g17a_step19b_partial_pre_post_behavior_20260821.txt
tc-bignum/规划/G17_飞行与移动/证据/g17a_step19b_default_off_acceptance_20260821.txt
tc-bignum/规划/G17_飞行与移动/证据/g17a_area_update_aura_recheck_source_audit_20260821.txt
tc-bignum/规划/G17_飞行与移动/证据/g17a_step20a_enable_conf_pass_20260821_164848.txt
tc-bignum/规划/G17_飞行与移动/证据/g17a_step20bs_enable_smoke_pass_20260821.txt
tc-bignum/规划/G17_飞行与移动/证据/g17a_step20c1_user_summary_pass_20260821.txt
tc-bignum/规划/G17_飞行与移动/probe_g17_source.py  # schema-2已完成，禁止重复
tc-bignum/规划/G17_飞行与移动/G17A_安全全世界飞行/  # 已归档完整交付包
tc-bignum/conf/worldserver.conf.d/g17_world_flight.conf
tc-bignum/规划/G17_飞行与移动/G17B0_Narrow_Probe_20260822.zip  # 已完成，禁止重复
tc-bignum/规划/G17_飞行与移动/证据/G17B0_LOCK_RESULT_20260822_120243.zip  # 用户真实原包
tc-bignum/规划/G17_飞行与移动/证据/g17b0_windows_lock_acceptance_20260822.md
tc-bignum/规划/G17_飞行与移动/证据/G17B0_SOURCE_PREFLIGHT_RESULT_20260822.txt  # 用户真实源码预检PASS，禁止重复CMD
tc-bignum/规划/G17_飞行与移动/G17B0_Preflight_20260822.zip  # 源码预检历史冻结包；禁止重复
tc-bignum/规划/G17_飞行与移动/G17B0_world_probe_readonly_v2_collation_safe.sql  # 已执行PASS，禁止重复
tc-bignum/规划/G17_飞行与移动/证据/G17B0_DB_PROBE_RESULT_20260822_1340.txt
tc-bignum/规划/G17_飞行与移动/证据/g17b0_db_probe_v2_acceptance_20260822.md
tc-bignum/规划/G17_飞行与移动/accept_g17b0_db_probe.py
tc-bignum/规划/G17_飞行与移动/G17B0_Source_Apply_20260822.zip  # 已执行PASS；禁止重复
tc-bignum/规划/G17_飞行与移动/G17B0_源码受控Apply包_20260822/
tc-bignum/规划/G17_飞行与移动/证据/g17b0_source_apply_delivery_20260822.txt
tc-bignum/规划/G17_飞行与移动/证据/G17B0_SOURCE_APPLY_RESULT_20260822.txt
tc-bignum/规划/G17_飞行与移动/证据/g17b0_source_apply_acceptance_20260822.md
tc-bignum/规划/G17_飞行与移动/accept_g17b0_source_apply.py
tc-bignum/规划/G17_飞行与移动/G17B0_World_Install_20260822.zip  # v2真实1267失败；禁止重跑
tc-bignum/规划/G17_飞行与移动/G17B0_World_Install_20260822/
tc-bignum/规划/G17_飞行与移动/证据/g17b0_world_install_delivery_20260822.txt
tc-bignum/规划/G17_飞行与移动/证据/G17B0_WORLD_INSTALL_V2_ERROR_1267_20260822.txt
tc-bignum/规划/G17_飞行与移动/G17B0_World_Install_v3_20260822.zip  # 真实写后检查PASS；禁止重复
tc-bignum/规划/G17_飞行与移动/G17B0_World_Install_v3_20260822/
tc-bignum/规划/G17_飞行与移动/证据/g17b0_world_install_v3_delivery_20260822.txt
tc-bignum/规划/G17_飞行与移动/证据/G17B0_WORLD_POSTCHECK_RESULT_20260822_1520.txt
tc-bignum/规划/G17_飞行与移动/证据/g17b0_world_postcheck_v3_acceptance_20260822.md
tc-bignum/规划/G17_飞行与移动/accept_g17b0_world_postcheck_v3.py
tc-bignum/规划/G17_飞行与移动/G17B0_Windows_Build_20260822.zip  # PS1 ParserError；未执行构建，禁止重跑
tc-bignum/规划/G17_飞行与移动/G17B0_Windows_Build_20260822/
tc-bignum/规划/G17_飞行与移动/证据/g17b0_windows_build_delivery_20260822.txt
tc-bignum/规划/G17_飞行与移动/证据/G17B0_WINDOWS_BUILD_V1_PARSER_ERROR_20260822.txt
tc-bignum/规划/G17_飞行与移动/G17B0_Windows_Build_v2_ASCII_20260822.zip  # native stderr提前catch；MSBuild未开始，禁止重跑
tc-bignum/规划/G17_飞行与移动/G17B0_Windows_Build_v2_ASCII_20260822/
tc-bignum/规划/G17_飞行与移动/证据/g17b0_windows_build_v2_ascii_delivery_20260822.txt
tc-bignum/规划/G17_飞行与移动/证据/G17B0_WINDOWS_BUILD_V2_RESULT_20260822.txt
tc-bignum/规划/G17_飞行与移动/证据/G17B0_WINDOWS_BUILD_V2_NATIVE_STDERR_ABORT_20260822.txt
tc-bignum/规划/G17_飞行与移动/G17B0_Windows_Build_v3_ASCII_NativeSafe_20260822.zip  # 真实Windows构建PASS；禁止重复
tc-bignum/规划/G17_飞行与移动/G17B0_Windows_Build_v3_ASCII_NativeSafe_20260822/
tc-bignum/规划/G17_飞行与移动/证据/g17b0_windows_build_v3_native_safe_delivery_20260822.txt
tc-bignum/规划/G17_飞行与移动/证据/G17B0_WINDOWS_BUILD_V3_RESULT_20260822.txt
tc-bignum/规划/G17_飞行与移动/证据/g17b0_windows_build_v3_acceptance_20260822.md
tc-bignum/规划/G17_飞行与移动/accept_g17b0_windows_build_v3.py
tc-bignum/规划/G17_飞行与移动/G17B0_预检包_20260822/
tc-bignum/补丁库/01_功能/G17B0_原生御龙载具/00-开发状态与设计.md
tc-bignum/补丁库/01_功能/G17B0_原生御龙载具/01-Windows前像结论与预检步骤.md
tc-bignum/补丁库/01_功能/G17B0_原生御龙载具/install_g17b0_source.py
tc-bignum/补丁库/01_功能/G17B0_原生御龙载具/payload/src/server/scripts/Commands/cs_dragonriding.cpp
tc-bignum/补丁库/01_功能/G17B0_原生御龙载具/sql/
tc-bignum/补丁库/01_功能/G17B0_原生御龙载具/tests/test_g17b0_static.py
tc-bignum/补丁库/01_功能/G17B0_原生御龙载具/tests/test_g17b0_world_apply_v2.py  # v2离线历史测试；真实1267已否决交付
tc-bignum/补丁库/01_功能/G17B0_原生御龙载具/tests/test_g17b0_world_apply_v3.py
tc-bignum/补丁库/01_功能/G17B0_原生御龙载具/tests/test_g17b0_windows_build_package.py  # v1历史不足测试
tc-bignum/补丁库/01_功能/G17B0_原生御龙载具/tests/test_g17b0_windows_build_v2_ascii_package.py  # v2历史不足测试
tc-bignum/补丁库/01_功能/G17B0_原生御龙载具/tests/test_g17b0_windows_build_v3_native_safe_package.py

tc-bignum/补丁库/02_修复/G17R1_御龙入座与纯飞行坐骑/  # 当前R1原件/后像/源码安装器/客户端DBC补丁器/测试
tc-bignum/规划/G17_飞行与移动/G17R1_Runtime_Fix_Windows_20260822.zip  # 用户Windows构建PASS，禁止重复
tc-bignum/规划/G17_飞行与移动/G17R1_Runtime_Fix_Windows_20260822/
tc-bignum/规划/G17_飞行与移动/G17R1_Client_MPQ_Install_Windows_20260822.zip  # 历史客户端包；禁止重复
tc-bignum/规划/G17_飞行与移动/G17R1_Client_MPQ_Install_Windows_20260822/
tc-bignum/规划/G17_飞行与移动/证据/G17R1_WINDOWS_AND_CLIENT_STAGE_RESULT_20260822.txt
tc-bignum/规划/G17_飞行与移动/证据/g17r1_windows_build_client_stage_acceptance_20260822.md
tc-bignum/规划/G17_飞行与移动/证据/g17r1_client_mpq_install_delivery_20260822.txt
tc-bignum/规划/G17_飞行与移动/证据/g17r1_linux_syntax_warning_compile_20260822.txt
tc-bignum/规划/G17_飞行与移动/证据/g17r1_real_zhcn_spell_dbc_patcher_validation_20260822.txt
tc-bignum/规划/G17_飞行与移动/证据/g17r1_windows_package_selftest_20260822.txt
tc-bignum/规划/G17_飞行与移动/证据/g17r1_runtime_fix_delivery_20260822.txt
tc-bignum/规划/G17_飞行与移动/证据/G17R5_LOCALE_MIRROR_RESULT_20260823.txt
tc-bignum/规划/G17_飞行与移动/证据/g17r5_real_runtime_pass_20260823.md
tc-bignum/补丁库/01_功能/G17B1_全坐骑自动接管与类型会话/
tc-bignum/规划/G17_飞行与移动/G17B1_All_Mounts_Auto_Intercept_Windows_20260823.zip  # 旧native runner卡死；永久废弃
tc-bignum/规划/G17_飞行与移动/证据/g17b1_windows_native_runner_hang_20260823.md
tc-bignum/补丁库/02_修复/G17B1R1_PowerShell原生参数卡死修复/
tc-bignum/规划/G17_飞行与移动/G17B1R1_Native_Runner_Fix_Windows_20260823.zip  # 已执行；最终时间戳断言假FAIL；禁止重跑
tc-bignum/规划/G17_飞行与移动/证据/G17B1R1_WINDOWS_BUILD_RESULT_20260823.txt
tc-bignum/规划/G17_飞行与移动/证据/g17b1r1_windows_build_accepted_false_pdb_timestamp_assertion_20260823.md
tc-bignum/规划/G17_飞行与移动/accept_g17b1r1_false_timestamp_failure.py
tc-bignum/补丁库/02_修复/G17B1R2_PDB前时间戳快照假失败修复/  # 规范脚本修正；用户无需执行
tc-bignum/补丁库/02_修复/G17B1R3_权威座位占用验证与误清理修复/
tc-bignum/规划/G17_飞行与移动/G17B1R3_Authoritative_Seat_Verification_Windows_20260823.zip  # 已执行；座位行为PASS；禁止重复
tc-bignum/规划/G17_飞行与移动/证据/g17b1_runtime_authoritative_seat_root_cause_20260823.md
tc-bignum/规划/G17_飞行与移动/证据/g17b1r3_zip_extract_validation_20260823.txt
tc-bignum/规划/G17_飞行与移动/证据/g17b1r3_authoritative_seat_fix_delivery_20260823.md
tc-bignum/补丁库/02_修复/G17B1R4_室内实时安全清理修复/
tc-bignum/规划/G17_飞行与移动/G17B1R4_Continuous_Indoor_Safety_Windows_20260823.zip  # 已执行；室内退出PASS；禁止重复
tc-bignum/规划/G17_飞行与移动/证据/g17b1r4_indoor_cleanup_root_cause_20260823.md
tc-bignum/规划/G17_飞行与移动/证据/g17b1r4_zip_extract_validation_20260823.txt
tc-bignum/规划/G17_飞行与移动/证据/g17b1r4_continuous_indoor_safety_delivery_20260823.md
tc-bignum/补丁库/02_修复/G17B1R5_包装坐骑与无降落伞清理修复/
tc-bignum/规划/G17_飞行与移动/G17B1R5_Wrapper_Mount_No_Parachute_Windows_20260823.zip  # 已执行PASS；B1关闭；禁止重复
tc-bignum/规划/G17_飞行与移动/证据/g17b1r5_wrapper_and_no_parachute_delivery_20260823.md
tc-bignum/规划/G17_飞行与移动/证据/g17b1r5_zip_extract_validation_20260823.txt
tc-bignum/补丁库/02_修复/G17B2_完整御空术与特色降落/  # 已真实部署；整体体验FAIL；禁止重跑
tc-bignum/补丁库/02_修复/G17B2R1_三技能体验重制/  # 当前源码/SQL/安全回滚/52项测试权威目录
tc-bignum/规划/G17_飞行与移动/G17B2R1_Runtime_Experience_Rework_Windows_20260824.zip  # 已真实执行构建PASS；禁止重跑；73526字节；SHA 94822d39...8daa
tc-bignum/规划/G17_飞行与移动/证据/G17B2R1_WINDOWS_BUILD_RESULT_20260824.txt  # 用户原始Windows PASS证据

tc-bignum/规划/G22_客户端魔改整合/02-完美客户端总纲_模型城市种族职业世界主线.md
tc-bignum/补丁库/02_修复/F44_combo道标与全职业目标策略/00-调查结论与修复设计.md
tc-bignum/补丁库/02_修复/F44_combo道标与全职业目标策略/04-F44安装编译验收与回滚.md
tc-bignum/补丁库/02_修复/F44_combo道标与全职业目标策略/install_f44_combo.py
tc-bignum/补丁库/02_修复/F44_combo道标与全职业目标策略/ORIGINALS/
tc-bignum/补丁库/02_修复/F44_combo道标与全职业目标策略/源文件/
tc-bignum/补丁库/02_修复/F44_combo道标与全职业目标策略/tools/
tc-bignum/补丁库/02_修复/F44_combo道标与全职业目标策略/tests/run_f44_tests.sh
tc-bignum/补丁库/02_修复/F44_combo道标与全职业目标策略/证据/actual_source_probe_20260821_184147_utf16le_original.txt
tc-bignum/补丁库/02_修复/F44_combo道标与全职业目标策略/证据/actual_source_probe_20260821_184147_utf8_normalized.txt

tc-bignum/补丁库/02_修复/F44R1_combo增益生命周期与治疗优先/README.md
tc-bignum/补丁库/02_修复/F44R1_combo增益生命周期与治疗优先/01-真人失败根因与全职业审计.md
tc-bignum/补丁库/02_修复/F44R1_combo增益生命周期与治疗优先/02-安装编译与真人验收.md
tc-bignum/补丁库/02_修复/F44R1_combo增益生命周期与治疗优先/install_f44r1_combo.py
tc-bignum/补丁库/02_修复/F44R1_combo增益生命周期与治疗优先/原始文件/F44_真人缺陷版/
tc-bignum/补丁库/02_修复/F44R1_combo增益生命周期与治疗优先/源文件/
tc-bignum/补丁库/02_修复/F44R1_combo增益生命周期与治疗优先/工具/
tc-bignum/补丁库/02_修复/F44R1_combo增益生命周期与治疗优先/tests/run_f44r1_tests.sh
tc-bignum/补丁库/02_修复/F44R1_combo增益生命周期与治疗优先/证据/f44r1_local_regression_pass_20260821.txt
tc-bignum/补丁库/02_修复/F44R1_combo增益生命周期与治疗优先/证据/f44r1_windows_check_pass_20260821.txt
tc-bignum/补丁库/02_修复/F44R1_combo增益生命周期与治疗优先/证据/f44r1_windows_apply_postcheck_pass_20260821.txt
tc-bignum/补丁库/02_修复/F44R1_combo增益生命周期与治疗优先/证据/f44r1_windows_build_pass_20260821.txt
tc-bignum/补丁库/02_修复/F44R1_combo增益生命周期与治疗优先/证据/f44r1_new_binary_runtime_and_user_smoke_20260821.txt
tc-bignum/规划/G23_Lua功能与自动拾取可靠性/01-仓库初审与分阶段实施计划.md
tc-bignum/规划/G23_Lua功能与自动拾取可靠性/02-G23-P0真实基线执行入口.md  # v1已关闭
tc-bignum/规划/G23_Lua功能与自动拾取可靠性/03-治疗血线与辅助策略后续增强决策.md
tc-bignum/规划/G23_Lua功能与自动拾取可靠性/04-G23-P0首次报告分析与症状归因边界.md
tc-bignum/规划/G23_Lua功能与自动拾取可靠性/05-G23-P0R真实路径与Eluna扩展补充取证.md  # 已PASS关闭
tc-bignum/规划/G23_Lua功能与自动拾取可靠性/06-G23-P0R结论与最小复现矩阵.md  # 矩阵已取消，历史根因文档
tc-bignum/规划/G23_Lua功能与自动拾取可靠性/07-G23-F45修复包与Windows实施.md  # F45关闭记录
tc-bignum/规划/G23_Lua功能与自动拾取可靠性/08-G23-P2-Lua稳定性独立交付与Windows实施.md  # P2实施及缺陷记录
tc-bignum/规划/G23_Lua功能与自动拾取可靠性/09-G23-P2R1-tp菜单跨state假超时热修.md  # 已关闭
tc-bignum/规划/G23_Lua功能与自动拾取可靠性/10-G23-P3A-Lua服务器助手与GM帮助完善.md  # 当前入口
tc-bignum/补丁库/01_功能/G23P3A_Lua服务器助手与GM帮助完善/  # P3A原件/后像/安装/回滚
tc-bignum/补丁库/02_修复/F45_AoE拾取随机漏尸与组队金币语义/  # F45原件/后像/安装/回滚
tc-bignum/补丁库/02_修复/G23P2_Lua稳定性与安全门/  # 冻结P2原包目录
tc-bignum/补丁库/02_修复/G23P2R1_tp菜单跨state会话修复/  # P2R1原件/后像/安装/回滚
tc-bignum/规划/G23_Lua功能与自动拾取可靠性/probe_g23_baseline.py  # v1已关闭
tc-bignum/规划/G23_Lua功能与自动拾取可靠性/probe_g23_p0r_actual_paths.py  # 已PASS关闭
tc-bignum/规划/G23_Lua功能与自动拾取可靠性/证据/g23_baseline_20260822_001210.txt
tc-bignum/规划/G23_Lua功能与自动拾取可靠性/证据/g23_p0r_20260822_003402.txt
tc-bignum/tools/specdata_v3_base.json
tc-bignum/tools/gen_specdata_v3.py

tc-bignum/规划/04-PBot与NPCBot真实完成度及后续路线_2026-08-21.md

tc-bignum/conf/worldserver.conf.d/ahbot.conf
tc-bignum/规划/G16_bot经济与打工/11-共享拍卖行只有37500条_根因与十万挂单修复.md
tc-bignum/规划/G16_bot经济与打工/12-共享拍卖行_只读诊断.sql
tc-bignum/规划/G16_bot经济与打工/13-每种物品多库存与传说过滤_新增硬需求.md
tc-bignum/规划/G16_bot经济与打工/14-拍卖行每物品库存与传说过滤_只读诊断.sql
tc-bignum/规划/G16_bot经济与打工/15-每entry十条与传说过滤_源码探针和回传清单.md
tc-bignum/规划/G16_bot经济与打工/probe_g16_min_stock.py
```

---

## 15. 交接结论

G11同一PID 40秒结果为28总/28 human/0 PBot/0缺字段/PASS=True，且用户确认所有human行均含 `kind=human`、`readonly=1`；human最小验收完成，不重复安装、编译或探针。完整T1–T10/性能仍是后续待办。

G17-A的schema-2、第17/18/18R/19A/19B/20A/20B-S均已通过并归档；第20C-1也已按用户真实摘要关闭，禁止重复主城/PVP测试。其余矩阵继续分批。安全一期主城落地不是最终飞行上限：G22已经把官方12.1持续对标和改造完成后的真正全世界/城市/新世界动态飞行写入权威总纲。

F44R1 `.combo`正确哈希新二进制已以PID 16556运行，用户真人定性反馈“没什么太大的问题”并授权转下一步；A至D全职业结构化矩阵未回传且不补造。默认治疗血线保持按场景递进，现用`.buff scene`；个性血线和其它辅助策略另立批次。

F45正常并转被动观察；P2R1已由用户确认修复成功。G23-P3A `.server`助手与153条/11类新版`.gmhelp`已通过本地/ZIP门槛，仍待Windows Check→停服→Apply→正常启动，无SQL/编译且禁止reload。

当前开发主线已进入G17-B2R1客户端Runtime。B1已关闭；B2已真实部署且技能3向前/向上和控制恢复PASS，但技能2反馈、高速转向与技能4类型化着陆体验FAIL。B2R1四阶段反馈、七节点曲线/平滑交还、52226安全动作及五类分段着陆已通过双严格编译、52/52、未知SHA零写入拒绝、World三状态模型、双AST、18文件包和ZIP解压门。用户真实B2R1构建报告已证明源码应用、World迁移、MSBuild、新鲜OBJ/EXE/PDB和新EXE哈希PASS；统一CMD禁止重跑。当前唯一动作是启动新worldserver并回A–E五行客户端体验。B3–B6仍未实现。

NPCBot场景装备、职责自动选择/补缺和BOSS人数缩放可玩性分别固定为`NPCBOT_CONTEXT_GEAR_ADAPTATION=NOT_IMPLEMENTED`、`BOT_ROLE_AUTO_ASSIGN_AND_FILL=NOT_IMPLEMENTED`、`BOT_INSTANCE_SCALING_CONTROL=NOT_IMPLEMENTED`；GEAR-P0–P5、ROLE能力编成/备援及SCALING有效人数/递减/TTK/防重复/数值安全均已加入04号权威路线。PBot/NPCBot超大团队入口固定为`BOT_MASS_RAID_ONE_CLICK=NOT_IMPLEMENTED`，需在上述硬门槛后推进，且不得提前打断御龙术。G19只剩游戏内场景/冷却验收，不重跑SQL。G16约99,250总量不算完成，仍必须实现每个eligible entry至少10条独立活动拍卖、禁止传说装备并保留传说坐骑/宠物例外。
