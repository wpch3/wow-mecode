-- ============================================================================
--  F18  诊断：设 500 为什么只加载 160 个
-- ============================================================================
--  【已定位根因】botdatamgr.cpp:665
--    for (i = 0; i < brackets_shuffled.size() && !teamSpareBotIdsPerClass.empty();)
--                                                 ^^^ 候选池空了就静默退出
--    且 GenerateWanderingBotToSpawn(botdatamgr.cpp:373) 全函数只有 return true，
--    不存在"生成失败"这回事。
--
--    所以：spawned = min(你设置的数量, 可用模板数)
--
--  【可用模板的三个条件】botdatamgr.cpp:316-326
--    1. 在 creature_template_npcbot_extras 里
--    2. class != 0 且【该职业在 conf 里启用】
--    3. 【不在】characters_npcbot 里
--
--  这个文件帮你算出每一层各剩多少。
--
--  DBeaver：Alt+X 执行全部（全是只读查询）
-- ============================================================================


SELECT '=== 第1层：extras 表里有多少模板 ===' AS `诊断`;

SELECT
    COUNT(*)                                        AS `extras总数`,
    SUM(CASE WHEN `class` = 0 THEN 1 ELSE 0 END)    AS `class为0_无效`,
    SUM(CASE WHEN `class` <> 0 THEN 1 ELSE 0 END)   AS `class有效`
FROM `world`.`creature_template_npcbot_extras`;


SELECT '=== 第2层：按职业分布（对照你的 Classes 配置）===' AS `诊断`;

SELECT
    e.`class`  AS `职业ID`,
    CASE e.`class`
        WHEN 1  THEN '战士'   WHEN 2  THEN '圣骑士' WHEN 3  THEN '猎人'
        WHEN 4  THEN '潜行者' WHEN 5  THEN '牧师'   WHEN 6  THEN '死亡骑士'
        WHEN 7  THEN '萨满'   WHEN 8  THEN '法师'   WHEN 9  THEN '术士'
        WHEN 11 THEN '德鲁伊'
        WHEN 12 THEN '刀锋大师' WHEN 13 THEN '死亡领主' WHEN 14 THEN '大法师'
        WHEN 15 THEN '恐惧魔王' WHEN 16 THEN '斯芬克斯' WHEN 17 THEN '灵魂舞者'
        WHEN 18 THEN '暗影猎手' WHEN 19 THEN '深渊领主' WHEN 20 THEN '黑暗游侠'
        WHEN 21 THEN '亡灵巫师' WHEN 22 THEN '海巫'
        ELSE CONCAT('未知', e.`class`)
    END        AS `职业`,
    COUNT(*)   AS `模板数`,
    SUM(CASE WHEN cn.`entry` IS NULL THEN 1 ELSE 0 END) AS `未被占用`
FROM `world`.`creature_template_npcbot_extras` e
LEFT JOIN `characters`.`characters_npcbot` cn ON cn.`entry` = e.`entry`
WHERE e.`class` <> 0
GROUP BY e.`class`
ORDER BY e.`class`;

--  【对照】你的 worldserver.conf 里这一项：
--    NpcBot.WanderingBots.Continents.Classes = 1,2,3,4,5,6,7,8,9,11,...
--  【只有列在这里面的职业才会进候选池】
--  上表里「未被占用」的数字，只有职业启用的那些才算数


SELECT '=== 第3层：最终可用候选池（这就是你能生成的上限）===' AS `诊断`;

SELECT COUNT(*) AS `可用候选池`
FROM `world`.`creature_template_npcbot_extras` e
LEFT JOIN `characters`.`characters_npcbot` cn ON cn.`entry` = e.`entry`
WHERE e.`class` <> 0
  AND cn.`entry` IS NULL;

--  【重要】上面这个数字【没有】排除"职业未启用"的
--  如果它远大于 160，说明是【职业启用配置】卡住了
--  如果它约等于 160，说明就是模板不够，要批量造


SELECT '=== 第4层：被 characters_npcbot 占用的（这些不进游荡池）===' AS `诊断`;

SELECT
    COUNT(*)                                        AS `已占用总数`,
    SUM(CASE WHEN `owner` = 0 THEN 1 ELSE 0 END)    AS `无主_固定bot`,
    SUM(CASE WHEN `owner` > 0 THEN 1 ELSE 0 END)    AS `被玩家招募`
FROM `characters`.`characters_npcbot`;


SELECT '=== 第5层：entry 段分布（看还有多少空号可用）===' AS `诊断`;

SELECT
    CASE
        WHEN `entry` <  70000 THEN '70000以下'
        WHEN `entry` <  70500 THEN '70001-70499'
        WHEN `entry` <  70800 THEN '70500-70799 (宠物段)'
        WHEN `entry` <  71000 THEN '70800-70999 (动态创建段)'
        WHEN `entry` <  72000 THEN '71000-71999'
        ELSE '72000以上'
    END          AS `entry段`,
    COUNT(*)     AS `已用`,
    MIN(`entry`) AS `最小`,
    MAX(`entry`) AS `最大`
FROM `world`.`creature_template_npcbot_extras`
GROUP BY 1
ORDER BY MIN(`entry`);

--  botcommon.h:39-41
--    BOT_ENTRY_BEGIN        = 70001
--    //BOT_ENTRY_END        = 71000   <- 【已注释掉，无上限】
--    BOT_ENTRY_CREATE_BEGIN = 70800   <- 70800+ 是运行时动态创建用的，【别占】
--
--  所以批量造模板建议用 71000 以上


SELECT '=== 结论速查 ===' AS `诊断`;

SELECT
    (SELECT COUNT(*) FROM `world`.`creature_template_npcbot_extras`
      WHERE `class` <> 0)                                            AS `有效模板`,
    (SELECT COUNT(*) FROM `characters`.`characters_npcbot`)          AS `已占用`,
    (SELECT COUNT(*) FROM `world`.`creature_template_npcbot_extras` e
       LEFT JOIN `characters`.`characters_npcbot` cn ON cn.`entry` = e.`entry`
      WHERE e.`class` <> 0 AND cn.`entry` IS NULL)                   AS `理论候选池`,
    160                                                              AS `你实际生成`;

--  【怎么读这张表】
--    理论候选池 ≈ 160  -> 模板不够，用 A30 的SQL批量造到 500+
--    理论候选池 >> 160 -> 是职业启用配置卡的，改 conf 的 Classes 项
--
--  把这个结果发我，我就能确定给你哪个方案。
