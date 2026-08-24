-- ============================================================
--  套装系统 v3 · 建表 1/4
--  副本击杀进度表（记录每个角色刷了每个副本多少次）
--  【本文件只有一条语句，把光标放在语句里执行】
-- ============================================================
CREATE TABLE IF NOT EXISTS characters.custom_raid_progress (
  owner_guid   INT UNSIGNED NOT NULL COMMENT '角色GUID',
  map_id       INT UNSIGNED NOT NULL COMMENT '地图ID',
  difficulty   TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '难度 0普通 1英雄 等',
  kill_count   INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '通关次数(击杀末王计1)',
  last_kill    INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '上次通关时间戳',
  PRIMARY KEY (owner_guid, map_id, difficulty),
  KEY idx_owner (owner_guid)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='套装系统-副本通关进度';
