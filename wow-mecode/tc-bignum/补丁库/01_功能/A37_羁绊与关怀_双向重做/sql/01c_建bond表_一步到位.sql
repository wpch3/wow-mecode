-- ============================================================================
--  A37  建 npcbot_bond 表（含 A37 新增的 6 个字段）—— 一步到位
--
--  【为什么要这个文件】
--  报错 "Table 'characters.npcbot_bond' doesn't exist"
--  原因：A32 只出了 SQL 文件，你从没跑过，表根本不存在。
--  我却让你去 ALTER 一张不存在的表 —— 是我的错。
--
--  本文件 = A32 原始表结构 + A37 需要的 6 个新字段，直接建好，
--  不需要再跑 01b，也不需要跑 A32 的 01_羁绊数据层.sql 里的建表段。
--
--  【安全性】用 CREATE TABLE IF NOT EXISTS，
--  如果表已存在则什么都不做，不会删你的数据。
--
--  DBeaver: Alt+X 执行全部
--  MySQL / MariaDB 都能跑（没有用任何方言特性）
-- ============================================================================


-- ============================================================================
--  第一部分：执行前确认
-- ============================================================================

SELECT '=== 1. npcbot_bond 现在存在吗 ===' AS `步骤`;

SELECT
    CASE WHEN COUNT(*) > 0 THEN '已存在-下面的CREATE会跳过'
         ELSE '不存在-下面会创建' END AS `状态`,
    COUNT(*) AS `找到几张`
FROM `information_schema`.`TABLES`
WHERE `TABLE_SCHEMA` = 'characters'
  AND `TABLE_NAME`   = 'npcbot_bond';


-- ============================================================================
--  第二部分：建表（A32 原结构 + A37 新增 6 字段）
-- ============================================================================

