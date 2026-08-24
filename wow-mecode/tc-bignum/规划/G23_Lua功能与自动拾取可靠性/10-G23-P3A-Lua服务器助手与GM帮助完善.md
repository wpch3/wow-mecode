# G23-P3A Lua服务器助手与GM帮助完善

更新时间：2026-08-22  
状态：源码、Lua 5.2/mock/安装器及最终ZIP完整性门槛PASS；Windows未安装  
前置：P2R1由用户确认`.tp`菜单恢复正常，专项链关闭

## 1. 范围

### `.server`统一助手

新增`.server`、`.server status/commands/daily/tp/gmhelp/health`。G23Core新增显式`PASS_THROUGH`契约：原生`.server info/motd/restart/shutdown/set`、未知未来子命令及控制台命令全部交回TrinityCore，不被Lua助手遮蔽。

### `.gmhelp`完整化

旧C++固定目录只有早期69条且遗漏大量后续项目命令。本批不修改C++、不编译，而是在Eluna命令钩子中安全优先处理游戏内`.gmhelp`：

- 153条去重中文项目/常用核心条目；
- 覆盖当前识别的全部自建顶层命令；
- 11类无状态Gossip菜单，避免重犯P2跨state session问题；
- `.gmhelp find`同时搜索静态项目目录和真实`world.command(name,help)`；
- `.gmhelp core/cat/list/all/verify`；
- 高危命令只展示说明，不自动执行；
- GM等级1入口，查询前权限拦截；无顶层DB查询。

## 2. 最终交付

```text
G23P3A_delivery_20260822.zip
size=34756
sha256=fc9d850f9b0a2cf0c4f1518b44c55f96a78423a6bb09cc76ac30074876415270
files=20
```

包目录：

```text
tc-bignum/补丁库/01_功能/G23P3A_Lua服务器助手与GM帮助完善/
```

`SHA256SUMS.txt`覆盖其余19文件；ZIP CRC、唯一条目及源目录逐字节对照PASS。

## 3. Windows唯一顺序

1. `G23P3A_Check.cmd`，必须为`READY_TO_APPLY`；
2. 正常停服；
3. `G23P3A_Apply.cmd`；
4. 正常启动worldserver；
5. 简短确认`.server`、原生`.server info`、`.gmhelp`、`.gmhelp find pbot`、`.gmhelp core reload`。

无需SQL、无需VS2022编译，不执行`.reload eluna`。

## 4. 结果与边界

```text
G23P2R1_RUNTIME=PASS_USER_CONFIRMED_TP_MENU
G23P3A_LUAC_5_2_ALL=PASS
G23P3A_STATIC=PASS catalog=153
G23P3A_COMMAND_PASS_THROUGH_MOCK=PASS
G23P3A_GMHELP_DYNAMIC_AND_STATELESS_MOCK=PASS
G23P3A_NO_TOP_LEVEL_DB=PASS
G23P3A_FULL_LOAD_MOCK_WORLD=PASS
G23P3A_FULL_LOAD_MOCK_MAP=PASS
G23P3A_FULL_P2R1_INTEGRATION=PASS
G23P3A_INSTALLER_SELFTEST=PASS
G23P3A_INSTALLER_SELFTEST_OPTIMIZED=PASS
G23P3A_POST_SHA_ALL=PASS
G23P3A_ZIP_CRC_AND_BYTE_COMPARE=PASS
G23P3A_WINDOWS_CHECK=NOT_RUN
G23P3A_WINDOWS_APPLY=NOT_RUN
G23P3A_RUNTIME=NOT_RUN
G23_CURRENT_NEXT=G23P3A_WINDOWS_INSTALL
```

P3B收藏/最近传送、P3C每日/每周活动面板、P3D个人便捷工具仍是后续独立批次，不冒充P3A已完成。
