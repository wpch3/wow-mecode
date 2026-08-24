# Lua脚本说明（G23-P3A）

更新时间：2026-08-22
仓库状态：P2R1已获用户运行确认；P3A服务器助手与完整GM帮助源码/本地/ZIP门槛PASS
Windows部署状态：P3A尚未Check/Apply/运行；文件存在不等于部署PASS

## 1. 当前唯一安装入口

不要手工散拷贝，不要执行`.reload eluna`。P2R1已关闭，当前只使用独立P3A包：

```text
G23P3A_delivery_20260822.zip
G23P3A_Check.cmd
G23P3A_Apply.cmd
G23P3A_Rollback.cmd（仅故障回滚）
```

P2/P2R1包保持不可变且禁止重复；P3A无需SQL或编译。

真实运行根目录由P0R锁定为：

```text
D:\TC-Build\bin\RelWithDebInfo
D:\TC-Build\bin\RelWithDebInfo\lua_scripts
```

G23-P2不修改F45 C++自动拾取、不需要编译、不重跑CMake。

## 2. 当前脚本与命令

| 文件 | 功能 | 命令/触发 |
|---|---|---|
| `extensions/G23Core.ext` | 共享命令表、权限过滤、原生命令透传、错误保护与DB验证层 | 自动 |
| `custom_server_assistant.lua` | 统一服务器助手及只读状态/健康页 | `.server` |
| `custom_gmhelp.lua` | 153条中文目录、11类无状态菜单、真实核心命令搜索 | `.gmhelp` |
| `custom_welcome.lua` | 登录欢迎、按权限过滤共享命令表导览 | `.help2` |
| `custom_announce.lua` | 仅世界state建立一个全服公告定时器 | 自动 |
| `custom_daily_reward.lua` | 数据库UUID令牌和唯一主键原子领取闸 | 登录自动 |
| `custom_teleport.lua` | 中文搜索、分页和完整安全门 | `.tp`、`.tp <关键词>` |
| `custom_diag.lua` | GM按需API/DB健康诊断；顶层不查库 | `.luadiag`（GM等级2） |
| `bignum_selftest.lua` | 必须API、大数值、数据库和精度自检 | `.bigtest` |
| `extensions/ObjectVariables.ext` | 历史失效对象扩展的同路径安全隔离件 | 自动 |

文件名仍不能以数字开头；Eluna会把开头数字解析成mapId。

## 3. P3A新增边界

### `.server`

裸`.server`及`status/commands/daily/tp/gmhelp/health`由Lua助手处理；`info/motd/restart/shutdown/set`、未知未来子命令与控制台输入通过`G23.PASS_THROUGH`交回TrinityCore，不能遮蔽原生命令。

### `.gmhelp`

新版游戏内`.gmhelp`优先于旧C++固定目录，提供153条去重项目/常用核心中文条目和11类无状态菜单；`.gmhelp find`按需联合查询`world.command(name,help)`。不在加载时查库，也不自动执行任何列表中的危险命令。回滚P3A后旧C++ `.gmhelp`自然恢复，无需重新编译。

`.help2`按玩家GM等级过滤Lua命令；普通玩家不再看到无权使用的`.gmhelp/.luadiag`。

## 4. P2/P2R1稳定性边界

### 公告

`custom_announce.lua`只在`GetStateMapId() == -1`的唯一世界state注册`ON_LUA_STATE_OPEN`回调并建立定时器。地图和副本state不注册公告回调，也不会因玩家登录重复建立全服定时器。

### 每日奖励

先导入：

```text
sql/G23P2_daily_reward_atomic.sql
```

`characters.custom_daily_reward_claim`使用`PRIMARY KEY (guid, claim_date)`作为数据库原子闸；每个Lua state先从数据库取得不同UUID令牌，只有实际写入主键的令牌所有者可以发奖。

- 不再使用异步`CharDBExecute`先发奖后写记录；
- 发奖失败时先补偿回滚金币，再验证删除pending claim，允许安全重试；
- 奖励发出后若最终DB确认失败，claim保持pending，宁可阻止重复也不二次发奖；
- `.luadiag`和`sql/G23P2_pending_claim_readonly.sql`可查看pending；不自动删除，避免把“已发奖但确认中断”变成重复领取。

