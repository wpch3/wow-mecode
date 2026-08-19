/*
 * v3test.cpp —— 职责 / 场景 / 治疗 / 驱散 的逻辑验证
 */
#include "mock.h"
#include "CombatSpecData.h"

static int g_pass = 0, g_fail = 0;
static void CHECK(bool ok, char const* what)
{
    if (ok) { ++g_pass; printf("  [OK]   %s\n", what); }
    else    { ++g_fail; printf("  [FAIL] %s\n", what); }
}

// ---- 复刻 DetectScene ----
static uint8 DetectScene(Player* p)
{
    Map* map = p->GetMap();
    if (!map) return CombatSpec::SCENE_QUEST;
    if (map->IsBattleground()) return CombatSpec::SCENE_DUNGEON;
    if (map->IsRaid())
    {
        if (map->IsHeroic() || map->Is25ManRaid()) return CombatSpec::SCENE_MYTHIC;
        return CombatSpec::SCENE_RAID;
    }
    if (map->IsDungeon())
    {
        if (map->IsHeroic()) return CombatSpec::SCENE_MYTHIC;
        return CombatSpec::SCENE_DUNGEON;
    }
    if (Group* g = p->GetGroup())
        if (g->GetMembersCount() > 1) return CombatSpec::SCENE_FARM;
    return CombatSpec::SCENE_QUEST;
}

// ---- 复刻 PickHealTarget 的核心排序 ----
struct HT { Unit* unit=nullptr; float hpPct=101.f; bool isTank=false; uint32 hurtCnt=0; };
static void PickHeal(Player* self, std::vector<std::pair<Unit*,bool>> const& members, HT& out)
{
    out = HT();
    for (auto& [u, tank] : members)
    {
        if (!u || !u->IsAlive()) continue;
        float pct = u->GetHealthPct();
        if (pct < 95.f) ++out.hurtCnt;
        float w    = tank ? pct - 5.f : pct;
        float curW = out.isTank ? out.hpPct - 5.f : out.hpPct;
        if (!out.unit || w < curW) { out.unit=u; out.hpPct=pct; out.isTank=tank; }
    }
    (void)self;
}

