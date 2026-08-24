# -*- coding: utf-8 -*-
"""
gen_specdata_v3.py —— 可复现生成 CombatSpecData.cpp（F44修订）

默认输入：同目录 specdata_v3_base.json（31个专精的受控基础数据）
默认输出：补丁库/01_功能/A06_战斗辅助/源文件/CombatSpecData.cpp

支持：
  --input PATH   指定基础JSON
  --output PATH  指定输出cpp
  --check        只在内存生成并与现有输出逐字节比较，不写文件

F44保证：不依赖/tmp，不硬编码旧workspace绝对路径；生成前执行目标语义、
重复项和高风险flag静态验证。所有新增ID来自NPCBot职业AI。
"""
import argparse
import copy
import json
from pathlib import Path
import sys

SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_INPUT = SCRIPT_DIR / "specdata_v3_base.json"
DEFAULT_OUTPUT = SCRIPT_DIR.parent / "补丁库" / "01_功能" / "A06_战斗辅助" / "源文件" / "CombatSpecData.cpp"

parser = argparse.ArgumentParser(description="生成/核验CombatSpecData.cpp")
parser.add_argument("--input", type=Path, default=DEFAULT_INPUT)
parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
parser.add_argument("--check", action="store_true")
args = parser.parse_args()

specs = json.loads(args.input.read_text(encoding="utf-8"))
specs = copy.deepcopy(specs)

# ============================================================
#  职责专属技能库（ID 全部来自 NPCBot bot_*_ai.cpp）
# ============================================================
# 格式: (spellId, flags, 中文名)

TANK = {
 # 战士防护
 (1,2): [
   (355,  'SF_TAUNT',                  '嘲讽'),
   (1161, 'SF_TAUNT_AOE|SF_SELF',      '挑战怒吼'),
   (694,  'SF_TAUNT|SF_MELEE',         '挑战'),
   (72,   'SF_INTERRUPT|SF_MELEE',     '盾牌猛击'),
   (2565, 'SF_SELF',                   '盾牌格挡'),
   (23920,'SF_SELF',                   '法术反射'),
 ],
 # 圣骑防护
 (2,1): [
   (62124,'SF_TAUNT',                  '清算之手'),
   (31789,'SF_TAUNT_AOE',              '正义防御'),
   (20925,'SF_SELF|SF_BUFF_KEEP',      '神圣之盾'),
   (31935,'SF_INTERRUPT',              '复仇者之盾'),
   (64205,'SF_SELF',                   '神圣牺牲'),
 ],
 # DK 鲜血
 (6,0): [
   (56222,'SF_TAUNT',                  '黑暗命令'),
   (49576,'SF_TAUNT',                  '死亡之握'),
   (47528,'SF_INTERRUPT|SF_MELEE',     '心灵冰冻'),
   (49222,'SF_SELF|SF_BUFF_KEEP',      '白骨之盾'),
   (51052,'SF_SELF|SF_AOE',            '反魔法领域'),
   (55233,'SF_SELF',                   '吸血鬼之血'),
 ],
 # 德鲁伊熊
 (11,2): [
   (6795, 'SF_TAUNT|SF_MELEE',         '低吼'),
   (5209, 'SF_TAUNT_AOE|SF_SELF',      '挑战咆哮'),
   (5211, 'SF_INTERRUPT|SF_MELEE',     '重击'),
   (61336,'SF_SELF',                   '生存本能'),
   (22842,'SF_SELF',                   '狂暴回复'),
 ],
}

