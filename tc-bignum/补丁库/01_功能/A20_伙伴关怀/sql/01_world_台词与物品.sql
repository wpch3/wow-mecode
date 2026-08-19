-- ============================================================================
--  step34  伙伴关怀系统 —— 【world 库】部分
--
--  本文件全部使用【完全限定名】 world.表名，
--  所以【不需要选库、不需要 USE】，直接 Alt+X 执行全部即可。
--
--  （上一版让用户手动分库执行，导致 "No database selected"。已修正。）
--
--  本文件建两张表：
--      world.npcbot_care_text     台词池
--      world.npcbot_care_item     可给予物品池
--
--  另一个文件 02_characters_背包.sql 建 characters 库的表，也要执行。
-- ============================================================================

-- ----------------------------------------------------------------------------
--  表1：关怀台词池
--
--  bot 主动关心玩家时说什么。按 care_type 分类，同类型有多条时按权重随机。
-- ----------------------------------------------------------------------------
DROP TABLE IF EXISTS `world`.`npcbot_care_text`;
CREATE TABLE `world`.`npcbot_care_text` (
  `id`          INT UNSIGNED     NOT NULL AUTO_INCREMENT COMMENT '自增主键',
  `care_type`   TINYINT UNSIGNED NOT NULL COMMENT '1=给食物 2=给水 3=给钱 4=闲聊 5=升级祝贺 6=复活关心',
  `bot_class`   TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '限定职业 0=通用',
  `text`        TEXT             NOT NULL COMMENT '台词。{item}替换物品名 {gold}替换金额 {from}替换来源',
  `emote`       INT UNSIGNED     NOT NULL DEFAULT 0 COMMENT '同时播放的表情ID 0=不播',
  `weight`      TINYINT UNSIGNED NOT NULL DEFAULT 10 COMMENT '随机权重，越大越常出现',
  `comment`     VARCHAR(255)              DEFAULT NULL COMMENT '备注',
  PRIMARY KEY (`id`),
  KEY `idx_type` (`care_type`, `bot_class`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='NPCBot 伙伴关怀台词池';

-- ----------------------------------------------------------------------------
--  表2：可给予物品池
--
--  【关键】不硬编码 itemid 到代码里。这张表定义"什么等级段给什么"，
--  服务端启动时会【校验 item_template 是否存在】，不存在的跳过并打日志。
-- ----------------------------------------------------------------------------
DROP TABLE IF EXISTS `world`.`npcbot_care_item`;
CREATE TABLE `world`.`npcbot_care_item` (
  `id`          INT UNSIGNED     NOT NULL AUTO_INCREMENT,
  `care_type`   TINYINT UNSIGNED NOT NULL COMMENT '1=食物 2=水',
  `item_id`     INT UNSIGNED     NOT NULL COMMENT 'item_template.entry',
  `min_level`   TINYINT UNSIGNED NOT NULL DEFAULT 1  COMMENT 'bot至少多少级才拿得出',
  `max_level`   TINYINT UNSIGNED NOT NULL DEFAULT 80 COMMENT '超过这个等级换更好的',
  `source_text` VARCHAR(100)              DEFAULT NULL COMMENT '来源描述，用于台词',
  PRIMARY KEY (`id`),
  KEY `idx_type_lvl` (`care_type`, `min_level`, `max_level`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='NPCBot 关怀物品池';


-- ============================================================================
--  初始台词
--  随时可以往里加，加完重启服务端即可，【不用重新编译】
-- ============================================================================

-- 1 = 给食物
INSERT INTO `world`.`npcbot_care_text` (`care_type`, `bot_class`, `text`, `weight`, `comment`) VALUES
(1, 0, '先垫垫肚子，你脸都白了。', 10, NULL),
(1, 0, '拿着，{item}。别硬撑着。', 10, '带物品名'),
(1, 0, '我包里还有{item}，你先吃。', 10, NULL),
(1, 0, '路上买的，正好派上用场。', 8,  NULL),
(1, 0, '吃点东西吧，接下来的路不好走。', 8, NULL);

-- 2 = 给水
INSERT INTO `world`.`npcbot_care_text` (`care_type`, `bot_class`, `text`, `weight`, `comment`) VALUES
(2, 0, '喝点水，别把自己耗干了。', 10, NULL),
(2, 0, '{item}，拿去。', 10, NULL),
(2, 0, '歇会儿，喝口水再走。', 8, NULL);

-- 3 = 给钱
INSERT INTO `world`.`npcbot_care_text` (`care_type`, `bot_class`, `text`, `weight`, `comment`) VALUES
(3, 0, '拿着，我路上捡的。', 10, NULL),
(3, 0, '我攒的，反正我也不花钱。', 10, NULL),
(3, 0, '看你钱袋子瘪了。这些先用着。', 10, NULL),
(3, 0, '别客气，咱们是一伙的。', 8, NULL);

-- 4 = 闲聊/见闻
INSERT INTO `world`.`npcbot_care_text` (`care_type`, `bot_class`, `text`, `weight`, `comment`) VALUES
(4, 0, '上次我在这附近见过一头白鹿，可惜没追上。', 10, NULL),
(4, 0, '你有没有觉得，最近的天色不太对劲？', 10, NULL),
(4, 0, '走了这么久，倒也不觉得累。', 8, NULL),
(4, 0, '跟着你，比一个人瞎逛强多了。', 10, NULL),
(4, 0, '我以前也想过一个人走遍艾泽拉斯。后来发现，一个人没意思。', 6, '稀有'),
(4, 0, '这条路我熟。前面山口，小心点。', 8, NULL);

-- 5 = 升级祝贺
INSERT INTO `world`.`npcbot_care_text` (`care_type`, `bot_class`, `text`, `weight`, `comment`) VALUES
(5, 0, '越来越像样了。', 10, NULL),
(5, 0, '不错嘛，又强了。', 10, NULL),
(5, 0, '照这个势头，迟早轮到你罩着我。', 8, NULL);

-- 6 = 复活后关心
INSERT INTO `world`.`npcbot_care_text` (`care_type`, `bot_class`, `text`, `weight`, `comment`) VALUES
(6, 0, '吓死我了，下次慢点。', 10, NULL),
(6, 0, '还好你没事。', 10, NULL),
(6, 0, '我该早点顶上去的。', 8, '带点自责');


-- ============================================================================
--  初始可给予物品
--
--  下面用 INSERT ... SELECT 的写法，【只插入你库里真实存在的物品】。
--  这样即使你的 item_template 和标准 3.3.5 不一样，也不会插进无效数据。
--
--  （服务端启动时还会再校验一次，双保险。）
-- ============================================================================

-- 食物（care_type = 1）
INSERT INTO `world`.`npcbot_care_item` (`care_type`, `item_id`, `min_level`, `max_level`, `source_text`)
SELECT 1, t.entry, v.minlvl, v.maxlvl, v.src
FROM (
    SELECT 4540  AS id,  1 AS minlvl, 15 AS maxlvl, '面包房'         AS src
    UNION ALL SELECT 4541, 10, 25, '路边小店'
    UNION ALL SELECT 4542, 20, 35, '旅店'
    UNION ALL SELECT 4544, 30, 45, '商队'
    UNION ALL SELECT 8950, 40, 60, '集市'
    UNION ALL SELECT 27855, 55, 80, '达拉然的面包铺'
) AS v
JOIN `world`.`item_template` AS t ON t.entry = v.id;

-- 水（care_type = 2）
INSERT INTO `world`.`npcbot_care_item` (`care_type`, `item_id`, `min_level`, `max_level`, `source_text`)
SELECT 2, t.entry, v.minlvl, v.maxlvl, v.src
FROM (
    SELECT 159   AS id,  1 AS minlvl, 15 AS maxlvl, '溪边' AS src
    UNION ALL SELECT 1179, 10, 25, '旅店'
    UNION ALL SELECT 1205, 20, 35, '商队'
    UNION ALL SELECT 1708, 30, 45, '集市'
    UNION ALL SELECT 8766, 40, 60, '酒馆'
    UNION ALL SELECT 28399, 55, 80, '达拉然'
) AS v
JOIN `world`.`item_template` AS t ON t.entry = v.id;


-- ============================================================================
--  执行完自查：看看实际插进去几条
--  （如果某个物品你库里没有，它就不会出现在结果里，这是正常的）
-- ============================================================================
SELECT
    CASE `care_type` WHEN 1 THEN '食物' WHEN 2 THEN '水' ELSE '其他' END AS `类型`,
    COUNT(*) AS `条数`
FROM `world`.`npcbot_care_item`
GROUP BY `care_type`;

SELECT COUNT(*) AS `台词总数` FROM `world`.`npcbot_care_text`;
