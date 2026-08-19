-- ============================================================================
--  一次性把所有可能查清楚。纯只读，不改任何数据。
--  DBeaver: Alt+X 执行全部，把【每一项】的结果都发给我
-- ============================================================================


SELECT '=== A. 三张表的数量对比 ===' AS `项`;

SELECT
    (SELECT COUNT(*) FROM `characters`.`characters_npcbot`)                       AS `A1_bot数据行数`,
    (SELECT COUNT(*) FROM `world`.`creature` c
        JOIN `world`.`creature_template_npcbot_extras` e ON e.`entry`=c.`id`)     AS `A2_creature里的bot实体`,
    (SELECT COUNT(*) FROM `world`.`creature_template_npcbot_extras`)              AS `A3_extras模板总数`,
    (SELECT COUNT(*) FROM `world`.`creature_template` WHERE `entry` BETWEEN 70001 AND 70799)
                                                                                  AS `A4_模板70001到70799`;


SELECT '=== B. 15个固定bot的完整体检（最重要）===' AS `项`;

SELECT
    cn.`entry`                                          AS `entry`,
    ct.`name`                                           AS `名字`,
    c.`guid`                                            AS `spawnId`,
    c.`map`                                             AS `地图`,
    c.`spawnMask`                                       AS `spawnMask`,
    c.`phaseMask`                                       AS `phaseMask`,
    c.`ScriptName`                                      AS `脚本名`,
    e.`class`                                           AS `职业`,
    e.`race`                                            AS `种族`,
    ct.`faction`                                        AS `阵营`,
    ct.`minlevel`                                       AS `最低等级`,
    cn.`owner`                                          AS `主人guid`,
    CASE WHEN eq.`CreatureID` IS NULL
         THEN '【缺装备模板-会ASSERT失败】' ELSE 'OK' END AS `装备模板`,
    CASE
        WHEN c.`guid` IS NULL       THEN '【creature表无实体】'
        WHEN e.`entry` IS NULL      THEN '【extras表缺失-不当bot加载】'
        WHEN ct.`entry` IS NULL     THEN '【creature_template缺失】'
        WHEN c.`spawnMask` = 0      THEN '【spawnMask=0永不生成】'
        WHEN c.`phaseMask` = 0      THEN '【phaseMask=0永不可见】'
        WHEN e.`class` = 0          THEN '【class=0非法】'
        ELSE '数据看起来正常'
    END                                                 AS `诊断`
FROM `characters`.`characters_npcbot` cn
LEFT JOIN `world`.`creature` c            ON c.`id`     = cn.`entry`
LEFT JOIN `world`.`creature_template` ct  ON ct.`entry` = cn.`entry`
LEFT JOIN `world`.`creature_template_npcbot_extras` e ON e.`entry` = cn.`entry`
LEFT JOIN `world`.`creature_equip_template` eq ON eq.`CreatureID` = cn.`entry` AND eq.`ID` = 1
ORDER BY cn.`entry`;


SELECT '=== C. 装备模板缺失检查（InitEquips会ASSERT）===' AS `项`;

--  bot_ai.cpp:15086-15087
--    EquipmentInfo const* einfo = BotDataMgr::GetBotEquipmentInfo(me->GetEntry());
--    ASSERT(einfo, "Trying to spawn bot with no equip info!");
--  没有 creature_equip_template 记录 -> 启动直接 ABORT 或 bot 起不来
SELECT
    cn.`entry`,
    ct.`name` AS `名字`,
    '缺 creature_equip_template' AS `问题`
FROM `characters`.`characters_npcbot` cn
LEFT JOIN `world`.`creature_equip_template` eq
       ON eq.`CreatureID` = cn.`entry` AND eq.`ID` = 1
LEFT JOIN `world`.`creature_template` ct ON ct.`entry` = cn.`entry`
WHERE eq.`CreatureID` IS NULL;


SELECT '=== D. extras 表缺失检查 ===' AS `项`;

--  botdatamgr.cpp:1178-1185
--    没有 extras -> report_inavlid_ids -> ABORT_MSG 直接崩服
SELECT
    cn.`entry`,
    '缺 creature_template_npcbot_extras' AS `问题`
FROM `characters`.`characters_npcbot` cn
LEFT JOIN `world`.`creature_template_npcbot_extras` e ON e.`entry` = cn.`entry`
WHERE e.`entry` IS NULL;


SELECT '=== E. creature表有bot实体但没bot数据（会ABORT崩服）===' AS `项`;

--  botdatamgr.cpp:1168-1174
SELECT
    c.`guid` AS `spawnId`,
    c.`id`   AS `entry`,
    '在creature表但characters_npcbot没数据' AS `问题`
FROM `world`.`creature` c
JOIN `world`.`creature_template` ct ON ct.`entry` = c.`id`
LEFT JOIN `characters`.`characters_npcbot` cn ON cn.`entry` = c.`id`
WHERE c.`id` >= 70001
  AND ct.`entry` IS NOT NULL
  AND cn.`entry` IS NULL;


SELECT '=== F. 同一entry在creature表出现多次 ===' AS `项`;

SELECT
    c.`id` AS `entry`,
    COUNT(*) AS `出现次数`,
    GROUP_CONCAT(c.`guid` ORDER BY c.`guid`) AS `所有guid`
FROM `world`.`creature` c
JOIN `world`.`creature_template_npcbot_extras` e ON e.`entry` = c.`id`
GROUP BY c.`id`
HAVING COUNT(*) > 1;


SELECT '=== G. 这些bot所在地图/坐标是否合法 ===' AS `项`;

SELECT
    c.`id`   AS `entry`,
    c.`map`  AS `地图`,
    ROUND(c.`position_x`,1) AS `x`,
    ROUND(c.`position_y`,1) AS `y`,
    ROUND(c.`position_z`,1) AS `z`,
    CASE
        WHEN c.`position_x` = 0 AND c.`position_y` = 0 THEN '【坐标是0,0-非法】'
        WHEN c.`map` NOT IN (0,1,530,571)              THEN '【非大陆地图】'
        WHEN c.`position_z` < -500                     THEN '【Z坐标异常】'
        ELSE 'OK'
    END AS `坐标诊断`
FROM `world`.`creature` c
JOIN `world`.`creature_template_npcbot_extras` e ON e.`entry` = c.`id`
ORDER BY c.`id`;


SELECT '=== H. 主人是否存在（owner指向已删除角色会出问题）===' AS `项`;

SELECT
    cn.`entry`,
    cn.`owner` AS `主人guid`,
    ch.`name`  AS `主人名字`,
    ch.`online` AS `在线`,
    CASE
        WHEN cn.`owner` = 0             THEN '自由bot'
        WHEN ch.`guid` IS NULL          THEN '【主人角色已不存在】'
        ELSE '主人正常'
    END AS `诊断`
FROM `characters`.`characters_npcbot` cn
LEFT JOIN `characters`.`characters` ch ON ch.`guid` = cn.`owner`
ORDER BY cn.`entry`;
