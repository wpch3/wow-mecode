-- G17-B2R2: make the quest-item command 52226 "飞行器着陆" castable from the G17
-- dragonriding vehicle, for every mount type, without breaking its original use.
--
-- Root cause of "skill 4 cannot be used on any mount": 52226 is a real quest-item
-- vehicle-landing spell. Its DBC row can carry an equipped-item requirement
-- (EquippedItemClass/SubClass/InventoryType) and/or stance/shape/focus restrictions
-- that the core enforces in Spell::CheckCast BEFORE the SpellScript OnCheckCast
-- hook runs. The G17 dragon is not the original flying machine and is not carrying
-- that item, so the cast is rejected before our landing handler ever runs.
--
-- This is a SERVER-SIDE spell_dbc override only (no client change). We clone the
-- client row into spell_dbc if needed, then clear the cast-restriction columns.
-- The C++ side still suppresses the default dummy effect and only starts the G17
-- landing state machine, so the spell's native quest behavior cannot fire while on
-- a G17 dragon.
--
-- Idempotent. Explicitly targets `world`.

USE `world`;
SET NAMES utf8mb4 COLLATE utf8mb4_unicode_ci;

SET @SPELL := 52226;

START TRANSACTION;

-- Does the client mirror table `dbc_spell` exist? (TC NPCBOT/Eluna forks differ.)
SET @has_dbc_spell := (
    SELECT COUNT(*) FROM information_schema.TABLES
    WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'dbc_spell'
);

-- Clone the client DBC row into spell_dbc only if:
--   * dbc_spell exists, and
--   * spell_dbc does not already have a row for 52226.
-- If spell_dbc already has the row, the NOT EXISTS guard makes this a no-op and
-- we patch that existing row below. If neither table has a row, the final SELECT
-- reports FAIL so the user can send the probe output instead of guessing.
SET @clone_sql := IF(@has_dbc_spell = 1,
    CONCAT(
        'INSERT INTO `spell_dbc` SELECT * FROM `dbc_spell` ',
        'WHERE `Id` = ', @SPELL,
        ' AND NOT EXISTS (SELECT 1 FROM `spell_dbc` WHERE `Id` = ', @SPELL, ')'),
    'SELECT ''dbc_spell absent; relying on existing spell_dbc row'' AS msg');
PREPARE stmt FROM @clone_sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

-- Clear the cast restrictions on the (now present) override row.
UPDATE `spell_dbc`
SET
    `Stances`                      = 0,
    `StancesNot`                   = 0,
    `EquippedItemClass`            = -1,
    `EquippedItemSubClassMask`     = 0,
    `EquippedItemInventoryTypeMask`= 0
WHERE `Id` = @SPELL;

-- Clear SpellFocusObject if the column exists in this fork.
SET @has_focus := (
    SELECT COUNT(*) FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'spell_dbc'
      AND COLUMN_NAME = 'SpellFocusObject'
);
SET @focus_sql := IF(@has_focus = 1,
    CONCAT('UPDATE `spell_dbc` SET `SpellFocusObject`=0 WHERE `Id`=', @SPELL),
    'SELECT ''no SpellFocusObject column'' AS msg');
PREPARE stmt FROM @focus_sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

COMMIT;

-- Report what we actually did.
SELECT
    CASE WHEN EXISTS (SELECT 1 FROM `spell_dbc` WHERE `Id` = @SPELL)
         THEN 'G17B2R2_SPELL_52226_OVERRIDE=PASS'
         ELSE 'G17B2R2_SPELL_52226_OVERRIDE=FAIL_NO_SPELL_DBC_ROW'
    END AS result,
    (SELECT `EquippedItemClass`       FROM `spell_dbc` WHERE `Id` = @SPELL) AS equipped_item_class,
    (SELECT `EquippedItemSubClassMask` FROM `spell_dbc` WHERE `Id` = @SPELL) AS equipped_subclass_mask,
    (SELECT `Stances`                FROM `spell_dbc` WHERE `Id` = @SPELL) AS stances,
    @has_dbc_spell AS dbc_spell_table_present;
