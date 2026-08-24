-- G17-B0 world受控安装v3：显式world库 + utf8mb4_unicode_ci跨collation修复
-- 前像锁：world / MySQL 8.0.46 / source 27756 VehicleId 70 / target 1000171空 / 自定义ScriptName绑定0
-- 执行方式：DBeaver执行整个脚本；本文件显式USE world，不依赖当前活动数据库。
-- 若任何语句抛出SQL错误：在同一连接立即执行 ROLLBACK;，停止并回传错误；不要继续编译。

USE `world`;
SET NAMES utf8mb4 COLLATE utf8mb4_unicode_ci;
SET @G17B0_ENTRY := 1000171;
SET @G17B0_SOURCE := 27756;
SET @G17B0_VEHICLE_ID := 70;
-- 所有参与列比较的文本变量都显式锁为world表的utf8mb4_unicode_ci；
-- 避免MySQL 8连接默认utf8mb4_0900_ai_ci与表列IMPLICIT比较触发1267。
SET @G17B0_SCRIPT := _utf8mb4'npc_g17_dragonriding_vehicle' COLLATE utf8mb4_unicode_ci;
SET @G17B0_S1 := _utf8mb4'spell_g17_dragon_breath_energy' COLLATE utf8mb4_unicode_ci;
SET @G17B0_S2 := _utf8mb4'spell_g17_dragon_accelerate_energy' COLLATE utf8mb4_unicode_ci;
SET @G17B0_S3 := _utf8mb4'spell_g17_dragon_climb' COLLATE utf8mb4_unicode_ci;
SET @G17B0_S4 := _utf8mb4'spell_g17_dragon_safe_landing' COLLATE utf8mb4_unicode_ci;
SET @G17B0_NAME := _utf8mb4'御空龙·B0' COLLATE utf8mb4_unicode_ci;
SET @G17B0_SUBNAME := _utf8mb4'G17原生御龙载具' COLLATE utf8mb4_unicode_ci;

SET @G17B0_SOURCE_ROWS := (
  SELECT COUNT(*) FROM `creature_template` WHERE `entry`=@G17B0_SOURCE
);
SET @G17B0_SOURCE_OK := (
  SELECT COUNT(*) FROM `creature_template`
  WHERE `entry`=@G17B0_SOURCE AND `VehicleId`=@G17B0_VEHICLE_ID
);
SET @G17B0_TARGET_ROWS_BEFORE := (
  SELECT COUNT(*) FROM `creature_template` WHERE `entry`=@G17B0_ENTRY
);
SET @G17B0_TARGET_OWNED_BEFORE := (
  SELECT COUNT(*) FROM `creature_template`
  WHERE `entry`=@G17B0_ENTRY AND `ScriptName` COLLATE utf8mb4_unicode_ci=@G17B0_SCRIPT
);
SET @G17B0_TARGET_FOREIGN := (
  SELECT COUNT(*) FROM `creature_template`
  WHERE `entry`=@G17B0_ENTRY AND COALESCE(`ScriptName`,_utf8mb4'' COLLATE utf8mb4_unicode_ci) COLLATE utf8mb4_unicode_ci<>@G17B0_SCRIPT
);
SET @G17B0_FOREIGN_SCRIPT_BINDINGS := (
  SELECT COUNT(*) FROM `spell_script_names`
  WHERE `ScriptName` COLLATE utf8mb4_unicode_ci IN (@G17B0_S1,@G17B0_S2,@G17B0_S3,@G17B0_S4)
    AND NOT (
      (`spell_id`=9573  AND `ScriptName` COLLATE utf8mb4_unicode_ci=@G17B0_S1) OR
      (`spell_id`=55215 AND `ScriptName` COLLATE utf8mb4_unicode_ci=@G17B0_S2) OR
      (`spell_id`=52197 AND `ScriptName` COLLATE utf8mb4_unicode_ci=@G17B0_S3) OR
      (`spell_id`=53208 AND `ScriptName` COLLATE utf8mb4_unicode_ci=@G17B0_S4)
    )
);
SET @G17B0_CAN_APPLY := (
  @G17B0_SOURCE_ROWS=1 AND @G17B0_SOURCE_OK=1 AND
  @G17B0_TARGET_FOREIGN=0 AND @G17B0_FOREIGN_SCRIPT_BINDINGS=0
);

