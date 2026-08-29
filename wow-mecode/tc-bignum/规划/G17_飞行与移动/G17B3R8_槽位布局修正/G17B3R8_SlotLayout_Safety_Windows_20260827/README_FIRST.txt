G17-B3R8 服务端：槽位布局安全修正（切页恒在第 6 格）
====================================================

问题：B3R7 把"切页"放到第 7 格（m_spells[6]），但实测 3.3.5 客户端只会
填充/渲染 6 个载具 Bonus 槽 → 切页按钮在默认条上消失、专用条第 7 格为空。

修复（源码-only，无 SQL/DBC/插件步骤）：
  * 两页布局改为：前 5 技能/机动 + 【切页@第6格】+【制动@第7格】
    - 切页永远在默认 6 格条上可见（任何客户端行为下都不会丢）
    - 制动在第 7 格：G17DragonBar v6 原生显示（若客户端填充第 7 槽），
      否则走插件的玩家施法兜底按钮（type="spell" spell=990028）
  * B3R7 的冷却 UI 封包逻辑原样保留

兼容：当前源码是 B3R6（3fdb46e8）或 B3R7（f2360d7e）都能直接装
     （安装器自动识别；未知现场状态零写入拒绝）。

操作：关闭 worldserver → 双击 01_Install_Build_G17B3R8.cmd →
      首行显示 G17B3R8_BUILD=dcfa78dd → PASSED → 重启 worldserver。
结果文件：uploads\G17B3R8_WINDOWS_BUILD_RESULT.txt
回滚：02_Rollback_G17B3R8.cmd（回到 B3R6 3fdb46e8）。

配套插件：G17DragonBar v6（G17DragonBar_FINAL.zip）——8 格混合条：
  槽 1-6 原生动作槽 / 槽 7-8 混合（原生或玩家施法兜底，默认槽7=制动、槽8=切页，
  可 /g17bar set 7|8 <法术ID> 换成任意 990000-990028）。
  诊断：/g17bar status 打印 BonusBarOffset + Bonus 槽 1-12 全部内容（能直接看出
  客户端到底填了几个槽）+ /g17bar testcast <ID> 测玩家施法路径。
