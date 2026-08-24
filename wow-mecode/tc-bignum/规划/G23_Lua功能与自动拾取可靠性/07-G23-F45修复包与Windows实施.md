# G23-F45修复包与Windows实施

更新时间：2026-08-22  
状态：F45本地门槛PASS；用户确认Windows Check、Apply、VS2022编译及正常使用运行烟雾均成功；群拾播报可见且观察到的拾取数量一致，F45转被动观察

## 1. 当前结论

用户已明确给出足够的真人症状：同一环境有时完整、有时不完整、有时只剩一具，怪物种类越多越明显。06号R1–R4结构化计数已取消，不再要求数尸体、重复`.gps`或人为构造四场景。

P0R真实C++已直接确认：

- 群拾入口错误依赖物品槽成功入包；
- 组队周围金币漏聚合，Custom金币又绕过分金链；
- roll/master按整尸跳过；
- 纯NPCBot真Group被误当真人roll组；
- 邮件兜底对所有库存错误过宽；
- 旧播报无法区分权限、旗标、roll、背包和上限。

Lua不是AoE Loot主链。F45不修改Lua或`.ext`；Lua稳定性继续留在独立P2。

## 2. F45交付入口

完整包：

```text
F45_delivery_20260822.zip
size=38016
sha256=469e55b028544d31cd9c812269a6229b99e080aef0ab8be76870955cf12982a8

tc-bignum/补丁库/02_修复/F45_AoE拾取随机漏尸与组队金币语义/
```

ZIP已通过CRC、13文件唯一性及包内逐字节对照。

权威安装文档：

```text
tc-bignum/补丁库/02_修复/F45_AoE拾取随机漏尸与组队金币语义/安装说明.md
```

Windows只按顺序：

1. 正常停服；
2. 双击`F45_Check.cmd`；
3. Check为`READY_TO_APPLY`时双击`F45_Apply.cmd`；
4. 使用现有`D:\TC-Build\TrinityCore.sln`增量编译worldserver，RelWithDebInfo/x64；
5. 正常启动。

无需重跑CMake、无需SQL、无需改conf、无需reload、无需R1–R4矩阵。

## 3. 本地门槛

```text
F45_SOURCE_GATES=PASS
F45_BEHAVIOR_MODEL=PASS
F45_INSTALLER_SELFTEST=PASS
F45_FULL_HEADER_SYNTAX_CORE=PASS
F45_FULL_HEADER_SYNTAX_ELUNA=PASS
```

语法门槛覆盖`CustomAoELoot.cpp`与P0R真实`LootHandler.cpp`后像，使用上游`NPCBOT-Eluna-zhCN-2026`完整头文件和Eluna子模块。它不是Windows最终链接或运行PASS。

证据：

```text
tc-bignum/规划/G23_Lua功能与自动拾取可靠性/证据/f45_local_acceptance_20260822.txt
```

## 4. 运行反馈已简化

启动后照平常刷怪即可。新的单行播报包含成功与保留原因；若仍偶发剩一具，只发当次播报原文，不再做结构化矩阵。

遇到重复物品/金币、roll物品被直接取走、异常邮件或崩服，立即停服，双击`F45_Rollback.cmd`并重新编译。

## 5. 当前状态标记

用户原始摘要为“两项都成功了，输出一致，编译也成功了”。未粘贴逐行输出和新exe哈希，因此按用户明确确认粒度归档，不补造具体数值；Check/Apply/编译禁止重复。

```text
G23_R1_R4_REQUIRED=False
F45_LOCAL_ACCEPTANCE=PASS
F45_WINDOWS_CHECK=PASS_USER_CONFIRMED
F45_WINDOWS_APPLY=PASS_USER_CONFIRMED
F45_WINDOWS_BUILD=PASS_USER_CONFIRMED
F45_RUNTIME=PASS_USER_CONFIRMED_NORMAL_USE
F45_ANNOUNCE_VISIBLE=PASS_USER_CONFIRMED
F45_OBSERVED_LOOT_COUNT_MATCH=PASS_USER_CONFIRMED
F45_REPEAT_CHECK_APPLY_BUILD=False
G23_CURRENT_NEXT=G23P3A_WINDOWS_INSTALL
```

安装/编译摘要证据：`证据/f45_windows_check_apply_build_user_summary_20260822.txt`。  
正常使用运行烟雾证据：`证据/f45_runtime_user_smoke_20260822.txt`。
