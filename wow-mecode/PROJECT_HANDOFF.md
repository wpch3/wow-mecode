# PROJECT_HANDOFF.md · 魔兽世界 3.3.5a 客户端与 TrinityCore 服务端魔改项目交接

> 更新日期：2026-08-27（Asia/Shanghai）
> 当前工作仓库：`https://github.com/wpch3/wow-mecode`（origin 已配置）
> 当前本地分支 / HEAD：`arena/01a03878-wow-mecode` / `e9716455ae47cc938e3ec60674e4264a9bd414c9`
> 上游参考仓库：`https://github.com/328950225/TrinityCore-NPCBOT-Eluna-zhCN`（分支 `NPCBOT-Eluna-zhCN-2026`，取证提交 `4e8762ee`）
> 用户真实编译源码：`D:\TrinityCore`（分支 `bignum-mod`，探针 HEAD `ae60f5b6...`）＋ 编译目录 `D:\TC-Build` ＋ 客户端 `D:\WOW`
> 本文件与 `HANDOFF_FILE_MANIFEST.tsv`、`HANDOFF_GIT_STATE.txt` 为同批三件套；本目录（仓库内 `wow-mecode/`）副本为权威副本。

---

## 0. 铁律区（永久生效，任何代理开工前必读）

### 0.1 查找资料 ＋ 完整对照 ＋ 深度学习（2026-08-27 用户明令写入，最高优先级）

用户原话：**"一定要记得你要查找资料以及完整的对照，深度学习"**。

执行细则（每一条都是硬性要求，不是建议）：

1. **禁止凭名字、直觉或记忆猜测任何游戏事实。** 法术视觉 ID、DBC 字段语义、客户端渲染行为、API 签名、表结构、封包格式——凡是不能 100% 确定的，必须先查权威资料，再动手写代码或补丁。
2. **权威资料源分层（按优先级）：**
   - **法术特效/法术数据**：Wowhead WotLK 数据库（`wowhead.com/wotlk`、`wotlk.evowow.com`）逐条核对真实法术的 SpellVisual、Effect、Range；再用本地客户端 DBC 复核该视觉 ID 真实存在。**每个视觉 ID 都要能回答"来自哪个真实法术"。**
   - **客户端 UI 行为**：用 G17Extract 提取的 282 个 FrameXML 源文件（仓库根 `G17_extracted/Interface/FrameXML/`）逐行对照。VehicleMenuBar.lua、MainMenuBar.lua、BonusActionBarFrame.lua、SecureTemplates.lua 都是现成证据，禁止猜 UI 逻辑。
   - **服务端机制**：对照 fork 真实源码（上游仓库 `328950225/TrinityCore-NPCBOT-Eluna-zhCN` 或重新稀疏克隆；旧的 `/tmp/tcsparse` 是临时目录、本会话已不存在，行号引用以本文 §3.3 记录为准）。真实签名、真实行号、真实表结构，不靠记忆。
3. **"完整的对照"＝逐字段全量对照，不是抽查。** 例如 Spell.dbc 是 234 字段结构，改一条法术时 Effect_1／School／SpellVisualID／RangeIndex／RecoveryTime／CategoryRecoveryTime／ImplicitTargetA_1 要逐列核对，不能只看一列就对全部 25 条批量写。
4. **"深度学习"＝钻到底层机制再下结论。** 客户端渲染规则（如"只有 SCHOOL_DAMAGE 效果才渲染视觉"）、封包（SMSG_PET_SPELLS／SMSG_SPELL_COOLDOWN 谁触发什么）、DBC 列语义（RecoveryTime 在按下按钮时客户端本地读）、UI 源码切换链（MainMenuBar_ToVehicleArt）——用完整证据链得出结论，不满足于表面现象修复。
5. **交付前自检：每个引用的外部事实必须能标注来源**（Wowhead 页面／FrameXML 文件:行号／fork 源码 文件:行号／DBC 列号）。标注不出来＝没查过＝不许写进补丁。

**起源案例（为什么立这条铁律）——C8 特效灾难：**
C8 批次给 25 个战斗技能分配视觉 ID 时**凭技能名字猜**，没有去 Wowhead 逐条核对真实法术。用户实测反馈：一技能"龙息·烈焰"显示绿色旋风、二技能"尾扫·裂地"也是同一个绿色旋风、三技能完全无特效；外加 DBC RecoveryTime 未清导致"施放被阻断仍然出现冷却转圈"。整批返工。C9 v2 改为 Wowhead 逐条验证的真实 Boss／玩家法术视觉（龙类 5 技全部来自可点名源法术：龙息术 42950、奥妮克希亚扫尾 68867、寒冰护体 11426、飞翼打击 31475、冲击新星 30616）才修复。**此类返工不允许再发生。**

### 0.3 路线铁律（2026-08-27 用户最终裁定，最高优先级）

用户原话："我从来没说过需要ui插件，我从始至终都说的是魔改客户端和服务端"。

1. **御龙术及一切功能只走两条路：客户端魔改（MPQ 链下发的文件级修改：DBC／FrameXML／Lua／XML，未来含模型等）＋服务端魔改（D:\TrinityCore C++ 源码）。**
2. **禁止再交付任何插件（AddOns）路线**——用户从未要求过。插件只出现在受限环境验证或一次性诊断场景，且必须事先经用户明确同意。
3. 已交付的三个插件包（G17CombatBar v4、G17DragonBar v5/v6、G17DragonRide）**一律作废归档**，不安装、不维护、不基于其继续开发。B3R9（玩家施法路径）保留为服务端能力（供未来客户端魔改调用），不再是插件配套。
4. 客户端魔改的标准流水线（C11 已闭环验证）：**G17Extract 提取 → 最小字节差异修改 → patch-Z.MPQ 链下发 → 四文件回读校验 → 幂等/回滚**。G22 深度客户端魔改全线复用。
5. 下一位代理若再提出"做个插件"类方案，视为方向错误，必须先回到本条。
6. **错误路线/失败包已物理归档**（2026-08-27 清理）：`G17_飞行与移动/归档_废弃_20260827/`＝G17CombatBar_v4、G17DragonBar_v6、G17DragonRide（插件）＋G17C8（视觉猜错）＋G17C9v1v2（安装器五缺陷）＋4 个对应 ZIP＋归档说明。C10 已装包原样保留（其内 addon 废弃勿装）；G17Diag/G17ChatLog（诊断）与 G17Extract/G17VisualDB（魔改流水线/铁律工具）保留；服务端 B0-R9 谱系原地保留。

### 0.2 既有铁律（沿用，违者同样返工）

1. 中文沟通；给用户**一条**明确主线；修复要直接可执行，不把多套互斥方案同时丢给用户。
2. `wpch3/wow-mecode` 是成果同步仓库；`328950225/TrinityCore-NPCBOT-Eluna-zhCN` 是项目实际 TC/NPCBot/Eluna 架构基础，不可降格成无关示例。
3. 客户端与服务端都属于项目范围，不得把项目缩成单一服务端工程。
4. 涉及用户真实源码先探针/哈希、后补丁；上游行号只作 API 与机制参考。
5. **服务端源码改动的交付合同（B3 系列起固化）：** 每包必须含 original/payload/rollback_safe 三镜像 ＋ 锁定谱系 SHA256 校验（未知现场状态一律零写入拒绝）＋ 取证备份 ＋ Windows MSBuild 编译证明；ZIP 内含安装/回滚 CMD（用户双击）＋ PS1 安装器 ＋ SHA256SUMS。用户期待 PASS/FAIL 结果文件自动写入 `uploads/`。
6. "搜 X 改 N 处"必须列出哪些命中要改、哪些相似名绝对不能改。
7. 声明与实现成对计数防 LNK2019；优先看第一条真实编译错误。
8. 用户报崩溃先要日志/调用栈，不靠猜。
9. 不执行破坏性 Git 命令；不覆盖用户 `D:\TrinityCore`；不擅自改数据库/客户端而无备份与回滚。
10. 每完成一批同步：README、待办总表、未完成想法、`tc-bignum/00-当前整体安装步骤_单文件入口.md`、三个 handoff 文件（本三件套）。
11. 状态口径分层（§2）：**存在补丁 ≠ 已安装 ≠ 已编译 ≠ 已部署 ≠ 游戏内验证**。不得把规划写成完成。

---

