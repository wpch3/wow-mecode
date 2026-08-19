-- ============================================================
--  step28  场景快照表
--
--  用途：.scene 指令把一片区域所有 NPC 的完整状态存下来，
--        之后一条指令完整还原。做剧情反复调试时不用重新摆位置。
--
--  两张表：
--    custom_scene       快照本身（名字、创建者、地图、中心点）
--    custom_scene_npc   快照里每个 NPC 的完整状态
--
--  本文件包含 2 条 CREATE，DBeaver 按 Alt+X 一次执行
-- ============================================================

-- ---------- 1. 快照主表 ----------
CREATE TABLE IF NOT EXISTS `world`.`custom_scene` (
  `id`          int unsigned NOT NULL AUTO_INCREMENT,
  `name`        varchar(64)  NOT NULL              COMMENT '快照名，用户自己起',
  `creator`     varchar(32)  NOT NULL DEFAULT ''   COMMENT '创建者角色名',
  `map`         int unsigned NOT NULL DEFAULT '0'  COMMENT '地图ID',
  `center_x`    float        NOT NULL DEFAULT '0'  COMMENT '拍快照时玩家站的位置',
  `center_y`    float        NOT NULL DEFAULT '0',
  `center_z`    float        NOT NULL DEFAULT '0',
  `radius`      float        NOT NULL DEFAULT '0'  COMMENT '拍摄半径',
  `npc_count`   int unsigned NOT NULL DEFAULT '0'  COMMENT '含多少个NPC',
  `created`     int unsigned NOT NULL DEFAULT '0'  COMMENT 'unix时间戳',
  `comment`     varchar(255)          DEFAULT NULL COMMENT '备注',
  PRIMARY KEY (`id`),
  UNIQUE KEY `idx_name` (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
  COMMENT='场景快照 - 主表';

-- ---------- 2. 快照内容表 ----------
--
--  为什么每个字段都要存，见 cs_scene.cpp 里的说明：
--    guid       = creature.guid（spawnId）。0 表示这是临时召唤物，
--                 还原时只能按 entry 重新召唤，回不到原来那只
--    entry      = creature_template.entry，用于重建丢失的 NPC
--    unit_flags = .nst 的六档状态就存在这里（免疫位/可选中位）
--    faction    = 阵营，和 unit_flags 一起决定"能不能打"
--    react      = REACT_PASSIVE/DEFENSIVE/AGGRESSIVE，决定主不主动
--    display    = 模型，配合 .morph 之类的改动
--    scale      = 缩放
--    stand      = 站姿（站/坐/跪/睡）
--    emote      = 情绪动作，配合 .emote 持续动作
CREATE TABLE IF NOT EXISTS `world`.`custom_scene_npc` (
  `scene_id`    int unsigned NOT NULL              COMMENT '关联 custom_scene.id',
  `idx`         int unsigned NOT NULL              COMMENT '序号，保证还原顺序稳定',
  `guid`        int unsigned NOT NULL DEFAULT '0'  COMMENT 'creature.guid，0=临时召唤物',
  `entry`       int unsigned NOT NULL DEFAULT '0'  COMMENT 'creature_template.entry',
  `pos_x`       float        NOT NULL DEFAULT '0',
  `pos_y`       float        NOT NULL DEFAULT '0',
  `pos_z`       float        NOT NULL DEFAULT '0',
  `orientation` float        NOT NULL DEFAULT '0',
  `unit_flags`  int unsigned NOT NULL DEFAULT '0'  COMMENT '.nst 档位存这里',
  `faction`     int unsigned NOT NULL DEFAULT '0',
  `react`       tinyint unsigned NOT NULL DEFAULT '1' COMMENT '0被动 1防御 2主动',
  `display`     int unsigned NOT NULL DEFAULT '0'  COMMENT '模型ID',
  `scale`       float        NOT NULL DEFAULT '1',
  `stand`       tinyint unsigned NOT NULL DEFAULT '0' COMMENT '站姿',
  `emote`       int unsigned NOT NULL DEFAULT '0'  COMMENT '情绪动作',
  `npcflag`     int unsigned NOT NULL DEFAULT '0'  COMMENT '商人/任务等功能位',
  `name_cache`  varchar(64)           DEFAULT NULL COMMENT '存个名字方便人看',
  PRIMARY KEY (`scene_id`,`idx`),
  KEY `idx_scene` (`scene_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
  COMMENT='场景快照 - NPC状态';

-- ============================================================
--  自检
-- ============================================================
SELECT TABLE_NAME, TABLE_COMMENT
FROM information_schema.TABLES
WHERE TABLE_SCHEMA = 'world'
  AND TABLE_NAME IN ('custom_scene','custom_scene_npc');
-- 期望 2 行

-- ============================================================
--  清空快照（需要时手动执行）
-- ============================================================
-- DELETE FROM `world`.`custom_scene_npc` WHERE `scene_id` = <id>;
-- DELETE FROM `world`.`custom_scene`     WHERE `id` = <id>;
