-- ============================================================
--  传送点中文名 · 建表
--  【本文件只有一条语句，把光标放在语句里执行】
--
--  说明：game_tele 表只有英文名，且官方没有本地化表。
--        这张表做中英映射，传送系统会优先显示中文。
-- ============================================================
CREATE TABLE IF NOT EXISTS world.custom_tele_cn (
  tele_id  INT UNSIGNED NOT NULL COMMENT '对应 game_tele.id',
  name_cn  VARCHAR(100) NOT NULL COMMENT '中文名',
  category VARCHAR(32) NOT NULL DEFAULT '' COMMENT '分类：主城/大陆/副本/团本',
  sort     INT NOT NULL DEFAULT 0 COMMENT '排序，越小越靠前',
  PRIMARY KEY (tele_id),
  KEY idx_cn (name_cn),
  KEY idx_cat (category)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='传送点中文名映射';
