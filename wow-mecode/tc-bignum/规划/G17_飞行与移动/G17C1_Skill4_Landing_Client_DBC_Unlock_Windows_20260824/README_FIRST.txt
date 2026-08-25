G17-C1 客户端 Spell.dbc 解锁（技能4 52226）— 双端解锁的客户端半
================================================

为什么还要这个包（重要）：
  B2R4 解锁的是服务器 DBC。但 WoW 3.3.5a 客户端在按下技能按钮时会
  【本地】读取 Spell.dbc 检查施放条件：52226 的 RequiresSpellFocus=1553
  和 CasterAuraSpell=52255 让客户端直接判定“不可用”，根本不发送封包。
  所以只改服务器端（无论 C++ 净化还是 DBC）技能4都不会有任何变化——
  这就是你上轮“四技能没有任何更改”的根因。
  本包把【客户端】的 Spell.dbc 也解掉这两个门槛（改的是 DBC 文件本身，
  按钮名字/图标完全不变），点击后封包才会真正到达服务器。

重要（v7 更新，防旧包）：你上次仍运行了旧包（patcher 无 auto-mkdir）。
  本版安装器启动会打印 C1_BUILD=v6_auto_mkdir，并自动校验 patcher 版本；
  若你误用旧包会明确提示 OBSOLETE_PACKAGE 而不是 FileNotFoundError。
  重下后请核对 zip 的 SHA256（见 README 末尾），并确认解压后
  `python tools\patch_g17c1_spell_dbc.py --version` 输出 v6_auto_mkdir。

（v6 修复）：上一版发现模式/探测/check全部成功，但补丁器写
  generated\DBFilesClient\Spell.dbc 时因父目录未创建而 FileNotFoundError。
  v6 已让补丁器与安装器都自动创建父目录（并用你 48.9MB 真实 Spell.dbc
  验证：输出哈希 03bf11fd...）。直接重跑即可，DBC/MPQ 均未被改动。

（v5 修复）：上一版虽已加入状态文件缺失的自动探测，但安装器开头的
  "必需文件"列表仍把 R4/R5 状态文件列入（漏删），导致你仍卡在
  required file missing: G17R4_CLIENT_MPQ_UPGRADE_STATE.txt。v5 已把这个
  硬前置删掉——只有工具和 patcher 是必需，状态文件缺失会自动内容探测。

（v4 修复）：即使 `uploads\` 里 R4/R5 状态文件丢失/被清理
  （你这次正是：G17R4_CLIENT_MPQ_UPGRADE_STATE.txt 缺失），本包会自动降级为
  内容探测：直接扫描客户端找到 DBC 完全匹配的 patch-Z.MPQ 与 zhCN 镜像，
  校验通过后照常解锁。日志会显示 ENV_MODE=DISCOVERY；不伪造状态文件。

前提（必须按顺序）：
  1. 服务器已装 G17-B2R3 + G17-B2R4（服务器 DBC 解锁，10 秒包）
  2. 客户端已装过 G17-R1→R5（即 Data\patch-Z.MPQ 与
     Data\zhCN\patch-zhCN-Y.MPQ 已存在且状态 PASS——你现在就是）

操作（约 1 分钟）：
  1. 完全关闭 WoW（任务管理器确认没有 wow.exe / wow-64.exe）。
  2. 双击 01_Install_G17C1_Client_Unlock.cmd
     （自动：校验 R4/R5 状态 -> 从 patch-Z.MPQ 提取 Spell.dbc ->
       把 52226 的焦点/光环清零 -> 重建 patch-Z.MPQ ->
       字节级同步到 patch-zhCN-Y.MPQ -> 清客户端缓存）
  3. 看到 [G17C1] CLIENT UNLOCK PASSED。
  4. 启动 WoW，召唤坐骑，按技能4：应能释放并分类型着陆。

回滚：双击 02_Rollback_G17C1_Client_Unlock.cmd。

结果文件：
  C:\Users\Administrator\Downloads\workspace\uploads\G17C1_CLIENT_MPQ_UNLOCK_RESULT.txt
  回传给我。
