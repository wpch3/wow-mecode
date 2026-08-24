-- G16 / AHBot 共享拍卖行只读诊断
-- 适用：TrinityCore 3.3.5a，characters.auctionhouse / characters.item_instance
-- 日期：2026-08-19
-- 已回传基线：唯一39498 / 有效37307 / 过期2191 / AHBot39498 / 玩家0。
-- 用户随后确认0/0/250 conf已安装；请在补货后重跑本文件作为新结果。
-- “每entry多库存+传说只允许坐骑/宠物”另见14号只读诊断。
-- 安全性：本文件只有 SELECT，不会修改或删除任何数据。
--
-- 已知项目配置：AuctionHouseBot.Account = 2
-- 源码判定 AHBot 物品的规则：itemowner=0，或卖家角色属于 AHBot 账号。
-- 如果你以后改了 AuctionHouseBot.Account，请把下方 c.account = 2 同步改掉。


-- ============================================================================
-- 结果 1：数据库唯一挂单总数
-- 判断重点：total_rows 应等于 unique_auctions 和 unique_items。
-- ============================================================================
SELECT
    COUNT(*) AS total_rows,
    COUNT(DISTINCT a.id) AS unique_auctions,
    COUNT(DISTINCT a.itemguid) AS unique_items,
    SUM(a.time > UNIX_TIMESTAMP()) AS active_rows,
    SUM(a.time <= UNIX_TIMESTAMP()) AS expired_waiting_cleanup,
    MIN(FROM_UNIXTIME(a.time)) AS earliest_expire,
    MAX(FROM_UNIXTIME(a.time)) AS latest_expire
FROM `characters`.`auctionhouse` AS a;


-- ============================================================================
-- 结果 2：按数据库 houseid 分组，并分开 AHBot 与玩家挂单
-- houseid：2=联盟，6=部落，7=中立。
-- 注意：AllowTwoSide.Interaction.Auction=1 时，三个 houseid 在内存中仍会合并到同一个池；
-- houseid 只是数据库记录来源，不代表客户端现在有三个独立市场。
-- ============================================================================
SELECT
    a.houseid,
    CASE a.houseid
        WHEN 2 THEN 'Alliance/联盟'
        WHEN 6 THEN 'Horde/部落'
        WHEN 7 THEN 'Neutral/中立'
        ELSE CONCAT('Unknown/', a.houseid)
    END AS house_name,
    COUNT(*) AS all_rows,
    SUM(a.itemowner = 0 OR c.account = 2) AS ahbot_rows,
    SUM(a.itemowner <> 0 AND (c.account IS NULL OR c.account <> 2)) AS player_rows,
    SUM(a.time > UNIX_TIMESTAMP()) AS active_rows,
    SUM(a.time <= UNIX_TIMESTAMP()) AS expired_rows
FROM `characters`.`auctionhouse` AS a
LEFT JOIN `characters`.`characters` AS c
       ON c.guid = a.itemowner
GROUP BY a.houseid
ORDER BY a.houseid;


-- ============================================================================
-- 结果 3：全服 AHBot / 玩家挂单的唯一总数
-- 新配置 0/0/250 完成补货后，ahbot_rows 目标约为 99,250。
-- 玩家挂单不属于 AHBot 的 99,250 目标，因此分开显示。
-- ============================================================================
SELECT
    SUM(a.itemowner = 0 OR c.account = 2) AS ahbot_rows,
    SUM(a.itemowner <> 0 AND (c.account IS NULL OR c.account <> 2)) AS player_rows,
    COUNT(*) AS all_rows,
    SUM((a.itemowner = 0 OR c.account = 2) AND a.time > UNIX_TIMESTAMP()) AS active_ahbot_rows,
    SUM((a.itemowner = 0 OR c.account = 2) AND a.time <= UNIX_TIMESTAMP()) AS expired_ahbot_rows
FROM `characters`.`auctionhouse` AS a
LEFT JOIN `characters`.`characters` AS c
       ON c.guid = a.itemowner;


