-- ============================================================
--  套装系统 v3 · 建表 3/4
--  角色开关配置表（职业套装开关等）
--  【本文件只有一条语句，把光标放在语句里执行】
-- ============================================================
CREATE TABLE IF NOT EXISTS characters.custom_gearset_config (
  owner_guid   INT UNSIGNED NOT NULL COMMENT '角色GUID',
  tier_enabled TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '职业套装开关 0关 1开',
  auto_equip   TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '默认自动穿戴 0否 1是',
  auto_gem     TINYINT UNSIGNED NOT NULL DEFAULT 1 COMMENT '自动附魔宝石 0否 1是',
  grant_req    TINYINT UNSIGNED NOT NULL DEFAULT 1 COMMENT '自动补足声望专业 0否 1是',
  PRIMARY KEY (owner_guid)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='套装系统-角色配置';
