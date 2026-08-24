# G17-B0 Windows窄探针真实结果验收

日期：2026-08-22

## GitHub来源

- 提交：`cc3c2194ea3e910a95129941a4a8f0c15cf4fb90`
- 文件：仓库根目录`G17B0_LOCK_RESULT_20260822_120243.zip`
- Git blob：`cd8aab951b72cc25eb4bdfa242c609eb874c35d0`
- 下载大小：`73265`
- 下载SHA-256：`ece3d768d3d46f7e573a76ac0527d439ab932c0b831ed923f3de6c600ddaee0c`
- ZIP：10项，CRC PASS，路径穿越检查PASS

本地保留原包：`tc-bignum/规划/G17_飞行与移动/证据/G17B0_LOCK_RESULT_20260822_120243.zip`。

## 用户真实源码状态

```text
branch=bignum-mod
HEAD=ae60f5b6f6ffe9a0426dfeb6e227712de7d6c8b7
G17B0_LOADER_STATE=PRE
G17B0_LOADER_WP_ANCHORS_READY=True
G17B0_NEW_SOURCE_ABSENT=True
G17B0_LIVE_DB_NOT_QUERIED=True
G17B0_PROBE_WRITES_SOURCE=False
G17B0_NARROW_SOURCE_LOCK_READY=True
```

loader前像：

```text
path=src/server/scripts/Commands/cs_script_loader.cpp
size=5589
sha256=2a4895a32532f3c6c2c6dc3096fced4bff6d53c39dd3787bd81a76653d42f3f7
BOM=False
EOL=CRLF
```

用户loader含大量自定义脚本声明/调用，已再次证明禁止上游整文件覆盖。

## API/schema锁定

探针确认下列关键锚点各1个：`VehicleAI`、`OnMapChanged`、`OnUpdateZone`、`EnterVehicle`、`IsDungeon`、`IsBattlegroundOrArena`及四张目标world表。

捕获的真实`CMakeLists.txt`使用`CollectSourceFiles`自动收集Commands源文件，因此不改CMake；`ScriptLoader.h`不是本批注册所需，也保持不改。

使用用户捕获的`CombatAI.h/ScriptMgr.h/Unit.h/Map.h`覆盖参考树对应头文件后，GCC 14/C++20对`cs_dragonriding.cpp`单翻译单元语法检查PASS。

## 后续产物与状态

- 已生成真实loader结构后像：`5502e5b4e22535957f3db81083530b048ec33f6852f4697fbe55795628cee5cc`；
- 已生成双哈希/唯一锚点、原子备份与回滚安装器；
- 本地Check→Apply→Check→Rollback→Check和2个负例PASS；
- 已生成只读world资源探针；
- 预检ZIP：`G17B0_Preflight_20260822.zip`，95950字节，SHA-256 `4affab7339c4ef636baae04175fdfc17723da98943a049bdd942fb016528132c`；
- 新增一键只读`Run-G17B0-Source-Preflight.cmd`，自动生成`workspace/uploads/G17B0_SOURCE_PREFLIGHT_RESULT.txt`；world SQL改为单语句/单结果表，便于一次导出；
- 预检ZIP不含Apply wrapper和审批标志，直接调用Apply也会因数据库尚未审阅而失败关闭。

## Windows真实源码预检结果验收

用户随后通过GitHub提交`db131492aef1fde77f731c08fe046205071b3b16`上传仓库根目录`G17B0_SOURCE_PREFLIGHT_RESULT.txt`。本地原样保存为：

`tc-bignum/规划/G17_飞行与移动/证据/G17B0_SOURCE_PREFLIGHT_RESULT_20260822.txt`

固定元数据：

```text
size=1914
sha256=724d58ee39a09767fb0ccb054b4b23c906d73e9e2ef499a61cf66ea3f78314b0
SOURCE_ROOT=D:\TrinityCore
G17B0_SELFTEST_RC=0
G17B0_CHECK_RC=0
G17B0_INSTALLER_SELF_TEST_PASS=True
G17B0_NEGATIVE_FIXTURES=2
G17B0_SOURCE_STATE=READY_TO_APPLY
G17B0_CHECK_SOURCE_EDITS=0
```

SelfTest在临时副本中完成Check→Apply→Check→Rollback→Check，真实源码上的最终只读Check仍为PRE/目标cpp缺席，7项上下文锁匹配，未改用户源码。结论：

```text
WINDOWS_SOURCE_CHECK=PASS
WINDOWS_SOURCE_MODIFIED_BY_PREFLIGHT=False
SOURCE_PREFLIGHT_RERUN_REQUIRED=False
```

不得再要求用户重跑这个CMD或重新上传源码预检结果。

## live world只读探针状态

第一次在DBeaver执行只读SQL得到：

```text
SQL Error [1046] [3D000]: No database selected
```

用户随后确认实际库名就是`world`并正确选库；第二次执行进入查询解析，但得到：

```text
SQL Error [1271] [HY000]: Illegal mix of collations for operation 'UNION'
```

这证明1046已解决。1271是v1探针将`information_schema`元数据字符串与world表不同charset/collation直接`UNION ALL`时的兼容缺陷，不是用户选错库，也不是目标entry、VehicleId或法术冲突；查询仍未返回live资源行。

已新增只读v2：

```text
tc-bignum/规划/G17_飞行与移动/G17B0_world_probe_readonly_v2_collation_safe.sql
size=9390
sha256=9d767115ecdc5983941e99ba900e7de43370d302ad8b6b837230f32ba9884310
```

v2在16个SELECT分支（15个`UNION ALL`）的三个文本结果列上统一使用`CONVERT(... USING utf8mb4) COLLATE utf8mb4_unicode_ci`，总计48组显式转换；仍为单SELECT、单结果表、无写语句、无`SET`/`USE`。本地只读合同、15个UNION边界、交付/正式副本逐字节一致和定界符检查均PASS。

用户随后通过提交`abd3c2179136f19231112d6177f0f42c2d1e6285`回传v2结果。17472字节、SHA-256 `af4a6031f4895767c34faf1211bb5ba8d026039beefa0d8cd79b52fe5a8a470e`、50数据行，首尾marker和`G17B0_DB_PREIMAGE_READY`齐全；world/MySQL 8.0.46、source 27756 VehicleId70、target 1000171为0行、候选ScriptName绑定0。离线验收PASS，详见`g17b0_db_probe_v2_acceptance_20260822.md`。

v1/v2探针均已关闭，禁止重复。当前为：`WINDOWS_SOURCE_CHECK=PASS`、`WINDOWS_LIVE_DATABASE_PROBE=PASS`、`G17B0_SOURCE_AND_DB_APPROVED=True`、`G17B0_WINDOWS_SOURCE_APPLY=NOT_RUN`、`G17B0_WORLD_INSTALL=NOT_RUN`、`WINDOWS_BUILD=NOT_RUN`、`WINDOWS_RUNTIME=NOT_RUN`。仅新受控Source Apply包含审批；历史预检包仍锁定。
