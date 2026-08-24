/*
 * ============================================================================
 *  打木桩 DPS 测试 —— cs_dummy.cpp   (step23)
 * ============================================================================
 *
 *   .dummy                    召唤木桩并开始统计
 *   .dummy stop               结束统计并出报告
 *   .dummy clear              清掉木桩（不出报告）
 *   .dummy armor <值>         设置木桩护甲，模拟不同目标
 *   .dummy check              只看属性溢出诊断，不用打
 *
 * ----------------------------------------------------------------------------
 *  为什么这个指令对本服【是刚需】而不是玩具
 *
 *  普通服打木桩就是看个 DPS 数字。但本服把 stat_value 扩到了 21 亿，
 *  会撞上一批【静默溢出】—— 不报错、不崩服、不写日志，
 *  只是数字悄悄变负或归零，你会以为是"平衡没调好"。
 *
 *  用户实测已确认的两条（源码已核实）：
 *    1. 耐力堆到 2.147 亿 -> 血量超 21.47亿 客户端归零
 *       （4.29 亿是服务端墙，客户端 21.47亿 就先炸了）
 *       StatSystem.cpp GetHealthBonusFromStamina(): moreStam * 10.0f
 *       -> 耐力 x10 进血量，SetMaxHealth((uint32)value) 上限 42.9 亿
 *       -> 耐力比别的属性【早 10 倍】撞墙
 *
 *    2. 力量堆到 10.737 亿 -> 攻强变 -2147483648
 *       StatSystem.cpp: SetAttackPower(int32(base_attPower))
 *       -> x86-64 的 cvttss2si 在 float 超 int32 时返回 INT_MIN
 *       -> 不是钳成 INT_MAX，是直接翻成巨大负数
 *       -> Unit.cpp:9754 if (ap < 0) return 0.0f  => 伤害掉回白板
 *       => 面板显示 -21 亿 + 实际伤害归零，两件事同时发生
 *
 *  五维加成全表（用户 2026-07-31 完整实测，19/19 对上源码公式）：
 *
 *    力量 5亿    -> 攻强 10亿   (lvl*3 + STR*2 - 20)
 *                -> 格挡 2.5亿  (STR*0.5 - 10, Player.cpp:5225)
 *    敏捷 4.4亿  -> 护甲 8.8亿  (AGI*2)
 *                -> 暴击 8400万%
 *    耐力 2亿    -> 生命 20亿   ((STAM-20)*10)
 *    智力 1.2亿  -> 法力 18亿   ((INT-20)*15)  <- 是 x15 不是 x10
 *                -> 法暴 72万%
 *    精神 122    -> 法力回复 22444/5s
 *                   = sqrt(智力) x 精神 x DBC系数，【和智力相乘】
 *                   精神涨到1万，回复就到184万 -> 用户刻意压到122
 *
 *  各属性客户端墙（= INT32_MAX / 倍率）：
 *    智力 1.43亿(x15) < 耐力 2.147亿(x10) < 力量/敏捷 10.7亿(x2)
 *    倍率越大越早撞墙。
 *
 *  用户原话（本文件的设计准则）：
 *    「最重要的不是服务端给的数据更改，而是客户端里的实际情况」
 *
 *  所以本指令【不做理论推算】，而是把 UNIT_FIELD_* 的实际存储值
 *  读回来对比 —— 测的是"客户端会看到什么"，不是"我以为会是什么"。
 *
 * ----------------------------------------------------------------------------
 *  已核实 API（全 public，行号对应本仓库源码）
 *
 *   PassiveAI.h:53      class TC_GAME_API NullCreatureAI : public CreatureAI
 *   UnitAI.h:225        virtual void DamageTaken(Unit*, uint32&, DamageEffectType, SpellInfo const*)
 *   ScriptMgr.h:1155    #define RegisterCreatureAI(ai_name) new GenericCreatureScript<ai_name>(#ai_name)
 *   npcs_special.cpp:1379  官方 npc_training_dummy 参考实现
 *
 *   Unit.cpp:730        victimAI->DamageTaken(attacker, damage, ...);   <- 在这抹零
 *   Unit.cpp:756        sScriptMgr->OnDamage(attacker, victim, damage); <- 已经是 0 了
 *                       -> 所以统计【必须】放 DamageTaken 里，不能用 OnDamage
 *
 *   Object.h:114-117    GetInt32Value / GetUInt32Value / GetFloatValue  (public)
 *   Object.h:88/90      GetGUID() / GetEntry()
 *   Unit.h:904          float GetStat(Stats) const
 *   Unit.h:906          uint32 GetArmor() const
 *   Unit.h:907          void SetArmor(int32)
 *   Unit.h:915          uint32 GetMaxHealth() const
 *   Unit.h:926-928      SetHealth / SetMaxHealth / SetFullHealth
 *   Unit.h:1136-1143    SetImmuneToAll / SetImmuneToPC / SetImmuneToNPC
 *   Creature.h:134      SetReactState(ReactStates)
 *   UnitDefines.h:408   REACT_PASSIVE = 0
 *   WorldObject.h:476   SummonCreature(entry,x,y,z,o,TempSummonType,Milliseconds,ObjectGuid)
 *   GameTime.h:35       uint32 GetGameTimeMS()
 *   SpellInfo.h:377     std::array<char const*, 16> SpellName
 *
 *   UpdateFields.h:104  UNIT_FIELD_MAXHEALTH      Size:1 INT  PUBLIC
 *   UpdateFields.h:141  UNIT_FIELD_STAT0          Size:1 INT  PRIVATE|OWNER
 *   UpdateFields.h:156  UNIT_FIELD_RESISTANCES    Size:7 INT
 *   UpdateFields.h:162  UNIT_FIELD_ATTACK_POWER   Size:1 INT  PRIVATE|OWNER
 *
 * ----------------------------------------------------------------------------
 *  两个必须注意的实现细节（都踩过）
 *
 *   0. 统计放在 AI 的 DamageTaken 里，【不能】用 UnitScript::OnDamage
 *      因为官方木桩那套会在 DamageTaken(Unit.cpp:730) 把 damage 抹成 0，
 *      而 OnDamage(Unit.cpp:756) 在其之后，读到的永远是 0。
 *
 *   1. 累计伤害【必须 uint64】
 *      DealDamage 单次是 uint32（上限 42.9 亿），但用户每击约 1.88 亿，
 *      打几下总量就超 42.9 亿。用 uint32 累计会回绕成小数字。
 *
 *   2. 判断"是否超 int32 上限"【必须用 double 比较】
 *      float(2147483647) 会被舍入成 2147483648.0f，
 *      用 float 比会漏判。测试里踩过这个。
 * ============================================================================
 */

