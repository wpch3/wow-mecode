/* 自定义套装 建表 1/2 -- 套装名 */
/* 库 world  每个文件只有一条语句 */
CREATE TABLE IF NOT EXISTS world.custom_itemset (
  setId INT UNSIGNED NOT NULL COMMENT '套装ID 2000起',
  name VARCHAR(64) NOT NULL COMMENT '套装名',
  PRIMARY KEY (setId)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COMMENT='自定义套装-待打DBC补丁';
