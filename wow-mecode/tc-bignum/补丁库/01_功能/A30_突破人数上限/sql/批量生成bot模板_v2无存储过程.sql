-- ============================================================================
--  step46  批量生成 NPCBot 模板 —— v2【无存储过程版】
-- ============================================================================
--
--  【为什么有 v2】
--    v1 用了存储过程 + DELIMITER，但 DBeaver 默认【不识别 DELIMITER】，
--    会把过程体在第一个分号处切断，导致：
--        SQL 错误 [1064] ... near '' at line 11
--    第 11 行正是 `DECLARE i INT DEFAULT 0;` —— MySQL 收到的是一个
--    没有闭合 END 的残缺 CREATE PROCEDURE。
--
--  【v2 的做法】完全不用循环、不用存储过程、不用 DELIMITER。
--    改用【数字辅助表 + 一次性 INSERT ... SELECT】批量生成。
--    这是纯标准 SQL，任何客户端都能跑。
--
--  执行方式：DBeaver 里直接 Alt+X 执行全部。
-- ============================================================================


-- ############################################################################
--  【执行前必读】关于 "No database selected"
-- ############################################################################
--
--  v2 第一版报了 SQL错误[1046] No database selected —— 原因是
--  MySQL 的 CREATE TEMPORARY TABLE 需要【当前数据库上下文】。
--
--  现已修正：所有表（包括临时表）都写成 `库`.`表` 的完全限定名，
--  不依赖任何客户端的"活动数据库"设置。
--
--  下面这行 USE 是双保险，即使它不生效也没关系。
--  （DBeaver 有时会忽略脚本里的 USE，以工具栏下拉框为准）
-- ############################################################################

USE `world`;


-- ############################################################################
--  第 0 步：先看现状
-- ############################################################################

SELECT
    (SELECT COUNT(*) FROM `world`.`creature_template_npcbot_extras`)   AS '模板总数',
    (SELECT COUNT(*) FROM `characters`.`characters_npcbot`)            AS '已被拥有',
    (SELECT COUNT(*) FROM `world`.`creature_template_npcbot_extras`)
  - (SELECT COUNT(*) FROM `characters`.`characters_npcbot`)            AS '可用备用数';

--  "可用备用数" 必须 >= NpcBot.WanderingBots.Continents.Count
--  否则启动时 botdatamgr.cpp:1878 会 ASSERT(false) 崩服。


-- 看现有模板的职业分布，挑一个做源模板
SELECT
    e.`class`                AS '职业ID',
    CASE e.`class`
        WHEN 0  THEN '【不能选！】class=0 无效，选了生成的模板全部作废'
        WHEN 1  THEN '战士'   WHEN 2  THEN '圣骑士' WHEN 3  THEN '猎人'
        WHEN 4  THEN '潜行者' WHEN 5  THEN '牧师'   WHEN 6  THEN '死亡骑士'
        WHEN 7  THEN '萨满'   WHEN 8  THEN '法师'   WHEN 9  THEN '术士'
        WHEN 11 THEN '德鲁伊' ELSE CONCAT('扩展职业(', e.`class`, ')')
    END                      AS '职业',
    COUNT(*)                 AS '数量',
    MIN(e.`entry`)           AS '可用作源模板的entry'
FROM `world`.`creature_template_npcbot_extras` e
JOIN `world`.`creature_template` ct2 ON ct2.`entry` = e.`entry`
WHERE e.`class` <> 0                                    -- 过滤 class=0
  AND (ct2.`flags_extra` & 0x8E000000) = 0x8E000000     -- 过滤宠物(0x8A000000)
GROUP BY e.`class`
ORDER BY e.`class`;

--  【重要】上面已经过滤掉 class=0 了。
--  botdatamgr.cpp:319  if (c != BOT_CLASS_NONE && ...) —— class=0 的模板
--  【永远不会被统计】，选它当源模板 = 生成500个废物。

--  职业ID：1战士 2圣骑士 3猎人 4潜行者 5牧师 6死亡骑士
--          7萨满 8法师 9术士 11德鲁伊  12+扩展职业


-- ############################################################################
--  第 1 步：参数
-- ############################################################################
--  只改这三个值。

SET @src_entry := 70001;      -- 源模板（从上面查询里挑一个真实存在的）
SET @start_id  := 71000;      -- 新 entry 起点（避开上游 70800+ 保留区）
SET @count     := 500;        -- 生成数量


-- ############################################################################
--  第 2 步：安全检查（两条都必须是 [OK] 才继续）
-- ############################################################################

