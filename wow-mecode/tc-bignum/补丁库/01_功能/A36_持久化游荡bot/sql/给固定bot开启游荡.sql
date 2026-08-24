-- ============================================================================
--  A36  给固定bot打上 wanderer 标记 —— 让它们既永久又游荡
-- ============================================================================
--  【前提1】必须先跑 A35，让 bot 进入 world.creature 表
--          否则下面的 UPDATE 匹配 0 行（用户已实测踩过）
--          游荡bot是纯内存对象(botdatamgr.cpp:460)，creature表里没有它们
--
--  【前提2】必须先装 A36 的代码改动（bot_ai.cpp:246 那处）
--          没装代码的话这个标记不起任何作用（ScriptName 对bot本来就无副作用）
--
--  【原理】改后的 bot_ai::InitializeAI()：
--    bool wantWander = !me->GetSpawnId();
--    if (!wantWander)
--        if (CreatureData const* cdata = me->GetCreatureData())
--            if (cdata->scriptId == GetScriptId("wanderer"))
--                wantWander = true;
--
--  【重要】ScriptName 写在 `creature` 表（每个spawn单独设），
--          不是 creature_template
--
--  【必须重启】ScriptName 在 ObjectMgr::LoadCreatures 时读进内存
--
--  DBeaver：Alt+X 执行全部
-- ============================================================================


-- ============================================================================
--  第一部分：先看现状（只查不改）
-- ============================================================================

SELECT '=== 1. 哪些bot已经在 creature 表里（即"永久bot"）===' AS `步骤`;

SELECT
    c.`guid`       AS `spawnId`,
    c.`id`         AS `entry`,
    ct.`name`      AS `名字`,
    c.`map`        AS `地图`,
    c.`ScriptName` AS `当前标记`,
    CASE
        WHEN c.`ScriptName` = 'wanderer' THEN '【已开启游荡】'
        WHEN c.`ScriptName` = ''         THEN '站桩（官方默认）'
        ELSE CONCAT('其它脚本: ', c.`ScriptName`)
    END AS `状态`
FROM `world`.`creature` c
JOIN `world`.`creature_template_npcbot_extras` e ON e.`entry` = c.`id`
LEFT JOIN `world`.`creature_template` ct ON ct.`entry` = c.`id`
ORDER BY c.`id`;


SELECT '=== 2. 统计 ===' AS `步骤`;

SELECT
    COUNT(*)                                                      AS `永久bot总数`,
    SUM(CASE WHEN c.`ScriptName` = 'wanderer' THEN 1 ELSE 0 END)  AS `已开启游荡`,
    SUM(CASE WHEN c.`ScriptName` = '' OR c.`ScriptName` IS NULL
             THEN 1 ELSE 0 END)                                   AS `站桩的`
FROM `world`.`creature` c
JOIN `world`.`creature_template_npcbot_extras` e ON e.`entry` = c.`id`;


-- ============================================================================
--  第二部分：开启游荡（会改数据）
-- ============================================================================
--  【默认注释掉，确认第一部分后手动放开】


-- ---------------------------------------------------------------------------
--  2.1 给【指定范围】的bot开启游荡（推荐，可控）
-- ---------------------------------------------------------------------------

-- SET @from_entry := 71001;      -- 起始 entry
-- SET @to_entry   := 71500;      -- 结束 entry
--
-- UPDATE `world`.`creature`
-- SET `ScriptName` = 'wanderer'
-- WHERE `id` BETWEEN @from_entry AND @to_entry
--   AND `id` IN (SELECT `entry` FROM `world`.`creature_template_npcbot_extras`);
--
-- SELECT ROW_COUNT() AS `开启了几个`;


-- ---------------------------------------------------------------------------
--  2.2 给【单个】bot开启（测试用）
-- ---------------------------------------------------------------------------

-- SET @one_entry := 71001;
--
-- UPDATE `world`.`creature`
-- SET `ScriptName` = 'wanderer'
-- WHERE `id` = @one_entry;
--
-- SELECT ROW_COUNT() AS `开启了几个`;


-- ---------------------------------------------------------------------------
--  2.3 给【全部永久bot】开启（谨慎！会影响你 .npcbot spawn 出来的）
-- ---------------------------------------------------------------------------
--  【警告】这会把你手动 spawn 的bot也变成游荡的。
--  只有在你确定"所有永久bot都该游荡"时才用。

-- UPDATE `world`.`creature`
-- SET `ScriptName` = 'wanderer'
-- WHERE `id` IN (SELECT `entry` FROM `world`.`creature_template_npcbot_extras`)
--   AND (`ScriptName` = '' OR `ScriptName` IS NULL);
--
-- SELECT ROW_COUNT() AS `开启了几个`;


-- ---------------------------------------------------------------------------
--  2.4 关闭游荡（改回站桩）
-- ---------------------------------------------------------------------------

-- SET @from_entry := 71001;
-- SET @to_entry   := 71500;
--
-- UPDATE `world`.`creature`
-- SET `ScriptName` = ''
-- WHERE `id` BETWEEN @from_entry AND @to_entry
--   AND `ScriptName` = 'wanderer';
--
-- SELECT ROW_COUNT() AS `关闭了几个`;


-- ============================================================================
--  第三部分：验证
-- ============================================================================

SELECT '=== 验证：标记结果 ===' AS `验证`;

SELECT
    c.`ScriptName` AS `标记`,
    COUNT(*)       AS `数量`
FROM `world`.`creature` c
JOIN `world`.`creature_template_npcbot_extras` e ON e.`entry` = c.`id`
GROUP BY c.`ScriptName`;


SELECT '=== 改完必须【重启服务端】才生效 ===' AS `重要`;

--  原因：ScriptName 在 ObjectMgr::LoadCreatures() 时读进 CreatureData::scriptId，
--        运行时改数据库不会更新内存。
--
--  重启后验证：选中bot用 .pin status
--    应该显示  world.creature : 有   且   IsWandererBot() : 是
--    这是以前不可能同时出现的组合。
