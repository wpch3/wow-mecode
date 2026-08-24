-- ============================================================
--  每日登录奖励 · 建表
--  配合 lua_scripts/03_daily_reward.lua
--  【本文件只有一条语句，把光标放在语句里执行】
--  执行在哪个库都行（已写完全限定名）
-- ============================================================
CREATE TABLE IF NOT EXISTS characters.custom_daily_reward (
  guid       INT UNSIGNED NOT NULL COMMENT '角色GUID',
  last_date  VARCHAR(8) NOT NULL DEFAULT '' COMMENT '上次领取日期 YYYYMMDD',
  streak     INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '连续登录天数',
  total_days INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '累计登录天数',
  PRIMARY KEY (guid)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='每日登录奖励记录';