DROP TEMPORARY TABLE IF EXISTS `_g17b0_creature_clone`;
CREATE TEMPORARY TABLE `_g17b0_creature_clone` LIKE `creature_template`;
INSERT INTO `_g17b0_creature_clone`
SELECT * FROM `creature_template`
WHERE `entry`=@G17B0_SOURCE AND @G17B0_CAN_APPLY=1;

UPDATE `_g17b0_creature_clone` SET
  `entry`=@G17B0_ENTRY,
  `difficulty_entry_1`=0,
  `difficulty_entry_2`=0,
  `difficulty_entry_3`=0,
  `KillCredit1`=0,
  `KillCredit2`=0,
  `name`=@G17B0_NAME,
  `subname`=@G17B0_SUBNAME,
  `IconName`='vehichleCursor',
  `gossip_menu_id`=0,
  `minlevel`=80,
  `maxlevel`=80,
  `faction`=35,
  `npcflag`=0,
  `speed_walk`=1.5,
  `speed_run`=1.5,
  `unit_flags`=0,
  `unit_flags2`=2048,
  `dynamicflags`=0,
  `VehicleId`=@G17B0_VEHICLE_ID,
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

START TRANSACTION;

INSERT INTO `creature_template`
SELECT * FROM `_g17b0_creature_clone` AS clone
WHERE @G17B0_CAN_APPLY=1
  AND NOT EXISTS (
    SELECT 1 FROM `creature_template` AS live WHERE live.`entry`=@G17B0_ENTRY
  );

SET @G17B0_OWNED_AFTER_INSERT := (
  SELECT COUNT(*) FROM `creature_template`
  WHERE `entry`=@G17B0_ENTRY AND `ScriptName` COLLATE utf8mb4_unicode_ci=@G17B0_SCRIPT
);

-- 幂等修正仅作用于本批ScriptName拥有的entry；不覆盖外来entry。
UPDATE `creature_template` SET
  `name`=@G17B0_NAME,
  `subname`=@G17B0_SUBNAME,
  `IconName`='vehichleCursor',
  `modelid1`=25854,
  `modelid2`=0,
  `modelid3`=0,
  `modelid4`=0,
  `minlevel`=80,
  `maxlevel`=80,
  `faction`=35,
  `npcflag`=0,
  `speed_walk`=1.5,
  `speed_run`=1.5,
  `unit_flags`=0,
  `unit_flags2`=2048,
  `dynamicflags`=0,
  `VehicleId`=@G17B0_VEHICLE_ID,
  `AIName`='',
  `MovementType`=0,
  `flags_extra`=0,
  `ScriptName`=@G17B0_SCRIPT,
  `VerifiedBuild`=NULL
WHERE `entry`=@G17B0_ENTRY AND `ScriptName` COLLATE utf8mb4_unicode_ci=@G17B0_SCRIPT
  AND @G17B0_CAN_APPLY=1 AND @G17B0_OWNED_AFTER_INSERT=1;

DELETE FROM `creature_template_spell`
WHERE `CreatureID`=@G17B0_ENTRY
  AND @G17B0_CAN_APPLY=1 AND @G17B0_OWNED_AFTER_INSERT=1;
