-- G17-B0 world库回滚：只删除由本批ScriptName确认拥有的自定义entry和绑定。
SET NAMES utf8mb4;
SET @G17B0_ENTRY := 1000171;
SET @G17B0_SCRIPT := 'npc_g17_dragonriding_vehicle';
SET @G17B0_OWNED := (
  SELECT COUNT(*) FROM `creature_template`
  WHERE `entry`=@G17B0_ENTRY AND `ScriptName`=@G17B0_SCRIPT
);

DELETE FROM `spell_script_names` WHERE `ScriptName` IN
('spell_g17_dragon_breath_energy','spell_g17_dragon_accelerate_energy',
 'spell_g17_dragon_climb','spell_g17_dragon_safe_landing') AND @G17B0_OWNED=1;
DELETE FROM `creature_template_spell` WHERE `CreatureID`=@G17B0_ENTRY AND @G17B0_OWNED=1;
DELETE FROM `creature_template_movement` WHERE `CreatureId`=@G17B0_ENTRY AND @G17B0_OWNED=1;
DELETE FROM `creature_template` WHERE `entry`=@G17B0_ENTRY AND `ScriptName`=@G17B0_SCRIPT;

SELECT CASE
  WHEN @G17B0_OWNED<>1 THEN 'ROLLBACK_NOOP_NOT_OWNED'
  WHEN (SELECT COUNT(*) FROM `creature_template` WHERE `entry`=@G17B0_ENTRY)<>0 THEN 'ROLLBACK_FAILED'
  ELSE 'G17B0_WORLD_ROLLBACK_PASS'
END AS `G17B0_RESULT`;
