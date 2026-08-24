-- ============================================================================
--  A36 配套：把 bot 分配到三层架构
--
--  背景：A36 的全量 UPDATE(2.1) 把 creature 表里【所有】bot 都设成了
--        ScriptName='wanderer'，导致固定据点 bot 一个都不剩。
--        这个脚本用来重新分配。
--
--  原理（实查源码）：
--    bot_ai.cpp:245  InitializeAI() 里判定是否游荡
--    A36 补丁让 creature.ScriptName == 'wanderer' 的 bot 也走 SetWanderer()
--    ScriptName 为空 -> 站桩固定 bot
--    ScriptName='wanderer' -> 永久化游荡 bot
--
--  【重要】改完必须【重启服务端】才生效
--          ScriptName 是 LoadCreatures 时读进内存的，热改数据库没用
--
--  DBeaver: Alt+X 执行全部（第一部分只读，第二部分默认全部注释掉了）
--  纯零变量，每条都能单独 Ctrl+Enter
-- ============================================================================


-- ============================================================================
--  第一部分：先看清楚这 15 个 bot 都是谁（只读，安全）
-- ============================================================================

SELECT '=== 1. 当前所有 bot 的分层状态 ===' AS `步骤`;

SELECT
    c.`guid`   AS `spawnId`,
    c.`id`     AS `entry`,
    ct.`name`  AS `名字`,
    e.`class`  AS `职业ID`,
    CASE e.`class`
        WHEN 1 THEN '战士' WHEN 2 THEN '圣骑士' WHEN 3 THEN '猎人'
        WHEN 4 THEN '潜行者' WHEN 5 THEN '牧师' WHEN 6 THEN '死亡骑士'
        WHEN 7 THEN '萨满' WHEN 8 THEN '法师' WHEN 9 THEN '术士'
        WHEN 11 THEN '德鲁伊' ELSE CONCAT('特殊职业', e.`class`)
    END AS `职业`,
    c.`map`    AS `地图`,
    ROUND(c.`position_x`, 0) AS `x`,
    ROUND(c.`position_y`, 0) AS `y`,
    cn.`owner` AS `主人guid`,
    c.`ScriptName` AS `脚本名`,
    CASE
        WHEN c.`ScriptName` = 'wanderer' THEN '第2层-永久化游荡bot'
        ELSE '第1层-固定据点NPC'
    END AS `当前属于哪层`
FROM `world`.`creature` c
JOIN `world`.`creature_template_npcbot_extras` e ON e.`entry` = c.`id`
LEFT JOIN `world`.`creature_template` ct ON ct.`entry` = c.`id`
LEFT JOIN `characters`.`characters_npcbot` cn ON cn.`entry` = c.`id`
ORDER BY c.`id`;


SELECT '=== 2. 三层架构统计 ===' AS `步骤`;

SELECT
    SUM(CASE WHEN c.`ScriptName` <> 'wanderer' OR c.`ScriptName` IS NULL
             THEN 1 ELSE 0 END)                                   AS `第1层_固定据点`,
    SUM(CASE WHEN c.`ScriptName` = 'wanderer' THEN 1 ELSE 0 END)  AS `第2层_永久游荡`,
    COUNT(*)                                                      AS `creature表bot总数`,
    '第3层完全随机游荡bot不在creature表里(spawnId=0)' AS `说明`
FROM `world`.`creature` c
JOIN `world`.`creature_template_npcbot_extras` e ON e.`entry` = c.`id`;


-- ============================================================================
--  第二部分：重新分配（默认全部注释，按需取消注释）
--
--  取消注释方法：选中要跑的那几行，DBeaver 按 Ctrl+/ 批量取消注释
-- ============================================================================


-- ---------------------------------------------------------------------------
--  方案A：全部恢复成固定据点 bot（撤销 A36）
-- ---------------------------------------------------------------------------
--  跑完 15 个全部站桩不动，回到 A36 之前的状态

