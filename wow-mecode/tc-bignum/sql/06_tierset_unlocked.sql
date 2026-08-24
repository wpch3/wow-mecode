-- ============================================================
--  套装系统 v3 · 建表 2/4
--  已解锁套装表（记录哪些 ItemSet 已经解锁）
--  【本文件只有一条语句，把光标放在语句里执行】
-- ============================================================
CREATE TABLE IF NOT EXISTS characters.custom_tierset_unlocked (
  owner_guid   INT UNSIGNED NOT NULL COMMENT '角色GUID',
  set_id       INT UNSIGNED NOT NULL COMMENT 'ItemSet.dbc 的 ID',
  unlock_time  INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '解锁时间戳',
  claimed      TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0未领取 1已领取',
  PRIMARY KEY (owner_guid, set_id),
  KEY idx_owner (owner_guid)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='套装系统-已解锁套装';
