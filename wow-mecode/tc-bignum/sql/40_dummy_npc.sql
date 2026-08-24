-- ============================================================
--  step23  打木桩 NPC 模板   entry 960010
--
--  用途：.dummy 召唤的测试假人
--    · 不还手（代码里 SetReactState(REACT_PASSIVE)）
--    · 血量拉满，正常打不死
--    · 不给经验、不掉落、不算击杀
--
--  【为什么 modelid1 用 3053 而不是官方训练假人的模型】
--    step21 踩过坑：随手填 displayid 15294 -> 无效 ID ->
--    服务端不校验、客户端静默失败 -> 只有召唤提示，看不到 NPC。
--    官方训练假人的 displayid 我【没有实测验证过】，不敢填。
--    3053（Kelstrum）是 step21 实测确认可见的，先用它。
--    等你确认了真正的假人模型 ID，改 modelid1 一个字段即可。
--
--  字段对齐 entry 70000 Lagretta（可见、可选中、中立）：
--    unit_flags 33088 / BaseAttackTime 0 / flags_extra 见下
--
--  npcflag = 0：木桩不需要对话
--  faction 35 = FACTION_FRIENDLY（SharedDefines.h:247）
--    -> 中立不主动攻击，但玩家可以手动攻击
--
--  本文件只有一条语句
-- ============================================================
REPLACE INTO `world`.`creature_template`
(`entry`,`difficulty_entry_1`,`difficulty_entry_2`,`difficulty_entry_3`,
 `KillCredit1`,`KillCredit2`,`modelid1`,`modelid2`,`modelid3`,`modelid4`,
 `name`,`subname`,`IconName`,`gossip_menu_id`,`minlevel`,`maxlevel`,`exp`,`faction`,`npcflag`,
 `speed_walk`,`speed_run`,`scale`,`rank`,`dmgschool`,`BaseAttackTime`,`RangeAttackTime`,
 `BaseVariance`,`RangeVariance`,`unit_class`,`unit_flags`,`unit_flags2`,`dynamicflags`,
 `family`,`type`,`type_flags`,`lootid`,`pickpocketloot`,`skinloot`,
 `PetSpellDataId`,`VehicleId`,`mingold`,`maxgold`,`AIName`,`MovementType`,`HoverHeight`,
 `HealthModifier`,`ManaModifier`,`ArmorModifier`,`DamageModifier`,`ExperienceModifier`,
 `RacialLeader`,`movementId`,`RegenHealth`,`mechanic_immune_mask`,`spell_school_immune_mask`,
 `flags_extra`,`ScriptName`,`StringId`,`VerifiedBuild`)
VALUES
(960010,0,0,0,0,0,3053,0,0,0,'测试木桩','DPS 测试','',0,80,80,0,31,0,
 1,1.14286,1,0,0,0,0,1,1,1,32832,2048,0,0,7,0,0,0,0,
 0,0,0,0,'',0,1,
 1000,1,1,1,0,
 0,0,1,0,0,
 66,'npc_bignum_dummy',NULL,-1);

