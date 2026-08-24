-- G17-B0 world安装后只读检查v3：显式world表、跨collation安全、单语句、单结果表、零写入。
-- 直接完整执行；所有表都写明world库，无需选择活动数据库，也不需要SET/会话变量。

SELECT CASE
  WHEN (SELECT COUNT(*) FROM `world`.`creature_template`
        WHERE `entry`=27756 AND `VehicleId`=70)<>1
    THEN 'BLOCKED_SOURCE_27756_NOT_EXACT_VEHICLE_70'
  WHEN (SELECT COUNT(*) FROM `world`.`creature_template`
        WHERE `entry`=1000171 AND `ScriptName` COLLATE utf8mb4_unicode_ci=_utf8mb4'npc_g17_dragonriding_vehicle' COLLATE utf8mb4_unicode_ci
          AND `VehicleId`=70 AND `modelid1`=25854 AND `name` COLLATE utf8mb4_unicode_ci=_utf8mb4'御空龙·B0' COLLATE utf8mb4_unicode_ci)<>1
    THEN 'FAILED_TARGET_POSTIMAGE'
  WHEN (SELECT COUNT(*) FROM `world`.`creature_template_spell`
        WHERE `CreatureID`=1000171)<>4
    OR (SELECT COUNT(*) FROM `world`.`creature_template_spell`
        WHERE (`CreatureID`,`Index`,`Spell`) IN
          ((1000171,0,9573),(1000171,1,55215),
           (1000171,2,52197),(1000171,3,53208)))<>4
    THEN 'FAILED_ACTION_BAR_POSTIMAGE'
  WHEN (SELECT COUNT(*) FROM `world`.`creature_template_movement`
        WHERE `CreatureId`=1000171 AND `Ground`=1 AND `Swim`=0
          AND `Flight`=1 AND `Rooted`=0 AND `Chase`=0 AND `Random`=0)<>1
    THEN 'FAILED_MOVEMENT_POSTIMAGE'
  WHEN (SELECT COUNT(*) FROM `world`.`spell_script_names`
        WHERE `ScriptName` COLLATE utf8mb4_unicode_ci IN
          (_utf8mb4'spell_g17_dragon_breath_energy' COLLATE utf8mb4_unicode_ci,
           _utf8mb4'spell_g17_dragon_accelerate_energy' COLLATE utf8mb4_unicode_ci,
           _utf8mb4'spell_g17_dragon_climb' COLLATE utf8mb4_unicode_ci,
           _utf8mb4'spell_g17_dragon_safe_landing' COLLATE utf8mb4_unicode_ci))<>4
    OR (SELECT COUNT(*) FROM `world`.`spell_script_names`
        WHERE ((`spell_id`=9573 AND `ScriptName` COLLATE utf8mb4_unicode_ci=_utf8mb4'spell_g17_dragon_breath_energy' COLLATE utf8mb4_unicode_ci)
          OR (`spell_id`=55215 AND `ScriptName` COLLATE utf8mb4_unicode_ci=_utf8mb4'spell_g17_dragon_accelerate_energy' COLLATE utf8mb4_unicode_ci)
          OR (`spell_id`=52197 AND `ScriptName` COLLATE utf8mb4_unicode_ci=_utf8mb4'spell_g17_dragon_climb' COLLATE utf8mb4_unicode_ci)
          OR (`spell_id`=53208 AND `ScriptName` COLLATE utf8mb4_unicode_ci=_utf8mb4'spell_g17_dragon_safe_landing' COLLATE utf8mb4_unicode_ci)))<>4
    THEN 'FAILED_SCRIPT_BINDING_POSTIMAGE'
  ELSE 'G17B0_WORLD_CHECK_PASS'
END AS `G17B0_RESULT`,
_utf8mb4'world' COLLATE utf8mb4_unicode_ci AS `database_name`,
(SELECT COUNT(*) FROM `world`.`creature_template` WHERE `entry`=27756) AS `source_rows`,
(SELECT COUNT(*) FROM `world`.`creature_template` WHERE `entry`=27756 AND `VehicleId`=70) AS `source_vehicle70_rows`,
(SELECT COUNT(*) FROM `world`.`creature_template` WHERE `entry`=1000171) AS `target_rows`,
(SELECT COUNT(*) FROM `world`.`creature_template`
 WHERE `entry`=1000171 AND `ScriptName` COLLATE utf8mb4_unicode_ci=_utf8mb4'npc_g17_dragonriding_vehicle' COLLATE utf8mb4_unicode_ci
   AND `VehicleId`=70 AND `modelid1`=25854 AND `name` COLLATE utf8mb4_unicode_ci=_utf8mb4'御空龙·B0' COLLATE utf8mb4_unicode_ci) AS `target_exact`,
(SELECT COUNT(*) FROM `world`.`creature_template_spell` WHERE `CreatureID`=1000171) AS `action_rows`,
(SELECT COUNT(*) FROM `world`.`creature_template_spell`
 WHERE (`CreatureID`,`Index`,`Spell`) IN
   ((1000171,0,9573),(1000171,1,55215),
    (1000171,2,52197),(1000171,3,53208))) AS `action_exact`,
(SELECT COUNT(*) FROM `world`.`creature_template_movement`
 WHERE `CreatureId`=1000171 AND `Ground`=1 AND `Swim`=0
   AND `Flight`=1 AND `Rooted`=0 AND `Chase`=0 AND `Random`=0) AS `movement_exact`,
(SELECT COUNT(*) FROM `world`.`spell_script_names`
 WHERE `ScriptName` COLLATE utf8mb4_unicode_ci IN
   (_utf8mb4'spell_g17_dragon_breath_energy' COLLATE utf8mb4_unicode_ci,
    _utf8mb4'spell_g17_dragon_accelerate_energy' COLLATE utf8mb4_unicode_ci,
    _utf8mb4'spell_g17_dragon_climb' COLLATE utf8mb4_unicode_ci,
    _utf8mb4'spell_g17_dragon_safe_landing' COLLATE utf8mb4_unicode_ci)) AS `script_rows`,
(SELECT COUNT(*) FROM `world`.`spell_script_names`
 WHERE ((`spell_id`=9573 AND `ScriptName` COLLATE utf8mb4_unicode_ci=_utf8mb4'spell_g17_dragon_breath_energy' COLLATE utf8mb4_unicode_ci)
   OR (`spell_id`=55215 AND `ScriptName` COLLATE utf8mb4_unicode_ci=_utf8mb4'spell_g17_dragon_accelerate_energy' COLLATE utf8mb4_unicode_ci)
   OR (`spell_id`=52197 AND `ScriptName` COLLATE utf8mb4_unicode_ci=_utf8mb4'spell_g17_dragon_climb' COLLATE utf8mb4_unicode_ci)
   OR (`spell_id`=53208 AND `ScriptName` COLLATE utf8mb4_unicode_ci=_utf8mb4'spell_g17_dragon_safe_landing' COLLATE utf8mb4_unicode_ci))) AS `script_exact`;
