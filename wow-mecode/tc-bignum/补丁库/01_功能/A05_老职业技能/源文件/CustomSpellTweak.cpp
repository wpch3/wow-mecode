/*
 * CustomSpellTweak.cpp - 老职业技能强化实现
 * 详见 CustomSpellTweak.h 头部说明
 */

#include "CustomSpellTweak.h"
#include "Config.h"
#include "Log.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include <algorithm>
#include <sstream>

CustomSpellTweakMgr* CustomSpellTweakMgr::instance()
{
    static CustomSpellTweakMgr inst;
    return &inst;
}

void CustomSpellTweakMgr::LoadConfig()
{
    _enabled     = sConfigMgr->GetBoolDefault("CustomSpell.Enable", true);
    _globalScale = sConfigMgr->GetFloatDefault("CustomSpell.GlobalScale", 1.0f);
    _globalScale = std::clamp(_globalScale, 0.1f, 10.0f);

    _warrior = sConfigMgr->GetBoolDefault("CustomSpell.Warrior",     true);
    _paladin = sConfigMgr->GetBoolDefault("CustomSpell.Paladin",     true);
    _hunter  = sConfigMgr->GetBoolDefault("CustomSpell.Hunter",      true);
    _rogue   = sConfigMgr->GetBoolDefault("CustomSpell.Rogue",       true);
    _priest  = sConfigMgr->GetBoolDefault("CustomSpell.Priest",      true);
    _dk      = sConfigMgr->GetBoolDefault("CustomSpell.DeathKnight", true);
    _shaman  = sConfigMgr->GetBoolDefault("CustomSpell.Shaman",      true);
    _mage    = sConfigMgr->GetBoolDefault("CustomSpell.Mage",        true);
    _warlock = sConfigMgr->GetBoolDefault("CustomSpell.Warlock",     true);
    _druid   = sConfigMgr->GetBoolDefault("CustomSpell.Druid",       true);
}

void CustomSpellTweakMgr::Log(uint32 spellId, char const* cls,
                              std::string const& what, bool ok, char const* reason)
{
    TweakLog l;
    l.spellId = spellId;
    l.cls     = cls;
    l.what    = what;
    l.applied = ok;
    l.reason  = reason;

    SpellInfo const* si = sSpellMgr->GetSpellInfo(spellId);
    if (si && si->SpellName[0])
        l.spellName = si->SpellName[0];
    else
        l.spellName = "(未知)";

    _log.push_back(l);
    if (ok)
        ++_applied;
}

// ============================================================================
//  底层操作
// ============================================================================

bool CustomSpellTweakMgr::ScaleEffect(uint32 spellId, float mult,
                                      char const* cls, char const* desc)
{
    SpellInfo* si = const_cast<SpellInfo*>(sSpellMgr->GetSpellInfo(spellId));
    if (!si)
    {
        Log(spellId, cls, desc, false, "法术不存在");
        return false;
    }

    float m = mult * _globalScale;
    bool touched = false;

    for (SpellEffectInfo& eff : si->_effects)
    {
        if (!eff.Effect)
            continue;
        if (eff.BasePoints == 0)
            continue;

        // int32 溢出保护（本服上限 21 亿）
        double v = double(eff.BasePoints) * double(m);
        if (v >  2100000000.0) v =  2100000000.0;
        if (v < -2100000000.0) v = -2100000000.0;
        eff.BasePoints = int32(v);

        // DieSides 同步放大，否则伤害浮动范围会失衡
        if (eff.DieSides != 0)
        {
            double d = double(eff.DieSides) * double(m);
            if (d >  2100000000.0) d =  2100000000.0;
            if (d < -2100000000.0) d = -2100000000.0;
            eff.DieSides = int32(d);
        }

        touched = true;
    }

    if (!touched)
    {
        Log(spellId, cls, desc, false, "该法术没有可缩放的数值");
        return false;
    }

    std::ostringstream ss;
    ss << desc << "（x" << m << "）";
    Log(spellId, cls, ss.str(), true);
    return true;
}

bool CustomSpellTweakMgr::ScaleCooldown(uint32 spellId, float mult,
                                        char const* cls, char const* desc)
{
    SpellInfo* si = const_cast<SpellInfo*>(sSpellMgr->GetSpellInfo(spellId));
    if (!si)
    {
        Log(spellId, cls, desc, false, "法术不存在");
        return false;
    }

    if (si->RecoveryTime == 0 && si->CategoryRecoveryTime == 0)
    {
        Log(spellId, cls, desc, false, "该法术本来就没有冷却");
        return false;
    }

    if (si->RecoveryTime)
        si->RecoveryTime = uint32(float(si->RecoveryTime) * mult);
    if (si->CategoryRecoveryTime)
        si->CategoryRecoveryTime = uint32(float(si->CategoryRecoveryTime) * mult);

    std::ostringstream ss;
    ss << desc << "（CD x" << mult << "）";
    Log(spellId, cls, ss.str(), true);
    return true;
}