SELECT
    CASE WHEN EXISTS(SELECT 1 FROM `world`.`creature_template` WHERE `entry` = @src_entry)
         THEN CONCAT('[OK] 源模板 ', @src_entry, ' 存在')
         ELSE CONCAT('[停止] 源模板 ', @src_entry, ' 不存在，回第0步换一个')
    END AS '检查1_源模板',
    CASE WHEN EXISTS(SELECT 1 FROM `world`.`creature_template_npcbot_extras` WHERE `entry` = @src_entry)
         THEN '[OK] 源模板有 creature_template_npcbot_extras 数据'
         ELSE '[停止] 源模板不是 npcbot！换一个'
    END AS '检查2_是否npcbot',
    CASE WHEN (SELECT (`flags_extra` & 0x8E000000) FROM `world`.`creature_template`
               WHERE `entry` = @src_entry) = 0x8A000000
         THEN '[停止！] 源模板是【宠物】不是bot，生成后会一直刷 no owner! 日志'
         WHEN (SELECT (`flags_extra` & 0x8E000000) FROM `world`.`creature_template`
               WHERE `entry` = @src_entry) <> 0x8E000000
         THEN '[停止！] 源模板 flags_extra 不是 NPCBOT，换一个'
         ELSE '[OK] 源模板是正常bot'
    END AS '检查2c_是否真bot',
    CASE WHEN (SELECT `class` FROM `world`.`creature_template_npcbot_extras`
               WHERE `entry` = @src_entry) = 0
         THEN '[停止！] 源模板 class=0，生成出来【一个都不算数】，必须换一个'
         WHEN (SELECT `class` FROM `world`.`creature_template_npcbot_extras`
               WHERE `entry` = @src_entry) IS NULL
         THEN '[停止] 查不到 class'
         ELSE CONCAT('[OK] 源模板 class=',
              (SELECT `class` FROM `world`.`creature_template_npcbot_extras`
               WHERE `entry` = @src_entry), ' 有效')
    END AS '检查2b_职业是否有效';

SELECT
    CASE WHEN EXISTS(
            SELECT 1 FROM `world`.`creature_template`
            WHERE `entry` BETWEEN @start_id AND @start_id + @count - 1)
         THEN CONCAT('[停止] ', @start_id, '-', @start_id + @count - 1, ' 区间已有数据，换个起点')
         ELSE CONCAT('[OK] ', @start_id, '-', @start_id + @count - 1, ' 区间空闲')
    END AS '检查3_区间冲突';


-- ############################################################################
--  第 3 步：造一张数字辅助表（0,1,2,...,9999）
-- ############################################################################
--
--  这是替代 WHILE 循环的关键。
--  用 4 张 10 行的表做笛卡尔积 = 10000 行，够生成 1 万个模板。
--  如果要更多，再叠一层 d4 即可。

DROP TEMPORARY TABLE IF EXISTS `world`.`tmp_numbers`;
CREATE TEMPORARY TABLE `world`.`tmp_numbers` (`n` INT UNSIGNED NOT NULL PRIMARY KEY);

INSERT INTO `world`.`tmp_numbers` (`n`)
SELECT d0.d + d1.d*10 + d2.d*100 + d3.d*1000 AS n
FROM
    (SELECT 0 d UNION ALL SELECT 1 UNION ALL SELECT 2 UNION ALL SELECT 3 UNION ALL SELECT 4
     UNION ALL SELECT 5 UNION ALL SELECT 6 UNION ALL SELECT 7 UNION ALL SELECT 8 UNION ALL SELECT 9) d0,
    (SELECT 0 d UNION ALL SELECT 1 UNION ALL SELECT 2 UNION ALL SELECT 3 UNION ALL SELECT 4
     UNION ALL SELECT 5 UNION ALL SELECT 6 UNION ALL SELECT 7 UNION ALL SELECT 8 UNION ALL SELECT 9) d1,
    (SELECT 0 d UNION ALL SELECT 1 UNION ALL SELECT 2 UNION ALL SELECT 3 UNION ALL SELECT 4
     UNION ALL SELECT 5 UNION ALL SELECT 6 UNION ALL SELECT 7 UNION ALL SELECT 8 UNION ALL SELECT 9) d2,
    (SELECT 0 d UNION ALL SELECT 1 UNION ALL SELECT 2 UNION ALL SELECT 3 UNION ALL SELECT 4
     UNION ALL SELECT 5 UNION ALL SELECT 6 UNION ALL SELECT 7 UNION ALL SELECT 8 UNION ALL SELECT 9) d3;

-- 只保留需要的行数
DELETE FROM `world`.`tmp_numbers` WHERE `n` >= @count;

