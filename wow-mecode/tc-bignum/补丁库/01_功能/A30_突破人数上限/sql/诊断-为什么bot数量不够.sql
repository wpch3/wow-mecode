-- ============================================================================
--  诊断：Only N out of M bots ... Aborting!
-- ============================================================================
--
--  报错原文：
--    Only 312 out of 317 bots of enabled classes aren't spawned.
--    Desired amount of wandering bots (500) cannot be created. Aborting!
--
--  【怎么读这句话】
--    317 = 数据库里【职业被conf启用】的 bot 模板总数
--    312 = 其中还没被人拥有的（317 - characters_npcbot的5行 = 312）
--    500 = 你 conf 里写的 NpcBot.WanderingBots.Continents.Count
--
--    312 < 500  ->  ASSERT 崩服
--
--  【bot 被统计的两个必要条件】botdatamgr.cpp:316-328
--    1. 在 creature_template_npcbot_extras 表里有记录
--    2. 它的 class 被 conf 的 NpcBot.WanderingBots.Classes.*.Enable 启用
--
--    光在 creature_template 里【不算数】。
--
--  执行：Alt+X 全部执行，看每一步的输出。
-- ============================================================================

USE `world`;


-- ############################################################################
--  第 1 步：三张表各有多少（最关键的一步）
-- ############################################################################

SELECT
    (SELECT COUNT(*) FROM `world`.`creature_template`
     WHERE `entry` >= 70001)                                      AS 'creature_template中的bot',
    (SELECT COUNT(*) FROM `world`.`creature_template_npcbot_extras`) AS 'extras表(决定是否算bot)',
    (SELECT COUNT(*) FROM `characters`.`characters_npcbot`)        AS '已被拥有',
    (SELECT COUNT(*) FROM `world`.`creature_template_npcbot_extras`)
  - (SELECT COUNT(*) FROM `characters`.`characters_npcbot`)        AS '理论可用备用数';

--  【怎么判断】
--   如果 "creature_template中的bot" 远大于 "extras表"
--   -> 说明第5步(插入extras)没执行成功，这是最常见的原因


-- ############################################################################
--  第 2 步：找出"有template但没extras"的孤儿（就是漏插的那批）
-- ############################################################################

SELECT COUNT(*) AS '孤儿数量(有template没extras，不算bot)'
FROM `world`.`creature_template` ct
LEFT JOIN `world`.`creature_template_npcbot_extras` e ON e.`entry` = ct.`entry`
WHERE ct.`entry` >= 70001 AND e.`entry` IS NULL;

-- 看看是哪些
SELECT ct.`entry`, ct.`name`
FROM `world`.`creature_template` ct
LEFT JOIN `world`.`creature_template_npcbot_extras` e ON e.`entry` = ct.`entry`
WHERE ct.`entry` >= 70001 AND e.`entry` IS NULL
ORDER BY ct.`entry`
LIMIT 10;


-- ############################################################################
--  第 3 步：按职业统计（看是不是职业没启用）
-- ############################################################################

SELECT
    e.`class`   AS '职业ID',
    CASE e.`class`
        WHEN 1  THEN '战士'    WHEN 2  THEN '圣骑士'  WHEN 3  THEN '猎人'
        WHEN 4  THEN '潜行者'  WHEN 5  THEN '牧师'    WHEN 6  THEN '死亡骑士'
        WHEN 7  THEN '萨满'    WHEN 8  THEN '法师'    WHEN 9  THEN '术士'
        WHEN 11 THEN '德鲁伊'
        WHEN 12 THEN '兽王(扩展)'      WHEN 13 THEN '斯芬克斯(扩展)'
        WHEN 14 THEN '大法师(扩展)'    WHEN 15 THEN '恐惧魔王(扩展)'
        WHEN 16 THEN '斩杀者(扩展)'    WHEN 17 THEN '黑暗游侠(扩展)'
        WHEN 18 THEN '亡灵法师(扩展)'  WHEN 19 THEN '海巫(扩展)'
        WHEN 20 THEN '地穴领主(扩展)'
        ELSE CONCAT('未知(', e.`class`, ')')
    END         AS '职业',
    COUNT(*)    AS '模板数'
FROM `world`.`creature_template_npcbot_extras` e
GROUP BY e.`class`
ORDER BY COUNT(*) DESC;

