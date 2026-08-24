# G17-B0 live world只读探针v2真实结果验收

日期：2026-08-22（Asia/Shanghai）

## GitHub来源与原件

- 提交：`abd3c2179136f19231112d6177f0f42c2d1e6285`
- 父提交：`db131492aef1fde77f731c08fe046205071b3b16`
- 提交时间：2026-08-22 13:42:03（Asia/Shanghai）
- Git blob：`8e3550e097960149aa3f2fcd029448ed517fa9b0`
- 用户文件：`_G17_B0_world库只读资源探针_v2_跨collation安全_单语句_单结果表_修复v1在information_s_202608221340.txt`
- 本地原样归档：`tc-bignum/规划/G17_飞行与移动/证据/G17B0_DB_PROBE_RESULT_20260822_1340.txt`
- 大小：`17472`
- SHA-256：`af4a6031f4895767c34faf1211bb5ba8d026039beefa0d8cd79b52fe5a8a470e`
- 编码/换行：ASCII / pure CRLF
- 物理行：52；结果数据行：50

离线验收器：`tc-bignum/规划/G17_飞行与移动/accept_g17b0_db_probe.py`。它锁定原件大小/哈希、表格结构、行数、markers、表/列集合、source/target/动作条/移动及summary，不修改数据库。

## v2执行已经成功

```text
G17B0_DB_PROBE_START
DATABASE=world
DATABASE_VERSION=8.0.46
PROBE_TIME=2026-08-22 13:40:46
G17B0_DB_PREIMAGE_READY
G17B0_DB_PROBE_COMPLETE
```

这证明：

- 用户活动数据库确实是`world`；
- v1的1046选库问题和1271 collation问题均已关闭；
- v2完整执行并返回首尾marker，不再要求用户重跑DB探针。

## schema与资源前像

7张目标表均存在、均为InnoDB、均报告`utf8mb4_unicode_ci`：

```text
creature_template
creature_template_spell
creature_template_movement
npc_spellclick_spells
spell_dbc
spell_script_names
vehicle_template_accessory
```

30个安装/检查所需列全部出现。关键实际类型包括：

```text
creature_template.entry=int unsigned
creature_template.VehicleId=int unsigned
creature_template.ScriptName=char(64)
creature_template_spell.CreatureID=int unsigned
creature_template_spell.Index=tinyint unsigned
creature_template_spell.Spell=int unsigned
creature_template_movement.CreatureId=int unsigned
spell_script_names.spell_id=int
spell_script_names.ScriptName=char(64)
```

## source 27756冻结结果

```text
source_rows=1
source_vehicle_rows=1
entry=27756
name=Ruby Drake
models=25854,0,0,0
level=80-80
faction=35
VehicleId=70
MovementType=0
HoverHeight=1
flags_extra=64
ScriptName=npc_ruby_emerald_amber_drake
VerifiedBuild=12340
```

动作条原像：

```text
index 0 -> 50232
index 1 -> 50240
index 2 -> 50253
index 5 -> 53389
```

移动原像：

```text
Ground=0
Swim=1
Flight=1
Rooted=0
Chase=0
Random=0
Pause=NULL
```

## target与冲突门

```text
target_rows=0
target_actions=0
candidate_script_bindings=0
```

结果中没有seq 80/90/100/110数据行；结合7张表均已列出，表示候选entry没有spellclick/accessory、四个自定义ScriptName没有既有绑定、`spell_dbc`没有这四个ID的world覆盖行。它只证明world表无冲突；基础客户端/服务端DBC法术是否可实际施放仍必须由后续worldserver加载、编译和游戏运行验收，不能在这里冒充Runtime PASS。

## 验收结论与边界

```text
G17B0_DB_PROBE_ACCEPTANCE=PASS
G17B0_DB_PREIMAGE_READY=True
G17B0_SOURCE_PREFLIGHT=PASS
G17B0_SOURCE_AND_DB_APPROVED=True
G17B0_SOURCE_APPLY=NOT_RUN
G17B0_WORLD_INSTALL=NOT_RUN
G17B0_WINDOWS_BUILD=NOT_RUN
G17B0_RUNTIME=NOT_RUN
```

数据库结果足以解除“受控源码Apply交付包”的审批锁；不等于已修改源码、不等于已导入world SQL、不等于编译或游戏验收完成。
