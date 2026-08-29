# G17 第 7 格问题研究与解决记录（2026-08-27 晚）

## 用户报告

C10＋B3R7＋G17DragonBar v5 三件套后："没有 7 格"（专用条第 7 格不显示）；"有冷却"（冷却显示修复 ✅ 已生效）。

## 研究过程（完整对照）

### 1. 提取的客户端 FrameXML（G17_extracted）逐行核对
- `VehicleMenuBar.lua:6`：`VEHICLE_MAX_ACTIONBUTTONS = 6` —— 载具皮肤 UI **硬编码 6 按钮**
- `VehicleMenuBar.xml`：VehicleMenuBarActionButton1-6 继承 `VehicleActionButtonTemplate` → `BonusActionButtonTemplate` → `ActionBarButtonTemplate` → `SecureActionButtonTemplate`（type="action"）
- `BonusActionBarFrame.lua:4`：`NUM_BONUS_ACTION_SLOTS = 12` —— bonus 条容量 12
- `ActionButton.lua:140`（ActionButton_CalculateAction）：bonus 槽动作 ID ＝ `id + (NUM_ACTIONBAR_PAGES + GetBonusBarOffset() - 1) * 12`
- `PetActionBarFrame.lua`：`NUM_PET_ACTION_SLOTS = 10`；`PetActionButtonUp(id)` → `CastPetAction(id)`（受保护）
- **`SecureTemplates.lua:334-339`：`SECURE_ACTIONS.spell` → `CastSpellByID(spellID, unit)`**（数字 spell 属性按 ID 施法，安全路径）
- `SecureTemplates.lua:316-319`：`SECURE_ACTIONS.petaction` → `CastPetAction(action, unit)`

### 2. fork 服务端源码核对
- `Player::VehicleSpellInitialize()`（Player.cpp:20961）：发送全部 m_spells[0..7]（MAKE_UNIT_ACTION_BUTTON(spellId, i+8)，i=0..7）＋ 2 个空槽（MAX_SPELL_CONTROL_BAR=10，Unit.h:39）＋ 载具当前冷却列表（WritePacket\<Pet\>）
- **`spell_g17_combat_skill`（cs_dragonriding.cpp:2491）注释明写 "B3-R3: dual caster — the vehicle-bar button (dragon caster) OR the [player caster]"；`ResolveDragonFromCaster()`（:426）对玩家施法者走 `GetDragon(player)`** —— 玩家按 ID 施放 29 个载体在服务端完全走得通（0x100 属性＋双门冷却＋能量全部生效）

### 3. 网上研究
- WowInterface（2010/2011）：SecureActionButton type="spell" 数字 ID 施法可行（"good news on being able to cast by spellID"）
- 宠物条 10 槽上限是客户端硬限制（不可扩展，2009 年起公认）——宠物条/载具皮肤都是固定 UI
- 无任何公开资料证明 3.3.5 客户端会**渲染**第 7 个载具槽；我们自己 B3R2d 的实测记录（B3R6 源码注释）："verified live: 7 sent -> 6 shown"（发 7 只显示 6）—— 该实测针对载具皮肤条；**Bonus 槽 7 是否被填充（即使不渲染）仍未知**，属客户端 C++ 内部行为，Lua 无法保证

### 4. 结论（两种可能，都能被新方案覆盖）
- 可能 A：用户未装 B3R7（服务端仍只发 6 个法术）
- 可能 B：B3R7 已装，但客户端只填充/渲染 6 个 Bonus 槽
- **无论哪种，可靠路径 = 不依赖客户端填充行为的"玩家施法"按钮**（服务端双施法路径已核实存在）

## 解决方案（本批交付）

### G17DragonBar v6（8 格混合条）
- 槽 1-6：原生 BonusActionButtonTemplate（安全动作槽，与默认载具条同机制）
- **槽 7-8：混合模式**——若 Bonus 槽 7/8 有动作→原生 type="action"；否则→**type="spell" spell=ID 玩家施法兜底**（默认槽 7=制动 990028、槽 8=切页 990025，可 `/g17bar set 7|8 <ID>` 换任意 990000-990028）
- **诊断**：`/g17bar status` 打印 GetBonusBarOffset＋Bonus 槽 1-12 全部动作内容（直接看出客户端到底填了几个槽，一次性回答"可能 A/B"）＋当前页＋槽 7/8 模式；`/g17bar testcast <ID>` 把槽 7 指向任意法术供点击测试玩家施法路径
- v5 功能保留：CLEU 冷却跟踪（无幻影）、龙能读数＋页名、按键 1-8/小键盘、拖动/缩放、任务坐骑零影响

### G17-B3R8（服务端槽位安全修正）
- B3R7 把切页放第 7 格是布局错误：若客户端只渲染 6 格，**切页按钮会从默认条上消失**
- B3R8 两页统一为：[技能0-4/机动0-4，**切页@第6格（永远可见）**，**制动@第7格**，0]
- 兼容：B3R6（3fdb46e8）或 B3R7（f2360d7e）源码状态均可直接安装（13 镜像白名单）；回滚目标 B3R6
- payload `dcfa78dd92ac4491882b9e2ec5c18a8b0803d6fcbb01cbe553e7fc069ac0f487`，26/26 自检 PASS

## 用户操作

1. 服务端：关 worldserver → `G17B3R8_FINAL.zip` → `01_Install_Build_G17B3R8.cmd` → 首行 `G17B3R8_BUILD=dcfa78dd` → PASSED → 重启
2. 插件：`G17DragonBar_FINAL.zip` 解压出的 `G17DragonBar` 覆盖 `D:\WOW\Interface\AddOns\G17DragonBar` → 重启客户端
3. 上龙 → 应看到 8 格条（第 7 格=制动、第 8 格=切页）→ 跑 `/g17bar status` 把输出回传（回答"客户端到底填几个槽"的最终数据，写入交接）
4. 若第 7 格点击无反应：`/g17bar testcast 990028` 后点击第 7 格并回报现象（测玩家施法路径是否被客户端拦截）

## 长期架构备注

若 status 证明客户端只填 6 槽＋玩家施法路径畅通：专用条可扩展到任意格数（每格一个 spell 按钮，法术 ID 硬编码/可配置），彻底摆脱 6 格皮肤限制——"御龙术格数更多"的上限从协议层(8/10/12)变为纯 UI 设计问题。