CREATE TABLE IF NOT EXISTS `characters`.`npcbot_bond` (
  `bot_id`           INT UNSIGNED     NOT NULL COMMENT 'bot 的 creature entry',
  `owner_guid`       INT UNSIGNED     NOT NULL COMMENT '玩家 guid 低位',

  `bond_level`       TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '羁绊等级 0-5，由经历算出',
  `bond_points`      INT UNSIGNED     NOT NULL DEFAULT 0 COMMENT '累计点数',

  `time_together`    INT UNSIGNED     NOT NULL DEFAULT 0 COMMENT '一起在线秒数',
  `battles_won`      INT UNSIGNED     NOT NULL DEFAULT 0 COMMENT '一起打赢的战斗',
  `times_saved_me`   INT UNSIGNED     NOT NULL DEFAULT 0 COMMENT 'bot救过你几次',
  `times_i_saved`    INT UNSIGNED     NOT NULL DEFAULT 0 COMMENT '你救过bot几次',
  `deaths_together`  INT UNSIGNED     NOT NULL DEFAULT 0 COMMENT '一起团灭几次',

  `gifts_from_bot`   INT UNSIGNED     NOT NULL DEFAULT 0 COMMENT 'bot给过你多少东西',
  `gifts_to_bot`     INT UNSIGNED     NOT NULL DEFAULT 0 COMMENT '你给过bot多少东西',
  `requests_met`     INT UNSIGNED     NOT NULL DEFAULT 0 COMMENT '你满足过它几次请求',
  `requests_ignored` INT UNSIGNED     NOT NULL DEFAULT 0 COMMENT '你无视过它几次请求',

  `first_met_zone`   INT UNSIGNED     NOT NULL DEFAULT 0 COMMENT '初遇地点',
  `memorable_zone`   INT UNSIGNED     NOT NULL DEFAULT 0 COMMENT '最难忘的地方(团灭最多)',
  `first_met`        INT UNSIGNED     NOT NULL DEFAULT 0 COMMENT '初遇时间戳',
  `last_request`     INT UNSIGNED     NOT NULL DEFAULT 0 COMMENT '上次索要时间(防打扰)',
  `last_gift_time`   INT UNSIGNED     NOT NULL DEFAULT 0 COMMENT '上次收礼时间(防刷)',

  `bot_kind`         TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0=NPCBot 1=PlayerBot',
  `gifts_refused`    INT UNSIGNED     NOT NULL DEFAULT 0 COMMENT 'bot拒收过几次',
  `gift_points`      INT UNSIGNED     NOT NULL DEFAULT 0 COMMENT '赠予累计得分',
  `last_gift_entry`  INT UNSIGNED     NOT NULL DEFAULT 0 COMMENT '上次收到什么',
  `daily_gift_count` SMALLINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '今日已收次数',
  `daily_reset_day`  INT UNSIGNED     NOT NULL DEFAULT 0 COMMENT '上次重置的天数序号',

  PRIMARY KEY (`bot_id`, `owner_guid`),
  KEY `idx_owner` (`owner_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='NPCBot 羁绊数据（A32+A37）';


-- ============================================================================
--  第三部分：验证
-- ============================================================================

SELECT '=== 2. 表建好了吗（必须是1）===' AS `步骤`;

SELECT COUNT(*) AS `必须为1`
FROM `information_schema`.`TABLES`
WHERE `TABLE_SCHEMA` = 'characters'
  AND `TABLE_NAME`   = 'npcbot_bond';


SELECT '=== 3. A37 需要的6个字段（必须是6）===' AS `步骤`;

SELECT COUNT(*) AS `必须为6`
FROM `information_schema`.`COLUMNS`
WHERE `TABLE_SCHEMA` = 'characters'
  AND `TABLE_NAME`   = 'npcbot_bond'
  AND `COLUMN_NAME` IN
      ('bot_kind','gifts_refused','gift_points','last_gift_entry','daily_gift_count','daily_reset_day');


SELECT '=== 4. 完整字段列表（应该 24 个）===' AS `步骤`;

SELECT
    `ORDINAL_POSITION` AS `序号`,
    `COLUMN_NAME`      AS `字段名`,
    `COLUMN_TYPE`      AS `类型`,
    `COLUMN_COMMENT`   AS `说明`
FROM `information_schema`.`COLUMNS`
WHERE `TABLE_SCHEMA` = 'characters'
  AND `TABLE_NAME`   = 'npcbot_bond'
ORDER BY `ORDINAL_POSITION`;


-- ============================================================================
--  第四部分：如果表【本来就存在】但缺那6个字段
-- ============================================================================
--  上面第3项如果不是 6，说明表是旧的 A32 结构，
--  取消下面 6 条的注释单独跑（报 Error 1060 Duplicate column 说明已有，无视即可）

-- ALTER TABLE `characters`.`npcbot_bond` ADD COLUMN `bot_kind` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0=NPCBot 1=PlayerBot';
-- ALTER TABLE `characters`.`npcbot_bond` ADD COLUMN `gifts_refused` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'bot拒收过几次';
-- ALTER TABLE `characters`.`npcbot_bond` ADD COLUMN `gift_points` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '赠予累计得分';
-- ALTER TABLE `characters`.`npcbot_bond` ADD COLUMN `last_gift_entry` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '上次收到什么';
-- ALTER TABLE `characters`.`npcbot_bond` ADD COLUMN `daily_gift_count` SMALLINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '今日已收次数';
-- ALTER TABLE `characters`.`npcbot_bond` ADD COLUMN `daily_reset_day` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '上次重置的天数序号';


-- ============================================================================
--  第五部分：A37 第1步整体完成度
-- ============================================================================

SELECT '=== 5. A37 四张表 + bond 表（应该 5 行）===' AS `步骤`;

SELECT
    `TABLE_SCHEMA` AS `库`,
    `TABLE_NAME`   AS `表名`
FROM `information_schema`.`TABLES`
WHERE (`TABLE_SCHEMA` = 'characters'
       AND `TABLE_NAME` IN ('npcbot_gift_log','npcbot_request_log','npcbot_bond'))
   OR (`TABLE_SCHEMA` = 'world'
       AND `TABLE_NAME` IN ('npcbot_gift_text','npcbot_request_text'))
ORDER BY `TABLE_SCHEMA`, `TABLE_NAME`;


SELECT '=== 6. 台词入库情况 ===' AS `步骤`;

SELECT
    (SELECT COUNT(*) FROM `world`.`npcbot_gift_text`)    AS `礼物台词条数`,
    (SELECT COUNT(*) FROM `world`.`npcbot_request_text`) AS `索要台词条数`,
    (SELECT COUNT(*) FROM `world`.`npc_text`
       WHERE `ID` BETWEEN 70801 AND 70808)               AS `第2步菜单台词_应为8`;
