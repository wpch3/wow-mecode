-- ============================================================================
--  F23  修复：固定bot在 `creature` 表里的实体丢失
--
--  症状：.npcbot move 报
--        "NpcBot 70231 has data but no live creature!
--         Not loaded / dead / grid unloaded, or missing from `creature` table."
--        全部固定bot都不行（70231/70002...），但游荡bot 800个完全正常
--
--  为什么游荡bot没事：游荡bot的 spawnId=0，压根不读 `creature` 表
--        botdatamgr.cpp:210  LoadBotCreatureFromDB(0, map, true, true, ...)
--
--  为什么固定bot全废：启动加载时按 entry 查 `creature` 表
--        botdatamgr.cpp:1104  SELECT guid, map, position_x, ... FROM creature WHERE id = {}
--        botdatamgr.cpp:1105-1108  if (!infores) { LOG_ERROR("...not found in `creature` table!"); continue; }
--        查不到 -> continue 跳过 -> bot 永远不 spawn
--        -> 不进 _existingBots -> FindBot 返回 nullptr -> 报你看到的那条错
--
--  本脚本做什么：
--        第一部分  只读诊断，告诉你到底缺了什么（先跑这个）
--        第二部分  自动补全缺失的 `creature` 记录（默认注释，看完诊断再放开）
--        第三部分  一致性收尾检查
--
--  DBeaver：Alt+X 执行全部
--  纯零变量，每条都能单独 Ctrl+Enter
-- ============================================================================


-- ============================================================================
--  第一部分：诊断（只读，安全，先跑）
-- ============================================================================

SELECT '=== 1. 核心结论：有bot数据但creature表没实体的 ===' AS `步骤`;

--  这些就是 .npcbot move 会失败的 bot
SELECT
    cn.`entry`                          AS `entry`,
    ct.`name`                           AS `bot名字`,
    e.`class`                           AS `职业ID`,
    cn.`owner`                          AS `主人guid`,
    '缺creature记录，启动时被跳过'      AS `问题`
FROM `characters`.`characters_npcbot` cn
LEFT JOIN `world`.`creature` c            ON c.`id`     = cn.`entry`
LEFT JOIN `world`.`creature_template` ct  ON ct.`entry` = cn.`entry`
LEFT JOIN `world`.`creature_template_npcbot_extras` e ON e.`entry` = cn.`entry`
WHERE c.`guid` IS NULL
ORDER BY cn.`entry`;


SELECT '=== 2. 数量对比（一眼看出问题规模）===' AS `步骤`;

SELECT
    (SELECT COUNT(*) FROM `characters`.`characters_npcbot`)          AS `bot数据总数`,
    (SELECT COUNT(*) FROM `world`.`creature` c
        JOIN `world`.`creature_template_npcbot_extras` e ON e.`entry` = c.`id`)
                                                                     AS `creature表bot实体数`,
    (SELECT COUNT(*) FROM `characters`.`characters_npcbot` cn
        LEFT JOIN `world`.`creature` c ON c.`id` = cn.`entry`
        WHERE c.`guid` IS NULL)                                      AS `缺实体的_要修的就是这个`;


SELECT '=== 3. 现存bot实体长什么样（用来照抄坐标/参数）===' AS `步骤`;

SELECT
    c.`guid`   AS `spawnId`,
    c.`id`     AS `entry`,
    ct.`name`  AS `名字`,
    c.`map`    AS `地图`,
    ROUND(c.`position_x`,1) AS `x`,
    ROUND(c.`position_y`,1) AS `y`,
    ROUND(c.`position_z`,1) AS `z`,
    c.`spawnMask`,
    c.`phaseMask`,
    c.`ScriptName`
FROM `world`.`creature` c
JOIN `world`.`creature_template_npcbot_extras` e ON e.`entry` = c.`id`
LEFT JOIN `world`.`creature_template` ct ON ct.`entry` = c.`id`
ORDER BY c.`id`;