INSERT INTO `creature_template_spell` (`CreatureID`,`Index`,`Spell`,`VerifiedBuild`)
SELECT @G17B0_ENTRY,0,9573,NULL FROM DUAL
WHERE @G17B0_CAN_APPLY=1 AND @G17B0_OWNED_AFTER_INSERT=1 UNION ALL
SELECT @G17B0_ENTRY,1,55215,NULL FROM DUAL
WHERE @G17B0_CAN_APPLY=1 AND @G17B0_OWNED_AFTER_INSERT=1 UNION ALL
SELECT @G17B0_ENTRY,2,52197,NULL FROM DUAL
WHERE @G17B0_CAN_APPLY=1 AND @G17B0_OWNED_AFTER_INSERT=1 UNION ALL
SELECT @G17B0_ENTRY,3,53208,NULL FROM DUAL
WHERE @G17B0_CAN_APPLY=1 AND @G17B0_OWNED_AFTER_INSERT=1;

DELETE FROM `creature_template_movement`
WHERE `CreatureId`=@G17B0_ENTRY
  AND @G17B0_CAN_APPLY=1 AND @G17B0_OWNED_AFTER_INSERT=1;
INSERT INTO `creature_template_movement`
(`CreatureId`,`Ground`,`Swim`,`Flight`,`Rooted`,`Chase`,`Random`,`InteractionPauseTimer`)
SELECT @G17B0_ENTRY,1,0,1,0,0,0,0 FROM DUAL
WHERE @G17B0_CAN_APPLY=1 AND @G17B0_OWNED_AFTER_INSERT=1;

DELETE FROM `spell_script_names`
WHERE `ScriptName` COLLATE utf8mb4_unicode_ci IN (@G17B0_S1,@G17B0_S2,@G17B0_S3,@G17B0_S4)
AND @G17B0_CAN_APPLY=1 AND @G17B0_OWNED_AFTER_INSERT=1;
INSERT INTO `spell_script_names` (`spell_id`,`ScriptName`)
SELECT 9573,@G17B0_S1 FROM DUAL
WHERE @G17B0_CAN_APPLY=1 AND @G17B0_OWNED_AFTER_INSERT=1 UNION ALL
SELECT 55215,@G17B0_S2 FROM DUAL
WHERE @G17B0_CAN_APPLY=1 AND @G17B0_OWNED_AFTER_INSERT=1 UNION ALL
SELECT 52197,@G17B0_S3 FROM DUAL
WHERE @G17B0_CAN_APPLY=1 AND @G17B0_OWNED_AFTER_INSERT=1 UNION ALL
SELECT 53208,@G17B0_S4 FROM DUAL
WHERE @G17B0_CAN_APPLY=1 AND @G17B0_OWNED_AFTER_INSERT=1;

COMMIT;
DROP TEMPORARY TABLE `_g17b0_creature_clone`;

SET @G17B0_POST_TARGET_ROWS := (
  SELECT COUNT(*) FROM `creature_template` WHERE `entry`=@G17B0_ENTRY
);
SET @G17B0_POST_TARGET_EXACT := (
  SELECT COUNT(*) FROM `creature_template`
  WHERE `entry`=@G17B0_ENTRY AND `ScriptName` COLLATE utf8mb4_unicode_ci=@G17B0_SCRIPT
    AND `VehicleId`=@G17B0_VEHICLE_ID AND `modelid1`=25854
    AND `name`=@G17B0_NAME
);
SET @G17B0_POST_ACTION_TOTAL := (
  SELECT COUNT(*) FROM `creature_template_spell` WHERE `CreatureID`=@G17B0_ENTRY
);
SET @G17B0_POST_ACTION_EXACT := (
  SELECT COUNT(*) FROM `creature_template_spell`
  WHERE (`CreatureID`,`Index`,`Spell`) IN
    ((@G17B0_ENTRY,0,9573),(@G17B0_ENTRY,1,55215),
     (@G17B0_ENTRY,2,52197),(@G17B0_ENTRY,3,53208))
);
SET @G17B0_POST_MOVEMENT_EXACT := (
  SELECT COUNT(*) FROM `creature_template_movement`
  WHERE `CreatureId`=@G17B0_ENTRY AND `Ground`=1 AND `Swim`=0
    AND `Flight`=1 AND `Rooted`=0 AND `Chase`=0 AND `Random`=0
);
SET @G17B0_POST_SCRIPT_TOTAL := (
  SELECT COUNT(*) FROM `spell_script_names`
  WHERE `ScriptName` COLLATE utf8mb4_unicode_ci IN (@G17B0_S1,@G17B0_S2,@G17B0_S3,@G17B0_S4)
);
SET @G17B0_POST_SCRIPT_EXACT := (
  SELECT COUNT(*) FROM `spell_script_names`
  WHERE (`spell_id`=9573  AND `ScriptName` COLLATE utf8mb4_unicode_ci=@G17B0_S1)
     OR (`spell_id`=55215 AND `ScriptName` COLLATE utf8mb4_unicode_ci=@G17B0_S2)
     OR (`spell_id`=52197 AND `ScriptName` COLLATE utf8mb4_unicode_ci=@G17B0_S3)
     OR (`spell_id`=53208 AND `ScriptName` COLLATE utf8mb4_unicode_ci=@G17B0_S4)
);

