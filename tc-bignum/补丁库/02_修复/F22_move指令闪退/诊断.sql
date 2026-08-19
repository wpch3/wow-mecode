-- ============================================================
-- F22 诊断：找出会让 .npcbot move 闪退的 entry
-- DBeaver: Alt+X 执行全部
-- 无会话变量、无多表 DELETE、纯只读
-- ============================================================

-- 【1】危险 entry 清单：characters_npcbot 有数据，但 creature 表没实体
--     对这些 entry 执行 .npcbot move 必定闪退（打完补丁后变成正常报错）
SELECT
    '1.危险entry-有数据无实体' AS 检查项,
    cn.entry,
    ct.name AS bot名字,
    cn.owner AS 主人guid,
    '对此entry执行 .npcbot move 会闪退' AS 说明
FROM characters_npcbot cn
LEFT JOIN creature c ON c.id = cn.entry
LEFT JOIN creature_template ct ON ct.entry = cn.entry
WHERE c.guid IS NULL
ORDER BY cn.entry;


-- 【2】上面那种情况的总数（0 = 安全，>0 = 有雷）
SELECT
    '2.危险entry总数' AS 检查项,
    COUNT(*) AS 数量,
    CASE WHEN COUNT(*) = 0
         THEN 'OK 没有会崩的entry'
         ELSE '有雷 打F22补丁后不再闪退' END AS 结论
FROM characters_npcbot cn
LEFT JOIN creature c ON c.id = cn.entry
WHERE c.guid IS NULL;


-- 【3】反向检查：creature 表有实体，但 characters_npcbot 没数据
--     这种 bot 启动时不会被 LoadNpcBots 认领，等于白占一条 creature 记录
SELECT
    '3.孤儿实体-有实体无数据' AS 检查项,
    c.guid,
    c.id AS entry,
    ct.name AS bot名字,
    c.map AS 地图,
    ROUND(c.position_x,1) AS x,
    ROUND(c.position_y,1) AS y
FROM creature c
JOIN creature_template ct ON ct.entry = c.id
LEFT JOIN characters_npcbot cn ON cn.entry = c.id
WHERE ct.entry BETWEEN 70000 AND 71999
  AND cn.entry IS NULL
ORDER BY c.id;


-- 【4】F21 那张表的一致性（组队成员是否都有实体）
--     这两个数字必须都是 0，否则启动仍会崩在 botdatamgr.cpp:1224
SELECT
    '4a.组队成员无bot数据' AS 检查项,
    COUNT(*) AS 必须为0
FROM characters_npcbot_group_member gm
LEFT JOIN characters_npcbot cn ON cn.entry = gm.entry
WHERE cn.entry IS NULL;

SELECT
    '4b.组队成员无creature实体' AS 检查项,
    COUNT(*) AS 必须为0
FROM characters_npcbot_group_member gm
LEFT JOIN creature c ON c.id = gm.entry
WHERE c.guid IS NULL;


-- 【5】同一 entry 在 creature 表出现多次（会让 LoadNpcBots 只取第一条）
--     botdatamgr.cpp:1104 的查询没有 LIMIT，多行时行为不确定
SELECT
    '5.entry重复spawn' AS 检查项,
    c.id AS entry,
    COUNT(*) AS 出现次数,
    GROUP_CONCAT(c.guid ORDER BY c.guid) AS 所有guid
FROM creature c
JOIN creature_template ct ON ct.entry = c.id
WHERE ct.entry BETWEEN 70000 AND 71999
GROUP BY c.id
HAVING COUNT(*) > 1
ORDER BY c.id;


-- 【6】总览
SELECT '6.总览' AS 检查项, 'characters_npcbot 行数' AS 项目, COUNT(*) AS 数量 FROM characters_npcbot
UNION ALL
SELECT '6.总览', 'creature 表中的bot实体数',  COUNT(*)
FROM creature c JOIN creature_template ct ON ct.entry = c.id
WHERE ct.entry BETWEEN 70000 AND 71999
UNION ALL
SELECT '6.总览', '组队成员记录数', COUNT(*) FROM characters_npcbot_group_member;