SELECT '=== 4. guid 占用情况（补记录要用没被占的guid）===' AS `步骤`;

SELECT
    MAX(`guid`)        AS `creature表最大guid`,
    MAX(`guid`) + 1    AS `下一个可用guid`,
    COUNT(*)           AS `creature表总行数`
FROM `world`.`creature`;


-- ============================================================================
--  第二部分：修复（会改数据，默认全部注释）
--
--  看完第一部分再决定放开哪一段。
--  DBeaver 批量取消注释：选中若干行按 Ctrl+/
-- ============================================================================


-- ---------------------------------------------------------------------------
--  2.1 【推荐】自动补全所有缺失的 creature 记录
-- ---------------------------------------------------------------------------
--  把所有「有bot数据但没实体」的 bot 补进 creature 表。
--  坐标统一放在暴风城喷泉广场（map 0, -8936.7 -129.2 83.3）。
--  补完重启服务端，bot 就会在那里出现，然后你可以用 .npcbot move 挪走。
--
--  guid 自动从当前最大值往后排，不会撞号。
--  ScriptName 设为 'wanderer'（保持A36的游荡能力，配合A38锚点用）。
--  如果你想让它们站桩不动，把 'wanderer' 改成 ''。

-- INSERT INTO `world`.`creature`
--     (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`,
--      `equipment_id`, `position_x`, `position_y`, `position_z`, `orientation`,
--      `spawntimesecs`, `wander_distance`, `currentwaypoint`, `curhealth`, `curmana`,
--      `MovementType`, `npcflag`, `unit_flags`, `dynamicflags`, `ScriptName`, `VerifiedBuild`)
-- SELECT
--     (SELECT MAX(`guid`) FROM `world`.`creature`)
--         + ROW_NUMBER() OVER (ORDER BY cn.`entry`)          AS `guid`,
--     cn.`entry`                                             AS `id`,
--     0                                                      AS `map`,
--     0                                                      AS `zoneId`,
--     0                                                      AS `areaId`,
--     1                                                      AS `spawnMask`,
--     1                                                      AS `phaseMask`,
--     0                                                      AS `equipment_id`,
--     -8936.7                                                AS `position_x`,
--     -129.2                                                 AS `position_y`,
--     83.3                                                   AS `position_z`,
--     0                                                      AS `orientation`,
--     300                                                    AS `spawntimesecs`,
--     0                                                      AS `wander_distance`,
--     0                                                      AS `currentwaypoint`,
--     1                                                      AS `curhealth`,
--     0                                                      AS `curmana`,
--     0                                                      AS `MovementType`,
--     0                                                      AS `npcflag`,
--     0                                                      AS `unit_flags`,
--     0                                                      AS `dynamicflags`,
--     'wanderer'                                             AS `ScriptName`,
--     0                                                      AS `VerifiedBuild`
-- FROM `characters`.`characters_npcbot` cn
-- LEFT JOIN `world`.`creature` c ON c.`id` = cn.`entry`
-- WHERE c.`guid` IS NULL
--   AND cn.`entry` IN (SELECT `entry` FROM `world`.`creature_template_npcbot_extras`);
--
-- SELECT ROW_COUNT() AS `补了几条creature记录`;


-- ---------------------------------------------------------------------------
--  2.2 只补【指定的几个】entry
-- ---------------------------------------------------------------------------
--  改最后那个 IN 列表即可

-- INSERT INTO `world`.`creature`
--     (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`,
--      `equipment_id`, `position_x`, `position_y`, `position_z`, `orientation`,
--      `spawntimesecs`, `wander_distance`, `currentwaypoint`, `curhealth`, `curmana`,
--      `MovementType`, `npcflag`, `unit_flags`, `dynamicflags`, `ScriptName`, `VerifiedBuild`)
-- SELECT
--     (SELECT MAX(`guid`) FROM `world`.`creature`)
--         + ROW_NUMBER() OVER (ORDER BY cn.`entry`),
--     cn.`entry`, 0, 0, 0, 1, 1, 0,
--     -8936.7, -129.2, 83.3, 0,
--     300, 0, 0, 1, 0, 0, 0, 0, 0, 'wanderer', 0
-- FROM `characters`.`characters_npcbot` cn
-- LEFT JOIN `world`.`creature` c ON c.`id` = cn.`entry`
-- WHERE c.`guid` IS NULL
--   AND cn.`entry` IN (70002, 70231);
--
-- SELECT ROW_COUNT() AS `补了几条`;


