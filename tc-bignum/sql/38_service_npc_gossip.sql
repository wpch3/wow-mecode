-- ============================================================
--  step21 v3.4  服务 NPC 的 Gossip 菜单 + 商店货物
--
--  v3.4 修正：MenuID 是 smallint unsigned（上限 65535），
--            上一版用 96001 直接溢出报 1264。改用 63001-63007 段。
--            注意 creature_template.gossip_menu_id 是 int unsigned，
--            两张表类型不一样，必须按小的那个来。
--
--  为什么需要这个（用户实测：商人没商店、修理没修理、邮箱不是邮箱）
--
--  根因 1：修理和商店按钮【由 Gossip 菜单驱动】，不是光有 npcflag 就行
--    Player.cpp:14163  遍历 gossip_menu_option，按 OptionNpcFlag 匹配
--    Player.cpp:14170  GOSSIP_OPTION_ARMORER  = 15  对应修理
--    Player.cpp:14177  GOSSIP_OPTION_VENDOR   = 3   对应商店
--    没有 gossip_menu_option 记录 = 菜单里没有那个按钮
--
--  根因 2：商人必须有货
--    Player.cpp:14179-14184  VENDOR 选项会检查 GetVendorItems()，
--    空列表直接 canTalk=false，还会往日志刷 sql.sql 报错
--
--  根因 3：邮箱不能用 NPC 做
--    UNIT_NPC_FLAG_MAILBOX 在 3.3.5 服务端【没有任何处理代码】
--    （全库 grep 无结果）。邮箱是 GameObject，不是 Creature。
--    -> 邮箱改用 GameObject 184137，走 SummonGameObject（代码侧已处理）
--
--  银行/拍卖/旅店为什么本来就能用：
--    这三个是客户端【硬编码】按 npcflag 直接弹窗的，不走 gossip_menu_option
--
--  本文件包含 3 条语句，DBeaver 按 Alt+X 一次执行全部
-- ============================================================

-- ---------- 1. Gossip 菜单头 ----------
REPLACE INTO `world`.`gossip_menu` (`MenuID`,`TextID`,`VerifiedBuild`) VALUES
(63001, 1, 0),
(63003, 1, 0),
(63007, 1, 0);

-- ---------- 2. Gossip 选项（修理 / 商店）----------
-- OptionType 15 = GOSSIP_OPTION_ARMORER（修理），OptionNpcFlag 4096
-- OptionType  3 = GOSSIP_OPTION_VENDOR （商店），OptionNpcFlag  128
REPLACE INTO `world`.`gossip_menu_option`
(`MenuID`,`OptionID`,`OptionIcon`,`OptionText`,`OptionBroadcastTextID`,
 `OptionType`,`OptionNpcFlag`,`ActionMenuID`,`ActionPoiID`,
 `BoxCoded`,`BoxMoney`,`BoxText`,`BoxBroadcastTextID`,`VerifiedBuild`) VALUES
(63003, 0, 1, '我要修理装备', 0, 15, 4096, 0, 0, 0, 0, NULL, 0, 0),
(63003, 1, 1, '我要购买物品', 0,  3,  128, 0, 0, 0, 0, NULL, 0, 0),
(63007, 0, 1, '我要购买物品', 0,  3,  128, 0, 0, 0, 0, NULL, 0, 0);

-- ---------- 3. 商店货物 ----------
-- 修理匠(960003) 和 商人(960007) 都要有货，否则 VENDOR 选项会被隐藏
-- 卖的都是常用消耗品，maxcount=0 表示无限供应
REPLACE INTO `world`.`npc_vendor`
(`entry`,`slot`,`item`,`maxcount`,`incrtime`,`ExtendedCost`,`VerifiedBuild`) VALUES
(960003, 0, 6948,  0, 0, 0, 0),
(960003, 0, 4540,  0, 0, 0, 0),
(960003, 0, 159,   0, 0, 0, 0),
(960003, 0, 2320,  0, 0, 0, 0),
(960003, 0, 2321,  0, 0, 0, 0),
(960003, 0, 6217,  0, 0, 0, 0),
(960007, 0, 6948,  0, 0, 0, 0),
(960007, 0, 4540,  0, 0, 0, 0),
(960007, 0, 159,   0, 0, 0, 0),
(960007, 0, 1179,  0, 0, 0, 0),
(960007, 0, 4536,  0, 0, 0, 0),
(960007, 0, 2320,  0, 0, 0, 0),
(960007, 0, 2321,  0, 0, 0, 0),
(960007, 0, 6217,  0, 0, 0, 0),
(960007, 0, 5956,  0, 0, 0, 0),
(960007, 0, 7005,  0, 0, 0, 0);
