-- ============================================================================
--  彻底清理指定 entry 区间的 bot 模板（四张表全清）
-- ============================================================================
--
--  【为什么需要这个】
--    "修复-宠物模板刷屏.sql" 只删了 creature_template_npcbot_extras 一张表
--    （那个脚本的目的是"把宠物踢出bot池"，不是"删除模板"），
--    creature_template 里的 500 行还在 -> 所以区间检查报
--        [停止] 71000-71499 区间已有数据，换个起点
--
--  【一个 bot 模板涉及四张表】
--      world.creature_template                    <- 主表（区间检查查的就是它）
--      world.creature_template_npcbot_extras      <- 职业/种族
--      world.creature_template_npcbot_appearance  <- 外观（可能没有）
--      world.creature_equip_template              <- 装备（可能没有）
--    另外如果已经生成过实体，还有：
--      world.creature                             <- 已落地的实体
--      characters.characters_npcbot               <- 已被拥有的
--
--  【删除顺序很重要】先删子表，最后删 creature_template
--
--  执行：Alt+X 全部执行
-- ============================================================================

USE `world`;


-- ############################################################################
--  第 1 步：设定要清理的区间
-- ############################################################################

SET @from_id := 71000;
SET @to_id   := 71499;


-- ############################################################################
--  第 2 步：先看这个区间现在有什么（删之前必看）
-- ############################################################################

SELECT
    (SELECT COUNT(*) FROM `world`.`creature_template`
     WHERE `entry` BETWEEN @from_id AND @to_id)                    AS 'creature_template',
    (SELECT COUNT(*) FROM `world`.`creature_template_npcbot_extras`
     WHERE `entry` BETWEEN @from_id AND @to_id)                    AS 'creature_template_npcbot_extras',
    (SELECT COUNT(*) FROM `world`.`creature_template_npcbot_appearance`
     WHERE `entry` BETWEEN @from_id AND @to_id)                    AS 'appearance',
    (SELECT COUNT(*) FROM `world`.`creature_equip_template`
     WHERE `CreatureID` BETWEEN @from_id AND @to_id)               AS 'equip_template',
    (SELECT COUNT(*) FROM `world`.`creature`
     WHERE `id` BETWEEN @from_id AND @to_id)                       AS 'creature(已生成实体)',
    (SELECT COUNT(*) FROM `characters`.`characters_npcbot`
     WHERE `entry` BETWEEN @from_id AND @to_id)                    AS 'characters_npcbot(已被拥有)';

--  【重点看 creature_template 那一列】
--   它不是 0，就是区间检查报错的原因。


-- ############################################################################
--  第 3 步：安全确认 —— 别误删上游原有的 bot
-- ############################################################################
--
--  上游原生 bot 一般在 70001-70800 区间，
--  71000+ 是我们自己生成的。如果你的区间和上游重叠，下面会警告。

SELECT
    CASE WHEN @from_id < 70801
         THEN CONCAT('[警告！] 起点 ', @from_id, ' < 70801，可能删到【上游原生bot】，请确认区间')
         ELSE CONCAT('[OK] 区间 ', @from_id, '-', @to_id, ' 在自定义范围内')
    END AS '安全检查';

-- 看看要删的都是些什么（抽查前10个）
SELECT ct.`entry`, ct.`name`, HEX(ct.`flags_extra`) AS 'flags_extra',
       CASE
           WHEN (ct.`flags_extra` & 0x8E000000) = 0x8E000000 THEN 'bot'
           WHEN (ct.`flags_extra` & 0x8E000000) = 0x8A000000 THEN '宠物'
           ELSE '其它'
       END AS '类型'
FROM `world`.`creature_template` ct
WHERE ct.`entry` BETWEEN @from_id AND @to_id
ORDER BY ct.`entry`
LIMIT 10;


-- ############################################################################
--  第 4 步：执行清理（顺序不能变：先子表，后主表）
-- ############################################################################

-- 4.1 已被拥有的记录（characters 库）
DELETE FROM `characters`.`characters_npcbot`
WHERE `entry` BETWEEN @from_id AND @to_id;

