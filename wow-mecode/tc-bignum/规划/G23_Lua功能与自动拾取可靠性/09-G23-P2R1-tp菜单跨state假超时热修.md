# G23-P2R1 `.tp`菜单跨Lua state假超时热修

更新时间：2026-08-22  
状态：用户确认Windows安装后`.tp`菜单恢复正常，P2R1专项链关闭；当前转10号P3A  
前置：P2 Check/Apply成功；P2R1 Check/Apply及修后菜单按用户回传粒度PASS

## 1. 用户现场与根因

用户报告：输入`.tp`后点击“东部王国”等分类立即显示“传送会话已超时，请重新输入.tp”，但`.tp 暴风城`仍可直接传送。

P2 `custom_teleport.lua`把菜单上下文保存在脚本局部`local sessions`。Eluna multistate下，命令和Player Gossip点击可能由不同Lua state处理；各state不共享局部Lua表，因此点击回调找不到命令state写入的session。单结果搜索直接调用`safeTeleport`，不依赖session，所以仍然正常。

独立回归已证明：锁定P2前像在“另一state直接收到分类点击”的模型中稳定复现旧超时且不查传送表；P2R1同一模型不超时并按分类重建列表。

## 2. 修复设计

- 删除`local sessions`、5分钟TTL、`sessionOf`、sweep和登出清理；
- 分类与页码通过Gossip `sender/intid`无状态编码；
- 分类分页每次按map和offset重新只读查询；
- 目标点击按不可变`game_tele.id`重新查询，不信任前一state中的Lua对象；
- 关键词结果超过28项时提示缩小关键词，不建立搜索session；
- 保留战斗/PVP/飞行/航线/坐骑/载具/死亡/副本/目标地图/坐标/GM权限全部安全门；
- 不修改F45、不改SQL/conf、不编译、不允许`.reload eluna`。

## 3. 最终交付

```text
G23P2R1_tp_menu_hotfix_20260822.zip
size=22217
sha256=a07a5dedc352e6aef4e48a6074677d4239788a2d4b940ce1d3146a2056cc38d9
files=14
```

包目录：

```text
tc-bignum/补丁库/02_修复/G23P2R1_tp菜单跨state会话修复/
```

ZIP已通过CRC、唯一条目、14文件及源目录逐字节对照；`SHA256SUMS.txt`覆盖其余13个文件。P2原ZIP保持原哈希，未修改。

## 4. Windows唯一顺序

1. 双击`G23P2R1_Check.cmd`，必须为`G23P2R1_STATE=READY_TO_APPLY`；
2. 正常停服；
3. 双击`G23P2R1_Apply.cmd`；
4. 正常启动worldserver；
5. 输入`.tp`，点击“东部王国”，确认出现地点列表；再确认`.tp 暴风城`正常。

无需SQL、无需VS2022编译，不执行`.reload eluna`。P2 SQL不重复导入。`G23P2R1_Rollback.cmd`只在热修异常时停服执行，正常安装不要运行。

## 5. 本地门槛与边界

```text
G23P2R1_LUAC_5_2=PASS
G23P2R1_CROSS_STATE_REPRO_OLD=PASS
G23P2R1_CROSS_STATE_FIXED_NEW=PASS
G23P2R1_TELEPORT_STATELESS_MOCK=PASS
G23P2R1_STATIC=PASS
G23P2R1_INSTALLER_SELFTEST=PASS
G23P2R1_INSTALLER_SELFTEST_OPTIMIZED=PASS
G23P2R1_CMD_CRLF=PASS
G23P2R1_FULL_P2_INTEGRATION=PASS
G23P2R1_POST_SHA_ALL=PASS
G23P2R1_ZIP_CRC_AND_BYTE_COMPARE=PASS
G23P2R1_WINDOWS_CHECK=PASS_USER_CONFIRMED
G23P2R1_WINDOWS_APPLY=PASS_USER_CONFIRMED
G23P2R1_RUNTIME=PASS_USER_CONFIRMED_TP_MENU
G23_CURRENT_NEXT=G23P3A_WINDOWS_INSTALL
```
