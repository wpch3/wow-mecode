-- ============================================================================
--  step48  NPCBot 界面汉化 —— 第二批（补全 150 条）
-- ============================================================================
--
--  用户反馈：「右键的交谈ui还没有完全汉化，只有管理装备和天赋还有技能
--            这三个管理类的汉化了」
--
--  【为什么之前不全】
--    step41 只做了 65 条（主菜单+专精名）。
--    实查 bot_ai.cpp 的 gossip 代码，界面实际用到 178 个文本常量，
--    所以还缺 150 条 —— 这就是"只有三个管理类是中文"的原因。
--
--  【统计方法】扫描 OnGossipHello(7700行起) 到 OnGossipSelect 结束(11177行)
--             范围内所有 BOT_TEXT_* 引用，与已汉化的取差集。
--
--  原理同 step41：bot_ai.cpp:215 LocalizedNpcText -> 查 npc_text_locale
--  执行：DBeaver 里 Alt+X 执行全部，完全限定名，不用选库。
--  执行后【重启服务端】或 .reload npc_text
-- ============================================================================

DELETE FROM `world`.`npc_text_locale`
WHERE `ID` IN (70300,70305,70306,70307,70308,70309,70312,70313,70316,70318,70319,70348,70355,70368,70435,70440,70441,70442,70443,70444,70445,70446,70452,70453,70454,70455,70456,70457,70458,70459,70460,70461,70462,70463,70464,70465,70466,70467,70468,70469,70470,70471,70472,70473,70481,70483,70484,70485,70486,70487,70488,70489,70490,70491,70492,70493,70494,70495,70496,70497,70498,70499,70500,70501,70502,70503,70504,70505,70506,70507,70508,70509,70510,70511,70512,70513,70514,70515,70516,70517,70518,70519,70520,70521,70522,70523,70524,70525,70526,70527,70528,70529,70530,70531,70532,70533,70534,70535,70536,70537,70538,70539,70540,70541,70542,70543,70553,70641,70642,70643,70644,70648,70649,70650,70652,70653,70658,70659,70660,70661,70662,70663,70664,70665,70666,70667,70668,70673,70674,70675,70676,70677,70678,70679,70680,70681,70682,70683,70684,70685,70687,70688,70690,70691,70693,70695,70696,70697,70699,70700) AND `Locale` = 'zhCN';

