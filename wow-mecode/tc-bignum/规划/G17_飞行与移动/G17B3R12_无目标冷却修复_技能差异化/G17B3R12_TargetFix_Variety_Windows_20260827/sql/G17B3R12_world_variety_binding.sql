-- G17-B3R12: bind the two skill-variety carriers to their spell scripts.
-- Explicit, idempotent, ownership-guarded, collation-safe (B3R1 pattern).

USE `world`;
SET NAMES utf8mb4 COLLATE utf8mb4_unicode_ci;

SET @G17B3R12_LO := 990029;
SET @G17B3R12_HI := 990030;

-- Refuse if either id is already bound to a DIFFERENT script.
SET @G17B3R12_FOREIGN := (
  SELECT COUNT(*) FROM `spell_script_names`
  WHERE `spell_id` BETWEEN @G17B3R12_LO AND @G17B3R12_HI
    AND `ScriptName` COLLATE utf8mb4_unicode_ci NOT IN
        (_utf8mb4'spell_g17_swoop_strike' COLLATE utf8mb4_unicode_ci,
         _utf8mb4'spell_g17_wind_stance' COLLATE utf8mb4_unicode_ci)
);

START TRANSACTION;

INSERT IGNORE INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
  (990029, 'spell_g17_swoop_strike'),
  (990030, 'spell_g17_wind_stance');

COMMIT;

-- Re-count AFTER the insert.
SET @G17B3R12_POST := (
  SELECT COUNT(*) FROM `spell_script_names`
  WHERE (`spell_id` = 990029 AND `ScriptName` COLLATE utf8mb4_unicode_ci = _utf8mb4'spell_g17_swoop_strike' COLLATE utf8mb4_unicode_ci)
     OR (`spell_id` = 990030 AND `ScriptName` COLLATE utf8mb4_unicode_ci = _utf8mb4'spell_g17_wind_stance' COLLATE utf8mb4_unicode_ci)
);

SELECT
  IF(@G17B3R12_FOREIGN=0 AND @G17B3R12_POST=2,
     'G17B3R12_WORLD_VARIETY_BINDING=PASS',
     'G17B3R12_WORLD_VARIETY_BINDING=FAIL') AS `G17B3R12_RESULT`,
  @G17B3R12_FOREIGN AS foreign_rows,
  @G17B3R12_POST AS bound_rows;
