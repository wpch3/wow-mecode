-- ============================================================================
--  把一批 bot 模板【平均分配】成 10 个职业 + 合法种族 + 两大阵营
-- ============================================================================
--
--  用户需求：「想让70001战士这个模板的bot平均分配所有玩家职业npc身上
--            （就是刚刚把0id的职业，平均分配类似的，让他平均变成其他的几个职业）」
--
--  ==========================================================================
--  【比"只改职业"多做了两件事，原因如下】
--  ==========================================================================
--
--   1. 种族必须跟着职业改
--      3.3.5 有职业x种族限制：牛头人不能当法师、侏儒不能当萨满。
--      填了非法组合，客户端角色面板会显示异常。
--
--   2. 种族决定阵营（这条最容易忽略）
--      botdatamgr.cpp:3517  GetDefaultFactionForBotRaceClass()
--          ChrRacesEntry const* rentry = sChrRacesStore.LookupEntry(bot_race);
--          return rentry ? rentry->FactionID : ...;
--      -> 种族填错，bot 会站错阵营（联盟bot跑去部落营地被卫兵打）
--
--   所以下面【职业+种族一起改】，并按 entry 奇偶分成联盟/部落。
--
--  【阵营不用手动填】characters_npcbot.faction 是运行时数据，
--  未招募的 bot 没有这行，生成时会按种族自动算。
--
--  执行：Alt+X 全部执行
-- ============================================================================

USE `world`;


-- ############################################################################
--  第 1 步：设定范围
-- ############################################################################

SET @from_id := 71000;    -- 你重新生成的起点
SET @to_id   := 71499;    -- 终点


-- 先看现状
SELECT
    e.`class`   AS '职业ID',
    e.`race`    AS '种族ID',
    COUNT(*)    AS '数量'
FROM `world`.`creature_template_npcbot_extras` e
WHERE e.`entry` BETWEEN @from_id AND @to_id
GROUP BY e.`class`, e.`race`
ORDER BY e.`class`;


-- ############################################################################
--  第 2 步：平均分配职业（按 entry 除以10取余，均分成10份）
-- ############################################################################
--
--  10 个可玩职业：1战士 2圣骑士 3猎人 4潜行者 5牧师
--                6死亡骑士 7萨满 8法师 9术士 11德鲁伊
--  （跳过 10，那不是有效职业ID）

UPDATE `world`.`creature_template_npcbot_extras`
SET `class` = ELT((`entry` % 10) + 1,  1, 2, 3, 4, 5, 6, 7, 8, 9, 11)
WHERE `entry` BETWEEN @from_id AND @to_id;

SELECT ROW_COUNT() AS '已分配职业的条数';


-- ############################################################################
--  第 3 步：按职业配【合法种族】，并按奇偶分联盟/部落
-- ############################################################################
--
--  entry 是偶数 -> 联盟种族
--  entry 是奇数 -> 部落种族
--  这样两个阵营各占一半，世界更平衡。
--
--  每个职业的种族都是查过 3.3.5 官方限制的合法组合：
--
--    职业      联盟代表        部落代表
--    战士      1  人类         2  兽人
--    圣骑士    1  人类         6  牛头人
--    猎人      3  矮人         2  兽人
--    潜行者    1  人类         2  兽人
--    牧师      1  人类         5  亡灵
--    死亡骑士  1  人类         2  兽人
--    萨满      11 德莱尼       2  兽人      <- 萨满联盟只能德莱尼
--    法师      1  人类         5  亡灵
--    术士      1  人类         2  兽人
--    德鲁伊    4  暗夜精灵     6  牛头人    <- 德鲁伊只有这两个种族