## 1. 给下一位代理的 30 秒摘要（2026-08-27 真实状态）

1. **当前主线：用户再次纠正方向——要的是客户端魔改本体（不是插件）。已照办：C11 真正的客户端魔改（修改客户端界面源码 VehicleMenuBar.lua/xml 经 MPQ 链下发，原版载具条 6→8 格），23/23 自检，待用户验收。** G17DragonRide 插件界面（B3R9 配套）转为可选方案。此前：C9 v3 特效 PASS、冷却 UI 生效、B3R8 安装 PASS。**G22 深度客户端魔改能力就此闭环证明：任意客户端 UI 文件可提取→修改→MPQ 下发→回滚。**
2. **御龙术架构已成型**：NPC 1000171 御空龙·B0（VehicleId 70、80 级、75000 HP、能量 100/100），5 种坐骑原型（龙/兽/法/机/通）×5 战斗槽＝25 技能（990000–990024）＋4 载体（990025 切页/990026 拉升/990027 俯冲/990028 制动）；载具动作条双页切换（移动页 6 键↔战斗页 6 键）；能量循环（俯冲+15/生成器+8/被动+1每2秒）；骑手交战时龙每 4.5 秒真实施放生成器（带完整施法动画与弹道）；冷却双门（载具＋玩家 SpellHistory）。
3. **3.3.5 客户端硬限制已全部摸清并有证据**（§4.3）：载具条最多 6 按钮（VehicleMenuBar.lua 第 6 行硬编码）；`LearnSpell` 只进服务端、客户端全部 IsKnown=false（玩家技能面板路线 B3R3 已死）；DUMMY 效果不渲染视觉（必须 SCHOOL_DAMAGE）；DBC RecoveryTime 造成"按了就转圈"的幻影冷却（C9 已清零，冷却完全由服务端 SMSG_SPELL_COOLDOWN 驱动）；上马卡顿根因是登乘时 40+ 封包（B3R6 已移除 LearnSpell/RemoveSpell）。
4. **冷却 UI 修复已生效**（用户确认"有冷却"）：服务端 SMSG_SPELL_COOLDOWN 双 GUID 变体（B3R7 起）＋插件 CLEU 施法成功跟踪，互为保险且无幻影。
5. **第 7 格问题最终结论＋方向性转变**：B3R8 安装 PASS 但原版条仍只显示 6 格——客户端 6 格渲染硬限坐实；用户决定不再修补原版条，改为全新自定义 UI 整体覆盖。**G17DragonRide 界面**：主行 6 原生槽（100% 可用）＋扩展行玩家施法按钮（`/g17ride add <1-6> <法术ID>` 任意技能——**格数无上限**）＋施法路径自动遥测（被客户端拦截则自动折叠＋提示，内置裁决不再盲猜）。服务端 B3R9 补齐三个真实移动技能的玩家施法路径（双施法者处理器＋能量门＋运行时补 0x100 属性）。
5. **未完成**：同屏 >6 按钮（BonusActionBar 已确认为载具模式下仍显示，是唯一候选路线，未实施）；B4 骑乘时玩家自己施法/吃喝/睡眠；B5 自动寻路；B6 客户端体验与压力加固。用户已提出技能设计新要求："技能太单调了，要多元，形态多样，释放方式有区别"——B4 起的技能设计必须满足。
6. 其余各线（G11/G16/G19/G22/G23/F44/F45/AHBot/PBot/NPCBot）状态见 §6，全部冻结待办，禁止误写完成。
7. **开工第一动作：先读本文件 §0 铁律（查资料＋完整对照＋深度学习——选特效必查 G17VisualDB 全量对照库），再读 §3 G17 现状，然后处理 C10/B3R7 验收或技能差异化设计。禁止重跑任何已关闭包。**

---
## 2. 项目身份、目的与仓库关系

### 2.1 项目目的

World of Warcraft 3.3.5a 客户端＋服务端联合魔改：TrinityCore ＋ NPCBots ＋ Eluna 中文整合分支上的 C++/SQL/Lua/配置改动，加客户端 DBC/MPQ/UI/模型等双端定制。终局目标（G22 权威总纲）：现代 Dragonflight 式御龙体验、高清模型、真实城市、新种族/职业/世界、bot 自主冒险经济社交、剧情《真龙纪元》，并持续对官方 12.x 建立差分账。纯服务端绕行只能作原型，不能冒充最终完成。

### 2.2 仓库与环境（不可混淆）

| 对象 | 角色 | 当前事实 |
|---|---|---|
| `wpch3/wow-mecode` | 成果同步仓库（补丁库/规划/工具/证据/交接） | 克隆于 `/home/user/wow-mecode`；会话分支 `arena/01a03878-wow-mecode`，HEAD `e9716455`；`main` 分支另存 |
| `328950225/TrinityCore-NPCBOT-Eluna-zhCN` | 项目实际 TC/NPCBot/Eluna 架构基础 | 参考分支 `NPCBOT-Eluna-zhCN-2026`；取证提交 `4e8762e`；用于 API/机制对照 |
| `D:\TrinityCore` | 用户真实编译源码 | 分支 `bignum-mod`，探针 HEAD `ae60f5b6`；含大量已跟踪修改，禁止 reset/clean |
| `D:\TC-Build` | 编译目录 | VS2022 / RelWithDebInfo / x64；产物 `bin\RelWithDebInfo\worldserver.exe` |
| `D:\WOW` | 3.3.5a zhCN 客户端 | Data\zhCN\patch-zhCN-Y.MPQ 为有效 locale 槽（R5 已验证）；Interface\AddOns\ 放 G17 工具插件 |
| `C:\Users\Administrator\Downloads\workspace` | 用户侧工作区镜像 | 内含 `wow-mecode`（即本目录结构）与 `uploads`（PASS/FAIL 结果回传目录） |

注意：`wow-mecode` **不是**可直接编译的完整 TrinityCore 源码树；它是补丁/成果/证据仓库。仓库根另存有 G17 系列 FINAL 交付 ZIP 与 `G17_extracted/`（282 个客户端 FrameXML 取证文件），见 §14。

### 2.3 证据与状态口径（沿用，判定冲突时按此分层）

1. 实际部署/实际编译使用的源码与数据库状态（最高）
2. 相对准确上游基线的 Git diff
3. 最新且已验证的改动清单/安装说明
4. 补丁库索引与待办总表
5. 独立测试结果
6. 旧版备份、早期聊天、仅规划文档（最低）

状态标记：✅ 已完成并验证｜🟢 已安装编译通过、游戏内未全验｜🟡 有补丁未安装/编译｜🟠 仅设计/测试桩｜🔴 已知失败须撤回｜⚪ 已废弃被替代｜❓ 无法确认。

---

## 3. G17 御龙术完整现状（核心章节，2026-08-27）

### 3.1 一句话进度

服务端技能体系全部落地；特效/冷却已验收；插件路线被用户否决后转入**真客户端魔改路线**；**最新交付＝C11（原版载具条 6→8 格的 FrameXML 补丁），待验收**；随后技能差异化＋B4＋G22 深度魔改全线复用此能力。

### 3.2 服务端架构事实（当前已部署语义）

- **载具 NPC**：entry 1000171（御空龙·B0），VehicleId 70，80 级，75000 HP，能量类型 3、上限 100（G17Diag 实测 100/100）。
- **技能 ID 空间**：25 个战斗技能 990000–990024（5 原型 ×5 槽）＋4 个载体 990025 切页／990026 拉升／990027 俯冲／990028 制动。全部带 SPELL_ATTR0_CASTABLE_WHILE_MOUNTED(0x100)。
- **动作条双页**：`Creature::m_spells[0..5]` 两套布局——移动页＝[拉升, 俯冲, 推进, 冲刺, 着陆, 切页]；战斗页＝[该原型 5 个战斗技能, 切页]。切页＝重写 m_spells ＋ `Player::VehicleSpellInitialize()` 重发 SMSG_PET_SPELLS。
- **坐骑原型**：DRAGON(0)／BEAST(1)／MAGIC(2)／MECHANICAL(3)／GENERIC(4)，按载具 entry 决定战斗页内容与视觉族。
- **能量循环**：上限 100；俯冲 +15；战斗生成器 +8；被动每 2 秒 +1。
- **伤害缩放**：随等级 +15/级，终结技 +45/级；伤害归属＝骑手玩家。
- **自动战斗**：骑手交战时龙每 4.5 秒**真实施放**类型生成器（完整施法动画＋法术视觉＋弹道；B3R5 起）；自动施法不刷聊天（用 `Spell::IsTriggered()` 抑制），玩家主动施法才提示。
- **冷却双门**：同一技能冷却同时写**载具**与**玩家**两个 SpellHistory（`SpellHistory::IsReady(SpellInfo const*)` 判定）；客户端显示完全由服务端 SMSG_SPELL_COOLDOWN 驱动（DBC RecoveryTime 已清零）。
- **施法解析**：`ResolveDragonFromCaster(Unit*)` 同时支持载具生物施法（动作条）与玩家施法（技能面板/未来 B4）两条入口。
- **绑定方式**：world 库 `spell_script_names`（990025–990028）＋ `creature_template_spell`（CreatureID/Index/Spell；fork 专用表，ObjectMgr.cpp:670 加载）——**fork 的 creature_template 没有 spell1–8 列**。
- **登乘性能**：PassengerBoarded 只发 VehicleSpellInitialize，不再 LearnSpell/RemoveSpell（B3R6；原先 40+ 封包造成上马卡顿）。

