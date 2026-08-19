-- ============================================================
--  step22  字段扩容 —— 解除 smallint 溢出限制
--
--  !! 执行顺序警告（务必先读）!!
--  必须【先跑 apply_step22.sh + 重新编译 worldserver】，再执行本文件。
--
--  为什么顺序不能反（已用 26 项测试实证）：
--    FieldValueConverters.h:59-70 GetNumericValue<T>() 会做截断检测，
--    发现 static_cast 回去对不上就调 LogTruncation()，
--    而 FieldValueConverter.cpp:48 里那是 ASSERT(false, ...) ——
--    Errors.h:68 #define ASSERT WPAssert，非 PERFORMANCE_PROFILING 构建
--    直接崩服，不是打个日志了事。
--
--    最阴的是：老菜单 MenuID 全都 < 65535，
--    所以【只改库不改码，平时一切正常，等你哪天用了大 ID 才崩】。
--
--    反过来（代码先改、库还没改）是安全的：加宽读窄列不会截断。
--    所以顺序只能是【先码后库】。
--
--  本文件包含 4 条 ALTER + 1 条自检，DBeaver 按 Alt+X 一次执行全部
-- ============================================================

-- ---------- 1. gossip_menu.MenuID ----------
-- smallint unsigned(65535) -> int unsigned(42亿)
-- 对应 C++: ObjectMgr.cpp:9546  gMenu.MenuID = fields[0].GetUInt32();
-- 结构体 GossipMenus::MenuID 本来就是 uint32（ObjectMgr.h:800），无需改
ALTER TABLE `world`.`gossip_menu`
  MODIFY `MenuID` int unsigned NOT NULL DEFAULT '0';

-- ---------- 2. gossip_menu_option.MenuID / OptionID ----------
-- 对应 C++: ObjectMgr.cpp:9584-9585  两个都改成 GetUInt32()
-- 结构体 GossipMenuItems::MenuID/OptionID 本来就是 uint32（ObjectMgr.h:782-783）
ALTER TABLE `world`.`gossip_menu_option`
  MODIFY `MenuID`   int unsigned NOT NULL DEFAULT '0',
  MODIFY `OptionID` int unsigned NOT NULL DEFAULT '0';

-- ---------- 3. gossip_menu_option_locale.MenuID / OptionID ----------
-- 对应 C++: ObjectMgr.cpp:305-306
-- 这张表容易被忘：它和上面两张表的 MenuID 必须【同时】扩，
-- 否则中文菜单文本会对不上号（主表能存 960001，locale 表存不进去）
ALTER TABLE `world`.`gossip_menu_option_locale`
  MODIFY `MenuID`   int unsigned NOT NULL DEFAULT '0',
  MODIFY `OptionID` int unsigned NOT NULL DEFAULT '0';

-- ---------- 4. game_tele.map ----------
-- 对应 C++: ObjectMgr.cpp:9064  gt.mapId = fields[5].GetUInt32();
-- 结构体 GameTele::mapId 本来就是 uint32（ObjectMgr.h:165）
-- 3.3.5 原版地图 ID 都很小，但自定义地图可以随便编号
ALTER TABLE `world`.`game_tele`
  MODIFY `map` int unsigned NOT NULL DEFAULT '0';

-- ============================================================
--  自检：确认列类型已生效
-- ============================================================
SELECT TABLE_NAME, COLUMN_NAME, COLUMN_TYPE
FROM information_schema.COLUMNS
WHERE TABLE_SCHEMA = 'world'
  AND (   (TABLE_NAME = 'gossip_menu'               AND COLUMN_NAME = 'MenuID')
       OR (TABLE_NAME = 'gossip_menu_option'        AND COLUMN_NAME IN ('MenuID','OptionID'))
       OR (TABLE_NAME = 'gossip_menu_option_locale' AND COLUMN_NAME IN ('MenuID','OptionID'))
       OR (TABLE_NAME = 'game_tele'                 AND COLUMN_NAME = 'map') )
ORDER BY TABLE_NAME, ORDINAL_POSITION;
-- 期望：6 行，COLUMN_TYPE 全部是 int unsigned
-- 如果还有 smallint unsigned，说明 ALTER 没执行成功

-- ============================================================
--  没有改的字段，以及为什么（免得以后自己犯嘀咕）
-- ============================================================
--
--  creature.zoneId / areaId （smallint）
--    -> 服务端【根本不读这两列】。ObjectMgr.cpp:2328 是
--       sMapMgr->GetZoneAndAreaId(...) 按坐标实时算的，
--       表里那两列纯粹是给人看/给工具用的缓存。改了没意义。
--
--  creature_template.minlevel / maxlevel （tinyint 255）
--    -> 卡点在代码不在库，改库没用：
--       CreatureData.h:309-310  uint8 minlevel / maxlevel
--       Unit.h:890  uint8 GetLevel()
--       DBCEnums.h:53  STRONG_MAX_LEVEL = 255
--       ObjectDefines.h:106  MAKE_PAIR16(level, class) 把 level 压进 8 位
--         -> 300级怪和44级怪会查到同一条 creature_classlevelstats
--       要真突破 255 得改 758 处 GetLevel() + MAKE_PAIR16 + DBC，
--       另立 step 做。tinyint 的 255 和 STRONG_MAX_LEVEL 正好吻合，
--       现阶段【255 级怪本来就能做，不用改任何东西】。
--
--  npc_vendor.maxcount（tinyint）/ conditions.ConditionTarget（tinyint）
--  各种 loot_template.GroupId（tinyint）
--    -> 语义上就不需要大数，改了只增加风险没有收益。
--
--  creature_template.faction （smallint）
--    -> DBC 里 faction 模板 ID 本身就远小于 65535，且受 DBC 约束，
--       改库不改 DBC 没意义。