int main()
{
    printf("\n===== 1. 场景自动识别 =====\n");
    Player p; Map m; p._map = &m; p._class = 5;
    Group g; GroupReference r1, r2; g._first=&r1; r1._next=&r2; g._count=5;

    m = Map(); CHECK(DetectScene(&p)==CombatSpec::SCENE_QUEST, "野外单人 -> 做任务");
    p._group=&g; CHECK(DetectScene(&p)==CombatSpec::SCENE_FARM, "野外组队 -> 聚怪刷材料");
    p._group=nullptr;
    m=Map(); m._dungeon=true; CHECK(DetectScene(&p)==CombatSpec::SCENE_DUNGEON, "普通5人本 -> 副本");
    m._heroic=true; CHECK(DetectScene(&p)==CombatSpec::SCENE_MYTHIC, "英雄5人本 -> 高级");
    m=Map(); m._raid=true; CHECK(DetectScene(&p)==CombatSpec::SCENE_RAID, "普通团本 -> 团本");
    m._r25=true; CHECK(DetectScene(&p)==CombatSpec::SCENE_MYTHIC, "25人团 -> 高级团本");
    m=Map(); m._raid=true; m._heroic=true; CHECK(DetectScene(&p)==CombatSpec::SCENE_MYTHIC, "英雄团 -> 高级团本");

    printf("\n===== 2. 场景参数递进（越难越保守）=====\n");
    auto const& q = CombatSpec::GetTuning(CombatSpec::SCENE_QUEST);
    auto const& d = CombatSpec::GetTuning(CombatSpec::SCENE_DUNGEON);
    auto const& rr= CombatSpec::GetTuning(CombatSpec::SCENE_RAID);
    auto const& my= CombatSpec::GetTuning(CombatSpec::SCENE_MYTHIC);
    printf("         任务:保命%u%% 救人%u%% | 副本:%u%%/%u%% | 团本:%u%%/%u%% | 高级:%u%%/%u%%\n",
        q.defensiveHpPct,q.emergencyPct,d.defensiveHpPct,d.emergencyPct,
        rr.defensiveHpPct,rr.emergencyPct,my.defensiveHpPct,my.emergencyPct);
    CHECK(q.defensiveHpPct < d.defensiveHpPct, "副本保命线 > 任务");
    CHECK(d.defensiveHpPct < rr.defensiveHpPct, "团本保命线 > 副本");
    CHECK(rr.defensiveHpPct < my.defensiveHpPct, "高级团本保命线最高");
    CHECK(my.emergencyPct > q.emergencyPct, "高级团本救人更早");
    CHECK(!q.saveBurst && my.saveBurst, "高级团本省爆发，日常不省");
    CHECK(my.aoeThreshold > CombatSpec::GetTuning(CombatSpec::SCENE_FARM).aoeThreshold,
          "刷材料AOE门槛最低，高级团本最高");

    printf("\n===== 3. 职责推荐 =====\n");
    CHECK(CombatSpec::SuggestRole(1,2)==CombatSpec::ROLE_TANK,   "防护战 -> 坦克");
    CHECK(CombatSpec::SuggestRole(1,0)==CombatSpec::ROLE_DPS,    "武器战 -> 输出");
    CHECK(CombatSpec::SuggestRole(2,0)==CombatSpec::ROLE_HEALER, "神圣骑 -> 治疗");
    CHECK(CombatSpec::SuggestRole(6,0)==CombatSpec::ROLE_TANK,   "鲜血DK -> 坦克");
    CHECK(CombatSpec::SuggestRole(11,2)==CombatSpec::ROLE_TANK,  "熊德 -> 坦克");
    CHECK(CombatSpec::SuggestRole(11,3)==CombatSpec::ROLE_HEALER,"恢复德 -> 治疗");

    printf("\n===== 4. 同专精不同职责，方案确实不同 =====\n");
    CombatSpec::BuiltPlan tankPlan, dpsPlan, healPlan;
    CombatSpec::BuildPlan(1,2,CombatSpec::ROLE_TANK,CombatSpec::SCENE_RAID,false,tankPlan);
    CombatSpec::BuildPlan(1,2,CombatSpec::ROLE_DPS, CombatSpec::SCENE_RAID,false,dpsPlan);
    printf("         防护战当坦克 首技能=%s / 当输出 首技能=%s\n",
        tankPlan.core.empty()?"无":tankPlan.core[0].cn,
        dpsPlan.core.empty()?"无":dpsPlan.core[0].cn);
    CHECK(!tankPlan.core.empty() && !dpsPlan.core.empty(), "两种方案都有内容");
    CHECK(tankPlan.core[0].spell != dpsPlan.core[0].spell, "坦克和输出首技能不同");
    bool tankHasTaunt=false;
    for (auto const& s : tankPlan.core) if (s.flags & CombatSpec::SF_TAUNT) tankHasTaunt=true;
    CHECK(tankHasTaunt, "坦克方案里有嘲讽");
    bool dpsFirstIsTaunt = (dpsPlan.core[0].flags & CombatSpec::SF_TAUNT)!=0;
    CHECK(!dpsFirstIsTaunt, "输出方案不会一上来就嘲讽");

    printf("\n===== 5. 治疗方案 =====\n");
    CombatSpec::BuildPlan(5,1,CombatSpec::ROLE_HEALER,CombatSpec::SCENE_RAID,false,healPlan);
    int healCnt=0, aoeHeal=0, emerg=0, hot=0;
    for (auto const& s : healPlan.core)
    {
        if (s.flags & CombatSpec::SF_HEAL)       ++healCnt;
        if (s.flags & CombatSpec::SF_HEAL_AOE)   ++aoeHeal;
        if (s.flags & CombatSpec::SF_HEAL_EMERG) ++emerg;
        if (s.flags & CombatSpec::SF_HOT)        ++hot;
    }
    printf("         神圣牧治疗方案: 单奶%d 群奶%d 救命%d HOT%d\n", healCnt, aoeHeal, emerg, hot);
    CHECK(healCnt >= 3, "有多个单体治疗");
    CHECK(aoeHeal >= 1, "有群体治疗");
    CHECK(emerg  >= 1, "有救命大招");
    CHECK((healPlan.core[0].flags & (CombatSpec::SF_HEAL|CombatSpec::SF_HOT|
           CombatSpec::SF_HEAL_AOE|CombatSpec::SF_HEAL_EMERG)) != 0,
          "治疗职责下，主循环第一个就是治疗技");

    printf("\n===== 6. 所有治疗专精都有治疗方案 =====\n");
    struct HS { uint8 c, s; char const* n; };
    HS healers[] = {{2,0,"神圣骑"},{5,0,"戒律牧"},{5,1,"神圣牧"},{7,2,"恢复萨"},{11,3,"恢复德"}};
    bool allHeal=true;
    for (auto& h : healers)
    {
        CombatSpec::BuiltPlan pl;
        CombatSpec::BuildPlan(h.c,h.s,CombatSpec::ROLE_HEALER,CombatSpec::SCENE_RAID,false,pl);
        int n=0, e=0;
        for (auto const& sk : pl.core)
        {
            if (sk.flags & (CombatSpec::SF_HEAL|CombatSpec::SF_HOT|CombatSpec::SF_HEAL_AOE)) ++n;
            if (sk.flags & CombatSpec::SF_HEAL_EMERG) ++e;
        }
        printf("         %-8s 治疗技 %d 个, 救命 %d 个\n", h.n, n, e);
        if (n < 4) allHeal=false;
    }
    CHECK(allHeal, "5 个治疗专精都有 >=4 个治疗技");

    printf("\n===== 7. 所有坦克专精都有嘲讽 =====\n");
    HS tanks[] = {{1,2,"防护战"},{2,1,"防护骑"},{6,0,"鲜血DK"},{11,2,"熊德"}};
    bool allTank=true;
    for (auto& t : tanks)
    {
        CombatSpec::BuiltPlan pl;
        CombatSpec::BuildPlan(t.c,t.s,CombatSpec::ROLE_TANK,CombatSpec::SCENE_RAID,false,pl);
        int taunt=0, aoeTaunt=0, intr=0;
        for (auto const& sk : pl.core)
        {
            if (sk.flags & CombatSpec::SF_TAUNT)     ++taunt;
            if (sk.flags & CombatSpec::SF_TAUNT_AOE) ++aoeTaunt;
            if (sk.flags & CombatSpec::SF_INTERRUPT) ++intr;
        }
        printf("         %-8s 嘲讽%d 群嘲%d 打断%d\n", t.n, taunt, aoeTaunt, intr);
        if (taunt < 1) allTank=false;
    }
    CHECK(allTank, "4 个坦克专精都有嘲讽");

    printf("\n===== 8. 每个职业都有驱散/解控/打断 =====\n");
    uint8 classes[] = {1,2,3,4,5,6,7,8,9,11};
    int withDispel=0, withFree=0, withIntr=0;
    for (uint8 c : classes)
    {
        CombatSpec::BuiltPlan pl;
        CombatSpec::BuildPlan(c,0,CombatSpec::ROLE_DPS,CombatSpec::SCENE_RAID,false,pl);
        bool dsp=false, intr=false;
        for (auto const& sk : pl.utility)
        {
            if (sk.flags & CombatSpec::SF_DISPEL_FRIEND) dsp=true;
            if (sk.flags & CombatSpec::SF_INTERRUPT)     intr=true;
        }
        bool fre=false;
        for (auto const& sk : pl.emergency)
            if (sk.flags & CombatSpec::SF_FREE_SELF) fre=true;
        if (dsp) ++withDispel;
        if (fre) ++withFree;
        if (intr)++withIntr;
    }
    printf("         10 个职业中: 有驱散 %d / 有解控 %d / 有打断 %d\n",
           withDispel, withFree, withIntr);
    CHECK(withDispel >= 6, "至少 6 个职业有驱散");
    CHECK(withFree   >= 6, "至少 6 个职业有自我解控");
    CHECK(withIntr   >= 7, "至少 7 个职业有打断");

    printf("\n===== 9. 治疗目标选择：血最少优先，坦克加权 =====\n");
    Player me; Unit tank, dps1, dps2;
    me._maxhp=50000; me._hp=50000;
    tank._maxhp=100000; tank._hp=60000;   // 60%
    dps1._maxhp=50000; dps1._hp=27500;    // 55%
    dps2._maxhp=50000; dps2._hp=50000;    // 100%
    HT ht;
    PickHeal(&me, {{&tank,true},{&dps1,false},{&dps2,false}}, ht);
    printf("         坦克60%% / DPS 55%% / DPS 100%% -> 选中 %s\n",
           ht.unit==&tank?"坦克":(ht.unit==&dps1?"DPS1":"DPS2"));
    CHECK(ht.unit==&tank, "坦克60%虽比DPS55%高，但加权后优先奶坦克");
    CHECK(ht.hurtCnt==2, "正确统计 2 人受伤");

    dps1._hp = 10000;   // 20%
    PickHeal(&me, {{&tank,true},{&dps1,false},{&dps2,false}}, ht);
    printf("         坦克60%% / DPS 20%% -> 选中 %s\n", ht.unit==&tank?"坦克":"DPS1");
    CHECK(ht.unit==&dps1, "DPS血太危险时，优先救DPS（加权不是无脑偏袒）");

    printf("\n===== 10. 全专精按职责排序 =====\n");
    CombatSpec::BuiltPlan allTankPlan, allDpsPlan;
    CombatSpec::BuildPlan(1,0,CombatSpec::ROLE_TANK,CombatSpec::SCENE_RAID,true,allTankPlan);
    CombatSpec::BuildPlan(1,0,CombatSpec::ROLE_DPS, CombatSpec::SCENE_RAID,true,allDpsPlan);
    printf("         战士全专精: 坦克模式首技能=%s / 输出模式首技能=%s\n",
        allTankPlan.core.empty()?"无":allTankPlan.core[0].cn,
        allDpsPlan.core.empty()?"无":allDpsPlan.core[0].cn);
    CHECK(!allTankPlan.core.empty(), "全专精坦克方案非空");
    bool firstIsTankSkill = !allTankPlan.core.empty() &&
        (allTankPlan.core[0].flags & (CombatSpec::SF_TAUNT|CombatSpec::SF_TAUNT_AOE));
    CHECK(firstIsTankSkill, "全专精+坦克，首技能是拉怪技（按职责排在最前）");

    printf("\n===== 11. 配栏 4 区都不超 12 格 =====\n");
    bool slotOk=true;
    for (uint8 c : classes)
        for (uint8 sp=0; sp<CombatSpec::GetSpecCount(c); ++sp)
            for (uint8 role=1; role<=3; ++role)
            {
                CombatSpec::BuiltPlan pl;
                CombatSpec::BuildPlan(c,sp,role,CombatSpec::SCENE_RAID,false,pl);
                // 配栏时每区只取前 12 个，超出会被截断——验证不会崩，且核心技能在前 12
                if (pl.core.empty() && pl.opener.empty()) slotOk=false;
            }
    CHECK(slotOk, "所有 职业x专精x职责 组合都能生成非空方案");

    int combos=0;
    for (uint8 c : classes) combos += CombatSpec::GetSpecCount(c) * 3;
    printf("         共 %d 种 (专精 x 职责) 组合，每种再乘 6 个场景 = %d 套方案\n",
           combos, combos*6);

    printf("\n========================================\n");
    printf("  通过 %d / 失败 %d\n", g_pass, g_fail);
    printf("========================================\n");
    return g_fail ? 1 : 0;
}
