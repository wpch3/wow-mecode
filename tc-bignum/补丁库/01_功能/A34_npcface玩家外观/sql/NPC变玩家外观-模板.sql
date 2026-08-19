-- ============================================================================
--  把 NPC 换成【玩家角色】的外观
-- ============================================================================
--  原理：玩家模型不在 CreatureDisplayInfo.dbc 里（那是生物表），
--        而在 ChrRaces.dbc，每个种族只有男/女 2 个 displayId。
--        所以不能像生物那样"填个数字完事"，要靠 creature_template_outfits 表。
--
--  服务端流程（实查）：
--    ObjectMgr.cpp:9004  启动时把 Modelid1 换成 ChrRaces 的玩家模型
--    ObjectMgr.cpp:9016  打上 UNIT_FLAG2_MIRROR_IMAGE
--    SpellHandler.cpp:604 客户端来要镜像数据时，回完整外观
--
--  DBeaver：Alt+X 执行全部
--  表名全部写全限定名，不需要 USE
--
--  【重要】改完必须【重启服务端】—— LoadCreatureOutfits() 只在启动时跑
-- ============================================================================


-- ============================================================================
--  第一部分：查资料（不改数据，先看）
-- ============================================================================

-- ---------------------------------------------------------------------------
--  1.1 查【你自己角色】的外观数值 —— 最常用，照抄自己最省事
-- ---------------------------------------------------------------------------
--  把 '你的角色名' 换成实际名字

SELECT '=== 我的角色外观（把这些数值抄到下面）===' AS `步骤`;

SELECT
    `name`         AS `角色名`,
    `race`         AS `race种族`,
    `gender`       AS `gender性别`,
    `skin`         AS `skin肤色`,
    `face`         AS `face脸型`,
    `hair`         AS `hair发型`,
    `haircolor`    AS `haircolor发色`,
    `facialstyle`  AS `facialhair胡子`
FROM `characters`.`characters`
WHERE `name` = '你的角色名';


-- ---------------------------------------------------------------------------
--  1.2 查【任意玩家】的外观（想抄别人的样子）
-- ---------------------------------------------------------------------------

-- SELECT `name`,`race`,`gender`,`skin`,`face`,`hair`,`haircolor`,`facialstyle`
-- FROM `characters`.`characters`
-- ORDER BY `guid` DESC LIMIT 20;


-- ---------------------------------------------------------------------------
--  1.3 查装备的 displayId（不是物品ID！）
-- ---------------------------------------------------------------------------
--  outfits 表的 head/chest/legs 等字段填的是【外观ID】

SELECT '=== 查装备外观ID ===' AS `步骤`;

SELECT
    `entry`        AS `物品ID`,
    `name`         AS `物品名`,
    `displayid`    AS `外观ID_填这个`,
    `InventoryType` AS `槽位类型`,
    `Quality`      AS `品质`
FROM `world`.`item_template`
WHERE `name` LIKE '%霜之哀伤%'
   OR `name` LIKE '%萨隆邪铁%'
LIMIT 20;

--  InventoryType 对照（决定填哪个字段）：
--    1=head  3=shoulders  4=body(衬衣)  5=chest/20=robe  6=waist
--    7=legs  8=feet  9=wrists  10=hands  16=back  19=tabard


-- ---------------------------------------------------------------------------
--  1.4 看现有的范例（你的库里自带两条）
-- ---------------------------------------------------------------------------

SELECT '=== 库里已有的 outfits 记录 ===' AS `步骤`;

SELECT
    o.`entry`,
    ct.`name`      AS `NPC名字`,
    o.`race`, o.`gender`, o.`skin`, o.`face`, o.`hair`, o.`haircolor`, o.`facialhair`,
    o.`chest`, o.`legs`, o.`feet`
FROM `world`.`creature_template_outfits` o
LEFT JOIN `world`.`creature_template` ct ON ct.`entry` = o.`entry`;


-- ============================================================================
--  第二部分：套用（改数据，跑之前先看完第一部分）
-- ============================================================================

-- ---------------------------------------------------------------------------
--  2.1 【最常用】把某个NPC变成你自己的样子（自动抄，不用手填）
-- ---------------------------------------------------------------------------
--  改两个地方：@npc_entry（目标NPC）和 @char_name（要抄谁）

SET @npc_entry := 70001;             -- <<< 改成你要改的 NPC entry
SET @char_name := '你的角色名';       -- <<< 改成要抄的角色名

INSERT INTO `world`.`creature_template_outfits`
    (`entry`, `race`, `gender`, `skin`, `face`, `hair`, `haircolor`, `facialhair`)
SELECT
    @npc_entry, `race`, `gender`, `skin`, `face`, `hair`, `haircolor`, `facialstyle`