HEAL = {
 # 圣骑神圣
 (2,0): [
   (633,  'SF_HEAL|SF_HEAL_EMERG',     '圣疗术'),
   (20473,'SF_HEAL',                   '圣光震击'),
   (53563,'SF_MAINTAIN_FRIEND|SF_BUFF_KEEP','圣光道标'),
   (19750,'SF_HEAL',                   '圣光闪现'),
   (635,  'SF_HEAL|SF_NO_MOVE',        '圣光术'),
   (53601,'SF_MAINTAIN_FRIEND|SF_BUFF_KEEP','神圣壁垒'),
   (1022, 'SF_HEAL|SF_HEAL_EMERG',     '保护之手'),
   (6940, 'SF_HEAL|SF_HEAL_EMERG',     '牺牲之手'),
   (1044, 'SF_HEAL',                   '自由之手'),
 ],
 # 牧师戒律
 (5,0): [
   (47788,'SF_HEAL|SF_HEAL_EMERG',     '守护之魂'),
   (33206,'SF_HEAL|SF_HEAL_EMERG',     '痛苦压制'),
   (17,   'SF_HEAL|SF_HOT',            '真言术盾'),
   (47540,'SF_HEAL',                   '苦修'),
   (33076,'SF_HEAL|SF_HOT',            '愈合祷言'),
   (2061, 'SF_HEAL|SF_NO_MOVE',        '快速治疗'),
   (139,  'SF_HEAL|SF_HOT',            '恢复'),
   (2060, 'SF_HEAL|SF_NO_MOVE',        '强效治疗'),
   (596,  'SF_HEAL_AOE|SF_NO_MOVE',    '治疗祷言'),
 ],
 # 牧师神圣
 (5,1): [
   (47788,'SF_HEAL|SF_HEAL_EMERG',     '守护之魂'),
   (34861,'SF_HEAL_AOE',               '治疗之环'),
   (64843,'SF_HEAL_AOE|SF_NO_MOVE',    '神圣赞美诗'),
   (33076,'SF_HEAL|SF_HOT',            '愈合祷言'),
   (139,  'SF_HEAL|SF_HOT',            '恢复'),
   (2061, 'SF_HEAL|SF_NO_MOVE',        '快速治疗'),
   (2060, 'SF_HEAL|SF_NO_MOVE',        '强效治疗'),
   (596,  'SF_HEAL_AOE|SF_NO_MOVE',    '治疗祷言'),
 ],
 # 萨满恢复
 (7,2): [
   (16188,'SF_HEAL_EMERG|SF_SELF',     '自然迅捷'),
   (55198,'SF_HEAL_EMERG|SF_SELF',     '潮汐之力'),
   (61295,'SF_HEAL|SF_HOT',            '激流'),
   (1064, 'SF_HEAL_AOE|SF_NO_MOVE',    '治疗链'),
   (8004, 'SF_HEAL|SF_NO_MOVE',        '次级治疗波'),
   (331,  'SF_HEAL|SF_NO_MOVE',        '治疗波'),
   (974,  'SF_MAINTAIN_FRIEND|SF_BUFF_KEEP','大地之盾'),
   # 图腾不是施法者Aura，不能按“缺Aura”反复投放；每次进战斗只部署一次。
   (5394, 'SF_SELF|SF_DEPLOYABLE',     '治疗之泉图腾'),
   (16190,'SF_SELF|SF_MANA_LOW',       '海潮图腾'),
 ],
 # 德鲁伊恢复
 (11,3): [
   (17116,'SF_HEAL_EMERG|SF_SELF',     '自然迅捷'),
   (18562,'SF_HEAL|SF_HEAL_EMERG',     '迅捷治愈'),
   (33763,'SF_HEAL|SF_HOT',            '生命之花'),
   (774,  'SF_HEAL|SF_HOT',            '回春术'),
   (48438,'SF_HEAL_AOE',               '野性成长'),
   (50464,'SF_HEAL|SF_NO_MOVE',        '滋养'),
   (8936, 'SF_HEAL|SF_NO_MOVE',        '愈合'),
   (5185, 'SF_HEAL|SF_NO_MOVE',        '治疗之触'),
   (740,  'SF_HEAL_AOE|SF_NO_MOVE',    '宁静'),
 ],
}

# 非治疗专精也给一点自保治疗（当治疗职责时用，或血少自救）
HEAL_MINOR = {
 (2,1): [(633,'SF_HEAL|SF_HEAL_EMERG','圣疗术'), (19750,'SF_HEAL|SF_NO_MOVE','圣光闪现')],
 (2,2): [(633,'SF_HEAL|SF_HEAL_EMERG','圣疗术'), (19750,'SF_HEAL|SF_NO_MOVE','圣光闪现')],
 (5,2): [(2061,'SF_HEAL|SF_NO_MOVE','快速治疗'), (17,'SF_HEAL|SF_HOT','真言术盾')],
 (7,0): [(8004,'SF_HEAL|SF_NO_MOVE','次级治疗波')],
 (7,1): [(8004,'SF_HEAL|SF_NO_MOVE','次级治疗波')],
 (11,0):[(8936,'SF_HEAL|SF_NO_MOVE','愈合'), (774,'SF_HEAL|SF_HOT','回春术')],
 (11,1):[(774,'SF_HEAL|SF_HOT','回春术')],
}

