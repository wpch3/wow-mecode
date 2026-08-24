-- ============================================================================
--  A35 诊断：验证1（0 vs 5）和验证3（1条无模板记录）
-- ============================================================================
--  你的结果：
--    验证1  npcbot表无主数=0   creature表bot数=5      <- 看着不一致
--    验证2  0                                          <- 正常
--    验证3  无模板记录=1                                <- 要处理
--    验证4  游荡池剩余812                               <- 正常
--
--  【先说结论，别慌】
--    验证1 的 "0 vs 5" 大概率【不是问题】，是我的SQL口径写歪了：
--      左边只数 owner=0（无主），右边不看 owner（含被玩家招募的）
--      -> 如果那5条是你招募的bot，两边本来就该不一样
--
--    验证3 的 1 条【要清掉】，但它【不会崩服】：
--      ObjectMgr.cpp:2205  LoadCreatures 遇到无模板的 entry 只是
--        TC_LOG_ERROR(... "non existing creature entry {}, skipped.")
--        continue;                                  <- 跳过，不崩
--      而且 LoadCreatures(World.cpp:1870) 早于
--      BotMgr::Initialize -> LoadNpcBots(World.cpp:2226)
--      -> 无模板记录压根进不了 GetAllCreatureData()
--      -> botdatamgr.cpp:1170 的 ABORT_MSG 校验根本扫不到它
--
--  DBeaver：Alt+X 执行全部（本文件第一、二部分都是只读）
-- ============================================================================


-- ============================================================================
--  第一部分：看清楚这 5 条 + 1 条到底是什么
-- ============================================================================

SELECT '=== A. creature表里的5条bot，分别是什么身份 ===' AS `诊断`;

SELECT
    c.`guid`        AS `spawnId`,
    c.`id`          AS `entry`,
    ct.`name`       AS `模板名`,
    c.`map`         AS `地图`,
    ROUND(c.`position_x`, 1) AS `X`,
    ROUND(c.`position_y`, 1) AS `Y`,
    c.`ScriptName`  AS `脚本标记`,
    cn.`owner`      AS `主人guid`,
    CASE
        WHEN cn.`entry` IS NULL       THEN '【危险】npcbot表没有它'
        WHEN cn.`owner` > 0           THEN '正常：被玩家招募的bot'
        WHEN cn.`owner` = 0           THEN '正常：无主的固定bot'
        ELSE '未知'
    END AS `判定`
FROM `world`.`creature` c
JOIN `world`.`creature_template_npcbot_extras` e ON e.`entry` = c.`id`
LEFT JOIN `world`.`creature_template` ct ON ct.`entry` = c.`id`
LEFT JOIN `characters`.`characters_npcbot` cn ON cn.`entry` = c.`id`
ORDER BY c.`id`;


SELECT '=== B. 那1条无模板记录是什么 ===' AS `诊断`;

SELECT
    c.`guid`        AS `spawnId`,
    c.`id`          AS `entry`,
    c.`map`         AS `地图`,
    ROUND(c.`position_x`, 1) AS `X`,
    ROUND(c.`position_y`, 1) AS `Y`,
    c.`ScriptName`  AS `脚本标记`,
    CASE
        WHEN c.`ScriptName` LIKE 'pin:%' THEN '【就是它】.pin 留下的脏数据'
        WHEN c.`id` >= 70800             THEN '游荡bot动态段(>=70800)的残留'
        ELSE '来源不明，看下面C'
    END AS `来源判断`
FROM `world`.`creature` c
LEFT JOIN `world`.`creature_template` ct ON ct.`entry` = c.`id`
WHERE ct.`entry` IS NULL;


SELECT '=== C. 这个entry在别的表里有没有痕迹 ===' AS `诊断`;