-- ---------------------------------------------------------------------------
--  2.3 反向清理：bot数据本身也不想要了
-- ---------------------------------------------------------------------------
--  【慎用】这是删掉 bot 数据，不是补全。
--  只有当你确认这些 bot 不要了才用。会丢装备和等级。

-- DELETE FROM `characters`.`characters_npcbot`
-- WHERE `entry` NOT IN (SELECT `id` FROM `world`.`creature`);
--
-- SELECT ROW_COUNT() AS `删了几条bot数据`;


-- ============================================================================
--  第三部分：修复后验证 + 跨表一致性收尾
--  （改完数据必跑，三个数字都必须是 0）
-- ============================================================================

SELECT '=== 5. 收尾检查：下面三个数字必须都是 0 ===' AS `步骤`;

SELECT
    '5a.有bot数据但creature无实体' AS `检查项`,
    COUNT(*) AS `必须为0`
FROM `characters`.`characters_npcbot` cn
LEFT JOIN `world`.`creature` c ON c.`id` = cn.`entry`
WHERE c.`guid` IS NULL;

SELECT
    '5b.creature有实体但无bot数据' AS `检查项`,
    COUNT(*) AS `必须为0`
FROM `world`.`creature` c
JOIN `world`.`creature_template_npcbot_extras` e ON e.`entry` = c.`id`
LEFT JOIN `characters`.`characters_npcbot` cn ON cn.`entry` = c.`id`
WHERE cn.`entry` IS NULL;

SELECT
    '5c.组队成员无creature实体' AS `检查项`,
    COUNT(*) AS `必须为0`
FROM `characters`.`characters_npcbot_group_member` gm
LEFT JOIN `world`.`creature` c ON c.`id` = gm.`entry`
WHERE c.`guid` IS NULL;


SELECT '=== 6. guid 唯一性（补记录后必查，撞号会崩服）===' AS `步骤`;

SELECT
    `guid`,
    COUNT(*) AS `重复次数`
FROM `world`.`creature`
GROUP BY `guid`
HAVING COUNT(*) > 1;
--  上面这条【没有任何输出】才是对的


SELECT '=== 7. 同一entry重复spawn（会让加载只取第一条）===' AS `步骤`;

SELECT
    c.`id`   AS `entry`,
    COUNT(*) AS `出现次数`,
    GROUP_CONCAT(c.`guid` ORDER BY c.`guid`) AS `所有guid`
FROM `world`.`creature` c
JOIN `world`.`creature_template_npcbot_extras` e ON e.`entry` = c.`id`
GROUP BY c.`id`
HAVING COUNT(*) > 1;
--  上面这条【没有任何输出】才是对的


SELECT '=== 8. 修复后的最终状态 ===' AS `步骤`;

SELECT
    c.`guid`  AS `spawnId`,
    c.`id`    AS `entry`,
    ct.`name` AS `名字`,
    c.`map`   AS `地图`,
    ROUND(c.`position_x`,1) AS `x`,
    ROUND(c.`position_y`,1) AS `y`,
    c.`ScriptName`,
    CASE WHEN c.`ScriptName` = 'wanderer' THEN '游荡' ELSE '站桩' END AS `行为`
FROM `world`.`creature` c
JOIN `world`.`creature_template_npcbot_extras` e ON e.`entry` = c.`id`
LEFT JOIN `world`.`creature_template` ct ON ct.`entry` = c.`id`
ORDER BY c.`id`;