### 3.3 客户端/引擎硬事实（全部有证据，写死在这里防止重查）

| # | 事实 | 证据锚点 |
|---|---|---|
| 1 | 载具技能来自 `Creature::m_spells[0..7]`（MAX_CREATURE_SPELLS=8），经 `Player::VehicleSpellInitialize()`（Player.cpp:20961–21010）发 SMSG_PET_SPELLS | fork 源码 |
| 2 | 客户端载具条**最多渲染 6 按钮**：`VEHICLE_MAX_ACTIONBUTTONS = 6` 硬编码 | VehicleMenuBar.lua 第 6 行（G17_extracted） |
| 3 | 载具模式下 MainMenuBar＋全部 MultiBar 隐藏，VehicleMenuBar＋**BonusActionBar** 显示（`ShowBonusActionBar(true)`）——BonusActionBar 是 >6 按钮唯一候选 | MainMenuBar.lua 第 90 行 `MainMenuBar_ToVehicleArt()` |
| 4 | 玩家骑载具时自己施法：fork `SpellInfo::CheckVehicle()`（SpellInfo.cpp:1911）放行带 CASTABLE_WHILE_MOUNTED 的法术；29 个 G17 载体全带 | fork 源码 |
| 5 | **3.3.5 `player->LearnSpell()` 只写服务端**：客户端全部 29 个 G17 法术 IsKnown=false，技能书 210 条扫描 0 命中——B3R3"玩家技能面板"路线因此死亡，禁止复活 | 用户 G17Diag 实测输出（已归档） |
| 6 | **DUMMY(3) 效果不渲染视觉**；客户端只为 SCHOOL_DAMAGE(2) 等真实效果渲染——C6 之前"无特效"的根因 | 火球 133(Effect=2 有视觉) vs 载体 990000(Effect=3 无视觉) 对照 |
| 7 | **客户端按下技能按钮时本地读 DBC RecoveryTime（234 字段结构第 29 列）即开始冷却动画，与服务器是否放行无关**——"施放被阻断仍转圈"根因；修复＝DBC RecoveryTime/CategoryRecoveryTime 置 0，冷却交给服务端 SMSG_SPELL_COOLDOWN | C8 用户反馈→C9 修复 |
| 8 | 上马卡顿根因＝登乘瞬间 40+ 封包（9 LearnSpell＋29 RemoveSpell＋VehicleSpellInitialize＋日志）；修复＝登离乘只发技能栏初始化 | B3R6 |
| 9 | fork 等级访问器是 `GetLevel()`（Unit.h:890），不是 `getLevel()`——B3R4 真实 MSVC C2039 教训 | 用户编译报告 |
| 10 | fork `PlayerScript::OnLogin(Player*, bool)`（ScriptMgr.h:692）双参签名 | fork 源码 |
| 11 | Spell.dbc（build 12340 zhCN）234 字段/记录 936 字节；关键列：29 RecoveryTime、30 CategoryRecoveryTime、46 RangeIndex（4=30 码、1=自身）、71 Effect_1、80 EffectBasePoints_1、92 ImplicitTargetA_1（18=敌方目标）、131 SpellVisualID、133 SpellIconID、140 Name、174 Description | DBC 解析工具实测 |
| 12 | G17Diag 确认：载具名"御空龙·B0"、GUID `0xF1500F42EB002E5C`、能量类型 3＝100/100、VehicleMenuBar 显示 6 按钮、MainMenuBar 隐藏 | 用户实测输出（§3.7） |
| 13 | 客户端检测载具状态的可靠信号＝power type 3 ＋ max 100（G17CombatBar v4/G17DragonBar v5 架构基础） | 诊断数据 |
| 14 | **载具法术存放在 BonusActionBar 动作槽**（SMSG_PET_SPELLS→客户端写入 bonus 页）；槽位 ID＝`id + (NUM_ACTIONBAR_PAGES + GetBonusBarOffset() - 1) * 12`（ActionButton.lua:140） | 提取的 FrameXML |
| 15 | **协议上限：m_spells 8 槽（MAX_CREATURE_SPELLS）→ SMSG_PET_SPELLS 10 槽（MAX_SPELL_CONTROL_BAR，Unit.h:39）→ BonusActionBar 12 槽（NUM_BONUS_ACTION_SLOTS）；默认 VehicleMenuBar 只渲染 6**。VehicleSpellInitialize 同时携带载具当前冷却列表（WritePacket\<Pet\>） | fork Player.cpp:20961 + FrameXML |
| 16 | VehicleMenuBarActionButton1-6 继承 **BonusActionButtonTemplate**（→ActionBarButtonTemplate→SecureActionButtonTemplate）：type="action" 安全点击、GetActionCooldown 冷却、UPDATE_BONUS_ACTIONBAR 自动刷新（ActionButton.lua:369）——**自建按钮继承同一模板即得全部原生行为，可显示施放第 7/8 格** | VehicleMenuBar.xml/BonusActionBarFrame.xml/ActionBarFrame.xml |
| 17 | **冷却不显示根因**：玩家施法靠 DBC RecoveryTime 本地预测（C9 已清零→无预测）；载具生物施法走 SpellHistory::StartCooldown，**只有 MOD_COOLDOWN 分支发包**，正常路径零发包；GetPlayerOwner() 对载具返回骑手（载具 charmer＝骑手，BotMgr.cpp:1935 佐证）但无人调用 | fork SpellHistory.cpp:280-360 |
| 18 | SMSG_SPELL_COOLDOWN（fork 0x134）＝GUID(8)+flags(1)+spellId(4)+cd(4)；`SpellHistory::BuildCooldownPacket` 为 public，脚本可直接复用（格式零漂移） | fork SpellHistory.cpp:608 |
| 19 | **客户端魔改机制（C11 已验证闭环）**：客户端从 MPQ 加载 Interface\FrameXML\*；patch-Z.MPQ（最高字母优先级）可覆盖任意客户端界面文件。载具条 6 格根源＝`VehicleMenuBar.lua:6 VEHICLE_MAX_ACTIONBUTTONS=6`＋xml 只定义 6 按钮；**按键路由（ActionButton.lua:17/31）自动跟随该常量**——改常量＋加按钮定义＝原版条扩容，全原生机制保留。G22 深度客户端魔改全线走此路 | 提取的 FrameXML＋C11 交付 |

### 3.4 客户端 DBC 补丁链（Spell.dbc，含 SHA256 链）

