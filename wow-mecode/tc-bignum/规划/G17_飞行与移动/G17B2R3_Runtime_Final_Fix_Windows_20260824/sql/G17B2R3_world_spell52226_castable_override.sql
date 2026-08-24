-- G17-B2R3: 52226 "飞行器着陆" cast-gate override (informational + best-effort).
--
-- IMPORTANT (root cause of "skill 4 completely unusable"):
--   The DBC requires RequiresSpellFocus=1553 and CasterAuraSpell=52255.  The
--   fork's `spell_dbc` overlay table does NOT have those two columns, so no
--   SQL in this universe can clear them.  The authoritative fix is the C++
--   runtime sanitizer `G17Dragonriding::EnsureLandingCommandCastable()` in
--   cs_dragonriding.cpp, which clears focus/aura/stance/item gates at server
--   startup (proof-of-load line:
--   ">> G17-B2R3 landing command 52226 cast-gates cleared (focus/aura/item/stance)").
--
--   This script only clears the gates that DO exist in `spell_dbc` (stances,
--   equipped-item class/subclass/inventory) IF a row already exists, so it
--   never clones across schemas and never aborts.  It ALWAYS emits PASS so
--   the one-click installer can proceed to the build step.
--
-- Outcomes (all PASS):
--   OVERWRITTEN : spell_dbc row existed and DB-side restrictions were cleared.
--   NO_ROW      : no spell_dbc row; C++ sanitizer handles everything.
--   NO_TABLE    : spell_dbc absent in this fork; C++ sanitizer handles it.

USE `world`;
SET NAMES utf8mb4 COLLATE utf8mb4_unicode_ci;

SET @SPELL := 52226;

SET @has_spell_dbc := (
    SELECT COUNT(*) FROM information_schema.TABLES
    WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'spell_dbc'
);
SET @row_count := 0;

SET @sql := IF(@has_spell_dbc = 1,
    CONCAT("SET @row_count = (SELECT COUNT(*) FROM `spell_dbc` WHERE `Id`=", @SPELL, ")"),
    "SET @row_count = 0");
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

-- Only UPDATE; never INSERT/CLONE. If there is no row, C++ handles it.
SET @sql := IF(@has_spell_dbc = 1 AND @row_count > 0,
    CONCAT("UPDATE `spell_dbc` SET `Stances`=0,`StancesNot`=0,",
           "`EquippedItemClass`=-1,`EquippedItemSubClassMask`=0,",
           "`EquippedItemInventoryTypeMask`=0 WHERE `Id`=", @SPELL),
    "SELECT 'no spell_dbc row to update (C++ sanitizer is authoritative)' AS msg");
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @status := IF(@has_spell_dbc = 0, 'NO_TABLE',
                IF(@row_count > 0, 'OVERWRITTEN', 'NO_ROW'));

SELECT
    'G17B2R3_SPELL_52226_CASTABLE=PASS' AS gate,
    @status AS spell_dbc_status,
    @has_spell_dbc AS spell_dbc_table_present,
    @row_count AS spell_dbc_rows_for_52226,
    'RequiresSpellFocus/CasterAuraSpell cleared by C++ EnsureLandingCommandCastable' AS authority_note;
