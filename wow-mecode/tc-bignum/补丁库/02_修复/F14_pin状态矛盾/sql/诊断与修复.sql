-- ============================================================================
--  step60  .pin 状态矛盾 —— 诊断与修复
-- ============================================================================
--  症状：
--    .pin      -> "这个bot已经有持久化数据了"
--    .pin off  -> "这个bot本来就不是永久的"
--    两个指令都进不去，bot 卡死
--
--  根因：永久化要【两张表同时写】，但中途断了，只写成一张。
--    characters.characters_npcbot   有记录
--    world.creature                 没记录
--
--  DBeaver 用法：Alt+X 执行全部
--  表名全部写全限定名（库.表），不需要 USE
-- ============================================================================


-- ============================================================================
--  第一部分：诊断（先看清楚，不改任何数据）
-- ============================================================================

-- ---------------------------------------------------------------------------
--  1.1 全库扫描：找出所有【状态损坏】的 bot
-- ---------------------------------------------------------------------------
--  这一条不用你填 entry，直接跑，把所有有问题的都列出来

SELECT '=== 损坏类型A：只有持久化数据，没有实体（就是你遇到的）===' AS `诊断`;

SELECT
    cn.`entry`                        AS `bot编号`,
    ct.`name`                         AS `bot名字`,
    '只有characters_npcbot'           AS `问题`,
    '安全，但.pin/.pin off都进不去'   AS `影响`
FROM `characters`.`characters_npcbot` cn
LEFT JOIN `world`.`creature` c ON c.`id` = cn.`entry`
LEFT JOIN `world`.`creature_template` ct ON ct.`entry` = cn.`entry`
WHERE c.`guid` IS NULL;


SELECT '=== 损坏类型B：只有实体，没有持久化数据（重启会崩服）===' AS `诊断`;

SELECT
    c.`id`                            AS `bot编号`,
    ct.`name`                         AS `bot名字`,
    c.`guid`                          AS `spawnId`,
    '只有world.creature'              AS `问题`,
    '危险！下次启动 ABORT_MSG 崩服'   AS `影响`
FROM `world`.`creature` c
INNER JOIN `world`.`creature_template_npcbot_extras` ne ON ne.`entry` = c.`id`
LEFT JOIN `characters`.`characters_npcbot` cn ON cn.`entry` = c.`id`
LEFT JOIN `world`.`creature_template` ct ON ct.`entry` = c.`id`
WHERE cn.`entry` IS NULL;


SELECT '=== 正常的已固定bot（两张表都有，这些不用动）===' AS `诊断`;

SELECT
    cn.`entry`      AS `bot编号`,
    ct.`name`       AS `bot名字`,
    c.`guid`        AS `spawnId`,
    c.`map`         AS `地图`,
    c.`ScriptName`  AS `谁固定的`
FROM `characters`.`characters_npcbot` cn
INNER JOIN `world`.`creature` c ON c.`id` = cn.`entry`
LEFT JOIN `world`.`creature_template` ct ON ct.`entry` = cn.`entry`;


-- ---------------------------------------------------------------------------
--  1.2 查单个 bot 的详细状态
-- ---------------------------------------------------------------------------
--  【把下面的 70001 改成你那个 bot 的实际 entry】
--  不知道 entry：游戏里选中它，用 .pin status（新指令）看，或 .npcbot info

SET @entry := 70001;

SELECT '=== 单个bot诊断 ===' AS `步骤`, @entry AS `查询的entry`;

SELECT
    @entry AS `entry`,
    (SELECT COUNT(*) FROM `characters`.`characters_npcbot` WHERE `entry` = @entry) AS `characters_npcbot有几条`,
    (SELECT COUNT(*) FROM `world`.`creature`               WHERE `id`    = @entry) AS `world_creature有几条`,
    (SELECT COUNT(*) FROM `world`.`creature_template_npcbot_extras`          WHERE `entry` = @entry) AS `npcbot_extras有几条`,
    (SELECT `name` FROM `world`.`creature_template` WHERE `entry` = @entry)        AS `名字`;

-- 判定表：
--   npcbot=1 creature=1  ->  正常，已固定
--   npcbot=0 creature=0  ->  正常，纯游荡bot
--   npcbot=1 creature=0  ->  损坏A（你的情况），用下面 2.1 修
--   npcbot=0 creature=1  ->  损坏B（会崩服），用下面 2.2 修


-- ============================================================================
--  第二部分：修复（会改数据，跑之前先看完第一部分）
-- ============================================================================
--  【建议】能用游戏内的 .pin fix 就用它，更安全。
--  下面的 SQL 是给"服务端起不来"或"要批量清"的情况用的。


-- ---------------------------------------------------------------------------
--  2.1 修单个 bot（损坏A：只有 characters_npcbot）
-- ---------------------------------------------------------------------------
--  取消下面的注释来执行（去掉每行开头的 -- ）

-- SET @entry := 70001;
--
-- DELETE FROM `characters`.`characters_npcbot`              WHERE `entry` = @entry;
-- DELETE FROM `characters`.`characters_npcbot_group_member` WHERE `entry` = @entry;
-- DELETE FROM `world`.`creature`                            WHERE `id`    = @entry;
--
-- SELECT '=== 修复后验证：下面三个都应该是 0 ===' AS `步骤`;
-- SELECT
--     (SELECT COUNT(*) FROM `characters`.`characters_npcbot` WHERE `entry` = @entry) AS `npcbot`,
--     (SELECT COUNT(*) FROM `world`.`creature`               WHERE `id`    = @entry) AS `creature`,
--     (SELECT COUNT(*) FROM `characters`.`characters_npcbot_group_member` WHERE `entry` = @entry) AS `group_member`;


-- ---------------------------------------------------------------------------
--  2.2 批量清理【所有】损坏A（谨慎，先跑 1.1 看清楚有哪些）
-- ---------------------------------------------------------------------------
--  取消注释来执行

-- DELETE FROM `characters`.`characters_npcbot`
-- WHERE `entry` NOT IN (SELECT `id` FROM `world`.`creature`);
--
-- SELECT ROW_COUNT() AS `清掉了几条孤儿记录`;


-- ---------------------------------------------------------------------------
--  2.3 批量清理【所有】损坏B（这种会崩服，优先处理）
-- ---------------------------------------------------------------------------
--  注意：只删 npcbot 的，不碰普通生物。
--  靠 INNER JOIN creature_template_npcbot_extras 来确保只删 bot。

-- DELETE FROM `world`.`creature`
-- WHERE `id` IN (SELECT `entry` FROM `world`.`creature_template_npcbot_extras`)
--   AND `id` NOT IN (SELECT `entry` FROM `characters`.`characters_npcbot`);
--
-- SELECT ROW_COUNT() AS `清掉了几条会崩服的记录`;


-- ============================================================================
--  第三部分：改完必须重启服务端
-- ============================================================================
--  characters_npcbot 的内容在内存里有一份缓存（BotDataMgr::_botsData），
--  botdatamgr.cpp 启动时加载。
--
--  【只删数据库不重启的话】内存里那份还在，
--  .pin 依然会说"已经有持久化数据了"。
--
--  所以：跑完 SQL -> 重启 worldserver -> 那个 bot 变回普通游荡bot
-- ============================================================================

SELECT '=== SQL执行完毕，请重启服务端让内存缓存同步 ===' AS `完成`;