| 批次 | 内容 | 输出 SHA256（前8位） | 记录数 | 状态 |
|---|---|---|---|---|
| 基线 | 客户端原版 | dd250911 | 49839 | — |
| C1 | 52226 施放门清理（技能4着陆解锁） | 03bf11fd | 49839+门清理 | ✅ 已装 |
| C2 | 追加 25 战斗技能 990000–990024 | 760d3f27 | 49864 | ✅ 已装 |
| C3v2 | 追加 4 个动作条按钮载体（切页/拉升/俯冲/制动） | 006a892b | 49868 | ✅ 已装（用户客户端长期停留态） |
| C6 | 每类型块视觉＋30 码射程 | 5db5b7a5 | 49868 | ✅ 已装（服务端 DBC 同镜像收敛） |
| C7 | Effect_1 DUMMY→SCHOOL_DAMAGE（特效渲染开关） | （C8 前像） | 49868 | ✅ 已装 |
| C8 | 每槽独立视觉（**凭名猜错**）＋RecoveryTime 写入（**引入幻影冷却**） | — | 49868 | 🔴 用户实测 FAIL，被 C9 取代 |
| C9 v3 | Wowhead 逐条验证 25 视觉＋RecoveryTime/Category=0；安装器五缺陷已修 | 安装后新哈希 | 49868 | ✅ 用户安装 PASS，龙类特效验收确认 |
| C10 | 兽/法/机/通 20 槽视觉重制（G17VisualDB 全量对照＋语义匹配源法术）；龙类不动 | 安装后新哈希 | 49868 | 🟡 已装待逐类验收 |
| **C11 客户端魔改** | **VehicleMenuBar.lua 常量 6→8 ＋ xml 新增按钮 7/8（客户端界面源码，MPQ 链下发；DBC 透传）** | 链内 lua/xml 固定哈希 | — | 🟡 已交付待验收（23/23 自检） |

当前权威客户端包：仓库根 `G17C11_FINAL.zip`（客户端魔改：原版载具条 6→8 格；payload lua `0d572a7f`/xml `31563ecf`，23/23 包自检 PASS）。C10（`9d9c546b...c9eee`，视觉）已装待逐类验收。C9 v3 的 `G17C9_FINAL.zip`（SHA `2079be3f...df4fb`）已被用户验收后由 C10 接棒。前置：C3v2 及之后任一状态（C3/C6/C7/C8/C9 已装均可直接升级，幂等）。操作：关 WoW → 双击 `01_Install_G17C10.cmd` → PASSED → 复制 addon\G17DragonBar 到 AddOns → 重启客户端。

**C9 v1/v2 安装器事故（2026-08-27 用户真实运行 FAIL，已归档根因）**：`OBSOLETE_PACKAGE: patcher is not v1_real_visuals_no_dbc_cd`。逐行对照查明五处先天缺陷：①版本门 grep 的是 C6 模板遗留变量名 `G17B3R5_VISUAL_PATCHER_VERSION`，而 C9 补丁器变量叫 `G17C9_VERSION`（用户撞到的）；②输入门硬编码要求 C3 镜像 `006a892b`（用户在 C8 态）；③输出门硬编码要求 C6 镜像 `5db5b7a5`（C9 输出按设计不同）；④C3 状态模式钉根 MPQ 哈希（C6/C7/C8 装过必变）；⑤回滚 PS1 为 0 字节空文件。v1/v2 从未具备可安装性；旧目录已改名 `G17C9_v1v2_已废弃_安装器五缺陷_20260826/`（含 DEFECT_NOTE.txt）。**v3 修复**：安装器按用户机器上真实跑通过的 C8 流程重写（状态文件只给路径不钉哈希、输入态由补丁器自判、输出改内容验证＋打包回读校验、真回滚脚本）；DBC 负载与 v2 完全一致；包自检 28/28 PASS，**并新增"安装器门仿真"测试（T2）——把 PS1 里每个对补丁器的正则门用真实补丁文本求值，此类 bug 交付前即被拦截**。证据：`证据/g17c9v3_installer_fix_delivery_20260827.md`。

### 3.5 cs_dragonriding.cpp 源码谱系（锁定升级链，全部可作升级前像）

`D:\TrinityCore\src\server\scripts\Commands\cs_dragonriding.cpp`

| SHA256（前8位） | 版本 | 说明 |
|---|---|---|
| 98446106 | B2R3 floor | 多页技能栏起点 |
| 2ddf54a6 | B3R1 部署像 | f3_decl_order；"法术书技能"方向被用户否决，被 R2 重制取代 |
| 1a96b72e | B3R1 r1 | 首试（6 个 MSVC 错误） |
| ecd307b4 | B3R1 FIX4 | f2 真实运行证明升级 PASS |
| feb3dad4 | B3R2 r1b | SQL 表错 |
| a65b0ddc | B3R2 r1c | 修正 |
| 175e5a12 | B3R2d r1d | 6 按钮布局（用户可见 6 槽+快速着陆） |
| 29f3e554 | B3R3 | 玩家技能面板（**死路**：IsKnown=false） |
| f49fd955 | B3R4 r1 | 用户真实运行：源码/DBC/插件 PASS，MSBuild 报 getLevel |
| 7cb417b3 | B3R4c | GetLevel 修复 |
| 1febdecb | B3R5 | 真实施法自动战斗（曾为用户现场态） |
| cd05b836 | B3R6 r1 | 性能修复＋目标校验（曾为用户现场态） |
| ddcaa119 | B3R6 r1b | BAD_TARGETS＋冲突标记（曾为用户现场态） |
| 726d4032 | B3R6 r1c | 移除目标校验（技能可放） |
| 3fdb46e8 | B3R6 最终像 | 已部署并被用户验收（C9 期） |
| f2360d7e | B3R7 七槽＋冷却封包 | 已交付；布局缺陷（切页在第 7 格会被 6 格客户端隐藏）由 B3R8 修正 |
| dcfa78dd | B3R8 槽位安全修正 | 已安装 PASS；原版条仍 6 格（硬限坐实） |
| **f0564c5a** | **B3R9 全玩家施法支持** | **当前 payload；55215/52197/52226 双施法者＋能量门＋运行时补 0x100；回滚＝3fdb46e8(B3R6)；B3R6/R7/R8 均可直装** |

安装器升级白名单＝上表除 2ddf54a6 外全部镜像；未知现场状态零写入拒绝。当前权威服务端包：仓库根 `G17B3R9_FINAL.zip`（224053 字节，SHA256 `ef15c5e381545c48a3784bd38470542ef16c67582c5a667fe1d127e32ccefdb6`，29/29 包自检 PASS）；包目录 `tc-bignum/规划/G17_飞行与移动/G17B3R9_全玩家施法UI支持/G17B3R9_PlayerCast_UI_Support_Windows_20260827/`。B3R6/R7/R8 包关闭禁重跑。

### 3.6 C9 v2 视觉分配表（每 ID 均经 Wowhead WotLK 核对真实法术）

龙类行已注明确切源法术；其余行源法术中文名记录于 `tools/patch_g17c9.py` 注释。

| 原型/槽 | 0 | 1 | 2 | 3 | 4 |
|---|---|---|---|---|---|
| 龙 DRAGON (990000–04) | 7860 龙息术(法术42950) | 3879 扫尾(68867 奥妮克希亚) | 4302 寒冰护体(11426) | 4961 飞翼打击(31475) | 7776 冲击新星(30616) |
| 兽 BEAST (990005–09) | 39 英勇打击 | 250 斩杀 | 57 强效治疗 | 322 制裁之锤 | 9333 火焰爆裂 |
| 法 MAGIC (990010–14) | 67 火球术 | 7749 奥术冲击 | 784 真言盾 | 965 魔爆术 | 8041 魔爆术(boss) |
| 机 MECHANICAL (990015–19) | 3445 烈焰震击 | 3819 铁皮手雷 | 8039 痛苦压制 | 266 偷袭 | 7479 流星 |
| 通 GENERIC (990020–24) | 39 英勇打击 | 219 顺劈斩 | 3077 快速治疗 | 322 制裁之锤 | 2253 炎爆术 |

同时写入：Effect_1=SCHOOL_DAMAGE(2)、RangeIndex=4（30 码）、ImplicitTargetA_1=18（敌方目标）、RecoveryTime=CategoryRecoveryTime=0。

**C10 视觉表（2026-08-27，兽/法/机/通 20 槽重制；每个 ID 经 G17VisualDB 全量对照库核对源法术）：**

| 原型/槽 | 0 | 1 | 2 | 3 | 4 |
|---|---|---|---|---|---|
| 兽 BEAST (990005–09) | 6587 凶猛撕咬(48576) | 8634 斜掠(33876) | 57 强效治疗(2060) | 3942 突袭(9005) | 372 嗜血(23881) |
| 法 MAGIC (990010–14) | 262 奥术飞弹(5143) | 12655 吸取生命(689) | 784 真言盾(17) | 12303 奥术超载(56432) | 8041 奥爆(29919) |
| 机 MECHANICAL (990015–19) | 1904 机枪(10346) | 6399 火箭冲击(1940) | 8697 焊接光束(35919) | 1164 烟雾弹(8817) | 7479 流星(24340) |
| 通 GENERIC (990020–24) | 1165 猛击(1464) | 219 顺劈斩(845) | 3077 快速治疗(2061) | 322 制裁之锤(853) | 2253 炎爆术(11366) |