SELECT CASE
  WHEN @G17B0_SOURCE_ROWS<>1 OR @G17B0_SOURCE_OK<>1
    THEN 'BLOCKED_SOURCE_27756_NOT_EXACT_VEHICLE_70'
  WHEN @G17B0_TARGET_FOREIGN<>0
    THEN 'BLOCKED_TARGET_1000171_FOREIGN_COLLISION'
  WHEN @G17B0_FOREIGN_SCRIPT_BINDINGS<>0
    THEN 'BLOCKED_FOREIGN_SCRIPT_BINDING_COLLISION'
  WHEN @G17B0_CAN_APPLY<>1
    THEN 'BLOCKED_PREIMAGE_GATE'
  WHEN @G17B0_POST_TARGET_ROWS<>1 OR @G17B0_POST_TARGET_EXACT<>1
    THEN 'FAILED_TARGET_POSTIMAGE'
  WHEN @G17B0_POST_ACTION_TOTAL<>4 OR @G17B0_POST_ACTION_EXACT<>4
    THEN 'FAILED_ACTION_BAR_POSTIMAGE'
  WHEN @G17B0_POST_MOVEMENT_EXACT<>1
    THEN 'FAILED_MOVEMENT_POSTIMAGE'
  WHEN @G17B0_POST_SCRIPT_TOTAL<>4 OR @G17B0_POST_SCRIPT_EXACT<>4
    THEN 'FAILED_SCRIPT_BINDING_POSTIMAGE'
  ELSE 'G17B0_WORLD_INSTALL_PASS'
END AS `G17B0_RESULT`,
DATABASE() AS `database_name`,
@G17B0_CAN_APPLY AS `preimage_gate`,
@G17B0_SOURCE_ROWS AS `source_rows`,
@G17B0_SOURCE_OK AS `source_vehicle70_rows`,
@G17B0_TARGET_ROWS_BEFORE AS `target_rows_before`,
@G17B0_TARGET_FOREIGN AS `foreign_target_rows`,
@G17B0_FOREIGN_SCRIPT_BINDINGS AS `foreign_script_bindings`,
@G17B0_POST_TARGET_ROWS AS `target_rows_after`,
@G17B0_POST_TARGET_EXACT AS `target_exact_after`,
@G17B0_POST_ACTION_TOTAL AS `action_rows_after`,
@G17B0_POST_ACTION_EXACT AS `action_exact_after`,
@G17B0_POST_MOVEMENT_EXACT AS `movement_exact_after`,
@G17B0_POST_SCRIPT_TOTAL AS `script_rows_after`,
@G17B0_POST_SCRIPT_EXACT AS `script_exact_after`;
