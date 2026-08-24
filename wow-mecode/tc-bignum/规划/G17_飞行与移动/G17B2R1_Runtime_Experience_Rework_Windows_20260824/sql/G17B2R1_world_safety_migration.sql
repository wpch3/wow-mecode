-- G17-B2R1 World safety migration (install and rollback safety floor)
-- Explicit database: world. Idempotent and ownership-guarded.
-- The legacy action is removed from both the vehicle bar and SpellScript binding;
-- rollback intentionally keeps the visual-free command instead of restoring a known-bad client visual.

USE `world`;

SET @G17B2R1_ENTRY := 1000171;
SET @G17B2R1_SLOT := 3;
SET @G17B2R1_LEGACY_ACTION := 53208;
SET @G17B2R1_SAFE_ACTION := 52226;
SET @G17B2R1_SCRIPT := 'spell_g17_dragon_safe_landing';

START TRANSACTION;

SET @G17B2R1_ACTION_ROWS := (
    SELECT COUNT(*)
    FROM `creature_template_spell`
    WHERE `CreatureID`=@G17B2R1_ENTRY AND `Index`=@G17B2R1_SLOT
      AND `Spell` IN (@G17B2R1_LEGACY_ACTION,@G17B2R1_SAFE_ACTION)
);
SET @G17B2R1_FOREIGN_SLOT_ROWS := (
    SELECT COUNT(*)
    FROM `creature_template_spell`
    WHERE `CreatureID`=@G17B2R1_ENTRY AND `Index`=@G17B2R1_SLOT
      AND `Spell` NOT IN (@G17B2R1_LEGACY_ACTION,@G17B2R1_SAFE_ACTION)
);
SET @G17B2R1_CAN_APPLY := (@G17B2R1_ACTION_ROWS=1 AND @G17B2R1_FOREIGN_SLOT_ROWS=0);

UPDATE `creature_template_spell`
SET `Spell`=@G17B2R1_SAFE_ACTION
WHERE `CreatureID`=@G17B2R1_ENTRY AND `Index`=@G17B2R1_SLOT
  AND `Spell` IN (@G17B2R1_LEGACY_ACTION,@G17B2R1_SAFE_ACTION)
  AND @G17B2R1_CAN_APPLY=1;

DELETE FROM `spell_script_names`
WHERE `ScriptName` COLLATE utf8mb4_unicode_ci=@G17B2R1_SCRIPT
  AND `spell_id`<>@G17B2R1_SAFE_ACTION
  AND @G17B2R1_CAN_APPLY=1;

INSERT INTO `spell_script_names` (`spell_id`,`ScriptName`)
SELECT @G17B2R1_SAFE_ACTION,@G17B2R1_SCRIPT FROM DUAL
WHERE @G17B2R1_CAN_APPLY=1
  AND NOT EXISTS (
      SELECT 1 FROM `spell_script_names`
      WHERE `spell_id`=@G17B2R1_SAFE_ACTION
        AND `ScriptName` COLLATE utf8mb4_unicode_ci=@G17B2R1_SCRIPT
  );

COMMIT;

SET @G17B2R1_POST_ACTION := (
    SELECT COUNT(*) FROM `creature_template_spell`
    WHERE `CreatureID`=@G17B2R1_ENTRY AND `Index`=@G17B2R1_SLOT
      AND `Spell`=@G17B2R1_SAFE_ACTION
);
SET @G17B2R1_POST_LEGACY_ACTION := (
    SELECT COUNT(*) FROM `creature_template_spell`
    WHERE `CreatureID`=@G17B2R1_ENTRY AND `Index`=@G17B2R1_SLOT
      AND `Spell`=@G17B2R1_LEGACY_ACTION
);
SET @G17B2R1_POST_SCRIPT := (
    SELECT COUNT(*) FROM `spell_script_names`
    WHERE `spell_id`=@G17B2R1_SAFE_ACTION
      AND `ScriptName` COLLATE utf8mb4_unicode_ci=@G17B2R1_SCRIPT
);
SET @G17B2R1_POST_LEGACY_SCRIPT := (
    SELECT COUNT(*) FROM `spell_script_names`
    WHERE `spell_id`=@G17B2R1_LEGACY_ACTION
      AND `ScriptName` COLLATE utf8mb4_unicode_ci=@G17B2R1_SCRIPT
);

SELECT
    IF(@G17B2R1_CAN_APPLY=1 AND @G17B2R1_POST_ACTION=1
       AND @G17B2R1_POST_LEGACY_ACTION=0 AND @G17B2R1_POST_SCRIPT=1
       AND @G17B2R1_POST_LEGACY_SCRIPT=0,
       'G17B2R1_WORLD_MIGRATION=PASS',
       'G17B2R1_WORLD_MIGRATION=FAIL') AS `G17B2R1_RESULT`,
    @G17B2R1_CAN_APPLY AS `ownership_guard`,
    @G17B2R1_POST_ACTION AS `safe_action_rows`,
    @G17B2R1_POST_SCRIPT AS `safe_script_rows`,
    @G17B2R1_POST_LEGACY_ACTION AS `legacy_action_rows`,
    @G17B2R1_POST_LEGACY_SCRIPT AS `legacy_script_rows`;
