-- =====================================================================
-- 测试物品 -- 每步严格【一条语句】
--
-- 上一版失败原因：STEP5 里放了 INSERT 和 DROP 两条语句，
-- 你的客户端只执行了其中一条（DROP），导致 INSERT 被跳过、
-- 临时表被删掉，900001 从未写入 -> 查询只有列名没有数据。
--
-- 本版每个 STEP 严格只有一条语句，绝不会再漏。
-- =====================================================================
--
-- 【执行方法】把光标放在某条语句上执行，然后换下一条。
--             STEP1 -> STEP2 -> STEP3 -> STEP4 -> STEP5 -> STEP6
--
-- 【如果中途失败】从 STEP1 重新开始即可，脚本可重复执行。
-- =====================================================================


-- ========================= STEP 1 =========================
-- 删除旧的测试物品

DELETE FROM world.item_template WHERE entry = 900001;


-- ========================= STEP 2 =========================
-- 清理可能残留的临时表

DROP TABLE IF EXISTS world.tmp_bignum_copy;


-- ========================= STEP 3 =========================
-- 复制模板 46017 到临时表（自动匹配全部 130+ 列）

CREATE TABLE world.tmp_bignum_copy AS SELECT * FROM world.item_template WHERE entry = 46017;


-- ========================= STEP 4 =========================
-- 在临时表里改主键 + 写入大数值

UPDATE world.tmp_bignum_copy SET entry = 900001, name = '测试-十亿之刃', description = '大数值改造验证物品', Quality = 6, ItemLevel = 999, RequiredLevel = 1, AllowableClass = -1, AllowableRace = -1, bonding = 0, StatsCount = 3, stat_type1 = 7, stat_value1 = 100000000, stat_type2 = 4, stat_value2 = 500000000, stat_type3 = 38, stat_value3 = 1000000000, armor = 1000000000, holy_res = 500000000, fire_res = 500000000, nature_res = 500000000, frost_res = 500000000, shadow_res = 500000000, arcane_res = 500000000, dmg_min1 = 1000000, dmg_max1 = 2000000, MaxDurability = 1000000;


-- ========================= STEP 5 =========================
-- 写回正式表  ★★★ 这一步最关键，上次就是它被跳过了 ★★★

INSERT INTO world.item_template SELECT * FROM world.tmp_bignum_copy;


-- ========================= STEP 6 =========================
-- 验证（先看结果，确认无误后再执行 STEP 7 清理）

SELECT entry, name, stat_value1 AS 耐力, stat_value2 AS 力量, stat_value3 AS 攻强, armor AS 护甲, holy_res AS 神圣抗性, MaxDurability AS 耐久 FROM world.item_template WHERE entry = 900001;


-- ========================= STEP 7 =========================
-- 确认 STEP6 有数据后，再执行这条清理临时表

DROP TABLE IF EXISTS world.tmp_bignum_copy;


-- =====================================================================
-- STEP 6 期望输出（必须有一行数据）：
--
-- entry  | name          | 耐力      | 力量      | 攻强       | 护甲       | 神圣抗性  | 耐久
-- 900001 | 测试-十亿之刃 | 100000000 | 500000000 | 1000000000 | 1000000000 | 500000000 | 1000000
--
-- 【只有列名没有数据行】-> STEP5 没执行成功，回去重新执行 STEP5
-- 【耐力显示 32767】    -> stat_value 列没扩展
-- 【神圣抗性显示 255】  -> 抗性列没扩展
-- 【中文乱码】          -> 客户端连接字符集不是 utf8mb4
--
-- 排查用：检查临时表是否还在、有没有数据
--   SELECT COUNT(*) FROM world.tmp_bignum_copy;
--   -- 返回 1 = STEP3/4 成功，问题在 STEP5
--   -- 报表不存在 = STEP3 没执行或 STEP7 提前执行了
-- =====================================================================
