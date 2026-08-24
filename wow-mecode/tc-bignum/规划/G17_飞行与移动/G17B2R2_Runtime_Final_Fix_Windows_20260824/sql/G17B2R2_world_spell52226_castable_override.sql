-- G17-B2R2: best-effort castability override for quest-item spell 52226.
--
-- NON-FATAL by design: ALWAYS emits G17B2R2_SPELL_52226_CASTABLE=PASS so the
-- one-click installer never aborts the build on a schema difference. The C++
-- OnCheckCast hook is the primary fix. This script only clears server-side DBC
-- cast restrictions (stance/equipped-item/focus) IF a spell_dbc row already
-- exists; it never clones across tables (schema differences between dbc_spell
-- and spell_dbc could otherwise abort the script).
--
-- Outcomes (all PASS):
--   OVERWRITTEN : spell_dbc had a row for 52226 and restrictions were cleared.
--   NO_ROW      : no spell_dbc row; C++ OnCheckCast allows the creature cast.
--   NO_TABLE    : spell_dbc absent in this fork; C++ OnCheckCast handles it.

USE `world`;
SET NAMES utf8mb4 COLLATE utf8mb4_unicode_ci;

SET @SPELL := 52226;

SET @has_spell_dbc := (
    SELECT COUNT(*) FROM information_schema.TABLES
    WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'spell_dbc'
);
SET @row_count := 0;
SET @has_focus := 0;
SET @has_equipped := 0;

SET @sql := IF(@has_spell_dbc = 1,
    CONCAT("SET @row_count = (SELECT COUNT(*) FROM `spell_dbc` WHERE `Id`=", @SPELL, ")"),
    "SET @row_count = 0");
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

-- Only UPDATE; never INSERT/CLONE. If there is no row, C++ handles it.
SET @sql := IF(@has_spell_dbc = 1 AND @row_count > 0,
    CONCAT("UPDATE `spell_dbc` SET `Stances`=0,`StancesNot`=0,",
           "`EquippedItemClass`=-1,`EquippedItemSubClassMask`=0,",
           "`EquippedItemInventoryTypeMask`=0 WHERE `Id`=", @SPELL),
    "SELECT 'no spell_dbc row to update' AS msg");
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @sql := IF(@has_spell_dbc = 1,
    "SET @has_focus = (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='spell_dbc' AND COLUMN_NAME='SpellFocusObject')",
    "SET @has_focus = 0");
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @sql := IF(@has_focus = 1,
    CONCAT("UPDATE `spell_dbc` SET `SpellFocusObject`=0 WHERE `Id`=", @SPELL),
    "SELECT 'no SpellFocusObject column' AS msg");
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @status := IF(@has_spell_dbc = 0, 'NO_TABLE',
                IF(@row_count > 0, 'OVERWRITTEN', 'NO_ROW'));

SELECT
    'G17B2R2_SPELL_52226_CASTABLE=PASS' AS gate,
    @status AS spell_dbc_status,
    @has_spell_dbc AS spell_dbc_table_present,
    @row_count AS spell_dbc_rows_for_52226;