# 驱散 / 解控 / 打断 / 回蓝
UTIL = {
 1: [(6552,'SF_INTERRUPT|SF_MELEE','拳击'), (18499,'SF_FREE_SELF|SF_SELF','狂暴之怒')],
 2: [(4987,'SF_DISPEL_FRIEND','清洁术'), (1152,'SF_DISPEL_FRIEND','净化术'),
     (642,'SF_FREE_SELF|SF_SELF','圣盾术'), (1044,'SF_FREE_SELF|SF_SELF','自由之手'),
     (54428,'SF_SELF|SF_MANA_LOW','神圣恳求')],
 3: [(34490,'SF_INTERRUPT','沉默射击'), (19801,'SF_DISPEL_FRIEND','宁神射击'),
     (5384,'SF_FREE_SELF|SF_SELF','假死'), (781,'SF_FREE_SELF|SF_SELF','逃脱'),
     # 蝰蛇守护只在低蓝时切换；误导是战斗仇恨转移，不是假装长期Aura。
     (34074,'SF_SELF|SF_MANA_LOW|SF_EXCLUSIVE_BUFF','蝰蛇守护'),
     (34477,'SF_MAINTAIN_FRIEND|SF_COMBAT_UTILITY','误导')],
 4: [(1766,'SF_INTERRUPT|SF_MELEE','脚踢'), (31224,'SF_FREE_SELF|SF_SELF','暗影斗篷'),
     (5277,'SF_SELF','闪避'),
     (57934,'SF_MAINTAIN_FRIEND|SF_COMBAT_UTILITY','嫁祸诀窍')],
 5: [(527,'SF_DISPEL_FRIEND','驱散魔法'), (528,'SF_DISPEL_FRIEND','驱除疾病'),
     (552,'SF_DISPEL_FRIEND','消除疾病'), (32375,'SF_DISPEL_FRIEND|SF_AOE','群体驱散'),
     (15487,'SF_INTERRUPT','沉默'), (6346,'SF_MAINTAIN_FRIEND|SF_BUFF_KEEP','防护恐惧结界'),
     (64901,'SF_SELF|SF_MANA_LOW','希望圣歌')],
 6: [(47528,'SF_INTERRUPT|SF_MELEE','心灵冰冻'), (48707,'SF_FREE_SELF|SF_SELF','反魔法护罩'),
     (48792,'SF_FREE_SELF|SF_SELF','冰封之韧')],
 7: [(57994,'SF_INTERRUPT','风剪'), (526,'SF_DISPEL_FRIEND','净化毒素'),
     (51886,'SF_DISPEL_FRIEND','净化精神'), (370,'SF_DISPEL_FRIEND','净化'),
     (8143,'SF_FREE_SELF|SF_SELF','战栗图腾')],
 8: [(2139,'SF_INTERRUPT','法术反制'), (475,'SF_DISPEL_FRIEND','解除诅咒'),
     (45438,'SF_FREE_SELF|SF_SELF','寒冰屏障'), (12051,'SF_SELF|SF_MANA_LOW','唤醒')],
 9: [(1454,'SF_SELF|SF_MANA_LOW','生命分流'), (18220,'SF_SELF|SF_MANA_LOW','黑暗契约')],
 11:[(2782,'SF_DISPEL_FRIEND','解除诅咒'), (8946,'SF_DISPEL_FRIEND','消毒术'),
     (2893,'SF_DISPEL_FRIEND','驱毒术'), (22812,'SF_SELF','树皮术'),
     (29166,'SF_SELF|SF_MANA_LOW','激活')],
}


# ============================================================
#  v3.3 新增：FILLER —— 填充技，专治"连招有空隙"
# ============================================================
# 用户反馈：
#   「输出连招和技能栏也要有其他专精的技能，比如圣骑士正义之锤」
#   「坦克差dps连招，放完防御技能就站在这里，不要让连招有空隙」
#
# 思路：每个职业准备一组【低门槛、无脑能放】的技能，
#       接在主循环最后。前面的高优先级技能都在 CD 时，
#       用这些填 GCD，保证不站桩发呆。
# ID 同样全部来自 NPCBot bot_*_ai.cpp。

FILLER = {
 # 战士：随时能放的
 1: [(78,   'SF_MELEE',            '英勇打击'),
     (845,  'SF_MELEE|SF_AOE',     '顺劈斩'),
     (57755,'SF_NONE',             '英勇投掷'),
     (772,  'SF_MELEE|SF_DEBUFF_KEEP','撕裂'),
     (1715, 'SF_MELEE|SF_DEBUFF_KEEP','断筋')],

 # 圣骑士：正义之锤/十字军打击/审判/驱邪术（用户点名要的）
 2: [(53595,'SF_MELEE|SF_AOE',     '正义之锤'),
     (35395,'SF_MELEE',            '十字军打击'),
     (20271,'SF_MELEE',            '审判'),
     (879,  'SF_NONE',             '驱邪术'),
     (26573,'SF_SELF|SF_AOE',      '奉献'),
     (2812, 'SF_AOE',              '神圣愤怒'),
     (24275,'SF_EXECUTE',          '复仇之怒锤')],

 # 猎人
 3: [(3044, 'SF_NONE',             '奥术射击'),
     (56641,'SF_NO_MOVE',          '稳固射击'),
     (2973, 'SF_MELEE',            '猛禽一击'),
     (1978, 'SF_DEBUFF_KEEP',      '毒蛇钉刺'),
     (2643, 'SF_AOE',              '多重射击'),
     (53351,'SF_EXECUTE',          '杀戮射击'),
     (1130, 'SF_DEBUFF_KEEP',      '猎人印记')],

 # 盗贼
 4: [(1752, 'SF_MELEE|SF_COMBO_BUILD','邪恶攻击'),
     (2098, 'SF_MELEE|SF_COMBO_FINISH','刺骨'),
     (51723,'SF_MELEE|SF_AOE',     '刀扇'),
     (8647, 'SF_MELEE|SF_COMBO_FINISH','破甲'),
     (1943, 'SF_MELEE|SF_COMBO_FINISH','割裂'),
     (5171, 'SF_MELEE|SF_COMBO_FINISH','切割'),
     (53,   'SF_MELEE|SF_COMBO_BUILD','背刺'),
     (16511,'SF_MELEE|SF_COMBO_BUILD','出血')],

 # 牧师
 5: [(585,  'SF_NO_MOVE',          '惩击'),
     (589,  'SF_DEBUFF_KEEP',      '暗言术痛'),
     (8092, 'SF_NONE',             '心灵震爆'),
     (14914,'SF_NONE',             '神圣之火'),
     (15407,'SF_NO_MOVE',          '精神鞭笞'),
     (2944, 'SF_DEBUFF_KEEP',      '噬灵疫病')],

 # 死骑：坦克专精也要有输出填充
 6: [(45902,'SF_MELEE',            '鲜血打击'),
     (45477,'SF_MELEE|SF_DEBUFF_KEEP','冰冷触摸'),
     (45462,'SF_MELEE|SF_DEBUFF_KEEP','瘟疫打击'),
     (47541,'SF_NONE',             '死亡缠绕'),
     (56815,'SF_MELEE',            '符文打击'),
     (48721,'SF_MELEE|SF_AOE',     '血液沸腾')],

 # 萨满
 7: [(403,  'SF_NO_MOVE',          '闪电箭'),
     (8042, 'SF_NONE',             '大地震击'),
     (8050, 'SF_DEBUFF_KEEP',      '烈焰震击'),
     (421,  'SF_AOE|SF_NO_MOVE',   '闪电链'),
     (8056, 'SF_NONE',             '冰霜震击'),
     (1535, 'SF_AOE|SF_SELF',      '火焰新星')],

 # 法师
 8: [(2136, 'SF_NONE',             '火焰冲击'),
     (116,  'SF_NO_MOVE',          '寒冰箭'),
     (133,  'SF_NO_MOVE',          '火球术'),
     (2948, 'SF_NO_MOVE',          '灼烧'),
     (30455,'SF_NONE',             '冰枪术'),
     (10,   'SF_AOE|SF_NO_MOVE',   '暴风雪'),
     (122,  'SF_AOE|SF_SELF',      '冰霜新星')],

 # 术士
 9: [(686,  'SF_NO_MOVE',          '暗影箭'),
     (172,  'SF_DEBUFF_KEEP',      '腐蚀术'),
     (348,  'SF_DEBUFF_KEEP',      '献祭'),
     (980,  'SF_DEBUFF_KEEP',      '痛苦诅咒'),
     (29722,'SF_NO_MOVE',          '烧尽'),
     (5676, 'SF_NO_MOVE',          '灼热之痛'),
     (17877,'SF_EXECUTE',          '暗影灼烧')],

 # 德鲁伊：熊坦也要有输出填充
 11:[(8921, 'SF_DEBUFF_KEEP',      '月火术'),
     (5176, 'SF_NO_MOVE',          '愤怒'),
     (6807, 'SF_MELEE',            '槌击'),
     (779,  'SF_MELEE|SF_AOE',     '横扫(熊)'),
     (1082, 'SF_MELEE|SF_COMBO_BUILD','爪击')],
}


