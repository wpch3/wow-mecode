-- ============================================================================
--  F21  修复启动崩溃：ASSERT_NOTNULL BotDataMgr::FindBot(creature_id)
-- ============================================================================
--  崩溃点：botdatamgr.cpp:1224  LoadNpcBotGroupData()
--      const_cast<Creature*>(ASSERT_NOTNULL(BotDataMgr::FindBot(creature_id)))
--          ->SetBotGroup(group, subgroup);
--
--  【和 F20 无关】这是数据库残留导致的，不是代码改动引起的。
--
--  【根因】characters_npcbot_group_member 表里有一条记录，它的 entry：
--      在 characters_npcbot 里【有】（所以躲过了上游的两道清理）
--      但那个 bot 【没能 spawn 成功】（creature 表里没有对应记录）
--      -> FindBot 在 _existingBots 里找不到 -> 返回 nullptr -> ASSERT 崩服
--
--  上游 botdatamgr.cpp:1197-1198 只清理了两种情况：
--      DELETE ... WHERE guid NOT IN (SELECT guid FROM `groups`)
--      DELETE ... WHERE entry NOT IN (SELECT entry FROM characters_npcbot)
--  漏了「有数据但没实体」这一种。
--
--  DBeaver：Alt+X 执行全部
-- ============================================================================


-- ============================================================================
--  第一部分：诊断（先看是哪条记录害的）
-- ============================================================================

SELECT '=== 1. group_member 表里的所有记录 ===' AS `诊断`;

SELECT
    gm.`guid`   AS `组ID`,
    gm.`entry`  AS `botEntry`,
    ct.`name`   AS `bot名字`,
    CASE WHEN cn.`entry` IS NULL THEN '否' ELSE '是' END AS `在npcbot表`,
    CASE WHEN c.`id`     IS NULL THEN '否' ELSE '是' END AS `在creature表`,
    CASE
        WHEN cn.`entry` IS NULL              THEN '会被上游自动清理'
        WHEN c.`id`     IS NULL              THEN '【就是它害的】有数据无实体'
        ELSE '正常'
    END AS `判定`
FROM `characters`.`characters_npcbot_group_member` gm
LEFT JOIN `characters`.`characters_npcbot` cn ON cn.`entry` = gm.`entry`
LEFT JOIN `world`.`creature` c ON c.`id` = gm.`entry`
LEFT JOIN `world`.`creature_template` ct ON ct.`entry` = gm.`entry`
ORDER BY gm.`guid`, gm.`entry`;


SELECT '=== 2. 统计 ===' AS `诊断`;

SELECT
    COUNT(*)                                                          AS `总记录数`,
    SUM(CASE WHEN c.`id` IS NULL THEN 1 ELSE 0 END)                   AS `会导致崩服的`
FROM `characters`.`characters_npcbot_group_member` gm
LEFT JOIN `world`.`creature` c ON c.`id` = gm.`entry`;


-- ============================================================================
--  第二部分：修复（会改数据）
-- ============================================================================
--  【最简单也最安全】直接清空整张表
--
--  这张表存的是「bot 属于哪个队伍」的关系。
--  清空的后果：所有 bot 退出队伍，你重新招募/组队即可。
--  不会丢失 bot 本身，也不会丢装备。


-- ---------------------------------------------------------------------------
--  2.1 【推荐】清空整张表（一劳永逸）
-- ---------------------------------------------------------------------------

DELETE FROM `characters`.`characters_npcbot_group_member`;

SELECT ROW_COUNT() AS `清掉了几条`;


-- ---------------------------------------------------------------------------
--  2.2 精确清理（只删会崩服的那些，保留正常的组队关系）
-- ---------------------------------------------------------------------------
--  如果你不想丢失现有组队关系，用这条代替 2.1
--  【注意】用之前先把 2.1 那两行注释掉

-- DELETE FROM `characters`.`characters_npcbot_group_member`
-- WHERE `entry` NOT IN (SELECT `id` FROM `world`.`creature`);
--
-- SELECT ROW_COUNT() AS `清掉了几条`;


-- ============================================================================
--  第三部分：顺便清理其它不一致（防止下次再崩）
-- ============================================================================

SELECT '=== 3. characters_npcbot 里有数据但没实体的 ===' AS `诊断`;

SELECT
    cn.`entry`,
    ct.`name`  AS `名字`,
    cn.`owner` AS `主人`,
    '有数据无实体，会让group_member崩服' AS `风险`
FROM `characters`.`characters_npcbot` cn
LEFT JOIN `world`.`creature` c ON c.`id` = cn.`entry`
LEFT JOIN `world`.`creature_template` ct ON ct.`entry` = cn.`entry`
WHERE c.`id` IS NULL AND cn.`owner` = 0;

--  【说明】owner > 0 的是玩家招募的bot，它们【本来就】不需要 creature 记录
--  （跟着玩家走，不落地）。所以只看 owner = 0 的。


-- ---------------------------------------------------------------------------
--  3.1 清理这些孤儿（可选，取消注释执行）
-- ---------------------------------------------------------------------------
--  【谨慎】这会让那些 bot 回到游荡池

-- DELETE FROM `characters`.`characters_npcbot`
-- WHERE `owner` = 0
--   AND `entry` NOT IN (SELECT `id` FROM `world`.`creature`);
--
-- SELECT ROW_COUNT() AS `清掉了几条孤儿`;


-- ============================================================================
--  第四部分：验证（这两个必须都是 0）
-- ============================================================================

SELECT '=== 验证：都必须是 0 才能启动 ===' AS `验证`;

SELECT
    (SELECT COUNT(*)
       FROM `characters`.`characters_npcbot_group_member` gm
       LEFT JOIN `world`.`creature` c ON c.`id` = gm.`entry`
      WHERE c.`id` IS NULL)                                  AS `group_member孤儿`,
    (SELECT COUNT(*)
       FROM `world`.`creature` c
       LEFT JOIN `world`.`creature_template` ct ON ct.`entry` = c.`id`
      WHERE ct.`entry` IS NULL)                              AS `creature无模板记录`;

SELECT '=== 上面两个都是 0 就可以启动服务端了 ===' AS `完成`;
