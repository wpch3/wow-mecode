# G23-P2 Lua稳定性：独立交付与Windows实施

更新时间：2026-08-22  
状态：Windows Check原始输出PASS、Apply用户确认PASS；P2的`.tp`缺陷已由P2R1修复并获用户确认；当前转10号P3A，SQL结果未单独确认  
前置：F45已获用户正常使用确认，群拾播报可见且观察到的数量一致；F45转被动观察

## 1. 对当前Lua完成度的诚实结论

用户询问时，Windows仍是P0R捕获的5个旧Lua和3个扩展，不能写成“缺失Lua已经全部添加”。现已完成并封装G23-P2当前计划内的Lua稳定性基线，但尚未在用户Windows安装：

- 新增`G23Core.ext`共享命令/配置式常量/错误保护/日志冷却/同步DB验证层；
- 新增GM按需`.luadiag`，脚本顶层不查库；
- 公告收敛为唯一世界state定时器；
- 每日奖励改为数据库UUID+`PRIMARY KEY(guid, claim_date)`原子领取闸，包含失败补偿与pending防重；
- 传送具备战斗、PVP、战场/竞技场、飞行/航线、坐骑、载具、死亡、副本/团本、目标地图和权限安全门；会话5分钟超时；
- `.bigtest`恒真断言已删除，必须API与可选标准库分开；
- `.help2`改为共享命令表；
- 旧`ObjectVariables.ext`失效Creature删除回调已用同路径安全隔离件替代，原件可回滚。

P3A的`.server`统一助手与新版`.gmhelp`已在独立批次完成；收藏/最近传送、活动面板、个人便捷工具等仍属后续批次，不冒充P2/P3A已完成。

## 2. 最终交付

```text
G23P2_delivery_20260822.zip
size=56108
sha256=7d22d2b54c911af566c1184ba78a206223707367c2f6ce4cdab9190f2143de46
files=36
```

包目录：

```text
tc-bignum/补丁库/02_修复/G23P2_Lua稳定性与安全门/
```

ZIP已通过CRC、唯一文件名、36文件与源目录逐字节对照。包内`SHA256SUMS.txt`覆盖其余35个文件并已全部验证。

## 3. Windows唯一顺序

完整细节见包内`安装说明.md`。唯一顺序为：

1. 双击`G23P2_Check.cmd`，必须为`READY_TO_APPLY`；
2. 正常停服；
3. HeidiSQL执行`sql\G23P2_daily_reward_atomic.sql`，末尾`summary_table=1`、`claim_table=1`；
4. 双击`G23P2_Apply.cmd`；
5. 正常启动worldserver。

无需VS2022编译，不重跑CMake，不执行`.reload eluna`。本包运行根锁定为P0R真实路径`D:\TC-Build\bin\RelWithDebInfo`。

## 4. 本地门槛

```text
G23P2_LUAC_5_2_ALL=PASS
G23P2_WORLD_STATE_LOAD_MOCK=PASS
G23P2_MAP_STATE_LOAD_MOCK=PASS
G23P2_NO_TOP_LEVEL_DB=PASS
G23P2_ANNOUNCE_SINGLE_WORLD_TIMER=PASS
G23P2_DAILY_ATOMIC_LUA_MOCK=PASS
G23P2_DAILY_FAILURE_COMPENSATION_MOCK=PASS
G23P2_TELEPORT_SAFETY_LUA_MOCK=PASS
G23P2_TELEPORT_EXPIRED_SESSION_MOCK=PASS
G23P2_STATIC_AND_MODEL=PASS
G23P2_INSTALLER_SELFTEST=PASS
G23P2_INSTALLER_SELFTEST_OPTIMIZED=PASS
G23P2_CMD_CRLF=PASS
G23P2_ZIP_CRC_AND_BYTE_COMPARE=PASS
```

证据：

```text
tc-bignum/补丁库/02_修复/G23P2_Lua稳定性与安全门/evidence/
```

## 5. Windows回传与状态标记

用户已回传Check原始输出：6个既有文件全部`PRE`、3个新增文件全部`ABSENT`，且明确出现`G23P2_STATE=READY_TO_APPLY`；随后确认Check和Apply两个安装CMD均执行成功。其后在游戏中确认`.tp 暴风城`可用，但`.tp` UI点击“东部王国”等分类立即误报会话超时，证明P2 Lua已进入运行路径并暴露已知缺陷。根因及独立热修见09号；HeidiSQL末尾两项结果仍未单独回传。

```text
F45_RUNTIME=PASS_USER_CONFIRMED_NORMAL_USE
G23P2_LOCAL_ACCEPTANCE=PASS
G23P2_ZIP_ACCEPTANCE=PASS
G23P2_WINDOWS_CHECK=PASS_RAW_OUTPUT
G23P2_WINDOWS_SQL=NOT_CONFIRMED
G23P2_WINDOWS_APPLY=PASS_USER_CONFIRMED
G23P2_RUNTIME=RUNNING_WITH_TP_MENU_BUG
G23P2_ROLLBACK=NOT_RUN_EXPECTED
G23P2R1_LOCAL_ACCEPTANCE=PASS
G23P2R1_ZIP_ACCEPTANCE=PASS
G23P2R1_WINDOWS_CHECK=PASS_USER_CONFIRMED
G23P2R1_WINDOWS_APPLY=PASS_USER_CONFIRMED
G23P2R1_RUNTIME=PASS_USER_CONFIRMED_TP_MENU
G23_CURRENT_NEXT=G23P3A_WINDOWS_INSTALL
```

P2R1已关闭。当前按`10-G23-P3A-Lua服务器助手与GM帮助完善.md`安装P3A；P2/P2R1禁止重复。