# ============================================================
#  v3.5：EXTRA_BUFF —— 补齐各职业遗漏的增益
# ============================================================
# 补齐长期增益。F44R1 的分类原则：
#   * 真正Aura才允许自动维护，并且只在Aura完全消失后补；
#   * 毒药/武器临时附魔没有Unit Aura，保留到技能栏但标记为手动；
#   * 图腾按“每次进入战斗部署一次”管理，不再按缺Aura循环施放；
#   * 误导/嫁祸属于战斗仇恨工具，进入UTIL而不是伪装成长效buff；
#   * 同一家族由运行时只选一个，祝福按目标职业/坦克职责选一个。
# ID 全部来自 NPCBot bot_*_ai.cpp。

EXTRA_BUFF = {
 # 战士：警戒锁定合法坦克；普通最低血变化不得迁移。
 (1,0): [(50720,'SF_MAINTAIN_FRIEND|SF_BUFF_KEEP','警戒')],
 (1,1): [(50720,'SF_MAINTAIN_FRIEND|SF_BUFF_KEEP','警戒')],
 (1,2): [(50720,'SF_MAINTAIN_FRIEND|SF_BUFF_KEEP','警戒')],

 # 圣骑：同一目标只选一种祝福；圣印每专精只保留一个首选。
 (2,0): [(25894,'SF_RAID_BUFF|SF_BUFF_KEEP|SF_EXCLUSIVE_BUFF','强效智慧祝福'),
         (25898,'SF_RAID_BUFF|SF_BUFF_KEEP|SF_EXCLUSIVE_BUFF','强效王者祝福'),
         (20165,'SF_SELF|SF_BUFF_KEEP|SF_EXCLUSIVE_BUFF','光明圣印')],
 (2,1): [(25898,'SF_RAID_BUFF|SF_BUFF_KEEP|SF_EXCLUSIVE_BUFF','强效王者祝福'),
         (25782,'SF_RAID_BUFF|SF_BUFF_KEEP|SF_EXCLUSIVE_BUFF','强效力量祝福'),
         (21084,'SF_SELF|SF_BUFF_KEEP|SF_EXCLUSIVE_BUFF','正义圣印')],
 (2,2): [(25898,'SF_RAID_BUFF|SF_BUFF_KEEP|SF_EXCLUSIVE_BUFF','强效王者祝福')],

 # 盗贼毒药需要物品目标；自动Unit施法不可安全维护，保留技能栏且禁止自动重放。
 (4,0): [(13219,'SF_SELF|SF_MANUAL','致伤毒药')],
 (4,1): [(13219,'SF_SELF|SF_MANUAL','致伤毒药')],
 (4,2): [(13219,'SF_SELF|SF_MANUAL','致伤毒药')],

 # 牧师：三个团队长效增益可并存；恐惧结界稳定锁定一个友方。
 (5,0): [(976,'SF_RAID_BUFF|SF_BUFF_KEEP','暗影防护'),
         (6346,'SF_MAINTAIN_FRIEND|SF_BUFF_KEEP','防护恐惧结界')],
 (5,1): [(976,'SF_RAID_BUFF|SF_BUFF_KEEP','暗影防护'),
         (6346,'SF_MAINTAIN_FRIEND|SF_BUFF_KEEP','防护恐惧结界')],
 (5,2): [(976,'SF_RAID_BUFF|SF_BUFF_KEEP','暗影防护'),
         (14752,'SF_RAID_BUFF|SF_BUFF_KEEP','神圣之灵')],

 # 死骑：白骨之盾是可检测Aura，正常到期后才补。
 (6,0): [(49222,'SF_SELF|SF_BUFF_KEEP','白骨之盾')],
 (6,2): [(49222,'SF_SELF|SF_BUFF_KEEP','白骨之盾')],

 # 萨满：武器附魔手动；图腾按元素互斥且每场战斗只部署一次。
 (7,0): [(8024,'SF_SELF|SF_MANUAL','火舌武器'),
         (3738,'SF_SELF|SF_DEPLOYABLE|SF_EXCLUSIVE_BUFF','空气之怒图腾'),
         (5675,'SF_SELF|SF_DEPLOYABLE|SF_EXCLUSIVE_BUFF','法力之泉图腾')],
 (7,1): [(8227,'SF_SELF|SF_DEPLOYABLE|SF_EXCLUSIVE_BUFF','火舌图腾')],
 (7,2): [(8512,'SF_SELF|SF_DEPLOYABLE|SF_EXCLUSIVE_BUFF','风怒图腾')],

 # 强化/抑制魔法有利有弊，保留到技能栏但不全团自动施放；魔法专注稳定锁定。
 (8,0): [(1008,'SF_RAID_BUFF|SF_BUFF_KEEP|SF_MANUAL|SF_EXCLUSIVE_BUFF','强化法术'),
         (54646,'SF_MAINTAIN_FRIEND|SF_BUFF_KEEP','魔法专注')],
 (8,1): [(1008,'SF_RAID_BUFF|SF_BUFF_KEEP|SF_MANUAL|SF_EXCLUSIVE_BUFF','强化法术'),
         (54646,'SF_MAINTAIN_FRIEND|SF_BUFF_KEEP','魔法专注')],
 (8,2): [(604,'SF_RAID_BUFF|SF_BUFF_KEEP|SF_MANUAL|SF_EXCLUSIVE_BUFF','法术抑制'),
         (54646,'SF_MAINTAIN_FRIEND|SF_BUFF_KEEP','魔法专注')],

 # 暗影防护结界属于受伤时防御技，已由defensive处理；不当长期buff维护。
 (9,0): [(5697,'SF_RAID_BUFF|SF_BUFF_KEEP','无尽呼吸')],

 # 德鲁伊团队长效增益可并存。
 (11,0): [(467,'SF_RAID_BUFF|SF_BUFF_KEEP','尖刺')],
 (11,1): [(1126,'SF_RAID_BUFF|SF_BUFF_KEEP','野性印记')],
 (11,2): [(467,'SF_RAID_BUFF|SF_BUFF_KEEP','尖刺')],
 (11,3): [(467,'SF_RAID_BUFF|SF_BUFF_KEEP','尖刺')],
}

