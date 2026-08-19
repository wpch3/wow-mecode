-- ============================================================================
-- G20 · 时间线试点：银月城渴魔症危机（零客户端改动）
-- ============================================================================
--  2026-08-09
--  用户想法：「有些时间线部落没有来入侵，但是拥有渴魔症的精灵
--             吸取生物的魔力或者自主吸取邪能」
--
--  【为什么零客户端】
--    用 phaseMask 实现。3.3.5 原生机制，客户端只是"看不到别的相位的对象"，
--    不需要任何 MPQ/DBC 补丁。
--    实证：creature 和 gameobject 表都有 phaseMask 字段
--         （WorldDatabase.cpp:86 / :89）
--
--  【相位分配】
--    phase 1   正常时间线（默认，所有人）
--    phase 2   渴魔症时间线
--    注意：phaseMask 是位掩码，1|2=3 表示两个相位都能看到
--
--  【entry 段】980001-980099 供本试点使用
--  【guid 段】用 MAX(guid)+1 自动避让，不写死
-- ============================================================================

USE `world`;

-- ----------------------------------------------------------------------------
-- 一、清理（幂等，可重复执行）
-- ----------------------------------------------------------------------------
DELETE FROM `world`.`creature` WHERE `id` BETWEEN 980001 AND 980099;
DELETE FROM `world`.`creature_template` WHERE `entry` BETWEEN 980001 AND 980099;
DELETE FROM `world`.`gossip_menu_option` WHERE `MenuID` BETWEEN 98001 AND 98010;
DELETE FROM `world`.`gossip_menu` WHERE `MenuID` BETWEEN 98001 AND 98010;
DELETE FROM `world`.`npc_text` WHERE `ID` BETWEEN 98001 AND 98010;


-- ----------------------------------------------------------------------------
-- 二、时间线守望者（入口NPC，站在银月城，正常相位可见）
-- ----------------------------------------------------------------------------
INSERT INTO `world`.`creature_template`
(`entry`, `modelid1`, `name`, `subname`, `gossip_menu_id`, `minlevel`, `maxlevel`,
 `faction`, `npcflag`, `speed_walk`, `speed_run`, `unit_class`, `unit_flags`,
 `type`, `type_flags`, `RegenHealth`, `MovementType`, `ScriptName`)
VALUES
(980001, 20286, '时间线守望者', '龙眠神殿', 98001, 80, 80,
 35, 1, 1.0, 1.14286, 1, 768,
 7, 0, 1, 0, '');


-- ----------------------------------------------------------------------------
-- 三、渴魔症相位的NPC（phase 2 才能看到）
-- ----------------------------------------------------------------------------
INSERT INTO `world`.`creature_template`
(`entry`, `modelid1`, `name`, `subname`, `gossip_menu_id`, `minlevel`, `maxlevel`,
 `faction`, `npcflag`, `speed_walk`, `speed_run`, `unit_class`, `unit_flags`,
 `type`, `type_flags`, `RegenHealth`, `MovementType`, `ScriptName`)
VALUES
-- 魔瘾失控的血精灵（敌对）
(980010, 18927, '魔瘾狂乱者', '失控的血精灵', 0, 78, 80,
 14, 0, 1.0, 1.14286, 1, 0,
 7, 0, 1, 1, ''),
(980011, 18928, '魔力吸取者', '渴魔症患者', 0, 79, 80,
 14, 0, 1.0, 1.14286, 1, 0,
 7, 0, 1, 1, ''),
-- 幸存的清醒者（中立，讲述剧情）
(980020, 20288, '清醒的法师', '还没有堕落', 98002, 80, 80,
 35, 1, 1.0, 1.14286, 1, 768,
 7, 0, 1, 0, '');


-- ----------------------------------------------------------------------------
-- 四、对话文本
-- ----------------------------------------------------------------------------
INSERT INTO `world`.`npc_text` (`ID`, `text0_0`, `BroadcastTextID0`, `Probability0`) VALUES
(98001,
 '时间的织物在这里有一道裂痕。\n\n世界之魂受创之后，某些"本不该发生"的事情，在别的地方发生了。\n\n我可以让你看一眼——但记住，你看到的是真实存在的另一种可能。',
 0, 1),
(98002,
 '你不是这条线上的人，我能看出来。\n\n在你的那条线上，兽人来了，对吧？我们失去了太阳井，但也……被迫清醒了。\n\n而在这里，没有入侵。我们保住了太阳井，然后慢慢地，所有人都疯了。\n\n魔瘾没有敌人来打断它。它只是一直长，一直长。',
 0, 1);


-- ----------------------------------------------------------------------------
-- 五、Gossip 菜单
-- ----------------------------------------------------------------------------
INSERT INTO `world`.`gossip_menu` (`MenuID`, `TextID`) VALUES
(98001, 98001),
(98002, 98002);

