/*
 * mod_test.cpp —— cs_modify.cpp 的 stat 逻辑验证
 * 把 ModifyOneStat / reset 分支抽出来，用桩件跑通
 */
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
typedef uint8_t uint8; typedef int32_t int32; typedef uint32_t uint32;

enum Stats { STAT_STRENGTH=0, STAT_AGILITY, STAT_STAMINA, STAT_INTELLECT, STAT_SPIRIT, MAX_STATS };
enum UnitMods { UNIT_MOD_STAT_STRENGTH=0, UNIT_MOD_STAT_AGILITY, UNIT_MOD_STAT_STAMINA,
                UNIT_MOD_STAT_INTELLECT, UNIT_MOD_STAT_SPIRIT,
                UNIT_MOD_STAT_START=UNIT_MOD_STAT_STRENGTH };
enum UnitModifierFlatType { BASE_VALUE=0, TOTAL_VALUE=1 };
template<typename E> constexpr int AsUnderlyingType(E e){ return int(e); }

struct ObjectGuid { uint32 c=42; uint32 GetCounter() const { return c; }
                    static ObjectGuid Empty; };
ObjectGuid ObjectGuid::Empty;

struct Player {
    float flat[5] = {0,0,0,0,0};
    ObjectGuid guid;
    ObjectGuid GetGUID() const { return guid; }
    void SetStatFlatModifier(UnitMods m, UnitModifierFlatType, float v){ flat[int(m)]=v; }
    void UpdateStats(Stats){} 
    void UpdateAllStats(){}
    uint32 GetMaxHealth() const { return 1000000; }
};

class CustomStatPersistMgr {
public:
    enum StatType : uint8 { TYPE_UNITMOD=0, TYPE_RATING=1 };
    static CustomStatPersistMgr* instance(){ static CustomStatPersistMgr m; return &m; }
    bool Enabled() const { return _enabled; }
    void Record(uint32 g, StatType t, uint8 i, float a){ recs.push_back({g,t,i,a}); }
    struct R { uint32 g; StatType t; uint8 i; float a; };
    std::vector<R> recs; bool _enabled=true;
};
#define sCustomStatPersist CustomStatPersistMgr::instance()

struct ChatHandler {
    template<typename...A> void PSendSysMessage(char const*, A...){}
    void SendSysMessage(char const*){}
    void SetSentErrorMessage(bool){}
    std::string GetNameLink(Player*){ return "测试"; }
};

// ============ 被测代码（从 cs_modify_fixed.cpp 原样抄入）============
static bool ModifyOneStat(ChatHandler* handler, Player* target, Stats stat, int32 amount)
{
    (void)handler;
    UnitMods unitMod = UnitMods(UNIT_MOD_STAT_START + AsUnderlyingType(stat));
    target->SetStatFlatModifier(unitMod, TOTAL_VALUE, float(amount));
    if (sCustomStatPersist->Enabled())
        sCustomStatPersist->Record(target->GetGUID().GetCounter(),
            CustomStatPersistMgr::TYPE_UNITMOD, uint8(unitMod), float(amount));
    target->UpdateStats(stat);
    return true;
}

static void DoReset(ChatHandler* handler, Player* target)
{
    for (uint8 i = STAT_STRENGTH; i < MAX_STATS; ++i)
    {
        UnitMods um = UnitMods(UNIT_MOD_STAT_START + i);
        target->SetStatFlatModifier(um, TOTAL_VALUE, 0.0f);
        if (sCustomStatPersist->Enabled())
            sCustomStatPersist->Record(target->GetGUID().GetCounter(),
                CustomStatPersistMgr::TYPE_UNITMOD, uint8(um), 0.0f);
        target->UpdateStats(Stats(i));
    }
    target->UpdateAllStats();
    handler->PSendSysMessage("已重置");
}
// ============ 被测代码结束 ============

static int P=0,F=0;
static void CHECK(bool ok,char const*w){ if(ok){++P;printf("  [OK]   %s\n",w);} else {++F;printf("  [FAIL] %s\n",w);} }

int main(){
    ChatHandler h; Player p; auto* mgr = sCustomStatPersist;

    printf("\n===== 1. .modify stat sta 100000 =====\n");
    mgr->recs.clear();
    ModifyOneStat(&h,&p,STAT_STAMINA,100000);
    CHECK(p.flat[UNIT_MOD_STAT_STAMINA]==100000.f, "内存生效");
    CHECK(mgr->recs.size()==1, "写库 1 条");
    CHECK(mgr->recs[0].i==uint8(UNIT_MOD_STAT_STAMINA), "索引是耐力的 UnitMod");
    CHECK(mgr->recs[0].t==CustomStatPersistMgr::TYPE_UNITMOD, "类型是 TYPE_UNITMOD（不是 RATING）");
    CHECK(mgr->recs[0].a==100000.f, "数值正确");

    printf("\n===== 2. .modify stat all =====\n");
    mgr->recs.clear();
    for(uint8 i=STAT_STRENGTH;i<MAX_STATS;++i) ModifyOneStat(&h,&p,Stats(i),5000);
    CHECK(mgr->recs.size()==5, "五维各写一条");
    bool allSet=true; for(int i=0;i<5;++i) if(p.flat[i]!=5000.f) allSet=false;
    CHECK(allSet, "五维内存全部生效");

    printf("\n===== 3. reset 必须清库（关键修复）=====\n");
    mgr->recs.clear();
    DoReset(&h,&p);
    bool memZero=true; for(int i=0;i<5;++i) if(p.flat[i]!=0.f) memZero=false;
    CHECK(memZero, "内存已清零");
    CHECK(mgr->recs.size()==5, "写了 5 条删除记录");
    bool allDel=true; for(auto&r:mgr->recs) if(r.a!=0.f) allDel=false;
    CHECK(allDel, "全部 amount=0（表示删除）-> 重登不会复活");

    printf("\n===== 4. Persist=0 时不写库 =====\n");
    mgr->_enabled=false; mgr->recs.clear();
    ModifyOneStat(&h,&p,STAT_STRENGTH,777);
    CHECK(p.flat[UNIT_MOD_STAT_STRENGTH]==777.f, "内存照样生效");
    CHECK(mgr->recs.empty(), "不写库（保持原版行为）");
    mgr->recs.clear(); DoReset(&h,&p);
    CHECK(mgr->recs.empty(), "reset 也不写库");
    mgr->_enabled=true;

    printf("\n===== 5. UnitMod 索引范围 =====\n");
    CHECK(UNIT_MOD_STAT_START+MAX_STATS-1 < 256, "五维 UnitMod 索引 <256，uint8 安全");
    CHECK(int(CustomStatPersistMgr::TYPE_UNITMOD)==0, "TYPE_UNITMOD=0");

    printf("\n========================================\n  通过 %d / 失败 %d\n========================================\n",P,F);
    return F?1:0;
}