#include "ScriptMgr.h"
#include "Chat.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "PassiveAI.h"
#include "GameTime.h"
// ObjectAccessor::GetCreature（ObjectAccessor.h:68）—— 拿木桩指针要用
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "RBAC.h"
// Unit.h:83 里 Spell 只是前向声明（class Spell;），
// 而我们要调 cur->GetSpellInfo()（Spell.h:456）需要【完整类型】，
// 所以必须显式 include Spell.h，否则报 C2027 使用了未定义类型"Spell"。
#include "Spell.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "TemporarySummon.h"
#include "Unit.h"
#include "UpdateFields.h"
#include "World.h"
#include "WorldSession.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace
{

// ============================================================================
//  配置
// ============================================================================

// 木桩 entry，跟服务 NPC（960001-960007）同段，避免和别的自定义冲突
constexpr uint32 DUMMY_ENTRY       = 960010;
// 木桩存活时长（毫秒）。20 分钟够跑很长的循环了
constexpr uint32 DUMMY_LIFETIME_MS = 20 * 60 * 1000;
// 报告里最多列几个技能
constexpr size_t TOP_SPELL_COUNT   = 12;

// ============================================================================
//  统计数据
// ============================================================================

struct SpellStat
{
    uint64 damage = 0;      // 必须 uint64，见文件头说明
    uint32 hits   = 0;
};

struct Session
{
    bool     active     = false;
    ObjectGuid dummyGuid;                 // 只统计打这个木桩的伤害
    uint32   startMs    = 0;
    uint32   endMs      = 0;
    uint64   total      = 0;              // uint64
    uint32   hits       = 0;
    uint32   overkill   = 0;              // 被木桩血量上限吃掉的部分（仅记录）
    std::map<uint32, SpellStat> bySpell;  // spellId(0=普攻) -> 统计

    void Reset()
    {
        active   = false;
        dummyGuid = ObjectGuid::Empty;
        startMs  = 0;
        endMs    = 0;
        total    = 0;
        hits     = 0;
        overkill = 0;
        bySpell.clear();
    }

    uint32 ElapsedMs() const
    {
        uint32 now = endMs ? endMs : GameTime::GetGameTimeMS();
        return (now > startMs) ? (now - startMs) : 0;
    }

    double Seconds() const { return ElapsedMs() / 1000.0; }

    double Dps() const
    {
        double s = Seconds();
        return (s > 0.0) ? (double(total) / s) : 0.0;
    }
};

// 按空格切分参数，写法与 cs_worldtools.cpp:265 的 WorldTools::Tok 保持一致
inline std::vector<std::string> Tok(char const* args)
{
    std::vector<std::string> t;
    if (!args)
        return t;
    std::string a = args;
    size_t pos = 0;
    while (pos < a.size())
    {
        size_t sp = a.find(' ', pos);
        if (sp == std::string::npos)
            sp = a.size();
        if (sp > pos)
            t.push_back(a.substr(pos, sp - pos));
        pos = sp + 1;
    }
    return t;
}

// 每个玩家一份统计。玩家不多，用 map 足够
std::map<ObjectGuid, Session> g_sessions;

Session* FindSession(ObjectGuid playerGuid)
{
    auto itr = g_sessions.find(playerGuid);
    return (itr != g_sessions.end()) ? &itr->second : nullptr;
}

// ============================================================================
//  木桩 AI —— 借鉴官方 npc_training_dummy（npcs_special.cpp:1379）
// ============================================================================
/*
 * 用户实测：「可以攻击了但是会死，而且一下就死了」
 *           「可以借鉴一下主城里的木桩，因为不会死，而且数据也在」
 *
 * 官方做法（npcs_special.cpp:1379 npc_training_dummy : NullCreatureAI）：
 *
 *     void DamageTaken(Unit* attacker, uint32& damage, ...) override
 *     {
 *         damage = 0;                       <- 关键就这一行
 *         if (!attacker || damageType == DOT) return;
 *         _combatTimer[attacker->GetGUID()] = 5s;
 *     }
 *
 * 伤害在【扣血之前】就被抹成 0，所以永远不死 —— 不是靠血厚。
 *
 * ---------------------------------------------------------------------------
 * 但这带来一个致命问题：统计不能再用 UnitScript::OnDamage 钩子了
 *
 *   Unit.cpp:730   victimAI->DamageTaken(attacker, damage, ...);   <- 这里抹成 0
 *   Unit.cpp:756   sScriptMgr->OnDamage(attacker, victim, damage); <- 这里读到的已经是 0
 *
 * DamageTaken 在 OnDamage 【之前】执行。照抄官方就会导致 DPS 全是 0。
 *
 * 所以改成：在自己的 AI 里【先记账、再抹零】，彻底不用 OnDamage 钩子。
 * 这样既不会死，数据又是真实的 —— 正是用户说的「不会死，而且数据也在」。
 *
 * UnitAI.h:225  virtual void DamageTaken(Unit* attacker, uint32& damage,
 *                                        DamageEffectType damageType,
 *                                        SpellInfo const* spellInfo)
 * PassiveAI.h:53  class NullCreatureAI : public CreatureAI  （完全被动，不动不打）
 */
struct npc_bignum_dummy : public NullCreatureAI
{
    explicit npc_bignum_dummy(Creature* creature) : NullCreatureAI(creature) { }

    void DamageTaken(Unit* attacker, uint32& damage, DamageEffectType damageType,
                     SpellInfo const* spellInfo) override
    {
        // ---------- 先记账（此时 damage 还是真实值）----------
        if (attacker && damage)
        {
            if (Player* player = attacker->ToPlayer())
            {
                Session* s = FindSession(player->GetGUID());
                if (s && s->active && me->GetGUID() == s->dummyGuid)
                {
                    s->total += damage;        // uint64 累加，见文件头说明
                    ++s->hits;

                    // 法术ID：DamageTaken 直接给了 spellInfo，比 OnDamage 那边
                    // 靠 GetCurrentSpell 猜准得多 —— 顺手把精度问题也解决了
                    uint32 spellId = spellInfo ? spellInfo->Id : 0;
                    if (damageType == DOT && spellInfo)
                        spellId = spellInfo->Id;

                    SpellStat& st = s->bySpell[spellId];
                    st.damage += damage;
                    ++st.hits;
                }
            }
        }

        // ---------- 再抹零（官方 npc_training_dummy 的做法）----------
        damage = 0;

        if (!attacker || damageType == DOT)
            return;

        _combatTimer[attacker->GetGUID()] = 5s;
    }

    // 5 秒没挨打就脱战，抄官方 npcs_special.cpp:1398
    void UpdateAI(uint32 diff) override
    {
        for (auto itr = _combatTimer.begin(); itr != _combatTimer.end();)
        {
            itr->second -= Milliseconds(diff);
            if (itr->second <= 0s)
            {
                auto const& pveRefs = me->GetCombatManager().GetPvECombatRefs();
                auto it = pveRefs.find(itr->first);
                if (it != pveRefs.end())
                    it->second->EndCombat();

                itr = _combatTimer.erase(itr);
            }
            else
                ++itr;
        }
    }

    void JustEnteredCombat(Unit* who) override
    {
        if (who)
            _combatTimer[who->GetGUID()] = 5s;
    }

private:
    std::unordered_map<ObjectGuid, Milliseconds> _combatTimer;
};

// ============================================================================
//  属性溢出诊断 —— 读【实际存储值】，不做理论推算
// ============================================================================

enum HealthLevel { HL_OK, HL_WARN, HL_DANGER, HL_BROKEN };

struct FieldCheck
{
    char const* name;
    double      current;      // 当前实际值
    double      wall;         // 该字段的溢出墙
    bool        negative;     // 是否已经翻负
    HealthLevel level;
    char const* note;
};

HealthLevel Grade(double cur, double wall, bool negative)
{
    if (negative)          return HL_BROKEN;
    if (cur <= 0.0)        return HL_OK;       // 没堆就不评
    double pct = cur / wall;
    if (pct >= 1.0)        return HL_BROKEN;
    if (pct >= 0.85)       return HL_DANGER;
    if (pct >= 0.60)       return HL_WARN;
    return HL_OK;
}

char const* LevelColor(HealthLevel l)
{
    switch (l)
    {
        case HL_OK:     return "|cff00ff00";   // 绿
        case HL_WARN:   return "|cffffff00";   // 黄
        case HL_DANGER: return "|cffff8000";   // 橙
        default:        return "|cffff0000";   // 红
    }
}

char const* LevelText(HealthLevel l)
{
    switch (l)
    {
        case HL_OK:     return "安全";
        case HL_WARN:   return "注意";
        case HL_DANGER: return "危险";
        default:        return "已溢出";
    }
}

// 画一个 10 格进度条
std::string Bar(double pct)
{
    if (pct < 0.0) pct = 0.0;
    if (pct > 1.0) pct = 1.0;
    int filled = int(pct * 10.0 + 0.5);
    std::string s;
    for (int i = 0; i < 10; ++i)
        s += (i < filled) ? "=" : ".";
    return s;
}

/*
 * 各属性的墙 —— 源码实证 + 用户客户端实测，见 规划-数值天花板与客户端改造.md
 *
 * 【重要】耐力/智力有【两道墙】，必须按【客户端】那道来报警：
 *
 *   服务端墙  血量 42.9 亿 = UINT32_MAX  -> 耐力 4.29 亿（真正回绕）
 *   客户端墙  血量 21.47 亿 = INT32_MAX  -> 耐力 2.147 亿（客户端显示归零）
 *
 *   用户实测原话：
 *     「耐力不能超过两亿一千万，不然血量就会超过21亿归零」
 *     「耐力最多4亿就无法往上堆叠」
 *
 *   服务端 SetMaxHealth 是 uint32 装得下 42.9 亿，
 *   但客户端血量字段按 int32 解读，超 21.47 亿就归零 ——
 *   所以【以客户端那道为准】，2.147 亿就要开始警告。
 *
 * 还有第三道墙（属性本身）：
 *   StatSystem.cpp UpdateStats():111  SetStat(stat, int32(value));
 *   Object.cpp:728  SetStatInt32Value(){ if (value < 0) value = 0; }
 *   -> 任何属性超 INT32_MAX 先翻负、再被钳成 0
 */
constexpr double WALL_UINT32 = 4294967295.0;   // 服务端字段上限
constexpr double WALL_INT32  = 2147483647.0;   // 客户端可见上限（血量按这个报警）

void BuildDiagnostics(Player* player, std::vector<FieldCheck>& out)
{
    // ---- 1. 攻击强度（用户实测会翻负的那个）----
    {
        // Object.h:114  int32 GetInt32Value(uint16) const
        int32 ap = player->GetInt32Value(UNIT_FIELD_ATTACK_POWER);
        FieldCheck fc;
        fc.name     = "攻击强度";
        fc.current  = double(ap);
        fc.wall     = WALL_INT32;
        fc.negative = (ap < 0);
        fc.level    = Grade(double(ap), WALL_INT32, fc.negative);
        fc.note     = fc.negative
                    ? "已翻 INT_MIN，服务端按 0 算 -> 伤害掉回白板"
                    : "力量 x2 进这里";
        out.push_back(fc);
    }

    // ---- 2. 远程攻击强度 ----
    {
        int32 rap = player->GetInt32Value(UNIT_FIELD_RANGED_ATTACK_POWER);
        FieldCheck fc;
        fc.name     = "远程攻强";
        fc.current  = double(rap);
        fc.wall     = WALL_INT32;
        fc.negative = (rap < 0);
        fc.level    = Grade(double(rap), WALL_INT32, fc.negative);
        fc.note     = fc.negative ? "已翻负" : "敏捷进这里";
        out.push_back(fc);
    }

    // ---- 3. 最大生命（耐力最先撞的墙）----
    {
        uint32 hp = player->GetMaxHealth();                 // Unit.h:915
        FieldCheck fc;
        fc.name     = "最大生命";
        fc.current  = double(hp);
        // step24 修复后墙抬到 UINT32_MAX（42.9亿）。
        // 修复前是 INT32_MAX：Unit::ModifyHealth 内部用 int32，
        // 血量超 21.47亿 时任何治疗/掉血都会 (int32) 强转成负数 -> 暴毙。
        // 现在内部已提升 int64，安全线 = UNIT_FIELD_HEALTH(uint32) 极限。
        fc.wall     = WALL_UINT32;
        fc.negative = false;                                // uint32 不会负，只会回绕
        fc.level    = Grade(double(hp), WALL_UINT32, false);
        fc.note     = "耐力 x10 进这里（step24后上限42.9亿）";
        out.push_back(fc);
    }

    // ---- 3.5 最大法力（智力 x15，比耐力更早撞墙）----
    {
        // Unit.h:937  uint32 GetMaxPower(Powers) const
        uint32 mp = player->GetMaxPower(POWER_MANA);
        if (mp > 0)                       // 非法力职业不报
        {
            FieldCheck fc;
            fc.name     = "最大法力";
            fc.current  = double(mp);
            fc.wall     = WALL_INT32;
            fc.negative = false;
            fc.level    = Grade(double(mp), WALL_INT32, false);
            fc.note     = "智力 x15 进这里，比耐力更早爆";
            out.push_back(fc);
        }
    }

    // ---- 3.8 格挡值（用户实测发现的第三条力量路径）----
    {
        // Player.cpp:5225  value = max(0, (flatMod + STR*0.5f - 10) * pctMod)
        // 用户实测：力量5亿 -> 格挡值2.5亿，确认系数 x0.5
        uint32 blk = player->GetUInt32Value(PLAYER_SHIELD_BLOCK);
        if (blk > 0)
        {
            FieldCheck fc;
            fc.name     = "格挡值";
            fc.current  = double(blk);
            fc.wall     = WALL_INT32;
            fc.negative = false;
            fc.level    = Grade(double(blk), WALL_INT32, false);
            fc.note     = "力量 x0.5 进这里";
            out.push_back(fc);
        }
    }

    // ---- 4. 护甲 ----
    {
        // Unit.h:906 GetArmor() 返回 uint32，但底层是 SetStatInt32Value
        int32 armor = player->GetInt32Value(UNIT_FIELD_RESISTANCES + SPELL_SCHOOL_NORMAL);
        FieldCheck fc;
        fc.name     = "护甲";
        fc.current  = double(armor);
        fc.wall     = WALL_INT32;
        fc.negative = (armor < 0);
        fc.level    = Grade(double(armor), WALL_INT32, fc.negative);
        fc.note     = fc.negative ? "已翻负" : "敏捷 x2 进这里";
        out.push_back(fc);
    }

    // ---- 5. 四围属性本身 ----
    struct StatDef { Stats id; char const* cn; double wall; char const* note; };
    static StatDef const stats[] =
    {
        { STAT_STRENGTH,  "力量", 1073741823.0, "->攻强(x2)"   },
        { STAT_AGILITY,   "敏捷", 1073741823.0, "->护甲(x2)"   },
        // 用【客户端墙】= INT32_MAX / 倍率，不是服务端的 UINT32_MAX。
        // 注意智力和耐力【倍率不同】，用户实测确认：
        //   StatSystem.cpp GetManaBonusFromIntellect(): moreInt * 15.0f   <- x15
        //   StatSystem.cpp GetHealthBonusFromStamina(): moreStam * 10.0f  <- x10
        // 用户实测「智力1.2亿 -> 法力18亿」正好 x15，吻合。
        { STAT_STAMINA,   "耐力",  429496729.0, "->生命(x10) step24后4.29亿封顶" },
        { STAT_INTELLECT, "智力",  143165576.0, "->法力(x15) 客户端1.43亿封顶" },
        // 精神不像其他四维有固定倍率 —— 它和【智力相乘】：
        //   StatSystem.cpp:960  power_regen = sqrt(Intellect) * OCTRegenMPPerSpirit()
        //   Player.cpp          OCTRegenMPPerSpirit(){ return spirit * moreRatio->Data; }
        // 用户实测：智力1.2亿 + 精神122 -> 法力回复 22444/5s
        //           精神只要涨到1万，回复就飙到 184万 —— 所以墙给得很保守
        { STAT_SPIRIT,    "精神",   10000000.0, "x sqrt(智力) 乘算，涨得极快" },
    };

    for (StatDef const& sd : stats)
    {
        float v = player->GetStat(sd.id);                  // Unit.h:904 返回 float
        FieldCheck fc;
        fc.name     = sd.cn;
        fc.current  = double(v);
        fc.wall     = sd.wall;
        fc.negative = (v < 0.0f);
        fc.level    = Grade(double(v), sd.wall, fc.negative);
        fc.note     = sd.note;
        out.push_back(fc);
    }
}

void PrintDiagnostics(ChatHandler* handler, Player* player)
{
    std::vector<FieldCheck> checks;
    BuildDiagnostics(player, checks);

    handler->PSendSysMessage("|cff00ccff===== 面板实际值诊断 =====|r");
    handler->PSendSysMessage("|cff888888读的是 UNIT_FIELD_* 实际存储值，不是理论推算|r");

    bool anyProblem = false;

    for (FieldCheck const& fc : checks)
    {
        double pct = (fc.wall > 0.0) ? (fc.current / fc.wall) : 0.0;

        if (fc.negative || fc.level == HL_BROKEN)
        {
            anyProblem = true;
            handler->PSendSysMessage("%s%-10s %20.0f  [!] %s|r",
                LevelColor(fc.level), fc.name, fc.current, fc.note);
        }
        else
        {
            if (fc.level >= HL_WARN)
                anyProblem = true;

            handler->PSendSysMessage("%s%-10s %20.0f  %s %3.0f%% %s|r  |cff888888%s|r",
                LevelColor(fc.level), fc.name, fc.current,
                Bar(pct).c_str(), pct * 100.0, LevelText(fc.level), fc.note);
        }
    }

    if (!anyProblem)
        handler->PSendSysMessage("|cff00ff00全部字段健康，余量充足|r");
    else
        handler->PSendSysMessage("|cffffff00提示：|r达到 100%% 会静默溢出 —— 不报错、不崩服，"
                                 "数字直接变负或归零");
}

// ============================================================================
//  报告输出
// ============================================================================

std::string SpellNameOf(uint32 spellId)
{
    if (!spellId)
        return "普通攻击";

    if (SpellInfo const* si = sSpellMgr->GetSpellInfo(spellId))
        if (si->SpellName[0] && *si->SpellName[0])
            return si->SpellName[0];

    char buf[32];
    snprintf(buf, sizeof(buf), "法术 %u", spellId);
    return buf;
}

// 大数字加千位分隔，21亿这种数字不分隔根本读不出来
std::string Comma(uint64 v)
{
    char raw[32];
    snprintf(raw, sizeof(raw), "%llu", (unsigned long long)v);
    std::string s(raw), out;
    int cnt = 0;
    for (int i = int(s.size()) - 1; i >= 0; --i)
    {
        out.insert(out.begin(), s[i]);
        if (++cnt % 3 == 0 && i > 0)
            out.insert(out.begin(), ',');
    }
    return out;
}

void PrintReport(ChatHandler* handler, Player* player, Session& s)
{
    double secs = s.Seconds();

    handler->PSendSysMessage("|cff00ff00===== 木桩测试报告 =====|r");

    if (secs <= 0.0 || !s.hits)
    {
        handler->PSendSysMessage("|cffff0000没有记录到任何伤害|r");
        handler->PSendSysMessage("|cff888888检查：是不是没打木桩？或者打的是别的目标？|r");
        return;
    }

    handler->PSendSysMessage("持续时间  |cffffffff%.1f|r 秒", secs);
    handler->PSendSysMessage("总伤害    |cffffffff%s|r", Comma(s.total).c_str());
    handler->PSendSysMessage("|cffffff00DPS       %s|r", Comma(uint64(s.Dps())).c_str());
    handler->PSendSysMessage("命中次数  |cffffffff%u|r  平均每击 |cffffffff%s|r",
        s.hits, Comma(s.hits ? (s.total / s.hits) : 0).c_str());

    // ---- 分技能占比 ----
    if (!s.bySpell.empty())
    {
        std::vector<std::pair<uint32, SpellStat>> v(s.bySpell.begin(), s.bySpell.end());
        std::sort(v.begin(), v.end(),
            [](auto const& a, auto const& b) { return a.second.damage > b.second.damage; });

        handler->PSendSysMessage("|cff00ccff----- 伤害构成 -----|r");
        size_t shown = 0;
        for (auto const& kv : v)
        {
            if (shown++ >= TOP_SPELL_COUNT)
                break;

            double pct = s.total ? (100.0 * double(kv.second.damage) / double(s.total)) : 0.0;
            handler->PSendSysMessage("  %-18s %14s  %5.1f%%  (%u次)",
                SpellNameOf(kv.first).c_str(),
                Comma(kv.second.damage).c_str(),
                pct, kv.second.hits);
        }
        if (v.size() > TOP_SPELL_COUNT)
            handler->PSendSysMessage("  |cff888888... 另有 %zu 个技能未列出|r",
                v.size() - TOP_SPELL_COUNT);
    }

    // ---- 顺便把溢出诊断打一遍 ----
    PrintDiagnostics(handler, player);
}

} // namespace

