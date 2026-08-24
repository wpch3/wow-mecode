/*
 * logictest.cpp —— 运行时逻辑测试
 * 直接复刻 cs_combathelper.cpp 里 CheckSkill / ResolveRank 的判定逻辑，
 * 用可控的假数据验证每个分支。
 */
#include "mock.h"
#include "CombatSpecData.h"

static int g_pass = 0, g_fail = 0;
static void CHECK(bool ok, char const* what)
{
    if (ok) { ++g_pass; printf("  [OK]   %s\n", what); }
    else    { ++g_fail; printf("  [FAIL] %s\n", what); }
}

// ---- 复刻 cs_combathelper.cpp 的 CheckSkill（逐行对齐）----
static uint8 CheckSkill(Player* player, Unit* target,
                        CombatSpec::Skill const& sk, uint32 realSpell,
                        uint32 nearbyCount, SpellInfo const* si)
{
    if (!si) return 2;
    if (!player->GetSpellHistory()->IsReady(si)) return 2;
    if (player->GetSpellHistory()->HasGlobalCooldown(si)) return 2;

    if (player->isMoving())
    {
        bool hasCastTime = si->CalcCastTime() > 0;
        bool breakOnMove = (si->InterruptFlags & SPELL_INTERRUPT_FLAG_MOVEMENT) != 0;
        if ((sk.flags & CombatSpec::SF_NO_MOVE) || (hasCastTime && breakOnMove))
            return 1;
    }

    if (player->IsNonMeleeSpellCast(false)) return 2;

    if ((sk.flags & CombatSpec::SF_MELEE) && target)
        if (!player->IsWithinMeleeRange(target)) return 2;

    if ((sk.flags & CombatSpec::SF_EXECUTE) && target)
    {
        uint32 mx = target->GetMaxHealth();
        if (!mx || (uint64(target->GetHealth()) * 100 / mx) > 20) return 2;
    }

    if ((sk.flags & CombatSpec::SF_AOE) && nearbyCount < 2) return 2;

    if ((sk.flags & CombatSpec::SF_DEBUFF_KEEP) && target)
        if (target->HasAura(realSpell, player->GetGUID())) return 2;

    if (sk.flags & CombatSpec::SF_BUFF_KEEP)
        if (player->HasAura(realSpell)) return 2;

    if ((sk.flags & CombatSpec::SF_COMBO_FINISH) && target)
        if (player->GetComboPoints(target) < 1) return 2;

    return 0;
}