bool CustomSpellTweakMgr::SetCooldown(uint32 spellId, uint32 ms,
                                      char const* cls, char const* desc)
{
    SpellInfo* si = const_cast<SpellInfo*>(sSpellMgr->GetSpellInfo(spellId));
    if (!si)
    {
        Log(spellId, cls, desc, false, "法术不存在");
        return false;
    }

    si->RecoveryTime = ms;
    if (si->CategoryRecoveryTime)
        si->CategoryRecoveryTime = ms;

    std::ostringstream ss;
    ss << desc << "（CD = " << (ms / 1000) << "秒）";
    Log(spellId, cls, ss.str(), true);
    return true;
}

bool CustomSpellTweakMgr::AddChainTargets(uint32 spellId, int32 add,
                                          char const* cls, char const* desc)
{
    SpellInfo* si = const_cast<SpellInfo*>(sSpellMgr->GetSpellInfo(spellId));
    if (!si)
    {
        Log(spellId, cls, desc, false, "法术不存在");
        return false;
    }

    bool touched = false;
    for (SpellEffectInfo& eff : si->_effects)
    {
        if (!eff.Effect)
            continue;
        if (eff.ChainTargets == 0)
            continue;

        int32 n = int32(eff.ChainTargets) + add;
        eff.ChainTargets = uint32(std::clamp(n, 1, 50));
        touched = true;
    }

    if (!touched)
    {
        Log(spellId, cls, desc, false, "该法术不是链式法术");
        return false;
    }

    std::ostringstream ss;
    ss << desc << "（跳跃 +" << add << "）";
    Log(spellId, cls, ss.str(), true);
    return true;
}

bool CustomSpellTweakMgr::SetMaxTargets(uint32 spellId, uint32 n,
                                        char const* cls, char const* desc)
{
    SpellInfo* si = const_cast<SpellInfo*>(sSpellMgr->GetSpellInfo(spellId));
    if (!si)
    {
        Log(spellId, cls, desc, false, "法术不存在");
        return false;
    }

    si->MaxAffectedTargets = n;

    std::ostringstream ss;
    ss << desc << "（最多 " << n << " 个目标）";
    Log(spellId, cls, ss.str(), true);
    return true;
}

bool CustomSpellTweakMgr::SetSummonCount(uint32 spellId, int32 n,
                                         char const* cls, char const* desc)
{
    /*
     * 自动追踪 TriggerSpell。
     *
     * 很多召唤类法术（如亡者大军 42650）本身不是 SPELL_EFFECT_SUMMON，
     * 而是用 SPELL_EFFECT_TRIGGER_SPELL 触发另一个法术去召唤。
     * 实测报「该法术不是召唤类」就是这个原因。
     *
     * 这里最多追 3 层，避免写死被触发法术的 ID（不同整合包可能不同）。
     */
    uint32 cur = spellId;
    uint32 realSummoner = 0;

    for (int depth = 0; depth < 3 && cur; ++depth)
    {
        SpellInfo* si = const_cast<SpellInfo*>(sSpellMgr->GetSpellInfo(cur));
        if (!si)
            break;

        // 先看这一层有没有召唤效果
        bool hasSummon = false;
        for (SpellEffectInfo const& eff : si->_effects)
        {
            if (eff.Effect == SPELL_EFFECT_SUMMON)
            {
                hasSummon = true;
                break;
            }
        }

        if (hasSummon)
        {
            realSummoner = cur;
            break;
        }

        // 没有就往下追 TriggerSpell
        uint32 next = 0;
        for (SpellEffectInfo const& eff : si->_effects)
        {
            if (eff.TriggerSpell)
            {
                next = eff.TriggerSpell;
                break;
            }
        }
        cur = next;
    }

    if (!realSummoner)
    {
        Log(spellId, cls, desc, false, "该法术不是召唤类，也追不到召唤用的 TriggerSpell");
        return false;
    }

    SpellInfo* target = const_cast<SpellInfo*>(sSpellMgr->GetSpellInfo(realSummoner));
    bool touched = false;
    for (SpellEffectInfo& eff : target->_effects)
    {
        if (eff.Effect == SPELL_EFFECT_SUMMON)
        {
            eff.BasePoints = n;
            touched = true;
        }
    }

    if (!touched)
    {
        Log(spellId, cls, desc, false, "内部错误：找到召唤法术却改不了");
        return false;
    }

    std::ostringstream ss;
    ss << desc << "（召唤 " << n << " 个";
    if (realSummoner != spellId)
        ss << "，实际改的是 " << realSummoner;
    ss << "）";
    Log(spellId, cls, ss.str(), true);
    return true;
}

