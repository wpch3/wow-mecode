-- ============================================================
--  套装系统 v3 · 安装自检（简版·最可靠）
--  【本文件只有一条语句，把光标放在语句里执行】
--
--  预期：返回 1 行，7 个数字列，全部有值不报错 = 安装完整
--
--  如果报错 "Table 'xxx' doesn't exist"，
--  错误信息里的表名就告诉你缺哪个文件了：
--    custom_gearset          -> 04_gearset_table.sql
--    custom_raid_progress    -> 05_tierset_progress.sql
--    custom_tierset_unlocked -> 06_tierset_unlocked.sql
--    custom_gearset_config   -> 07_tierset_config.sql
--    custom_tierset_dungeon  -> 08_tierset_dungeon_req.sql
-- ============================================================
SELECT
  (SELECT COUNT(*) FROM characters.custom_gearset)            AS `04_方案表`,
  (SELECT COUNT(*) FROM characters.custom_raid_progress)      AS `05_刷本进度`,
  (SELECT COUNT(*) FROM characters.custom_tierset_unlocked)   AS `06_已解锁`,
  (SELECT COUNT(*) FROM characters.custom_gearset_config)     AS `07_角色配置`,
  (SELECT COUNT(*) FROM world.custom_tierset_dungeon)         AS `08_09_副本门槛_应为68`,
  (SELECT COUNT(*) FROM auth.rbac_permissions
     WHERE id = 71005)                                        AS `10_权限_应为1`,
  (SELECT COUNT(*) FROM auth.rbac_linked_permissions
     WHERE id = 192 AND linkedId = 71005)                     AS `11_权限组_应为1`;
