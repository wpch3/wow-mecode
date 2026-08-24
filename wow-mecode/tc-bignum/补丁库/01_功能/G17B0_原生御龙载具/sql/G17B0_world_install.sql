-- G17-B0 原生可控御龙载具（world库）
-- 目标：TrinityCore 3.3.5a / NPCBOT-Eluna-zhCN-2026
-- 可重复执行；若entry=1000171被其它内容占用，本脚本拒绝覆盖并在末尾报告BLOCKED_COLLISION。

SET NAMES utf8mb4;
SET @G17B0_ENTRY := 1000171;
SET @G17B0_SOURCE := 27756; -- TrinityDB原生Ruby Drake，复用其模型/VehicleId/可控座位
SET @G17B0_SCRIPT := 'npc_g17_dragonriding_vehicle';

SET @G17B0_SOURCE_OK := (
  SELECT COUNT(*) FROM `creature_template`
  WHERE `entry`=@G17B0_SOURCE AND `VehicleId`<>0
);
SET @G17B0_COLLISION := (
  SELECT COUNT(*) FROM `creature_template`
  WHERE `entry`=@G17B0_ENTRY AND COALESCE(`ScriptName`,'')<>@G17B0_SCRIPT
);

SELECT @G17B0_SOURCE_OK AS `source_vehicle_ready_should_be_1`,
       @G17B0_COLLISION AS `foreign_collision_should_be_0`;

DROP TEMPORARY TABLE IF EXISTS `_g17b0_creature_clone`;
CREATE TEMPORARY TABLE `_g17b0_creature_clone` LIKE `creature_template`;
INSERT INTO `_g17b0_creature_clone`
SELECT * FROM `creature_template`
WHERE `entry`=@G17B0_SOURCE AND @G17B0_SOURCE_OK=1 AND @G17B0_COLLISION=0;

UPDATE `_g17b0_creature_clone` SET
  `entry`=@G17B0_ENTRY,
  `difficulty_entry_1`=0,
  `difficulty_entry_2`=0,
  `difficulty_entry_3`=0,
  `KillCredit1`=0,
  `KillCredit2`=0,
  `name`='御空龙·B0',
  `subname`='G17原生御龙载具',
  `IconName`='vehichleCursor',
  `gossip_menu_id`=0,
  `minlevel`=80,
  `maxlevel`=80,
  `faction`=35,
  `npcflag`=0,
  `speed_walk`=1.5,
  `speed_run`=1.5,
  `lootid`=0,
  `pickpocketloot`=0,
  `skinloot`=0,
  `mingold`=0,
  `maxgold`=0,
  `AIName`='',
  `MovementType`=0,
  `RacialLeader`=0,
  `RegenHealth`=1,
  `flags_extra`=0,
  `ScriptName`=@G17B0_SCRIPT,
  `VerifiedBuild`=NULL;

INSERT INTO `creature_template`
SELECT * FROM `_g17b0_creature_clone` AS clone
WHERE NOT EXISTS (
  SELECT 1 FROM `creature_template` AS live WHERE live.`entry`=@G17B0_ENTRY
);
DROP TEMPORARY TABLE `_g17b0_creature_clone`;

SET @G17B0_OWNED := (
  SELECT COUNT(*) FROM `creature_template`
  WHERE `entry`=@G17B0_ENTRY AND `ScriptName`=@G17B0_SCRIPT
);

-- 原生VehicleActionBar：1龙息、2短时加速、3爬升、4安全着陆。
DELETE FROM `creature_template_spell`
WHERE `CreatureID`=@G17B0_ENTRY AND @G17B0_OWNED=1;
INSERT INTO `creature_template_spell` (`CreatureID`,`Index`,`Spell`,`VerifiedBuild`)
SELECT @G17B0_ENTRY,0,9573,NULL FROM DUAL WHERE @G17B0_OWNED=1 UNION ALL
SELECT @G17B0_ENTRY,1,55215,NULL FROM DUAL WHERE @G17B0_OWNED=1 UNION ALL
SELECT @G17B0_ENTRY,2,52197,NULL FROM DUAL WHERE @G17B0_OWNED=1 UNION ALL
SELECT @G17B0_ENTRY,3,53208,NULL FROM DUAL WHERE @G17B0_OWNED=1;

