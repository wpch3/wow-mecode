-- G17-B3R1: bind the 25 custom combat carriers to spell_g17_combat_skill.
-- Explicit, idempotent, ownership-guarded, collation-safe.
-- NOTE: prior revision used a "generate 25 ids from CROSS JOIN + NOT EXISTS"
-- construct; on MySQL 8 the NOT EXISTS against the target table caused the
-- INSERT to be skipped/partial so the post-count stayed 5 instead of 25 and
-- the gate failed.  This revision inserts 25 explicit rows and recounts AFTER
-- the insert.

USE `world`;
SET NAMES utf8mb4 COLLATE utf8mb4_unicode_ci;

SET @G17B3R1_SCRIPT := _utf8mb4'spell_g17_combat_skill' COLLATE utf8mb4_unicode_ci;
SET @G17B3R1_LO := 990000;
SET @G17B3R1_HI := 990024;

-- Refuse if any of the 25 ids is already bound to a DIFFERENT script.
SET @G17B3R1_FOREIGN := (
  SELECT COUNT(*) FROM `spell_script_names`
  WHERE `spell_id` BETWEEN @G17B3R1_LO AND @G17B3R1_HI
    AND `ScriptName` COLLATE utf8mb4_unicode_ci <> @G17B3R1_SCRIPT
);

START TRANSACTION;

INSERT IGNORE INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
  (990000, @G17B3R1_SCRIPT),
  (990001, @G17B3R1_SCRIPT),
  (990002, @G17B3R1_SCRIPT),
  (990003, @G17B3R1_SCRIPT),
  (990004, @G17B3R1_SCRIPT),
  (990005, @G17B3R1_SCRIPT),
  (990006, @G17B3R1_SCRIPT),
  (990007, @G17B3R1_SCRIPT),
  (990008, @G17B3R1_SCRIPT),
  (990009, @G17B3R1_SCRIPT),
  (990010, @G17B3R1_SCRIPT),
  (990011, @G17B3R1_SCRIPT),
  (990012, @G17B3R1_SCRIPT),
  (990013, @G17B3R1_SCRIPT),
  (990014, @G17B3R1_SCRIPT),
  (990015, @G17B3R1_SCRIPT),
  (990016, @G17B3R1_SCRIPT),
  (990017, @G17B3R1_SCRIPT),
  (990018, @G17B3R1_SCRIPT),
  (990019, @G17B3R1_SCRIPT),
  (990020, @G17B3R1_SCRIPT),
  (990021, @G17B3R1_SCRIPT),
  (990022, @G17B3R1_SCRIPT),
  (990023, @G17B3R1_SCRIPT),
  (990024, @G17B3R1_SCRIPT);

COMMIT;

-- Re-count AFTER the insert (the failure mode before).
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
  @G17B3R1_POST AS bound_rows,
  @G17B3R1_POST_FOREIGN AS post_foreign_rows;
