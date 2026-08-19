-- ============================================================
--  A36 是否失效：ScriptName='wanderer' 但 owner!=0 的 bot
--
--  原理（实查源码）：
--    bot_ai.cpp:20412  void bot_ai::SetWanderer() { if (IAmFree()) { _wanderer = true; ... } }
--    bot_ai.cpp:15379  bool bot_ai::IAmFree() { if (!_botData->owner) return true; ... }
--    -> owner != 0 的 bot，SetWanderer() 直接空转，A36 的 ScriptName 白写
--
--  DBeaver: Alt+X 执行全部。纯只读，无会话变量。
-- ============================================================


-- 【1】总览：A36 到底对几个 bot 真正生效了
SELECT
    '1.A36生效统计' AS 检查项,
    COUNT(*) AS `标记了wanderer的总数`,
    SUM(CASE WHEN cn.`owner` = 0 OR cn.`owner` IS NULL THEN 1 ELSE 0 END) AS `真正会游荡`,
    SUM(CASE WHEN cn.`owner` <> 0 THEN 1 ELSE 0 END)                     AS `被IAmFree挡住_白写`
FROM `world`.`creature` c
JOIN `world`.`creature_template_npcbot_extras` e ON e.`entry` = c.`id`
LEFT JOIN `characters`.`characters_npcbot` cn ON cn.`entry` = c.`id`
WHERE c.`ScriptName` = 'wanderer';


-- 【2】逐个列出：哪些 bot 的 A36 标记是白写的
--     这些 bot 执行 .npcbot move 都可能报 "no live creature"
SELECT
    '2.A36失效清单' AS 检查项,
    c.`guid`  AS `spawnId`,
    c.`id`    AS `entry`,
    ct.`name` AS `名字`,
    cn.`owner` AS `主人guid`,
    ch.`name` AS `主人名字`,
    ch.`online` AS `主人在线`,
    '标记了wanderer但owner!=0，SetWanderer()空转' AS `问题`
FROM `world`.`creature` c
JOIN `world`.`creature_template_npcbot_extras` e ON e.`entry` = c.`id`
JOIN `characters`.`characters_npcbot` cn ON cn.`entry` = c.`id`
LEFT JOIN `world`.`creature_template` ct ON ct.`entry` = c.`id`
LEFT JOIN `characters`.`characters` ch ON ch.`guid` = cn.`owner`
WHERE c.`ScriptName` = 'wanderer'
  AND cn.`owner` <> 0
ORDER BY c.`id`;


-- 【3】反过来：真正会游荡的（owner=0 且标记了 wanderer）
SELECT
    '3.真正会游荡的' AS 检查项,
    c.`guid`  AS `spawnId`,
    c.`id`    AS `entry`,
    ct.`name` AS `名字`,
    c.`map`   AS `地图`
FROM `world`.`creature` c
JOIN `world`.`creature_template_npcbot_extras` e ON e.`entry` = c.`id`
JOIN `characters`.`characters_npcbot` cn ON cn.`entry` = c.`id`
LEFT JOIN `world`.`creature_template` ct ON ct.`entry` = c.`id`
WHERE c.`ScriptName` = 'wanderer'
  AND cn.`owner` = 0
ORDER BY c.`id`;


-- 【4】按主人分组，看是谁雇了这些 bot
SELECT
    '4.按主人分组' AS 检查项,
    cn.`owner`  AS `主人guid`,
    ch.`name`   AS `主人名字`,
    ch.`online` AS `在线`,
    COUNT(*)    AS `雇了几个bot`
FROM `characters`.`characters_npcbot` cn
LEFT JOIN `characters`.`characters` ch ON ch.`guid` = cn.`owner`
WHERE cn.`owner` <> 0
GROUP BY cn.`owner`, ch.`name`, ch.`online`
ORDER BY COUNT(*) DESC;
