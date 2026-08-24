-- G17-B2R3 World binding guard (idempotent, ownership-guarded).
-- R2 already migrated vehicle action slot 3 from 53208 to the real quest
-- command 52226 "飞行器着陆" and bound spell_g17_dragon_safe_landing to it.
-- R2 does not change IDs; it only re-affirms the binding so a half-applied or
-- rolled-back B2R1 database cannot leave skill 4 without a script.  This is
-- safe to run repeatedly and refuses to touch a foreign/conflicting slot 3.
--
-- Explicitly targets the `world` database.  If any statement errors, run
-- ROLLBACK; in the same connection and stop.

USE `world`;
SET NAMES utf8mb4 COLLATE utf8mb4_unicode_ci;

SET @G17B2R3_ENTRY := 1000171;
SET @G17B2R3_SLOT := 3;
SET @G17B2R3_SAFE_ACTION := 52226;
SET @G17B2R3_LEGACY_ACTION := 53208;
SET @G17B2R3_SCRIPT := _utf8mb4'spell_g17_dragon_safe_landing' COLLATE utf8mb4_unicode_ci;

START TRANSACTION;

SET @G17B2R3_ACTION_ROWS := (
    SELECT COUNT(*)
    FROM `creature_template_spell`
    WHERE `CreatureID`=@G17B2R3_ENTRY AND `Index`=@G17B2R3_SLOT
      AND `Spell` IN (@G17B2R3_LEGACY_ACTION,@G17B2R3_SAFE_ACTION)
);
SET @G17B2R3_FOREIGN_SLOT_ROWS := (
    SELECT COUNT(*)
    FROM `creature_template_spell`
    WHERE `CreatureID`=@G17B2R3_ENTRY AND `Index`=@G17B2R3_SLOT
      AND `Spell` NOT IN (@G17B2R3_LEGACY_ACTION,@G17B2R3_SAFE_ACTION)
);
SET @G17B2R3_CAN_APPLY := (@G17B2R3_ACTION_ROWS=1 AND @G17B2R3_FOREIGN_SLOT_ROWS=0);

-- Ensure action slot 3 is 52226 (covers both fresh B0 and a B2R1 migration).
UPDATE `creature_template_spell`
SET `Spell`=@G17B2R3_SAFE_ACTION
WHERE `CreatureID`=@G17B2R3_ENTRY AND `Index`=@G17B2R3_SLOT
  AND `Spell` IN (@G17B2R3_LEGACY_ACTION,@G17B2R3_SAFE_ACTION)
  AND @G17B2R3_CAN_APPLY=1;

-- Ensure the landing script is bound to 52226 and only 52226.
DELETE FROM `spell_script_names`
WHERE `ScriptName` COLLATE utf8mb4_unicode_ci=@G17B2R3_SCRIPT
  AND `spell_id`<>@G17B2R3_SAFE_ACTION
  AND @G17B2R3_CAN_APPLY=1;

INSERT INTO `spell_script_names` (`spell_id`,`ScriptName`)
SELECT @G17B2R3_SAFE_ACTION,@G17B2R3_SCRIPT FROM DUAL
WHERE @G17B2R3_CAN_APPLY=1
  AND NOT EXISTS (
      SELECT 1 FROM `spell_script_names`
      WHERE `spell_id`=@G17B2R3_SAFE_ACTION
        AND `ScriptName` COLLATE utf8mb4_unicode_ci=@G17B2R3_SCRIPT
  );

COMMIT;

SET @G17B2R3_POST_ACTION := (
    SELECT COUNT(*) FROM `creature_template_spell`
    WHERE `CreatureID`=@G17B2R3_ENTRY AND `Index`=@G17B2R3_SLOT
      AND `Spell`=@G17B2R3_SAFE_ACTION
);
SET @G17B2R3_POST_LEGACY_ACTION := (
    SELECT COUNT(*) FROM `creature_template_spell`
    WHERE `CreatureID`=@G17B2R3_ENTRY AND `Index`=@G17B2R3_SLOT
      AND `Spell`=@G17B2R3_LEGACY_ACTION
);
SET @G17B2R3_POST_SCRIPT := (
    SELECT COUNT(*) FROM `spell_script_names`
    WHERE `spell_id`=@G17B2R3_SAFE_ACTION
      AND `ScriptName` COLLATE utf8mb4_unicode_ci=@G17B2R3_SCRIPT
);
SET @G17B2R3_POST_LEGACY_SCRIPT := (
    SELECT COUNT(*) FROM `spell_script_names`
    WHERE `spell_id`=@G17B2R3_LEGACY_ACTION
      AND `ScriptName` COLLATE utf8mb4_unicode_ci=@G17B2R3_SCRIPT
);

SELECT
    IF(@G17B2R3_CAN_APPLY=1 AND @G17B2R3_POST_ACTION=1
       AND @G17B2R3_POST_LEGACY_ACTION=0 AND @G17B2R3_POST_SCRIPT=1
       AND @G17B2R3_POST_LEGACY_SCRIPT=0,
       'G17B2R3_WORLD_BINDING_GUARD=PASS',
       'G17B2R3_WORLD_BINDING_GUARD=FAIL') AS `G17B2R3_RESULT`,
    @G17B2R3_CAN_APPLY AS `ownership_guard`,
    @G17B2R3_POST_ACTION AS `safe_action_rows`,
    @G17B2R3_POST_SCRIPT AS `safe_script_rows`,
    @G17B2R3_POST_LEGACY_ACTION AS `legacy_action_rows`,
    @G17B2R3_POST_LEGACY_SCRIPT AS `legacy_script_rows`;
