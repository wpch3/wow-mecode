-- ============================================================================
--  F18  诊断：500 个到底生成了没有 —— 用【服务端自己的数字】说话
-- ============================================================================
--  【为什么要跑这个】
--  我推断过两轮都不准：
--    第1轮说"候选池耗尽"     -> 你的池子 812，被打脸
--    第2轮说"500ms限速没出完" -> 你改快了还是160，又被打脸
--
--  现在不猜了。下面用三个【独立来源】交叉验证，看 160 到底是什么数字。
--
--  DBeaver：Alt+X 执行全部（全只读）
-- ============================================================================


-- ============================================================================
--  来源1：服务端启动时【自己算出来的候选池】
-- ============================================================================
--  botdatamgr.cpp:1878  maxbots = sBotGen->GetSpareBotsCount();
--  botdatamgr.cpp:1881  if (maxbots < 500) -> ASSERT(false) 崩服
--
--  【关键推论】你的服务端能起来，说明 maxbots >= 500。
--  也就是说【候选池确实够】，问题不在这。
--
--  但候选池的构建有一层 SQL 查不到的过滤（botdatamgr.cpp:318）：
--      if (c != BOT_CLASS_NONE && BotCfg::IsWanderingClassEnabled(c))
--  职业是否启用来自 conf，不在数据库里。
--
--  所以下面按职业列出，你对照 conf 自己核对。

SELECT '=== 1. 按职业统计可用模板（对照你的 conf 逐项核对）===' AS `诊断`;

SELECT
    e.`class` AS `职业ID`,
    CASE e.`class`
        WHEN 1  THEN 'Warrior 战士'      WHEN 2  THEN 'Paladin 圣骑士'
        WHEN 3  THEN 'Hunter 猎人'       WHEN 4  THEN 'Rogue 潜行者'
        WHEN 5  THEN 'Priest 牧师'       WHEN 6  THEN 'DeathKnight 死骑'
        WHEN 7  THEN 'Shaman 萨满'       WHEN 8  THEN 'Mage 法师'
        WHEN 9  THEN 'Warlock 术士'      WHEN 11 THEN 'Druid 德鲁伊'
        WHEN 12 THEN 'Blademaster 刀锋'  WHEN 13 THEN 'Dreadlord 恐惧魔王'
        WHEN 14 THEN 'Archmage 大法师'   WHEN 15 THEN 'SpellBreaker 破法者'
        WHEN 16 THEN 'ObsidianDestroyer' WHEN 17 THEN 'CryptLord 深渊领主'
        WHEN 18 THEN 'DarkRanger 黑暗游侠' WHEN 19 THEN 'Necromancer 亡灵巫师'
        WHEN 20 THEN 'SeaWitch 海巫'
        ELSE CONCAT('未知 ', e.`class`)
    END AS `职业`,
    COUNT(*) AS `可用模板数`,
    CONCAT('NpcBot.WanderingBots.Classes.',
        CASE e.`class`
            WHEN 1 THEN 'Warrior' WHEN 2 THEN 'Paladin' WHEN 3 THEN 'Hunter'
            WHEN 4 THEN 'Rogue' WHEN 5 THEN 'Priest' WHEN 6 THEN 'DeathKnight'
            WHEN 7 THEN 'Shaman' WHEN 8 THEN 'Mage' WHEN 9 THEN 'Warlock'
            WHEN 11 THEN 'Druid' WHEN 12 THEN 'Blademaster'
            WHEN 13 THEN 'Dreadlord' WHEN 14 THEN 'Archmage'
            WHEN 15 THEN 'SpellBreaker' WHEN 16 THEN 'ObsidianDestroyer'
            WHEN 17 THEN 'CryptLord' WHEN 18 THEN 'DarkRanger'
            WHEN 19 THEN 'Necromancer' WHEN 20 THEN 'SeaWitch'
            ELSE '???'
        END, '.Enable') AS `对应conf项`
FROM `world`.`creature_template_npcbot_extras` e
LEFT JOIN `characters`.`characters_npcbot` cn ON cn.`entry` = e.`entry`
WHERE e.`class` <> 0 AND cn.`entry` IS NULL
GROUP BY e.`class`
ORDER BY COUNT(*) DESC;