### 3.7 用户反馈与已修复问题史（防重复踩坑）

| 问题 | 根因 | 修复 | 状态 |
|---|---|---|---|
| 战斗技能无任何特效 | 载体 Effect_1=DUMMY；SpellVisualID=0 | C7 改 SCHOOL_DAMAGE＋C6 起写视觉 | ✅ |
| C8 特效张冠李戴（龙息/尾扫都显示绿色旋风；龙鳞无特效） | **视觉 ID 凭名字猜，未查 Wowhead** | C9 v2 逐条 Wowhead 验证（铁律 §0.1 起源案例） | 🟡 待用户验收 |
| 无目标按技能也转冷却 | DBC RecoveryTime 客户端本地触发 | C9 清零，服务端 SMSG_SPELL_COOLDOWN 独占 | 🟡 待用户验收 |
| C9 v1/v2 安装即 FAIL（OBSOLETE_PACKAGE，2026-08-27 用户真实运行） | 安装器五缺陷（§3.4 详）＋交付前缺"安装器门仿真"自测 | C9 v3 重写安装器（C8 已验证流程）＋28/28 包自检含门仿真 | ✅ 用户安装 PASS |
| 有冷却但 UI 不显示（2026-08-27 用户报告） | DBC RecoveryTime=0 无本地预测＋载具施法不走 StartCooldown 发包分支（§3.3-17） | B3R7 双 GUID 冷却封包＋G17DragonBar CLEU 跟踪，互为保险 | 🟡 待用户验收 |
| 上马卡顿 | 登乘 40+ 封包 | B3R6 移除 LearnSpell/RemoveSpell | ✅ 用户确认 |
| 技能被"需要目标"阻断 | B3R6 r1 目标校验过严 | r1c 移除校验 | ✅ |
| MSVC C2039 getLevel | fork 访问器名不同 | B3R4c 改 GetLevel() | ✅ |
| 自动施法刷屏 | 聊天提示无差别 | IsTriggered() 抑制 | ✅ |
| 玩家技能面板 11 按钮不显示 | LearnSpell 客户端不可见（IsKnown 全 false） | 放弃该路线；转 BonusActionBar 候选 | ⚪ 路线关闭 |
| "技能太单调，要多元，形态多样，释放方式有区别" | 25 技能同质（都是目标伤害） | **未修**——B4 起技能设计硬需求：读条/引导/瞬发/冲锋/光环/形态切换等差异化机制 | 🔴 设计债 |

用户 G17Diag 实测归档（B3R3 诊断期）：`UnitInVehicle=1；御空龙·B0 GUID 0xF1500F42EB002E5C；75000/75000；80 级；能量3=100/100；29 法术 IsKnown 全 false；技能书 210 条 0 命中；VehicleMenuBar 6 按钮显示；MainMenuBar 隐藏`。

### 3.8 客户端工具与取证资产

| 工具 | 位置 | 用途 |
|---|---|---|
| G17Diag | `tc-bignum/规划/G17_飞行与移动/G17工具/G17Diag_客户端诊断工具_20260825/`；根 ZIP `G17Diag_FINAL.zip` | 纯客户端插件；`/g17diag` 一键采集载具/法术/动作条全量诊断（§3.3-5 号证据来源） |
| G17ChatLog | 同上 `G17ChatLog_...20260825/`；`G17ChatLog_FINAL.zip` | 聊天 5000 行＋独立 10000 缓冲；导出窗口 Ctrl+A/C；`/g17log save` |
| G17CombatBar v4 | `G17CombatBar_v4_战斗技能条_20260826/`；`G17CombatBar_FINAL.zip` | 基于能量类型3+max100 检测载具状态的战斗条（不再依赖 IsKnown） |
| G17Extract | `G17Extract_客户端提取工具_20260826/`；`G17Extract_FINAL.zip` | 从客户端 MPQ 提取 Interface/FrameXML 源码 |
| G17DragonRide 界面（🔴已废止归档） | `归档_废弃_20260827/G17DragonRide_插件_错误路线/` | 插件路线（铁律 §0.3 废止）：主行 6 原生槽（100% 可用）＋扩展行最多 6 格玩家施法按钮（`/g17ride add <1-6> <法术ID>` 任意技能——**格数无上限**）＋图形能量条＋页面指示＋冷却圈＋**默认隐藏原版载具条 6 按钮**＋**施法路径自动遥测**（无 SENT 事件→自动折叠扩展行＋红字提示）＋按键 1-6/7-0-=；任务坐骑零影响。取代 G17DragonBar v5/v6（安装前需删除旧插件） |
| **G17VisualDB** | `G17工具/G17VisualDB_法术视觉对照库_20260827/` | **3.3.5.12340 全量 Spell.dbc 视觉对照库**（49839 条，Kaev 转储，五法术校准列位）；`g17visualdb.py <法术ID或视觉ID>` 正反查、`--name` 搜索——选特效必查，铁律工具化 |
| G17_extracted | 仓库根 `G17_extracted/Interface/FrameXML/`（282 文件） | 客户端 UI 源码取证库（§0.1 权威资料源） |

关键取证结论（已写死，无需重查）：VehicleMenuBar.lua 1194 行（第 6 行 VEHICLE_MAX_ACTIONBUTTONS=6）；MainMenuBar.lua 第 90 行载具美术切换（隐藏主条/多条，显示载具条＋BonusActionBar）；UIParent.lua 第 1756 行载具偏移；BonusActionBarFrame.lua 载具模式仍显示。

### 3.9 G17 未完成项与下一步（唯一顺序）

1. **用户安装 C9 v2 并验收**（当前唯一动作）：5 技特效互异且符合语义；无目标不转圈；有目标正常伤害＋冷却。若个别视觉仍不对→回 Wowhead 换该槽源法术（只改单槽，禁整批重猜）。
2. **技能差异化设计**（响应用户"多元/形态多样/释放方式有区别"）：为 5 原型设计读条、引导、瞬发、位移、光环、终结连锁等不同施法机制——先查 Wowhead 真实法术机制做原型，再落 DBC/C++。
3. **B4R1**：骑乘时玩家自身施法（CheckVehicle 已放行 0x100 属性）＋骑乘吃喝/睡眠恢复。
4. **>6 按钮**：BonusActionBar 路线验证（载具模式仍显示，客户端源码已证实显示链；未实施）。
5. **B5**：自动寻路/航线/固定不消失/异常恢复。
6. **B6**：客户端反馈打磨、网络节流、压力加固。
7. 长期：G11 bot 整合御龙、G22 完整客户端。

---
## 4. 当前优先级与唯一主线

### P0（当前唯一执行）：G17 三件套（B3R7＋C10＋G17DragonBar）用户安装与验收
- 服务端 `G17B3R7_FINAL.zip`（payload f2360d7e；关 worldserver → `01_Install_Build_G17B3R7.cmd`，首行必须 `G17B3R7_BUILD=f2360d7e`）
- 客户端 `G17C10_FINAL.zip`（关 WoW → `01_Install_G17C10.cmd`）
- 插件：C10 包内 `addon\G17DragonBar` → `D:\WOW\Interface\AddOns\G17DragonBar`
- 验收五点见 §3.9；若个别槽视觉不符→G17VisualDB 换该槽源法术重打 C10（幂等，禁整批重猜）。
- 验收四点：①龙类 5 技特效＝龙息火焰／扫尾／冰盾／振翼／环形爆发，互不相同；②其余 4 原型各自成族；③无目标按技能无冷却转圈；④有目标正常伤害＋服务端冷却显示。
- 若个别槽视觉不符：只查 Wowhead 换该槽 SpellVisualID，重打 C9（幂等），禁止整批凭感觉重配。

### P1（C9 通过后立即开始）：技能差异化 ＋ B4R1 骑乘玩家施法/吃喝
- 技能差异化设计先行（用户明确硬需求），设计文档须逐技能标注 Wowhead 源法术与机制。
- B4：玩家骑乘自身施法走 `CheckVehicle()` 0x100 属性路径；吃喝/睡眠＝骑乘状态物品使用＋恢复循环；全部服务端为主，客户端只需既有动作条/按钮。

