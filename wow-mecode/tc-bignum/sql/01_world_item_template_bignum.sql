-- =====================================================================
-- TrinityCore 3.3.5 (NPCBOT-Eluna-zhCN) —— 物品数值上限解锁 / world 库
-- 目标：stat_value 等字段从 smallint(+/-32767) 扩展到 int(+/-21亿)
--
-- !! 执行顺序警告 !!
-- 必须【先编译好打过补丁的 worldserver】，再执行本 SQL。
-- 如果只改库不改源码，ObjectMgr.cpp 里的 Field::GetInt16() 会在读到
-- >32767 的值时触发 LogTruncation() -> ASSERT(false) -> 服务端直接崩溃。
-- （已实测验证：col=int val=100000 via GetInt16 -> ASSERT 命中）
--
-- 建议先备份： mysqldump -u root -p world item_template > item_template.bak.sql
-- =====================================================================

USE world;

-- ---------------------------------------------------------------
-- 1) 10 条属性值 stat_value1..10 : smallint -> int
--    对应 C++: itemTemplate.ItemStat[i].ItemStatValue = fields[29+i*2].GetInt32();
--    结构体 _ItemStat::ItemStatValue 本来就是 int32，无需改结构体。
-- ---------------------------------------------------------------
ALTER TABLE item_template
  MODIFY stat_value1  int NOT NULL DEFAULT '0',
  MODIFY stat_value2  int NOT NULL DEFAULT '0',
  MODIFY stat_value3  int NOT NULL DEFAULT '0',
  MODIFY stat_value4  int NOT NULL DEFAULT '0',
  MODIFY stat_value5  int NOT NULL DEFAULT '0',
  MODIFY stat_value6  int NOT NULL DEFAULT '0',
  MODIFY stat_value7  int NOT NULL DEFAULT '0',
  MODIFY stat_value8  int NOT NULL DEFAULT '0',
  MODIFY stat_value9  int NOT NULL DEFAULT '0',
  MODIFY stat_value10 int NOT NULL DEFAULT '0';

-- ---------------------------------------------------------------
-- 2) 护甲 + 6 系抗性 : smallint/tinyint -> int unsigned
--    对应 C++: Armor/HolyRes/.../ArcaneRes 全部改用 GetUInt32()
--    注意：抗性原本是 tinyint(255)，这是最容易被忽略的一个坑。
-- ---------------------------------------------------------------
ALTER TABLE item_template
  MODIFY armor      int unsigned NOT NULL DEFAULT '0',
  MODIFY holy_res   int unsigned NOT NULL DEFAULT '0',
  MODIFY fire_res   int unsigned NOT NULL DEFAULT '0',
  MODIFY nature_res int unsigned NOT NULL DEFAULT '0',
  MODIFY frost_res  int unsigned NOT NULL DEFAULT '0',
  MODIFY shadow_res int unsigned NOT NULL DEFAULT '0',
  MODIFY arcane_res int unsigned NOT NULL DEFAULT '0';

-- ---------------------------------------------------------------
-- 3) 物品等级 / 最大耐久 : smallint -> int unsigned
--    MaxDurability 必须与 characters.item_instance.durability 一起改，
--    见 02_characters_durability.sql，否则存盘会被 setUInt16 截断。
-- ---------------------------------------------------------------
ALTER TABLE item_template
  MODIFY ItemLevel     int unsigned NOT NULL DEFAULT '0',
  MODIFY MaxDurability int unsigned NOT NULL DEFAULT '0';

-- ---------------------------------------------------------------
-- 4) dmg_min/dmg_max 已经是 float，无需 ALTER。
--    float 在超过 ~1677 万后会丢失整数精度，如需绝对精确请改 double
--    （同时要把 ItemTemplate.h 的 _Damage 与 QueryPackets 改成 double，
--     但 3.3.5 客户端封包该字段就是 4 字节 float，改了客户端也读不到，
--     所以这里【不建议】动。）
-- ---------------------------------------------------------------

-- ---------------------------------------------------------------
-- 5) 自检：确认列类型已生效
-- ---------------------------------------------------------------
SELECT COLUMN_NAME, COLUMN_TYPE
FROM information_schema.COLUMNS
WHERE TABLE_SCHEMA = 'world'
  AND TABLE_NAME   = 'item_template'
  AND COLUMN_NAME IN ('stat_value1','armor','holy_res','ItemLevel','MaxDurability')
ORDER BY ORDINAL_POSITION;
-- 期望输出： stat_value1=int, armor=int unsigned, holy_res=int unsigned,
--            ItemLevel=int unsigned, MaxDurability=int unsigned
