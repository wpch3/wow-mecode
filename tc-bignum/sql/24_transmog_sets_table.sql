/* 幻化系统 建表 2/2 -- 外观方案（多套保存） */
/* 库 characters  每个文件只有一条语句 */
CREATE TABLE IF NOT EXISTS characters.custom_transmog_sets (
  guid INT UNSIGNED NOT NULL COMMENT '角色GUID',
  setName VARCHAR(32) NOT NULL COMMENT '方案名',
  slot TINYINT UNSIGNED NOT NULL COMMENT '装备槽位 0-18',
  fakeEntry INT UNSIGNED NOT NULL COMMENT '外观物品ID',
  PRIMARY KEY (guid, setName, slot),
  KEY idx_guid (guid)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COMMENT='幻化-外观方案';
