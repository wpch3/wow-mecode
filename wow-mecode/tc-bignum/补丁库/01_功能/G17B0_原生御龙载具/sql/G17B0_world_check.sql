-- G17-B0 world库只读检查
SET NAMES utf8mb4;
SET @G17B0_ENTRY := 1000171;
SET @G17B0_SCRIPT := 'npc_g17_dragonriding_vehicle';

SELECT CASE
  WHEN (SELECT COUNT(*) FROM `creature_template` WHERE `entry`=27756 AND `VehicleId`<>0)<>1
    THEN 'BLOCKED_SOURCE_RUBY_DRAKE_NOT_READY'
  WHEN (SELECT COUNT(*) FROM `creature_template`
        WHERE `entry`=@G17B0_ENTRY AND `ScriptName`=@G17B0_SCRIPT AND `VehicleId`<>0)<>1
    THEN 'NOT_INSTALLED_COLLISION_OR_INVALID_VEHICLE'
  WHEN (SELECT COUNT(*) FROM `creature_template_spell` WHERE `CreatureID`=@G17B0_ENTRY)<>4
    OR (SELECT COUNT(*) FROM `creature_template_spell` WHERE (`CreatureID`,`Index`,`Spell`) IN
      ((@G17B0_ENTRY,0,9573),(@G17B0_ENTRY,1,55215),
       (@G17B0_ENTRY,2,52197),(@G17B0_ENTRY,3,53208)))<>4
    THEN 'FAILED_ACTION_BAR_MAPPING'
  WHEN (SELECT COUNT(*) FROM `creature_template_movement`
        WHERE `CreatureId`=@G17B0_ENTRY AND `Flight`=1 AND `Rooted`=0)<>1
    THEN 'FAILED_FLIGHT_MOVEMENT'
  WHEN (SELECT COUNT(*) FROM `spell_script_names` WHERE (`spell_id`,`ScriptName`) IN
    ((9573,'spell_g17_dragon_breath_energy'),(55215,'spell_g17_dragon_accelerate_energy'),
     (52197,'spell_g17_dragon_climb'),(53208,'spell_g17_dragon_safe_landing')))<>4
    THEN 'FAILED_SPELL_SCRIPT_BINDINGS'
  ELSE 'G17B0_WORLD_CHECK_PASS'
END AS `G17B0_RESULT`;

SELECT `entry`,`name`,`subname`,`VehicleId`,`ScriptName`,`speed_walk`,`speed_run`
FROM `creature_template` WHERE `entry` IN (27756,@G17B0_ENTRY) ORDER BY `entry`;
SELECT `CreatureID`,`Index`,`Spell` FROM `creature_template_spell`
WHERE `CreatureID`=@G17B0_ENTRY ORDER BY `Index`;
SELECT `spell_id`,`ScriptName` FROM `spell_script_names`
WHERE `ScriptName` LIKE 'spell_g17_dragon_%' ORDER BY `spell_id`;