// ============================================================================
//  指令实现
// ============================================================================

class dummy_commandscript : public CommandScript
{
public:
    dummy_commandscript() : CommandScript("dummy_commandscript") { }

    /*
     * 命令表格式对齐 cs_worldtools.cpp（用户实际在编译的那份）：
     *   { name, rbac, allowConsole, &handler, "" }
     * 子命令不用嵌套表，在 HandleDummy 里自己分发 —— 和 .inst / .gear 一个路子。
     */
    std::vector<ChatCommand> GetCommands() const override
    {
        static std::vector<ChatCommand> commandTable =
        {
            { "dummy", rbac::RBAC_PERM_COMMAND_WORLDTOOLS, false, &HandleDummy, "" },
        };
        return commandTable;
    }

    // ------------------------------------------------------------------
    //  .dummy 总入口 —— 自己分发子命令
    // ------------------------------------------------------------------
    static bool HandleDummy(ChatHandler* handler, char const* args)
    {
        std::vector<std::string> tok = Tok(args);

        if (tok.empty())
            return HandleStart(handler);

        std::string const& sub = tok[0];

        if (sub == "stop"  || sub == "结束") return HandleStop(handler);
        if (sub == "clear" || sub == "清除") return HandleClear(handler);
        if (sub == "check" || sub == "检查") return HandleCheck(handler);
        if (sub == "armor" || sub == "护甲")
            return HandleArmor(handler, tok.size() > 1 ? tok[1].c_str() : nullptr);

        handler->PSendSysMessage("|cff00ff00===== 打木桩 =====|r");
        handler->PSendSysMessage("|cffffff00.dummy|r            召唤木桩并开始统计");
        handler->PSendSysMessage("|cffffff00.dummy stop|r       结束并出报告");
        handler->PSendSysMessage("|cffffff00.dummy clear|r      清掉木桩（不出报告）");
        handler->PSendSysMessage("|cffffff00.dummy armor <值>|r 设木桩护甲，模拟不同目标");
        handler->PSendSysMessage("|cffffff00.dummy check|r      只看属性溢出诊断，不用打");
        return true;
    }

