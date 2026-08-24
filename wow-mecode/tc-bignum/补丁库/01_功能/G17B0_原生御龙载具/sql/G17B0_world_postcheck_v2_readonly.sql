-- G17-B0 world安装后只读检查v2：单语句、单结果表、零写入。
-- DBeaver选中world后完整执行；无需SET/会话变量。

SELECT CASE
  WHEN (SELECT COUNT(*) FROM `creature_template`
        WHERE `entry`=27756 AND `VehicleId`=70)<>1
    THEN 'BLOCKED_SOURCE_27756_NOT_EXACT_VEHICLE_70'
  WHEN (SELECT COUNT(*) FROM `creature_template`
        WHERE `entry`=1000171 AND `ScriptName`='npc_g17_dragonriding_vehicle'
          AND `VehicleId`=70 AND `modelid1`=25854 AND `name`='御空龙·B0')<>1
    THEN 'FAILED_TARGET_POSTIMAGE'
  WHEN (SELECT COUNT(*) FROM `creature_template_spell`
        WHERE `CreatureID`=1000171)<>4
    OR (SELECT COUNT(*) FROM `creature_template_spell`
        WHERE (`CreatureID`,`Index`,`Spell`) IN
          ((1000171,0,9573),(1000171,1,55215),
           (1000171,2,52197),(1000171,3,53208)))<>4
    THEN 'FAILED_ACTION_BAR_POSTIMAGE'
  WHEN (SELECT COUNT(*) FROM `creature_template_movement`
        WHERE `CreatureId`=1000171 AND `Ground`=1 AND `Swim`=0
          AND `Flight`=1 AND `Rooted`=0 AND `Chase`=0 AND `Random`=0)<>1
    THEN 'FAILED_MOVEMENT_POSTIMAGE'
  WHEN (SELECT COUNT(*) FROM `spell_script_names`
        WHERE `ScriptName` IN
          ('spell_g17_dragon_breath_energy','spell_g17_dragon_accelerate_energy',
           'spell_g17_dragon_climb','spell_g17_dragon_safe_landing'))<>4
    OR (SELECT COUNT(*) FROM `spell_script_names`
        WHERE (`spell_id`,`ScriptName`) IN
          ((9573,'spell_g17_dragon_breath_energy'),
           (55215,'spell_g17_dragon_accelerate_energy'),
           (52197,'spell_g17_dragon_climb'),
           (53208,'spell_g17_dragon_safe_landing')))<>4
    THEN 'FAILED_SCRIPT_BINDING_POSTIMAGE'
  ELSE 'G17B0_WORLD_CHECK_PASS'
END AS `G17B0_RESULT`,
DATABASE() AS `database_name`,
(SELECT COUNT(*) FROM `creature_template` WHERE `entry`=27756) AS `source_rows`,
(SELECT COUNT(*) FROM `creature_template` WHERE `entry`=27756 AND `VehicleId`=70) AS `source_vehicle70_rows`,
(SELECT COUNT(*) FROM `creature_template` WHERE `entry`=1000171) AS `target_rows`,
(SELECT COUNT(*) FROM `creature_template`
 WHERE `entry`=1000171 AND `ScriptName`='npc_g17_dragonriding_vehicle'
   AND `VehicleId`=70 AND `modelid1`=25854 AND `name`='御空龙·B0') AS `target_exact`,
(SELECT COUNT(*) FROM `creature_template_spell` WHERE `CreatureID`=1000171) AS `action_rows`,
(SELECT COUNT(*) FROM `creature_template_spell`
 WHERE (`CreatureID`,`Index`,`Spell`) IN
   ((1000171,0,9573),(1000171,1,55215),
    (1000171,2,52197),(1000171,3,53208))) AS `action_exact`,
(SELECT COUNT(*) FROM `creature_template_movement`
 WHERE `CreatureId`=1000171 AND `Ground`=1 AND `Swim`=0
   AND `Flight`=1 AND `Rooted`=0 AND `Chase`=0 AND `Random`=0) AS `movement_exact`,
(SELECT COUNT(*) FROM `spell_script_names`
 WHERE `ScriptName` IN
   ('spell_g17_dragon_breath_energy','spell_g17_dragon_accelerate_energy',
    'spell_g17_dragon_climb','spell_g17_dragon_safe_landing')) AS `script_rows`,
(SELECT COUNT(*) FROM `spell_script_names`
 WHERE (`spell_id`,`ScriptName`) IN
   ((9573,'spell_g17_dragon_breath_energy'),
    (55215,'spell_g17_dragon_accelerate_energy'),
    (52197,'spell_g17_dragon_climb'),
    (53208,'spell_g17_dragon_safe_landing'))) AS `script_exact`;