DELETE FROM `creature_template_movement`
WHERE `CreatureId`=@G17B0_ENTRY AND @G17B0_OWNED=1;
INSERT INTO `creature_template_movement`
(`CreatureId`,`Ground`,`Swim`,`Flight`,`Rooted`,`Chase`,`Random`,`InteractionPauseTimer`)
SELECT @G17B0_ENTRY,1,0,1,0,0,0,0 FROM DUAL WHERE @G17B0_OWNED=1;

DELETE FROM `spell_script_names` WHERE `ScriptName` IN
('spell_g17_dragon_breath_energy','spell_g17_dragon_accelerate_energy',
 'spell_g17_dragon_climb','spell_g17_dragon_safe_landing') AND @G17B0_OWNED=1;
INSERT INTO `spell_script_names` (`spell_id`,`ScriptName`)
SELECT 9573,'spell_g17_dragon_breath_energy' FROM DUAL WHERE @G17B0_OWNED=1 UNION ALL
SELECT 55215,'spell_g17_dragon_accelerate_energy' FROM DUAL WHERE @G17B0_OWNED=1 UNION ALL
SELECT 52197,'spell_g17_dragon_climb' FROM DUAL WHERE @G17B0_OWNED=1 UNION ALL
SELECT 53208,'spell_g17_dragon_safe_landing' FROM DUAL WHERE @G17B0_OWNED=1;

SELECT CASE
  WHEN @G17B0_SOURCE_OK<>1 THEN 'BLOCKED_SOURCE_RUBY_DRAKE_NOT_READY'
  WHEN @G17B0_COLLISION<>0 THEN 'BLOCKED_COLLISION_ENTRY_1000171'
  WHEN @G17B0_OWNED<>1 THEN 'FAILED_TARGET_NOT_CREATED'
  WHEN (SELECT COUNT(*) FROM `creature_template`
        WHERE `entry`=@G17B0_ENTRY AND `ScriptName`=@G17B0_SCRIPT AND `VehicleId`<>0)<>1
    THEN 'FAILED_TARGET_VEHICLE'
  WHEN (SELECT COUNT(*) FROM `creature_template_spell` WHERE `CreatureID`=@G17B0_ENTRY)<>4
    OR (SELECT COUNT(*) FROM `creature_template_spell` WHERE (`CreatureID`,`Index`,`Spell`) IN
      ((@G17B0_ENTRY,0,9573),(@G17B0_ENTRY,1,55215),
       (@G17B0_ENTRY,2,52197),(@G17B0_ENTRY,3,53208)))<>4
    THEN 'FAILED_ACTION_BAR'
  WHEN (SELECT COUNT(*) FROM `creature_template_movement`
        WHERE `CreatureId`=@G17B0_ENTRY AND `Flight`=1 AND `Rooted`=0)<>1
    THEN 'FAILED_FLIGHT_MOVEMENT'
  WHEN (SELECT COUNT(*) FROM `spell_script_names` WHERE (`spell_id`,`ScriptName`) IN
    ((9573,'spell_g17_dragon_breath_energy'),(55215,'spell_g17_dragon_accelerate_energy'),
     (52197,'spell_g17_dragon_climb'),(53208,'spell_g17_dragon_safe_landing')))<>4
    THEN 'FAILED_SPELL_SCRIPTS'
  ELSE 'G17B0_WORLD_INSTALL_PASS'
END AS `G17B0_RESULT`;

SELECT `entry`,`name`,`subname`,`VehicleId`,`ScriptName`,`speed_walk`,`speed_run`
FROM `creature_template` WHERE `entry`=@G17B0_ENTRY;
SELECT `CreatureID`,`Index`,`Spell` FROM `creature_template_spell`
WHERE `CreatureID`=@G17B0_ENTRY ORDER BY `Index`;
