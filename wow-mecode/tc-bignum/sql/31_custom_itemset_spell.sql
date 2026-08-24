/* 自定义套装 建表 2/2 -- 套装效果 */
/* 库 world  每个文件只有一条语句 */
CREATE TABLE IF NOT EXISTS world.custom_itemset_spell (
  setId INT UNSIGNED NOT NULL COMMENT '套装ID',
  idx TINYINT UNSIGNED NOT NULL COMMENT '效果槽 0-7',
  spellId INT UNSIGNED NOT NULL COMMENT '法术ID',
  threshold TINYINT UNSIGNED NOT NULL COMMENT '需要几件触发',
  PRIMARY KEY (setId, idx)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COMMENT='自定义套装效果';