--  【对照你的 conf】
--  NpcBot.WanderingBots.Classes.Warrior.Enable = 1
--  NpcBot.WanderingBots.Classes.Paladin.Enable = 1
--  ... 等等
--
--  只有【启用的职业】才计入那个 317。
--  如果你生成的500个全是某个【没启用】的职业，那它们一个都不算。


-- ############################################################################
--  第 4 步：看你生成的那批到底在不在
-- ############################################################################

SET @start_id := 71000;    -- 改成你当初设的起点
SET @count    := 500;

SELECT
    (SELECT COUNT(*) FROM `world`.`creature_template`
     WHERE `entry` BETWEEN @start_id AND @start_id + @count - 1)   AS '你生成的-template',
    (SELECT COUNT(*) FROM `world`.`creature_template_npcbot_extras`
     WHERE `entry` BETWEEN @start_id AND @start_id + @count - 1)   AS '你生成的-extras',
    CASE WHEN (SELECT COUNT(*) FROM `world`.`creature_template`
               WHERE `entry` BETWEEN @start_id AND @start_id + @count - 1)
            = (SELECT COUNT(*) FROM `world`.`creature_template_npcbot_extras`
               WHERE `entry` BETWEEN @start_id AND @start_id + @count - 1)
         THEN '[OK] 两表一致'
         ELSE '[问题] extras 缺失，这就是原因'
    END AS '判断';


-- ############################################################################
--  第 5 步：修复 —— 给孤儿补上 extras
-- ############################################################################
--
--  如果第2步显示有孤儿、第4步显示 extras 缺失，执行下面这句补齐。
--  它会把【源模板的职业和种族】复制给所有缺失的 entry。

SET @src_entry := 70001;   -- 改成你当初用的源模板

-- 先看会补多少条（预览，不改数据）
SELECT COUNT(*) AS '将要补充的extras条数'
FROM `world`.`creature_template` ct
LEFT JOIN `world`.`creature_template_npcbot_extras` e ON e.`entry` = ct.`entry`
WHERE ct.`entry` BETWEEN @start_id AND @start_id + @count - 1
  AND e.`entry` IS NULL;

-- 确认数字合理后，执行补充
INSERT INTO `world`.`creature_template_npcbot_extras` (`entry`, `class`, `race`)
SELECT ct.`entry`, src.`class`, src.`race`
FROM `world`.`creature_template` ct
CROSS JOIN (SELECT `class`, `race` FROM `world`.`creature_template_npcbot_extras`
            WHERE `entry` = @src_entry) src
LEFT JOIN `world`.`creature_template_npcbot_extras` e ON e.`entry` = ct.`entry`
WHERE ct.`entry` BETWEEN @start_id AND @start_id + @count - 1
  AND e.`entry` IS NULL;

SELECT ROW_COUNT() AS '实际补充条数';


-- ############################################################################
--  第 6 步：修复后复查
-- ############################################################################

SELECT
    (SELECT COUNT(*) FROM `world`.`creature_template_npcbot_extras`) AS 'extras总数',
    (SELECT COUNT(*) FROM `characters`.`characters_npcbot`)          AS '已被拥有',
    (SELECT COUNT(*) FROM `world`.`creature_template_npcbot_extras`)
  - (SELECT COUNT(*) FROM `characters`.`characters_npcbot`)          AS '可用备用数';

--  【把 conf 的 Count 改成小于等于"可用备用数"的值】
--  比如可用 812，就写 800，留点余量。


-- ############################################################################
--  应急：先让服务端能启动
-- ############################################################################
--
--  不想折腾的话，直接把 conf 改小就能启动：
--
--    worldserver.conf.d\unlimited.conf
--    NpcBot.WanderingBots.Continents.Count = 300
--
--  （300 < 312，一定能启动）
--  启动后再慢慢查模板的事。


-- ############################################################################
--  为什么会漏
-- ############################################################################
--
--  批量生成SQL是分步的：
--    第4步 -> creature_template
--    第5步 -> creature_template_npcbot_extras     <- 漏了这步就白干
--    第6步 -> appearance（可选）
--    第7步 -> equip（可选）
--
--  4.3 那次报 Duplicate entry 之后，如果你只重跑了第4步，
--  第5步就没执行 -> template 有500个，extras 还是原来的317个。
--
--  botdatamgr.cpp:316 遍历的是 extras 表，
--  所以那500个"不算bot"。
