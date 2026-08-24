-- ============================================================================
--  step50  羁绊系统 —— 第1步：数据层
-- ============================================================================
--
--  用户需求：「羁绊系统和关怀系统让玩家有更多参与感，
--            比如我也能给他食物或者各种东西，他也会索要，
--            还有就是有更多的互动选项和体验，按照最高规格来做」
--
--  设计文档：规划-羁绊系统-最高规格设计.md
--
--  【核心理念】好感度不是进度条，是【共同经历】。
--  bot 记住和你一起做过什么，然后在合适的时候提起。
--
--  本文件建 3 张表：
--      characters.npcbot_bond        羁绊数据（每个bot对每个玩家独立）
--      world.npcbot_bond_text        情境化台词池（11种situation x 6级）
--      world.npcbot_bond_request     索要台词池（5种需求情境）
--
--  全部完全限定名，直接 Alt+X 执行全部，不用选库。
--  【本步纯SQL，不用编译】先把数据备好，第2步的代码才有东西可读。
-- ============================================================================


-- ############################################################################
--  表1：羁绊数据（characters 库）
-- ############################################################################

DROP TABLE IF EXISTS `characters`.`npcbot_bond`;
CREATE TABLE `characters`.`npcbot_bond` (
  `bot_id`           INT UNSIGNED     NOT NULL COMMENT 'bot 的 creature entry',
  `owner_guid`       INT UNSIGNED     NOT NULL COMMENT '玩家 guid 低位',

  `bond_level`       TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '羁绊等级 0-5，由经历算出',
  `bond_points`      INT UNSIGNED     NOT NULL DEFAULT 0 COMMENT '累计点数',

  -- === 共同经历（这才是灵魂，台词会引用这些真实数字）===
  `time_together`    INT UNSIGNED     NOT NULL DEFAULT 0 COMMENT '一起在线秒数',
  `battles_won`      INT UNSIGNED     NOT NULL DEFAULT 0 COMMENT '一起打赢的战斗',
  `times_saved_me`   INT UNSIGNED     NOT NULL DEFAULT 0 COMMENT 'bot救过你几次',
  `times_i_saved`    INT UNSIGNED     NOT NULL DEFAULT 0 COMMENT '你救过bot几次',
  `deaths_together`  INT UNSIGNED     NOT NULL DEFAULT 0 COMMENT '一起团灭几次',

  -- === 双向礼物（用户要的参与感核心）===
  `gifts_from_bot`   INT UNSIGNED     NOT NULL DEFAULT 0 COMMENT 'bot给过你多少东西',
  `gifts_to_bot`     INT UNSIGNED     NOT NULL DEFAULT 0 COMMENT '你给过bot多少东西',
  `requests_met`     INT UNSIGNED     NOT NULL DEFAULT 0 COMMENT '你满足过它几次请求',
  `requests_ignored` INT UNSIGNED     NOT NULL DEFAULT 0 COMMENT '你无视过它几次请求',

  -- === 情境记忆（用于"还记得吗"）===
  `first_met_zone`   INT UNSIGNED     NOT NULL DEFAULT 0 COMMENT '初遇地点',
  `memorable_zone`   INT UNSIGNED     NOT NULL DEFAULT 0 COMMENT '最难忘的地方(团灭最多)',
  `first_met`        INT UNSIGNED     NOT NULL DEFAULT 0 COMMENT '初遇时间戳',
  `last_request`     INT UNSIGNED     NOT NULL DEFAULT 0 COMMENT '上次索要时间(防打扰)',
  `last_gift_time`   INT UNSIGNED     NOT NULL DEFAULT 0 COMMENT '上次收礼时间(防刷)',

  PRIMARY KEY (`bot_id`, `owner_guid`),
  KEY `idx_owner` (`owner_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='NPCBot 羁绊数据';


-- ############################################################################
--  表2：情境化台词池（world 库）
-- ############################################################################
--
--  【为什么不做随机台词池】随机抽一句说出来，玩家两天就腻。
--  我们的台词带【触发情境】，只在合适的时候说。
--
--  situation 取值：
--    after_hard_win    刚打赢一场险仗（残血获胜）
--    after_wipe        刚一起团灭
--    player_low_hp     你快死了
--    bot_saved_you     它刚救了你
--    you_saved_bot     你刚救了它
--    idle_night        夜晚闲着
--    enter_zone        进入新地图
--    revisit_zone      回到"难忘的地方"
--    gift_received     收到你的礼物
--    long_time_no_see  很久没上线
--    level_up          你升级了

DROP TABLE IF EXISTS `world`.`npcbot_bond_text`;
CREATE TABLE `world`.`npcbot_bond_text` (
  `id`         INT UNSIGNED     NOT NULL AUTO_INCREMENT,
  `min_bond`   TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '需要几级羁绊才会说',
  `situation`  VARCHAR(32)      NOT NULL COMMENT '触发情境',
  `text`       VARCHAR(512)     NOT NULL COMMENT '台词。{n}系占位符见文档',
  `weight`     TINYINT UNSIGNED NOT NULL DEFAULT 10 COMMENT '随机权重',
  `comment`    VARCHAR(255)              DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `idx_lookup` (`situation`, `min_bond`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='NPCBot 羁绊情境台词';

INSERT INTO `world`.`npcbot_bond_text` (`min_bond`, `situation`, `text`, `weight`, `comment`) VALUES
(0, 'after_hard_win', '险胜。', 10, '0级只说必要的话'),
(0, 'after_wipe', '重整旗鼓。', 10, NULL),
(1, 'after_hard_win', '这一场打得真险……我手都在抖。', 10, '1级开始有情绪'),
(1, 'after_hard_win', '差一点就交代在这儿了。', 10, NULL),
(1, 'after_wipe', '抱歉，我没能撑住。', 10, NULL),
(1, 'idle_night', '夜里总是特别安静。', 8, NULL),
(1, 'enter_zone', '这地方我没来过。', 8, NULL),
(2, 'after_hard_win', '我们配合得越来越好了。', 10, '2级开始有"我们"'),
(2, 'after_wipe', '别自责，这次是我冲太前了。', 10, NULL),
(2, 'idle_night', '你也睡不着吗？', 8, NULL),
(2, 'player_low_hp', '撑住！我来了！', 12, NULL),
(2, 'bot_saved_you', '别谢我，换你也会这么做。', 10, NULL),
(2, 'you_saved_bot', '……谢谢。我记下了。', 10, NULL),
(3, 'after_hard_win', '这种时候我总会想，还好身边是你。', 10, '3级开始交心'),
(3, 'after_wipe', '我们又一起躺下了。习惯了。', 10, NULL),
(3, 'revisit_zone', '又回到这里了……上次我们在这儿吃了大亏。', 12, '引用共同经历'),
(3, 'idle_night', '有时候我在想，如果没遇到你，我现在会在哪。', 8, NULL),
(3, 'player_low_hp', '别死在我前面！', 12, NULL),
(3, 'gift_received', '你总是这样……什么都想着我。', 10, NULL),
(3, 'level_up', '你又变强了。我得加把劲才跟得上。', 10, NULL),
(4, 'after_hard_win', '只要你还站着，这仗就不算输。', 10, '4级是战友'),
(4, 'after_wipe', '……又倒下了。但你还在，那就还没输。起来，再来一次。', 12, NULL),
(4, 'player_low_hp', '躲我后面！', 15, NULL),
(4, 'bot_saved_you', '说过多少次了，别冲那么前。', 10, NULL),
(4, 'you_saved_bot', '你不该为我冒这个险的。……但谢谢。', 10, NULL),
(4, 'idle_night', '我以前是一个人。现在不是了。', 8, NULL),
(4, 'long_time_no_see', '你去哪了？我一直在这儿等着。', 12, NULL),
(5, 'after_hard_win', '我们又活下来了。真好。', 10, '5级生死之交'),
(5, 'after_wipe', '死了这么多次，我都快习惯了。……但每次看你倒下，还是会慌。', 12, NULL),
(5, 'player_low_hp', '不准死！听见没有！', 15, NULL),
(5, 'you_saved_bot', '这条命是你捡回来的。以后它就是你的了。', 12, NULL),
(5, 'idle_night', '说句可能有点傻的话——遇见你，是我这辈子最好的运气。', 6, NULL),
(5, 'long_time_no_see', '你终于回来了。……我还以为你不要我了。', 12, NULL),
(5, 'revisit_zone', '这地方承载了我们太多回忆。好的坏的都有。', 10, NULL);


-- ############################################################################
--  表3：索要台词池（world 库）—— 【本系统的灵魂】
-- ############################################################################
--
--  一个只会给予、从不索取的伙伴是"仆人"；
--  【会向你求助的才是"战友"】。
--
--  触发条件（都要满足）：
--    1. 羁绊 >= min_bond
--    2. 距上次索要 > 30 分钟（不能烦人）
--    3. 有【真实需求】（不是随机说话）
--
--  want_type：
--    0 = 不要任何物品，只要你回应一句   <- 最打动人的一类
--    1 = 食物   2 = 饮料   3 = 绷带   4 = 装备/修理

DROP TABLE IF EXISTS `world`.`npcbot_bond_request`;
CREATE TABLE `world`.`npcbot_bond_request` (
  `id`         INT UNSIGNED     NOT NULL AUTO_INCREMENT,
  `min_bond`   TINYINT UNSIGNED NOT NULL DEFAULT 2 COMMENT '2级才解锁索要',
  `situation`  VARCHAR(32)      NOT NULL COMMENT '需求情境',
  `text`       VARCHAR(512)     NOT NULL,
  `want_type`  TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0=只要回应 1食物 2饮料 3绷带 4装备',
  `weight`     TINYINT UNSIGNED NOT NULL DEFAULT 10,
  `comment`    VARCHAR(255)              DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `idx_lookup` (`situation`, `min_bond`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='NPCBot 主动索要台词';

INSERT INTO `world`.`npcbot_bond_request` (`min_bond`, `situation`, `text`, `want_type`, `weight`, `comment`) VALUES
(2, 'need_food', '我的干粮吃完了……你那儿还有吗？', 1, 10, '食物'),
(2, 'need_drink', '能给我点水吗？我快没蓝了。', 2, 10, '饮料'),
(3, 'need_bandage', '绷带用光了，下次可能就扛不住了。', 3, 10, '绷带'),
(3, 'need_repair', '我的武器快断了，有多余的吗？', 4, 10, '装备'),
(3, 'need_comfort', '……我是不是拖后腿了。', 0, 10, '不要物品，只要回应'),
(4, 'need_comfort', '说实话，刚才那下我真以为要交代了。', 0, 10, '不要物品'),
(5, 'need_comfort', '有时候我会想，我这样的存在，算是活着吗？', 0, 10, '不要物品');


-- ############################################################################
--  验证
-- ############################################################################

SELECT COUNT(*) AS '情境台词条数' FROM `world`.`npcbot_bond_text`;
SELECT COUNT(*) AS '索要台词条数' FROM `world`.`npcbot_bond_request`;

-- 看看各羁绊等级分别有多少台词
SELECT `min_bond` AS '羁绊等级', COUNT(*) AS '台词数'
FROM `world`.`npcbot_bond_text` GROUP BY `min_bond` ORDER BY `min_bond`;

-- 看看覆盖了哪些情境
SELECT `situation` AS '情境', COUNT(*) AS '台词数'
FROM `world`.`npcbot_bond_text` GROUP BY `situation` ORDER BY COUNT(*) DESC;

SELECT '建表完成。第2步的代码会读这三张表。' AS '完成';