int main()
{
    Player p; Unit t;
    p._class = 8; p._maxhp = 100000; p._hp = 100000;
    t._maxhp = 100000; t._hp = 100000;

    SpellInfo instant;  instant.Id = 1; instant.CastTimeEntry = nullptr; instant.InterruptFlags = 0;
    SpellCastTimesEntry ct; ct.Base = 2500;
    SpellInfo channel;  channel.Id = 2; channel.CastTimeEntry = &ct; channel.InterruptFlags = SPELL_INTERRUPT_FLAG_MOVEMENT;

    CombatSpec::Skill skPlain  { 100, CombatSpec::SF_NONE,          "普通" };
    CombatSpec::Skill skNoMove { 101, CombatSpec::SF_NO_MOVE,       "读条" };
    CombatSpec::Skill skMelee  { 102, CombatSpec::SF_MELEE,         "近战" };
    CombatSpec::Skill skExec   { 103, CombatSpec::SF_EXECUTE,       "斩杀" };
    CombatSpec::Skill skAoe    { 104, CombatSpec::SF_AOE,           "群攻" };
    CombatSpec::Skill skDot    { 105, CombatSpec::SF_DEBUFF_KEEP,   "DOT"  };
    CombatSpec::Skill skBuff   { 106, CombatSpec::SF_BUFF_KEEP,     "BUFF" };
    CombatSpec::Skill skFin    { 107, CombatSpec::SF_COMBO_FINISH,  "终结" };

    printf("\n===== A. 移动判定（用户重点要求）=====\n");
    p._moving = false;
    CHECK(CheckSkill(&p,&t,skNoMove,101,1,&instant) == 0, "站定时 读条技 可放");
    p._moving = true;
    CHECK(CheckSkill(&p,&t,skNoMove,101,1,&instant) == 1, "移动时 读条技 返回1(移动挡住)");
    CHECK(CheckSkill(&p,&t,skPlain,100,1,&instant)  == 0, "移动时 瞬发技 照样可放");
    CHECK(CheckSkill(&p,&t,skPlain,100,1,&channel)  == 1, "移动时 有读条+可打断 也返回1");
    p._moving = false;
    CHECK(CheckSkill(&p,&t,skPlain,100,1,&channel)  == 0, "站定后 读条技恢复可放（不断连招）");

    printf("\n===== B. 不打断自己正在读的条 =====\n");
    p._casting = true;
    CHECK(CheckSkill(&p,&t,skPlain,100,1,&instant) == 2, "正在读条时 跳过（不打断）");
    p._casting = false;
    CHECK(CheckSkill(&p,&t,skPlain,100,1,&instant) == 0, "读条结束后 恢复");

    printf("\n===== C. 近战距离 =====\n");
    p._inMelee = true;
    CHECK(CheckSkill(&p,&t,skMelee,102,1,&instant) == 0, "贴身时 近战技可放");
    p._inMelee = false;
    CHECK(CheckSkill(&p,&t,skMelee,102,1,&instant) == 2, "距离远 近战技跳过");
    CHECK(CheckSkill(&p,&t,skPlain,100,1,&instant) == 0, "距离远 远程技照放");
    p._inMelee = true;

    printf("\n===== D. 斩杀血线 =====\n");
    t._hp = 100000;
    CHECK(CheckSkill(&p,&t,skExec,103,1,&instant) == 2, "满血 斩杀技跳过");
    t._hp = 15000;   // 15%
    CHECK(CheckSkill(&p,&t,skExec,103,1,&instant) == 0, "15%%血 斩杀技可放");
    t._hp = 25000;   // 25%
    CHECK(CheckSkill(&p,&t,skExec,103,1,&instant) == 2, "25%%血 斩杀技跳过（阈值20%%）");
    t._hp = 100000;

    printf("\n===== E. AOE 怪数 =====\n");
    CHECK(CheckSkill(&p,&t,skAoe,104,1,&instant) == 2, "只有1个怪 AOE跳过");
    CHECK(CheckSkill(&p,&t,skAoe,104,3,&instant) == 0, "3个怪 AOE可放");

    printf("\n===== F. DOT 不重复上 =====\n");
    t._auras.clear();
    CHECK(CheckSkill(&p,&t,skDot,105,1,&instant) == 0, "目标没有DOT 可以上");
    t._auras.insert(105);
    CHECK(CheckSkill(&p,&t,skDot,105,1,&instant) == 2, "目标已有DOT 跳过");
    t._auras.clear();

    printf("\n===== G. BUFF 不重复开 =====\n");
    p._auras.clear();
    CHECK(CheckSkill(&p,&t,skBuff,106,1,&instant) == 0, "自己没BUFF 可以开");
    p._auras.insert(106);
    CHECK(CheckSkill(&p,&t,skBuff,106,1,&instant) == 2, "自己已有BUFF 跳过");
    p._auras.clear();

    printf("\n===== H. 终结技连击点 =====\n");
    p._combo = 0;
    CHECK(CheckSkill(&p,&t,skFin,107,1,&instant) == 2, "0连击点 终结技跳过");
    p._combo = 5;
    CHECK(CheckSkill(&p,&t,skFin,107,1,&instant) == 0, "5连击点 终结技可放");

    printf("\n===== I. 优先级顺延（核心行为）=====\n");
    /* 模拟：法师移动中，主循环前几个都是读条技，
       应该顺延到瞬发技（火焰冲击），而不是卡住什么都不放 */
    p._moving = true; p._combo = 0;
    auto* mage = CombatSpec::GetSpec(8, 1);   // 火焰
    int firstCastable = -1;
    int blockedByMove = 0;
    for (size_t i = 0; i < mage->rotation.size(); ++i)
    {
        auto const& sk = mage->rotation[i];
        SpellInfo const* use = (sk.flags & CombatSpec::SF_NO_MOVE) ? &channel : &instant;
        uint8 r = CheckSkill(&p, &t, sk, sk.spell, 1, use);
        if (r == 1) ++blockedByMove;
        if (r == 0 && firstCastable < 0) firstCastable = int(i);
    }
    printf("         火焰法师移动中：%d 个读条技被挡，第 %d 个技能(%s)可放\n",
           blockedByMove, firstCastable,
           firstCastable >= 0 ? mage->rotation[firstCastable].cn : "无");
    CHECK(blockedByMove > 0,   "移动中确实有读条技被挡");
    CHECK(firstCastable >= 0,  "但仍能顺延找到可放的技能（连招不断）");

    printf("\n===== J. 站定后立刻恢复 =====\n");
    p._moving = false;
    int firstAfterStop = -1;
    for (size_t i = 0; i < mage->rotation.size(); ++i)
    {
        auto const& sk = mage->rotation[i];
        SpellInfo const* use = (sk.flags & CombatSpec::SF_NO_MOVE) ? &channel : &instant;
        if (CheckSkill(&p, &t, sk, sk.spell, 1, use) == 0) { firstAfterStop = int(i); break; }
    }
    printf("         站定后第 %d 个技能(%s)可放\n", firstAfterStop,
           firstAfterStop >= 0 ? mage->rotation[firstAfterStop].cn : "无");
    CHECK(firstAfterStop >= 0 && firstAfterStop <= firstCastable,
          "站定后能放更靠前（优先级更高）的技能");

    printf("\n===== K. 配栏槽位分区 =====\n");
    // BAR_ROTATION=0 BURST=12 DEFENSIVE=24 BUFF=36，每区12格
    std::map<uint8,uint32> buttons;
    auto* war = CombatSpec::GetSpec(1, 0);
    uint8 zones[4] = {0, 12, 24, 36};
    std::vector<CombatSpec::Skill> const* lists[4] =
        { &war->rotation, &war->burst, &war->defensive, &war->buffs };
    for (int z = 0; z < 4; ++z)
    {
        uint8 slot = zones[z];
        for (auto const& sk : *lists[z])
        {
            if (slot >= zones[z] + 12) break;
            buttons[slot++] = sk.spell;
        }
    }
    bool noOverlap = true;
    for (int z = 0; z < 3; ++z)
    {
        uint32 used = 0;
        for (auto const& kv : buttons)
            if (kv.first >= zones[z] && kv.first < zones[z]+12) ++used;
        if (used > 12) noOverlap = false;
    }
    CHECK(noOverlap, "四个分区互不越界");
    CHECK(buttons.size() > 0 && buttons.rbegin()->first < 48, "所有槽位 < 48");
    printf("         战士-武器 共占用 %zu 格，最大槽位 %u\n",
           buttons.size(), buttons.rbegin()->first);

    printf("\n========================================\n");
    printf("  通过 %d / 失败 %d\n", g_pass, g_fail);
    printf("========================================\n");
    return g_fail ? 1 : 0;
}