--  【怎么用这张表】
--  在 worldserver.conf 里逐个 grep 上面列出的 conf 项：
--      grep -n "NpcBot.WanderingBots.Classes" worldserver.conf
--  把【=0 或 =false】的职业，从上表里把它的"可用模板数"划掉
--  剩下的总和，才是服务端眼里的真实候选池


-- ============================================================================
--  来源2：等级分档能不能装下 500 个
-- ============================================================================
--  botdatamgr.cpp:402
--      desired_bracket = max(desired_bracket, GetMinLevelForBotClass(bot_class) / 10);
--
--  【这是最可疑的一点】某些职业有最低等级限制：
--      死亡骑士 最低 55 级 -> 强制进 5 档（50-59）
--      恐惧魔王 最低 60 级 -> 强制进 6 档（60-69）
--
--  而你的分档配置（默认）：
--      "20,15,15,10,10,15,15,0,0"
--       0档 1档 2档 3档 4档 5档 6档 7档 8档
--
--  如果 5档/6档 的名额（各75个）被死骑等高级职业占满，
--  而低等级档（0-4档，共350个名额）又没有足够的【低等级职业】模板，
--  就会出现"总数够但分不匹配"的情况。

SELECT '=== 2. 高等级限定职业占了多少 ===' AS `诊断`;

SELECT
    CASE
        WHEN e.`class` = 6  THEN '死骑(最低55级，强制5档)'
        WHEN e.`class` IN (13,17) THEN '恐惧魔王/深渊领主(最低60级，强制6档)'
        ELSE '普通职业(任意档)'
    END AS `等级限制`,
    COUNT(*) AS `模板数`
FROM `world`.`creature_template_npcbot_extras` e
LEFT JOIN `characters`.`characters_npcbot` cn ON cn.`entry` = e.`entry`
WHERE e.`class` <> 0 AND cn.`entry` IS NULL
GROUP BY 1;


-- ============================================================================
--  来源3：实际生成了多少（最直接）
-- ============================================================================
--  游荡bot的 entry 是运行时从 next_bot_id 递增分配的（botcommon.h:41 起 70800+）
--  这个值会存进 worldstates 表！

SELECT '=== 3. worldstates 里记录的 next_bot_id ===' AS `诊断`;

SELECT
    `entry`  AS `worldstate_entry`,
    `value`  AS `next_bot_id`,
    `value` - 70800 AS `估算已生成过的bot数`
FROM `world`.`worldstates`
WHERE `entry` = 70000;

--  botdatamgr.cpp:683
--      CharacterDatabase.PExecute("UPDATE worldstates SET value = {} WHERE entry = {}",
--                                 next_bot_id, uint32(BOT_GIVER_ENTRY));
--
--  【注意】这个值是累计的，重启不清零，所以只能看"有没有涨到500以上"
--  如果它一直停在 70960 左右（=160个），说明确实只安排了160个


-- ============================================================================
--  结论怎么读
-- ============================================================================

SELECT '=== 汇总 ===' AS `诊断`;

SELECT
    (SELECT COUNT(*) FROM `world`.`creature_template_npcbot_extras` e
       LEFT JOIN `characters`.`characters_npcbot` cn ON cn.`entry` = e.`entry`
      WHERE e.`class` <> 0 AND cn.`entry` IS NULL)   AS `SQL算的候选池`,
    (SELECT `value` FROM `world`.`worldstates` WHERE `entry` = 70000) AS `next_bot_id`,
    160                                              AS `你看到的数量`,
    500                                              AS `你配置的数量`;

--  【三种可能，按 next_bot_id 判断】
--
--  A) next_bot_id ≈ 70800+500 = 71300 左右
--     -> 500个确实安排了，是【出场慢】或【你数的方式】的问题
--
--  B) next_bot_id ≈ 70800+160 = 70960 左右
--     -> 只安排了160个，是【候选池被职业启用过滤到160】
--        -> 对照来源1的表，找出哪些职业被禁用了
--
--  C) 查不到 entry=70000 的记录
--     -> BOT_GIVER_ENTRY 不是 70000，告诉我，我重新查
