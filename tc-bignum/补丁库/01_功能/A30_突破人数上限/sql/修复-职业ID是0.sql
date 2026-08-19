-- ============================================================================
--  修复：生成的模板 class = 0（未知职业），一个都不算数
-- ============================================================================
--
--  【根因，已实查确认】
--
--    SharedDefines.h:138   CLASS_NONE = 0
--    botcommon.h:216       BOT_CLASS_NONE = CLASS_NONE   -> 也就是 0
--
--    botdatamgr.cpp:319
--        if (c != BOT_CLASS_NONE && BotCfg::IsWanderingClassEnabled(c))
--            ^^^^^^^^^^^^^^^^^^^^ 第一个条件就把 class=0 全部排除
--
--    -> class=0 的模板【永远不会被统计】，所以还是 317 个。
--
--  【为什么会变成 0】
--    批量生成时源模板选错了 —— 挑了一个 class=0 的 entry 当模板，
--    克隆出来的 500 个自然全是 0。
--
--  【好消息】这些模板【不用删】，改一条 UPDATE 就能救活。
--    creature_template 里的数据是好的，只是 extras.class 填错了。
--
--  执行：Alt+X 全部执行
-- ============================================================================

USE `world`;


-- ############################################################################
--  第 1 步：确认问题范围
-- ############################################################################

SELECT
    `class`  AS '职业ID',
    CASE `class`
        WHEN 0  THEN '【无效】未知职业，不会被统计'
        WHEN 1  THEN '战士'    WHEN 2  THEN '圣骑士'  WHEN 3  THEN '猎人'
        WHEN 4  THEN '潜行者'  WHEN 5  THEN '牧师'    WHEN 6  THEN '死亡骑士'
        WHEN 7  THEN '萨满'    WHEN 8  THEN '法师'    WHEN 9  THEN '术士'
        WHEN 11 THEN '德鲁伊'
        ELSE CONCAT('扩展职业(', `class`, ')')
    END      AS '职业',
    COUNT(*) AS '数量'
FROM `world`.`creature_template_npcbot_extras`
GROUP BY `class`
ORDER BY COUNT(*) DESC;

--  如果看到 class=0 有 500 个，那就是它了。


-- ############################################################################
--  第 2 步：设定参数
-- ############################################################################

SET @start_id := 71000;    -- 你当初设的起点
SET @count    := 500;      -- 生成数量

-- 确认这批的现状
SELECT
    COUNT(*)                                    AS '这批模板总数',
    SUM(CASE WHEN `class` = 0 THEN 1 ELSE 0 END) AS '其中class=0的(无效)',
    SUM(CASE WHEN `race`  = 0 THEN 1 ELSE 0 END) AS '其中race=0的'
FROM `world`.`creature_template_npcbot_extras`
WHERE `entry` BETWEEN @start_id AND @start_id + @count - 1;


-- ############################################################################
--  第 3 步：挑一个【真正可用】的源模板
-- ############################################################################
--
--  只列出 class 有效（非0）且职业默认启用的，避免再挑错。

SELECT
    e.`entry`   AS '可用作源模板',
    e.`class`   AS '职业ID',
    CASE e.`class`
        WHEN 1 THEN '战士'   WHEN 2 THEN '圣骑士' WHEN 3 THEN '猎人'
        WHEN 4 THEN '潜行者' WHEN 5 THEN '牧师'   WHEN 6 THEN '死亡骑士'
        WHEN 7 THEN '萨满'   WHEN 8 THEN '法师'   WHEN 9 THEN '术士'
        WHEN 11 THEN '德鲁伊'
        ELSE CONCAT('扩展(', e.`class`, ')')
    END         AS '职业',
    e.`race`    AS '种族ID',
    ct.`name`   AS '名字'
FROM `world`.`creature_template_npcbot_extras` e
JOIN `world`.`creature_template` ct ON ct.`entry` = e.`entry`
WHERE e.`class` BETWEEN 1 AND 11
  AND e.`class` <> 10                       -- 10 不是有效职业
  AND e.`entry` NOT BETWEEN @start_id AND @start_id + @count - 1
GROUP BY e.`class`, e.`entry`, e.`race`, ct.`name`
ORDER BY e.`class`, e.`entry`;

--  从上表挑几个不同职业的 entry，填到第4步。


-- ############################################################################
--  第 4 步【推荐】：把 500 个平均分配给 9 个基础职业
-- ############################################################################
--
--  比全改成同一个职业好得多 —— 游荡bot全是战士很奇怪。
--  用 entry 对 9 取模，均匀分成 9 份。
--
--  基础职业：1战士 2圣骑士 3猎人 4潜行者 5牧师 6死亡骑士 7萨满 8法师 9术士 11德鲁伊
--  （跳过 10，那不是有效职业）

UPDATE `world`.`creature_template_npcbot_extras`
SET `class` = ELT((`entry` % 10) + 1,  1, 2, 3, 4, 5, 6, 7, 8, 9, 11)
WHERE `entry` BETWEEN @start_id AND @start_id + @count - 1
  AND `class` = 0;