bool CustomSpellTweakMgr::ScaleStack(uint32 spellId, float mult,
                                     char const* cls, char const* desc)
{
    SpellInfo* si = const_cast<SpellInfo*>(sSpellMgr->GetSpellInfo(spellId));
    if (!si)
    {
        Log(spellId, cls, desc, false, "法术不存在");
        return false;
    }

    if (si->StackAmount == 0)
    {
        Log(spellId, cls, desc, false, "该法术不可叠加");
        return false;
    }

    uint32 n = uint32(float(si->StackAmount) * mult);
    si->StackAmount = std::clamp(n, 1u, 999u);

    std::ostringstream ss;
    ss << desc << "（层数 " << si->StackAmount << "）";
    Log(spellId, cls, ss.str(), true);
    return true;
}

// ============================================================================
//  主入口
// ============================================================================

void CustomSpellTweakMgr::ApplyAll()
{
    LoadConfig();

    _log.clear();
    _applied = 0;

    if (!_enabled)
    {
        TC_LOG_INFO("server.loading", ">> 老职业技能强化：已关闭（CustomSpell.Enable = 0）");
        return;
    }

    if (_warrior) ApplyWarrior();
    if (_paladin) ApplyPaladin();
    if (_hunter)  ApplyHunter();
    if (_rogue)   ApplyRogue();
    if (_priest)  ApplyPriest();
    if (_dk)      ApplyDeathKnight();
    if (_shaman)  ApplyShaman();
    if (_mage)    ApplyMage();
    if (_warlock) ApplyWarlock();
    if (_druid)   ApplyDruid();

    uint32 failed = uint32(_log.size()) - _applied;

    TC_LOG_INFO("server.loading",
        ">> 老职业技能强化：应用 {} 条，跳过 {} 条（全局倍率 x{}）",
        _applied, failed, _globalScale);

    if (failed)
    {
        TC_LOG_INFO("server.loading",
            "   跳过的用 .spell tweak 查看原因（多半是该法术在本版本里 ID 不同）");
    }
}

// ============================================================================
//  战士 —— 缺远程手段、自我治疗、团队增益
// ============================================================================
void CustomSpellTweakMgr::ApplyWarrior()
{
    char const* C = "战士";

    // 投掷 —— 战士全程零远程手段，怪跑了只能干瞪眼
    // 投掷只有 2764 这一个 Rank 有实际数值，7266/7267 是空壳（实测跳过）
    ScaleEffect(2764,  3.0f, C, "投掷伤害");

    // 雷霆一击 —— 战士的主要 AOE，原版偏弱
    ScaleEffect(47502, 1.5f, C, "雷霆一击伤害");

    // 战斗怒吼 —— 团队增益
    ScaleEffect(47436, 1.5f, C, "战斗怒吼攻强");

    // 破釜沉舟 —— 爆发窗口，CD 太长
    ScaleCooldown(1719, 0.7f, C, "鲁莽冷却");

    // 血性狂暴 —— 自我治疗
    ScaleEffect(2687,  2.0f, C, "血性狂暴回怒");
}

// ============================================================================
//  圣骑士 —— 缺范围输出、机动性
// ============================================================================
void CustomSpellTweakMgr::ApplyPaladin()
{
    char const* C = "圣骑士";

    // 神圣新星 —— 圣骑唯一的范围治疗+伤害，原版数值可怜
    ScaleEffect(20424, 2.0f, C, "神圣复仇");

    // 奉献 —— 主要 AOE
    ScaleEffect(48819, 1.5f, C, "奉献伤害");

    // 圣光术 —— 主力治疗
    ScaleEffect(48782, 1.3f, C, "圣光术治疗");

    // 圣佑术 —— 保命，CD 5分钟太长
    ScaleCooldown(642, 0.7f, C, "圣盾术冷却");

    // 复仇之怒 —— 爆发
    ScaleCooldown(31884, 0.7f, C, "复仇之怒冷却");

    // 清洁术 4987 本来就没冷却（实测），换成保护之手 —— 圣骑核心保命技
    ScaleCooldown(10278, 0.6f, C, "保护之手冷却");
}

