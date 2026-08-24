-- ============================================================================
--  step64 紧急：检查 .pin 有没有往 creature 表写脏数据
-- ============================================================================
--  【为什么紧急】
--  step63 版本的 .pin 绕过了上游保护，可能已经把游荡bot写进了
--  world.creature 表。而游荡bot的 creature_template 是【内存临时对象】
--  （botdatamgr.cpp:63 _botsExtraCreatureTemplates），重启后不存在。
--
--  -> creature 表里有 id，但 creature_template 里没有对应模板
--  -> ObjectMgr.cpp:10279 ASSERT_NODEBUGINFO 断言失败
--  -> 【启动时崩服】
--
--  DBeaver：Alt+X 执行全部
-- ============================================================================


-- ============================================================================
--  第一部分：诊断（只查，不改）
-- ============================================================================

SELECT '=== 1. creature表里有没有【没有模板】的记录（会崩服）===' AS `检查`;

SELECT
    c.`guid`        AS `spawnId`,
    c.`id`          AS `entry`,
    c.`map`         AS `地图`,
    c.`position_x`  AS `X`,
    c.`position_y`  AS `Y`,
    c.`ScriptName`  AS `标记`,
    '【危险】没有creature_template，启动会崩服' AS `问题`
FROM `world`.`creature` c
LEFT JOIN `world`.`creature_template` ct ON ct.`entry` = c.`id`
WHERE ct.`entry` IS NULL;


SELECT '=== 2. 被 .pin 标记过的记录（ScriptName 以 pin: 开头）===' AS `检查`;

SELECT
    c.`guid`        AS `spawnId`,
    c.`id`          AS `entry`,
    ct.`name`       AS `模板名`,
    c.`ScriptName`  AS `pin标记`,
    CASE WHEN ct.`entry` IS NULL
         THEN '【危险】无模板'
         ELSE '有模板，相对安全' END AS `状态`
FROM `world`.`creature` c
LEFT JOIN `world`.`creature_template` ct ON ct.`entry` = c.`id`
WHERE c.`ScriptName` LIKE 'pin:%';


SELECT '=== 3. entry >= 70800 的 creature 记录（游荡bot专用段）===' AS `检查`;

--  botcommon.h:41  BOT_ENTRY_CREATE_BEGIN = 70800
--  这个段的 entry 是运行时动态分配给游荡bot的，
--  【正常情况下 creature 表里不该有】

SELECT
    c.`guid`        AS `spawnId`,
    c.`id`          AS `entry`,
    c.`map`         AS `地图`,
    c.`ScriptName`  AS `标记`,
    '这个段本不该出现在creature表' AS `说明`
FROM `world`.`creature` c
WHERE c.`id` >= 70800;


SELECT '=== 4. characters_npcbot 里的孤儿（无模板）===' AS `检查`;

SELECT
    cn.`entry`  AS `entry`,
    cn.`owner`  AS `主人`,
    CASE WHEN ct.`entry` IS NULL THEN '无模板' ELSE ct.`name` END AS `模板`
FROM `characters`.`characters_npcbot` cn
LEFT JOIN `world`.`creature_template` ct ON ct.`entry` = cn.`entry`
WHERE ct.`entry` IS NULL;


-- ============================================================================
--  第二部分：清理（会改数据）
-- ============================================================================
--  【先看完第一部分，确认有问题再跑】
--  取消注释（去掉行首的 -- ）来执行


-- ---------------------------------------------------------------------------
--  2.1 删掉 creature 表里【没有模板】的记录 —— 这个必须删，否则崩服
-- ---------------------------------------------------------------------------

-- DELETE FROM `world`.`creature`
-- WHERE `id` NOT IN (SELECT `entry` FROM `world`.`creature_template`);
--
-- SELECT ROW_COUNT() AS `删掉了几条会崩服的记录`;


-- ---------------------------------------------------------------------------
--  2.2 删掉所有 pin: 标记的记录（如果 2.1 没删干净）
-- ---------------------------------------------------------------------------

-- DELETE FROM `world`.`creature` WHERE `ScriptName` LIKE 'pin:%';
-- SELECT ROW_COUNT() AS `删掉了几条pin记录`;


-- ---------------------------------------------------------------------------
--  2.3 删掉 entry >= 70800 的 creature 记录（游荡bot动态段）
-- ---------------------------------------------------------------------------
--  【谨慎】如果你用 .npcbot spawn 在这个段创建过真bot，会被一起删。
--  先跑第一部分的检查3，确认列出来的都是 pin 造成的再执行。

-- DELETE FROM `world`.`creature` WHERE `id` >= 70800;
-- SELECT ROW_COUNT() AS `删掉了几条`;


-- ---------------------------------------------------------------------------
--  2.4 清理 characters_npcbot 里的无模板孤儿
-- ---------------------------------------------------------------------------

-- DELETE FROM `characters`.`characters_npcbot`
-- WHERE `entry` NOT IN (SELECT `entry` FROM `world`.`creature_template`);
--
-- SELECT ROW_COUNT() AS `清掉了几条孤儿`;


-- ============================================================================
--  第三部分：清理后验证
-- ============================================================================

SELECT '=== 验证：下面这条应该返回 0 行 ===' AS `验证`;

SELECT c.`guid`, c.`id`
FROM `world`.`creature` c
LEFT JOIN `world`.`creature_template` ct ON ct.`entry` = c.`id`
WHERE ct.`entry` IS NULL;

SELECT '=== 如果上面是空的，就可以安全启动服务端了 ===' AS `完成`;
