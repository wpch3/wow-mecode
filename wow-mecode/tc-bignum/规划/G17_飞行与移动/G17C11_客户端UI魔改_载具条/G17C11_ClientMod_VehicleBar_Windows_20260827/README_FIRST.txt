G17-C11 真正的客户端魔改：原版载具动作条 6 格 → 8 格
====================================================

这不是插件——直接修改客户端自己的界面源码（FrameXML），通过我们一直在用的
MPQ 补丁链下发（和 C 系列 DBC 补丁同一机制、同一安装流程）：

改动只有两处（都是从你自己客户端提取的原版文件上改的，其余字节不动）：
  1. Interface\FrameXML\VehicleMenuBar.lua
     VEHICLE_MAX_ACTIONBUTTONS = 6 → 8
     （客户端的按键路由 ActionButton.lua 自动跟随这个常量：按 7/8 会点第 7/8 格）
  2. Interface\FrameXML\VehicleMenuBar.xml
     新增 VehicleMenuBarActionButton7 / Button8（与 1-6 完全同模板同锚链）

效果：原版载具条本身显示最多 8 个技能按钮（图标/冷却/点击/快捷键全部原生）。
配 B3R8+ 服务端布局：第 7 格＝制动。若客户端把 Bonus 槽 7/8 填上了数据，
第 7/8 格会直接出现技能图标——这就是"7 格"问题的最终客户端答案。
DBC 文件（Spell.dbc/AreaTable.dbc）原样透传，一个字节都不变。

前置：G17 MPQ 补丁链已装（C3v2 及之后任一状态）。
操作：关闭 WoW → 双击 01_Install_G17C11.cmd → PASSED → 重启客户端。
回滚：02_Rollback_G17C11.cmd。
结果文件：uploads\G17C11_CLIENTMOD_RESULT.txt

验收：
  1. 上龙 → 原版载具条出现第 7 格（制动图标）——快捷键 7 也能施放
  2. 若第 8 格也有图标 → 客户端确实填充 Bonus 槽 8（告知我们，写入交接）
  3. 切页正常、冷却正常、特效正常（都不受影响）

意义：这证明了 G22 深度客户端魔改的可行性——客户端任何界面文件都能用
这套 MPQ 链改（登录界面、动作条、所有 UI）。以后所有深度客户端改造
都走这条路。