// ============================================================================
//  猎人 —— 缺爆发窗口
// ============================================================================
void CustomSpellTweakMgr::ApplyHunter()
{
    char const* C = "猎人";

    // 瞄准射击 —— 主力爆发
    ScaleEffect(49050, 1.4f, C, "瞄准射击伤害");

    // 多重射击 —— AOE
    ScaleEffect(49048, 1.5f, C, "多重射击伤害");
    AddChainTargets(49048, 2, C, "多重射击目标数");

    // 猎人印记 —— 团队增益
    ScaleEffect(53338, 1.5f, C, "猎人印记");

    // 误导 —— 仇恨转移，CD 太长
    ScaleCooldown(34477, 0.6f, C, "误导冷却");

    // 假死 —— 保命
    ScaleCooldown(5384, 0.6f, C, "假死冷却");
}

// ============================================================================
//  盗贼 —— 缺团队贡献、AOE
// ============================================================================
void CustomSpellTweakMgr::ApplyRogue()
{
    char const* C = "盗贼";

    // 剑刃乱舞 —— 盗贼唯一 AOE，CD 极长
    ScaleCooldown(13877, 0.5f, C, "剑刃乱舞冷却");

    // 破甲 —— 团队增益（盗贼终于有点团队贡献）
    ScaleEffect(8647,  1.5f, C, "破甲效果");

    // 消失 —— 保命
    ScaleCooldown(1856, 0.6f, C, "消失冷却");

    // 闪避 —— 保命
    ScaleCooldown(5277, 0.6f, C, "闪避冷却");

    // 切割 —— 持续输出
    ScaleEffect(48668, 1.3f, C, "毁伤伤害");
}

// ============================================================================
//  牧师 —— 缺机动性、瞬发治疗
// ============================================================================
void CustomSpellTweakMgr::ApplyPriest()
{
    char const* C = "牧师";

    // 神圣新星 —— 范围治疗+伤害，原版几乎没人用
    ScaleEffect(48078, 3.0f, C, "神圣新星");

    // 治疗祷言 —— 群疗
    ScaleEffect(48072, 1.4f, C, "治疗祷言");

    // 真言术：盾 —— 护盾
    ScaleEffect(48066, 1.5f, C, "真言术盾吸收量");

    // 精神鞭笞 —— 暗牧主力
    ScaleEffect(48156, 1.3f, C, "精神鞭笞伤害");

    // 心灵尖啸 —— 保命 AOE 恐惧
    ScaleCooldown(8122, 0.6f, C, "心灵尖啸冷却");

    // 恢复 —— HOT
    ScaleEffect(48068, 1.4f, C, "恢复治疗量");
}

// ============================================================================
//  死亡骑士 —— 强化亡灵主题
// ============================================================================
void CustomSpellTweakMgr::ApplyDeathKnight()
{
    char const* C = "死骑";

    /*
     * 亡者大军 —— 8 只改 10 只
     *
     * 【注意】光改 BasePoints 不够，还要打 apply_summon.sh 补丁。
     *
     * 根因 SpellEffects.cpp:2281：
     *     switch (properties->ID) {
     *         case 64: case 61: ... case 713:
     *             numSummons = (damage > 0) ? damage : 1;   // 白名单才读 BasePoints
     *             break;
     *         default:
     *             numSummons = 1;                            // 亡者大军走这里
     *     }
     * 亡者大军的 SummonProperties ID 不在白名单里，所以数量写死。
     * apply_summon.sh 把 default 分支改成也读 BasePoints 即可。
     */
    /*
     * 亡者大军 42650 本身不是召唤类（实测报「该法术不是召唤类」），
     * 它用 SPELL_EFFECT_TRIGGER_SPELL 触发另一个法术去召唤。
     * SetSummonCount 会自动追踪 TriggerSpell（最多3层），
     * 不用写死被触发法术的 ID —— 不同整合包可能不一样。
     */
    SetSummonCount(42650, 10, C, "亡者大军数量(需 apply_summon.sh)");
    ScaleCooldown(42650, 0.7f, C, "亡者大军冷却");

    // 死亡缠绕 —— 远程手段
    ScaleEffect(49895, 1.5f, C, "死亡缠绕伤害");

    // 血之疫病 / 冰霜疫病 —— 疾病流
    ScaleEffect(55078, 1.4f, C, "血之疫病伤害");
    ScaleEffect(55095, 1.4f, C, "冰霜疫病伤害");

    // 食尸 —— 自我治疗
    ScaleEffect(48979, 2.0f, C, "食尸治疗量");

    // 反魔法护罩 —— 保命
    ScaleCooldown(48707, 0.6f, C, "反魔法护罩冷却");
}

