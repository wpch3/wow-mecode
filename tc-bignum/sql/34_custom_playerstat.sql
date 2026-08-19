/* GM属性持久化 建表 */
/* 库 characters  每个文件只有一条语句 */
CREATE TABLE IF NOT EXISTS characters.custom_playerstat (
  guid INT UNSIGNED NOT NULL COMMENT '角色GUID',
  statType TINYINT UNSIGNED NOT NULL COMMENT '0=UnitMod(.modify) 1=Rating(.set)',
  statIndex TINYINT UNSIGNED NOT NULL COMMENT '枚举下标',
  amount FLOAT NOT NULL COMMENT '修正值',
  PRIMARY KEY (guid, statType, statIndex),
  KEY idx_guid (guid)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COMMENT='GM属性修改-持久化';
