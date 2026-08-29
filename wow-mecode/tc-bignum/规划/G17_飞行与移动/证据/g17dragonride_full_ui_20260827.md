# G17 全新自定义御龙界面交付记录（2026-08-27 深夜）

## 用户指令（方向性变更）

"我的意思你还不明白，我的意思是你做个新的客户端ui界面直接覆盖整个项目的无效ui，能懂吗？如果你实在不行我们就先搁置整个补丁项目，之后重置项目。"

→ 停止在原版 6 格载具条上做增量修补；交付**全新专属 UI 整体接管御龙操作**。已照办。

## 本批前的最后研究（补齐玩家施法路径的服务端半边）

从完整 3.3.5.12340 Spell.dbc 转储核对属性（列位经龙类五法术校准）：
- 52226 着陆：Attributes=0x100（可骑乘施放）✓
- **55215 推进：Attributes=0（不可）→ 玩家骑乘施法会被 SpellInfo::CheckVehicle 拒绝**
- **52197 冲刺：Attributes=0x50010（无 0x100）→ 同上**
- 三个真实移动技能的脚本处理器用 `IsDragon(GetCaster())` 硬检查 → 玩家施法是空操作
- `CheckEnergyCast` 对非龙施法者直接放行（能量门不作用于玩家路径）

## 交付一：G17-B3R9 服务端（payload f0564c5a，29/29 自检 PASS）

`G17B3R9_FINAL.zip`（224053B，SHA ef15c5e3...efdb6），PRE=B3R8 dcfa78dd（B3R6/R7/R8 任一状态可直装），回滚=B3R6：
1. 三个移动技能处理器改 `ResolveDragonFromCaster`（双施法者）——玩家施法触发同样 AI 动作并扣龙能量
2. `CheckEnergyCast` 双施法者能量门（玩家路径不能白嫖能量）
3. 启动时给 55215/52197 运行时补 `SPELL_ATTR0_CASTABLE_WHILE_MOUNTED`（沿用 B2R3 已验证的 const_cast 手法，磁盘 DBC 不动）
4. 战斗技能与 4 载体本就支持玩家施法（B3R3 双施法路径）

## 交付二：G17DragonRide 全新自定义界面（插件，9100B，SHA fccc6fb1...24d04）

`G17DragonRide_FINAL.zip`。三层保障架构：
1. **主技能行（6 原生槽）**：BonusActionButtonTemplate——与原版条同机制，100% 可用，显示服务端当前页
2. **扩展技能行（最多 6 格·玩家施法）**：SecureActionButtonTemplate + type="spell" 按 ID（CastSpellByID）——配 B3R9 后任意技能可放（`/g17ride add <1-6> <法术ID>`），**技能格数从此没有上限**
3. **原版无效 UI 覆盖**：默认隐藏原版载具条 6 按钮（俯仰/离开等有效控件保留）
- 能量条（图形）＋页面指示＋冷却圈（CLEU 成功跟踪，无幻影）
- 按键 1-6 主行 / 7 8 9 0 - = 扩展行；拖动/缩放/锁定
- **施法路径自动遥测**：扩展行点击后 0.5s 无 UNIT_SPELLCAST_SENT → 判定客户端拦截 → 自动折叠＋红字提示（/g17ride forcextra 重试、status 诊断）——内置裁决机制，不再盲猜
- 只在御龙载具激活（能量类型3+上限100），任务坐骑零影响
- 安装时删除旧 G17DragonBar（防双条）

## 用户操作

1. 服务端：关 worldserver → `G17B3R9_FINAL.zip` → `01_Install_Build_G17B3R9.cmd` → 首行 `G17B3R9_BUILD=f0564c5a` → PASSED → 重启
2. 插件：删除 AddOns\G17DragonBar → `G17DragonRide_FINAL.zip` 里的 G17DragonRide 复制到 AddOns → 重启客户端
3. 验收：①专属界面出现（标题/能量条/双行按钮）②原版 6 格条隐藏 ③主行 6 技全可用 ④扩展行制动可点（若折叠＋红字→回报，说明客户端拦截玩家施法）⑤`/g17ride add 2 990004` 再加一技能试试 ⑥`/g17ride status` 输出发回

## 技术事实沉淀（写入交接 §3.3）

- 55215/52197/52226 的 DBC Attributes 实测值（见上）
- SecureTemplates.lua:316-339：petaction→CastPetAction / spell→CastSpellByID（安全按钮两条施法路径）
- 客户端拦截未知法术施法与否仍无公开资料——本批用内置遥测解决判定问题（不再依赖外部资料）

## 风险与后备

- 若遥测判定"客户端拦截玩家施法"：扩展行不可用 → 剩余路线＝①petaction 安全路径（需研究载具下 CastPetAction 的索引映射）②有限客户端补丁（MPQ 层）。主行 6 原生格在任何情况下都可用，界面本身不受影响。
