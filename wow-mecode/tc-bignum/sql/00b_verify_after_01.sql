-- =====================================================================
-- 执行 01 之后的确认脚本
-- 单结果集，一屏看完，不依赖客户端切换标签页
-- =====================================================================

SELECT
    CONCAT(TABLE_SCHEMA, '.', COLUMN_NAME)  AS '库.列名',
    COLUMN_TYPE                             AS '当前类型',
    CASE
        WHEN COLUMN_TYPE LIKE 'int%'      THEN '[OK] 已扩展'
        WHEN COLUMN_TYPE LIKE 'bigint%'   THEN '[OK] 已扩展'
        ELSE '[NG] 未扩展 - 需重跑对应脚本'
    END                                     AS '状态',
    CASE COLUMN_NAME
        WHEN 'stat_value1'   THEN '01脚本 - 核心属性值'
        WHEN 'stat_value10'  THEN '01脚本 - 核心属性值'
        WHEN 'armor'         THEN '01脚本 - 护甲'
        WHEN 'holy_res'      THEN '01脚本 - 抗性'
        WHEN 'arcane_res'    THEN '01脚本 - 抗性'
        WHEN 'ItemLevel'     THEN '01脚本 - 物品等级'
        WHEN 'MaxDurability' THEN '01脚本 - 最大耐久'
        WHEN 'durability'    THEN '02脚本 - 当前耐久'
        ELSE ''
    END                                     AS '来源'
FROM information_schema.COLUMNS
WHERE (TABLE_SCHEMA = 'world'
       AND TABLE_NAME = 'item_template'
       AND COLUMN_NAME IN ('stat_value1','stat_value10','armor',
                           'holy_res','arcane_res','ItemLevel','MaxDurability'))
   OR (TABLE_SCHEMA = 'characters'
       AND TABLE_NAME = 'item_instance'
       AND COLUMN_NAME = 'durability')
ORDER BY
    CASE WHEN COLUMN_TYPE LIKE 'int%' THEN 1 ELSE 0 END,  -- 未扩展的排最前面
    TABLE_SCHEMA, COLUMN_NAME;

-- =====================================================================
-- 判读标准
-- =====================================================================
-- 执行完 01 之后，world 库的 7 个列应该全部 [OK]：
--     stat_value1    int
--     stat_value10   int
--     armor          int unsigned
--     holy_res       int unsigned
--     arcane_res     int unsigned
--     ItemLevel      int unsigned      <- 你执行01前是 smallint，重点看这个
--     MaxDurability  int unsigned      <- 你执行01前是 smallint，重点看这个
--
-- characters.durability 此时仍是 [NG] 属正常，它由 02 脚本负责。
--
-- 如果 ItemLevel / MaxDurability 仍显示 smallint：
--     说明 01 脚本没有真正执行（可能只选中了部分语句执行）。
--     请全选整个 01 文件重新执行一次。