FROM `characters`.`characters`
WHERE `name` = @char_name
ON DUPLICATE KEY UPDATE
    `race`       = VALUES(`race`),
    `gender`     = VALUES(`gender`),
    `skin`       = VALUES(`skin`),
    `face`       = VALUES(`face`),
    `hair`       = VALUES(`hair`),
    `haircolor`  = VALUES(`haircolor`),
    `facialhair` = VALUES(`facialhair`);

SELECT ROW_COUNT() AS `影响行数_1是新增_2是更新`;

-- 验证
SELECT '=== 套用后的结果 ===' AS `步骤`;
SELECT * FROM `world`.`creature_template_outfits` WHERE `entry` = @npc_entry;


-- ---------------------------------------------------------------------------
--  2.2 手动指定（不抄现成角色，自己填）
-- ---------------------------------------------------------------------------
--  取消注释使用

-- INSERT INTO `world`.`creature_template_outfits`
--     (`entry`, `race`, `gender`, `skin`, `face`, `hair`, `haircolor`, `facialhair`)
-- VALUES
--     (70001,    1,      0,        5,      3,      2,      7,           1)
-- ON DUPLICATE KEY UPDATE
--     `race`=VALUES(`race`), `gender`=VALUES(`gender`), `skin`=VALUES(`skin`),
--     `face`=VALUES(`face`), `hair`=VALUES(`hair`),
--     `haircolor`=VALUES(`haircolor`), `facialhair`=VALUES(`facialhair`);

--  种族 race：
--    1=人类  2=兽人  3=矮人  4=暗夜精灵  5=亡灵
--    6=牛头人 7=侏儒  8=巨魔  10=血精灵   11=德莱尼
--    （没有 9，那是被砍掉的种族）
--  性别 gender：0=男  1=女
--
--  【重要】skin/face/hair/haircolor/facialhair 的合法范围【每个种族不同】，
--  由 CharSections.dbc 决定。填超范围客户端会显示异常。
--  最保险：用 2.1 抄现成角色的数值。


-- ---------------------------------------------------------------------------
--  2.3 给它穿装备（填装备的 displayId）
-- ---------------------------------------------------------------------------

-- SET @npc_entry := 70001;
--
-- UPDATE `world`.`creature_template_outfits` SET
--     `head`      = 0,        -- 头
--     `shoulders` = 0,        -- 肩
--     `body`      = 0,        -- 衬衣
--     `chest`     = 0,        -- 胸
--     `waist`     = 59194,    -- 腰
--     `legs`      = 64674,    -- 腿
--     `feet`      = 0,        -- 脚
--     `wrists`    = 36248,    -- 腕
--     `hands`     = 0,        -- 手
--     `back`      = 0,        -- 披风
--     `tabard`    = 0         -- 战袍
-- WHERE `entry` = @npc_entry;


-- ---------------------------------------------------------------------------
--  2.4 批量：把一批 bot 都变成玩家外观（随机种族）
-- ---------------------------------------------------------------------------
--  谨慎使用。这里用 npcbot_extras 里已有的种族，保证阵营一致。

-- INSERT INTO `world`.`creature_template_outfits` (`entry`, `race`, `gender`)
-- SELECT `entry`, `race`, 0
-- FROM `world`.`creature_template_npcbot_extras`
-- WHERE `entry` BETWEEN 70001 AND 70010
-- ON DUPLICATE KEY UPDATE `race`=VALUES(`race`), `gender`=VALUES(`gender`);


-- ---------------------------------------------------------------------------
--  2.5 还原（删掉记录，变回原本的生物模型）
-- ---------------------------------------------------------------------------

-- SET @npc_entry := 70001;
-- DELETE FROM `world`.`creature_template_outfits` WHERE `entry` = @npc_entry;
--
--  注意：删了之后 Modelid1 在内存里【还是玩家模型】（启动时被改过了），
--  必须重启才会读回 creature_template 里的原始值。


-- ============================================================================
--  第三部分：必读的三个注意事项
-- ============================================================================

SELECT '=== 改完必须做的事 ===' AS `完成`;

--  1) 【必须重启服务端】
--     ObjectMgr.cpp:8959 LoadCreatureOutfits() 只在启动时调用，
--     没有 .reload 指令能单独重载这张表。
--
--  2) 【客户端缓存】
--     如果重启后还是旧样子，删掉：
--       D:\WOW\Cache\WDB\zhCN\creaturecache.wdb
--     然后重新登录客户端。
--
--  3) 【NPCBot 有自己的一套】
--     如果目标是 NPCBot（entry 70001+），它走的是
--     creature_template_npcbot_appearance 表（SpellHandler.cpp:640），
--     优先级在 outfits 之后。两张表都有记录时以 outfits 为准
--     （SpellHandler.cpp:606 先查 outfits，命中就 return）。
--
--     想改 NPCBot 外观，用 step51 的 `换外观工具.sql` 更合适。
--     这份模板主要用于【普通NPC】。
