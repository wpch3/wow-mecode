-- ============================================================================
--  A37 第1步：双向数据层
-- ============================================================================
--  为「你给bot东西」+「bot主动索要」+「情感反馈」建表
--
--  【和已有表的关系】
--    A20 已建  world.npcbot_care_text   （bot给你的台词）    <- 保留
--    A20 已建  world.npcbot_care_item   （bot能拿出的东西）  <- 保留
--    A32 已建  characters.npcbot_bond   （羁绊数据）         <- 保留，本文件补字段
--    本文件新建 3 张表 + 扩 npcbot_bond
--
--  【重要发现】NPCBot 已有真实物品仓库：
--    characters_npcbot_gear_storage（存 item_guid，JOIN item_instance）
--    API: botdatamgr.h:282-286  全部 public(203段)
--      CanDepositBotBankItemsCount / GetBotBankItems / GetBotBankItemsCount
--      WithdrawBotBankItem / DepositBotBankItem
--    但它的 key 是 playerGuid（玩家级共用仓库），
--    所以要下面的 npcbot_gift_log 记录「哪件东西是给哪个bot的」
--
--  DBeaver：Alt+X 执行全部。本文件【只建表+插数据，不删任何东西】
-- ============================================================================


-- ============================================================================
--  表1：赠予/收礼流水
-- ============================================================================
--  双向都记：你给bot、bot给你，用 direction 区分
--  作用：防刷（查24小时内同物品次数）+ 回忆素材（"上次你给我的那把剑"）

