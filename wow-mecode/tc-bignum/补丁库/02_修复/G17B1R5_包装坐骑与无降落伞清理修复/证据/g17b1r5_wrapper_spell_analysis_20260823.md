# G17-B1R5 包装坐骑法术链精确调查（2026-08-23）

## 输入与口径

- 客户端真实 DBC：`g17r5_lifecycle/create/DBFilesClient/Spell.dbc`
- SHA-256：`dd25091167f671764735ce88c78b66485c6d661fadf05d322574c261f6e464ea`
- WDBC 头：`records=49839`、`fields=234`、`record_size=936`、`string_block=2307035`
- 服务端锁定基线：`328950225/TrinityCore-NPCBOT-Eluna-zhCN` commit `4e8762ee2b00948fa103d0cd1afd78ccdf4364fb`
- `SpellMgr.cpp` SHA-256：`4cf92fb8ecfca032f3f9bdd4645facfebd3e0e5fbafc6d332a1ca4cbc8c3e5ee`
- `spell_generic.cpp` SHA-256：`abb57958a484c763405cf84adda81c1b14efc536bba7b5b9dc395b96e7393c48`

字段含义：Effect 6=`SPELL_EFFECT_APPLY_AURA`，Effect 77=`SPELL_EFFECT_SCRIPT_EFFECT`；Aura 4=`SPELL_AURA_DUMMY`，Aura 32=`SPELL_AURA_MOD_INCREASE_MOUNTED_SPEED`，Aura 78=`SPELL_AURA_MOUNTED`，Aura 207=`SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED`。

## DBC 精确记录

### 无头骑士的坐骑

- 外层已学习法术 48025：Effect=[6,6,77]；Aura=[78,4,0]；MiscValue=[27153,27153,0]；TriggerSpell=[0,0,0]。
- 内层 60% 法术 51621：Effect=[6,6,0]；Aura=[78,32,0]；MiscValue=[27153,0,0]；TriggerSpell=[0,0,0]。
- 内层 100% 法术 48024：Effect=[6,6,0]；Aura=[78,32,0]；MiscValue=[27153,0,0]；TriggerSpell=[0,0,0]。
- 内层 150% 法术 51617：Effect=[6,6,6]；Aura=[78,32,207]；MiscValue=[27153,0,0]；TriggerSpell=[0,0,0]。
- 内层 280% 法术 48023：Effect=[6,6,6]；Aura=[78,32,207]；MiscValue=[27153,0,0]；TriggerSpell=[0,0,0]。

### 爱情火箭

- 外层已学习法术 71342：Effect=[6,6,77]；Aura=[78,4,0]；MiscValue=[38204,0,0]；TriggerSpell=[0,0,0]。
- 内层 0 法术 71343：Effect=[6,0,0]；Aura=[78,0,0]；MiscValue=[38204,0,0]；TriggerSpell=[0,0,0]。
- 内层 60% 法术 71344：Effect=[6,6,0]；Aura=[78,32,0]；MiscValue=[38204,0,0]；TriggerSpell=[0,0,0]。
- 内层 100% 法术 71345：Effect=[6,6,0]；Aura=[78,32,0]；MiscValue=[38204,0,0]；TriggerSpell=[0,0,0]。
- 内层 150% 法术 71346：Effect=[6,6,6]；Aura=[78,207,32]；MiscValue=[38204,0,0]；TriggerSpell=[0,0,0]。
- 内层 310% 法术 71347：Effect=[6,6,6]；Aura=[78,207,32]；MiscValue=[38204,0,0]；TriggerSpell=[0,0,0]。

## 服务端真实施法链

1. 两个外层法术的 DBC `TriggerSpell=[0,0,0]`，因此内层关系不在 DBC TriggerSpell 字段中。
2. `spell_generic.cpp` 的通用 `spell_gen_mount` 在外层法术的 EFFECT_2 `SPELL_EFFECT_SCRIPT_EFFECT` 命中时，根据骑术等级、区域飞行许可和 310% 能力选择内层法术，再由玩家触发施放内层法术。
3. 无头骑士映射：`51621 / 48024 / 51617 / 48023`；爱情火箭映射：`71343 / 71344 / 71345 / 71346 / 71347`。
4. `SpellMgr.cpp` 对这类包装坐骑执行共同修复：把外层 EFFECT_0 和 EFFECT_1 的 `Effect` 改成 `SPELL_EFFECT_NONE`，避免 NO_TARGET 对当前施法者错误叠加直接 Aura。
5. 该修复只改 `Effect`，原始 `ApplyAuraName` metadata remains；所以运行时外层 `SpellInfo::HasAura(SPELL_AURA_MOUNTED)` 为假，但 `GetEffect(EFFECT_0).ApplyAuraName` 仍为 78。
6. 内层真正生成 `SPELL_AURA_MOUNTED`，但内层 ID 不是玩家法术书直接学习的 ID。

## B1R4 漏识别根因

B1R4 同时要求：

- 当前施放 `SpellInfo::HasAura(SPELL_AURA_MOUNTED)`；
- 玩家 `HasSpell(info->Id)`；
- 延迟后查找 `effect->GetId() == 外层法术 ID` 的 Mounted Aura。

包装链三处都不满足：外层的直接 Aura Effect 已被服务端改为 `SPELL_EFFECT_NONE`；内层不是玩家直接学习法术；最终活动 Mounted Aura 的 ID 是内层而非外层。

## 通用修复结论

- 候选门读取每个 effect 保留的 `ApplyAuraName == SPELL_AURA_MOUNTED`，同时覆盖普通直接坐骑和被 SpellMgr 清空 Effect 的包装坐骑；不按名称或法术 ID 硬编码。
- 玩家所有权仍由已学习的外层施放法术验证。
- 延迟接管以玩家当前活动的任意 `SPELL_AURA_MOUNTED` 为运行时权威，使用该内层 Aura 的 `MiscValue` 取得坐骑生物 entry，并使用玩家当前 `GetMountDisplayId()` 保留实际模型。
- 非坐骑法术必须同时通过 metadata 候选门、成功 Mounted 状态和活动 Mounted Aura 三重结果门，因此普通脚本法术和非坐骑变身不会接管。