### P2：>6 按钮（BonusActionBar 取证→原型）
- 客户端源已证实载具模式显示 BonusActionBar；需取证其按钮绑定来源（BonusActionBar 1–12 与姿态页关系）后做最小原型，再决定是否并入正式页。

### P3（御龙全链后恢复）：B5 自动寻路 → B6 打磨 → NPCBot 场景装备（GEAR-P0–P5）→ Bot 职责自动选择/补缺（ROLE）→ BOSS 人数缩放（SCALING）→ 40/100/250/500/1000 一键编团 → PBot 完整化 → G11 完整矩阵 → G16 每entry十条 → G19 游戏内验收 → G22-CP0 客户端只读基线审计。

---

## 5. 其他各线状态速览（全部冻结，禁止误写完成；详细历史见 Git 历史版交接与 00-入口）

| 线 | 当前状态 | 禁止事项 |
|---|---|---|
| G11 bot 感知第2步 | ✅ human 最小验收 28/28/0/0/True（同 PID 40 秒）；T1–T10/性能未做 | 禁止重复 Apply/编译/探针 |
| G16/AHBot | 总量约 99,250 不算完成；硬需求＝每 eligible item ≥10 条独立有效挂单、禁传说装备（传说坐骑/宠物例外）；C++ 未生成 | 不得以总量或 status 求和冒充 |
| G19 情境对话第3步 | 🟢 编译/数据库/启动加载 PASS；游戏内场景＋900 秒冷却矩阵未做 | 不重跑 SQL |
| G22 完美客户端 | 🟠 权威总纲已立（含 12.1 差分账与终局全世界可飞口径）；CP0 只读审计未开始 | 不得把服务端绕行当完成 |
| G23/F45 | F45 正常转被动观察；P2R1 `.tp` 菜单用户确认修复；P3A `.server` 助手＋153 条 GM 帮助已过本地/ZIP 门，Windows Check→停服→Apply→启动未执行 | 禁止 reload eluna |
| F44/F44R1 `.combo` | 旧 F44 真人 FAIL 已废弃；F44R1 Windows Check/Apply/编译/正确哈希新二进制（PID 16556）全 PASS，用户定性烟雾"没什么太大的问题"；A–D 全职业矩阵未回传不补造 | 禁止重复 Check/Apply/编译/启动 |
| NPCBot 场景装备 | `NPCBOT_CONTEXT_GEAR_ADAPTATION=NOT_IMPLEMENTED`（GEAR-P0–P5 路线已冻结在 04 号规划） | 不得凭上游推断施工 |
| Bot 职责自动选择/补缺 | `BOT_ROLE_AUTO_ASSIGN_AND_FILL=NOT_IMPLEMENTED` | 同上 |
| BOSS 人数缩放 | `BOT_INSTANCE_SCALING_CONTROL=NOT_IMPLEMENTED`（用户实测人数多时 BOSS 血量上亿不可杀） | 探针前不得武断归因 |
| 超大团队一键编团 | `BOT_MASS_RAID_ONE_CLICK=NOT_IMPLEMENTED`（40–1000 路线已持久化） | 不打断御龙术 |
| PBot | 固定口径"只有基础，其它完善完全没有"；权威清单 `tc-bignum/规划/04-PBot与NPCBot真实完成度及后续路线_2026-08-21.md` | 旧 A25/A27/A39/A41 勾选只算基础 |

---

## 6. G17 已关闭批次总账（防重复安装清单）

**全部禁止重跑/回滚的已关闭包**（按时间）：

- R 系（纯飞行门）：G17-A（SpellInfo.cpp 安全全世界飞行，conf 默认关）、R1（入座＋客户端 DBC 门清理）、R2（服务端施放链）、R2A（门诊断）、R3（AreaTable）、R4（双区旗）、R5（有效 zhCN locale 槽镜像 → 59961 召唤/上马/起飞/移动/室内解除全 PASS，纯飞行门关闭）。
- B 系（御龙骨架）：B0（原生载具 `.dragon` 命令族）、B1R1–B1R5（全坐骑接管；B1R4 室内 PASS；B1R5 包装坐骑/无降落伞 PASS 后 B1 关闭；旧 B1 包因 PS1 `$Args` 覆盖卡死永久废弃）、B2/B2R1–B2R4（御空体验三技能重制；B2R2 用户确认技能2 OK/技能3 偶倒车/技能4 不可用→B2R3/B2R4 修复关闭）。
- B3 系（战斗技能）：B3R1（2ddf54a6 部署，方向被否决）、B3R2/B3R2d（多页 6 按钮技能栏）、B3R3（玩家面板死路）、B3R4/R4b/R4c（体验强化＋getLevel 修复）、B3R5（真实施法自动战斗＋C6 服务端 DBC 收敛 5db5b7a5）、B3R6（3fdb46e8：登乘性能＋目标校验移除；用户随 C9 v3 验收）。当前批：**B3R7（f2360d7e：7 槽＋冷却封包，待验收）**。
- C 系（客户端 DBC）：C1（52226 门）、C2（+25 技能）、C3v2（+4 按钮，修复 v1 跑旧包问题）、C6（类型视觉＋射程）、C7（DUMMY→SCHOOL_DAMAGE）、C8（🔴 视觉猜错＋幻影冷却，被 C9 取代）、C9 v1/v2（🔴 安装器五缺陷，用户真实运行 FAIL，已废弃改名禁装）、C9 v3（✅ 用户安装 PASS，龙类特效确认）、**C10（🟡 待装；35/35 自检；全类型视觉＋G17DragonBar）**。

用户真实 Windows 运行证据归档（仓库根 `uploads/`）：B3R1 f2run PASS（`G17B3R1_WINDOWS_BUILD_RESULT_f2run_20260825.txt`：谱系门/源码 1a96b72e→ecd307b4/DBC ALREADY_APPENDED/SQL 0/25/0 全 PASS）、B3R2 r1b FAIL、B3R4 r1 FAIL（getLevel）；B3R4b 真实运行（源码+DBC+插件 PASS、仅 MSVC getLevel）与 B3R5 用户现场态（1febdecb 曾为 PRE"用户当前"）由 00-入口文档与包内谱系注释佐证。

---

## 7. 本轮（2026-08-27 交接更新批）修改文件

| 文件 | 动作 |
|---|---|
| `wow-mecode/PROJECT_HANDOFF.md` | **全面重写**：新增 §0 铁律（查资料＋完整对照＋深度学习，用户明令）、§3 G17 完整现状（B3R6+C9v2）、刷新主线/优先级/未知项/速查/结论 |
| `wow-mecode/HANDOFF_GIT_STATE.txt` | 重新生成：会话分支 `arena/01a03878-wow-mecode`、HEAD `e9716455`、近 20 提交、工作树状态、根级 G17 资产清单 |
| `wow-mecode/HANDOFF_FILE_MANIFEST.tsv` | 重新生成：全仓库文件哈希清单（含 B3/C 全部新包、G17 工具、G17_extracted、根级 FINAL ZIP） |
| `wow-mecode/tc-bignum/00-当前整体安装步骤_单文件入口.md` | 顶部插入 2026-08-27 最新批次指针（后按 C9 v3 事故更新） |

第二批（同日，C9 v3 安装器修复）：

| 文件 | 动作 |
|---|---|
| `wow-mecode/tc-bignum/规划/G17_飞行与移动/G17C9_真实特效_客户端/G17C9v3_Real_Visuals_Fix_Client_20260827/` | **新建 v3 包**：重写 Install PS1（C8 已验证流程：状态文件只给路径、输入态补丁器自判、输出内容验证＋打包回读）、新增真实 Rollback PS1（v1/v2 为空文件）、补丁器版本指纹升 v3（DBC 负载不变）、新增 `tools/test_g17c9_package.py` 门仿真自测（28/28 PASS） |
| 同目录 `G17C9_v1v2_已废弃_安装器五缺陷_20260826/`（原 `G17C9_Real_Visuals_Fix_Client_20260826` 改名） | 加 DEFECT_NOTE.txt，标记禁止安装 |
| 仓库根 `G17C9_FINAL.zip` | 重建为 v3 平铺包（459833 字节，SHA256 `2079be3f2b9626a6421d0e7efff94ab59de07f4872c0ef83290cfd1051adf4fb`） |
| `wow-mecode/tc-bignum/规划/G17_飞行与移动/证据/g17c9v3_installer_fix_delivery_20260827.md` | 新增：用户 FAIL 证据＋五缺陷根因＋修复＋28/28 自检记录 |