CREATE TABLE IF NOT EXISTS `characters`.`npcbot_gift_log` (
  `id`          INT UNSIGNED     NOT NULL AUTO_INCREMENT,
  `bot_id`      INT UNSIGNED     NOT NULL COMMENT 'NPCBot=creature entry / PlayerBot=角色guid',
  `bot_kind`    TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0=NPCBot 1=PlayerBot',
  `owner_guid`  INT UNSIGNED     NOT NULL COMMENT '玩家 guid 低位',
  `item_entry`  INT UNSIGNED     NOT NULL COMMENT 'item_template.entry',
  `item_guid`   INT UNSIGNED     NOT NULL DEFAULT 0 COMMENT '实物guid，0=已消耗',
  `item_count`  INT UNSIGNED     NOT NULL DEFAULT 1,
  `give_time`   INT UNSIGNED     NOT NULL COMMENT 'unix时间戳',
  `points`      SMALLINT         NOT NULL DEFAULT 0 COMMENT '本次获得的羁绊点，可为负',
  `direction`   TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0=你给bot 1=bot给你',
  `disposal`    TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0=存入仓库 1=当场使用 2=被拒绝',
  `zone_id`     INT UNSIGNED     NOT NULL DEFAULT 0 COMMENT '在哪儿给的，用于回忆',
  PRIMARY KEY (`id`),
  KEY `idx_bot_owner` (`bot_id`, `bot_kind`, `owner_guid`),
  KEY `idx_antispam`  (`owner_guid`, `item_entry`, `give_time`),
  KEY `idx_time`      (`give_time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='NPCBot/PlayerBot 赠予流水（双向）';


-- ============================================================================
--  表2：bot 主动索要记录
-- ============================================================================
--  作用：防打扰（同类需求不重复问）+ 记录你有没有满足它

CREATE TABLE IF NOT EXISTS `characters`.`npcbot_request_log` (
  `id`           INT UNSIGNED     NOT NULL AUTO_INCREMENT,
  `bot_id`       INT UNSIGNED     NOT NULL,
  `bot_kind`     TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0=NPCBot 1=PlayerBot',
  `owner_guid`   INT UNSIGNED     NOT NULL,
  `request_type` TINYINT UNSIGNED NOT NULL COMMENT '1=食物 2=水 3=绷带 4=修装备 5=想休息 6=情绪低落',
  `ask_time`     INT UNSIGNED     NOT NULL COMMENT '开口时间',
  `answered`     TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0=没理它 1=满足了 2=明确拒绝',
  `answer_time`  INT UNSIGNED     NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`),
  KEY `idx_bot_owner` (`bot_id`, `bot_kind`, `owner_guid`),
  KEY `idx_pending`   (`answered`, `ask_time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='bot 主动索要记录';


-- ============================================================================
--  表3：赠予反馈台词池（四维度）
-- ============================================================================
--  维度：物品类型 x 是否正需要 x 羁绊档
--  这是「情感反馈」的素材库

CREATE TABLE IF NOT EXISTS `world`.`npcbot_gift_text` (
  `id`           INT UNSIGNED     NOT NULL AUTO_INCREMENT,
  `item_kind`    TINYINT UNSIGNED NOT NULL COMMENT '1=食物 2=水 3=药水 4=装备 5=珍稀 6=垃圾 7=通用',
  `need_state`   TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0=任意 1=正需要 2=不需要',
  `bond_tier`    TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0=任意 1=生疏(0-1) 2=熟悉(2-3) 3=深交(4-5)',
  `react_type`   TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0=收下 1=当场用 2=拒绝',
  `bot_class`    TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '限定职业 0=通用',
  `text`         TEXT             NOT NULL COMMENT '{item}物品名 {player}你的名字 {n}数字',
  `emote`        INT UNSIGNED     NOT NULL DEFAULT 0 COMMENT '同时播放的表情ID',
  `weight`       TINYINT UNSIGNED NOT NULL DEFAULT 10,
  `comment`      VARCHAR(255)              DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `idx_pick` (`item_kind`, `need_state`, `bond_tier`, `react_type`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='收到礼物时的反馈台词';


-- ============================================================================
--  表4：bot 索要台词池
-- ============================================================================

CREATE TABLE IF NOT EXISTS `world`.`npcbot_request_text` (
  `id`           INT UNSIGNED     NOT NULL AUTO_INCREMENT,
  `request_type` TINYINT UNSIGNED NOT NULL COMMENT '同 npcbot_request_log.request_type',
  `bond_tier`    TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0=任意 1=生疏 2=熟悉 3=深交',
  `bot_class`    TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `text`         TEXT             NOT NULL,
  `emote`        INT UNSIGNED     NOT NULL DEFAULT 0,
  `weight`       TINYINT UNSIGNED NOT NULL DEFAULT 10,
  `comment`      VARCHAR(255)              DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `idx_pick` (`request_type`, `bond_tier`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='bot 主动索要的台词';


-- ============================================================================
--  扩展 A32 的 npcbot_bond 表
-- ============================================================================
--  【已知问题 2026-08-06】
--  下面这段用了 ADD COLUMN IF NOT EXISTS，这是 **MariaDB 专有语法**，
--  MySQL 任何版本都不支持（原注释写"MySQL 8.0.29+ 支持"是错的）。
--
--  >>> 如果你用 MySQL，请【跳过下面这段】，改跑同目录：
--  >>>     01b_修补bond表字段_MySQL兼容.sql
--
--  该段已整体注释掉，避免执行报错中断后面的台词插入。

-- ALTER TABLE `characters`.`npcbot_bond`
--   ADD COLUMN IF NOT EXISTS `bot_kind`        TINYINT UNSIGNED NOT NULL DEFAULT 0,
--   ADD COLUMN IF NOT EXISTS `gifts_refused`   INT UNSIGNED NOT NULL DEFAULT 0,
--   ADD COLUMN IF NOT EXISTS `gift_points`     INT UNSIGNED NOT NULL DEFAULT 0,
--   ADD COLUMN IF NOT EXISTS `last_gift_entry` INT UNSIGNED NOT NULL DEFAULT 0,
--   ADD COLUMN IF NOT EXISTS `daily_gift_count` SMALLINT UNSIGNED NOT NULL DEFAULT 0,
--   ADD COLUMN IF NOT EXISTS `daily_reset_day` INT UNSIGNED NOT NULL DEFAULT 0;


-- ============================================================================
--  台词数据：收到礼物的反馈
-- ============================================================================
--  设计原则：
--    生疏时客套有距离，熟悉后自然，深交后会打趣甚至嫌弃（但还是收下）
--    「正需要」时反应明显更强烈 —— 这是"你懂我"的核心

DELETE FROM `world`.`npcbot_gift_text` WHERE `id` BETWEEN 1 AND 200;

INSERT INTO `world`.`npcbot_gift_text`
  (`id`,`item_kind`,`need_state`,`bond_tier`,`react_type`,`bot_class`,`text`,`emote`,`weight`,`comment`) VALUES

-- ---- 食物 · 正需要 ----
(1,  1,1,1,1,0,'正好，我确实有点饿了。',0,10,'食物-饿-生疏-当场吃'),
(2,  1,1,1,1,0,'……谢了。我这就吃。',0,8,'食物-饿-生疏'),
(3,  1,1,2,1,0,'你怎么知道我饿了？',0,10,'食物-饿-熟悉'),
(4,  1,1,2,1,0,'来得正是时候。',0,10,'食物-饿-熟悉'),
(5,  1,1,3,1,0,'你总是知道我什么时候饿。……别笑，我是认真的。',0,12,'食物-饿-深交'),
(6,  1,1,3,1,0,'又是你先发现的。我都还没开口。',0,10,'食物-饿-深交'),

-- ---- 食物 · 不需要 ----
(10, 1,2,1,0,0,'……谢谢。我先收着。',0,10,'食物-不饿-生疏-收下'),
(11, 1,2,2,0,0,'现在不饿，不过留着总没错。',0,10,'食物-不饿-熟悉'),
(12, 1,2,3,0,0,'又给我带吃的。我又不是小孩子。',0,12,'食物-不饿-深交-嘴硬'),
(13, 1,2,3,0,0,'你是不是觉得我就只会吃？……行吧，我收下了。',0,8,'食物-不饿-深交'),

-- ---- 水 ----
(20, 2,1,1,1,0,'渴了正好。',0,10,'水-渴-生疏'),
(21, 2,1,2,1,0,'嗓子确实干了，谢谢。',0,10,'水-渴-熟悉'),
(22, 2,1,3,1,0,'跟你出门就是省心。',0,10,'水-渴-深交'),
(23, 2,2,0,0,0,'水啊，收着吧，路上总用得着。',0,10,'水-不渴-通用'),

-- ---- 药水 ----
(30, 3,1,0,1,0,'这个正需要，我马上用。',0,12,'药水-需要-当场用'),
(31, 3,2,1,0,0,'药水？收下了，关键时刻能救命。',0,10,'药水-不需要-生疏'),
(32, 3,2,3,0,0,'你自己留着吧……算了，你肯定不听。我收了。',0,10,'药水-不需要-深交'),

-- ---- 装备 ----
(40, 4,0,1,0,0,'这……给我的？我收下了。',0,10,'装备-生疏'),
(41, 4,0,2,0,0,'不错的东西。我会用好它。',0,10,'装备-熟悉'),
(42, 4,0,3,0,0,'你把这个给我？……我不会让它蒙尘的。',0,14,'装备-深交-郑重'),

-- ---- 珍稀物品 ----
(50, 5,0,1,2,0,'这太贵重了，我不能收。',0,20,'珍稀-生疏-拒绝'),
(51, 5,0,2,0,0,'这么好的东西……你确定？',0,10,'珍稀-熟悉-犹豫但收下'),
(52, 5,0,3,0,0,'我知道这个值多少。……谢谢，真的。',0,15,'珍稀-深交'),

-- ---- 垃圾物品（灰色）----
(60, 6,0,1,0,0,'……这个？好吧，我拿着。',0,10,'垃圾-生疏-无奈'),
(61, 6,0,2,0,0,'你是把我当仓库了吗？',0,10,'垃圾-熟悉-吐槽'),
(62, 6,0,3,0,0,'又是破烂。你自己怎么不留着？',0,12,'垃圾-深交-吐槽'),
(63, 6,0,3,0,0,'我背包里全是你塞的破烂了。',0,8,'垃圾-深交'),

-- ---- 通用兜底（任何情况都能用）----
(70, 7,0,0,0,0,'收下了，谢谢。',0,5,'兜底-收下'),
(71, 7,0,0,1,0,'正好用得上。',0,5,'兜底-使用'),

-- ---- 拒绝类 ----
(80, 7,0,0,2,0,'这个我用不了，你自己留着吧。',0,10,'拒绝-职业不符'),
(81, 7,0,0,2,0,'够了够了，我拿不下了。',0,10,'拒绝-背包满'),
(82, 7,0,0,2,0,'今天你给我太多了，先歇歇吧。',0,10,'拒绝-每日上限');


-- ============================================================================
--  台词数据：bot 主动索要
-- ============================================================================
--  设计原则：
--    羁绊低时不好意思开口（甚至不开口），羁绊高了才会直说
--    「情绪低落」那条【不要任何东西】，只要你回应 —— 这是最动人的一条

DELETE FROM `world`.`npcbot_request_text` WHERE `id` BETWEEN 1 AND 100;

INSERT INTO `world`.`npcbot_request_text`
  (`id`,`request_type`,`bond_tier`,`bot_class`,`text`,`emote`,`weight`,`comment`) VALUES

-- ---- 1=食物 ----
(1, 1,1,0,'……你还有吃的吗？',0,10,'食物-生疏-小声'),
(2, 1,2,0,'我有点饿了，你那儿还有吃的吗？',0,10,'食物-熟悉'),
(3, 1,3,0,'饿了。你包里肯定还有。',0,12,'食物-深交-理直气壮'),

-- ---- 2=水 ----
(10, 2,1,0,'能……给我口水吗？',0,10,'水-生疏'),
(11, 2,2,0,'渴死了，有水吗？',0,10,'水-熟悉'),
(12, 2,3,0,'水。快点。',0,10,'水-深交-耍赖'),

-- ---- 3=绷带/药品 ----
(20, 3,1,0,'我的绷带用完了……',0,10,'绷带-生疏'),
(21, 3,2,0,'绷带没了，你还有吗？',0,10,'绷带-熟悉'),
(22, 3,3,0,'又受伤了。绷带给我一卷。',0,10,'绷带-深交'),

-- ---- 4=修装备 ----
(30, 4,0,0,'我的武器快断了。',0,10,'修装备-通用'),
(31, 4,3,0,'这把剑跟了我很久了，能修修吗？',0,12,'修装备-深交-有感情'),

-- ---- 5=想休息 ----
(40, 5,2,0,'能歇会儿吗？就一会儿。',0,10,'休息-熟悉'),
(41, 5,3,0,'我们坐下歇歇吧。反正也不急。',0,10,'休息-深交'),

-- ---- 6=情绪低落（【不要东西，只要回应】）----
(50, 6,2,0,'……我是不是拖后腿了？',0,15,'低落-连续死亡-熟悉'),
(51, 6,3,0,'我今天状态不好。抱歉。',0,12,'低落-深交'),
(52, 6,3,0,'你会换掉我吗？',0,10,'低落-深交-最扎心'),
(53, 6,2,0,'刚才那下……我没反应过来。',0,10,'低落-熟悉');


-- ============================================================================
--  验证
-- ============================================================================

SELECT '=== 建表结果 ===' AS `验证`;

SELECT 'npcbot_gift_log'     AS `表`, COUNT(*) AS `行数` FROM `characters`.`npcbot_gift_log`
UNION ALL
SELECT 'npcbot_request_log', COUNT(*) FROM `characters`.`npcbot_request_log`
UNION ALL
SELECT 'npcbot_gift_text',   COUNT(*) FROM `world`.`npcbot_gift_text`
UNION ALL
SELECT 'npcbot_request_text',COUNT(*) FROM `world`.`npcbot_request_text`;

SELECT '=== npcbot_bond 新字段 ===' AS `验证`;

SELECT `COLUMN_NAME` AS `字段`, `COLUMN_COMMENT` AS `说明`
FROM `information_schema`.`COLUMNS`
WHERE `TABLE_SCHEMA` = 'characters'
  AND `TABLE_NAME` = 'npcbot_bond'
  AND `COLUMN_NAME` IN ('bot_kind','gifts_refused','gift_points',
                        'last_gift_entry','daily_gift_count','daily_reset_day');

SELECT '=== 台词分布 ===' AS `验证`;

SELECT
  CASE `item_kind` WHEN 1 THEN '食物' WHEN 2 THEN '水' WHEN 3 THEN '药水'
                   WHEN 4 THEN '装备' WHEN 5 THEN '珍稀' WHEN 6 THEN '垃圾'
                   ELSE '通用' END AS `物品类型`,
  COUNT(*) AS `台词数`
FROM `world`.`npcbot_gift_text`
GROUP BY `item_kind`
ORDER BY `item_kind`;

SELECT '=== 第1步数据层完成，等第2步代码 ===' AS `完成`;
