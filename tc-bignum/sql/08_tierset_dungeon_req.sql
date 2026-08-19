-- ============================================================
--  套装系统 v3 · 建表 4/4
--  副本解锁门槛表（可在游戏内用 .gearset admin 修改，也可直接改这张表）
--  【本文件只有一条语句，把光标放在语句里执行】
-- ============================================================
CREATE TABLE IF NOT EXISTS world.custom_tierset_dungeon (
  map_id       INT UNSIGNED NOT NULL COMMENT '地图ID',
  difficulty   TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '难度',
  need_kills   INT UNSIGNED NOT NULL DEFAULT 10 COMMENT '解锁需要的通关次数',
  set_ids      VARCHAR(512) NOT NULL DEFAULT '' COMMENT '解锁的ItemSet ID列表,逗号分隔,空=按职业自动匹配',
  ilvl_min     INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '该本套装装等下限(自动匹配用)',
  ilvl_max     INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '该本套装装等上限(自动匹配用)',
  dungeon_name VARCHAR(64) NOT NULL DEFAULT '' COMMENT '副本名(仅显示用)',
  PRIMARY KEY (map_id, difficulty)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='套装系统-副本解锁门槛';