    // ------------------------------------------------------------------
    //  .dummy —— 召唤木桩并开始统计
    // ------------------------------------------------------------------
    static bool HandleStart(ChatHandler* handler)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        // 模板不存在直接说清楚，别让用户猜
        if (!sObjectMgr->GetCreatureTemplate(DUMMY_ENTRY))
        {
            handler->PSendSysMessage("|cffff0000缺少木桩模板 %u|r，请先执行 |cffffff00sql/40_dummy_npc.sql|r",
                DUMMY_ENTRY);
            return true;
        }

        Session& s = g_sessions[player->GetGUID()];

        // 已经在测了就先清掉旧的，避免叠加
        if (s.active)
        {
            handler->PSendSysMessage("|cffffff00上一次测试还在进行，已重置|r");
            DespawnDummy(player, s);
        }

        s.Reset();

        // 放在面前 5 码
        float x = player->GetPositionX() + 5.0f * std::cos(player->GetOrientation());
        float y = player->GetPositionY() + 5.0f * std::sin(player->GetOrientation());

        // WorldObject.h:476  SummonCreature(entry,x,y,z,o,TempSummonType,Milliseconds,ObjectGuid)
        TempSummon* dummy = player->SummonCreature(DUMMY_ENTRY, x, y,
            player->GetPositionZ(), player->GetOrientation() + float(M_PI),
            TEMPSUMMON_TIMED_DESPAWN, Milliseconds(DUMMY_LIFETIME_MS));