ROLE_OF = {'坦克':'ROLE_TANK', '治疗':'ROLE_HEALER'}

KNOWN_FLAGS = {
    'SF_NONE', 'SF_MELEE', 'SF_EXECUTE', 'SF_AOE', 'SF_NO_MOVE', 'SF_SELF',
    'SF_DEBUFF_KEEP', 'SF_BUFF_KEEP', 'SF_COMBO_FINISH', 'SF_COMBO_BUILD',
    'SF_HEAL', 'SF_HEAL_AOE', 'SF_HEAL_EMERG', 'SF_HOT', 'SF_TAUNT',
    'SF_TAUNT_AOE', 'SF_DISPEL_FRIEND', 'SF_FREE_SELF', 'SF_RAID_BUFF',
    'SF_MANA_LOW', 'SF_INTERRUPT', 'SF_MAINTAIN_FRIEND', 'SF_FRIEND',
    'SF_EXCLUSIVE_BUFF', 'SF_MANUAL', 'SF_DEPLOYABLE', 'SF_COMBAT_UTILITY'
}

def flag_set(expr):
    return {x.strip() for x in expr.split('|') if x.strip()}

def validate_item(where, item, is_buff_group=False):
    sid, expr, name = item
    flags = flag_set(expr)
    unknown = flags - KNOWN_FLAGS
    if unknown:
        raise ValueError(f'{where}: spell={sid} {name} unknown flags={sorted(unknown)}')
    if 'SF_NONE' in flags and len(flags) != 1:
        raise ValueError(f'{where}: spell={sid} SF_NONE cannot be combined')
    if 'SF_DEBUFF_KEEP' in flags and flags & {'SF_HEAL','SF_HEAL_AOE','SF_HOT','SF_RAID_BUFF','SF_MAINTAIN_FRIEND'}:
        raise ValueError(f'{where}: spell={sid} friendly skill marked as debuff')
    if 'SF_MAINTAIN_FRIEND' in flags and flags & {'SF_SELF','SF_RAID_BUFF','SF_TAUNT','SF_TAUNT_AOE','SF_DISPEL_FRIEND'}:
        raise ValueError(f'{where}: spell={sid} stable-friendly target conflict')
    if 'SF_FRIEND' in flags and flags & {'SF_SELF','SF_RAID_BUFF','SF_MAINTAIN_FRIEND','SF_DEBUFF_KEEP'}:
        raise ValueError(f'{where}: spell={sid} dynamic-friendly target conflict')
    if 'SF_RAID_BUFF' in flags and 'SF_BUFF_KEEP' not in flags:
        raise ValueError(f'{where}: spell={sid} raid buff must be maintained')
    if 'SF_MANUAL' in flags and 'SF_DEPLOYABLE' in flags:
        raise ValueError(f'{where}: spell={sid} cannot be manual and deployable')
    if 'SF_DEPLOYABLE' in flags and 'SF_SELF' not in flags:
        raise ValueError(f'{where}: spell={sid} deployable must use self cast semantics')
    if 'SF_COMBAT_UTILITY' in flags and 'SF_MAINTAIN_FRIEND' not in flags:
        raise ValueError(f'{where}: spell={sid} combat utility requires a stable friendly target')
    if is_buff_group and not flags & {'SF_SELF','SF_RAID_BUFF','SF_MAINTAIN_FRIEND'}:
        raise ValueError(f'{where}: enemy/dynamic-target skill cannot be in opener buffs')

