-- ============================================================================
--  bot 批量永久化 —— 第1步：先试 10 个（验证机制）
-- ============================================================================
--  【原理】实证 botdatamgr.cpp:316-326
--    for (auto const& [id, extras] : _botsExtras)          // 遍历 npcbot_extras
--        if (职业启用 && !_botsData.contains(id))          // 【不在 characters_npcbot】
--            _spareBotIdsPerClassMap.at(c).insert(id);     // 才进游荡池
--
--  所以：往 characters_npcbot 写一条 -> 该 entry 自动退出游荡池 -> 变永久
--  这正是 .npcbot spawn 干的事（botcommands.cpp:3860）
--
--  【硬约束】botdatamgr.cpp:1174 启动交叉校验
--    creature 表有 + characters_npcbot 没有 -> ABORT_MSG 崩服
--    所以【先写 characters_npcbot，再写 creature】，顺序不能反
--
--  DBeaver：Ctrl+Enter 逐条执行 / Alt+X 执行全部
-- ============================================================================
--
--  【怎么用这个文件】
--    第一部分  纯查询，Alt+X 直接跑，不改任何数据
--    第二部分  改数据。**默认是注释掉的**，要手动取消注释才会执行
--              （这是防误操作，不是漏写）
--    第三部分  执行后的验证，必须看
--
--  取消注释的方法：选中那几行，DBeaver 按 Ctrl+/ 切换注释
--                  或手动删掉每行开头的 "-- "
-- ============================================================================


-- ============================================================================
--  第一部分：先看清楚现状（只查不改，直接跑）
-- ============================================================================

SELECT '=== 1. 你有多少 bot 模板可用 ===' AS `步骤`;

SELECT
    COUNT(*)                                     AS `模板总数`,
    SUM(CASE WHEN `class` = 0 THEN 1 ELSE 0 END) AS `职业为0的_无效`,
    MIN(`entry`)                                 AS `最小entry`,
    MAX(`entry`)                                 AS `最大entry`
FROM `world`.`creature_template_npcbot_extras`;


SELECT '=== 2. 已经是永久的有多少 ===' AS `步骤`;

SELECT
    COUNT(*)                                     AS `已永久`,
    SUM(CASE WHEN `owner` = 0 THEN 1 ELSE 0 END) AS `无主的`,
    SUM(CASE WHEN `owner` > 0 THEN 1 ELSE 0 END) AS `被玩家招募的`
FROM `characters`.`characters_npcbot`;


SELECT '=== 3. 当前游荡池大小（有extras但不在characters_npcbot）===' AS `步骤`;

SELECT COUNT(*) AS `游荡池大小`
FROM `world`.`creature_template_npcbot_extras` e
LEFT JOIN `characters`.`characters_npcbot` cn ON cn.`entry` = e.`entry`
WHERE cn.`entry` IS NULL AND e.`class` <> 0;


SELECT '=== 4. 可用路点（固定位置从这里挑）===' AS `步骤`;

SELECT
    COUNT(*)                AS `路点总数`,
    COUNT(DISTINCT `mapid`) AS `覆盖几张地图`,
    SUM(CASE WHEN `mapid` IN (0,1) THEN 1 ELSE 0 END) AS `东部王国和卡利姆多`
FROM `world`.`creature_template_npcbot_wander_nodes`;


SELECT '=== 5. 预览：即将固定这10个（先确认名单）===' AS `步骤`;

SELECT
    e.`entry`,
    ct.`name`  AS `名字`,
    e.`class`  AS `职业ID`,
    e.`race`   AS `种族ID`,
    CASE e.`class`
        WHEN 1 THEN '战士'   WHEN 2 THEN '圣骑士' WHEN 3 THEN '猎人'
        WHEN 4 THEN '潜行者' WHEN 5 THEN '牧师'   WHEN 6 THEN '死亡骑士'
        WHEN 7 THEN '萨满'   WHEN 8 THEN '法师'   WHEN 9 THEN '术士'
        WHEN 11 THEN '德鲁伊' ELSE CONCAT('扩展职业', e.`class`)
    END AS `职业`
FROM `world`.`creature_template_npcbot_extras` e
LEFT JOIN `world`.`creature_template` ct ON ct.`entry` = e.`entry`
LEFT JOIN `characters`.`characters_npcbot` cn ON cn.`entry` = e.`entry`
WHERE cn.`entry` IS NULL
  AND e.`class` <> 0
ORDER BY e.`entry`
LIMIT 10;