        if (!dummy)
        {
            handler->PSendSysMessage("|cffff0000木桩召唤失败|r");
            return true;
        }

        /*
         * 【双保险】运行时强制设 faction。
         *
         * 演进过程（三次实测）：
         *   35   FACTION_FRIENDLY        -> 绿名，打不了
         *   1868 FACTION_MONSTER_SPAR_BUDDY -> 用户实测仍绿名
         *        （客户端 FactionTemplate.dbc 里它对玩家仍友好，
         *          只用于 BT 阿卡玛的特定脚本，不通用）
         *   14   FACTION_MONSTER         -> 能打了，但是【红名敌对】
         *   31   FACTION_PREY            -> 中立黄名，可打不主动 <- 当前
         *
         * 用户要的是「中立」不是「敌对」：
         *   SharedDefines.h:245  FACTION_PREY = 31
         *   用例 npcs_special.cpp:231  me->SetFaction(FACTION_PREY);
         *
         * Unit.h:975  void SetFaction(uint32 faction)
         * 即使 SQL 没跑 / 模板缓存没刷新，这里也能兜住。
         */
        dummy->SetFaction(FACTION_PREY);

        /*
         * 模型：官方训练假人的 displayid 直接写死 + 库内探测兜底。
         *
         * 上一版只探 creature_template 的官方假人 entry，但你库里
         * 【很可能根本没有这些 entry】（TDB 数据包不全 / NPCBot 精简库），
         * 探不到就回退成矮人 3053 —— 这就是你连着三次看到矮人的原因。
         *
         * 3.3.5 官方训练假人的 displayid（来自 CreatureDisplayInfo.dbc，
         * 这些是稻草人模型，客户端 MPQ 自带，不依赖你的 world 库）：
         *   25225 = 训练假人（北裂境/银色比武场那种稻草人）
         *   16925 = 战斗训练假人
         *   24792 = 训练假人（另一款）
         *
         * 策略：先用库里探到的官方假人模型（最准），
         *       探不到就用写死的 25225（客户端自带，不会隐形）。
         *
         * 注意 SetDisplayId 和 SetNativeDisplayId 【两个都要设】，
         * 否则某些刷新路径会把模型回退回模板值。
         *   Unit.h:1595  virtual void SetDisplayId(uint32 modelId);
         *   Unit.h:1598  void SetNativeDisplayId(uint32 displayId);
         */
        static uint32 s_dummyModel = 0xFFFFFFFF;
        if (s_dummyModel == 0xFFFFFFFF)
        {
            s_dummyModel = 0;
            static uint32 const officialDummies[] =
                { 32546, 32541, 31144, 32666, 2673, 30527, 31146 };

            for (uint32 e : officialDummies)
            {
                if (CreatureTemplate const* ct = sObjectMgr->GetCreatureTemplate(e))
                {
                    if (ct->Modelid1)
                    {
                        s_dummyModel = ct->Modelid1;
                        break;
                    }
                }
            }

            // 库里没有官方假人 -> 用客户端自带的稻草人 displayid
            if (!s_dummyModel)
                s_dummyModel = 25225;
        }

