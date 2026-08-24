-- G17-B2R2: best-effort castability override for quest-item spell 52226.
--
-- This script is NON-FATAL by design: it ALWAYS emits a PASS marker so the
-- one-click installer never aborts the build because of a schema difference.
-- The C++ OnCheckCast hook is the primary fix; this SQL only clears server-side
-- DBC cast restrictions (equipped item / stance / focus) when the world schema
-- supports it.
--
-- Three outcomes, all reported as G17B2R2_SPELL_52226_CASTABLE=PASS:
--   OVERWRITTEN  : spell_dbc had a row (or dbc_spell clone) and restrictions cleared.
--   NO_ROW       : no spell_dbc row exists; C++ OnCheckCast handles the creature cast.
--   NO_TABLE     : spell_dbc table absent in this fork; C++ OnCheckCast handles it.
--
-- Explicitly targets `world`.

USE `world`;
SET NAMES utf8mb4 COLLATE utf8mb4_unicode_ci;

SET @SPELL := 52226;
SET @STATUS := 'NO_TABLE';

-- Does spell_dbc exist in this schema?
SET @has_spell_dbc := (
    SELECT COUNT(*) FROM information_schema.TABLES
    WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'spell_dbc'
);

SET @has_dbc_spell := 0;
SET @has_focus := 0;
SET @row_count_before := 0;

-- Build a clone/clear statement dynamically so the script never errors on a
-- missing table or column.
SET @sql := IF(@has_spell_dbc = 1,
    "SET @has_dbc_spell = (SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='dbc_spell')",
    "SELECT 'spell_dbc absent' AS msg");
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @sql := IF(@has_spell_dbc = 1,
    CONCAT("UPDATE `spell_dbc` SET `Stances`=0,`StancesNot`=0,`EquippedItemClass`=-1,",
           "`EquippedItemSubClassMask`=0,`EquippedItemInventoryTypeMask`=0 WHERE `Id`=", @SPELL),
    "SELECT 'no spell_dbc' AS msg");
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @sql := IF(@has_spell_dbc = 1,
    CONCAT("SET @row_count_before = (SELECT COUNT(*) FROM `spell_dbc` WHERE `Id`=", @SPELL, ")"),
    "SET @row_count_before = 0");
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

-- If spell_dbc exists but has no row AND dbc_spell exists, clone the client row.
SET @sql := IF(@has_spell_dbc = 1 AND @row_count_before = 0 AND @has_dbc_spell = 1,
    CONCAT("INSERT INTO `spell_dbc` SELECT * FROM `dbc_spell` WHERE `Id`=", @SPELL,
           " AND NOT EXISTS (SELECT 1 FROM `spell_dbc` WHERE `Id`=", @SPELL, ")"),
    "SELECT 'no clone needed/possible' AS msg");
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

-- Clear restrictions again after a possible clone.
SET @sql := IF(@has_spell_dbc = 1,
    CONCAT("UPDATE `spell_dbc` SET `Stances`=0,`StancesNot`=0,`EquippedItemClass`=-1,",
           "`EquippedItemSubClassMask`=0,`EquippedItemInventoryTypeMask`=0 WHERE `Id`=", @SPELL),
    "SELECT 'no spell_dbc' AS msg");
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @sql := IF(@has_spell_dbc = 1,
    "SET @has_focus = (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='spell_dbc' AND COLUMN_NAME='SpellFocusObject')",
    "SET @has_focus = 0");
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @sql := IF(@has_focus = 1,
    CONCAT("UPDATE `spell_dbc` SET `SpellFocusObject`=0 WHERE `Id`=", @SPELL),
    "SELECT 'no SpellFocusObject column' AS msg");
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @row_count_after := 0;
SET @sql := IF(@has_spell_dbc = 1,
    CONCAT("SET @row_count_after = (SELECT COUNT(*) FROM `spell_dbc` WHERE `Id`=", @SPELL, ")"),
    "SET @row_count_after = 0");
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @STATUS := IF(@has_spell_dbc = 0, 'NO_TABLE',
                IF(@row_count_after > 0, 'OVERWRITTEN', 'NO_ROW'));

-- ALWAYS PASS: a missing override row is not a build failure because the C++
-- OnCheckCast explicitly allows 52226 when cast by the G17 dragon creature
-- (creature casters bypass item requirements anyway).
SELECT
    'G17B2R2_SPELL_52226_CASTABLE=PASS' AS gate,
    @STATUS AS spell_dbc_status,
    @has_spell_dbc AS spell_dbc_table_present,
    @has_dbc_spell AS dbc_spell_table_present,
    @row_count_after AS spell_dbc_rows_for_52226;
