# G17-C11 真正的客户端魔改交付记录（2026-08-27 深夜）

## 用户指令（方向纠正）

"我没让你做插件，而是让你做客户端魔改，我们连这个都做不到，更别说计划里的深度客户端魔改了。"

→ 之前两批（G17DragonBar/G17DragonRide）做成了插件，方向错了。本批交付**真正的客户端魔改**：修改客户端自己的界面源码，通过 MPQ 补丁链下发。

## 技术依据（全部已验证）

- 3.3.5 客户端从 MPQ 加载 Interface\FrameXML\*（G17Extract 提取出的 282 个源文件即来自客户端 MPQ）
- patch MPQ 按字母序优先级覆盖基础 MPQ 同路径文件——我们的 patch-Z.MPQ 链（R1 起一直在用）是最高优先级
- 载具条 6 格的根源：`VehicleMenuBar.lua:6 VEHICLE_MAX_ACTIONBUTTONS = 6` ＋ `VehicleMenuBar.xml` 只定义了 6 个按钮
- **按键路由自动跟随常量**：`ActionButton.lua:17/31` `if ( VehicleMenuBar:IsShown() and id <= VEHICLE_MAX_ACTIONBUTTONS ) then button = _G["VehicleMenuBarActionButton"..id]`——改常量后按 7/8 自动路由到第 7/8 格
- 按钮继承 VehicleActionButtonTemplate（alwaysBonus=1，修复过 bug 151189）→ 全部原生机制（动作槽/图标/冷却/安全点击）

## 交付：G17C11_FINAL.zip（客户端魔改，23/23 自检 PASS）

修改内容（基于用户客户端提取的原版文件，仅两处差异，其余字节不动）：
1. `Interface\FrameXML\VehicleMenuBar.lua`：`VEHICLE_MAX_ACTIONBUTTONS = 6 → 8`
2. `Interface\FrameXML\VehicleMenuBar.xml`：新增 `VehicleMenuBarActionButton7/8`（同模板同锚链 LEFT of 前一格 offset x=2）

安装器：C9v3 已验证流程（状态文件给路径/未知状态零写入/打包后四文件回读校验：Spell.dbc 透传哈希不变＋AreaTable 不变＋lua/xml ＝ payload 哈希/双 MPQ 镜像/清缓存/真回滚）。
- payload lua `0d572a7f`（基线 `2183cb19`），xml `31563ecf`（基线 `fff0aec7`）
- 幂等：链内已有 v1 lua → ALREADY_CURRENT PASS

## 验收要点

1. 上龙 → 原版载具条出现第 7 格（配 B3R8+：制动），按键 7 可施放
2. 若第 8 格也有图标 → 客户端确实填充 Bonus 槽 8（回告写入交接）
3. 特效/冷却/切页不受影响（DBC 原样透传）

## 战略意义（G22 能力证明）

**这套"改任意客户端界面文件 → MPQ 链下发"的流程已闭环**：提取（G17Extract）→ 修改（保持字节级最小 diff）→ 打包（mpqcli）→ 链内校验 → 回滚。G22 深度客户端魔改（登录界面/动作条/任何 UI）全部走这条路，不再有技术疑点。

## 与插件路线的关系

G17DragonRide 界面（B3R9 配套）保留为可选方案；C11 客户端魔改为当前权威路线（用户明确要求）。若 C11 验收确认 Bonus 槽 7/8 有数据，则"格数更多"在原版条上彻底解决。