SELECT COUNT(*) AS '辅助表行数（应等于你设的count）' FROM `world`.`tmp_numbers`;


-- ############################################################################
--  第 4 步：克隆 creature_template
-- ############################################################################
--
--  【关键：不写列名】
--    第一版我手写了全部列名，核对真实建表语句（world_database.sql）后发现是错的：
--    写了 trainer_type / spell1-8 / resistance1-6 / InhabitType 这些
--    【本版本根本不存在】的列，还漏了 StringId。实际是 59 列。
--
--  正确做法：整表结构复制 -> 批量插入 -> 改 entry。
--  这样不管表有多少列、将来加没加列，都不会错。

-- ----------------------------------------------------------------------------
--  【v2.1 修正】这里原本分三步：LIKE建表 -> 插500份 -> UPDATE改entry
--  会报： Duplicate entry '70501' for key 'tmp_ct2.PRIMARY'
--
--  原因：CREATE TEMPORARY TABLE ... LIKE 会【连主键一起复制】
--        （creature_template 的 PRIMARY KEY 就是 entry），
--        而那 500 行的 entry 全是源模板的值 -> 插第2行就撞主键。
--
--  正确做法：【插入时就把 entry 算好】，根本不需要先插再改。
--  用 CREATE TABLE ... SELECT（不带 LIKE）建表，不会继承主键。
-- ----------------------------------------------------------------------------

-- 4.1 建临时表：用 LIKE 保留全部 59 列结构（不写列名）
DROP TEMPORARY TABLE IF EXISTS `world`.`tmp_ct2`;
CREATE TEMPORARY TABLE `world`.`tmp_ct2` LIKE `world`.`creature_template`;

-- 4.2 【关键】去掉主键约束
--     LIKE 会连 PRIMARY KEY(entry) 一起复制，
--     而下一步要插入 500 行【entry 暂时相同】的数据，会撞主键。
--     去掉主键后就能自由插入，改完 entry 再写回正式表（正式表主键仍在，安全）。
ALTER TABLE `world`.`tmp_ct2` DROP PRIMARY KEY;

-- 4.3 笛卡尔积：把源模板复制 @count 份
INSERT INTO `world`.`tmp_ct2`
SELECT t.* FROM `world`.`creature_template` t, `world`.`tmp_numbers` n
WHERE t.`entry` = @src_entry;

SELECT COUNT(*) AS '临时表行数（应等于count）' FROM `world`.`tmp_ct2`;

-- 4.4 改 entry：每行 = 起点 + 行号
SET @rn := -1;
UPDATE `world`.`tmp_ct2` SET `entry` = @start_id + (@rn := @rn + 1);

-- 4.5 校验：改完必须全部不重复
SELECT
    COUNT(*)              AS '总行数',
    COUNT(DISTINCT `entry`) AS '不重复entry数',
    MIN(`entry`)          AS '最小entry',
    MAX(`entry`)          AS '最大entry',
    CASE WHEN COUNT(*) = COUNT(DISTINCT `entry`)
         THEN '[OK] 无重复，可以写入'
         ELSE '[停止] entry 有重复，别往下执行！'
    END                   AS '检查'
FROM `world`.`tmp_ct2`;

-- 4.6 写入正式表（上面显示 [OK] 才执行这一句）
INSERT INTO `world`.`creature_template`
SELECT * FROM `world`.`tmp_ct2`;

SELECT ROW_COUNT() AS '本次插入 creature_template 行数';


-- ############################################################################
--  第 5 步：克隆 creature_template_npcbot_extras（职业和种族）—— 必须有，否则不算bot
-- ############################################################################

INSERT INTO `world`.`creature_template_npcbot_extras` (`entry`, `class`, `race`)
SELECT @start_id + n.`n`, e.`class`, e.`race`
FROM `world`.`tmp_numbers` n, `world`.`creature_template_npcbot_extras` e
WHERE e.`entry` = @src_entry;

SELECT ROW_COUNT() AS '本次插入 creature_template_npcbot_extras 行数';


-- ############################################################################
--  第 6 步：克隆外观（可选，没有就跳过不报错）
-- ############################################################################

INSERT INTO `world`.`creature_template_npcbot_appearance`
    (`entry`, `gender`, `skin`, `face`, `hair`, `haircolor`, `features`)
SELECT @start_id + n.`n`, a.`gender`, a.`skin`, a.`face`, a.`hair`, a.`haircolor`, a.`features`
FROM `world`.`tmp_numbers` n, `world`.`creature_template_npcbot_appearance` a
WHERE a.`entry` = @src_entry;


