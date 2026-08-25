-- G17-B3R1: bind the 25 custom combat carriers to spell_g17_combat_skill.
-- Idempotent, ownership-guarded, collation-safe, explicit `world`.
-- Refuses if any of the 25 ids is already bound to a DIFFERENT script.

USE `world`;
SET NAMES utf8mb4 COLLATE utf8mb4_unicode_ci;

SET @G17B3R1_SCRIPT := _utf8mb4'spell_g17_combat_skill' COLLATE utf8mb4_unicode_ci;
SET @G17B3R1_LO := 990000;
SET @G17B3R1_HI := 990024;

SET @G17B3R1_FOREIGN := (
  SELECT COUNT(*) FROM `spell_script_names`
  WHERE `spell_id` BETWEEN @G17B3R1_LO AND @G17B3R1_HI
    AND `ScriptName` COLLATE utf8mb4_unicode_ci <> @G17B3R1_SCRIPT
);
SET @G17B3R1_EXISTING := (
  SELECT COUNT(*) FROM `spell_script_names`
  WHERE `spell_id` BETWEEN @G17B3R1_LO AND @G17B3R1_HI
    AND `ScriptName` COLLATE utf8mb4_unicode_ci = @G17B3R1_SCRIPT
);

START TRANSACTION;

INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`)
SELECT n, @G17B3R1_SCRIPT
FROM (
  SELECT 990000 + a.n + 10*b.n + 100*c.n AS n
  FROM (SELECT 0 n UNION SELECT 1 UNION SELECT 2 UNION SELECT 3 UNION SELECT 4) a
  CROSS JOIN (SELECT 0 n) b
  CROSS JOIN (SELECT 0 n) c
) AS _ids
WHERE n BETWEEN @G17B3R1_LO AND @G17B3R1_HI
  AND @G17B3R1_FOREIGN = 0
  AND NOT EXISTS (
      SELECT 1 FROM `spell_script_names`
      WHERE `spell_id`=n
        AND `ScriptName` COLLATE utf8mb4_unicode_ci=@G17B3R1_SCRIPT
  );

COMMIT;

SET @G17B3R1_POST := (
  SELECT COUNT(*) FROM `spell_script_names`
  WHERE `spell_id` BETWEEN @G17B3R1_LO AND @G17B3R1_HI
    AND `ScriptName` COLLATE utf8mb4_unicode_ci = @G17B3R1_SCRIPT
);
SET @G17B3R1_POST_FOREIGN := (
  SELECT COUNT(*) FROM `spell_script_names`
  WHERE `spell_id` BETWEEN @G17B3R1_LO AND @G17B3R1_HI
    AND `ScriptName` COLLATE utf8mb4_unicode_ci <> @G17B3R1_SCRIPT
);

SELECT
  IF(@G17B3R1_FOREIGN=0 AND @G17B3R1_POST=25 AND @G17B3R1_POST_FOREIGN=0,
     'G17B3R1_WORLD_COMBAT_BINDING=PASS',
     'G17B3R1_WORLD_COMBAT_BINDING=FAIL') AS `G17B3R1_RESULT`,
  @G17B3R1_FOREIGN AS foreign_rows,
  @G17B3R1_EXISTING AS existing_rows,
  @G17B3R1_POST AS bound_rows,
  @G17B3R1_POST_FOREIGN AS post_foreign_rows;
