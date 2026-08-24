/*
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
    // ---- 武器 (cls 1 spec 0) ----
    { 1, 0, "武器", "近战DPS", ROLE_DPS,
      {   // rotation
        { 12294, SF_MELEE, "致死打击" },
        { 1680, SF_MELEE|SF_AOE, "旋风斩" },
        { 5308, SF_MELEE|SF_EXECUTE, "斩杀" },
        { 7384, SF_MELEE, "压制" },
        { 772, SF_MELEE|SF_DEBUFF_KEEP, "撕裂" },
        { 1464, SF_MELEE, "猛击" },
        { 6343, SF_MELEE|SF_AOE, "雷霆一击" },
        { 845, SF_MELEE|SF_AOE, "顺劈斩" },
        { 78, SF_MELEE, "英勇打击" },
        { 57755, SF_NONE, "英勇投掷" },
      },
      {   // burst
        { 1719, SF_SELF, "鲁莽" },
        { 12328, SF_SELF, "横扫攻击" },
        { 46924, SF_MELEE|SF_AOE, "剑刃风暴" },
        { 18499, SF_SELF, "狂暴之怒" },
      },
      {   // defensive
        { 12975, SF_SELF, "破釜沉舟" },
        { 871, SF_SELF, "盾墙" },
        { 55694, SF_SELF, "狂怒回复" },
        { 23920, SF_SELF, "法术反射" },
      },
      {   // buffs
        { 6673, SF_SELF|SF_BUFF_KEEP|SF_EXCLUSIVE_BUFF, "战斗怒吼" },
        { 2457, SF_SELF|SF_BUFF_KEEP|SF_EXCLUSIVE_BUFF, "战斗姿态" },
        { 50720, SF_MAINTAIN_FRIEND|SF_BUFF_KEEP, "警戒" },
      },
      {   // tankKit

      },
      {   // healKit

      },
      {   // utility
        { 6552, SF_INTERRUPT|SF_MELEE, "拳击" },
        { 18499, SF_FREE_SELF|SF_SELF, "狂暴之怒" },
      },
      {   // filler（v3.3：填补 GCD 空隙，跨专精补输出）
        { 78, SF_MELEE, "英勇打击" },
        { 845, SF_MELEE|SF_AOE, "顺劈斩" },
        { 57755, SF_NONE, "英勇投掷" },
        { 772, SF_MELEE|SF_DEBUFF_KEEP, "撕裂" },
        { 1715, SF_MELEE|SF_DEBUFF_KEEP, "断筋" },
      } },
    // ---- 狂怒 (cls 1 spec 1) ----
    { 1, 1, "狂怒", "近战DPS", ROLE_DPS,
      {   // rotation
        { 23881, SF_MELEE, "嗜血" },
        { 1680, SF_MELEE|SF_AOE, "旋风斩" },
        { 5308, SF_MELEE|SF_EXECUTE, "斩杀" },
        { 34428, SF_MELEE, "乘胜追击" },
        { 6343, SF_MELEE|SF_AOE, "雷霆一击" },
        { 845, SF_MELEE|SF_AOE, "顺劈斩" },
        { 78, SF_MELEE, "英勇打击" },
        { 57755, SF_NONE, "英勇投掷" },
      },
      {   // burst
        { 1719, SF_SELF, "鲁莽" },
        { 12292, SF_SELF, "血性狂暴" },
        { 18499, SF_SELF, "狂暴之怒" },
        { 2687, SF_SELF, "血性狂怒" },
      },
      {   // defensive
        { 12975, SF_SELF, "破釜沉舟" },
        { 55694, SF_SELF, "狂怒回复" },
        { 23920, SF_SELF, "法术反射" },
      },
      {   // buffs
        { 6673, SF_SELF|SF_BUFF_KEEP|SF_EXCLUSIVE_BUFF, "战斗怒吼" },
        { 2458, SF_SELF|SF_BUFF_KEEP|SF_EXCLUSIVE_BUFF, "狂暴姿态" },
        { 50720, SF_MAINTAIN_FRIEND|SF_BUFF_KEEP, "警戒" },
      },
      {   // tankKit

      },
      {   // healKit

      },
      {   // utility
        { 6552, SF_INTERRUPT|SF_MELEE, "拳击" },
        { 18499, SF_FREE_SELF|SF_SELF, "狂暴之怒" },
      },
      {   // filler（v3.3：填补 GCD 空隙，跨专精补输出）
        { 78, SF_MELEE, "英勇打击" },
        { 845, SF_MELEE|SF_AOE, "顺劈斩" },
        { 57755, SF_NONE, "英勇投掷" },
        { 772, SF_MELEE|SF_DEBUFF_KEEP, "撕裂" },
        { 1715, SF_MELEE|SF_DEBUFF_KEEP, "断筋" },
      } },
    // ---- 防护 (cls 1 spec 2) ----
    { 1, 2, "防护", "坦克", ROLE_TANK,
      {   // rotation
        { 23922, SF_MELEE, "盾牌猛击" },
        { 6572, SF_MELEE, "复仇" },
        { 20243, SF_MELEE, "毁灭打击" },
        { 6343, SF_MELEE|SF_AOE, "雷霆一击" },
        { 46968, SF_MELEE|SF_AOE, "震荡波" },
        { 845, SF_MELEE|SF_AOE, "顺劈斩" },
        { 12809, SF_MELEE, "破胆怒吼" },
        { 355, SF_TAUNT, "嘲讽" },
        { 78, SF_MELEE, "英勇打击" },
        { 1160, SF_AOE, "挫志怒吼" },
      },
      {   // burst
        { 12328, SF_SELF, "横扫攻击" },
        { 1161, SF_TAUNT_AOE|SF_SELF, "挑战怒吼" },
      },
      {   // defensive
        { 871, SF_SELF, "盾墙" },
        { 12975, SF_SELF, "破釜沉舟" },
        { 2565, SF_SELF, "盾牌格挡" },
        { 23920, SF_SELF, "法术反射" },
      },
      {   // buffs
        { 71, SF_SELF|SF_BUFF_KEEP|SF_EXCLUSIVE_BUFF, "防御姿态" },
        { 469, SF_SELF|SF_BUFF_KEEP|SF_EXCLUSIVE_BUFF, "命令怒吼" },
        { 50720, SF_MAINTAIN_FRIEND|SF_BUFF_KEEP, "警戒" },
      },
      {   // tankKit
        { 355, SF_TAUNT, "嘲讽" },
        { 1161, SF_TAUNT_AOE|SF_SELF, "挑战怒吼" },
        { 694, SF_TAUNT|SF_MELEE, "挑战" },
        { 72, SF_INTERRUPT|SF_MELEE, "盾牌猛击" },
        { 2565, SF_SELF, "盾牌格挡" },
        { 23920, SF_SELF, "法术反射" },
      },
      {   // healKit

      },
      {   // utility
        { 6552, SF_INTERRUPT|SF_MELEE, "拳击" },
        { 18499, SF_FREE_SELF|SF_SELF, "狂暴之怒" },
      },
      {   // filler（v3.3：填补 GCD 空隙，跨专精补输出）
        { 78, SF_MELEE, "英勇打击" },
        { 845, SF_MELEE|SF_AOE, "顺劈斩" },
        { 57755, SF_NONE, "英勇投掷" },
        { 772, SF_MELEE|SF_DEBUFF_KEEP, "撕裂" },
        { 1715, SF_MELEE|SF_DEBUFF_KEEP, "断筋" },
      } },
    // ---- 神圣 (cls 2 spec 0) ----
    { 2, 0, "神圣", "治疗", ROLE_HEALER,
      {   // rotation
        { 20473, SF_NONE, "圣光震击" },
        { 635, SF_HEAL|SF_NO_MOVE, "圣光术" },
        { 19750, SF_HEAL, "圣光闪现" },
        { 2812, SF_AOE, "神圣愤怒" },
        { 879, SF_NONE, "驱邪术" },
      },
      {   // burst
        { 31821, SF_SELF, "光环之王" },
        { 20216, SF_SELF, "神圣恩典" },
        { 31842, SF_SELF, "神圣启示" },
        { 31884, SF_SELF, "复仇之怒" },
      },
      {   // defensive
        { 642, SF_SELF, "圣盾术" },
        { 498, SF_SELF, "圣佑术" },
        { 633, SF_SELF, "圣疗术" },
        { 1022, SF_FRIEND, "保护之手" },
      },
      {   // buffs
        { 25782, SF_RAID_BUFF|SF_BUFF_KEEP|SF_EXCLUSIVE_BUFF, "强效力量祝福" },
        { 465, SF_SELF|SF_BUFF_KEEP|SF_EXCLUSIVE_BUFF, "虔诚光环" },
        { 25894, SF_RAID_BUFF|SF_BUFF_KEEP|SF_EXCLUSIVE_BUFF, "强效智慧祝福" },
        { 25898, SF_RAID_BUFF|SF_BUFF_KEEP|SF_EXCLUSIVE_BUFF, "强效王者祝福" },
        { 20165, SF_SELF|SF_BUFF_KEEP|SF_EXCLUSIVE_BUFF, "光明圣印" },
      },
      {   // tankKit

      },
      {   // healKit
        { 633, SF_HEAL|SF_HEAL_EMERG, "圣疗术" },
        { 20473, SF_HEAL, "圣光震击" },
        { 53563, SF_MAINTAIN_FRIEND|SF_BUFF_KEEP, "圣光道标" },
        { 19750, SF_HEAL, "圣光闪现" },
        { 635, SF_HEAL|SF_NO_MOVE, "圣光术" },
        { 53601, SF_MAINTAIN_FRIEND|SF_BUFF_KEEP, "神圣壁垒" },
        { 1022, SF_HEAL|SF_HEAL_EMERG, "保护之手" },
        { 6940, SF_HEAL|SF_HEAL_EMERG, "牺牲之手" },
        { 1044, SF_HEAL, "自由之手" },
      },
      {   // utility
        { 4987, SF_DISPEL_FRIEND, "清洁术" },
        { 1152, SF_DISPEL_FRIEND, "净化术" },
        { 642, SF_FREE_SELF|SF_SELF, "圣盾术" },
        { 1044, SF_FREE_SELF|SF_SELF, "自由之手" },
        { 54428, SF_SELF|SF_MANA_LOW, "神圣恳求" },
      },
      {   // filler（v3.3：填补 GCD 空隙，跨专精补输出）
        { 53595, SF_MELEE|SF_AOE, "正义之锤" },
        { 35395, SF_MELEE, "十字军打击" },
        { 20271, SF_MELEE, "审判" },
        { 879, SF_NONE, "驱邪术" },
        { 26573, SF_SELF|SF_AOE, "奉献" },
        { 2812, SF_AOE, "神圣愤怒" },
        { 24275, SF_EXECUTE, "复仇之怒锤" },
      } },
    // ---- 防护 (cls 2 spec 1) ----
    { 2, 1, "防护", "坦克", ROLE_TANK,
      {   // rotation
        { 53595, SF_MELEE|SF_AOE, "正义之锤" },
        { 31935, SF_INTERRUPT, "复仇者之盾" },
        { 26573, SF_SELF|SF_AOE, "奉献" },
        { 53600, SF_MELEE, "正义盾击" },
        { 20271, SF_MELEE, "审判" },
        { 2812, SF_AOE, "神圣愤怒" },
        { 62124, SF_TAUNT, "清算之手" },
        { 31789, SF_TAUNT_AOE, "正义防御" },
        { 879, SF_NONE, "驱邪术" },
      },
      {   // burst
        { 31884, SF_SELF, "复仇之怒" },
        { 20925, SF_SELF, "神圣之盾" },
      },
      {   // defensive
        { 642, SF_SELF, "圣盾术" },
        { 498, SF_SELF, "圣佑术" },
        { 633, SF_SELF, "圣疗术" },
        { 64205, SF_SELF, "神圣牺牲" },
      },
      {   // buffs
        { 20911, SF_RAID_BUFF|SF_BUFF_KEEP|SF_EXCLUSIVE_BUFF, "庇护祝福" },
        { 465, SF_SELF|SF_BUFF_KEEP|SF_EXCLUSIVE_BUFF, "虔诚光环" },
        { 25780, SF_SELF|SF_BUFF_KEEP, "正义之怒" },
        { 25898, SF_RAID_BUFF|SF_BUFF_KEEP|SF_EXCLUSIVE_BUFF, "强效王者祝福" },
        { 25782, SF_RAID_BUFF|SF_BUFF_KEEP|SF_EXCLUSIVE_BUFF, "强效力量祝福" },
        { 21084, SF_SELF|SF_BUFF_KEEP|SF_EXCLUSIVE_BUFF, "正义圣印" },
      },
      {   // tankKit
        { 62124, SF_TAUNT, "清算之手" },
        { 31789, SF_TAUNT_AOE, "正义防御" },
        { 20925, SF_SELF|SF_BUFF_KEEP, "神圣之盾" },
        { 31935, SF_INTERRUPT, "复仇者之盾" },
        { 64205, SF_SELF, "神圣牺牲" },
      },
      {   // healKit
        { 633, SF_HEAL|SF_HEAL_EMERG, "圣疗术" },
        { 19750, SF_HEAL|SF_NO_MOVE, "圣光闪现" },
      },
      {   // utility
        { 4987, SF_DISPEL_FRIEND, "清洁术" },
        { 1152, SF_DISPEL_FRIEND, "净化术" },
        { 642, SF_FREE_SELF|SF_SELF, "圣盾术" },
        { 1044, SF_FREE_SELF|SF_SELF, "自由之手" },
        { 54428, SF_SELF|SF_MANA_LOW, "神圣恳求" },
      },
      {   // filler（v3.3：填补 GCD 空隙，跨专精补输出）
        { 53595, SF_MELEE|SF_AOE, "正义之锤" },
        { 35395, SF_MELEE, "十字军打击" },
        { 20271, SF_MELEE, "审判" },
        { 879, SF_NONE, "驱邪术" },
        { 26573, SF_SELF|SF_AOE, "奉献" },
        { 2812, SF_AOE, "神圣愤怒" },
        { 24275, SF_EXECUTE, "复仇之怒锤" },
      } },
    // ---- 惩戒 (cls 2 spec 2) ----
    { 2, 2, "惩戒", "近战DPS", ROLE_DPS,
      {   // rotation
        { 35395, SF_MELEE, "十字军打击" },
        { 53385, SF_MELEE|SF_AOE, "神圣风暴" },
        { 20271, SF_MELEE, "审判" },
        { 24275, SF_EXECUTE, "复仇之怒锤" },
        { 26573, SF_SELF|SF_AOE, "奉献" },
        { 879, SF_NONE, "驱邪术" },
        { 2812, SF_AOE, "神圣愤怒" },
      },
      {   // burst
        { 31884, SF_SELF, "复仇之怒" },
      },
      {   // defensive
        { 642, SF_SELF, "圣盾术" },
        { 498, SF_SELF, "圣佑术" },
        { 633, SF_SELF, "圣疗术" },
        { 1022, SF_FRIEND, "保护之手" },
      },
      {   // buffs
        { 31801, SF_SELF|SF_BUFF_KEEP|SF_EXCLUSIVE_BUFF, "复仇圣印" },
        { 25782, SF_RAID_BUFF|SF_BUFF_KEEP|SF_EXCLUSIVE_BUFF, "强效力量祝福" },
        { 7294, SF_SELF|SF_BUFF_KEEP|SF_EXCLUSIVE_BUFF, "惩戒光环" },
        { 25898, SF_RAID_BUFF|SF_BUFF_KEEP|SF_EXCLUSIVE_BUFF, "强效王者祝福" },
      },
      {   // tankKit

      },
      {   // healKit
        { 633, SF_HEAL|SF_HEAL_EMERG, "圣疗术" },
        { 19750, SF_HEAL|SF_NO_MOVE, "圣光闪现" },
      },
      {   // utility
        { 4987, SF_DISPEL_FRIEND, "清洁术" },
        { 1152, SF_DISPEL_FRIEND, "净化术" },
        { 642, SF_FREE_SELF|SF_SELF, "圣盾术" },
        { 1044, SF_FREE_SELF|SF_SELF, "自由之手" },
        { 54428, SF_SELF|SF_MANA_LOW, "神圣恳求" },
      },
      {   // filler（v3.3：填补 GCD 空隙，跨专精补输出）
        { 53595, SF_MELEE|SF_AOE, "正义之锤" },
        { 35395, SF_MELEE, "十字军打击" },
        { 20271, SF_MELEE, "审判" },
        { 879, SF_NONE, "驱邪术" },
        { 26573, SF_SELF|SF_AOE, "奉献" },
        { 2812, SF_AOE, "神圣愤怒" },
        { 24275, SF_EXECUTE, "复仇之怒锤" },
      } },
    // ---- 野兽掌握 (cls 3 spec 0) ----
    { 3, 0, "野兽掌握", "远程DPS", ROLE_DPS,
      {   // rotation
        { 53351, SF_EXECUTE, "杀戮射击" },
        { 3044, SF_NONE, "奥术射击" },
        { 1978, SF_DEBUFF_KEEP, "毒蛇钉刺" },
        { 2643, SF_AOE, "多重射击" },
        { 1510, SF_AOE|SF_NO_MOVE, "乱射" },
        { 56641, SF_NO_MOVE, "稳固射击" },
        { 1130, SF_DEBUFF_KEEP, "猎人印记" },
      },
      {   // burst
        { 19574, SF_SELF, "狂野怒火" },
        { 3045, SF_SELF, "急速射击" },
        { 23989, SF_SELF, "整装待发" },
      },
      {   // defensive
        { 5384, SF_SELF, "假死" },
        { 19263, SF_SELF, "威慑" },
        { 781, SF_SELF, "逃脱" },
        { 136, SF_SELF, "治疗宠物" },
      },
      {   // buffs
        { 13165, SF_SELF|SF_BUFF_KEEP|SF_EXCLUSIVE_BUFF, "雄鹰守护" },
        { 19506, SF_SELF|SF_BUFF_KEEP, "强击光环" },
      },
      {   // tankKit

      },
      {   // healKit

      },
      {   // utility
        { 34490, SF_INTERRUPT, "沉默射击" },
        { 19801, SF_DISPEL_FRIEND, "宁神射击" },
        { 5384, SF_FREE_SELF|SF_SELF, "假死" },
        { 781, SF_FREE_SELF|SF_SELF, "逃脱" },
        { 34074, SF_SELF|SF_MANA_LOW|SF_EXCLUSIVE_BUFF, "蝰蛇守护" },
        { 34477, SF_MAINTAIN_FRIEND|SF_COMBAT_UTILITY, "误导" },
      },
      {   // filler（v3.3：填补 GCD 空隙，跨专精补输出）
        { 3044, SF_NONE, "奥术射击" },
        { 56641, SF_NO_MOVE, "稳固射击" },
        { 2973, SF_MELEE, "猛禽一击" },
        { 1978, SF_DEBUFF_KEEP, "毒蛇钉刺" },
        { 2643, SF_AOE, "多重射击" },
        { 53351, SF_EXECUTE, "杀戮射击" },
        { 1130, SF_DEBUFF_KEEP, "猎人印记" },
      } },
    // ---- 射击 (cls 3 spec 1) ----
    { 3, 1, "射击", "远程DPS", ROLE_DPS,
      {   // rotation
        { 53209, SF_NONE, "奇美拉射击" },
        { 19434, SF_NO_MOVE, "瞄准射击" },
        { 53351, SF_EXECUTE, "杀戮射击" },
        { 3044, SF_NONE, "奥术射击" },
        { 1978, SF_DEBUFF_KEEP, "毒蛇钉刺" },
        { 2643, SF_AOE, "多重射击" },
        { 1510, SF_AOE|SF_NO_MOVE, "乱射" },
        { 56641, SF_NO_MOVE, "稳固射击" },
        { 1130, SF_DEBUFF_KEEP, "猎人印记" },
      },
      {   // burst
        { 3045, SF_SELF, "急速射击" },
        { 23989, SF_SELF, "整装待发" },
        { 34490, SF_NONE, "沉默射击" },
      },
      {   // defensive
        { 5384, SF_SELF, "假死" },
        { 19263, SF_SELF, "威慑" },
        { 781, SF_SELF, "逃脱" },
      },
      {   // buffs
        { 13165, SF_SELF|SF_BUFF_KEEP|SF_EXCLUSIVE_BUFF, "雄鹰守护" },
        { 19506, SF_SELF|SF_BUFF_KEEP, "强击光环" },
      },
      {   // tankKit

      },
      {   // healKit

      },
      {   // utility
        { 34490, SF_INTERRUPT, "沉默射击" },
        { 19801, SF_DISPEL_FRIEND, "宁神射击" },
        { 5384, SF_FREE_SELF|SF_SELF, "假死" },
        { 781, SF_FREE_SELF|SF_SELF, "逃脱" },
        { 34074, SF_SELF|SF_MANA_LOW|SF_EXCLUSIVE_BUFF, "蝰蛇守护" },
        { 34477, SF_MAINTAIN_FRIEND|SF_COMBAT_UTILITY, "误导" },
      },
      {   // filler（v3.3：填补 GCD 空隙，跨专精补输出）
        { 3044, SF_NONE, "奥术射击" },
        { 56641, SF_NO_MOVE, "稳固射击" },
        { 2973, SF_MELEE, "猛禽一击" },
        { 1978, SF_DEBUFF_KEEP, "毒蛇钉刺" },
        { 2643, SF_AOE, "多重射击" },
        { 53351, SF_EXECUTE, "杀戮射击" },
        { 1130, SF_DEBUFF_KEEP, "猎人印记" },
      } },
    // ---- 生存 (cls 3 spec 2) ----
    { 3, 2, "生存", "远程DPS", ROLE_DPS,
      {   // rotation
        { 53301, SF_NONE, "爆炸射击" },
        { 3674, SF_DEBUFF_KEEP, "黑箭" },
        { 53351, SF_EXECUTE, "杀戮射击" },
        { 1978, SF_DEBUFF_KEEP, "毒蛇钉刺" },
        { 3044, SF_NONE, "奥术射击" },
        { 13813, SF_AOE|SF_SELF, "爆炸陷阱" },
        { 2643, SF_AOE, "多重射击" },
        { 56641, SF_NO_MOVE, "稳固射击" },
        { 1130, SF_DEBUFF_KEEP, "猎人印记" },
      },
      {   // burst
        { 3045, SF_SELF, "急速射击" },
        { 23989, SF_SELF, "整装待发" },
      },
      {   // defensive
        { 5384, SF_SELF, "假死" },
        { 19263, SF_SELF, "威慑" },
        { 781, SF_SELF, "逃脱" },
      },
      {   // buffs
        { 61846, SF_SELF|SF_BUFF_KEEP|SF_EXCLUSIVE_BUFF, "巨龙鹰守护" },
        { 19506, SF_SELF|SF_BUFF_KEEP, "强击光环" },
      },
      {   // tankKit

      },
      {   // healKit

      },
      {   // utility
        { 34490, SF_INTERRUPT, "沉默射击" },
        { 19801, SF_DISPEL_FRIEND, "宁神射击" },
        { 5384, SF_FREE_SELF|SF_SELF, "假死" },
        { 781, SF_FREE_SELF|SF_SELF, "逃脱" },
        { 34074, SF_SELF|SF_MANA_LOW|SF_EXCLUSIVE_BUFF, "蝰蛇守护" },
        { 34477, SF_MAINTAIN_FRIEND|SF_COMBAT_UTILITY, "误导" },
      },
      {   // filler（v3.3：填补 GCD 空隙，跨专精补输出）
        { 3044, SF_NONE, "奥术射击" },
        { 56641, SF_NO_MOVE, "稳固射击" },
        { 2973, SF_MELEE, "猛禽一击" },
        { 1978, SF_DEBUFF_KEEP, "毒蛇钉刺" },
        { 2643, SF_AOE, "多重射击" },
        { 53351, SF_EXECUTE, "杀戮射击" },
        { 1130, SF_DEBUFF_KEEP, "猎人印记" },
      } },
    // ---- 刺杀 (cls 4 spec 0) ----
    { 4, 0, "刺杀", "近战DPS", ROLE_DPS,
      {   // rotation
        { 32645, SF_MELEE|SF_COMBO_FINISH, "毒伤" },
        { 1943, SF_MELEE|SF_COMBO_FINISH, "割裂" },
        { 5171, SF_MELEE|SF_COMBO_FINISH, "切割" },
        { 1329, SF_MELEE|SF_COMBO_BUILD, "毁伤" },
        { 51723, SF_MELEE|SF_AOE, "刀扇" },
        { 1752, SF_MELEE|SF_COMBO_BUILD, "邪恶攻击" },
      },
      {   // burst
        { 14177, SF_SELF, "冷血" },
        { 51662, SF_SELF, "嗜血成性" },
      },
      {   // defensive
        { 5277, SF_SELF, "闪避" },
        { 1856, SF_SELF, "消失" },
        { 31224, SF_SELF, "暗影斗篷" },
        { 1966, SF_SELF, "佯攻" },
      },
      {   // buffs
        { 2823, SF_SELF|SF_MANUAL, "致命毒药" },
        { 8679, SF_SELF|SF_MANUAL, "速效毒药" },
        { 13219, SF_SELF|SF_MANUAL, "致伤毒药" },
      },
      {   // tankKit

      },
      {   // healKit

      },
      {   // utility
        { 1766, SF_INTERRUPT|SF_MELEE, "脚踢" },
        { 31224, SF_FREE_SELF|SF_SELF, "暗影斗篷" },
        { 5277, SF_SELF, "闪避" },
        { 57934, SF_MAINTAIN_FRIEND|SF_COMBAT_UTILITY, "嫁祸诀窍" },
      },
      {   // filler（v3.3：填补 GCD 空隙，跨专精补输出）
        { 1752, SF_MELEE|SF_COMBO_BUILD, "邪恶攻击" },
        { 2098, SF_MELEE|SF_COMBO_FINISH, "刺骨" },
        { 51723, SF_MELEE|SF_AOE, "刀扇" },
        { 8647, SF_MELEE|SF_COMBO_FINISH, "破甲" },
        { 1943, SF_MELEE|SF_COMBO_FINISH, "割裂" },
        { 5171, SF_MELEE|SF_COMBO_FINISH, "切割" },
        { 53, SF_MELEE|SF_COMBO_BUILD, "背刺" },
        { 16511, SF_MELEE|SF_COMBO_BUILD, "出血" },
      } },
    // ---- 战斗 (cls 4 spec 1) ----
    { 4, 1, "战斗", "近战DPS", ROLE_DPS,
      {   // rotation
        { 2098, SF_MELEE|SF_COMBO_FINISH, "刺骨" },
        { 5171, SF_MELEE|SF_COMBO_FINISH, "切割" },
        { 8647, SF_MELEE|SF_COMBO_FINISH, "破甲" },
        { 51723, SF_MELEE|SF_AOE, "刀扇" },
        { 1752, SF_MELEE|SF_COMBO_BUILD, "邪恶攻击" },
      },
      {   // burst
        { 13750, SF_SELF, "冲动" },
        { 13877, SF_SELF, "剑刃乱舞" },
        { 51690, SF_MELEE, "杀戮盛宴" },
        { 14185, SF_SELF, "预谋" },
      },
      {   // defensive
        { 5277, SF_SELF, "闪避" },
        { 1856, SF_SELF, "消失" },
        { 31224, SF_SELF, "暗影斗篷" },
        { 1966, SF_SELF, "佯攻" },
      },
      {   // buffs
        { 8679, SF_SELF|SF_MANUAL, "速效毒药" },
        { 2823, SF_SELF|SF_MANUAL, "致命毒药" },
        { 13219, SF_SELF|SF_MANUAL, "致伤毒药" },
      },
      {   // tankKit

      },
      {   // healKit

      },
      {   // utility
        { 1766, SF_INTERRUPT|SF_MELEE, "脚踢" },
        { 31224, SF_FREE_SELF|SF_SELF, "暗影斗篷" },
        { 5277, SF_SELF, "闪避" },
        { 57934, SF_MAINTAIN_FRIEND|SF_COMBAT_UTILITY, "嫁祸诀窍" },
      },
      {   // filler（v3.3：填补 GCD 空隙，跨专精补输出）
        { 1752, SF_MELEE|SF_COMBO_BUILD, "邪恶攻击" },
        { 2098, SF_MELEE|SF_COMBO_FINISH, "刺骨" },
        { 51723, SF_MELEE|SF_AOE, "刀扇" },
        { 8647, SF_MELEE|SF_COMBO_FINISH, "破甲" },
        { 1943, SF_MELEE|SF_COMBO_FINISH, "割裂" },
        { 5171, SF_MELEE|SF_COMBO_FINISH, "切割" },
        { 53, SF_MELEE|SF_COMBO_BUILD, "背刺" },
        { 16511, SF_MELEE|SF_COMBO_BUILD, "出血" },
      } },
    // ---- 敏锐 (cls 4 spec 2) ----
    { 4, 2, "敏锐", "近战DPS", ROLE_DPS,
      {   // rotation
        { 2098, SF_MELEE|SF_COMBO_FINISH, "刺骨" },
        { 1943, SF_MELEE|SF_COMBO_FINISH, "割裂" },
        { 5171, SF_MELEE|SF_COMBO_FINISH, "切割" },
        { 16511, SF_MELEE|SF_COMBO_BUILD, "出血" },
        { 53, SF_MELEE|SF_COMBO_BUILD, "背刺" },
        { 51723, SF_MELEE|SF_AOE, "刀扇" },
        { 1752, SF_MELEE|SF_COMBO_BUILD, "邪恶攻击" },
      },
      {   // burst
        { 51713, SF_SELF, "暗影之舞" },
        { 36554, SF_MELEE, "暗影步" },
        { 14177, SF_SELF, "冷血" },
      },
      {   // defensive
        { 5277, SF_SELF, "闪避" },
        { 1856, SF_SELF, "消失" },
        { 31224, SF_SELF, "暗影斗篷" },
        { 1966, SF_SELF, "佯攻" },
      },
      {   // buffs
        { 2823, SF_SELF|SF_MANUAL, "致命毒药" },
        { 8679, SF_SELF|SF_MANUAL, "速效毒药" },
        { 13219, SF_SELF|SF_MANUAL, "致伤毒药" },
      },
      {   // tankKit

      },
      {   // healKit

      },
      {   // utility
        { 1766, SF_INTERRUPT|SF_MELEE, "脚踢" },
        { 31224, SF_FREE_SELF|SF_SELF, "暗影斗篷" },
        { 5277, SF_SELF, "闪避" },
        { 57934, SF_MAINTAIN_FRIEND|SF_COMBAT_UTILITY, "嫁祸诀窍" },
      },
      {   // filler（v3.3：填补 GCD 空隙，跨专精补输出）
        { 1752, SF_MELEE|SF_COMBO_BUILD, "邪恶攻击" },
        { 2098, SF_MELEE|SF_COMBO_FINISH, "刺骨" },
        { 51723, SF_MELEE|SF_AOE, "刀扇" },
        { 8647, SF_MELEE|SF_COMBO_FINISH, "破甲" },
        { 1943, SF_MELEE|SF_COMBO_FINISH, "割裂" },
        { 5171, SF_MELEE|SF_COMBO_FINISH, "切割" },
        { 53, SF_MELEE|SF_COMBO_BUILD, "背刺" },
        { 16511, SF_MELEE|SF_COMBO_BUILD, "出血" },
      } },
    // ---- 戒律 (cls 5 spec 0) ----
    { 5, 0, "戒律", "治疗", ROLE_HEALER,
      {   // rotation
        { 47540, SF_NONE, "苦修" },
        { 17, SF_HEAL|SF_HOT, "真言术盾" },
        { 2061, SF_HEAL|SF_NO_MOVE, "快速治疗" },
        { 139, SF_HEAL|SF_HOT, "恢复" },
        { 2060, SF_HEAL|SF_NO_MOVE, "强效治疗" },
        { 33076, SF_HEAL|SF_HOT, "愈合祷言" },
        { 596, SF_HEAL_AOE|SF_NO_MOVE, "治疗祷言" },
        { 585, SF_NO_MOVE, "惩击" },
      },
      {   // burst
        { 10060, SF_SELF, "能量灌注" },
        { 14751, SF_SELF, "心灵专注" },
        { 64901, SF_SELF, "希望圣歌" },
      },
      {   // defensive
        { 586, SF_SELF, "渐隐" },
        { 19236, SF_SELF, "绝望祷言" },
      },
      {   // buffs
        { 1243, SF_RAID_BUFF|SF_BUFF_KEEP, "真言术韧" },
        { 588, SF_SELF|SF_BUFF_KEEP, "心灵之火" },
        { 14752, SF_RAID_BUFF|SF_BUFF_KEEP, "神圣之灵" },
        { 976, SF_RAID_BUFF|SF_BUFF_KEEP, "暗影防护" },
        { 6346, SF_MAINTAIN_FRIEND|SF_BUFF_KEEP, "防护恐惧结界" },
      },
      {   // tankKit

      },
      {   // healKit
        { 47788, SF_HEAL|SF_HEAL_EMERG, "守护之魂" },
        { 33206, SF_HEAL|SF_HEAL_EMERG, "痛苦压制" },
        { 17, SF_HEAL|SF_HOT, "真言术盾" },
        { 47540, SF_HEAL, "苦修" },
        { 33076, SF_HEAL|SF_HOT, "愈合祷言" },
        { 2061, SF_HEAL|SF_NO_MOVE, "快速治疗" },
        { 139, SF_HEAL|SF_HOT, "恢复" },
        { 2060, SF_HEAL|SF_NO_MOVE, "强效治疗" },
        { 596, SF_HEAL_AOE|SF_NO_MOVE, "治疗祷言" },
      },
      {   // utility
        { 527, SF_DISPEL_FRIEND, "驱散魔法" },
        { 528, SF_DISPEL_FRIEND, "驱除疾病" },
        { 552, SF_DISPEL_FRIEND, "消除疾病" },
        { 32375, SF_DISPEL_FRIEND|SF_AOE, "群体驱散" },
        { 15487, SF_INTERRUPT, "沉默" },
        { 6346, SF_MAINTAIN_FRIEND|SF_BUFF_KEEP, "防护恐惧结界" },
        { 64901, SF_SELF|SF_MANA_LOW, "希望圣歌" },
      },
      {   // filler（v3.3：填补 GCD 空隙，跨专精补输出）
        { 585, SF_NO_MOVE, "惩击" },
        { 589, SF_DEBUFF_KEEP, "暗言术痛" },
        { 8092, SF_NONE, "心灵震爆" },
        { 14914, SF_NONE, "神圣之火" },
        { 15407, SF_NO_MOVE, "精神鞭笞" },
        { 2944, SF_DEBUFF_KEEP, "噬灵疫病" },
      } },
    // ---- 神圣 (cls 5 spec 1) ----
    { 5, 1, "神圣", "治疗", ROLE_HEALER,
      {   // rotation
        { 2061, SF_HEAL|SF_NO_MOVE, "快速治疗" },
        { 139, SF_HEAL|SF_HOT, "恢复" },
        { 34861, SF_HEAL_AOE, "治疗之环" },
        { 33076, SF_HEAL|SF_HOT, "愈合祷言" },
        { 2060, SF_HEAL|SF_NO_MOVE, "强效治疗" },
        { 596, SF_HEAL_AOE|SF_NO_MOVE, "治疗祷言" },
        { 14914, SF_NONE, "神圣之火" },
        { 585, SF_NO_MOVE, "惩击" },
      },
      {   // burst
        { 14751, SF_SELF, "心灵专注" },
        { 10060, SF_SELF, "能量灌注" },
      },
      {   // defensive
        { 586, SF_SELF, "渐隐" },
        { 19236, SF_SELF, "绝望祷言" },
      },
      {   // buffs
        { 1243, SF_RAID_BUFF|SF_BUFF_KEEP, "真言术韧" },
        { 588, SF_SELF|SF_BUFF_KEEP, "心灵之火" },
        { 14752, SF_RAID_BUFF|SF_BUFF_KEEP, "神圣之灵" },
        { 6346, SF_MAINTAIN_FRIEND|SF_BUFF_KEEP, "防护恐惧结界" },
        { 976, SF_RAID_BUFF|SF_BUFF_KEEP, "暗影防护" },
      },
      {   // tankKit

      },
      {   // healKit
        { 47788, SF_HEAL|SF_HEAL_EMERG, "守护之魂" },
        { 34861, SF_HEAL_AOE, "治疗之环" },
        { 64843, SF_HEAL_AOE|SF_NO_MOVE, "神圣赞美诗" },
        { 33076, SF_HEAL|SF_HOT, "愈合祷言" },
        { 139, SF_HEAL|SF_HOT, "恢复" },
        { 2061, SF_HEAL|SF_NO_MOVE, "快速治疗" },
        { 2060, SF_HEAL|SF_NO_MOVE, "强效治疗" },
        { 596, SF_HEAL_AOE|SF_NO_MOVE, "治疗祷言" },
      },
      {   // utility
        { 527, SF_DISPEL_FRIEND, "驱散魔法" },
        { 528, SF_DISPEL_FRIEND, "驱除疾病" },
        { 552, SF_DISPEL_FRIEND, "消除疾病" },
        { 32375, SF_DISPEL_FRIEND|SF_AOE, "群体驱散" },
        { 15487, SF_INTERRUPT, "沉默" },
        { 6346, SF_MAINTAIN_FRIEND|SF_BUFF_KEEP, "防护恐惧结界" },
        { 64901, SF_SELF|SF_MANA_LOW, "希望圣歌" },
      },
      {   // filler（v3.3：填补 GCD 空隙，跨专精补输出）
        { 585, SF_NO_MOVE, "惩击" },
        { 589, SF_DEBUFF_KEEP, "暗言术痛" },
        { 8092, SF_NONE, "心灵震爆" },
        { 14914, SF_NONE, "神圣之火" },
        { 15407, SF_NO_MOVE, "精神鞭笞" },
        { 2944, SF_DEBUFF_KEEP, "噬灵疫病" },
      } },
    // ---- 暗影 (cls 5 spec 2) ----
    { 5, 2, "暗影", "远程DPS", ROLE_DPS,
      {   // rotation
        { 34914, SF_DEBUFF_KEEP, "吸血鬼之触" },
        { 589, SF_DEBUFF_KEEP, "暗言术痛" },
        { 2944, SF_DEBUFF_KEEP, "噬灵疫病" },
        { 8092, SF_NONE, "心灵震爆" },
        { 32379, SF_EXECUTE, "暗言术灭" },
        { 48045, SF_AOE|SF_NO_MOVE, "精神灼烧" },
        { 15407, SF_NO_MOVE, "精神鞭笞" },
      },
      {   // burst
        { 34433, SF_NONE, "暗影魔" },
        { 10060, SF_SELF, "能量灌注" },
        { 14751, SF_SELF, "心灵专注" },
      },
      {   // defensive
        { 47585, SF_SELF, "消散" },
        { 586, SF_SELF, "渐隐" },
        { 19236, SF_SELF, "绝望祷言" },
        { 15286, SF_SELF, "吸血鬼拥抱" },
      },
      {   // buffs
        { 15473, SF_SELF|SF_BUFF_KEEP, "暗影形态" },
        { 1243, SF_RAID_BUFF|SF_BUFF_KEEP, "真言术韧" },
        { 588, SF_SELF|SF_BUFF_KEEP, "心灵之火" },
        { 976, SF_RAID_BUFF|SF_BUFF_KEEP, "暗影防护" },
        { 14752, SF_RAID_BUFF|SF_BUFF_KEEP, "神圣之灵" },
      },
      {   // tankKit

      },
      {   // healKit
        { 2061, SF_HEAL|SF_NO_MOVE, "快速治疗" },
        { 17, SF_HEAL|SF_HOT, "真言术盾" },
      },
      {   // utility
        { 527, SF_DISPEL_FRIEND, "驱散魔法" },
        { 528, SF_DISPEL_FRIEND, "驱除疾病" },
        { 552, SF_DISPEL_FRIEND, "消除疾病" },
        { 32375, SF_DISPEL_FRIEND|SF_AOE, "群体驱散" },
        { 15487, SF_INTERRUPT, "沉默" },
        { 6346, SF_MAINTAIN_FRIEND|SF_BUFF_KEEP, "防护恐惧结界" },
        { 64901, SF_SELF|SF_MANA_LOW, "希望圣歌" },
      },
      {   // filler（v3.3：填补 GCD 空隙，跨专精补输出）
        { 585, SF_NO_MOVE, "惩击" },
        { 589, SF_DEBUFF_KEEP, "暗言术痛" },
        { 8092, SF_NONE, "心灵震爆" },
        { 14914, SF_NONE, "神圣之火" },
        { 15407, SF_NO_MOVE, "精神鞭笞" },
        { 2944, SF_DEBUFF_KEEP, "噬灵疫病" },
      } },
    // ---- 鲜血 (cls 6 spec 0) ----
    { 6, 0, "鲜血", "坦克", ROLE_TANK,
      {   // rotation
        { 55050, SF_MELEE, "心脏打击" },
        { 45477, SF_MELEE|SF_DEBUFF_KEEP, "冰冷触摸" },
        { 45462, SF_MELEE|SF_DEBUFF_KEEP, "瘟疫打击" },
        { 49998, SF_MELEE, "死亡打击" },
        { 56815, SF_MELEE, "符文打击" },
        { 48721, SF_MELEE|SF_AOE, "血液沸腾" },
        { 43265, SF_SELF|SF_AOE, "凋零缠绕" },
        { 50842, SF_MELEE|SF_AOE, "传染" },
        { 47541, SF_NONE, "死亡缠绕" },
        { 56222, SF_TAUNT, "黑暗命令" },
      },
      {   // burst
        { 49028, SF_SELF, "符文刃舞" },
        { 47568, SF_SELF, "精通符文刃" },
      },
      {   // defensive
        { 48792, SF_SELF, "冰封之韧" },
        { 48707, SF_SELF, "反魔法护罩" },
        { 55233, SF_SELF, "吸血鬼之血" },
        { 48982, SF_SELF, "符文分流" },
      },
      {   // buffs
        { 48266, SF_SELF|SF_BUFF_KEEP|SF_EXCLUSIVE_BUFF, "鲜血领域" },
        { 57330, SF_SELF|SF_BUFF_KEEP, "寒冬号角" },
        { 49222, SF_SELF|SF_BUFF_KEEP, "白骨之盾" },
      },
      {   // tankKit
        { 56222, SF_TAUNT, "黑暗命令" },
        { 49576, SF_TAUNT, "死亡之握" },
        { 47528, SF_INTERRUPT|SF_MELEE, "心灵冰冻" },
        { 49222, SF_SELF|SF_BUFF_KEEP, "白骨之盾" },
        { 51052, SF_SELF|SF_AOE, "反魔法领域" },
        { 55233, SF_SELF, "吸血鬼之血" },
      },
      {   // healKit

      },
      {   // utility
        { 47528, SF_INTERRUPT|SF_MELEE, "心灵冰冻" },
        { 48707, SF_FREE_SELF|SF_SELF, "反魔法护罩" },
        { 48792, SF_FREE_SELF|SF_SELF, "冰封之韧" },
      },
      {   // filler（v3.3：填补 GCD 空隙，跨专精补输出）
        { 45902, SF_MELEE, "鲜血打击" },
        { 45477, SF_MELEE|SF_DEBUFF_KEEP, "冰冷触摸" },
        { 45462, SF_MELEE|SF_DEBUFF_KEEP, "瘟疫打击" },
        { 47541, SF_NONE, "死亡缠绕" },
        { 56815, SF_MELEE, "符文打击" },
        { 48721, SF_MELEE|SF_AOE, "血液沸腾" },
      } },
    // ---- 冰霜 (cls 6 spec 1) ----
    { 6, 1, "冰霜", "近战DPS", ROLE_DPS,
      {   // rotation
        { 49020, SF_MELEE, "湮没" },
        { 45477, SF_MELEE|SF_DEBUFF_KEEP, "冰冷触摸" },
        { 45462, SF_MELEE|SF_DEBUFF_KEEP, "瘟疫打击" },
        { 49143, SF_MELEE, "冰霜打击" },
        { 49184, SF_AOE, "凛风冲击" },
        { 48721, SF_MELEE|SF_AOE, "血液沸腾" },
        { 43265, SF_SELF|SF_AOE, "凋零缠绕" },
        { 50842, SF_MELEE|SF_AOE, "传染" },
        { 45902, SF_MELEE, "鲜血打击" },
      },
      {   // burst
        { 51271, SF_SELF, "坚固护甲" },
        { 47568, SF_SELF, "精通符文刃" },
        { 49796, SF_SELF, "死亡之寒" },
      },
      {   // defensive
        { 48792, SF_SELF, "冰封之韧" },
        { 48707, SF_SELF, "反魔法护罩" },
        { 48982, SF_SELF, "符文分流" },
      },
      {   // buffs
        { 48263, SF_SELF|SF_BUFF_KEEP|SF_EXCLUSIVE_BUFF, "冰霜领域" },
        { 57330, SF_SELF|SF_BUFF_KEEP, "寒冬号角" },
      },
      {   // tankKit

      },
      {   // healKit

      },
      {   // utility
        { 47528, SF_INTERRUPT|SF_MELEE, "心灵冰冻" },
        { 48707, SF_FREE_SELF|SF_SELF, "反魔法护罩" },
        { 48792, SF_FREE_SELF|SF_SELF, "冰封之韧" },
      },
      {   // filler（v3.3：填补 GCD 空隙，跨专精补输出）
        { 45902, SF_MELEE, "鲜血打击" },
        { 45477, SF_MELEE|SF_DEBUFF_KEEP, "冰冷触摸" },
        { 45462, SF_MELEE|SF_DEBUFF_KEEP, "瘟疫打击" },
        { 47541, SF_NONE, "死亡缠绕" },
        { 56815, SF_MELEE, "符文打击" },
        { 48721, SF_MELEE|SF_AOE, "血液沸腾" },
      } },
    // ---- 邪恶 (cls 6 spec 2) ----
    { 6, 2, "邪恶", "近战DPS", ROLE_DPS,
      {   // rotation
        { 55090, SF_MELEE, "天灾打击" },
        { 45477, SF_MELEE|SF_DEBUFF_KEEP, "冰冷触摸" },
        { 45462, SF_MELEE|SF_DEBUFF_KEEP, "瘟疫打击" },
        { 47541, SF_NONE, "死亡缠绕" },
        { 43265, SF_SELF|SF_AOE, "凋零缠绕" },
        { 48721, SF_MELEE|SF_AOE, "血液沸腾" },
        { 50842, SF_MELEE|SF_AOE, "传染" },
        { 45902, SF_MELEE, "鲜血打击" },
      },
      {   // burst
        { 42650, SF_SELF, "亡者大军" },
        { 49206, SF_NONE, "召唤石像鬼" },
        { 47568, SF_SELF, "精通符文刃" },
      },
      {   // defensive
        { 48792, SF_SELF, "冰封之韧" },
        { 48707, SF_SELF, "反魔法护罩" },
        { 49222, SF_SELF, "白骨之盾" },
        { 48982, SF_SELF, "符文分流" },
      },
      {   // buffs
        { 48265, SF_SELF|SF_BUFF_KEEP|SF_EXCLUSIVE_BUFF, "邪恶领域" },
        { 57330, SF_SELF|SF_BUFF_KEEP, "寒冬号角" },
        { 49222, SF_SELF|SF_BUFF_KEEP, "白骨之盾" },
      },
      {   // tankKit

      },
      {   // healKit

      },
      {   // utility
        { 47528, SF_INTERRUPT|SF_MELEE, "心灵冰冻" },
        { 48707, SF_FREE_SELF|SF_SELF, "反魔法护罩" },
        { 48792, SF_FREE_SELF|SF_SELF, "冰封之韧" },
      },
      {   // filler（v3.3：填补 GCD 空隙，跨专精补输出）
        { 45902, SF_MELEE, "鲜血打击" },
        { 45477, SF_MELEE|SF_DEBUFF_KEEP, "冰冷触摸" },
        { 45462, SF_MELEE|SF_DEBUFF_KEEP, "瘟疫打击" },
        { 47541, SF_NONE, "死亡缠绕" },
        { 56815, SF_MELEE, "符文打击" },
        { 48721, SF_MELEE|SF_AOE, "血液沸腾" },
      } },
    // ---- 元素 (cls 7 spec 0) ----
    { 7, 0, "元素", "远程DPS", ROLE_DPS,
      {   // rotation
        { 51505, SF_NONE, "熔岩爆裂" },
        { 8050, SF_DEBUFF_KEEP, "烈焰震击" },
        { 421, SF_AOE|SF_NO_MOVE, "闪电链" },
        { 8042, SF_NONE, "大地震击" },
        { 403, SF_NO_MOVE, "闪电箭" },
      },
      {   // burst
        { 51490, SF_SELF|SF_AOE, "雷暴" },
        { 2825, SF_SELF, "嗜血" },
        { 2894, SF_SELF, "火元素图腾" },
      },
      {   // defensive
        { 30823, SF_SELF, "萨满之怒" },
        { 2645, SF_SELF, "幽魂之狼" },
        { 21169, SF_SELF|SF_MANUAL, "重生" },
      },
      {   // buffs
        { 324, SF_SELF|SF_BUFF_KEEP|SF_EXCLUSIVE_BUFF, "闪电之盾" },
        { 30706, SF_SELF|SF_DEPLOYABLE|SF_EXCLUSIVE_BUFF, "愤怒图腾" },
        { 8024, SF_SELF|SF_MANUAL, "火舌武器" },
        { 3738, SF_SELF|SF_DEPLOYABLE|SF_EXCLUSIVE_BUFF, "空气之怒图腾" },
        { 5675, SF_SELF|SF_DEPLOYABLE|SF_EXCLUSIVE_BUFF, "法力之泉图腾" },
      },
      {   // tankKit

      },
      {   // healKit
        { 8004, SF_HEAL|SF_NO_MOVE, "次级治疗波" },
      },
      {   // utility
        { 57994, SF_INTERRUPT, "风剪" },
        { 526, SF_DISPEL_FRIEND, "净化毒素" },
        { 51886, SF_DISPEL_FRIEND, "净化精神" },
        { 370, SF_DISPEL_FRIEND, "净化" },
        { 8143, SF_FREE_SELF|SF_SELF, "战栗图腾" },
      },
      {   // filler（v3.3：填补 GCD 空隙，跨专精补输出）
        { 403, SF_NO_MOVE, "闪电箭" },
        { 8042, SF_NONE, "大地震击" },
        { 8050, SF_DEBUFF_KEEP, "烈焰震击" },
        { 421, SF_AOE|SF_NO_MOVE, "闪电链" },
        { 8056, SF_NONE, "冰霜震击" },
        { 1535, SF_AOE|SF_SELF, "火焰新星" },
      } },
    // ---- 增强 (cls 7 spec 1) ----
    { 7, 1, "增强", "近战DPS", ROLE_DPS,
      {   // rotation
        { 17364, SF_MELEE, "风暴打击" },
        { 8050, SF_MELEE|SF_DEBUFF_KEEP, "烈焰震击" },
        { 8042, SF_MELEE, "大地震击" },
        { 1535, SF_AOE|SF_SELF, "火焰新星" },
        { 421, SF_AOE|SF_NO_MOVE, "闪电链" },
        { 403, SF_NO_MOVE, "闪电箭" },
      },
      {   // burst
        { 51533, SF_SELF, "野性狼魂" },
        { 2825, SF_SELF, "嗜血" },
        { 2894, SF_SELF, "火元素图腾" },
      },
      {   // defensive
        { 30823, SF_SELF, "萨满之怒" },
        { 2645, SF_SELF, "幽魂之狼" },
        { 21169, SF_SELF|SF_MANUAL, "重生" },
      },
      {   // buffs
        { 324, SF_SELF|SF_BUFF_KEEP|SF_EXCLUSIVE_BUFF, "闪电之盾" },
        { 8232, SF_SELF|SF_MANUAL, "风怒武器" },
        { 8075, SF_SELF|SF_DEPLOYABLE|SF_EXCLUSIVE_BUFF, "大地之力图腾" },
        { 8512, SF_SELF|SF_DEPLOYABLE|SF_EXCLUSIVE_BUFF, "风怒图腾" },
        { 8227, SF_SELF|SF_DEPLOYABLE|SF_EXCLUSIVE_BUFF, "火舌图腾" },
      },
      {   // tankKit

      },
      {   // healKit
        { 8004, SF_HEAL|SF_NO_MOVE, "次级治疗波" },
      },
      {   // utility
        { 57994, SF_INTERRUPT, "风剪" },
        { 526, SF_DISPEL_FRIEND, "净化毒素" },
        { 51886, SF_DISPEL_FRIEND, "净化精神" },
        { 370, SF_DISPEL_FRIEND, "净化" },
        { 8143, SF_FREE_SELF|SF_SELF, "战栗图腾" },
      },
      {   // filler（v3.3：填补 GCD 空隙，跨专精补输出）
        { 403, SF_NO_MOVE, "闪电箭" },
        { 8042, SF_NONE, "大地震击" },
        { 8050, SF_DEBUFF_KEEP, "烈焰震击" },
        { 421, SF_AOE|SF_NO_MOVE, "闪电链" },
        { 8056, SF_NONE, "冰霜震击" },
        { 1535, SF_AOE|SF_SELF, "火焰新星" },
      } },
    // ---- 恢复 (cls 7 spec 2) ----
    { 7, 2, "恢复", "治疗", ROLE_HEALER,
      {   // rotation
        { 61295, SF_HEAL|SF_HOT, "激流" },
        { 8004, SF_HEAL|SF_NO_MOVE, "次级治疗波" },
        { 1064, SF_HEAL_AOE|SF_NO_MOVE, "治疗链" },
        { 331, SF_HEAL|SF_NO_MOVE, "治疗波" },
        { 8050, SF_DEBUFF_KEEP, "烈焰震击" },
        { 403, SF_NO_MOVE, "闪电箭" },
      },
      {   // burst
        { 2825, SF_SELF, "嗜血" },
      },
      {   // defensive
        { 2645, SF_SELF, "幽魂之狼" },
        { 21169, SF_SELF|SF_MANUAL, "重生" },
        { 30823, SF_SELF, "萨满之怒" },
      },
      {   // buffs
        { 52127, SF_SELF|SF_BUFF_KEEP|SF_EXCLUSIVE_BUFF, "水之护盾" },
        { 51730, SF_SELF|SF_MANUAL, "大地之赐武器" },
        { 5675, SF_SELF|SF_DEPLOYABLE|SF_EXCLUSIVE_BUFF, "法力之泉图腾" },
        { 8075, SF_SELF|SF_DEPLOYABLE|SF_EXCLUSIVE_BUFF, "大地之力图腾" },
        { 8512, SF_SELF|SF_DEPLOYABLE|SF_EXCLUSIVE_BUFF, "风怒图腾" },
      },
      {   // tankKit

      },
      {   // healKit
        { 16188, SF_HEAL_EMERG|SF_SELF, "自然迅捷" },
        { 55198, SF_HEAL_EMERG|SF_SELF, "潮汐之力" },
        { 61295, SF_HEAL|SF_HOT, "激流" },
        { 1064, SF_HEAL_AOE|SF_NO_MOVE, "治疗链" },
        { 8004, SF_HEAL|SF_NO_MOVE, "次级治疗波" },
        { 331, SF_HEAL|SF_NO_MOVE, "治疗波" },
        { 974, SF_MAINTAIN_FRIEND|SF_BUFF_KEEP, "大地之盾" },
        { 5394, SF_SELF|SF_DEPLOYABLE, "治疗之泉图腾" },
        { 16190, SF_SELF|SF_MANA_LOW, "海潮图腾" },
      },
      {   // utility
        { 57994, SF_INTERRUPT, "风剪" },
        { 526, SF_DISPEL_FRIEND, "净化毒素" },
        { 51886, SF_DISPEL_FRIEND, "净化精神" },
        { 370, SF_DISPEL_FRIEND, "净化" },
        { 8143, SF_FREE_SELF|SF_SELF, "战栗图腾" },
      },
      {   // filler（v3.3：填补 GCD 空隙，跨专精补输出）
        { 403, SF_NO_MOVE, "闪电箭" },
        { 8042, SF_NONE, "大地震击" },
        { 8050, SF_DEBUFF_KEEP, "烈焰震击" },
        { 421, SF_AOE|SF_NO_MOVE, "闪电链" },
        { 8056, SF_NONE, "冰霜震击" },
        { 1535, SF_AOE|SF_SELF, "火焰新星" },
      } },
    // ---- 奥术 (cls 8 spec 0) ----
    { 8, 0, "奥术", "远程DPS", ROLE_DPS,
      {   // rotation
        { 30451, SF_NO_MOVE, "奥术冲击" },
        { 44614, SF_NO_MOVE, "霜火箭" },
        { 5143, SF_NO_MOVE, "奥术飞弹" },
        { 2120, SF_AOE|SF_NO_MOVE, "烈焰风暴" },
        { 10, SF_AOE|SF_NO_MOVE, "暴风雪" },
        { 2136, SF_NONE, "火焰冲击" },
      },
      {   // burst
        { 12042, SF_SELF, "奥术强化" },
        { 12043, SF_SELF, "洞察先机" },
        { 55342, SF_SELF, "镜像" },
      },
      {   // defensive
        { 45438, SF_SELF, "寒冰屏障" },
        { 11426, SF_SELF, "寒冰护体" },
        { 1953, SF_SELF, "闪现术" },
        { 122, SF_SELF|SF_AOE, "冰霜新星" },
      },
      {   // buffs
        { 30482, SF_SELF|SF_BUFF_KEEP|SF_EXCLUSIVE_BUFF, "熔岩护甲" },
        { 1459, SF_RAID_BUFF|SF_BUFF_KEEP, "奥术智慧" },
        { 1008, SF_RAID_BUFF|SF_BUFF_KEEP|SF_MANUAL|SF_EXCLUSIVE_BUFF, "强化法术" },
        { 54646, SF_MAINTAIN_FRIEND|SF_BUFF_KEEP, "魔法专注" },
      },
      {   // tankKit

      },
      {   // healKit

      },
      {   // utility
        { 2139, SF_INTERRUPT, "法术反制" },
        { 475, SF_DISPEL_FRIEND, "解除诅咒" },
        { 45438, SF_FREE_SELF|SF_SELF, "寒冰屏障" },
        { 12051, SF_SELF|SF_MANA_LOW, "唤醒" },
      },
      {   // filler（v3.3：填补 GCD 空隙，跨专精补输出）
        { 2136, SF_NONE, "火焰冲击" },
        { 116, SF_NO_MOVE, "寒冰箭" },
        { 133, SF_NO_MOVE, "火球术" },
        { 2948, SF_NO_MOVE, "灼烧" },
        { 30455, SF_NONE, "冰枪术" },
        { 10, SF_AOE|SF_NO_MOVE, "暴风雪" },
        { 122, SF_AOE|SF_SELF, "冰霜新星" },
      } },
    // ---- 火焰 (cls 8 spec 1) ----
    { 8, 1, "火焰", "远程DPS", ROLE_DPS,
      {   // rotation
        { 44457, SF_DEBUFF_KEEP, "活动炸弹" },
        { 11366, SF_NO_MOVE, "炎爆术" },
        { 133, SF_NO_MOVE, "火球术" },
        { 2948, SF_NO_MOVE, "灼烧" },
        { 2136, SF_NONE, "火焰冲击" },
        { 2120, SF_AOE|SF_NO_MOVE, "烈焰风暴" },
        { 11113, SF_AOE|SF_SELF, "冲击波" },
        { 31661, SF_AOE|SF_SELF, "龙息术" },
      },
      {   // burst
        { 11129, SF_SELF, "燃烧" },
        { 12043, SF_SELF, "洞察先机" },
        { 55342, SF_SELF, "镜像" },
      },
      {   // defensive
        { 45438, SF_SELF, "寒冰屏障" },
        { 11426, SF_SELF, "寒冰护体" },
        { 1953, SF_SELF, "闪现术" },
        { 122, SF_SELF|SF_AOE, "冰霜新星" },
      },
      {   // buffs
        { 30482, SF_SELF|SF_BUFF_KEEP|SF_EXCLUSIVE_BUFF, "熔岩护甲" },
        { 1459, SF_RAID_BUFF|SF_BUFF_KEEP, "奥术智慧" },
        { 1008, SF_RAID_BUFF|SF_BUFF_KEEP|SF_MANUAL|SF_EXCLUSIVE_BUFF, "强化法术" },
        { 54646, SF_MAINTAIN_FRIEND|SF_BUFF_KEEP, "魔法专注" },
      },
      {   // tankKit

      },
      {   // healKit

      },
      {   // utility
        { 2139, SF_INTERRUPT, "法术反制" },
        { 475, SF_DISPEL_FRIEND, "解除诅咒" },
        { 45438, SF_FREE_SELF|SF_SELF, "寒冰屏障" },
        { 12051, SF_SELF|SF_MANA_LOW, "唤醒" },
      },
      {   // filler（v3.3：填补 GCD 空隙，跨专精补输出）
        { 2136, SF_NONE, "火焰冲击" },
        { 116, SF_NO_MOVE, "寒冰箭" },
        { 133, SF_NO_MOVE, "火球术" },
        { 2948, SF_NO_MOVE, "灼烧" },
        { 30455, SF_NONE, "冰枪术" },
        { 10, SF_AOE|SF_NO_MOVE, "暴风雪" },
        { 122, SF_AOE|SF_SELF, "冰霜新星" },
      } },
    // ---- 冰霜 (cls 8 spec 2) ----
    { 8, 2, "冰霜", "远程DPS", ROLE_DPS,
      {   // rotation
        { 44572, SF_NONE, "深度冻结" },
        { 30455, SF_NONE, "冰枪术" },
        { 44614, SF_NO_MOVE, "霜火箭" },
        { 116, SF_NO_MOVE, "寒冰箭" },
        { 2136, SF_NONE, "火焰冲击" },
        { 10, SF_AOE|SF_NO_MOVE, "暴风雪" },
        { 120, SF_AOE|SF_SELF, "冰锥术" },
      },
      {   // burst
        { 12472, SF_SELF, "冰冷血脉" },
        { 31687, SF_SELF, "召唤水元素" },
        { 11958, SF_SELF, "急速冷却" },
        { 55342, SF_SELF, "镜像" },
      },
      {   // defensive
        { 45438, SF_SELF, "寒冰屏障" },
        { 11426, SF_SELF, "寒冰护体" },
        { 1953, SF_SELF, "闪现术" },
        { 122, SF_SELF|SF_AOE, "冰霜新星" },
      },
      {   // buffs
        { 7302, SF_SELF|SF_BUFF_KEEP|SF_EXCLUSIVE_BUFF, "寒冰护甲" },
        { 1459, SF_RAID_BUFF|SF_BUFF_KEEP, "奥术智慧" },
        { 604, SF_RAID_BUFF|SF_BUFF_KEEP|SF_MANUAL|SF_EXCLUSIVE_BUFF, "法术抑制" },
        { 54646, SF_MAINTAIN_FRIEND|SF_BUFF_KEEP, "魔法专注" },
      },
      {   // tankKit

      },
      {   // healKit

      },
      {   // utility
        { 2139, SF_INTERRUPT, "法术反制" },
        { 475, SF_DISPEL_FRIEND, "解除诅咒" },
        { 45438, SF_FREE_SELF|SF_SELF, "寒冰屏障" },
        { 12051, SF_SELF|SF_MANA_LOW, "唤醒" },
      },
      {   // filler（v3.3：填补 GCD 空隙，跨专精补输出）
        { 2136, SF_NONE, "火焰冲击" },
        { 116, SF_NO_MOVE, "寒冰箭" },
        { 133, SF_NO_MOVE, "火球术" },
        { 2948, SF_NO_MOVE, "灼烧" },
        { 30455, SF_NONE, "冰枪术" },
        { 10, SF_AOE|SF_NO_MOVE, "暴风雪" },
        { 122, SF_AOE|SF_SELF, "冰霜新星" },
      } },
    // ---- 痛苦 (cls 9 spec 0) ----
    { 9, 0, "痛苦", "远程DPS", ROLE_DPS,
      {   // rotation
        { 48181, SF_NONE, "鬼影缠身" },
        { 30108, SF_DEBUFF_KEEP, "痛苦无常" },
        { 172, SF_DEBUFF_KEEP, "腐蚀术" },
        { 980, SF_DEBUFF_KEEP, "痛苦诅咒" },
        { 1490, SF_DEBUFF_KEEP, "元素诅咒" },
        { 27243, SF_AOE, "腐蚀之种" },
        { 1120, SF_NO_MOVE|SF_EXECUTE, "吸取灵魂" },
        { 686, SF_NO_MOVE, "暗影箭" },
      },
      {   // burst
        { 603, SF_DEBUFF_KEEP, "末日诅咒" },
      },
      {   // defensive
        { 6229, SF_SELF, "暗影防护" },
        { 5484, SF_SELF|SF_AOE, "恐惧嚎叫" },
        { 6789, SF_NONE, "死亡缠绕" },
        { 29858, SF_SELF, "灵魂粉碎" },
      },
      {   // buffs
        { 28176, SF_SELF|SF_BUFF_KEEP|SF_EXCLUSIVE_BUFF, "魔誓防护" },
        { 5697, SF_RAID_BUFF|SF_BUFF_KEEP, "无尽呼吸" },
      },
      {   // tankKit

      },
      {   // healKit

      },
      {   // utility
        { 1454, SF_SELF|SF_MANA_LOW, "生命分流" },
        { 18220, SF_SELF|SF_MANA_LOW, "黑暗契约" },
      },
      {   // filler（v3.3：填补 GCD 空隙，跨专精补输出）
        { 686, SF_NO_MOVE, "暗影箭" },
        { 172, SF_DEBUFF_KEEP, "腐蚀术" },
        { 348, SF_DEBUFF_KEEP, "献祭" },
        { 980, SF_DEBUFF_KEEP, "痛苦诅咒" },
        { 29722, SF_NO_MOVE, "烧尽" },
        { 5676, SF_NO_MOVE, "灼热之痛" },
        { 17877, SF_EXECUTE, "暗影灼烧" },
      } },
    // ---- 恶魔学识 (cls 9 spec 1) ----
    { 9, 1, "恶魔学识", "远程DPS", ROLE_DPS,
      {   // rotation
        { 172, SF_DEBUFF_KEEP, "腐蚀术" },
        { 348, SF_DEBUFF_KEEP, "献祭" },
        { 1490, SF_DEBUFF_KEEP, "元素诅咒" },
        { 6353, SF_NO_MOVE, "灵魂之火" },
        { 29722, SF_NO_MOVE, "烧尽" },
        { 27243, SF_AOE, "腐蚀之种" },
        { 686, SF_NO_MOVE, "暗影箭" },
      },
      {   // burst
        { 603, SF_DEBUFF_KEEP, "末日诅咒" },
      },
      {   // defensive
        { 6229, SF_SELF, "暗影防护" },
        { 5484, SF_SELF|SF_AOE, "恐惧嚎叫" },
        { 29858, SF_SELF, "灵魂粉碎" },
      },
      {   // buffs
        { 28176, SF_SELF|SF_BUFF_KEEP|SF_EXCLUSIVE_BUFF, "魔誓防护" },
      },
      {   // tankKit

      },
      {   // healKit

      },
      {   // utility
        { 1454, SF_SELF|SF_MANA_LOW, "生命分流" },
        { 18220, SF_SELF|SF_MANA_LOW, "黑暗契约" },
      },
      {   // filler（v3.3：填补 GCD 空隙，跨专精补输出）
        { 686, SF_NO_MOVE, "暗影箭" },
        { 172, SF_DEBUFF_KEEP, "腐蚀术" },
        { 348, SF_DEBUFF_KEEP, "献祭" },
        { 980, SF_DEBUFF_KEEP, "痛苦诅咒" },
        { 29722, SF_NO_MOVE, "烧尽" },
        { 5676, SF_NO_MOVE, "灼热之痛" },
        { 17877, SF_EXECUTE, "暗影灼烧" },
      } },
    // ---- 毁灭 (cls 9 spec 2) ----
    { 9, 2, "毁灭", "远程DPS", ROLE_DPS,
      {   // rotation
        { 50796, SF_NO_MOVE, "混乱之箭" },
        { 17962, SF_NONE, "燃烧" },
        { 348, SF_DEBUFF_KEEP, "献祭" },
        { 29722, SF_NO_MOVE, "烧尽" },
        { 17877, SF_EXECUTE, "暗影灼烧" },
        { 1490, SF_DEBUFF_KEEP, "元素诅咒" },
        { 5740, SF_AOE|SF_NO_MOVE, "火焰之雨" },
        { 686, SF_NO_MOVE, "暗影箭" },
      },
      {   // burst
        { 6353, SF_NO_MOVE, "灵魂之火" },
      },
      {   // defensive
        { 6229, SF_SELF, "暗影防护" },
        { 5484, SF_SELF|SF_AOE, "恐惧嚎叫" },
        { 30283, SF_AOE, "暗影之怒" },
        { 29858, SF_SELF, "灵魂粉碎" },
      },
      {   // buffs
        { 28176, SF_SELF|SF_BUFF_KEEP|SF_EXCLUSIVE_BUFF, "魔誓防护" },
      },
      {   // tankKit

      },
      {   // healKit

      },
      {   // utility
        { 1454, SF_SELF|SF_MANA_LOW, "生命分流" },
        { 18220, SF_SELF|SF_MANA_LOW, "黑暗契约" },
      },
      {   // filler（v3.3：填补 GCD 空隙，跨专精补输出）
        { 686, SF_NO_MOVE, "暗影箭" },
        { 172, SF_DEBUFF_KEEP, "腐蚀术" },
        { 348, SF_DEBUFF_KEEP, "献祭" },
        { 980, SF_DEBUFF_KEEP, "痛苦诅咒" },
        { 29722, SF_NO_MOVE, "烧尽" },
        { 5676, SF_NO_MOVE, "灼热之痛" },
        { 17877, SF_EXECUTE, "暗影灼烧" },
      } },
    // ---- 平衡 (cls 11 spec 0) ----
    { 11, 0, "平衡", "远程DPS", ROLE_DPS,
      {   // rotation
        { 8921, SF_DEBUFF_KEEP, "月火术" },
        { 5570, SF_DEBUFF_KEEP, "虫群" },
        { 2912, SF_NO_MOVE, "星火术" },
        { 5176, SF_NO_MOVE, "愤怒" },
        { 16914, SF_AOE|SF_NO_MOVE, "飓风" },
        { 770, SF_DEBUFF_KEEP, "精灵之火" },
      },
      {   // burst
        { 48505, SF_SELF|SF_NO_MOVE, "星辰坠落" },
        { 50516, SF_AOE|SF_SELF, "台风" },
        { 33831, SF_SELF, "自然之力" },
      },
      {   // defensive
        { 22812, SF_SELF, "树皮术" },
        { 33786, SF_NONE, "飓风术" },
        { 16689, SF_SELF, "自然之握" },
      },
      {   // buffs
        { 24858, SF_SELF|SF_BUFF_KEEP|SF_EXCLUSIVE_BUFF, "枭兽形态" },
        { 1126, SF_RAID_BUFF|SF_BUFF_KEEP, "野性印记" },
        { 467, SF_RAID_BUFF|SF_BUFF_KEEP, "尖刺" },
      },
      {   // tankKit

      },
      {   // healKit
        { 8936, SF_HEAL|SF_NO_MOVE, "愈合" },
        { 774, SF_HEAL|SF_HOT, "回春术" },
      },
      {   // utility
        { 2782, SF_DISPEL_FRIEND, "解除诅咒" },
        { 8946, SF_DISPEL_FRIEND, "消毒术" },
        { 2893, SF_DISPEL_FRIEND, "驱毒术" },
        { 22812, SF_SELF, "树皮术" },
        { 29166, SF_SELF|SF_MANA_LOW, "激活" },
      },
      {   // filler（v3.3：填补 GCD 空隙，跨专精补输出）
        { 8921, SF_DEBUFF_KEEP, "月火术" },
        { 5176, SF_NO_MOVE, "愤怒" },
        { 6807, SF_MELEE, "槌击" },
        { 779, SF_MELEE|SF_AOE, "横扫(熊)" },
        { 1082, SF_MELEE|SF_COMBO_BUILD, "爪击" },
      } },
    // ---- 野性-猫 (cls 11 spec 1) ----
    { 11, 1, "野性-猫", "近战DPS", ROLE_DPS,
      {   // rotation
        { 22568, SF_MELEE|SF_COMBO_FINISH, "凶猛撕咬" },
        { 1079, SF_MELEE|SF_COMBO_FINISH, "割裂" },
        { 52610, SF_MELEE|SF_COMBO_FINISH, "野蛮咆哮" },
        { 1822, SF_MELEE|SF_DEBUFF_KEEP, "斜掠" },
        { 33876, SF_MELEE|SF_COMBO_BUILD, "裂伤" },
        { 5221, SF_MELEE|SF_COMBO_BUILD, "撕碎" },
        { 62078, SF_MELEE|SF_AOE, "横扫" },
        { 1082, SF_MELEE|SF_COMBO_BUILD, "爪击" },
        { 16857, SF_DEBUFF_KEEP, "精灵之火(野性)" },
      },
      {   // burst
        { 5217, SF_SELF, "猛虎之怒" },
        { 50334, SF_SELF, "狂暴" },
        { 1850, SF_SELF, "急奔" },
      },
      {   // defensive
        { 22842, SF_SELF, "狂暴回复" },
        { 61336, SF_SELF, "生存本能" },
        { 8998, SF_SELF, "畏缩" },
        { 5215, SF_SELF, "潜行" },
      },
      {   // buffs
        { 768, SF_SELF|SF_BUFF_KEEP|SF_EXCLUSIVE_BUFF, "猎豹形态" },
        { 1126, SF_RAID_BUFF|SF_BUFF_KEEP, "野性印记" },
      },
      {   // tankKit

      },
      {   // healKit
        { 774, SF_HEAL|SF_HOT, "回春术" },
      },
      {   // utility
        { 2782, SF_DISPEL_FRIEND, "解除诅咒" },
        { 8946, SF_DISPEL_FRIEND, "消毒术" },
        { 2893, SF_DISPEL_FRIEND, "驱毒术" },
        { 22812, SF_SELF, "树皮术" },
        { 29166, SF_SELF|SF_MANA_LOW, "激活" },
      },
      {   // filler（v3.3：填补 GCD 空隙，跨专精补输出）
        { 8921, SF_DEBUFF_KEEP, "月火术" },
        { 5176, SF_NO_MOVE, "愤怒" },
        { 6807, SF_MELEE, "槌击" },
        { 779, SF_MELEE|SF_AOE, "横扫(熊)" },
        { 1082, SF_MELEE|SF_COMBO_BUILD, "爪击" },
      } },
    // ---- 野性-熊 (cls 11 spec 2) ----
    { 11, 2, "野性-熊", "坦克", ROLE_TANK,
      {   // rotation
        { 33878, SF_MELEE, "裂伤(熊)" },
        { 33745, SF_MELEE|SF_DEBUFF_KEEP, "割碎" },
        { 779, SF_MELEE|SF_AOE, "横扫(熊)" },
        { 6807, SF_MELEE, "槌击" },
        { 5211, SF_INTERRUPT|SF_MELEE, "重击" },
        { 6795, SF_TAUNT|SF_MELEE, "低吼" },
        { 16857, SF_DEBUFF_KEEP, "精灵之火(野性)" },
      },
      {   // burst
        { 50334, SF_SELF, "狂暴" },
        { 5229, SF_SELF, "激怒" },
        { 5209, SF_TAUNT_AOE|SF_SELF, "挑战咆哮" },
      },
      {   // defensive
        { 22842, SF_SELF, "狂暴回复" },
        { 61336, SF_SELF, "生存本能" },
        { 22812, SF_SELF, "树皮术" },
      },
      {   // buffs
        { 5487, SF_SELF|SF_BUFF_KEEP|SF_EXCLUSIVE_BUFF, "熊形态" },
        { 1126, SF_RAID_BUFF|SF_BUFF_KEEP, "野性印记" },
        { 467, SF_RAID_BUFF|SF_BUFF_KEEP, "尖刺" },
      },
      {   // tankKit
        { 6795, SF_TAUNT|SF_MELEE, "低吼" },
        { 5209, SF_TAUNT_AOE|SF_SELF, "挑战咆哮" },
        { 5211, SF_INTERRUPT|SF_MELEE, "重击" },
        { 61336, SF_SELF, "生存本能" },
        { 22842, SF_SELF, "狂暴回复" },
      },
      {   // healKit

      },
      {   // utility
        { 2782, SF_DISPEL_FRIEND, "解除诅咒" },
        { 8946, SF_DISPEL_FRIEND, "消毒术" },
        { 2893, SF_DISPEL_FRIEND, "驱毒术" },
        { 22812, SF_SELF, "树皮术" },
        { 29166, SF_SELF|SF_MANA_LOW, "激活" },
      },
      {   // filler（v3.3：填补 GCD 空隙，跨专精补输出）
        { 8921, SF_DEBUFF_KEEP, "月火术" },
        { 5176, SF_NO_MOVE, "愤怒" },
        { 6807, SF_MELEE, "槌击" },
        { 779, SF_MELEE|SF_AOE, "横扫(熊)" },
        { 1082, SF_MELEE|SF_COMBO_BUILD, "爪击" },
      } },
    // ---- 恢复 (cls 11 spec 3) ----
    { 11, 3, "恢复", "治疗", ROLE_HEALER,
      {   // rotation
        { 33763, SF_HEAL|SF_HOT, "生命之花" },
        { 774, SF_HEAL|SF_HOT, "回春术" },
        { 48438, SF_HEAL_AOE, "野性成长" },
        { 18562, SF_HEAL|SF_HEAL_EMERG, "迅捷治愈" },
        { 50464, SF_HEAL|SF_NO_MOVE, "滋养" },
        { 8936, SF_HEAL|SF_NO_MOVE, "愈合" },
        { 5185, SF_HEAL|SF_NO_MOVE, "治疗之触" },
        { 8921, SF_DEBUFF_KEEP, "月火术" },
      },
      {   // burst

      },
      {   // defensive
        { 22812, SF_SELF, "树皮术" },
        { 33786, SF_NONE, "飓风术" },
        { 16689, SF_SELF, "自然之握" },
      },
      {   // buffs
        { 33891, SF_SELF|SF_BUFF_KEEP|SF_EXCLUSIVE_BUFF, "树形态" },
        { 1126, SF_RAID_BUFF|SF_BUFF_KEEP, "野性印记" },
        { 467, SF_RAID_BUFF|SF_BUFF_KEEP, "尖刺" },
      },
      {   // tankKit

      },
      {   // healKit
        { 17116, SF_HEAL_EMERG|SF_SELF, "自然迅捷" },
        { 18562, SF_HEAL|SF_HEAL_EMERG, "迅捷治愈" },
        { 33763, SF_HEAL|SF_HOT, "生命之花" },
        { 774, SF_HEAL|SF_HOT, "回春术" },
        { 48438, SF_HEAL_AOE, "野性成长" },
        { 50464, SF_HEAL|SF_NO_MOVE, "滋养" },
        { 8936, SF_HEAL|SF_NO_MOVE, "愈合" },
        { 5185, SF_HEAL|SF_NO_MOVE, "治疗之触" },
        { 740, SF_HEAL_AOE|SF_NO_MOVE, "宁静" },
      },
      {   // utility
        { 2782, SF_DISPEL_FRIEND, "解除诅咒" },
        { 8946, SF_DISPEL_FRIEND, "消毒术" },
        { 2893, SF_DISPEL_FRIEND, "驱毒术" },
        { 22812, SF_SELF, "树皮术" },
        { 29166, SF_SELF|SF_MANA_LOW, "激活" },
      },
      {   // filler（v3.3：填补 GCD 空隙，跨专精补输出）
        { 8921, SF_DEBUFF_KEEP, "月火术" },
        { 5176, SF_NO_MOVE, "愤怒" },
        { 6807, SF_MELEE, "槌击" },
        { 779, SF_MELEE|SF_AOE, "横扫(熊)" },
        { 1082, SF_MELEE|SF_COMBO_BUILD, "爪击" },
      } },
    };
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
