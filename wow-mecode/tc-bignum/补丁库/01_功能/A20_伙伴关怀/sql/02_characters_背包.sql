-- ============================================================================
--  step34  伙伴关怀系统 —— 【characters 库】部分
--
--  本文件使用【完全限定名】 characters.表名，
--  【不需要选库、不需要 USE】，直接 Alt+X 执行全部即可。
--
--  本文件建一张表：
--      characters.npcbot_inventory    bot 的虚拟背包
--
--  另一个文件 01_world_台词与物品.sql 建 world 库的表，也要执行。
-- ============================================================================

-- ----------------------------------------------------------------------------
--  bot 的虚拟背包
--
--  用户原话：「是会自己拿出背包里的东西给予伙伴的一个战友」
--
--  所以物品必须有【真实来源】，不能凭空生成：
--    - bot 给你东西时，从这张表扣
--    - 扣光了就真的给不出来
--    - acquired_from 让台词能说清楚"这东西哪来的"
--
--  放 characters 库的原因：这是每个 bot 实例的运行时数据，
--  和角色数据同生命周期，不是静态配置。
-- ----------------------------------------------------------------------------
DROP TABLE IF EXISTS `characters`.`npcbot_inventory`;
CREATE TABLE `characters`.`npcbot_inventory` (
  `bot_guid`      INT UNSIGNED NOT NULL COMMENT 'bot 的 creature guid（低位）',
  `item_id`       INT UNSIGNED NOT NULL COMMENT 'item_template.entry',
  `count`         INT UNSIGNED NOT NULL DEFAULT 1 COMMENT '数量',
  `acquired_from` VARCHAR(100)          DEFAULT NULL COMMENT '来源描述，用于台词',
  `acquired_time` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '获得时间戳',
  PRIMARY KEY (`bot_guid`, `item_id`),
  KEY `idx_bot` (`bot_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='NPCBot 虚拟背包（伙伴系统）';


-- ============================================================================
--  执行完自查
-- ============================================================================
SELECT
    TABLE_NAME   AS `表名`,
    TABLE_ROWS   AS `行数`,
    TABLE_COMMENT AS `说明`
FROM `information_schema`.`TABLES`
WHERE TABLE_SCHEMA = 'characters'
  AND TABLE_NAME = 'npcbot_inventory';
