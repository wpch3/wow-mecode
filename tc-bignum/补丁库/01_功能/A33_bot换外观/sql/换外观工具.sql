-- ============================================================================
--  step51  给 bot 换样貌 / 性别 / 种族 —— 纯 SQL，不用编译
-- ============================================================================
--
--  用户需求：「再加一个给机器人能换样貌性别和种族的功能」
--
--  ==========================================================================
--  【原理】实查确认，外观由【三处】共同决定，缺一不可
--  ==========================================================================
--
--   1. world.creature_template.modelid1          <- 【你实际看到的模型】
--      SpellHandler.cpp:645  data << uint32(bot->GetDisplayId());
--      这是最关键的一处。换种族【必须换它】，否则外观不会变。
--
--   2. world.creature_template_npcbot_extras.race  <- 种族标识
--      QueryHandler.cpp:69  bdata.Race = BotMgr::GetBotPlayerRace(...)
--      影响：角色信息面板显示的种族、种族被动、部分技能判定
--
--   3. world.creature_template_npcbot_appearance   <- 脸/发型/肤色/性别
--      QueryHandler.cpp:70  bdata.Sex = appData->gender
--      SpellHandler.cpp:648-653  skin/face/hair/haircolor/features
--      影响：镜像（宝石/双子）、查询包
--
--  【只改一处的后果】
--    只改 race 不改 modelid1 -> 面板写着"暗夜精灵"但看着还是人类
--    只改 modelid1 不改 race -> 看着变了但种族属性没变
--    -> 所以下面的模板【三处一起改】
--
--  执行：DBeaver 里 Alt+X。全部完全限定名，不用选库。
--  改完执行 .reload creature_template <entry> 或重启。
-- ============================================================================

USE `world`;


-- ############################################################################
--  第 0 步：查一个 bot 现在长什么样
-- ############################################################################

SET @bot := 70001;      -- 改成你要改的 bot entry

SELECT
    ct.`entry`,
    ct.`name`                AS '名字',
    ct.`modelid1`            AS '当前模型ID',
    e.`race`                 AS '种族ID',
    e.`class`                AS '职业ID',
    a.`gender`               AS '性别(0男1女)',
    a.`skin`                 AS '肤色',
    a.`face`                 AS '脸型',
    a.`hair`                 AS '发型',
    a.`haircolor`            AS '发色',
    a.`features`             AS '胡子/耳环等'
FROM `world`.`creature_template` ct
LEFT JOIN `world`.`creature_template_npcbot_extras` e     ON e.`entry` = ct.`entry`
LEFT JOIN `world`.`creature_template_npcbot_appearance` a ON a.`entry` = ct.`entry`
WHERE ct.`entry` = @bot;


-- ############################################################################
--  第 1 步：找一个你想要的模型ID
-- ############################################################################
--
--  【最省事的办法】从别的 bot 身上抄一个现成的。
--  下面按种族+性别列出所有 bot 的模型，挑一个填到第2步。

SELECT
    e.`race`                 AS '种族ID',
    CASE e.`race`
        WHEN 1  THEN '人类'      WHEN 2  THEN '兽人'
        WHEN 3  THEN '矮人'      WHEN 4  THEN '暗夜精灵'
        WHEN 5  THEN '亡灵'      WHEN 6  THEN '牛头人'
        WHEN 7  THEN '侏儒'      WHEN 8  THEN '巨魔'
        WHEN 10 THEN '血精灵'    WHEN 11 THEN '德莱尼'
        ELSE CONCAT('其它(', e.`race`, ')')
    END                      AS '种族',
    a.`gender`               AS '性别',
    ct.`modelid1`            AS '模型ID',
    MIN(ct.`entry`)          AS '样例bot',
    COUNT(*)                 AS '有几个bot用'
FROM `world`.`creature_template` ct
JOIN `world`.`creature_template_npcbot_extras` e     ON e.`entry` = ct.`entry`
LEFT JOIN `world`.`creature_template_npcbot_appearance` a ON a.`entry` = ct.`entry`
WHERE ct.`modelid1` > 0
GROUP BY e.`race`, a.`gender`, ct.`modelid1`
ORDER BY e.`race`, a.`gender`;

--  【重要】模型ID 必须和种族+性别匹配。
--  拿一个"人类女"的模型配"牛头人"种族，客户端会显示成人类女。
--  所以【从上表里挑】最保险 —— 那些都是验证过能正常显示的组合。


-- ############################################################################
--  第 2 步：设置参数
-- ############################################################################

SET @bot       := 70001;   -- 要改的 bot
SET @new_model := 0;       -- 新模型ID（从第1步表里挑，0=不改模型）
SET @new_race  := 0;       -- 新种族ID（1人类 2兽人 3矮人 4暗夜 5亡灵
                           --           6牛头 7侏儒 8巨魔 10血精 11德莱尼，0=不改）
SET @new_gender:= 255;     -- 0=男 1=女，255=不改
SET @new_skin  := 255;     -- 肤色 0-x，255=不改
SET @new_face  := 255;     -- 脸型，255=不改
SET @new_hair  := 255;     -- 发型，255=不改
SET @new_hairc := 255;     -- 发色，255=不改
SET @new_feat  := 255;     -- 胡子/耳环，255=不改


-- 安全检查
SELECT
    CASE WHEN EXISTS(SELECT 1 FROM `world`.`creature_template` WHERE `entry` = @bot)
         THEN CONCAT('[OK] bot ', @bot, ' 存在')
         ELSE CONCAT('[停止] bot ', @bot, ' 不存在')
    END AS '检查1',
    CASE WHEN EXISTS(SELECT 1 FROM `world`.`creature_template_npcbot_extras` WHERE `entry` = @bot)
         THEN '[OK] 是 npcbot'
         ELSE '[停止] 这不是 npcbot'
    END AS '检查2';