SELECT ROW_COUNT() AS '已修复条数';


-- ############################################################################
--  第 5 步：种族也要对（种族和职业有搭配限制）
-- ############################################################################
--
--  race=0 也是无效的。按职业给一个合法种族。
--  下面用最保守的搭配（人类/兽人都能当的职业给人类，特殊的给对应种族）。
--
--    1战士   -> 1人类     2圣骑士 -> 1人类     3猎人   -> 4暗夜精灵
--    4潜行者 -> 1人类     5牧师   -> 1人类     6死亡骑士 -> 1人类
--    7萨满   -> 2兽人     8法师   -> 1人类     9术士   -> 1人类
--   11德鲁伊 -> 4暗夜精灵

UPDATE `world`.`creature_template_npcbot_extras`
SET `race` = CASE `class`
    WHEN 3  THEN 4      -- 猎人 -> 暗夜精灵
    WHEN 7  THEN 2      -- 萨满 -> 兽人
    WHEN 11 THEN 4      -- 德鲁伊 -> 暗夜精灵
    ELSE 1              -- 其余 -> 人类
END
WHERE `entry` BETWEEN @start_id AND @start_id + @count - 1
  AND (`race` = 0 OR `race` IS NULL);

SELECT ROW_COUNT() AS '已修复种族条数';


-- ############################################################################
--  第 6 步：验证
-- ############################################################################

-- 这批的职业分布
SELECT
    `class` AS '职业ID',
    CASE `class`
        WHEN 0  THEN '【仍然无效】'
        WHEN 1  THEN '战士'   WHEN 2  THEN '圣骑士' WHEN 3  THEN '猎人'
        WHEN 4  THEN '潜行者' WHEN 5  THEN '牧师'   WHEN 6  THEN '死亡骑士'
        WHEN 7  THEN '萨满'   WHEN 8  THEN '法师'   WHEN 9  THEN '术士'
        WHEN 11 THEN '德鲁伊' ELSE CONCAT('扩展(', `class`, ')')
    END     AS '职业',
    `race`  AS '种族',
    COUNT(*) AS '数量'
FROM `world`.`creature_template_npcbot_extras`
WHERE `entry` BETWEEN @start_id AND @start_id + @count - 1
GROUP BY `class`, `race`
ORDER BY `class`;

-- 全库可用数
SELECT
    (SELECT COUNT(*) FROM `world`.`creature_template_npcbot_extras`
     WHERE `class` <> 0)                                          AS '有效模板总数',
    (SELECT COUNT(*) FROM `characters`.`characters_npcbot`)        AS '已被拥有',
    (SELECT COUNT(*) FROM `world`.`creature_template_npcbot_extras`
     WHERE `class` <> 0)
  - (SELECT COUNT(*) FROM `characters`.`characters_npcbot`)        AS '可用备用数';

-- 最终判定
SELECT
    CASE WHEN (SELECT COUNT(*) FROM `world`.`creature_template_npcbot_extras`
               WHERE `entry` BETWEEN @start_id AND @start_id + @count - 1 AND `class` = 0) = 0
         THEN '[OK] 这批模板已全部有效'
         ELSE '[问题] 还有 class=0 的，检查第4步是否执行成功'
    END AS '判定';


-- ############################################################################
--  第 7 步：改 conf 并重启
-- ############################################################################
--
--  把上面查到的"可用备用数"填进 conf（留点余量）：
--
--    worldserver.conf.d\unlimited.conf
--    NpcBot.WanderingBots.Continents.Count = 750     -- 假设可用812，填750
--
--  启动日志应看到：
--      >> Set up spawning of 750 wandering bots in XXX ms
--
--  【还要确认职业都启用了】conf 里这些必须是 1：
--      NpcBot.WanderingBots.Classes.Warrior.Enable = 1
--      NpcBot.WanderingBots.Classes.Paladin.Enable = 1
--      NpcBot.WanderingBots.Classes.Hunter.Enable = 1
--      ... 以此类推
--  没启用的职业，它的模板同样不计入统计。


-- ############################################################################
--  如果你就是想删掉重来
-- ############################################################################
--
--  【先把 conf 的 Count 调小到 300】，否则重启崩服。
--
--  DELETE FROM `world`.`creature_template_npcbot_extras`     WHERE `entry` BETWEEN 71000 AND 71499;
--  DELETE FROM `world`.`creature_template_npcbot_appearance` WHERE `entry` BETWEEN 71000 AND 71499;
--  DELETE FROM `world`.`creature_equip_template`             WHERE `CreatureID` BETWEEN 71000 AND 71499;
--  DELETE FROM `world`.`creature_template`                   WHERE `entry` BETWEEN 71000 AND 71499;
--
--  【顺序很重要】先删子表再删 creature_template。
--
--  但我不建议删 —— 上面 UPDATE 一下就能用，删了还得重新生成。