        if (s_dummyModel)
        {
            dummy->SetDisplayId(s_dummyModel);
            dummy->SetNativeDisplayId(s_dummyModel);   // 两个都设，防回退
        }

        // 不还手（Creature.h:134 / UnitDefines.h:408）
        dummy->SetReactState(REACT_PASSIVE);
        // 不打别人也不被别人打，只挨你打
        dummy->SetImmuneToNPC(true);

        /*
         * 【关键】运行时直接注入木桩 AI —— 不依赖 creature_template.ScriptName。
         *
         * 你实测「还是会死」，根因是 ScriptName 没绑上：
         *   ObjectMgr.cpp:9834  GetScriptId() 在 _scriptNamesStore 里二分查找，
         *                       查不到返回 0 = 无脚本
         *   ObjectMgr.cpp:9765  LoadScriptNames() 启动时一次性收集所有表的
         *                       ScriptName，【在 SQL 执行之后才重启】才会生效。
         * 只要顺序不对（先重启后跑SQL、或只 reload 没重启），木桩就是个普通怪，
         * 一下被打死。
         *
         * 这里直接用 AIM_Initialize 把 AI 实例塞进去，绕开整条 ScriptName 链路：
         *   Creature.h:166  bool AIM_Initialize(CreatureAI* ai = nullptr);
         *   实现里 AIM_Create(ai) 后调 AI()->InitializeAI()
         * 这样【SQL 跑没跑、ScriptName 有没有绑，都不影响】。
         */
        dummy->AIM_Initialize(new npc_bignum_dummy(dummy));

