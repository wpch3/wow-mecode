-- =====================================================================
-- 执行前自检 v3 —— 只查询，不修改任何数据
--
-- v3 改动：放弃 UNION ALL 长链（在 MySQL 8.0 下报 1064 语法错误），
--          改用 4 条彼此独立的简单 SELECT。
--          每条都是最基础的语法，MySQL / MariaDB 全版本通用。
--
-- [!] 会产生 4 个结果集。HeidiSQL / Navicat 默认只显示最后一个，
--    请在结果区切换标签页（通常在下方，标着「结果1/结果2...」）逐个查看。
--    命令行用户会依次全部打印。
-- =====================================================================


-- =====================================================================
-- 【结果集 1】列类型 —— 最重要，改造前应全是 smallint / tinyint
-- =====================================================================
SELECT
    TABLE_SCHEMA AS '库',
    COLUMN_NAME  AS '列名',
    COLUMN_TYPE  AS '当前类型'
FROM information_schema.COLUMNS
WHERE (TABLE_SCHEMA = 'world'
       AND TABLE_NAME = 'item_template'
       AND COLUMN_NAME IN ('stat_value1','stat_value10','armor',
                           'holy_res','arcane_res','ItemLevel','MaxDurability'))
   OR (TABLE_SCHEMA = 'characters'
       AND TABLE_NAME = 'item_instance'
       AND COLUMN_NAME = 'durability')
ORDER BY TABLE_SCHEMA, COLUMN_NAME;

-- 改造【前】期望（就是这些限制）：
--   stat_value1    smallint             +/-32767   <- 核心限制
--   stat_value10   smallint             +/-32767
--   armor          smallint unsigned    0~65535
--   holy_res       tinyint unsigned     0~255    <- 抗性最容易漏
--   arcane_res     tinyint unsigned     0~255
--   ItemLevel      smallint unsigned    0~65535
--   MaxDurability  smallint unsigned    0~65535
--   durability     smallint unsigned    0~65535
--
-- 改造【后】期望：全部变成 int 或 int unsigned


-- =====================================================================
-- 【结果集 2】表规模 —— 决定 ALTER 要跑多久
-- =====================================================================
SELECT
    TABLE_ROWS                                        AS '大致行数',
    CONCAT(ROUND((DATA_LENGTH+INDEX_LENGTH)/1024/1024, 1), ' MB') AS '表大小'
FROM information_schema.TABLES
WHERE TABLE_SCHEMA = 'world' AND TABLE_NAME = 'item_template';
-- 一般 5万~10万行 / 50~150MB，ALTER 约 10~60 秒


-- =====================================================================
-- 【结果集 3】现有数据最大值 —— 应该都在旧上限内
-- =====================================================================
SELECT
    MAX(stat_value1)   AS 'stat1最大_应<=32767',
    MAX(armor)         AS 'armor最大_应<=65535',
    MAX(holy_res)      AS '神圣抗性最大_应<=255',
    MAX(ItemLevel)     AS '物品等级最大',
    MAX(MaxDurability) AS '耐久最大'
FROM world.item_template;


-- =====================================================================
-- 【结果集 4】冲突检查 —— 测试物品要用的两个 entry
-- =====================================================================
SELECT
    (SELECT COUNT(*) FROM world.item_template WHERE entry = 900001) AS '900001已存在_应为0',
    (SELECT COUNT(*) FROM world.item_template WHERE entry = 46017)  AS '46017模板存在_应为1';

-- 900001 = 0  -> 可以安全创建测试物品
-- 46017  = 1  -> 可以用它当模板
--    若 46017 = 0，说明你库里没这件装备，
--    需把 03_test_item.sql 里的 46017 改成你库里存在的任意武器 entry。
--    查一个可用的： SELECT entry,name FROM world.item_template
--                   WHERE class=2 AND Quality>=4 LIMIT 5;