def validate_spec(cls, idx, groups):
    names = ('rotation','burst','defensive','buffs','tankKit','healKit','utility','filler')
    for group_name, items in zip(names, groups):
        seen_in_group = set()
        for item in items:
            validate_item(f'cls={cls} spec={idx} group={group_name}', item, group_name == 'buffs')
            key = (item[0], item[1])
            if key in seen_in_group:
                raise ValueError(f'cls={cls} spec={idx} group={group_name}: duplicate spell/flags {key}')
            seen_in_group.add(key)

def fmt(items, indent=8):
    if not items: return ''
    sp=' '*indent
    out=[]
    for sid, fl, cn in items:
        out.append(f'{sp}{{ {sid}, {fl}, "{cn}" }},')
    return '\n'.join(out)

hdr = '''/*
 * ============================================================================
 *  CombatSpecData.cpp —— 31 个专精 x 3 职责 x 6 场景（v3）
 * ============================================================================
 *
 *  ID 溯源：src/server/game/AI/NpcBots/bot_<class>_ai.cpp 的 enum 常量。
 *  这些是 NPCBot 实际在用的 rank-1 ID，运行时再由 rank 链升到玩家会的最高阶。
 *
 *  v3 新增（每个专精多 3 组）：
 *    tankKit —— 当坦克时用：嘲讽、群嘲、打断、减伤
 *    healKit —— 当治疗时用：单奶、群奶、HOT、救命大招
 *    utility —— 驱散队友负面、自我解控、打断、回蓝
 *
 *  本文件由 tools/gen_specdata_v3.py 生成，不要手改。
 * ============================================================================
 */

#include "CombatSpecData.h"

namespace CombatSpec
{

char const* RoleName(uint8 r)
{
    switch (r)
    {
        case ROLE_TANK:   return "坦克";
        case ROLE_DPS:    return "输出";
        case ROLE_HEALER: return "治疗";
        default:          return "自动";
    }
}

char const* SceneName(uint8 s)
{
    switch (s)
    {
        case SCENE_QUEST:   return "做任务/刷怪";
        case SCENE_FARM:    return "聚怪刷材料";
        case SCENE_DUNGEON: return "5人副本";
        case SCENE_RAID:    return "团本";
        case SCENE_MYTHIC:  return "高级团本";
        default:            return "自动识别";
    }
}

/*
 * 场景调参表 —— 这是「设置了就能打本」的关键。
 *
 * 越到高难度：AOE 门槛越高（不乱 AOE 拉仇恨）、
 *             保命/治疗血线越高（提前救不等死）、
 *             增益必须严格维持、爆发要留给 BOSS。
 */
SceneTuning const& GetTuning(uint8 scene)
{
    static SceneTuning const t[SCENE_MAX] =
    {
        // aoeTh  defHp  healSelf  healTgt  emerg  keepBuff  saveBurst  desc
        {  2,     35,    70,       75,      30,    true,     false, "自动识别" },
        {  2,     30,    60,       65,      25,    false,    false, "做任务/刷怪：怪少血薄，2个就AOE，不留保命" },
        {  2,     35,    65,       70,      30,    false,    false, "聚怪刷材料：AOE门槛最低，优先群伤" },
        {  3,     45,    75,       80,      35,    true,     false, "5人副本：保命提前，增益维持" },
        {  3,     55,    80,       85,      40,    true,     true,  "团本：血线拉高，爆发留给BOSS" },
        {  4,     65,    85,       90,      45,    true,     true,  "高级团本：最保守，提前救人，绝不浪费爆发" },
    };
    return t[scene < SCENE_MAX ? scene : 0];
}

std::vector<SpecInfo> const& GetAllSpecs()
{
    static std::vector<SpecInfo> tbl =
    {
'''

