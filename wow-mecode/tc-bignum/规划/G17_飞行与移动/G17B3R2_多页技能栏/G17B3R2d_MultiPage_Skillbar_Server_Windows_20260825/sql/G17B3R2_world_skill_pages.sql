-- G17-B3R2: multi-page vehicle skill bar world bindings.
--   1) bind the 4 new bar carriers (990025-990028) to their SpellScripts
--   2) align creature_template_spell (CreatureID/Index/Spell) with the
--      movement page so the FIRST action bar send (before the script
--      re-writes m_spells) is already the correct page.
-- NOTE (real user run 2026-08-25): this fork has NO creature_template.spell1..8
-- columns (ERROR 1054).  Creature spells live in `creature_template_spell`
-- and load into CreatureTemplate.spells[Index] (ObjectMgr.cpp:670-701,
-- MAX_CREATURE_SPELLS = 8).
-- Explicit, idempotent, ownership-guarded, collation-safe (B3-R1 pattern).

USE `world`;
SET NAMES utf8mb4 COLLATE utf8mb4_unicode_ci;

SET @G17B3R2_ENTRY := 1000171;
SET @G17B3R2_LO := 990025;
SET @G17B3R2_HI := 990028;

-- ScriptName bindings for the new carriers.
SET @G17B3R2_SCRIPT_PAGE  := _utf8mb4'spell_g17_page_switch'  COLLATE utf8mb4_unicode_ci;
SET @G17B3R2_SCRIPT_CLIMB := _utf8mb4'spell_g17_ascend'       COLLATE utf8mb4_unicode_ci;
SET @G17B3R2_SCRIPT_DIVE  := _utf8mb4'spell_g17_dive'         COLLATE utf8mb4_unicode_ci;
SET @G17B3R2_SCRIPT_BRAKE := _utf8mb4'spell_g17_glide_brake'  COLLATE utf8mb4_unicode_ci;

-- Refuse if any of the 4 ids is already bound to a DIFFERENT script.
SET @G17B3R2_FOREIGN := (
  SELECT COUNT(*) FROM `spell_script_names`
  WHERE `spell_id` BETWEEN @G17B3R2_LO AND @G17B3R2_HI
    AND `ScriptName` COLLATE utf8mb4_unicode_ci NOT IN
        (@G17B3R2_SCRIPT_PAGE, @G17B3R2_SCRIPT_CLIMB,
         @G17B3R2_SCRIPT_DIVE, @G17B3R2_SCRIPT_BRAKE)
);

START TRANSACTION;

INSERT IGNORE INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
  (990025, @G17B3R2_SCRIPT_PAGE),
  (990026, @G17B3R2_SCRIPT_CLIMB),
  (990027, @G17B3R2_SCRIPT_DIVE),
  (990028, @G17B3R2_SCRIPT_BRAKE);

-- Align the template bar with the movement page (this fork's storage):
--   Index 0=990026 拉升, 1=990027 俯冲, 2=55215 推进, 3=52197 冲刺,
--   4=52226 着陆, 5=990025 切页.  B3-R2d: the stock 3.3.5 client
--   VehicleMenuBar renders a FIXED 6 spell buttons, so only indices 0-5
--   are used (brake left the bar; S key covers braking).
--   Full replace = idempotent realign.
DELETE FROM `creature_template_spell`
WHERE `CreatureID` = @G17B3R2_ENTRY AND `Index` BETWEEN 0 AND 7;

INSERT INTO `creature_template_spell` (`CreatureID`, `Index`, `Spell`) VALUES
  (@G17B3R2_ENTRY, 0, 990026),
  (@G17B3R2_ENTRY, 1, 990027),
  (@G17B3R2_ENTRY, 2, 55215),
  (@G17B3R2_ENTRY, 3, 52197),
  (@G17B3R2_ENTRY, 4, 52226),
  (@G17B3R2_ENTRY, 5, 990025);

COMMIT;

-- Re-count AFTER the writes.
SET @G17B3R2_POST := (
  SELECT COUNT(*) FROM `spell_script_names`
  WHERE `spell_id` BETWEEN @G17B3R2_LO AND @G17B3R2_HI
    AND `ScriptName` COLLATE utf8mb4_unicode_ci IN
        (@G17B3R2_SCRIPT_PAGE, @G17B3R2_SCRIPT_CLIMB,
         @G17B3R2_SCRIPT_DIVE, @G17B3R2_SCRIPT_BRAKE)
);
SET @G17B3R2_POST_FOREIGN := (
  SELECT COUNT(*) FROM `spell_script_names`
  WHERE `spell_id` BETWEEN @G17B3R2_LO AND @G17B3R2_HI
    AND `ScriptName` COLLATE utf8mb4_unicode_ci NOT IN
        (@G17B3R2_SCRIPT_PAGE, @G17B3R2_SCRIPT_CLIMB,
         @G17B3R2_SCRIPT_DIVE, @G17B3R2_SCRIPT_BRAKE)
);
-- The 7 expected template rows, exactly.
SET @G17B3R2_TEMPLATE_OK := (
  SELECT COUNT(*) FROM `creature_template_spell`
  WHERE `CreatureID` = @G17B3R2_ENTRY
    AND ((`Index` = 0 AND `Spell` = 990026) OR (`Index` = 1 AND `Spell` = 990027)
      OR (`Index` = 2 AND `Spell` = 55215)   OR (`Index` = 3 AND `Spell` = 52197)
      OR (`Index` = 4 AND `Spell` = 52226)   OR (`Index` = 5 AND `Spell` = 990025))
);
-- No stale/extra rows for the entry.
SET @G17B3R2_TEMPLATE_EXTRA := (
  SELECT COUNT(*) FROM `creature_template_spell`
  WHERE `CreatureID` = @G17B3R2_ENTRY
    AND NOT ((`Index` = 0 AND `Spell` = 990026) OR (`Index` = 1 AND `Spell` = 990027)
      OR (`Index` = 2 AND `Spell` = 55215)   OR (`Index` = 3 AND `Spell` = 52197)
      OR (`Index` = 4 AND `Spell` = 52226)   OR (`Index` = 5 AND `Spell` = 990025))
);

SELECT
  IF(@G17B3R2_FOREIGN=0 AND @G17B3R2_POST=4 AND @G17B3R2_POST_FOREIGN=0
     AND @G17B3R2_TEMPLATE_OK=6 AND @G17B3R2_TEMPLATE_EXTRA=0,
     'G17B3R2_WORLD_SKILL_PAGES=PASS',
     'G17B3R2_WORLD_SKILL_PAGES=FAIL') AS `G17B3R2_RESULT`,
  @G17B3R2_FOREIGN AS foreign_rows,
  @G17B3R2_POST AS bound_rows,
  @G17B3R2_POST_FOREIGN AS post_foreign_rows,
  @G17B3R2_TEMPLATE_OK AS template_ok,
  @G17B3R2_TEMPLATE_EXTRA AS template_extra;