-- ============================================================================
--  第二部分：执行固定（会改数据）
-- ============================================================================
--  【默认全部注释掉，这是防误操作】
--  确认第一部分的数字合理后，把 2.1 和 2.2 的 "-- " 去掉再跑
--
--  执行顺序不能反：先 2.1（characters_npcbot）再 2.2（creature）
--  反了会触发 botdatamgr.cpp:1174 的 ABORT_MSG 崩服


-- ---------------------------------------------------------------------------
--  2.1 写 characters_npcbot —— 让这10个bot退出游荡池
-- ---------------------------------------------------------------------------
--  roles / spec / faction 的算法全部实证自代码：
--    spec    botdatamgr.cpp:3726 SelectSpecForClass（取每职业第一个）
--    roles   botdatamgr.cpp:3586 DefaultRolesForClass
--            = BOT_ROLE_DPS(0x04)，非近战再 |BOT_ROLE_RANGED(0x10) = 0x14 = 20
--            近战职业清单 botcommon.h:249 = 战1 骑2 贼4 DK6
--    faction botdatamgr.cpp:3522 取 ChrRaces.dbc 的 FactionID（种族决定阵营）

-- INSERT INTO `characters`.`characters_npcbot`
--     (`entry`, `owner`, `roles`, `spec`, `faction`)
-- SELECT
--     e.`entry`,
--     0,
--     CASE WHEN e.`class` IN (1,2,4,6) THEN 4 ELSE 20 END,
--     CASE e.`class`
--         WHEN 1  THEN 1    WHEN 2  THEN 6    WHEN 3  THEN 7
--         WHEN 4  THEN 10   WHEN 5  THEN 13   WHEN 6  THEN 16
--         WHEN 7  THEN 19   WHEN 8  THEN 23   WHEN 9  THEN 25
--         WHEN 11 THEN 28   ELSE 31
--     END,
--     CASE e.`race`
--         WHEN 1  THEN 1    WHEN 2  THEN 2    WHEN 3  THEN 3
--         WHEN 4  THEN 4    WHEN 5  THEN 5    WHEN 6  THEN 6
--         WHEN 7  THEN 115  WHEN 8  THEN 116  WHEN 10 THEN 1610
--         WHEN 11 THEN 1629 ELSE 35
--     END
-- FROM `world`.`creature_template_npcbot_extras` e
-- LEFT JOIN `characters`.`characters_npcbot` cn ON cn.`entry` = e.`entry`
-- WHERE cn.`entry` IS NULL
--   AND e.`class` <> 0
-- ORDER BY e.`entry`
-- LIMIT 10;
--
-- SELECT ROW_COUNT() AS `写入characters_npcbot几条_应该是10`;


-- ---------------------------------------------------------------------------
--  2.2 写 world.creature —— 给它们一个站立位置
-- ---------------------------------------------------------------------------
--  位置从 wander_nodes 挑：那些点本来就是给bot走的，不会卡地形/掉水里
--  字段清单实证自 WorldDatabase.cpp:86 WORLD_INS_CREATURE
--
--  【2026-08-05 已重写】旧版用 (@i := @i+1) 生成行号，有4个缺陷：
--    1) 变量不重置，跑第二次 rowno 会接着上次累加 -> JOIN 不上 -> 插入0条
--    2) MySQL 8.0 起 SELECT 里的变量赋值已弃用，且【不保证求值顺序】
--    3) 派生表里的 ORDER BY 会被优化器忽略（8.0.21+ 明确行为）
--    4) 两个子查询 LIMIT 不对称，多出来的行会静默丢失
--  现改用 ROW_NUMBER() 窗口函数（MySQL 8.0+），无副作用、顺序有保证。