SQL只使用标准DDL，无过程、函数、游标或自定义分隔符。

### 安全传送

所有命令、Gossip点击和最终`Teleport`前都会重新检查：

- 战斗；
- PVP标记、战场、竞技场和战场排队；
- 飞行、航线、坐骑和载具；
- 死亡/灵魂状态；
- 当前副本、团本和PVP地图；
- 目标地图和坐标；
- GM权限。

普通玩家只允许前往0、1、530、571四个开放世界地图；副本、团本及特殊地图目标至少需要GM等级2；GM等级3才可绕过动态安全门。P2R1不再保存跨点击session：分类和页码由Gossip参数编码，目标按`game_tele.id`重新查询，因此不存在跨Lua state丢失会话或假超时。

`.tp home`和`.tp back`仍只引导原版炉石/`.recall`，不伪造这版Eluna不存在的炉石读取API。

### ObjectVariables

P0R历史日志证明旧扩展会在Creature删除事件收到已失效userdata后调用`GetGUIDLow`。P0R同时证明当前部署的项目Lua没有任何`GetData`/`SetData`依赖。因此P2使用同路径安全隔离件：不注册旧对象方法，也不注册删除回调。字节级旧原件保存在交付包`original/`，可精确回滚。

### `.bigtest`

已删除`p53 + 1 ~= p53 or true`恒真断言，改为同时验证：

```text
2^53 + 1 == 2^53
2^53 - 1 ~= 2^53
```

必须API计入PASS；`os/io/debug/package/coroutine`只显示可选可用性，不再把可选标准库混入必须门槛。所有数据库查询只在执行`.bigtest`时发生。

### `.luadiag`

脚本加载阶段不查询数据库。只有GM等级2实际输入`.luadiag`后才检查API、每日奖励表、pending claim及传送表。

## 5. 传送数据库

中文传送仍依赖：

```text
sql/20_tele_cn.sql
sql/21_tele_cn_data.sql
```

Gossip sender固定使用9101–9105，避开套装系统1–11；分类列表每页28项，分类/页码无状态编码。关键词结果超过28项时提示缩小关键词，不依赖内存session翻页。

## 6. 本地验收

已通过：

```text
G23P2_LUAC_5_2_ALL=PASS
G23P2_LUA_MOCK_WORLD_STATE=PASS
G23P2_LUA_MOCK_MAP_STATE=PASS
G23P2_NO_TOP_LEVEL_DB=PASS
G23P2_ANNOUNCE_SINGLE_WORLD_TIMER=PASS
G23P2_DAILY_ATOMIC_MODEL=PASS
G23P2_TELEPORT_SAFETY_STATIC=PASS
G23P2_INSTALLER_SELFTEST=PASS
G23P2_INSTALLER_SELFTEST_OPTIMIZED=PASS
G23P2R1_CROSS_STATE_REPRO_OLD=PASS
G23P2R1_CROSS_STATE_FIXED_NEW=PASS
G23P2R1_TELEPORT_STATELESS_MOCK=PASS
G23P2R1_FULL_P2_INTEGRATION=PASS
G23P2R1_INSTALLER_SELFTEST=PASS
G23P2R1_RUNTIME=PASS_USER_CONFIRMED_TP_MENU
G23P3A_STATIC=PASS catalog=153
G23P3A_COMMAND_PASS_THROUGH_MOCK=PASS
G23P3A_GMHELP_DYNAMIC_AND_STATELESS_MOCK=PASS
G23P3A_NO_TOP_LEVEL_DB=PASS
G23P3A_FULL_P2R1_INTEGRATION=PASS
G23P3A_INSTALLER_SELFTEST=PASS
G23P3A_ZIP_CRC_AND_BYTE_COMPARE=PASS
```

P3A本地门槛不等于Windows运行PASS。当前只按P3A说明执行Check→停服→Apply→正常启动；无需SQL或编译。