INSERT INTO `world`.`gossip_menu_option`
(`MenuID`, `OptionID`, `OptionIcon`, `OptionText`, `OptionBroadcastTextID`,
 `OptionType`, `OptionNpcFlag`, `ActionMenuID`, `ActionPoiID`, `BoxCoded`, `BoxMoney`, `BoxText`, `BoxBroadcastTextID`)
VALUES
(98001, 0, 0, '让我看看那条时间线。', 0, 1, 1, 0, 0, 0, 0, '', 0),
(98001, 1, 0, '带我回到原本的时间线。', 0, 1, 1, 0, 0, 0, 0, '', 0),
(98002, 0, 0, '这里发生了什么？', 0, 1, 1, 0, 0, 0, 0, '', 0);


-- ============================================================================
-- 六、放置 NPC（坐标：银月城 / 永歌森林）
-- ============================================================================
--  map 530 = 外域地图（银月城和幽暗森林在这张图上，这是暴雪的历史遗留）
--  时间线守望者放在银月城内，正常相位(1)可见
-- ============================================================================

-- 【零会话变量版】坑表规则：不用 SET @x，因为 DBeaver 的 Ctrl+Enter
--   只执行光标处那一条，SET 跑不到就会静默失效（返回0行且不报错）。
--   这里改用子查询取 guid 基数，每条 INSERT 都能【单独执行】。

-- 时间线守望者（正常相位，phaseMask=1）
INSERT INTO `world`.`creature`
(`guid`, `id`, `map`, `spawnMask`, `phaseMask`, `position_x`, `position_y`, `position_z`, `orientation`,
 `spawntimesecs`, `wander_distance`, `MovementType`)
SELECT IFNULL(MAX(`guid`), 0) + 1, 980001, 530, 1, 1, 9738.0, -7454.0, 13.5, 3.14, 300, 0, 0
FROM `world`.`creature`;

-- 清醒的法师（渴魔症相位，phaseMask=2）
INSERT INTO `world`.`creature`
(`guid`, `id`, `map`, `spawnMask`, `phaseMask`, `position_x`, `position_y`, `position_z`, `orientation`,
 `spawntimesecs`, `wander_distance`, `MovementType`)
SELECT IFNULL(MAX(`guid`), 0) + 1, 980020, 530, 1, 2, 9742.0, -7450.0, 13.5, 1.57, 300, 0, 0
FROM `world`.`creature`;

-- 魔瘾狂乱者 x2
INSERT INTO `world`.`creature`
(`guid`, `id`, `map`, `spawnMask`, `phaseMask`, `position_x`, `position_y`, `position_z`, `orientation`,
 `spawntimesecs`, `wander_distance`, `MovementType`)
SELECT IFNULL(MAX(`guid`), 0) + 1, 980010, 530, 1, 2, 9760.0, -7470.0, 13.5, 0.0, 120, 10, 1
FROM `world`.`creature`;

INSERT INTO `world`.`creature`
(`guid`, `id`, `map`, `spawnMask`, `phaseMask`, `position_x`, `position_y`, `position_z`, `orientation`,
 `spawntimesecs`, `wander_distance`, `MovementType`)
SELECT IFNULL(MAX(`guid`), 0) + 1, 980010, 530, 1, 2, 9775.0, -7485.0, 13.5, 0.0, 120, 10, 1
FROM `world`.`creature`;

-- 魔力吸取者
INSERT INTO `world`.`creature`
(`guid`, `id`, `map`, `spawnMask`, `phaseMask`, `position_x`, `position_y`, `position_z`, `orientation`,
 `spawntimesecs`, `wander_distance`, `MovementType`)
SELECT IFNULL(MAX(`guid`), 0) + 1, 980011, 530, 1, 2, 9790.0, -7460.0, 13.5, 0.0, 120, 10, 1
FROM `world`.`creature`;


-- ============================================================================
-- 七、验证
-- ============================================================================
SELECT `entry`, `name`, `subname` FROM `world`.`creature_template`
WHERE `entry` BETWEEN 980001 AND 980099
ORDER BY `entry`;
-- 应该 4 行

SELECT `phaseMask`, COUNT(*) AS `数量` FROM `world`.`creature`
WHERE `id` BETWEEN 980001 AND 980099
GROUP BY `phaseMask`;
-- phaseMask=1 一个（守望者），phaseMask=2 四个


-- ============================================================================
-- 八、怎么用（相位切换需要一条 GM 命令或小脚本）
-- ============================================================================
--  执行完这个 SQL 后，先用 GM 命令手动测试：
--
--    .go xyz 9738 -7454 13.5 530     传送到银月城守望者旁边
--    .modify phase 2                 切到渴魔症时间线
--    .modify phase 1                 切回正常
--
--  确认能看到不同的 NPC 之后，再做 gossip 自动切相位的脚本
--  （那需要一小段 C++ 或 Eluna，下一步交付）
--
--  【坐标说明】上面的坐标是银月城大致位置，
--  如果 NPC 卡在墙里，用 .go creature 找到它再 .npc move 调整。
-- ============================================================================