body=[]
for sp in specs:
    cls, idx, name, role = sp['cls'], sp['idx'], sp['name'], sp['role']
    g = copy.deepcopy(sp['groups'])
    while len(g) < 4:
        g.append([])
    drole = ROLE_OF.get(role, 'ROLE_DPS')
    tank = TANK.get((cls,idx), [])
    heal = HEAL.get((cls,idx), HEAL_MINOR.get((cls,idx), []))
    util = UTIL.get(cls, [])
    fill = FILLER.get(cls, [])
    # v3.5：合并额外 buff，去重
    extra = EXTRA_BUFF.get((cls,idx), [])
    seen = {x[0] for x in g[3]}
    for e in extra:
        if e[0] not in seen:
            g[3].append(e)
            seen.add(e[0])
    validate_spec(cls, idx, [g[0], g[1], g[2], g[3], tank, heal, util, fill])
    body.append(f'''    // ---- {name} (cls {cls} spec {idx}) ----
    {{ {cls}, {idx}, "{name}", "{role}", {drole},
      {{   // rotation
{fmt(g[0])}
      }},
      {{   // burst
{fmt(g[1])}
      }},
      {{   // defensive
{fmt(g[2])}
      }},
      {{   // buffs
{fmt(g[3])}
      }},
      {{   // tankKit
{fmt(tank)}
      }},
      {{   // healKit
{fmt(heal)}
      }},
      {{   // utility
{fmt(util)}
      }},
      {{   // filler（v3.3：填补 GCD 空隙，跨专精补输出）
{fmt(fill)}
      }} }},
''')