-- ============================================================================
-- 结果 4：按物品 Class 统计 AHBot 有效挂单
-- 这能复核你之前得到的 13 类合计 37,500，并观察 0/0/250 后的分布。
-- ============================================================================
SELECT
    it.class AS item_class,
    CASE it.class
        WHEN 0  THEN 'Consumable/消耗品'
        WHEN 1  THEN 'Container/容器'
        WHEN 2  THEN 'Weapon/武器'
        WHEN 3  THEN 'Gem/宝石'
        WHEN 4  THEN 'Armor/护甲'
        WHEN 5  THEN 'Reagent/施法材料'
        WHEN 6  THEN 'Projectile/弹药'
        WHEN 7  THEN 'Trade Goods/材料'
        WHEN 8  THEN 'Generic/通用'
        WHEN 9  THEN 'Recipe/配方'
        WHEN 10 THEN 'Money/货币'
        WHEN 11 THEN 'Quiver/箭袋'
        WHEN 12 THEN 'Quest/任务'
        WHEN 13 THEN 'Key/钥匙'
        WHEN 14 THEN 'Permanent/永久物品'
        WHEN 15 THEN 'Miscellaneous/杂项'
        WHEN 16 THEN 'Glyph/雕文'
        ELSE CONCAT('Unknown/', it.class)
    END AS class_name,
    COUNT(*) AS active_ahbot_rows,
    COUNT(DISTINCT ii.itemEntry) AS distinct_item_entries,
    SUM(ii.count) AS total_stack_units
FROM `characters`.`auctionhouse` AS a
INNER JOIN `characters`.`item_instance` AS ii
        ON ii.guid = a.itemguid
INNER JOIN `world`.`item_template` AS it
        ON it.entry = ii.itemEntry
LEFT JOIN `characters`.`characters` AS c
       ON c.guid = a.itemowner
WHERE (a.itemowner = 0 OR c.account = 2)
  AND a.time > UNIX_TIMESTAMP()
GROUP BY it.class
ORDER BY active_ahbot_rows DESC, it.class;


-- ============================================================================
-- 结果 5：按品质统计 AHBot 有效挂单
-- 新配置的理论品质预算乘以 250%：
-- Gray 5000 / White 30000 / Green 30000 / Blue 20000 /
-- Purple 12500 / Orange 1250 / Yellow 500，总计 99,250。
-- 实际值会因物品池、分配取整、过期与补货时点有少量差异。
-- ============================================================================
SELECT
    it.Quality AS quality_id,
    CASE it.Quality
        WHEN 0 THEN 'Gray/灰'
        WHEN 1 THEN 'White/白'
        WHEN 2 THEN 'Green/绿'
        WHEN 3 THEN 'Blue/蓝'
        WHEN 4 THEN 'Purple/紫'
        WHEN 5 THEN 'Orange/橙'
        WHEN 6 THEN 'Yellow/神器'
        ELSE CONCAT('Unknown/', it.Quality)
    END AS quality_name,
    COUNT(*) AS active_ahbot_rows,
    COUNT(DISTINCT ii.itemEntry) AS distinct_item_entries
FROM `characters`.`auctionhouse` AS a
INNER JOIN `characters`.`item_instance` AS ii
        ON ii.guid = a.itemguid
INNER JOIN `world`.`item_template` AS it
        ON it.entry = ii.itemEntry
LEFT JOIN `characters`.`characters` AS c
       ON c.guid = a.itemowner
WHERE (a.itemowner = 0 OR c.account = 2)
  AND a.time > UNIX_TIMESTAMP()
GROUP BY it.Quality
ORDER BY it.Quality;


-- ============================================================================
-- 结果 6：列出卖家归属，防止把玩家挂单误算成 AHBot
-- ============================================================================
SELECT
    a.itemowner,
    COALESCE(c.name, IF(a.itemowner = 0, '<owner=0 AHBot>', '<missing character>')) AS seller_name,
    c.account,
    CASE
        WHEN a.itemowner = 0 OR c.account = 2 THEN 'AHBot'
        ELSE 'Player/Other'
    END AS seller_kind,
    COUNT(*) AS auction_rows,
    SUM(a.time > UNIX_TIMESTAMP()) AS active_rows
FROM `characters`.`auctionhouse` AS a
LEFT JOIN `characters`.`characters` AS c
       ON c.guid = a.itemowner
GROUP BY a.itemowner, c.name, c.account
ORDER BY auction_rows DESC, a.itemowner;
