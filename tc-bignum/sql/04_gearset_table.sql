CREATE TABLE IF NOT EXISTS characters.custom_gearset (
  owner_guid int unsigned NOT NULL,
  set_name varchar(64) NOT NULL,
  slot tinyint unsigned NOT NULL,
  item_entry int unsigned NOT NULL,
  PRIMARY KEY (owner_guid, set_name, slot),
  KEY idx_owner (owner_guid)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