UPDATE `world`.`creature_template_npcbot_extras`
SET `race` = CASE
    WHEN `entry` % 2 = 0 THEN          -- 偶数 = 联盟
        CASE `class`
            WHEN 1  THEN 1             -- 战士     -> 人类
            WHEN 2  THEN 1             -- 圣骑士   -> 人类
            WHEN 3  THEN 3             -- 猎人     -> 矮人
            WHEN 4  THEN 1             -- 潜行者   -> 人类
            WHEN 5  THEN 1             -- 牧师     -> 人类
            WHEN 6  THEN 1             -- 死亡骑士 -> 人类
            WHEN 7  THEN 11            -- 萨满     -> 德莱尼（联盟唯一可选）
            WHEN 8  THEN 1             -- 法师     -> 人类
            WHEN 9  THEN 1             -- 术士     -> 人类
            WHEN 11 THEN 4             -- 德鲁伊   -> 暗夜精灵
            ELSE 1
        END
    ELSE                               -- 奇数 = 部落
        CASE `class`
            WHEN 1  THEN 2             -- 战士     -> 兽人
            WHEN 2  THEN 6             -- 圣骑士   -> 牛头人
            WHEN 3  THEN 2             -- 猎人     -> 兽人
            WHEN 4  THEN 2             -- 潜行者   -> 兽人
            WHEN 5  THEN 5             -- 牧师     -> 亡灵
            WHEN 6  THEN 2             -- 死亡骑士 -> 兽人
            WHEN 7  THEN 2             -- 萨满     -> 兽人
            WHEN 8  THEN 5             -- 法师     -> 亡灵
            WHEN 9  THEN 2             -- 术士     -> 兽人
            WHEN 11 THEN 6             -- 德鲁伊   -> 牛头人
            ELSE 2
        END
END
WHERE `entry` BETWEEN @from_id AND @to_id;

SELECT ROW_COUNT() AS '已分配种族的条数';


-- ############################################################################
--  第 4 步：模型也要跟着种族走（否则看着还是原来那个战士）
-- ############################################################################
--
--  【这一步很关键】
--    SpellHandler.cpp:645  data << uint32(bot->GetDisplayId());
--    你实际看到的模型来自 creature_template.modelid1，
--    只改 race 不改 modelid1 -> 面板写"牛头人"但看着还是人类。
--
--  做法：从【上游原生bot】里找同种族的模型抄过来。
--        原生bot在 70001-70500，它们的种族x模型是验证过能正常显示的。

-- 4.1 先看有哪些现成的种族->模型映射可用
SELECT
    e.`race`         AS '种族ID',
    CASE e.`race`
        WHEN 1 THEN '人类'   WHEN 2 THEN '兽人'   WHEN 3 THEN '矮人'
        WHEN 4 THEN '暗夜'   WHEN 5 THEN '亡灵'   WHEN 6 THEN '牛头人'
        WHEN 7 THEN '侏儒'   WHEN 8 THEN '巨魔'
        WHEN 10 THEN '血精灵' WHEN 11 THEN '德莱尼'
        ELSE CONCAT('?', e.`race`)
    END              AS '种族',
    MIN(ct.`modelid1`) AS '可用模型ID',
    COUNT(*)         AS '样本数'
FROM `world`.`creature_template_npcbot_extras` e
JOIN `world`.`creature_template` ct ON ct.`entry` = e.`entry`
WHERE e.`entry` < @from_id                              -- 只看原生bot
  AND (ct.`flags_extra` & 0x8E000000) = 0x8E000000      -- 必须是bot不是宠物
  AND ct.`modelid1` > 0
GROUP BY e.`race`
ORDER BY e.`race`;

-- 4.2 自动按种族套用模型（从原生bot里取同种族的第一个模型）
UPDATE `world`.`creature_template` tgt
JOIN `world`.`creature_template_npcbot_extras` tgt_e ON tgt_e.`entry` = tgt.`entry`
JOIN (
    SELECT e.`race`, MIN(ct.`modelid1`) AS model
    FROM `world`.`creature_template_npcbot_extras` e
    JOIN `world`.`creature_template` ct ON ct.`entry` = e.`entry`
    WHERE e.`entry` < @from_id
      AND (ct.`flags_extra` & 0x8E000000) = 0x8E000000
      AND ct.`modelid1` > 0
    GROUP BY e.`race`
) src ON src.`race` = tgt_e.`race`
SET tgt.`modelid1` = src.`model`
WHERE tgt.`entry` BETWEEN @from_id AND @to_id;

SELECT ROW_COUNT() AS '已更新模型的条数';

--  【如果某个种族在原生bot里没有样本】那批 bot 的模型不会被更新，
--  仍是原来战士的模型。第5步的验证会显示出来。


-- ############################################################################
--  第 5 步：验证
-- ############################################################################

SELECT
    e.`class`  AS '职业ID',
    CASE e.`class`
        WHEN 1 THEN '战士'   WHEN 2 THEN '圣骑士' WHEN 3 THEN '猎人'
        WHEN 4 THEN '潜行者' WHEN 5 THEN '牧师'   WHEN 6 THEN '死亡骑士'
        WHEN 7 THEN '萨满'   WHEN 8 THEN '法师'   WHEN 9 THEN '术士'
        WHEN 11 THEN '德鲁伊' ELSE CONCAT('?', e.`class`)
    END        AS '职业',
    e.`race`   AS '种族ID',
    CASE WHEN e.`race` IN (1,3,4,7,11) THEN '联盟' ELSE '部落' END AS '阵营',
    COUNT(*)   AS '数量',
    COUNT(DISTINCT ct.`modelid1`) AS '模型种类'
