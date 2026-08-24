-- ============================================================================
--  A37 第1步 补丁：npcbot_bond 加 6 个字段（MySQL 兼容版）
--
--  【为什么要这个文件】
--  01_双向数据层.sql 里用了：
--      ALTER TABLE ... ADD COLUMN IF NOT EXISTS ...
--  这是 **MariaDB 专有语法，MySQL 任何版本都不支持**。
--  （原文档写"MySQL 8.0.29+ 支持"是错的，已更正）
--
--  【怎么用】
--  第一部分先跑，看第2项告诉你缺哪几个字段。
--  第二部分是 6 条独立 ALTER，一条一个字段：
--      DBeaver 里【逐条 Ctrl+Enter】，或直接 Alt+X 全部执行。
--      如果某个字段已经存在，那一条会报
--          Error 1060: Duplicate column name 'xxx'
--      **这个报错可以直接无视**，继续跑下一条即可，不影响其它字段。
--  第三部分验证，那个数字必须是 6。
-- ============================================================================


-- ============================================================================
--  第一部分：先看现状
-- ============================================================================

SELECT '=== 1. npcbot_bond 现有字段 ===' AS `步骤`;

SELECT
    `ORDINAL_POSITION` AS `序号`,
    `COLUMN_NAME`      AS `字段名`,
    `COLUMN_TYPE`      AS `类型`,
    `COLUMN_COMMENT`   AS `说明`
FROM `information_schema`.`COLUMNS`
WHERE `TABLE_SCHEMA` = 'characters'
  AND `TABLE_NAME`   = 'npcbot_bond'
ORDER BY `ORDINAL_POSITION`;


SELECT '=== 2. 这6个字段缺哪几个（缺的才要跑第二部分对应那条）===' AS `步骤`;

SELECT 'bot_kind' AS `字段`,
    CASE WHEN EXISTS (SELECT 1 FROM `information_schema`.`COLUMNS`
        WHERE `TABLE_SCHEMA`='characters' AND `TABLE_NAME`='npcbot_bond' AND `COLUMN_NAME`='bot_kind')
    THEN '已存在-跳过2.1' ELSE '缺失-要跑2.1' END AS `状态`
UNION ALL
SELECT 'gifts_refused',
    CASE WHEN EXISTS (SELECT 1 FROM `information_schema`.`COLUMNS`
        WHERE `TABLE_SCHEMA`='characters' AND `TABLE_NAME`='npcbot_bond' AND `COLUMN_NAME`='gifts_refused')
    THEN '已存在-跳过2.2' ELSE '缺失-要跑2.2' END
UNION ALL
SELECT 'gift_points',
    CASE WHEN EXISTS (SELECT 1 FROM `information_schema`.`COLUMNS`
        WHERE `TABLE_SCHEMA`='characters' AND `TABLE_NAME`='npcbot_bond' AND `COLUMN_NAME`='gift_points')
    THEN '已存在-跳过2.3' ELSE '缺失-要跑2.3' END
UNION ALL
SELECT 'last_gift_entry',
    CASE WHEN EXISTS (SELECT 1 FROM `information_schema`.`COLUMNS`
        WHERE `TABLE_SCHEMA`='characters' AND `TABLE_NAME`='npcbot_bond' AND `COLUMN_NAME`='last_gift_entry')
    THEN '已存在-跳过2.4' ELSE '缺失-要跑2.4' END
UNION ALL
SELECT 'daily_gift_count',
    CASE WHEN EXISTS (SELECT 1 FROM `information_schema`.`COLUMNS`
        WHERE `TABLE_SCHEMA`='characters' AND `TABLE_NAME`='npcbot_bond' AND `COLUMN_NAME`='daily_gift_count')
    THEN '已存在-跳过2.5' ELSE '缺失-要跑2.5' END
UNION ALL
SELECT 'daily_reset_day',
    CASE WHEN EXISTS (SELECT 1 FROM `information_schema`.`COLUMNS`
        WHERE `TABLE_SCHEMA`='characters' AND `TABLE_NAME`='npcbot_bond' AND `COLUMN_NAME`='daily_reset_day')
    THEN '已存在-跳过2.6' ELSE '缺失-要跑2.6' END;


-- ============================================================================
--  第二部分：6 条独立 ALTER
--  报 "Error 1060: Duplicate column name" = 该字段已存在，无视继续
-- ============================================================================

-- 2.1
ALTER TABLE `characters`.`npcbot_bond`
  ADD COLUMN `bot_kind` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0=NPCBot 1=PlayerBot';

-- 2.2
ALTER TABLE `characters`.`npcbot_bond`
  ADD COLUMN `gifts_refused` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'bot拒收过几次';

-- 2.3
ALTER TABLE `characters`.`npcbot_bond`
  ADD COLUMN `gift_points` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '赠予累计得分';

-- 2.4
ALTER TABLE `characters`.`npcbot_bond`
  ADD COLUMN `last_gift_entry` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '上次收到什么';

-- 2.5
ALTER TABLE `characters`.`npcbot_bond`
  ADD COLUMN `daily_gift_count` SMALLINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '今日已收次数';

-- 2.6
ALTER TABLE `characters`.`npcbot_bond`
  ADD COLUMN `daily_reset_day` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '上次重置的天数序号';


-- ============================================================================
--  第三部分：验证（这个数字必须是 6）
-- ============================================================================

SELECT '=== 3. 执行后：6个字段应该全部到位 ===' AS `步骤`;

SELECT
    COUNT(*) AS `必须为6`
FROM `information_schema`.`COLUMNS`
WHERE `TABLE_SCHEMA` = 'characters'
  AND `TABLE_NAME`   = 'npcbot_bond'
  AND `COLUMN_NAME` IN
      ('bot_kind','gifts_refused','gift_points','last_gift_entry','daily_gift_count','daily_reset_day');


SELECT '=== 4. 逐个确认 ===' AS `步骤`;

SELECT
    `COLUMN_NAME`    AS `字段名`,
    `COLUMN_TYPE`    AS `类型`,
    `COLUMN_DEFAULT` AS `默认值`,
    `COLUMN_COMMENT` AS `说明`
FROM `information_schema`.`COLUMNS`
WHERE `TABLE_SCHEMA` = 'characters'
  AND `TABLE_NAME`   = 'npcbot_bond'
  AND `COLUMN_NAME` IN
      ('bot_kind','gifts_refused','gift_points','last_gift_entry','daily_gift_count','daily_reset_day')
ORDER BY `ORDINAL_POSITION`;


-- ============================================================================
--  第四部分：A37 第1步整体完成度检查
-- ============================================================================

SELECT '=== 5. A37 四张表（应该 4 行）===' AS `步骤`;

SELECT
    `TABLE_SCHEMA` AS `库`,
    `TABLE_NAME`   AS `表名`
FROM `information_schema`.`TABLES`
WHERE (`TABLE_SCHEMA` = 'characters' AND `TABLE_NAME` IN ('npcbot_gift_log','npcbot_request_log'))
   OR (`TABLE_SCHEMA` = 'world'      AND `TABLE_NAME` IN ('npcbot_gift_text','npcbot_request_text'))
ORDER BY `TABLE_SCHEMA`, `TABLE_NAME`;


SELECT '=== 6. 台词是否入库 ===' AS `步骤`;

SELECT
    (SELECT COUNT(*) FROM `world`.`npcbot_gift_text`)    AS `礼物台词条数`,
    (SELECT COUNT(*) FROM `world`.`npcbot_request_text`) AS `索要台词条数`;
