-- ============================================================
--  F22 追查：70231 为什么 "has data but no live creature"
--  DBeaver: Alt+X 执行全部
--  纯只读，无会话变量
-- ============================================================


-- 【1】70231 在三张关键表里各是什么状态
--     这一条就能定性，重点看最后一列
SELECT
    '1.70231三表状态' AS 检查项,
    70231 AS entry,
    (SELECT COUNT(*) FROM `characters`.`characters_npcbot`
      WHERE `entry` = 70231)                                   AS `characters_npcbot有几行`,
    (SELECT COUNT(*) FROM `world`.`creature`
      WHERE `id` = 70231)                                      AS `creature表有几行`,
    (SELECT COUNT(*) FROM `world`.`creature_template`
      WHERE `entry` = 70231)                                   AS `模板有几行`,
    (SELECT COUNT(*) FROM `world`.`creature_template_npcbot_extras`
      WHERE `entry` = 70231)                                   AS `extras有几行`,
    CASE
        WHEN (SELECT COUNT(*) FROM `world`.`creature` WHERE `id` = 70231) = 0
            THEN 'A类-creature表没实体：启动时被跳过，永远不会活'
        WHEN (SELECT COUNT(*) FROM `world`.`creature` WHERE `id` = 70231) > 1
            THEN 'B类-creature表有重复行：加载行为不确定'
        ELSE 'C类-两表都有：不是数据问题，看第2/3项'
    END AS `定性`;


-- 【2】如果 creature 表里有，它到底在哪、ScriptName 是什么
--     ScriptName='wanderer' = 装了A36，会变成游荡bot
SELECT
    '2.70231的spawn详情' AS 检查项,
    c.`guid`       AS `spawnId`,
    c.`id`         AS `entry`,
    c.`map`        AS `地图ID`,
    ROUND(c.`position_x`,1) AS `x`,
    ROUND(c.`position_y`,1) AS `y`,
    ROUND(c.`position_z`,1) AS `z`,
    c.`spawnMask`,
    c.`phaseMask`,
    c.`ScriptName` AS `脚本名`,
    CASE
        WHEN c.`ScriptName` = 'wanderer'
            THEN '装了A36-会变游荡bot-move对它无意义'
        WHEN c.`spawnMask` = 0
            THEN 'spawnMask=0！永远不会生成'
        WHEN c.`phaseMask` = 0
            THEN 'phaseMask=0！永远不可见'
        WHEN c.`map` NOT IN (0,1,530,571)
            THEN CONCAT('在非大陆地图 ', c.`map`, ' 上，可能未加载')
        ELSE '配置看起来正常'
    END AS `诊断`
FROM `world`.`creature` c
WHERE c.`id` = 70231;


-- 【3】模板本身是否合法（faction/class 为 0 会导致 AI 起不来）
SELECT
    '3.70231模板' AS 检查项,
    ct.`entry`,
    ct.`name`      AS `名字`,
    ct.`faction`,
    ct.`minlevel`,
    ct.`maxlevel`,
    ct.`ScriptName` AS `模板脚本`,
    e.`class`      AS `职业`,
    e.`race`       AS `种族`,
    CASE
        WHEN e.`entry` IS NULL THEN 'extras表缺失-不会被当bot加载'
        WHEN e.`class` = 0     THEN 'class=0-非法职业'
        WHEN ct.`faction` = 0  THEN 'faction=0-可能异常'
        ELSE '模板正常'
    END AS `诊断`
FROM `world`.`creature_template` ct
LEFT JOIN `world`.`creature_template_npcbot_extras` e ON e.`entry` = ct.`entry`
WHERE ct.`entry` = 70231;


-- 【4】70231 的 bot 数据行（owner!=0 说明被雇佣了）
SELECT
    '4.70231的bot数据' AS 检查项,
    cn.`entry`,
    cn.`owner`  AS `主人guid`,
    cn.`spec`,
    cn.`faction`,
    CASE
        WHEN cn.`owner` = 0 THEN '自由bot'
        ELSE '已被玩家雇佣-可能随主人下线而不在世界'
    END AS `诊断`
FROM `characters`.`characters_npcbot` cn
WHERE cn.`entry` = 70231;


-- 【5】把所有"有数据无实体"的 entry 全列出来（不只70231）
SELECT
    '5.所有危险entry' AS 检查项,
    cn.`entry`,
    ct.`name` AS `名字`,
    cn.`owner` AS `主人guid`
FROM `characters`.`characters_npcbot` cn
LEFT JOIN `world`.`creature` c ON c.`id` = cn.`entry`
LEFT JOIN `world`.`creature_template` ct ON ct.`entry` = cn.`entry`
WHERE c.`guid` IS NULL
ORDER BY cn.`entry`;


-- 【6】反过来：creature 表里有、但启动时可能加载失败的
--     spawnMask=0 或 phaseMask=0 是最常见的静默失败
SELECT
    '6.配置异常的bot spawn' AS 检查项,
    c.`guid` AS `spawnId`,
    c.`id`   AS `entry`,
    c.`map`  AS `地图`,
    c.`spawnMask`,
    c.`phaseMask`,
    c.`ScriptName`
FROM `world`.`creature` c
JOIN `world`.`creature_template_npcbot_extras` e ON e.`entry` = c.`id`
WHERE c.`spawnMask` = 0 OR c.`phaseMask` = 0
ORDER BY c.`id`;


-- 【7】装了 A36 的 bot 清单（ScriptName='wanderer'）
--     这些 bot 启动后会变成游荡bot，move 对它们无意义
SELECT
    '7.已开启游荡的固定bot' AS 检查项,
    c.`guid` AS `spawnId`,
    c.`id`   AS `entry`,
    ct.`name` AS `名字`,
    c.`ScriptName`
FROM `world`.`creature` c
JOIN `world`.`creature_template_npcbot_extras` e ON e.`entry` = c.`id`
LEFT JOIN `world`.`creature_template` ct ON ct.`entry` = c.`id`
WHERE c.`ScriptName` = 'wanderer'
ORDER BY c.`id`;