-- ############################################################################
--  第 7 步：克隆装备（可选）
-- ############################################################################

INSERT INTO `world`.`creature_equip_template`
    (`CreatureID`, `ID`, `ItemID1`, `ItemID2`, `ItemID3`, `VerifiedBuild`)
SELECT @start_id + n.`n`, q.`ID`, q.`ItemID1`, q.`ItemID2`, q.`ItemID3`, q.`VerifiedBuild`
FROM `world`.`tmp_numbers` n, `world`.`creature_equip_template` q
WHERE q.`CreatureID` = @src_entry;


-- ############################################################################
--  第 8 步：清理临时表
-- ############################################################################

DROP TEMPORARY TABLE IF EXISTS `world`.`tmp_ct2`;
DROP TEMPORARY TABLE IF EXISTS `world`.`tmp_numbers`;


-- ############################################################################
--  第 9 步：验证
-- ############################################################################

SELECT
    (SELECT COUNT(*) FROM `world`.`creature_template_npcbot_extras`)   AS '模板总数_新',
    (SELECT COUNT(*) FROM `characters`.`characters_npcbot`)            AS '已被拥有',
    (SELECT COUNT(*) FROM `world`.`creature_template_npcbot_extras`)
  - (SELECT COUNT(*) FROM `characters`.`characters_npcbot`)            AS '可用备用数_新';

-- 抽查前 5 个（template 和 extras 都要有，缺一个就不算bot）
SELECT ct.`entry`, ct.`name`, e.`class`, e.`race`
FROM `world`.`creature_template` ct
JOIN `world`.`creature_template_npcbot_extras` e ON e.`entry` = ct.`entry`
WHERE ct.`entry` BETWEEN @start_id AND @start_id + 4;

-- 一致性检查：两张表数量必须匹配
SELECT
    CASE WHEN (SELECT COUNT(*) FROM `world`.`creature_template`
               WHERE `entry` BETWEEN @start_id AND @start_id + @count - 1)
            = (SELECT COUNT(*) FROM `world`.`creature_template_npcbot_extras`
               WHERE `entry` BETWEEN @start_id AND @start_id + @count - 1)
         THEN '[OK] template 和 extras 数量一致'
         ELSE '[警告] 两表数量不一致，检查上面第4/5步是否都成功'
    END AS '一致性检查';


-- ############################################################################
--  第 10 步：配置并重启
-- ############################################################################
--
--  worldserver.conf.d\unlimited.conf
--
--    # 这个值必须【小于等于】上面的"可用备用数_新"
--    NpcBot.WanderingBots.Continents.Count = 500
--
--  启动日志应看到：
--    >> Set up spawning of 500 wandering bots in XXX ms
--
--  若看到这个就是模板不够（把 Count 调小或多生成模板）：
--    Only N out of M bots ... cannot be created. Aborting!


-- ############################################################################
--  生成不同职业：改参数再跑一遍第1~8步
-- ############################################################################
--
--    SET @src_entry := 70020;    -- 换一个法师模板
--    SET @start_id  := 71500;    -- 起点错开，别和上一批重叠
--    SET @count     := 500;
--
--  建议各职业均衡生成，不然游荡bot全是战士很怪。


-- ############################################################################
--  撤销
-- ############################################################################
--
--  【删之前先把 NpcBot.WanderingBots.Continents.Count 调小】，
--  否则重启时模板不够会 ASSERT 崩服。
--
--  DELETE FROM `world`.`creature_template`                   WHERE `entry` BETWEEN 71000 AND 71499;
--  DELETE FROM `world`.`creature_template_npcbot_extras`     WHERE `entry` BETWEEN 71000 AND 71499;
--  DELETE FROM `world`.`creature_template_npcbot_appearance` WHERE `entry` BETWEEN 71000 AND 71499;
--  DELETE FROM `world`.`creature_equip_template`             WHERE `CreatureID` BETWEEN 71000 AND 71499;


-- ############################################################################
--  性能提醒
-- ############################################################################
--
--  TrinityCore 是【单线程主循环】，每个 bot 每 tick 都跑 AI。
--
--     1,000 个   约 0.5-1 GB，可接受
--    10,000 个   约 5-10 GB，单核吃紧
--   100,000 个   单进程扛不住（不是内存问题，是主循环卡死）
--
--  建议 500 -> 1000 -> 3000 逐档验证，每档看 worldserver 的
--  CPU 占用和 "Update time diff" 日志。
--
--  要真正上万，需要先做【bot休眠/分片机制】（远离玩家的bot降低AI频率）。
