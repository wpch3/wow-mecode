-- ============================================================================
--  A36 v2  给固定bot开启游荡 —— 零变量版（直接跑，不用填任何东西）
-- ============================================================================
--  【为什么出 v2】
--  v1 用了 SET @from_entry / @one_entry 这类会话变量。
--  DBeaver 里如果用 Ctrl+Enter【只执行光标处那一条】，
--  SET 那行没跑到，变量是 NULL，UPDATE 的 WHERE id = NULL 永远不匹配
--  -> 静默返回 0 行，不报错。这大概率就是你遇到的情况。
--
--  v2 完全不用变量，每条 SQL 都能单独执行。
--
--  【前提】bot 必须已经在 world.creature 表里（= 先跑过 A35）
--  你的数据已确认有 15 条（70001-70316），可以直接用。
--
--  DBeaver：Alt+X 执行全部
-- ============================================================================


-- ============================================================================
--  第一部分：先看现状
-- ============================================================================

SELECT '=== 1. creature 表里的所有 bot（这些才能被改）===' AS `步骤`;

SELECT
    c.`guid`       AS `spawnId`,
    c.`id`         AS `entry`,
    ct.`name`      AS `名字`,
    c.`map`        AS `地图`,
    c.`ScriptName` AS `当前标记`,
    CASE
        WHEN c.`ScriptName` = 'wanderer' THEN '已开启游荡'
        WHEN c.`ScriptName` = '' OR c.`ScriptName` IS NULL THEN '站桩（默认）'
        ELSE CONCAT('其它脚本: ', c.`ScriptName`)
    END AS `状态`
FROM `world`.`creature` c
JOIN `world`.`creature_template_npcbot_extras` e ON e.`entry` = c.`id`
LEFT JOIN `world`.`creature_template` ct ON ct.`entry` = c.`id`
ORDER BY c.`id`;


SELECT '=== 2. 统计 ===' AS `步骤`;

SELECT
    COUNT(*)                                                     AS `creature表bot总数`,
    SUM(CASE WHEN c.`ScriptName` = 'wanderer' THEN 1 ELSE 0 END) AS `已开启游荡`,
    SUM(CASE WHEN c.`ScriptName` = '' OR c.`ScriptName` IS NULL
             THEN 1 ELSE 0 END)                                  AS `站桩的`
FROM `world`.`creature` c
JOIN `world`.`creature_template_npcbot_extras` e ON e.`entry` = c.`id`;


-- ============================================================================
--  第二部分：开启游荡（零变量，取消注释直接跑）
-- ============================================================================


-- ---------------------------------------------------------------------------
--  2.1 【推荐】给 creature 表里【所有】bot 开启游荡
-- ---------------------------------------------------------------------------
--  自动匹配，不用填 entry。只改本来就是空标记的，不动别的脚本。
--
--  这就是「永久化游荡bot」= 三层架构里的第2层

-- UPDATE `world`.`creature`
-- SET `ScriptName` = 'wanderer'
-- WHERE `id` IN (SELECT `entry` FROM `world`.`creature_template_npcbot_extras`)
--   AND (`ScriptName` = '' OR `ScriptName` IS NULL);
--
-- SELECT ROW_COUNT() AS `开启了几个`;


-- ---------------------------------------------------------------------------
--  2.2 只给【指定的几个】开启（entry 直接写在 IN 里，不用变量）
-- ---------------------------------------------------------------------------
--  改括号里的数字即可

-- UPDATE `world`.`creature`
-- SET `ScriptName` = 'wanderer'
-- WHERE `id` IN (70001, 70002, 70003)
--   AND `id` IN (SELECT `entry` FROM `world`.`creature_template_npcbot_extras`);
--
-- SELECT ROW_COUNT() AS `开启了几个`;


-- ---------------------------------------------------------------------------
--  2.3 留一部分当【固定据点bot】（三层架构第1层）
-- ---------------------------------------------------------------------------
--  比如：70001-70005 留着站桩当据点，其余都开游荡

-- UPDATE `world`.`creature`
-- SET `ScriptName` = 'wanderer'
-- WHERE `id` IN (SELECT `entry` FROM `world`.`creature_template_npcbot_extras`)
--   AND `id` > 70005
--   AND (`ScriptName` = '' OR `ScriptName` IS NULL);
--
-- SELECT ROW_COUNT() AS `开启了几个`;


-- ---------------------------------------------------------------------------
--  2.4 关闭游荡（改回站桩）
-- ---------------------------------------------------------------------------

-- UPDATE `world`.`creature`
-- SET `ScriptName` = ''
-- WHERE `ScriptName` = 'wanderer';
--
-- SELECT ROW_COUNT() AS `关闭了几个`;


-- ============================================================================
--  第三部分：验证
-- ============================================================================

SELECT '=== 验证：标记结果 ===' AS `验证`;

SELECT
    CASE WHEN c.`ScriptName` = '' OR c.`ScriptName` IS NULL
         THEN '(空=站桩)' ELSE c.`ScriptName` END AS `标记`,
    COUNT(*) AS `数量`
FROM `world`.`creature` c
JOIN `world`.`creature_template_npcbot_extras` e ON e.`entry` = c.`id`
GROUP BY 1;


SELECT '=== 逐条确认 ===' AS `验证`;

SELECT
    c.`id`         AS `entry`,
    ct.`name`      AS `名字`,
    c.`ScriptName` AS `标记`
FROM `world`.`creature` c
JOIN `world`.`creature_template_npcbot_extras` e ON e.`entry` = c.`id`
LEFT JOIN `world`.`creature_template` ct ON ct.`entry` = c.`id`
ORDER BY c.`id`;


-- ============================================================================
--  改完必须【重启服务端】
-- ============================================================================
--  ScriptName 在 ObjectMgr::LoadCreatures() 时读进 CreatureData::scriptId，
--  运行时改数据库不会更新内存。
--
--  重启后验证：选中bot用 .pin status
--    应显示  world.creature : 有   且   IsWandererBot() : 是
--    这是以前不可能同时出现的组合。
--
--  【前提】A36 的代码改动（bot_ai.cpp:246 那处）必须已经编译进去，
--          否则这个标记不起任何作用。
-- ============================================================================