第一批为纯交接同步（不改源码/SQL/客户端文件）；第二批只改交付包与文档；第三批（同日晚，全类型特效＋冷却修复＋专用条）：

| 文件 | 动作 |
|---|---|
| `G17C9_真实特效_客户端/G17C10_全类型特效_客户端/G17C10_All_Archetypes_Visuals_Client_20260827/` | **新建 C10 包**：20 槽视觉重制（G17VisualDB 核对）＋C9 v3 安装器克隆＋35/35 自检（T1-T8，新增 T8 插件检查）＋`addon/G17DragonBar`（专用 8 格条 v5：原生动作槽＋CLEU 冷却跟踪＋任务坐骑零影响） |
| `G17B3R7_七槽与冷却显示/G17B3R7_SevenSlots_CooldownUI_Windows_20260827/` | **新建 B3R7 服务端包**：payload f2360d7e（PRE/回滚=3fdb46e8，13 镜像白名单）；两页 7 槽＋SendVehicleCooldownPackets 双 GUID 封包；25/25 自检（含真实跑 apply check 三态验证） |
| `G17工具/G17VisualDB_法术视觉对照库_20260827/` | **新建工具**：49839 条全量视觉对照库＋查询/反查/名字搜索 CLI（数据源 Kaev/AzerothcoreDBCToSQL，列位五法术校准） |
| 仓库根 `G17C10_FINAL.zip` / `G17B3R7_FINAL.zip` | 新建交付 ZIP（467969B/9d9c546b...；219175B/a11e6ced...） |
| `证据/g17c10_b3r7_delivery_20260827.md` | 新增：查证结论＋视觉表＋验收步骤 |

---

## 8. 验证证据矩阵（当前口径）

| 日期 | 对象 | 验证层级 | 结果 | 证据 |
|---|---|---|---|---|
| 2026-08-25 | B3R1 f2 重跑 | Windows 源码升级＋DBC＋SQL | PASS（源码 1a96b72e→ecd307b4；DBC ALREADY_APPENDED；SQL 0/25/0） | 根 `uploads/G17B3R1_WINDOWS_BUILD_RESULT_f2run_20260825.txt` |
| 2026-08-25 | B3R2 r1b | Windows 运行 | FAIL（SQL 表错） | 根 `uploads/G17B3R2_r1b_run_FAIL_20260825.txt` |
| 2026-08-25 | B3R4 r1 | Windows 运行 | 源码/DBC/插件 PASS、MSBuild FAIL（getLevel×2） | 根 `uploads/G17B3R4_r1_run_FAIL_20260825.txt` |
| 2026-08-25/26 | B3R4b/R4c/R5 | Windows 安装链 | PASS（00-入口与包谱系注释佐证；B3R5 后像 1febdecb 曾为用户现场态） | `tc-bignum/00-当前整体安装步骤_单文件入口.md` L380–L405 |
| 2026-08-26 | B3R6 | 交付＋包自检 | 🟢 已交付（payload 3fdb46e8/回滚 1febdecb；最终像独立 Windows 报告未归档） | 包目录 SHA256SUMS.txt |
| 2026-08-26 | C8 | 用户游戏内实测 | 🔴 视觉张冠李戴＋幻影冷却 | 用户反馈（§3.7） |
| 2026-08-26 | C9 v2 | 交付＋包自检＋静态门 | 🟡 待用户安装验收 | `G17C9_FINAL.zip`；包内 patch_g17c9.py 逐槽 Wowhead 注释 |
| 2026-08-27 | C9 v1/v2 | **用户真实 Windows 运行** | 🔴 FAIL：OBSOLETE_PACKAGE（安装器五缺陷，任何机器必挂） | 用户控制台/结果文件输出（已归档 §3.4/证据 md） |
| 2026-08-27 | C9 v3 | **用户 Windows 安装＋游戏内验收** | ✅ PASS（龙类特效确认；冷却 UI 缺失转新任务） | 用户确认＋`证据/g17c9v3_installer_fix_delivery_20260827.md` |
| 2026-08-27 | C10＋B3R7＋G17DragonBar v5 | 交付＋35/35 与 25/25 包自检＋fork/FrameXML 源码查证 | 🟡 部分：冷却 ✅ 用户确认；第 7 格 🔴（客户端 6 格渲染硬限） | `证据/g17c10_b3r7_delivery_20260827.md` |
| 2026-08-27 | B3R8＋G17DragonBar v6 | 研究＋交付＋26/26 自检 | ✅ B3R8 安装 PASS；原版条仍 6 格（硬限坐实）；v6 混合模式被全新 UI 路线取代 | `证据/g17dragonbar_v6_research_20260827.md` |
| 2026-08-27 | B3R9＋G17DragonRide 全新界面 | 属性核对（55215/52197 无 0x100）＋服务端双施法补齐＋全新 UI＋29/29 自检＋内置遥测 | 🟡 待用户验收 | `证据/g17dragonride_full_ui_20260827.md` |
| 2026-08-26 | G17Diag | 用户游戏内实测 | ✅ 输出归档（29 法术 IsKnown=false 等） | 用户粘贴输出（§3.3/§3.7） |

---
## 9. 下一位代理的精确执行顺序

1. 通读本文件，重点 §0 铁律（**查资料＋完整对照＋深度学习**）与 §3 G17 现状。
2. 校验 `HANDOFF_FILE_MANIFEST.tsv` 关键文件 SHA256、`HANDOFF_GIT_STATE.txt` 的分支/HEAD。
3. 确认会话分支 `arena/01a03878-wow-mecode`（本会话固定；不换分支、不推其他分支）。
4. 若用户已装 C9 v3：收取验收结论（特效四点）。PASS → 进入技能差异化设计＋B4；个别槽不符 → 仅该槽查 Wowhead 换视觉重打 C9（幂等）。若再报 FAIL：读 `uploads\G17C9_CLIENT_VISUALS_RESULT.txt` 定位到具体门再修。
5. 若用户未装：引导重新下载 `G17C9_FINAL.zip`（v3，SHA `2079be3f...df4fb`）解压后双击 `01_Install_G17C9.cmd`（注意旧 v1/v2 解压目录存在的话不要再用，会重现 OBSOLETE_PACKAGE）。
6. 任何新技能/特效设计：**先 Wowhead 逐条查真实法术（视觉＋机制），再 FrameXML 对照客户端行为，再 fork 源码对照服务端钩子**，三源齐备才写代码；设计文档逐条标注来源。
7. 服务端源码改动：走 §0.2-5 交付合同（三镜像＋谱系锁＋取证备份＋MSBuild 证明＋双击 CMD＋SHA256SUMS＋uploads 结果文件）。
8. SQL 一律只读预检＋备份计划先行；客户端 DBC/MPQ 改动沿用幂等补丁器＋回滚包。
9. 每批结束同步 00-入口、待办总表、三件套交接文件，再 commit 到会话分支并 push。
10. 禁止重跑 §6 已关闭包；禁止把测试桩/旧像当权威；禁止用上游原版整文件覆盖已改文件；禁止擅自改自定义 ID（1000171／990000–990028／70）。

## 10. 当前 Git 事实

- 仓库根：`/home/user/wow-mecode`；remote `origin = https://github.com/wpch3/wow-mecode.git`
- 会话分支：`arena/01a03878-wow-mecode`（自 `arena/01a03493-wow-mecode` 的 `ef24e03a` 分出）；另有 `main`
- HEAD：`e9716455ae47cc938e3ec60674e4264a9bd414c9`（"G17-C9 v2: Wowhead验证的真实Boss法术视觉ID + 删除DBC冷却"）
- 近提交：`010c838` G17-C9 v1 → `e971645` G17-C9 v2；更早 B3/C 系列在父分支 `arena/01a03493-wow-mecode`
- 精确状态、未跟踪清单与根级资产哈希见 `HANDOFF_GIT_STATE.txt`（同批再生成）
- 推送约定：只推会话分支 `git push origin arena/01a03878-wow-mecode`

## 11. 当前未知项（写"未确认"，不得推断）

