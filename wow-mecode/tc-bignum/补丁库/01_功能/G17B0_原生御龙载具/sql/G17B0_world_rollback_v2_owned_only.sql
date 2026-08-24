-- G17-B0 world回滚v2：仅当target entry由本批ScriptName拥有时删除。
-- 不要自行执行；只在后续明确要求回滚时使用。

SET NAMES utf8mb4;
SET @G17B0_ENTRY := 1000171;
SET @G17B0_SCRIPT := 'npc_g17_dragonriding_vehicle';
SET @G17B0_OWNED := (
  SELECT COUNT(*) FROM `creature_template`
  WHERE `entry`=@G17B0_ENTRY AND `ScriptName`=@G17B0_SCRIPT
);
SET @G17B0_FOREIGN := (
  SELECT COUNT(*) FROM `creature_template`
  WHERE `entry`=@G17B0_ENTRY AND COALESCE(`ScriptName`,'')<>@G17B0_SCRIPT
);

START TRANSACTION;
DELETE FROM `spell_script_names`
WHERE `ScriptName` IN
('spell_g17_dragon_breath_energy','spell_g17_dragon_accelerate_energy',
 'spell_g17_dragon_climb','spell_g17_dragon_safe_landing')
AND @G17B0_OWNED=1 AND @G17B0_FOREIGN=0;
DELETE FROM `creature_template_spell`
WHERE `CreatureID`=@G17B0_ENTRY AND @G17B0_OWNED=1 AND @G17B0_FOREIGN=0;
DELETE FROM `creature_template_movement`
WHERE `CreatureId`=@G17B0_ENTRY AND @G17B0_OWNED=1 AND @G17B0_FOREIGN=0;
DELETE FROM `creature_template`
WHERE `entry`=@G17B0_ENTRY AND `ScriptName`=@G17B0_SCRIPT
  AND @G17B0_OWNED=1 AND @G17B0_FOREIGN=0;
COMMIT;

SELECT CASE
  WHEN @G17B0_FOREIGN<>0 THEN 'ROLLBACK_BLOCKED_FOREIGN_TARGET'
  WHEN @G17B0_OWNED=0 THEN 'ROLLBACK_NOOP_NOT_INSTALLED'
  WHEN (SELECT COUNT(*) FROM `creature_template` WHERE `entry`=@G17B0_ENTRY)<>0
    THEN 'ROLLBACK_FAILED_TARGET_REMAINS'
  WHEN (SELECT COUNT(*) FROM `creature_template_spell` WHERE `CreatureID`=@G17B0_ENTRY)<>0
    THEN 'ROLLBACK_FAILED_ACTIONS_REMAIN'
  WHEN (SELECT COUNT(*) FROM `creature_template_movement` WHERE `CreatureId`=@G17B0_ENTRY)<>0
    THEN 'ROLLBACK_FAILED_MOVEMENT_REMAINS'
  WHEN (SELECT COUNT(*) FROM `spell_script_names` WHERE `ScriptName` IN
    ('spell_g17_dragon_breath_energy','spell_g17_dragon_accelerate_energy',
     'spell_g17_dragon_climb','spell_g17_dragon_safe_landing'))<>0
    THEN 'ROLLBACK_FAILED_SCRIPTS_REMAIN'
  ELSE 'G17B0_WORLD_ROLLBACK_PASS'
END AS `G17B0_RESULT`,
DATABASE() AS `database_name`,
@G17B0_OWNED AS `owned_rows_before`,
@G17B0_FOREIGN AS `foreign_rows_before`,
(SELECT COUNT(*) FROM `creature_template` WHERE `entry`=@G17B0_ENTRY) AS `target_rows_after`,
(SELECT COUNT(*) FROM `creature_template_spell` WHERE `CreatureID`=@G17B0_ENTRY) AS `action_rows_after`,
(SELECT COUNT(*) FROM `creature_template_movement` WHERE `CreatureId`=@G17B0_ENTRY) AS `movement_rows_after`,
(SELECT COUNT(*) FROM `spell_script_names` WHERE `ScriptName` IN
  ('spell_g17_dragon_breath_energy','spell_g17_dragon_accelerate_energy',
   'spell_g17_dragon_climb','spell_g17_dragon_safe_landing')) AS `script_rows_after`;