-- UPDATE `world`.`creature`
-- SET `ScriptName` = ''
-- WHERE `id` IN (SELECT `entry` FROM `world`.`creature_template_npcbot_extras`)
--   AND `ScriptName` = 'wanderer';


-- ---------------------------------------------------------------------------
--  方案B：留【指定的几个】当固定据点，其余继续游荡
-- ---------------------------------------------------------------------------
--  把括号里的 entry 换成你想留在据点的那几个
--  （先跑第一部分看清单，挑好了再改这里）

-- UPDATE `world`.`creature`
-- SET `ScriptName` = ''
-- WHERE `id` IN (70001, 70002, 70003);


-- ---------------------------------------------------------------------------
--  方案C：反过来 —— 只让【指定的几个】游荡，其余全部固定
-- ---------------------------------------------------------------------------
--  第1步：先全部清空

-- UPDATE `world`.`creature`
-- SET `ScriptName` = ''
-- WHERE `id` IN (SELECT `entry` FROM `world`.`creature_template_npcbot_extras`);

--  第2步：再给指定的几个开游荡

-- UPDATE `world`.`creature`
-- SET `ScriptName` = 'wanderer'
-- WHERE `id` IN (70201, 70231, 70316);


-- ---------------------------------------------------------------------------
--  方案D：按职业分 —— 例如让治疗职业固定在据点当"驻站牧师"
-- ---------------------------------------------------------------------------
--  class: 5=牧师 2=圣骑士 7=萨满 11=德鲁伊

-- UPDATE `world`.`creature`
-- SET `ScriptName` = ''
-- WHERE `id` IN (SELECT `entry` FROM `world`.`creature_template_npcbot_extras`
--                WHERE `class` IN (5));


-- ============================================================================
--  第三部分：改完之后验证（改完再跑一次，看分层对不对）
-- ============================================================================

SELECT '=== 3. 改动后的分层（应与预期一致）===' AS `步骤`;

SELECT
    CASE
        WHEN c.`ScriptName` = 'wanderer' THEN '第2层-永久化游荡bot'
        ELSE '第1层-固定据点NPC'
    END AS `层级`,
    COUNT(*) AS `数量`,
    GROUP_CONCAT(c.`id` ORDER BY c.`id`) AS `entry列表`
FROM `world`.`creature` c
JOIN `world`.`creature_template_npcbot_extras` e ON e.`entry` = c.`id`
GROUP BY CASE
        WHEN c.`ScriptName` = 'wanderer' THEN '第2层-永久化游荡bot'
        ELSE '第1层-固定据点NPC'
    END;


-- ============================================================================
--  第四部分：跨表一致性检查（每个改数据的SQL都必须带，见坑表规则）
-- ============================================================================

SELECT '=== 4. 一致性检查：下面三个数字必须都是 0 ===' AS `步骤`;

SELECT
    '4a.有bot数据但creature表无实体' AS `检查项`,
    COUNT(*) AS `必须为0`
FROM `characters`.`characters_npcbot` cn
LEFT JOIN `world`.`creature` c ON c.`id` = cn.`entry`
WHERE c.`guid` IS NULL;

SELECT
    '4b.creature有实体但无bot数据' AS `检查项`,
    COUNT(*) AS `必须为0`
FROM `world`.`creature` c
JOIN `world`.`creature_template_npcbot_extras` e ON e.`entry` = c.`id`
LEFT JOIN `characters`.`characters_npcbot` cn ON cn.`entry` = c.`id`
WHERE cn.`entry` IS NULL;

SELECT
    '4c.组队成员无creature实体' AS `检查项`,
    COUNT(*) AS `必须为0`
FROM `characters`.`characters_npcbot_group_member` gm
LEFT JOIN `world`.`creature` c ON c.`id` = gm.`entry`
WHERE c.`guid` IS NULL;
