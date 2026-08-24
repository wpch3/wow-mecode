/* 幻化系统 建表 1/2 -- 当前幻化 */
/* 库 characters  每个文件只有一条语句 */
CREATE TABLE IF NOT EXISTS characters.custom_transmog (
  guid INT UNSIGNED NOT NULL COMMENT '角色GUID',
  slot TINYINT UNSIGNED NOT NULL COMMENT '装备槽位 0-18',
  fakeEntry INT UNSIGNED NOT NULL COMMENT '外观物品ID',
  PRIMARY KEY (guid, slot),
  KEY idx_guid (guid)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COMMENT='幻化-当前外观';