FROM `world`.`creature_template_npcbot_extras` e
JOIN `world`.`creature_template` ct ON ct.`entry` = e.`entry`
WHERE e.`entry` BETWEEN @from_id AND @to_id
GROUP BY e.`class`, e.`race`
ORDER BY e.`class`, e.`race`;

-- 阵营平衡检查
SELECT
    SUM(CASE WHEN `race` IN (1,3,4,7,11) THEN 1 ELSE 0 END) AS '联盟数',
    SUM(CASE WHEN `race` NOT IN (1,3,4,7,11) THEN 1 ELSE 0 END) AS '部落数',
    COUNT(*) AS '总数'
FROM `world`.`creature_template_npcbot_extras`
WHERE `entry` BETWEEN @from_id AND @to_id;

-- 最终判定
SELECT
    CASE WHEN (SELECT COUNT(*) FROM `world`.`creature_template_npcbot_extras`
               WHERE `entry` BETWEEN @from_id AND @to_id AND (`class` = 0 OR `race` = 0)) = 0
         THEN '[OK] 职业和种族都已正确分配'
         ELSE '[问题] 还有 class=0 或 race=0 的，检查上面步骤'
    END AS '判定';


-- ############################################################################
--  第 6 步：让它生效
-- ############################################################################
--
--  1. 查可用备用数，改 conf：
--       SELECT COUNT(*) - (SELECT COUNT(*) FROM `characters`.`characters_npcbot`)
--       FROM `world`.`creature_template_npcbot_extras` WHERE `class` <> 0;
--
--     worldserver.conf.d\unlimited.conf
--       NpcBot.WanderingBots.Continents.Count = 填上面查到的数字（略小一点）
--
--  2. 【确认各职业都在 conf 里启用了】没启用的职业模板不计入统计：
--       NpcBot.WanderingBots.Classes.Warrior.Enable     = 1
--       NpcBot.WanderingBots.Classes.Paladin.Enable     = 1
--       NpcBot.WanderingBots.Classes.Hunter.Enable      = 1
--       NpcBot.WanderingBots.Classes.Rogue.Enable       = 1
--       NpcBot.WanderingBots.Classes.Priest.Enable      = 1
--       NpcBot.WanderingBots.Classes.DeathKnight.Enable = 1
--       NpcBot.WanderingBots.Classes.Shaman.Enable      = 1
--       NpcBot.WanderingBots.Classes.Mage.Enable        = 1
--       NpcBot.WanderingBots.Classes.Warlock.Enable     = 1
--       NpcBot.WanderingBots.Classes.Druid.Enable       = 1
--
--  3. 重启服务端，日志应看到：
--       >> Set up spawning of XXX wandering bots in ... ms
--
--  4. 客户端可能有缓存，看到旧模型的话：
--       删 客户端目录\Cache\WDB\zhCN\creaturecache.wdb 后重登


-- ############################################################################
--  想要更多样化？
-- ############################################################################
--
--  上面每个职业只用了1个联盟种族+1个部落种族。
--  想让种族更丰富，可以在合法范围内随机（下面是战士的例子）：
--
--  UPDATE `world`.`creature_template_npcbot_extras`
--  SET `race` = ELT(FLOOR(RAND() * 5) + 1, 1, 3, 4, 7, 11)   -- 战士的联盟种族
--  WHERE `entry` BETWEEN @from_id AND @to_id
--    AND `class` = 1 AND `entry` % 2 = 0;
--
--  【但改完种族要重跑第4步更新模型】，否则模型对不上。
--
--  各职业合法种族速查（3.3.5）：
--    战士     1,2,3,4,5,6,7,8,11
--    圣骑士   1,3,6,10,11
--    猎人     2,3,4,6,8,10,11
--    潜行者   1,2,3,4,5,7,8,10
--    牧师     1,3,4,5,8,10,11
--    死亡骑士 1,2,3,4,5,6,7,8,10,11
--    萨满     2,6,8,11
--    法师     1,3,5,7,8,10,11
--    术士     1,2,5,7,8,10
--    德鲁伊   4,6
--  （联盟=1,3,4,7,11  部落=2,5,6,8,10）
