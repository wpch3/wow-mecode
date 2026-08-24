-- G16：每物品库存覆盖与传说过滤只读诊断
-- 日期：2026-08-19
-- 安全性：只有 SELECT，不修改任何数据。
-- AHBot 账号：2。若以后修改 AuctionHouseBot.Account，请同步修改 c.account = 2。
-- 用户已确认最终下限：每个 eligible itemEntry 至少10条有效独立挂单。
-- SUM(ii.count) 只观察堆叠总件数，不代替“至少10条独立挂单”的验收。


-- ============================================================================
-- 结果1：当前“已出现的物品 entry”库存概况
-- 注意：完全没上架的 eligible entry 不在这里，因此本结果不能单独证明100%覆盖。
-- ============================================================================
SELECT
    COUNT(*) AS distinct_active_entries,
    SUM(x.auction_count) AS active_ahbot_rows,
    MIN(x.auction_count) AS min_copies_per_visible_entry,
    ROUND(AVG(x.auction_count), 2) AS avg_copies_per_visible_entry,
    MAX(x.auction_count) AS max_copies_per_visible_entry,
    SUM(x.auction_count = 1) AS entries_with_1,
    SUM(x.auction_count BETWEEN 2 AND 4) AS entries_with_2_to_4,
    SUM(x.auction_count BETWEEN 5 AND 9) AS entries_with_5_to_9,
    SUM(x.auction_count >= 10) AS entries_with_10_plus
FROM (
    SELECT ii.itemEntry, COUNT(*) AS auction_count
    FROM `characters`.`auctionhouse` AS a
    INNER JOIN `characters`.`item_instance` AS ii
            ON ii.guid = a.itemguid
    LEFT JOIN `characters`.`characters` AS c
           ON c.guid = a.itemowner
    WHERE (a.itemowner = 0 OR c.account = 2)
      AND a.time > UNIX_TIMESTAMP()
    GROUP BY ii.itemEntry
) AS x;


-- ============================================================================
-- 结果2：不同最低库存目标的达标率
-- 用户已选 min_10；min_5/min_12 保留为容量对照。最终要求 pct_meeting_min_10=100，
-- 但这里只统计已出现 entry，完整 eligible 池覆盖仍须由 C++ 物品池指标证明。
-- ============================================================================
SELECT
    COUNT(*) AS distinct_active_entries,
    SUM(x.auction_count >= 5) AS entries_meeting_min_5,
    ROUND(100.0 * SUM(x.auction_count >= 5) / COUNT(*), 2) AS pct_meeting_min_5,
    SUM(x.auction_count >= 10) AS entries_meeting_min_10,
    ROUND(100.0 * SUM(x.auction_count >= 10) / COUNT(*), 2) AS pct_meeting_min_10,
    SUM(x.auction_count >= 12) AS entries_meeting_min_12,
    ROUND(100.0 * SUM(x.auction_count >= 12) / COUNT(*), 2) AS pct_meeting_min_12
FROM (
    SELECT ii.itemEntry, COUNT(*) AS auction_count
    FROM `characters`.`auctionhouse` AS a
    INNER JOIN `characters`.`item_instance` AS ii
            ON ii.guid = a.itemguid
    LEFT JOIN `characters`.`characters` AS c
           ON c.guid = a.itemowner
    WHERE (a.itemowner = 0 OR c.account = 2)
      AND a.time > UNIX_TIMESTAMP()
    GROUP BY ii.itemEntry
) AS x;


-- ============================================================================
-- 结果3：所有传说品质有效 AHBot 挂单
-- class=15 且 subclass=2/5 才是用户允许保留的宠物/坐骑例外。
-- ============================================================================
SELECT
    it.entry,
    it.name,
    it.class AS item_class,
    it.subclass AS item_subclass,
    CASE
        WHEN it.class = 15 AND it.subclass = 2 THEN 'ALLOW_LEGENDARY_PET'
        WHEN it.class = 15 AND it.subclass = 5 THEN 'ALLOW_LEGENDARY_MOUNT'
        ELSE 'FORBIDDEN_LEGENDARY'
    END AS required_policy,
    COUNT(*) AS active_ahbot_rows,
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
  AND it.Quality = 5
GROUP BY it.entry, it.name, it.class, it.subclass
ORDER BY required_policy, active_ahbot_rows DESC, it.entry;


-- ============================================================================
-- 结果4：违反用户新规则的传说挂单总数
-- 最终验收目标：forbidden_entries=0 且 forbidden_rows=0。
-- ============================================================================
SELECT
    COUNT(DISTINCT it.entry) AS forbidden_entries,
    COUNT(*) AS forbidden_rows
FROM `characters`.`auctionhouse` AS a
INNER JOIN `characters`.`item_instance` AS ii
        ON ii.guid = a.itemguid
INNER JOIN `world`.`item_template` AS it
        ON it.entry = ii.itemEntry
LEFT JOIN `characters`.`characters` AS c
       ON c.guid = a.itemowner
WHERE (a.itemowner = 0 OR c.account = 2)
  AND a.time > UNIX_TIMESTAMP()
  AND it.Quality = 5
  AND NOT (it.class = 15 AND it.subclass IN (2, 5));


-- ============================================================================
-- 结果5：允许保留的传说坐骑/宠物
-- 这里可以为0；为0不代表过滤失败，只代表当前物品池没有这些例外。
-- ============================================================================
SELECT
    CASE it.subclass
        WHEN 2 THEN 'Legendary Pet/传说宠物'
        WHEN 5 THEN 'Legendary Mount/传说坐骑'
    END AS exception_type,
    COUNT(DISTINCT it.entry) AS distinct_entries,
    COUNT(*) AS active_ahbot_rows
FROM `characters`.`auctionhouse` AS a
INNER JOIN `characters`.`item_instance` AS ii
        ON ii.guid = a.itemguid
INNER JOIN `world`.`item_template` AS it
        ON it.entry = ii.itemEntry
LEFT JOIN `characters`.`characters` AS c
       ON c.guid = a.itemowner
WHERE (a.itemowner = 0 OR c.account = 2)
  AND a.time > UNIX_TIMESTAMP()
  AND it.Quality = 5
  AND it.class = 15
  AND it.subclass IN (2, 5)
GROUP BY it.subclass
ORDER BY it.subclass;


-- ============================================================================
-- 结果6：库存最低的500种已出现物品
-- 用于观察哪些 entry 只有1-2条；不包含完全没出现的 entry。
-- ============================================================================
SELECT
    it.entry,
    it.name,
    it.class AS item_class,
    it.subclass AS item_subclass,
    it.Quality AS quality,
    COUNT(*) AS active_auction_copies,
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
GROUP BY it.entry, it.name, it.class, it.subclass, it.Quality
ORDER BY active_auction_copies ASC, it.Quality DESC, it.entry
LIMIT 500;