- **C11 客户端魔改验收结果**（当前最大未知：①第 7/8 格是否显示并可用——若第 7 格空，说明客户端不填充 Bonus 槽 7+（数据在 SMSG_PET_SPELLS 里但客户端 C++ 不写入动作槽），则需要改下探方向（改 BonusActionBarFrame.xml 让 Bonus 条自身显示槽 7-12，或客户端 C++ 层不可及则走 B3R9 玩家施法）②兽/法/机/通视觉逐类验收
- 已确认事实：B3R8 安装 PASS 且原版条仍只显示 6 格——客户端 Bonus 槽 7+ 渲染硬限坐实（无论是否填充）
- 若遥测判定玩家施法被拦截：备选＝petaction 安全路径／有限客户端补丁；主行 6 原生格不受任何影响
- B3R6 最终像（3fdb46e8）的独立 Windows 构建/运行报告未单独归档（用户现场曾为 r1/r1b 中间像；按与 C9 合并验收处理）
- 同屏 >6 按钮是否可行：BonusActionBar 显示链已证实，但其按钮绑定/姿态页机制未取证，未做原型
- B4 骑乘玩家施法的具体技能集、吃喝/睡眠数值曲线（设计未开始）
- 技能差异化（"多元/形态多样/释放方式有区别"）的具体机制清单（需先做 Wowhead 机制调研）
- 旧线全部未知项沿用：F44R1 A–D 矩阵未回传；G23-P3A Windows 未执行；G11 T1–T10/性能；G19 游戏内矩阵；G16 每 entry 十条 C++；G22-CP0；BOSS 缩放代码链；NPCBot 装备真实持久化状态
- fork 源码稀疏克隆 `/tmp/tcsparse` 已随临时目录销毁；再需源码取证时重新克隆或用本文 §3.3 行号锚点

## 12. 关键文件速查

```text
# 三件套（本批已全部更新）
wow-mecode/PROJECT_HANDOFF.md                # 本文件（权威）
wow-mecode/HANDOFF_GIT_STATE.txt
wow-mecode/HANDOFF_FILE_MANIFEST.tsv

# 当前权威交付包（仓库根，用户从这里拿）
G17C11_FINAL.zip       # 客户端魔改当前：原版载具条6→8格(改FrameXML经MPQ链) 23/23自检（待装）
G17B3R9_FINAL.zip      # 服务端可选：全玩家施法支持 payload f0564c5a SHA ef15c5e3...efdb6
G17DragonRide_FINAL.zip # 插件可选路线：全新自定义御龙界面 SHA fccc6fb1...24d04
G17C10_FINAL.zip        # 客户端：全类型视觉(已装，视觉验收中) SHA 9d9c546b...c9eee
G17B3R7_FINAL.zip       # 已被 B3R8 取代 SHA a11e6ced...e38fb（勿重跑）
G17C9_FINAL.zip         # v3 已验收归档 SHA 2079be3f...df4fb（勿重跑）
G17B3R6_FINAL.zip       # 已关闭 SHA 0e08748b...87da（勿重跑）
G17Diag_FINAL.zip / G17ChatLog_FINAL.zip / G17CombatBar_FINAL.zip / G17Extract_FINAL.zip

# G17 权威包目录（wow-mecode/tc-bignum/规划/G17_飞行与移动/）
G17B3R6_性能修复/G17B3R6_Performance_Fix_Windows_20260826/    # 三镜像+谱系锁+自检
G17C9_真实特效_客户端/G17C9v3_Real_Visuals_Fix_Client_20260827/ # C9 v3 已验收归档
G17C9_真实特效_客户端/G17C10_全类型特效_客户端/G17C10_All_Archetypes_Visuals_Client_20260827/ # 当前客户端权威包(35/35自检; addon/G17DragonBar)
归档_废弃_20260827/                                    # 🔴 错误路线+失败包物理归档(3插件+C8+C9v1v2+4ZIP+说明), 勿装勿重跑
G17B3R7_七槽与冷却显示/G17B3R7_SevenSlots_CooldownUI_Windows_20260827/ # 已被B3R8取代
G17C11_客户端UI魔改_载具条/G17C11_ClientMod_VehicleBar_Windows_20260827/ # 当前客户端魔改权威包(23/23自检)
G17B3R9_全玩家施法UI支持/G17B3R9_PlayerCast_UI_Support_Windows_20260827/ # 服务端可选(玩家施法路径)

G17B3R8_槽位布局修正/ G17工具/G17DragonBar_v6_专用条_20260827/          # 已被取代, 归档
G17工具/G17VisualDB_法术视觉对照库_20260827/                     # 全量视觉对照库(选特效必查)
G17C8_技能差异化_客户端/  # 🔴 历史FAIL包，只作对照，禁止安装
G17C7_战斗特效修复_客户端/ G17C6_战斗特效客户端/ G17C3_客户端技能页按钮/
G17B3R1_战斗技能服务端/ … G17B3R5_战斗表现/                    # 历史谱系包
G17工具/G17Diag_…/G17ChatLog_…/G17CombatBar_v4_…/G17Extract_…/ # 客户端四工具
G17_通用御龙与战斗坐骑总设计_20260823.md                        # B0–B6 总设计
证据/                                                          # R/B系全部交付验收证据

# 客户端取证
G17_extracted/Interface/FrameXML/   # 282文件；VehicleMenuBar.lua/MainMenuBar.lua/BonusActionBarFrame.lua/UIParent.lua/SecureTemplates.lua

# 用户真实结果回传（仓库根 uploads/）
G17B3R1_WINDOWS_BUILD_RESULT_f2run_20260825.txt  # PASS
G17B3R2_r1b_run_FAIL_20260825.txt / G17B3R4_r1_run_FAIL_20260825.txt
（B0/B1R1/B2R1/R2A/R3/R4/R5 及 G11/G23 探针历史结果同目录）

# 统一入口与规划
wow-mecode/tc-bignum/00-当前整体安装步骤_单文件入口.md   # 顶部已插 2026-08-27 指针；正文B3R5后需下批补
wow-mecode/tc-bignum/待办总表.md / 未完成想法-总清单.md
wow-mecode/tc-bignum/规划/04-PBot与NPCBot真实完成度及后续路线_2026-08-21.md
wow-mecode/tc-bignum/规划/G22_客户端魔改整合/02-完美客户端总纲_模型城市种族职业世界主线.md

# 服务端改动的真实目标（用户机）
D:\TrinityCore\src\server\scripts\Commands\cs_dragonriding.cpp   # 谱系见 §3.5
D:\TrinityCore\src\server\game\Spells\SpellInfo.cpp              # G17-A 安全飞行（已关）
D:\TC-Build\bin\RelWithDebInfo\worldserver.exe
```

## 13. 交接结论

1. 本文件基于当前工作树、全部 G17 包目录/SHA256SUMS、谱系安装器白名单、用户 uploads 结果文件与用户反馈链完整重写；旧版（2026-08-24，B2R3 时代）在 Git 历史中可查。
2. G17 御龙术服务端链（多页技能栏/25 技能/能量/自动战斗/性能）与客户端链（按钮/射程/特效渲染/冷却）均已修到各自的最新交付：服务端 B3R6（3fdb46e8）、特效与冷却均已验收；B3R8 实测坐实原版条 6 格硬限后，按用户方向性指令转入全新自定义 UI 路线：当前批＝B3R9（全玩家施法）＋G17DragonRide 界面（主行原生 6 格保底＋扩展行玩家施法格数无上限＋内置遥测裁决），**唯一待办＝用户验收**。架构上"御龙术格数更多"不再受任何客户端限制，取决于玩家施法遥测结果。
3. 用户明令的"查找资料＋完整对照＋深度学习"已立为 §0.1 最高铁律并写入起源案例（C8 灾难）；后续任何代理任何批次开工前必须先读。
4. B3R3"玩家技能面板"与"11 同屏按钮（IsKnown 路线）"已判定死路，禁止复活；>6 按钮唯一候选＝BonusActionBar（显示链已证实、未实施）。
5. 技能差异化（多元/形态多样/释放方式有区别）是用户已提出的硬需求，当前未满足，排在 C9 验收后与 B4 合并设计。
6. 其余各线全部按 §5 冻结口径执行；所有"已完成"均附证据层级；无证据处一律"未确认"。
7. 建议用户：从会话分支 `arena/01a03878-wow-mecode` 重新下载/拉取最新仓库，确保本三件套进入备份。
