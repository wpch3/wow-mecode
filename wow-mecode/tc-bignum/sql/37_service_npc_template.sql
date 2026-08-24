-- ============================================================
--  step21 v4.0  便携服务 NPC 模板   entry 960001-960007
--
--  v4.0 修正（用户实测：其他五个都好了，就差邮箱，还是个人类且没功能）
--    根因：960004 的 npcflag 写的是 1（只有 GOSSIP），漏了 MAILBOX。
--          上一版误判「邮箱不能用 NPC 做」，就没给它挂 MAILBOX 位，
--          结果它退化成一个只会打招呼的人类 NPC（模型 1300 是 Lyria）。
--
--    上一版那个结论是【错的】，现已全量源码复查更正：
--      MailHandler.cpp:54  else if (guid.IsAnyTypeCreature())
--      MailHandler.cpp:56      GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_MAILBOX)
--    官方银之侍从就是 NPC 邮箱：
--      npcs_special.cpp:2091  me->SetNpcFlag(UNIT_NPC_FLAG_MAILBOX);
--
--    修法：npcflag 1 -> 67108865
--          = GOSSIP(1) + MAILBOX(0x04000000 = 67108864)
--
--    另外代码侧（cs_worldtools.cpp v4.0）加了双保险：
--    召唤邮差时会运行时探测库里真实存在的邮箱 GameObject 一起放，
--    所以就算你的 TDB 缺 GO 184137 也不影响。
--
--  v3.2 修正（用户实测：有召唤提示但看不到 NPC）
--    根因：modelid 15294 是我随手填的，从未验证 ——
--          displayid 无效 = 模型加载不出来 = 隐形。
--    修法：改用 NPCBot 模板里【实际在用、确认可见】的 displayid
--          （来源 sql/custom/world/npcbot_2000_00_00_00_creature_template.sql）
--    同时对齐 entry 70000 Lagretta（可见可对话的中立NPC）的关键字段：
--          unit_flags = 33088、BaseAttackTime = 0、flags_extra = 0
--
--  为什么必须独立 entry：
--    客户端按 entry 缓存名字和 npcflag（CreatureCache），
--    服务端 SetName + ReplaceAllNpcFlags 骗不过它。
--
--  npcflag 取值（UnitDefines.h:237 起）：
--    GOSSIP 1 / TRAINER 16 / VENDOR 128 / REPAIR 4096
--    INNKEEPER 65536 / BANKER 131072 / AUCTIONEER 2097152 / MAILBOX 67108864
--
--  unit_flags 33088 = IMMUNE_TO_PC(256) + IMMUNE_TO_NPC(512) + 32320其余
--    这是 Lagretta 在用的值，实测可见、可点、不被打
--
--  注意：本文件只有一条语句
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
(960001,0,0,0,0,0,3343,0,0,0,'便携拍卖师','随行服务','',0,80,80,0,35,2097153,
 1,1.14286,1,0,0,0,0,1,1,1,33088,2048,0,0,7,0,0,0,0,
 0,0,0,0,'',0,1,1,1,1,1,1,0,0,1,0,0,0,'',NULL,-1),
(960002,0,0,0,0,0,3399,0,0,0,'便携银行家','随行服务','',0,80,80,0,35,131073,
 1,1.14286,1,0,0,0,0,1,1,1,33088,2048,0,0,7,0,0,0,0,
 0,0,0,0,'',0,1,1,1,1,1,1,0,0,1,0,0,0,'',NULL,-1),
(960003,0,0,0,0,0,3431,0,0,0,'便携修理匠','随行服务','',63003,80,80,0,35,4225,
 1,1.14286,1,0,0,0,0,1,1,1,33088,2048,0,0,7,0,0,0,0,
 0,0,0,0,'',0,1,1,1,1,1,1,0,0,1,0,0,0,'',NULL,-1),
(960004,0,0,0,0,0,1300,0,0,0,'便携邮差','随行服务','',0,80,80,0,35,67108865,
 1,1.14286,1,0,0,0,0,1,1,1,33088,2048,0,0,7,0,0,0,0,
 0,0,0,0,'',0,1,1,1,1,1,1,0,0,1,0,0,0,'',NULL,-1),
(960005,0,0,0,0,0,1578,0,0,0,'便携旅店老板','随行服务','',0,80,80,0,35,65537,
 1,1.14286,1,0,0,0,0,1,1,1,33088,2048,0,0,7,0,0,0,0,
 0,0,0,0,'',0,1,1,1,1,1,1,0,0,1,0,0,0,'',NULL,-1),
(960007,0,0,0,0,0,3053,0,0,0,'便携商人','随行服务','',63007,80,80,0,35,129,
 1,1.14286,1,0,0,0,0,1,1,1,33088,2048,0,0,7,0,0,0,0,
 0,0,0,0,'',0,1,1,1,1,1,1,0,0,1,0,0,0,'',NULL,-1)
