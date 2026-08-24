# G17-B0 Windows真实前像结论与预检步骤

更新时间：2026-08-22

## 1. 已接收并验证的真实证据

- GitHub提交：`cc3c2194ea3e910a95129941a4a8f0c15cf4fb90`；
- 用户结果包：`G17B0_LOCK_RESULT_20260822_120243.zip`；
- 下载字节数：`73265`；
- SHA-256：`ece3d768d3d46f7e573a76ac0527d439ab932c0b831ed923f3de6c600ddaee0c`；
- ZIP：10项、CRC全部通过、路径穿越检查通过；
- 用户源码：`bignum-mod` / `ae60f5b6f6ffe9a0426dfeb6e227712de7d6c8b7`；
- 探针状态：`G17B0_LOADER_STATE=PRE`、`G17B0_NEW_SOURCE_ABSENT=True`、`G17B0_NARROW_SOURCE_LOCK_READY=True`；
- 探针明确：未查询live DB，未修改`D:\TrinityCore`。

真实loader包含大量项目自定义注册，禁止整文件套用上游。锁定前像：

```text
cs_script_loader.cpp
size=5589
sha256=2a4895a32532f3c6c2c6dc3096fced4bff6d53c39dd3787bd81a76653d42f3f7
BOM=False / CRLF
```

`cs_dragonriding.cpp`真实状态为缺席。用户`CMakeLists.txt`使用`CollectSourceFiles`收集Commands目录，因此无需修改CMake；`ScriptLoader.h`也无需修改。

## 2. 已生成的结构锚点安装器

`install_g17b0_source.py`只管理两个路径：

1. 新建`src/server/scripts/Commands/cs_dragonriding.cpp`；
2. 在真实`cs_script_loader.cpp`唯一的`AddSC_wp_commandscript`声明和调用后各插入一行。

它不会覆盖上游loader，也不修改`ScriptLoader.h`或`CMakeLists.txt`。双门控包括：

- loader真实前像/后像精确SHA；
- wp声明/调用唯一结构锚点；
- 新cpp缺席或精确payload SHA；
- 7个真实API/CMake/schema上下文文件精确SHA；
- 原子写入、loader备份、异常恢复；
- 回滚只删除精确匹配的G17-B0 cpp，拒绝删除未知文件。

精确后像：

```text
loader post sha256=5502e5b4e22535957f3db81083530b048ec33f6852f4697fbe55795628cee5cc
cs_dragonriding.cpp sha256=c9535dca3390ece6735e6ff6b7418ed99ff206628b5e8febd7b78b05cba999bd
```

本地Check→Apply→Check→Rollback→Check、两个负例均PASS。C++还使用用户探针捕获的`CombatAI.h/ScriptMgr.h/Unit.h/Map.h`覆盖上游头文件执行GCC 14/C++20单翻译单元语法检查，结果PASS。它仍不等于Windows VS2022完整构建PASS。

## 3. 当前只执行预检，不执行Apply

当前Apply由缺席的`G17B0_APPLY_APPROVED.txt`硬锁。数据库只读资源探针未审完前，即使手工调用`--apply`也会失败关闭。

当前只执行：

1. 双击`Run-G17B0-Source-Preflight.cmd`，它会隔离执行SelfTest，再对`D:\TrinityCore`执行只读源码Check；
2. 自动结果位于`C:\Users\Administrator\Downloads\workspace\uploads\G17B0_SOURCE_PREFLIGHT_RESULT.txt`；
3. 在DBeaver真实world库执行`sql/G17B0_world_probe_readonly.sql`；该文件是单语句、单结果表；
4. 将结果表导出为UTF-8 `G17B0_DB_PROBE_RESULT.txt`；
5. 将上述两个txt上传GitHub后告知文件名。个别调试时才分别使用`G17B0_SelfTest.cmd`和`G17B0_Check.cmd`。

期望源码标志：

```text
G17B0_INSTALLER_SELF_TEST_PASS=True
G17B0_SOURCE_STATE=READY_TO_APPLY
G17B0_CHECK_SOURCE_EDITS=0
```

期望数据库关键标志之一：

```text
G17B0_DB_PREIMAGE_READY
```

如果是`BLOCKED_SOURCE_27756_MISSING_OR_NOT_VEHICLE`或`BLOCKED_TARGET_1000171_COLLISION`，不要导入安装SQL。

## 4. 仍未通过的边界

- live world资源结果未回传；
- 候选entry、VehicleId、动作条、移动和法术尚未冻结；
- install/check/rollback SQL尚未在真实MySQL/MariaDB执行；
- Windows完整编译、链接和新二进制运行未执行；
- 游戏内座位、动作条、四技能、禁区和清理矩阵未执行；
- B1/B2未完成。