-- 4.2 已生成的实体
DELETE FROM `world`.`creature`
WHERE `id` BETWEEN @from_id AND @to_id;

-- 4.3 装备
DELETE FROM `world`.`creature_equip_template`
WHERE `CreatureID` BETWEEN @from_id AND @to_id;

-- 4.4 外观
DELETE FROM `world`.`creature_template_npcbot_appearance`
WHERE `entry` BETWEEN @from_id AND @to_id;

-- 4.5 职业/种族
DELETE FROM `world`.`creature_template_npcbot_extras`
WHERE `entry` BETWEEN @from_id AND @to_id;

-- 4.6 【最后】主表 —— 这一步做完，区间检查才会通过
DELETE FROM `world`.`creature_template`
WHERE `entry` BETWEEN @from_id AND @to_id;


-- ############################################################################
--  第 5 步：验证 —— 六张表必须全为 0
-- ############################################################################

SELECT
    (SELECT COUNT(*) FROM `world`.`creature_template`
     WHERE `entry` BETWEEN @from_id AND @to_id)                    AS 'creature_template',
    (SELECT COUNT(*) FROM `world`.`creature_template_npcbot_extras`
     WHERE `entry` BETWEEN @from_id AND @to_id)                    AS 'creature_template_npcbot_extras',
    (SELECT COUNT(*) FROM `world`.`creature_template_npcbot_appearance`
     WHERE `entry` BETWEEN @from_id AND @to_id)                    AS 'appearance',
    (SELECT COUNT(*) FROM `world`.`creature_equip_template`
     WHERE `CreatureID` BETWEEN @from_id AND @to_id)               AS 'equip_template',
    (SELECT COUNT(*) FROM `world`.`creature`
     WHERE `id` BETWEEN @from_id AND @to_id)                       AS 'creature',
    (SELECT COUNT(*) FROM `characters`.`characters_npcbot`
     WHERE `entry` BETWEEN @from_id AND @to_id)                    AS 'characters_npcbot';

SELECT
    CASE WHEN (SELECT COUNT(*) FROM `world`.`creature_template`
               WHERE `entry` BETWEEN @from_id AND @to_id) = 0
         THEN CONCAT('[OK] 区间 ', @from_id, '-', @to_id, ' 已清空，可以重新生成了')
         ELSE '[仍有残留] 检查上面哪张表还有数据'
    END AS '判定';


-- ############################################################################
--  第 6 步：清完之后
-- ############################################################################
--
--  1. 查现在还剩多少可用模板：
--
--     SELECT
--         (SELECT COUNT(*) FROM `world`.`creature_template_npcbot_extras`
--          WHERE `class` <> 0)                                   AS '有效模板数',
--         (SELECT COUNT(*) FROM `characters`.`characters_npcbot`) AS '已被拥有',
--         (SELECT COUNT(*) FROM `world`.`creature_template_npcbot_extras`
--          WHERE `class` <> 0)
--       - (SELECT COUNT(*) FROM `characters`.`characters_npcbot`) AS '可用备用数';
--
--  2. 【立刻】把 conf 的 Count 改到"可用备用数"以下，否则重启崩服：
--     NpcBot.WanderingBots.Continents.Count = 300
--
--  3. 重新生成模板时，务必用 v2 脚本，它现在会检查：
--       检查2b - 源模板 class 是否为 0
--       检查2c - 源模板是不是【宠物】（这次踩的坑）
--     两个检查都是 [OK] 再往下执行。


-- ############################################################################
--  怎么挑一个【正确的】源模板
-- ############################################################################
--
--  直接用这个查询，它已经过滤掉 class=0 和宠物：
--
--  SELECT e.`entry`, e.`class`, e.`race`, ct.`name`
--  FROM `world`.`creature_template_npcbot_extras` e
--  JOIN `world`.`creature_template` ct ON ct.`entry` = e.`entry`
--  WHERE e.`class` BETWEEN 1 AND 11 AND e.`class` <> 10
--    AND (ct.`flags_extra` & 0x8E000000) = 0x8E000000    -- 必须是bot不是宠物
--  ORDER BY e.`class`, e.`entry`;
--
--  从结果里挑一个填进 v2 的 @src_entry。