-- ############################################################################
--  第 3 步：执行修改（三处一起改）
-- ############################################################################

-- 3.1 模型（这是你实际看到的样子）
UPDATE `world`.`creature_template`
SET `modelid1` = @new_model
WHERE `entry` = @bot AND @new_model > 0;

-- 3.2 种族
UPDATE `world`.`creature_template_npcbot_extras`
SET `race` = @new_race
WHERE `entry` = @bot AND @new_race > 0;

-- 3.3 外观明细
--     先确保有这一行（没有就插一条默认的）
INSERT INTO `world`.`creature_template_npcbot_appearance`
    (`entry`, `gender`, `skin`, `face`, `hair`, `haircolor`, `features`)
SELECT @bot, 0, 0, 0, 0, 0, 0
FROM DUAL
WHERE NOT EXISTS(
    SELECT 1 FROM `world`.`creature_template_npcbot_appearance` WHERE `entry` = @bot);

--     再逐项更新（255 表示不改这一项）
UPDATE `world`.`creature_template_npcbot_appearance`
SET `gender`    = IF(@new_gender = 255, `gender`,    @new_gender),
    `skin`      = IF(@new_skin   = 255, `skin`,      @new_skin),
    `face`      = IF(@new_face   = 255, `face`,      @new_face),
    `hair`      = IF(@new_hair   = 255, `hair`,      @new_hair),
    `haircolor` = IF(@new_hairc  = 255, `haircolor`, @new_hairc),
    `features`  = IF(@new_feat   = 255, `features`,  @new_feat)
WHERE `entry` = @bot;


-- ############################################################################
--  第 4 步：验证
-- ############################################################################

SELECT
    ct.`entry`, ct.`name` AS '名字', ct.`modelid1` AS '模型ID',
    e.`race` AS '种族', a.`gender` AS '性别',
    a.`skin` AS '肤色', a.`face` AS '脸', a.`hair` AS '发型',
    a.`haircolor` AS '发色', a.`features` AS '特征'
FROM `world`.`creature_template` ct
LEFT JOIN `world`.`creature_template_npcbot_extras` e     ON e.`entry` = ct.`entry`
LEFT JOIN `world`.`creature_template_npcbot_appearance` a ON a.`entry` = ct.`entry`
WHERE ct.`entry` = @bot;


-- ############################################################################
--  第 5 步：让它生效
-- ############################################################################
--
--  游戏里执行（把 70001 换成你的 entry）：
--      .reload creature_template 70001
--
--  【客户端缓存】改完你自己可能还看到旧样子，这是正常的：
--      QueryHandler.cpp:119  客户端问过一次 NPC 信息就存本地缓存不再问
--  解决：关客户端 -> 删 客户端目录\Cache\WDB\zhCN\creaturecache.wdb -> 重登
--
--  【已经生成在世界里的 bot】需要重启服务端才会换模型
--  （模型在 spawn 时确定）。


-- ############################################################################
--  批量：把一批 bot 全改成某个种族外观
-- ############################################################################
--
--  比如把 71000-71099 全改成暗夜精灵女：
--
--  UPDATE `world`.`creature_template`
--  SET `modelid1` = 从第1步挑一个暗夜精灵女的模型ID
--  WHERE `entry` BETWEEN 71000 AND 71099;
--
--  UPDATE `world`.`creature_template_npcbot_extras`
--  SET `race` = 4
--  WHERE `entry` BETWEEN 71000 AND 71099;
--
--  UPDATE `world`.`creature_template_npcbot_appearance`
--  SET `gender` = 1
--  WHERE `entry` BETWEEN 71000 AND 71099;
--
--  【建议】批量生成的 bot（step46 那批）默认都长一样，
--  用这个批量改成各种族混合，世界会生动很多。


-- ############################################################################
--  随机化：让一批 bot 的脸/发型各不相同
-- ############################################################################
--
--  同种族同性别的 bot，只改 face/hair/haircolor 就能显著区分。
--  取值范围因种族而异，一般 face 0-11、hair 0-11、haircolor 0-9 比较安全。
--
--  UPDATE `world`.`creature_template_npcbot_appearance`
--  SET `face`      = FLOOR(RAND() * 10),
--      `hair`      = FLOOR(RAND() * 10),
--      `haircolor` = FLOOR(RAND() * 8)
--  WHERE `entry` BETWEEN 71000 AND 71499;
--
--  【注意】这只影响【镜像和查询包】，不影响你直接看到的模型
--  （模型由 modelid1 决定）。想让外观真的千人千面，
--  需要给不同 bot 配不同的 modelid1。


-- ############################################################################
--  常见问题
-- ############################################################################
--
--  Q: 改了 race 但看着没变？
--  A: modelid1 没改。种族只是个标识，模型才是你看到的东西。
--
--  Q: 改了 modelid1 但显示成别的种族？
--  A: 模型和种族不匹配。从第1步的表里挑经过验证的组合。
--
--  Q: 性别改了但模型还是原来的性别？
--  A: 同上，男女是【不同的 modelid】。改性别要连 modelid1 一起改。
--
--  Q: 扩展职业（BM/大法师/黑暗游侠等）能改种族吗？
--  A: 它们的种族是【代码里写死的】（botmgr.cpp:1135 GetBotPlayerRace），
--     改 extras.race 对它们无效。但 modelid1 仍然可以改。
