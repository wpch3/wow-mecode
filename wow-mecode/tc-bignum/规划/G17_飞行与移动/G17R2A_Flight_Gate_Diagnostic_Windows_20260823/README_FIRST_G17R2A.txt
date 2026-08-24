G17-R2A：59961“无法在这里使用”只读分流诊断
================================================

当前已确认
----------
- G17R2_WINDOWS_BUILD_RESULT=PASS（用户确认结果行；原始结果文件尚未回传）。
- 红色始祖幼龙59961在湿地仍失败。
- 新提示为“无法在这里使用”。

这不再直接证明R2同一严格门仍失败。必须先区分：
1. 客户端是否真正加载了G17-R1 patched Spell.dbc；
2. 59961是否送达服务器并通过R2 CheckLocation；
3. 若两者都通过，才进入后续Spell.cpp服务端门。

唯一动作
--------
1. 在湿地重现59961失败一次。
2. 关闭Wow.exe；worldserver可以继续运行，以便读取现有日志。
3. 解压本包并双击：Run-G17R2A-Diagnostic.cmd
4. 回传小文件：
   C:\Users\Administrator\Downloads\workspace\uploads\G17R2A_GATE_DIAGNOSTIC_RESULT.txt

脚本只读
--------
- 不修改D:\TrinityCore；
- 不修改D:\TC-Build；
- 不修改D:\WOW；
- 不执行SQL；
- 不构建、不启动/停止服务器；
- 不重复R1或R2；
- 临时抽取目录在诊断结束时删除。

自动检查
--------
- R2结果文件和活动worldserver.exe是否匹配；
- SpellInfo.cpp是否为R2后镜像；
- Spell.cpp实际SHA及是否匹配锁定上游前像；
- WorldFlight运行配置行；
- 最近服务器日志是否出现：
  G17R2 old-world pure-flight location allowed: spell=59961
- 客户端安装report/state是否存在且一致；
- 从patch-Z到patch-A直接抽取探针，不依赖MPQ listfile；
- 最高优先级根Data Spell.dbc是否为精确patched DBC：
  dd25091167f671764735ce88c78b66485c6d661fadf05d322574c261f6e464ea

关键分类
--------
- CLIENT_DBC_GATE_NOT_INSTALLED_OR_OVERRIDDEN：客户端补丁没有生效；不做R3服务端改动。
- SERVER_LOCATION_GATE_PASSED_CHECK_LATER_SERVER_GATE：R2日志已命中，转查Spell.cpp后续门。
- CLIENT_PATCH_PRESENT_BUT_SERVER_LOCATION_MARKER_NOT_FOUND：客户端补丁存在，但日志未证明请求通过R2；结合report中的活动EXE、配置和日志发现结果处理。
- CLIENT_DBC_PRIORITY_UNKNOWN：存在无法安全读取的高优先级槽，先解决优先级证据。

不要先重装任何旧包，也不要手工覆盖MPQ槽。诊断结果会决定下一步是客户端加载修复还是独立R3服务端窄修复。