-- ------------------------------------------------------------
--  v5 修正（用户实测：能打了但会死、是敌对红名、还是矮人）
--
--   1) ScriptName = 'npc_bignum_dummy'  <- 【新增，最关键】
--      挂上木桩 AI，借鉴官方 npc_training_dummy（npcs_special.cpp:1379）：
--          void DamageTaken(Unit*, uint32& damage, ...) { damage = 0; ... }
--      伤害在【扣血前】被抹成 0 -> 永远不死。不是靠血厚扛。
--      我们的 AI 在抹零【之前】先记账，所以「不会死，数据也在」。
--      没有这个 ScriptName，木桩就是个普通怪，一下就被打死。
--
--   2) faction 14 -> 31
--      14 = FACTION_MONSTER 能打，但【红名敌对】，用户要的是中立
--      31 = FACTION_PREY（SharedDefines.h:245）中立黄名，可打不主动
--           用例 npcs_special.cpp:231  me->SetFaction(FACTION_PREY);
--
--   3) 模型：代码侧改为【运行时自动探测】官方假人模型
--      不再依赖这里的 modelid1。见 cs_dummy.cpp 里 s_dummyModel 那段：
--      按 32546/32541/31144/32666/2673 顺序找库里存在的官方假人，
--      取其 Modelid1 覆盖。探不到才用下面这个 3053。
--
--  ---- 以下是 v4 的记录 ----
--  v4 修正：faction -> 14（用户重启后实测 1868 仍是绿名打不了）
--
--    35   = FACTION_FRIENDLY（SharedDefines.h:247）绿名，打不了
--    1868 = FACTION_MONSTER_SPAR_BUDDY  实测【仍然绿名】，
--           这个阵营在 3.3.5 客户端 FactionTemplate.dbc 里对玩家是友好的，
--           它只在 BT 阿卡玛那个特定场景下配合脚本用，不通用。
--    14   = FACTION_MONSTER（SharedDefines.h:242）
--           官方脚本里【把 NPC 变成可攻击】的标准做法，5 处用例：
--             zone_icecrown.cpp:72      me->SetFaction(FACTION_MONSTER);
--             boss_freya.cpp:228        me->SetFaction(FACTION_MONSTER);
--             zone_azuremyst_isle.cpp:235
--           红名敌对、可直接左键攻击。
--
--    注意：代码侧（cs_dummy.cpp v2）还会在召唤后【运行时再强制设一次】
--          faction，即使模板没生效也能保证可打 —— 双保险。
--
--  v3 修正 2（可选）：modelid1 换个不是矮人的模型
--
--    3053 是 NPCBot 的 Kelstrum，矮人男 —— step21 实测确认【可见】，
--    所以先用它保证能看到。你要是嫌矮人难看，改 modelid1 就行，
--    但【务必用你确认过能显示的 ID】：
--
--      UPDATE `world`.`creature_template` SET `modelid1` = <你的ID> WHERE `entry` = 960010;
--
--    step21 踩过坑：随手填 15294 -> 无效 ID -> 服务端不报错、
--    客户端静默失败 -> 只有召唤提示看不到 NPC。所以我不敢替你猜。
--
--    官方训练假人的模型（供参考，但我没在你的库里验证过）：
--      creature 32546「训练假人」/ 32541「战斗训练假人」
--    可以先查一下再用：
--      SELECT entry,name,modelid1 FROM world.creature_template
--       WHERE entry IN (32546,32541,32545,31144);
-- ------------------------------------------------------------

-- ------------------------------------------------------------
--  上面几个关键字段的意思（免得以后自己看不懂）
--
--   HealthModifier  1000  -> 血量放大 1000 倍，配合下面的 RegenHealth
--                            让它在大数值端也不容易被一击打死
--   RegenHealth        1  -> 自动回血，打不死
--   ExperienceModifier 0  -> 不给经验
--   lootid             0  -> 不掉东西
--   flags_extra       66  -> 【v2 修正】原来填 130 是错的，会隐形！
--
--     66 = 0x42 = CIVILIAN(0x02) + NO_XP(0x40)
--        CreatureData.h:182  CREATURE_FLAG_EXTRA_CIVILIAN = 0x02  不主动仇恨
--        CreatureData.h:187  CREATURE_FLAG_EXTRA_NO_XP    = 0x40  击杀不给经验
--
--     130 = 0x82 = CIVILIAN(0x02) + TRIGGER(0x80)   <- 我把 NO_XP 记成 0x80 了
--        CreatureData.h:188  CREATURE_FLAG_EXTRA_TRIGGER = 0x80  触发器
--        Creature.cpp:647    // trigger creature is always uninteractible
--                            if (IsTrigger()) SetUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
--        Creature.cpp:162    return 11686;   <- 触发器的隐形模型
--     => 触发器 NPC 是【故意设计成看不见、不能打】的，
--        所以会出现"提示召唤成功但看不到木桩"
--   BaseAttackTime     0  -> 对齐 Lagretta，不攻击
--   unit_flags     32832  -> 【重要】不是 33088！
--
--     服务 NPC 用的 33088 含 UNIT_FLAG_IMMUNE_TO_PC(0x100)
--     （UnitDefines.h:143 "disables combat with PlayerCharacters"）
--     木桩挂了这个位【玩家根本打不动】，DPS 永远是 0。
--
--     33088 = 0x40 + 0x100(IMMUNE_TO_PC) + 0x8000(CAN_SWIM)
--     32832 = 0x40             + 0x8000(CAN_SWIM)      <- 去掉 IMMUNE_TO_PC
--
--     不还手靠代码里的 SetReactState(REACT_PASSIVE)（Creature.h:134）
--     和 SetImmuneToNPC(true)，不靠这个 flag。
-- ------------------------------------------------------------

-- ------------------------------------------------------------
--  自检
-- ------------------------------------------------------------
SELECT `entry`,`name`,`modelid1`,`faction`,`npcflag`,`flags_extra`,`HealthModifier`
FROM `world`.`creature_template`
WHERE `entry` = 960010;
-- 期望 1 行：960010 / 测试木桩 / 3053 / 35 / 0 / 130 / 1000
