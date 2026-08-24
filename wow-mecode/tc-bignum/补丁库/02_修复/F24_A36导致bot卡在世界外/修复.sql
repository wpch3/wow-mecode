-- ============================================================================
--  F24  修复：A36 的 ScriptName='wanderer' 导致固定bot卡在世界外
--
--  症状：这15个bot 无法 move、无法 spawn；
--        但新 spawn 的bot 一切正常；delete 后重新 spawn 也正常
--        唯一区别就是 creature.ScriptName
--
--  根因（两行源码）：
--    botdatamgr.cpp:1126  固定bot加载
--        bot->LoadBotCreatureFromDB(tableGuid, map, false, false, entry, &spawnPos)
--                                                    ^^^^^ addToMap = false
--        -> 出生时【不在世界里】，要靠后续 Update 拉进去
--
--    botdatamgr.cpp:832   那个负责拉进世界的 Update
--        if (!bot->IsInWorld() && bot->FindMap() && !bot->IsWandererBot() && ...)
--                                                   ^^^^^^^^^^^^^^^^^^^^^
--        -> A36 让它们 IsWandererBot()==true，这个条件【永远为假】
--        -> 永远卡在世界外 -> 不进 _existingBots -> FindBot 返回 nullptr
--
--    对照：.npcbot spawn 走 botcommands.cpp:3865
--        creature->LoadBotCreatureFromDB(db_guid, map)   <- addToMap 默认 true
--        当场进世界，所以新spawn的能用
--
--  结论：A36 的 ScriptName='wanderer' 方案对【creature表里的固定bot】不可用。
--        真正的游荡bot是 spawnId=0、不进 creature 表的那批（你的800个，一切正常）。
--
--  DBeaver: Alt+X 执行全部。零变量。
-- ============================================================================


-- ============================================================================
--  第一部分：修复前状态
-- ============================================================================

SELECT '=== 1. 修复前：被A36卡住的bot ===' AS `步骤`;

SELECT
    c.`guid`  AS `spawnId`,
    c.`id`    AS `entry`,
    ct.`name` AS `名字`,
    c.`map`   AS `地图`,
    c.`ScriptName` AS `脚本名`,
    '卡在世界外，无法move/spawn' AS `状态`
FROM `world`.`creature` c
JOIN `world`.`creature_template_npcbot_extras` e ON e.`entry` = c.`id`
LEFT JOIN `world`.`creature_template` ct ON ct.`entry` = c.`id`
WHERE c.`ScriptName` = 'wanderer'
ORDER BY c.`id`;


-- ============================================================================
--  第二部分：修复（这两条直接跑，不用取消注释）
-- ============================================================================

--  2.1 清空 ScriptName，让它们回到正常的固定bot
UPDATE `world`.`creature`
SET `ScriptName` = ''
WHERE `id` IN (SELECT `entry` FROM `world`.`creature_template_npcbot_extras`)
  AND `ScriptName` = 'wanderer';


SELECT ROW_COUNT() AS `2_1_修好了几个`;


--  2.2 顺带清掉 A38 锚点残留（miscvalues 里的 40-44 号键）
--      这些bot不再是游荡bot，锚点没有意义；留着不影响运行，但清掉更干净。
--      如果你之后想用 A38，重新 .npcbot move 一次就会写回去。
UPDATE `characters`.`characters_npcbot`
SET `miscvalues` = ''
WHERE `entry` IN (SELECT `entry` FROM `world`.`creature_template_npcbot_extras`)
  AND `miscvalues` IS NOT NULL
  AND `miscvalues` <> '';


SELECT ROW_COUNT() AS `2_2_清了几个锚点残留`;


-- ============================================================================
--  第三部分：验证（下面这些必须全部符合预期）
-- ============================================================================

SELECT '=== 3. 修复后：应该一个 wanderer 都不剩 ===' AS `步骤`;

SELECT
    COUNT(*) AS `还剩几个wanderer_必须为0`
FROM `world`.`creature` c
JOIN `world`.`creature_template_npcbot_extras` e ON e.`entry` = c.`id`
WHERE c.`ScriptName` = 'wanderer';


SELECT '=== 4. 修复后的15个bot状态 ===' AS `步骤`;

SELECT
    c.`guid`  AS `spawnId`,
    c.`id`    AS `entry`,
    ct.`name` AS `名字`,
    c.`map`   AS `地图`,
    ROUND(c.`position_x`,1) AS `x`,
    ROUND(c.`position_y`,1) AS `y`,
    CASE WHEN c.`ScriptName` = '' OR c.`ScriptName` IS NULL
         THEN '正常固定bot-可move'
         ELSE CONCAT('异常:', c.`ScriptName`) END AS `状态`,
    cn.`owner` AS `主人guid`
FROM `world`.`creature` c
JOIN `world`.`creature_template_npcbot_extras` e ON e.`entry` = c.`id`
LEFT JOIN `world`.`creature_template` ct ON ct.`entry` = c.`id`
LEFT JOIN `characters`.`characters_npcbot` cn ON cn.`entry` = c.`id`
ORDER BY c.`id`;


-- ============================================================================
--  第四部分：跨表一致性收尾（三个数字必须都是 0）
-- ============================================================================

SELECT '=== 5. 一致性检查 ===' AS `步骤`;

SELECT '5a.有bot数据但creature无实体' AS `检查项`, COUNT(*) AS `必须为0`
FROM `characters`.`characters_npcbot` cn
LEFT JOIN `world`.`creature` c ON c.`id` = cn.`entry`
WHERE c.`guid` IS NULL;

SELECT '5b.creature有实体但无bot数据' AS `检查项`, COUNT(*) AS `必须为0`
FROM `world`.`creature` c
JOIN `world`.`creature_template_npcbot_extras` e ON e.`entry` = c.`id`
LEFT JOIN `characters`.`characters_npcbot` cn ON cn.`entry` = c.`id`
WHERE cn.`entry` IS NULL;

SELECT '5c.组队成员无creature实体' AS `检查项`, COUNT(*) AS `必须为0`
FROM `characters`.`characters_npcbot_group_member` gm
LEFT JOIN `world`.`creature` c ON c.`id` = gm.`entry`
WHERE c.`guid` IS NULL;