// ============================================================================
//  萨满 —— 让图腾从"放着不管"变成有用
// ============================================================================
void CustomSpellTweakMgr::ApplyShaman()
{
    char const* C = "萨满";

    // 治疗链 —— 跳跃 +3
    AddChainTargets(55459, 3, C, "治疗链跳跃");
    ScaleEffect(55459, 1.3f, C, "治疗链治疗量");

    // 闪电链 —— 跳跃 +2
    AddChainTargets(49271, 2, C, "闪电链跳跃");

    // 火焰新星图腾 —— AOE
    ScaleEffect(61657, 1.5f, C, "火焰新星伤害");

    // 大地之盾 —— 保命
    ScaleEffect(49284, 1.4f, C, "大地之盾治疗");

    // 先祖之魂 —— 团队增益
    ScaleCooldown(16190, 0.7f, C, "法力之潮图腾冷却");

    // 嗜血/英勇 —— 团队爆发，CD 太长
    ScaleCooldown(2825,  0.7f, C, "嗜血冷却");
    ScaleCooldown(32182, 0.7f, C, "英勇冷却");
}

// ============================================================================
//  法师 —— 缺生存能力
// ============================================================================
void CustomSpellTweakMgr::ApplyMage()
{
    char const* C = "法师";

    // 法力护盾 —— 法师太脆
    ScaleEffect(43020, 2.0f, C, "法力护盾吸收量");

    // 寒冰护体 —— 减伤
    ScaleEffect(43038, 1.5f, C, "寒冰护体吸收量");

    // 暴风雪 —— AOE
    ScaleEffect(42940, 1.4f, C, "暴风雪伤害");

    // 炎爆术 —— 爆发
    ScaleEffect(42891, 1.3f, C, "炎爆术伤害");

    // 寒冰箭 —— 主力
    ScaleEffect(42842, 1.2f, C, "寒冰箭伤害");

    // 闪现术 —— 位移，CD 太长
    ScaleCooldown(1953, 0.5f, C, "闪现术冷却");

    // 寒冰屏障 —— 保命
    ScaleCooldown(45438, 0.7f, C, "寒冰屏障冷却");
}

// ============================================================================
//  术士 —— 强化"献祭换力量"主题
// ============================================================================
void CustomSpellTweakMgr::ApplyWarlock()
{
    char const* C = "术士";

    // 献祭 —— 主力
    ScaleEffect(47838, 1.3f, C, "献祭伤害");

    // 暗影箭 —— 主力
    ScaleEffect(47809, 1.3f, C, "暗影箭伤害");

    // 生命虹吸 —— 自我治疗
    ScaleEffect(47857, 1.8f, C, "生命虹吸");

    // 吸取生命 —— 自我治疗
    ScaleEffect(47857, 1.5f, C, "吸取生命");

    // 地狱火 —— AOE 召唤
    ScaleCooldown(1122, 0.7f, C, "召唤地狱火冷却");

    // 47883 是"使用灵魂石"没有冷却（实测），改用术士真正的保命技：
    ScaleCooldown(6229, 0.7f, C, "暗影魔法防护冷却");

    // 死亡缠绕（术士版）—— 控制
    ScaleCooldown(6789, 0.7f, C, "死亡缠绕冷却");
}

// ============================================================================
//  德鲁伊 —— 让"全能"真正全能
// ============================================================================
void CustomSpellTweakMgr::ApplyDruid()
{
    char const* C = "德鲁伊";

    // 愈合 —— HOT
    ScaleEffect(48441, 1.4f, C, "回春术治疗量");

    // 生命绽放 —— HOT
    ScaleEffect(48451, 1.4f, C, "生命绽放");

    // 野性成长 —— 群疗
    ScaleEffect(53251, 1.4f, C, "野性成长");
    // 野性成长不是链式法术（实测），目标数由 MaxAffectedTargets 控制
    SetMaxTargets(53251, 7, C, "野性成长目标数");

    // 星火术 —— 平衡主力
    ScaleEffect(48465, 1.3f, C, "星火术伤害");

    // 割裂 —— 野性主力
    ScaleEffect(48568, 1.3f, C, "割裂伤害");

    // 飓风 —— AOE
    ScaleEffect(48467, 1.4f, C, "飓风伤害");

    // 激励 —— 保命
    ScaleCooldown(22812, 0.6f, C, "树皮术冷却");

    // 复生 —— 战斗复活
    ScaleCooldown(20484, 0.6f, C, "复生冷却");
}