INSERT INTO `world`.`npc_text_locale` (`ID`, `Locale`, `Text0_0`) VALUES
(70300, 'zhCN', '受死！'),    -- BOT_TEXT_DIE | Die!
(70305, 'zhCN', '我还不能变水'),    -- BOT_TEXT_CANT_CONJURE_WATER_YET | I can't conjure water yet
(70306, 'zhCN', '我还不能变食物'),    -- BOT_TEXT_CANT_CONJURE_FOOD_YET | I can't conjure food yet
(70307, 'zhCN', '我现在做不到'),    -- BOT_TEXT_CANT_RIGHT_NOW | I can't do it right now
(70308, 'zhCN', '给你……'),    -- BOT_TEXT_HERE_YOU_GO | Here you go...
(70309, 'zhCN', '已禁用'),    -- BOT_TEXT_DISABLED | Disabled
(70312, 'zhCN', '失败'),    -- BOT_TEXT_FAILED | Failed
(70313, 'zhCN', '完成'),    -- BOT_TEXT_DONE | Done
(70316, 'zhCN', '我还不会制作治疗石！'),    -- BOT_TEXT_CANT_CREATE_HEALTHSTONE | I can't create healthstones yet!
(70318, 'zhCN', '我的技能等级不够'),    -- BOT_TEXT_SKILL_LEVEL_TOO_LOW | My skill level in not high enough
(70319, 'zhCN', '正在切换专精为'),    -- BOT_TEXT_CHANGING_MY_SPEC_TO_ | Changing my spec to
(70348, 'zhCN', '并不认可'),    -- BOT_TEXT_HIREDENY_SPHYNX | is not convinced
(70355, 'zhCN', '未知'),    -- BOT_TEXT_UNKNOWN | unknown
(70368, 'zhCN', '隐藏'),    -- BOT_TEXT_HIDDEN | hidden
(70435, 'zhCN', '名玩家'),    -- BOT_TEXT_PLAYERS | players
(70440, 'zhCN', '你确定要冒险引起'),    -- BOT_TEXT_HIREWARN_SPHYNX_1 | Are you sure you want to risk drawing
(70441, 'zhCN', '的注意吗？'),    -- BOT_TEXT_HIREWARN_SPHYNX_2 | 's attention?
(70442, 'zhCN', '<投入硬币>'),    -- BOT_TEXT_HIREOPTION_SPHYNX | <Insert Coin>
(70443, 'zhCN', '你想要引诱'),    -- BOT_TEXT_HIREWARN_DREADLORD | Do you want to entice
(70444, 'zhCN', '<尝试献上供品>'),    -- BOT_TEXT_HIREOPTION_DREADLORD | <Try to make an offering>
(70445, 'zhCN', '你想雇佣'),    -- BOT_TEXT_HIREWARN_DEFAULT | Do you wish to hire
(70446, 'zhCN', '<雇佣>'),    -- BOT_TEXT_HIREOPTION_DEFAULT | <Hire bot>
(70452, 'zhCN', '给予消耗品...'),    -- BOT_TEXT_GIVE_CONSUMABLE | Give consumable...
(70453, 'zhCN', '<创建队伍>'),    -- BOT_TEXT_CREATE_GROUP | <Create group>
(70454, 'zhCN', '<创建队伍（所有bot）>'),    -- BOT_TEXT_CREATE_GROUP_ALL | <Create group (all bots)>
(70455, 'zhCN', '<加入队伍>'),    -- BOT_TEXT_ADD_TO_GROUP | <Add to group>
(70456, 'zhCN', '<所有bot加入队伍>'),    -- BOT_TEXT_ADD_TO_GROUP_ALL | <Add all bots to group>
(70457, 'zhCN', '<移出队伍>'),    -- BOT_TEXT_REMOVE_FROM_GROUP | <Remove from group>
(70458, 'zhCN', '跟着我'),    -- BOT_TEXT_FOLLOW_ME | Follow me
(70459, 'zhCN', '原地待命'),    -- BOT_TEXT_HOLD_POSITION | Hold your position
(70460, 'zhCN', '待在这里别动'),    -- BOT_TEXT_STAY_HERE | Stay here and don't do anything
(70461, 'zhCN', '我需要食物'),    -- BOT_TEXT_MAGE_FOOD | I need food
(70462, 'zhCN', '我需要水'),    -- BOT_TEXT_MAGE_DRINK | I need water
(70463, 'zhCN', '我需要一张茶点桌'),    -- BOT_TEXT_MAGE_TABLE | I need a refreshment table
(70464, 'zhCN', '帮我开个锁'),    -- BOT_TEXT_ROGUE_PICKLOCK | Help me pick a lock
(70465, 'zhCN', '我需要你的治疗石'),    -- BOT_TEXT_WARLOCK_HEALTHSTONE | I need your your healthstone
(70466, 'zhCN', '我需要一个灵魂之井'),    -- BOT_TEXT_WARLOCK_SOULWELL | I need a soulwell
(70467, 'zhCN', '我需要你刷新毒药'),    -- BOT_TEXT_ROGUE_POISON_REFRESH | I need you to refresh poisons
(70468, 'zhCN', '<选择毒药（主手）>'),    -- BOT_TEXT_ROGUE_POISON_MH | <Choose poison (Main Hand)>
(70469, 'zhCN', '<选择毒药（副手）>'),    -- BOT_TEXT_ROGUE_POISON_OH | <Choose poison (Offhand)>
(70470, 'zhCN', '我需要你刷新武器附魔'),    -- BOT_TEXT_SHAMAN_ENCH_REFRESH | I need you to refresh enchants
(70471, 'zhCN', '<选择附魔（主手）>'),    -- BOT_TEXT_SHAMAN_ENCH_MH | <Choose enchant (Main Hand)>
(70472, 'zhCN', '<选择附魔（副手）>'),    -- BOT_TEXT_SHAMAN_ENCH_OH | <Choose enchant (Offhand)>
(70473, 'zhCN', '我需要你解除变形'),    -- BOT_TEXT_REMOVE_SHAPESHIFT | I need you to remove shapeshift
(70481, 'zhCN', '距离'),    -- BOT_TEXT_DISTANCE_SHORT | dist
(70483, 'zhCN', '<自动>'),    -- BOT_TEXT_AUTO | <Auto>
(70484, 'zhCN', '<无>'),    -- BOT_TEXT_NONE2 | <None>
(70485, 'zhCN', '随机（狡诈）'),    -- BOT_TEXT_RANDOMPET_CUNNING | Random (Cunning)
(70486, 'zhCN', '随机（凶猛）'),    -- BOT_TEXT_RANDOMPET_FEROCITY | Random (Ferocity)
(70487, 'zhCN', '随机（坚韧）'),    -- BOT_TEXT_RANDOMPET_TENACITY | Random (Tenacity)
(70488, 'zhCN', '给我看看你的背包'),    -- BOT_TEXT_SHOW_INVENTORY | Show me your inventory
(70489, 'zhCN', '自动装备'),    -- BOT_TEXT_AUTOEQUIP | Auto-equip
(70490, 'zhCN', '主手'),    -- BOT_TEXT_SLOT_MH | Main hand
(70491, 'zhCN', '副手'),    -- BOT_TEXT_SLOT_OH | Off-hand
(70492, 'zhCN', '远程'),    -- BOT_TEXT_SLOT_RH | Ranged
(70493, 'zhCN', '圣物'),    -- BOT_TEXT_SLOT_RELIC | Relic
(70494, 'zhCN', '头部'),    -- BOT_TEXT_SLOT_HEAD | Head
(70495, 'zhCN', '肩部'),    -- BOT_TEXT_SLOT_SHOULDERS | Shoulders
(70496, 'zhCN', '胸甲'),    -- BOT_TEXT_SLOT_CHEST | Chest
(70497, 'zhCN', '腰带'),    -- BOT_TEXT_SLOT_WAIST | Waist
(70498, 'zhCN', '腿部'),    -- BOT_TEXT_SLOT_LEGS | Legs
(70499, 'zhCN', '脚'),    -- BOT_TEXT_SLOT_FEET | Feet
(70500, 'zhCN', '手腕'),    -- BOT_TEXT_SLOT_WRIST | Wrist
(70501, 'zhCN', '手'),    -- BOT_TEXT_SLOT_HANDS | Hands
(70502, 'zhCN', '背部'),    -- BOT_TEXT_SLOT_BACK | Back
(70503, 'zhCN', '衬衣'),    -- BOT_TEXT_SLOT_SHIRT | Shirt
(70504, 'zhCN', '戒指1'),    -- BOT_TEXT_SLOT_FINGER1 | Finger1
(70505, 'zhCN', '戒指2'),    -- BOT_TEXT_SLOT_FINGER2 | Finger2
(70506, 'zhCN', '饰品1'),    -- BOT_TEXT_SLOT_TRINKET1 | Trinket1
(70507, 'zhCN', '饰品2'),    -- BOT_TEXT_SLOT_TRINKET2 | Trinket2
(70508, 'zhCN', '颈部'),    -- BOT_TEXT_SLOT_NECK | Neck
(70509, 'zhCN', '卸下全部装备'),    -- BOT_TEXT_UNEQUIP_ALL | Unequip all
(70510, 'zhCN', '刷新外观'),    -- BOT_TEXT_UPDATE_VISUAL | Update visual
(70511, 'zhCN', '仅外观'),    -- BOT_TEXT_VISUALONLY | visual only
(70512, 'zhCN', '已装备'),    -- BOT_TEXT_EQUIPPED | Equipped
(70513, 'zhCN', '无'),    -- BOT_TEXT_NOTHING | nothing
(70514, 'zhCN', '使用你原来的装备'),    -- BOT_TEXT_USE_OLD_EQUIPMENT | Use your old equipment
(70515, 'zhCN', '卸下它'),    -- BOT_TEXT_UNEQUIP | Unequip it
(70516, 'zhCN', '嗯……我没有什么可以给你的'),    -- BOT_TEXT_NOTHING_TO_GIVE | Hm... I have nothing to give you
(70517, 'zhCN', '采集'),    -- BOT_TEXT_GATHERING | Gathering
(70518, 'zhCN', '技能状态'),    -- BOT_TEXT_ABILITIES_STATUS | Abilities status
(70519, 'zhCN', '管理可用技能'),    -- BOT_TEXT_ALLOWED_ABILITIES | Manage allowed abilities
(70520, 'zhCN', '使用'),    -- BOT_TEXT_USE_ | Use
(70521, 'zhCN', '刷新'),    -- BOT_TEXT_UPDATE | Update
(70522, 'zhCN', '伤害'),    -- BOT_TEXT_DAMAGE | Damage
(70523, 'zhCN', '控制'),    -- BOT_TEXT_CONTROL | Control
(70524, 'zhCN', '治疗'),    -- BOT_TEXT_HEAL | Heal
(70525, 'zhCN', '其它'),    -- BOT_TEXT_OTHER | Other
(70526, 'zhCN', '发出一阵摩擦声，开始跟随'),    -- BOT_TEXT_HIRE_EMOTE_SPHYNX | makes a grinding sound and begins to follow
(70527, 'zhCN', '%s 在被原主人解雇前不会加入你'),    -- BOT_TEXT_HIREFAIL_OWNED | %s will not join you until dismissed by the owner
(70528, 'zhCN', '%s 要等你到60级才会加入'),    -- BOT_TEXT_HIREFAIL_LVL60 | %s will not join you until you are level 60
(70529, 'zhCN', '%s 要等你到55级才会加入'),    -- BOT_TEXT_HIREFAIL_LVL55 | %s will not join you until you are level 55
(70530, 'zhCN', '%s 要等你到40级才会加入'),    -- BOT_TEXT_HIREFAIL_LVL40 | %s will not join you until you are level 40
(70531, 'zhCN', '%s 要等你到20级才会加入'),    -- BOT_TEXT_HIREFAIL_LVL20 | %s will not join you until you are level 20
(70532, 'zhCN', '超出你当前等级可带的bot上限（%u）'),    -- BOT_TEXT_HIREFAIL_MAXBOTS | You exceed max npcbots for your level (%u)
(70533, 'zhCN', '你的钱不够'),    -- BOT_TEXT_HIREFAIL_COST | You don't have enough money
(70534, 'zhCN', '该职业的bot已达上限！%u / %u'),    -- BOT_TEXT_HIREFAIL_MAXCLASSBOTS | You cannot have more bots of that class! %u of %u
(70535, 'zhCN', '无法重置 %u 号栏位的装备（%s）！无法解雇bot！'),    -- BOT_TEXT_CANT_DISMISS_EQUIPMENT | Cannot reset equipment in slot %u (%s)! Cannot dismi
(70536, 'zhCN', '当前'),    -- BOT_TEXT_CURRENT | current
(70537, 'zhCN', '攻击距离'),    -- BOT_TEXT_ATTACK_DISTANCE | Attack distance
(70538, 'zhCN', '近距离攻击'),    -- BOT_TEXT_SHORT_RANGE_ATTACKS | Short range attacks
(70539, 'zhCN', '远距离攻击'),    -- BOT_TEXT_LONG_RANGE_ATTACKS | Long range attacks
(70540, 'zhCN', '精确'),    -- BOT_TEXT_EXACT | Exact
(70541, 'zhCN', '移除增益'),    -- BOT_TEXT_REMOVE_BUFF | Remove buff
(70542, 'zhCN', '修正你的能量类型'),    -- BOT_TEXT_FIX_POWER | Fix your power type
(70543, 'zhCN', '由于某种愚蠢的原因无法卸下 %s！已通过邮件寄送'),    -- BOT_TEXT_CANT_UNEQUIP_MAILING | Cannot unequip %s for some stupid reason! Sending th
(70553, 'zhCN', '银行已满'),    -- BOT_TEXT_BANK_IS_FULL | Bank is full
(70641, 'zhCN', '接战方式'),    -- BOT_TEXT_ENGAGE_BEHAVIOR | Engage behavior
(70642, 'zhCN', '延迟攻击'),    -- BOT_TEXT_DELAY_ATTACK_BY | Delay attack by
(70643, 'zhCN', '延迟治疗'),    -- BOT_TEXT_DELAY_HEALING_BY | Delay healing by
(70644, 'zhCN', '秒'),    -- BOT_TEXT_SECOND_SHORT | s
(70648, 'zhCN', '攻击角度'),    -- BOT_TEXT_ATTACK_ANGLE | Attack angle
(70649, 'zhCN', '正常'),    -- BOT_TEXT_NORMAL | Normal
(70650, 'zhCN', '避开正面AOE'),    -- BOT_TEXT_AVOID_FRONTAL_AOE | Avoid frontal AOE
(70652, 'zhCN', '你确定这能行？这最好是世界上最好的水……'),    -- BOT_TEXT_HIREWARN_SEAWITCH | Are you sure this is gonna work? It's better be the 
(70653, 'zhCN', '看起来你确实需要喝口清水。'),    -- BOT_TEXT_HIREOPTION_SEAWITCH | Seems like you could really use a drink of fresh wat
(70658, 'zhCN', '幻化...'),    -- BOT_TEXT_TRANSMOGRIFICATION | Transmogrification...
(70659, 'zhCN', '关闭战斗走位'),    -- BOT_TEXT_DISABLE_COMBAT_POSITIONIN | DISABLE combat positioning
(70660, 'zhCN', '优先目标'),    -- BOT_TEXT_PRIORITY_TARGET | Priority target
(70661, 'zhCN', 'Bot装备银行...'),    -- BOT_TEXT_BOT_GEAR_BANK | Bot gear bank...
(70662, 'zhCN', '存入物品...'),    -- BOT_TEXT_DEPOSIT_ITEMS | Deposit items...
(70663, 'zhCN', '取出物品...'),    -- BOT_TEXT_WITHDRAW_ITEMS | Withdraw items...
(70664, 'zhCN', '银行是空的'),    -- BOT_TEXT_BANK_IS_EMPTY | Bank is empty
(70665, 'zhCN', '上一页'),    -- BOT_TEXT_PREVIOUS_PAGE | Previous page
(70666, 'zhCN', '下一页'),    -- BOT_TEXT_NEXT_PAGE | Next page
(70667, 'zhCN', '你真的要花这么多钱让地穴领主重新行动吗？'),    -- BOT_TEXT_HIREWARN_CRYPTLORD | Do you really want to spend all this money to make C
(70668, 'zhCN', '我怀疑你目前的状态还能造成多少伤害，不过我愿意带领你，帮你恢复力量。'),    -- BOT_TEXT_HIREOPTION_CRYPTLORD | I doubt your ability to do much harm in your current
(70673, 'zhCN', '治疗目标血量阈值'),    -- BOT_TEXT_HEAL_TARGET_HEALTH_THRESH | Heal target health threshold
(70674, 'zhCN', '我需要一个传送门'),    -- BOT_TEXT_I_NEED_A_PORTAL | I need a portal
(70675, 'zhCN', '暴风城'),    -- BOT_TEXT_STORMWIND | Stormwind
(70676, 'zhCN', '铁炉堡'),    -- BOT_TEXT_IRONFORGE | Ironforge
(70677, 'zhCN', '达纳苏斯'),    -- BOT_TEXT_DARNASSUS | Darnassus
(70678, 'zhCN', '埃索达'),    -- BOT_TEXT_EXORDAR | Exordar
(70679, 'zhCN', '奥格瑞玛'),    -- BOT_TEXT_ORGRIMMAR | Orgrimmar
(70680, 'zhCN', '幽暗城'),    -- BOT_TEXT_UNDERCITY | Undercity
(70681, 'zhCN', '雷霆崖'),    -- BOT_TEXT_THUNDER_BLUFF | Thunder Bluff
(70682, 'zhCN', '银月城'),    -- BOT_TEXT_SILVERMOON | Silvermoon
(70683, 'zhCN', '沙塔斯'),    -- BOT_TEXT_SHATTRATH | Shattrath
(70684, 'zhCN', '达拉然'),    -- BOT_TEXT_DALARAN | Dalaran
(70685, 'zhCN', '超出你账号可带的bot上限（%u >= %u）'),    -- BOT_TEXT_HIREFAIL_MAXBOTS_ACCOUNT | You exceed max npcbots for your account (%u >= %u)
(70687, 'zhCN', '（装备银行）'),    -- BOT_TEXT___GEAR_BANK_ | (gear bank)
(70688, 'zhCN', '装备银行空间不足，无法存入 %u 件物品（%u / %u）！'),    -- BOT_TEXT_NOT_ENOUGH_GEAR_BANK_SPAC | Not enough gear bank space to store %u item(s) (%u /
(70690, 'zhCN', '创建'),    -- BOT_TEXT_CREATE | Create
(70691, 'zhCN', '删除'),    -- BOT_TEXT_DELETE | Delete
(70693, 'zhCN', '缺失'),    -- BOT_TEXT_MISSING | missing
(70695, 'zhCN', '<添加共享主人>'),    -- BOT_TEXT_ADD_OWNER | <Add owner>
(70696, 'zhCN', '警告：共享bot所有权后，对方将获得对其背包、职责和所有设置的【完全控制权】（包括再分享给别人）'),    -- BOT_TEXT_SHARED_BOT_WARN_ADD | WARNING: by sharing ownership over your bot you give
(70697, 'zhCN', '<移除共享主人>'),    -- BOT_TEXT_REMOVE_OWNER | <Remove owner>
(70699, 'zhCN', '共享给'),    -- BOT_TEXT_SHARED_WITH | Shared with
(70700, 'zhCN', '主人'); -- BOT_TEXT_OWNER | Owner

-- ============================================================================
--  验证
-- ============================================================================
SELECT COUNT(*) AS '本批已汉化条数（应为 150）'
FROM `world`.`npc_text_locale`
WHERE `Locale` = 'zhCN' AND `ID` BETWEEN 70001 AND 70700;

-- 连同 step41 的 65 条，总数应为 215
SELECT COUNT(*) AS 'zhCN汉化总条数（step41的65 + 本批150 = 215）'
FROM `world`.`npc_text_locale` WHERE `Locale` = 'zhCN';