ftr = '''    };
    return tbl;
}

void GetSpecsOfClass(uint8 cls, std::vector<SpecInfo const*>& out)
{
    out.clear();
    for (SpecInfo const& s : GetAllSpecs())
        if (s.cls == cls)
            out.push_back(&s);
}

SpecInfo const* GetSpec(uint8 cls, uint8 specIdx)
{
    for (SpecInfo const& s : GetAllSpecs())
        if (s.cls == cls && s.specIdx == specIdx)
            return &s;
    return nullptr;
}

uint8 GetSpecCount(uint8 cls)
{
    uint8 n = 0;
    for (SpecInfo const& s : GetAllSpecs())
        if (s.cls == cls)
            ++n;
    return n;
}

uint8 SuggestRole(uint8 cls, uint8 specIdx)
{
    if (SpecInfo const* sp = GetSpec(cls, specIdx))
        return sp->defaultRole;
    return ROLE_DPS;
}

// ============================================================================
//  BuildPlan —— v3 核心：按「专精 + 职责 + 场景」组装最终优先级序列
// ============================================================================
static void AppendUnique(std::vector<Skill>& dst, std::vector<Skill> const& src)
{
    /*
     * F44：不能再只按spell去重。相同法术可能有合法的不同意图：
     * 圣光震击可伤敌也可治疗，圣盾术可保命也可解控。
     * 旧“先到先得”会让职责/全专精排序偷偷改变flag语义。
     * 现在仅折叠spell+flags完全相同的条目，不同意图都保留并分别判定。
     */
    for (Skill const& s : src)
    {
        bool dup = false;
        for (Skill const& d : dst)
            if (d.spell == s.spell && d.flags == s.flags)
            {
                dup = true;
                break;
            }
        if (!dup)
            dst.push_back(s);
    }
}

/*
 * F44R1：全专精模式不能把互斥姿态/形态/守护/护甲/圣印/图腾混在一起。
 * 数字是运行时家族键，只用于同一BuiltPlan内“第一优先项胜出”。
 * 圣骑祝福不在这里折叠：它们必须按目标职业/坦克职责选一种。
 */
static uint8 ExclusiveBuffFamily(uint32 spell)
{
    switch (spell)
    {
        case 2457: case 71: case 2458:                         return 1;  // 战士姿态
        case 6673: case 469:                                   return 2;  // 战士怒吼
        case 465: case 7294:                                   return 3;  // 圣骑光环
        case 20165: case 21084: case 31801: case 20375:         return 4;  // 圣印
        case 13165: case 61846: case 34074:                     return 5;  // 猎人守护
        case 48266: case 48263: case 48265:                     return 6;  // 死骑领域
        case 324: case 52127:                                  return 7;  // 萨满护盾
        case 30482: case 7302:                                 return 8;  // 法师护甲
        case 1008: case 604:                                   return 9;  // 强化/抑制魔法
        case 28176: case 706:                                  return 10; // 术士护甲
        case 24858: case 768: case 5487: case 33891:            return 11; // 德鲁伊形态
        case 3599: case 30706: case 8227:                       return 12; // 火图腾
        case 8075:                                               return 13; // 地图腾
        case 5675: case 5394:                                  return 14; // 水图腾
        case 3738: case 8512:                                  return 15; // 风图腾
        default:                                                return 0;
    }
}

static void AppendOpenerUnique(std::vector<Skill>& dst, std::vector<Skill> const& src)
{
    for (Skill const& s : src)
    {
        uint8 family = ExclusiveBuffFamily(s.spell);
        bool familyAlreadyChosen = false;
        if (family)
            for (Skill const& d : dst)
                if (ExclusiveBuffFamily(d.spell) == family)
                {
                    familyAlreadyChosen = true;
                    break;
                }
        if (!familyAlreadyChosen)
            AppendUnique(dst, { s });
    }
}

static void AppendCombatOnly(std::vector<Skill>& dst, std::vector<Skill> const& src)
{
    for (Skill const& s : src)
        if (!(s.flags & (SF_BUFF_KEEP | SF_RAID_BUFF | SF_MAINTAIN_FRIEND |
                         SF_MANUAL | SF_DEPLOYABLE | SF_COMBAT_UTILITY)))
            AppendUnique(dst, { s });
}

void BuildPlan(uint8 cls, uint8 specIdx, uint8 role, uint8 scene,
               bool allSpecs, BuiltPlan& out)
{
    out.opener.clear(); out.emergency.clear();
    out.core.clear();   out.burst.clear(); out.utility.clear();
    out.specName = "未知"; out.roleName = RoleName(role); out.sceneName = SceneName(scene);

    std::vector<SpecInfo const*> use;
    if (allSpecs)
    {
        GetSpecsOfClass(cls, use);
        out.specName = "全专精";

        /*
         * 全专精按「最强顺序」排：
         * 先放和所选职责匹配的专精（坦克就先放坦克专精的技能），
         * 这样主循环开头就是该职责最核心的技能，不会被别的专精挤掉。
         */
        std::vector<SpecInfo const*> sorted;
        for (SpecInfo const* sp : use)
            if (sp->defaultRole == role)
                sorted.push_back(sp);
        for (SpecInfo const* sp : use)
            if (sp->defaultRole != role)
                sorted.push_back(sp);
        use.swap(sorted);
    }
    else
    {
        if (SpecInfo const* sp = GetSpec(cls, specIdx))
        {
            use.push_back(sp);
            out.specName = sp->name;
        }
        else
            return;
    }

    SceneTuning const& tune = GetTuning(scene);

    for (SpecInfo const* sp : use)
    {
        // ---- 增益：互斥家族只保留职责排序后的首选；稳定友方/图腾也进opener ----
        AppendOpenerUnique(out.opener, sp->buffs);
        for (Skill const& s : sp->healKit)
            if (s.flags & (SF_MAINTAIN_FRIEND | SF_DEPLOYABLE))
                AppendOpenerUnique(out.opener, { s });

        // ---- 最高优先级：解控 > 紧急救命 > 保命 ----
        for (Skill const& s : sp->utility)
            if (s.flags & SF_FREE_SELF)
                AppendUnique(out.emergency, { s });

        if (role == ROLE_HEALER)
            for (Skill const& s : sp->healKit)
                if (s.flags & SF_HEAL_EMERG)
                    AppendUnique(out.emergency, { s });

        AppendUnique(out.emergency, sp->defensive);

        // ---- 主循环：按职责组装 ----
        if (role == ROLE_TANK)
        {
            /*
             * 坦克 = 拉怪 + 输出，不能放完防御技就站着。
             * 顺序：嘲讽/减伤 -> 本专精循环 -> 填充技（跨专精补输出）-> 自保治疗
             */
            AppendUnique(out.core, sp->tankKit);
            AppendUnique(out.core, sp->rotation);
            AppendUnique(out.core, sp->filler);      // v3.3：填 GCD 空隙
            AppendCombatOnly(out.core, sp->healKit);
        }
        else if (role == ROLE_HEALER)
        {
            // 治疗：先奶人，血线安全时才输出，最后填充
            AppendCombatOnly(out.core, sp->healKit);
            AppendUnique(out.core, sp->rotation);
            AppendUnique(out.core, sp->filler);
        }
        else
        {
            // 输出：伤害循环 -> 填充技（跨专精补输出）-> 治疗只当自保
            AppendUnique(out.core, sp->rotation);
            AppendUnique(out.core, sp->filler);      // v3.3：正义之锤这类
            AppendCombatOnly(out.core, sp->healKit);
        }

        // ---- 爆发 ----
        AppendUnique(out.burst, sp->burst);

        // ---- 功能技：驱散/打断/回蓝 ----
        for (Skill const& s : sp->utility)
            if (!(s.flags & SF_FREE_SELF))
                AppendUnique(out.utility, { s });
    }

    (void)tune;   // 场景参数在运行时用（CheckSkill 里读），这里只做组装
}

} // namespace CombatSpec
'''

generated = hdr + ''.join(body) + ftr
encoded = generated.encode('utf-8')

if args.check:
    if not args.output.is_file():
        print(f'[FAIL] output missing: {args.output}', file=sys.stderr)
        sys.exit(2)
    current = args.output.read_bytes()
    if current != encoded:
        print(f'[FAIL] generated output differs: {args.output}', file=sys.stderr)
        print(f'[INFO] generated_bytes={len(encoded)} current_bytes={len(current)}', file=sys.stderr)
        sys.exit(3)
    print(f'[OK] SPEC_GENERATOR_CHECK=True bytes={len(encoded)} path={args.output}')
    sys.exit(0)

args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_bytes(encoded)
print(f'[OK] SPEC_GENERATOR_WRITE=True bytes={len(encoded)} path={args.output}')