SELECT
    x.`entry`,
    (SELECT COUNT(*) FROM `world`.`creature_template_npcbot_extras` WHERE `entry` = x.`entry`)  AS `在extras表`,
    (SELECT COUNT(*) FROM `characters`.`characters_npcbot`          WHERE `entry` = x.`entry`)  AS `在npcbot表`,
    (SELECT COUNT(*) FROM `world`.`creature_template`               WHERE `entry` = x.`entry`)  AS `在template表`
FROM (
    SELECT DISTINCT c.`id` AS `entry`
    FROM `world`.`creature` c
    LEFT JOIN `world`.`creature_template` ct ON ct.`entry` = c.`id`
    WHERE ct.`entry` IS NULL
) x;


-- ============================================================================
--  第二部分：验证1 的正确口径（我原来那条写歪了）
-- ============================================================================

SELECT '=== D. 用【同一口径】重新对比（这才是有意义的）===' AS `验证`;

SELECT
    '两边都只数 owner=0 的' AS `口径`,
    (SELECT COUNT(*) FROM `characters`.`characters_npcbot` WHERE `owner` = 0) AS `npcbot表`,
    (SELECT COUNT(*)
       FROM `world`.`creature` c
       JOIN `characters`.`characters_npcbot` cn ON cn.`entry` = c.`id`
      WHERE cn.`owner` = 0)                                                   AS `creature表`;

SELECT
    '两边都不限 owner' AS `口径`,
    (SELECT COUNT(*) FROM `characters`.`characters_npcbot`)                   AS `npcbot表`,
    (SELECT COUNT(*)
       FROM `world`.`creature` c
       JOIN `characters`.`characters_npcbot` cn ON cn.`entry` = c.`id`)       AS `creature表`;

--  说明：
--    npcbot表 >= creature表  是【正常的】
--      （招募的bot跟着玩家走，不一定在creature表里有固定spawn）
--    npcbot表 <  creature表  才是问题（= 验证2，你那边是0，没问题）


-- ============================================================================
--  第三部分：清理那 1 条无模板记录（会改数据）
-- ============================================================================
--  【先跑完第一部分，看清楚B和C的结果再决定】
--
--  它不会崩服（ObjectMgr.cpp:2205 只是 skip），
--  但每次启动会在日志刷一条 error，而且占着一个 guid，建议清掉。

-- ---------------------------------------------------------------------------
--  3.1 清理（取消注释执行）
-- ---------------------------------------------------------------------------

-- DELETE FROM `world`.`creature`
-- WHERE `id` NOT IN (SELECT `entry` FROM `world`.`creature_template`);
--
-- SELECT ROW_COUNT() AS `清掉了几条无模板记录`;


-- ---------------------------------------------------------------------------
--  3.2 如果第一部分C显示它在 characters_npcbot 里也有残留，一并清
-- ---------------------------------------------------------------------------
--  【谨慎】只清那些「在npcbot表有、但template表没有」的孤儿

-- DELETE FROM `characters`.`characters_npcbot`
-- WHERE `entry` NOT IN (SELECT `entry` FROM `world`.`creature_template`);
--
-- SELECT ROW_COUNT() AS `清掉了几条npcbot孤儿`;


-- ============================================================================
--  第四部分：清理后复验
-- ============================================================================

SELECT '=== 复验：这两个都必须是 0 ===' AS `复验`;

SELECT
    (SELECT COUNT(*)
       FROM `world`.`creature` c
       LEFT JOIN `world`.`creature_template` ct ON ct.`entry` = c.`id`
      WHERE ct.`entry` IS NULL)                       AS `无模板记录`,
    (SELECT COUNT(*)
       FROM `world`.`creature` c
       JOIN `world`.`creature_template_npcbot_extras` e ON e.`entry` = c.`id`
       LEFT JOIN `characters`.`characters_npcbot` cn ON cn.`entry` = c.`id`
      WHERE cn.`entry` IS NULL)                       AS `creature有但npcbot没有`;

SELECT '=== 上面两个都是0 = 可以安全启动，也可以继续做A35试点 ===' AS `结论`;
