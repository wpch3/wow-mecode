-- ============================================================================
--  修复：后台一直刷 botpet:GlobalUpdate(): no owner!
-- ============================================================================
--
--  【先说结论：不崩服，但要修】
--
--    bpet_ai.cpp:2277
--      bool bot_pet_ai::GlobalUpdate(uint32 diff)
--      {
--          if (!petOwner)
--          {
--              BOT_LOG_ERROR("entities.unit", "botpet:GlobalUpdate(): no owner!");
--              return false;        <- 提前退出，不会崩
--          }
--
--    危害：
--      1. 每帧刷一条日志 -> 日志文件暴涨、控制台刷屏
--      2. 那个宠物什么都不做，白占内存和一次遍历
--      3. 真正的错误信息被淹没在刷屏里（这个最麻烦）
--
--  【根因】petOwner 只在一处赋值：
--    bpet_ai.cpp:2208   petOwner = summoner->ToCreature();
--                       在 IsSummonedBy() 里，也就是"被主人召唤出来"时
--
--    如果一只宠物【不是被召唤出来的】（比如被当成普通bot直接生成），
--    IsSummonedBy 永远不会调用 -> petOwner 永远是 nullptr -> 每帧报错。
--
--  【最可能的来源】批量生成模板时把【宠物模板】也克隆了
--
--    CreatureData.h:216-217
--      NPCBOT     = 0x8E000000
--      NPCBOT_PET = 0x8A000000   <- 比 bot 少一位(UNUSED_26)
--
--    宠物模板如果进了 creature_template_npcbot_extras 表，
--    就会被游荡bot系统当成普通bot处理 -> 生成出来没有主人。
--
--  【重要区分：本脚本只"踢出bot池"，不删模板】
--    本脚本第3步只 DELETE `creature_template_npcbot_extras` 一张表，
--    目的是让宠物不再被当成bot生成。
--    `creature_template` 里的行【原封不动】保留。
--
--    所以跑完本脚本后，如果你想在同一个entry区间【重新生成】模板，
--    会报 "[停止] 区间已有数据" —— 那是正常的，因为主表没删。
--
--    要【彻底删除】请用：彻底清理指定区间.sql（六张表全清）
--
--  执行：Alt+X 全部执行
-- ============================================================================

USE `world`;


-- ############################################################################
--  第 1 步：找出混进来的宠物模板
-- ############################################################################

SELECT
    ct.`entry`,
    ct.`name`                            AS '名字',
    HEX(ct.`flags_extra`)                AS 'flags_extra(16进制)',
    e.`class`                            AS '职业',
    CASE
        WHEN (ct.`flags_extra` & 0x8E000000) = 0x8E000000 THEN '正常bot'
        WHEN (ct.`flags_extra` & 0x8E000000) = 0x8A000000 THEN '【宠物】不该在extras表里'
        ELSE '【异常】既不是bot也不是宠物'
    END                                  AS '类型判定'
FROM `world`.`creature_template_npcbot_extras` e
JOIN `world`.`creature_template` ct ON ct.`entry` = e.`entry`
WHERE (ct.`flags_extra` & 0x8E000000) <> 0x8E000000
ORDER BY ct.`entry`;

--  【怎么看】
--   有输出 = 这些 entry 混进了 extras 表，就是刷屏的来源
--   无输出 = 不是这个原因，看第4步的其它可能


-- ############################################################################
--  第 2 步：统计数量
-- ############################################################################

SELECT
    SUM(CASE WHEN (ct.`flags_extra` & 0x8E000000) = 0x8E000000 THEN 1 ELSE 0 END) AS '正常bot数',
    SUM(CASE WHEN (ct.`flags_extra` & 0x8E000000) = 0x8A000000 THEN 1 ELSE 0 END) AS '【宠物】混入数',
    SUM(CASE WHEN (ct.`flags_extra` & 0x8E000000) NOT IN (0x8E000000, 0x8A000000) THEN 1 ELSE 0 END) AS '其它异常数',
    COUNT(*)                                                                       AS 'extras表总数'
FROM `world`.`creature_template_npcbot_extras` e
JOIN `world`.`creature_template` ct ON ct.`entry` = e.`entry`;


-- ############################################################################
--  第 3 步：修复 —— 把宠物从 extras 表里踢出去
-- ############################################################################
--
--  【只删 extras 表的记录，不删 creature_template】
--  宠物模板本身是有用的（真正的宠物召唤要用），只是不该出现在 bot 池里。

-- 先预览会删多少
SELECT COUNT(*) AS '将要移出extras表的条数'
FROM `world`.`creature_template_npcbot_extras` e
JOIN `world`.`creature_template` ct ON ct.`entry` = e.`entry`
WHERE (ct.`flags_extra` & 0x8E000000) <> 0x8E000000;

-- 确认数字后执行
DELETE FROM `world`.`creature_template_npcbot_extras`
WHERE `entry` IN (
    SELECT `entry` FROM (
        SELECT ct.`entry`
        FROM `world`.`creature_template` ct
        WHERE (ct.`flags_extra` & 0x8E000000) <> 0x8E000000
    ) AS tmp
);

SELECT ROW_COUNT() AS '已移出条数';


-- ############################################################################
--  第 3b 步【二选一】：或者把它们【转正】成真 bot
-- ############################################################################
--
--  如果你想让这些模板真的变成可用的 bot（而不是删掉），
--  给它们补上缺失的那一位（UNUSED_26 = 0x4000000）：
--
--  【注意】这会把宠物模板改成 bot 模板。
--  如果这些 entry 是你 step46 批量克隆出来的（71000+），转正是合理的；
--  如果是上游原有的宠物模板（比如猎人宠物），【不要动】。
--
--  SET @start_id := 71000;
--  SET @count    := 500;
--
--  UPDATE `world`.`creature_template`
--  SET `flags_extra` = `flags_extra` | 0x4000000
--  WHERE `entry` BETWEEN @start_id AND @start_id + @count - 1
--    AND (`flags_extra` & 0x8E000000) = 0x8A000000;


-- ############################################################################
--  第 4 步：如果第1步没有输出，检查其它可能
-- ############################################################################

-- 4a. characters_npcbot 里有没有宠物 entry（那也会导致问题）
SELECT cn.`entry`, ct.`name`, HEX(ct.`flags_extra`) AS 'flags'
FROM `characters`.`characters_npcbot` cn
JOIN `world`.`creature_template` ct ON ct.`entry` = cn.`entry`
WHERE (ct.`flags_extra` & 0x8E000000) <> 0x8E000000;

-- 4b. creature 表（已落地的实体）里有没有宠物被当bot生成
SELECT c.`guid`, c.`id`, ct.`name`, HEX(ct.`flags_extra`) AS 'flags'
FROM `world`.`creature` c
JOIN `world`.`creature_template` ct ON ct.`entry` = c.`id`
WHERE c.`id` >= 70001
  AND (ct.`flags_extra` & 0x8E000000) = 0x8A000000
LIMIT 20;

--  4b 有输出的话，删掉那些 creature 行：
--  DELETE FROM `world`.`creature`
--  JOIN `world`.`creature_template` ct ON ct.`entry` = c.`id`
--  WHERE c.`id` >= 70001 AND (ct.`flags_extra` & 0x8E000000) = 0x8A000000;


-- ############################################################################
--  第 5 步：验证
-- ############################################################################

SELECT
    CASE WHEN NOT EXISTS(
        SELECT 1 FROM `world`.`creature_template_npcbot_extras` e
        JOIN `world`.`creature_template` ct ON ct.`entry` = e.`entry`
        WHERE (ct.`flags_extra` & 0x8E000000) <> 0x8E000000)
         THEN '[OK] extras表里全是正常bot，重启后不会再刷屏'
         ELSE '[仍有问题] 还有非bot残留，回第1步看'
    END AS '判定';

-- 顺带看看修完之后还剩多少可用
SELECT
    (SELECT COUNT(*) FROM `world`.`creature_template_npcbot_extras` WHERE `class` <> 0) AS '有效模板数',
    (SELECT COUNT(*) FROM `characters`.`characters_npcbot`)                              AS '已被拥有',
    (SELECT COUNT(*) FROM `world`.`creature_template_npcbot_extras` WHERE `class` <> 0)
  - (SELECT COUNT(*) FROM `characters`.`characters_npcbot`)                              AS '可用备用数';

--  【删掉宠物后可用数会变少】，记得回头把 conf 的
--  NpcBot.WanderingBots.Continents.Count 调到这个数字以下，否则重启崩服。


-- ############################################################################
--  临时办法：先把日志刷屏关掉
-- ############################################################################
--
--  如果一时不想动数据库，可以先把这条日志压下去。
--
--  worldserver.conf 里找到 Logger.entities.unit（没有就加）：
--      Logger.entities.unit = 4,Console Server
--                             ^ 改成 4(Error) 以上级别就不会打印这条
--
--  【但这只是掩耳盗铃】——那个宠物还在空转，真正的错误也会被一起压掉。
--  建议还是按上面第3步修掉。