-- SET @guid_base := (SELECT COALESCE(MAX(`guid`), 0) + 1000 FROM `world`.`creature`);
--
-- INSERT INTO `world`.`creature`
--     (`guid`, `id`, `map`, `spawnMask`, `phaseMask`, `modelid`, `equipment_id`,
--      `position_x`, `position_y`, `position_z`, `orientation`,
--      `spawntimesecs`, `wander_distance`, `currentwaypoint`,
--      `curhealth`, `curmana`, `MovementType`, `npcflag`, `unit_flags`, `dynamicflags`)
-- SELECT
--     @guid_base + t.rn,
--     t.entry,
--     w.mapid,
--     1,              -- spawnMask
--     1,              -- phaseMask
--     0,              -- modelid=0 用模板的
--     0,              -- equipment_id
--     w.x, w.y, w.z, w.o,
--     300,            -- spawntimesecs 复活时间
--     0,              -- wander_distance=0 原地不动（想让它溜达改成 5-10）
--     0,              -- currentwaypoint
--     1, 1,           -- curhealth/curmana=1 让核心自己算
--     0,              -- MovementType=0 IDLE
--     0, 0, 0         -- npcflag / unit_flags / dynamicflags
-- FROM (
--     SELECT cn.`entry` AS entry,
--            ROW_NUMBER() OVER (ORDER BY cn.`entry`) AS rn
--     FROM `characters`.`characters_npcbot` cn
--     JOIN `world`.`creature_template_npcbot_extras` e ON e.`entry` = cn.`entry`
--     LEFT JOIN `world`.`creature` c ON c.`id` = cn.`entry`
--     WHERE cn.`owner` = 0 AND c.`guid` IS NULL
-- ) t
-- JOIN (
--     SELECT `mapid` AS mapid, `x` AS x, `y` AS y, `z` AS z, `o` AS o,
--            ROW_NUMBER() OVER (ORDER BY `id`) AS rn
--     FROM `world`.`creature_template_npcbot_wander_nodes`
--     WHERE `mapid` IN (0, 1)
-- ) w ON w.rn = t.rn;
--
-- SELECT ROW_COUNT() AS `写入creature几条_应该和上面一致`;


-- ============================================================================
--  第三部分：执行后验证（必看，两个必须为0）
-- ============================================================================

SELECT '=== 验证1: 两张表数量应该一致 ===' AS `验证`;

SELECT
    (SELECT COUNT(*) FROM `characters`.`characters_npcbot` WHERE `owner` = 0) AS `npcbot表无主数`,
    (SELECT COUNT(*) FROM `world`.`creature` c
       JOIN `world`.`creature_template_npcbot_extras` e ON e.`entry` = c.`id`) AS `creature表bot数`;


SELECT '=== 验证2【必须为0】否则启动 ABORT_MSG 崩服 ===' AS `验证`;

SELECT COUNT(*) AS `creature有但npcbot表没有_危险`
FROM `world`.`creature` c
JOIN `world`.`creature_template_npcbot_extras` e ON e.`entry` = c.`id`
LEFT JOIN `characters`.`characters_npcbot` cn ON cn.`entry` = c.`id`
WHERE cn.`entry` IS NULL;


SELECT '=== 验证3【必须为0】creature表不能有无模板记录 ===' AS `验证`;

SELECT COUNT(*) AS `无模板记录_危险`
FROM `world`.`creature` c
LEFT JOIN `world`.`creature_template` ct ON ct.`entry` = c.`id`
WHERE ct.`entry` IS NULL;


SELECT '=== 验证4: 游荡池还剩多少（要同步调低配置）===' AS `验证`;

SELECT COUNT(*) AS `游荡池剩余`
FROM `world`.`creature_template_npcbot_extras` e
LEFT JOIN `characters`.`characters_npcbot` cn ON cn.`entry` = e.`entry`
WHERE cn.`entry` IS NULL AND e.`class` <> 0;

--  如果 worldserver.conf 里 Npcbot.WanderingBots.Continents.Count
--  超过上面这个数字，启动会 ASSERT 崩服（botdatamgr.cpp:1887）
--  -> 固定了几个，就把那个配置调低几个


SELECT '=== 验证2和验证3都必须是 0 才能启动服务端 ===' AS `重要`;


-- ============================================================================
--  第四部分：回滚（出问题了用这个撤销）
-- ============================================================================
--  注意顺序和写入相反：先删 creature，再删 characters_npcbot

-- DELETE FROM `world`.`creature`
-- WHERE `guid` >= @guid_base
--   AND `id` IN (SELECT `entry` FROM `world`.`creature_template_npcbot_extras`);
--
-- SELECT ROW_COUNT() AS `删除creature几条`;
--
-- DELETE FROM `characters`.`characters_npcbot`
-- WHERE `owner` = 0
--   AND `entry` NOT IN (SELECT `id` FROM `world`.`creature`);
--
-- SELECT ROW_COUNT() AS `删除characters_npcbot几条`;
--
--  【注意】上面第二条会删掉【所有】无主且没有creature记录的条目。
--  如果你之前手动招募过bot又解雇了，可能误删。
--  保险做法：把 @guid_base 记下来，用 entry 精确指定要删哪些。