        s.active    = true;
        s.dummyGuid = dummy->GetGUID();
        s.startMs   = GameTime::GetGameTimeMS();       // GameTime.h:35

        handler->PSendSysMessage("|cff00ff00[木桩已就位]|r 开打吧");
        handler->PSendSysMessage("|cff888888护甲 %u  |  打完输入 |cffffff00.dummy stop|r|cff888888 出报告|r",
            dummy->GetArmor());
        handler->PSendSysMessage("|cff888888木桩 %u 分钟后自动消失|r", DUMMY_LIFETIME_MS / 60000);
        return true;
    }

    // ------------------------------------------------------------------
    //  .dummy stop —— 结束并出报告
    // ------------------------------------------------------------------
    static bool HandleStop(ChatHandler* handler)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        Session* s = FindSession(player->GetGUID());
        if (!s || !s->active)
        {
            handler->PSendSysMessage("|cffffff00当前没有进行中的测试|r，用 |cffffff00.dummy|r 开始");
            return true;
        }

        s->endMs  = GameTime::GetGameTimeMS();
        s->active = false;                 // 先停统计，再出报告

        PrintReport(handler, player, *s);
        DespawnDummy(player, *s);
        return true;
    }

    // ------------------------------------------------------------------
    //  .dummy clear —— 清掉木桩，不出报告
    // ------------------------------------------------------------------
    static bool HandleClear(ChatHandler* handler)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        Session* s = FindSession(player->GetGUID());
        if (!s)
        {
            handler->PSendSysMessage("|cffffff00没有木桩|r");
            return true;
        }

        DespawnDummy(player, *s);
        s->Reset();
        handler->PSendSysMessage("|cff00ff00已清除|r");
        return true;
    }

    // ------------------------------------------------------------------
    //  .dummy armor <值> —— 设置木桩护甲
    // ------------------------------------------------------------------
    static bool HandleArmor(ChatHandler* handler, char const* args)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        if (!args || !*args)
        {
            handler->PSendSysMessage("用法：|cffffff00.dummy armor <数值>|r");
            handler->PSendSysMessage("|cff888888参考：0=无甲  10643=80级BOSS典型  50000+=减伤封顶75%%|r");
            return true;
        }

        int64 v = atoll(args);
        if (v < 0)
            v = 0;
        // Unit.h:907 SetArmor(int32)，别自己造一个溢出出来
        if (v > 2147483647LL)
        {
            v = 2147483647LL;
            handler->PSendSysMessage("|cffffff00已钳到 int32 上限|r");
        }

        Session* s = FindSession(player->GetGUID());
        if (!s || !s->active || s->dummyGuid.IsEmpty())
        {
            handler->PSendSysMessage("|cffff0000没有木桩|r，先用 |cffffff00.dummy|r 召唤");
            return true;
        }

        Creature* dummy = ObjectAccessor::GetCreature(*player, s->dummyGuid);
        if (!dummy)
        {
            handler->PSendSysMessage("|cffff0000木桩已不在|r");
            return true;
        }

        dummy->SetArmor(int32(v));

        // 顺手算一下这个护甲对当前等级意味着多少减伤
        // 公式来自 Unit::CalcArmorReducedDamage 结尾段：
        //   levelModifier = attackerLevel; if (>59) lm = lm + 4.5*(lm-59)
        //   dr = 0.1*armor / (8.5*lm + 40);  dr /= (1+dr);  钳 [0, 0.75]
        double lm = double(player->GetLevel());
        if (lm > 59.0)
            lm = lm + 4.5 * (lm - 59.0);
        double dr = 0.1 * double(v) / (8.5 * lm + 40.0);
        dr /= (1.0 + dr);
        if (dr > 0.75) dr = 0.75;
        if (dr < 0.0)  dr = 0.0;

        handler->PSendSysMessage("|cff00ff00木桩护甲 -> %s|r", Comma(uint64(v)).c_str());
        handler->PSendSysMessage("|cff888888对 %u 级的你，物理减伤约 %.1f%%|r",
            player->GetLevel(), dr * 100.0);
        return true;
    }

    // ------------------------------------------------------------------
    //  .dummy check —— 只看诊断，不用打
    // ------------------------------------------------------------------
    static bool HandleCheck(ChatHandler* handler)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        PrintDiagnostics(handler, player);
        return true;
    }

private:
    static void DespawnDummy(Player* player, Session& s)
    {
        if (s.dummyGuid.IsEmpty())
            return;

        if (Creature* dummy = ObjectAccessor::GetCreature(*player, s.dummyGuid))
            dummy->DespawnOrUnsummon();     // Creature.h:268

        s.dummyGuid = ObjectGuid::Empty;
    }
};

void AddSC_dummy_commandscript()
{
    new dummy_commandscript();
    // ScriptMgr.h:1155  木桩 AI，需要 creature_template.ScriptName = 'npc_bignum_dummy'
    RegisterCreatureAI(npc_bignum_dummy);
}
