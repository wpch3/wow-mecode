-- G23-P2 每日奖励原子领取闸
-- 目标库：characters
-- 纯标准DDL；不使用自定义分隔符、过程、函数或游标。
-- 可重复执行。

CREATE TABLE IF NOT EXISTS characters.custom_daily_reward (
  guid       INT UNSIGNED NOT NULL COMMENT '角色GUID',
  last_date  VARCHAR(8) NOT NULL DEFAULT '' COMMENT '上次成功领取日期 YYYYMMDD',
  streak     INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '连续成功领取天数',
  total_days INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '累计成功领取天数',
  PRIMARY KEY (guid)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='每日登录奖励汇总';

CREATE TABLE IF NOT EXISTS characters.custom_daily_reward_claim (
  guid        INT UNSIGNED NOT NULL COMMENT '角色GUID',
  claim_date  DATE NOT NULL COMMENT '领取自然日（数据库服务器日期）',
  token       CHAR(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL COMMENT '数据库UUID生成的state所有权令牌',
  status      ENUM('pending','granted') NOT NULL DEFAULT 'pending' COMMENT 'pending防重保护/granted已完成',
  streak      INT UNSIGNED NOT NULL COMMENT '本次连续天数快照',
  total_days  INT UNSIGNED NOT NULL COMMENT '本次累计天数快照',
  created_at  TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  granted_at  TIMESTAMP NULL DEFAULT NULL,
  PRIMARY KEY (guid, claim_date),
  UNIQUE KEY uq_custom_daily_reward_claim_token (token),
  KEY ix_custom_daily_reward_claim_status (status, created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='每日奖励原子领取闸；主键保证每角色每日只有一个winner';

SELECT
  (SELECT COUNT(*) FROM information_schema.tables
   WHERE table_schema='characters' AND table_name='custom_daily_reward') AS summary_table,
  (SELECT COUNT(*) FROM information_schema.tables
   WHERE table_schema='characters' AND table_name='custom_daily_reward_claim') AS claim_table;
