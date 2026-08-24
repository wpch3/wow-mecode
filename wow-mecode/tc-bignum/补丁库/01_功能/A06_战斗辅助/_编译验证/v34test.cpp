/*
 * v34test.cpp —— v3.4：猎人只剩普通攻击、法师只剩火焰冲击
 *
 * 用户实测：
 *   「猎人发现只有普通攻击，奥术射击都没有」
 *   「法师也是除了火焰冲击没看到用其他的技能」
 *
 * 根因：v3.1 引入的退避表反噬自己 ——
 *   count 只增不减，技能因【临时原因】失败 3 次就被拉黑 60 秒，
 *   战斗越久拉黑越多，最后只剩不吃退避的技能。
 */
#include "mock.h"
#include "CombatSpecData.h"

namespace GameTime { void AdvanceMs(uint32 d); void SetMs(uint32 v); }

static int g_pass = 0, g_fail = 0;
static void CHECK(bool ok, char const* what)
{
    if (ok) { ++g_pass; printf("  [OK]   %s\n", what); }
    else    { ++g_fail; printf("  [FAIL] %s\n", what); }
}

// ---------- 旧版退避（复现 bug）----------
struct OldFI { uint32 untilMs = 0; uint32 count = 0; };
static std::unordered_map<uint32, OldFI> g_old;
static bool OldBackedOff(uint32 sp)
{ auto it=g_old.find(sp); return it!=g_old.end() && GameTime::GetGameTimeMS()<it->second.untilMs; }
static void OldMark(uint32 sp, SpellCastResult r)
{
    uint32 cd;
    switch (r) {
        case SPELL_FAILED_REAGENTS: case SPELL_FAILED_ITEM_NOT_FOUND:
        case SPELL_FAILED_BAD_TARGETS: cd=300000; break;
        case SPELL_FAILED_OUT_OF_RANGE: case SPELL_FAILED_LINE_OF_SIGHT:
        case SPELL_FAILED_MOVING: case SPELL_FAILED_NOT_READY: cd=2000; break;
        case SPELL_FAILED_NO_POWER: cd=5000; break;
        default: cd=10000; break;        // TOO_CLOSE 落这里！
    }
    OldFI& fi=g_old[sp];
    fi.untilMs=GameTime::GetGameTimeMS()+cd;
    ++fi.count;                           // 只增不减
    if (fi.count>=3 && cd<60000) fi.untilMs=GameTime::GetGameTimeMS()+60000;
}

// ---------- 新版退避 ----------
struct NewFI { uint32 untilMs=0; uint32 count=0; uint32 lastFailMs=0; };
static std::unordered_map<uint32, NewFI> g_new;
static bool NewBackedOff(uint32 sp)
{ auto it=g_new.find(sp); return it!=g_new.end() && GameTime::GetGameTimeMS()<it->second.untilMs; }
static void NewMark(uint32 sp, SpellCastResult r)
{
    uint32 cd; bool transient=false;
    switch (r) {
        case SPELL_FAILED_REAGENTS: case SPELL_FAILED_ITEM_NOT_FOUND:
        case SPELL_FAILED_NEED_AMMO: case SPELL_FAILED_NO_AMMO:
        case SPELL_FAILED_EQUIPPED_ITEM_CLASS: cd=300000; break;
        case SPELL_FAILED_NOT_SHAPESHIFT: cd=15000; break;
        case SPELL_FAILED_TOO_CLOSE: case SPELL_FAILED_OUT_OF_RANGE:
        case SPELL_FAILED_LINE_OF_SIGHT: case SPELL_FAILED_UNIT_NOT_INFRONT:
        case SPELL_FAILED_MOVING: cd=800; transient=true; break;
        case SPELL_FAILED_SPELL_IN_PROGRESS: case SPELL_FAILED_NOT_READY:
            cd=500; transient=true; break;
        case SPELL_FAILED_NO_POWER: cd=2000; transient=true; break;
        case SPELL_FAILED_BAD_TARGETS: case SPELL_FAILED_TARGET_AURASTATE:
        case SPELL_FAILED_CASTER_AURASTATE: cd=3000; transient=true; break;
        default: cd=5000; break;
    }
    uint32 now=GameTime::GetGameTimeMS();
    NewFI& fi=g_new[sp];
    if (fi.lastFailMs && now-fi.lastFailMs>30000) fi.count=0;   // 关键：会清零
    fi.lastFailMs=now; fi.untilMs=now+cd;
    if (!transient) { ++fi.count; if (fi.count>=5 && cd<60000) fi.untilMs=now+60000; }
}

int main()
{
    printf("\n===== 场景1：猎人怪贴脸（TOO_CLOSE）=====\n");
    printf("  射击技有最小射程，怪贴脸时全部返回 SPELL_FAILED_TOO_CLOSE\n\n");
    {
        // 8 个射击技，怪贴脸 -> 全部 TOO_CLOSE
        std::vector<uint32> shots = {53209,19434,53351,3044,1978,2643,1510,56641};

        // --- 旧版 ---
        // 真实情况：TOO_CLOSE 退避 10 秒，到期后再试还是失败，如此反复
        GameTime::SetMs(100000); g_old.clear();
        for (int round=0; round<3; ++round) {        // 三轮近身缠斗
            for (uint32 sp : shots) {
                if (OldBackedOff(sp)) continue;
                OldMark(sp, SPELL_FAILED_TOO_CLOSE); // 每轮失败一次 -> count 累加
            }
            GameTime::AdvanceMs(11000);              // 等 10 秒退避到期
        }
        // 怪被拉开了，现在能放了吗？
        GameTime::AdvanceMs(1000);
        int oldUsable=0;
        for (uint32 sp : shots) if (!OldBackedOff(sp)) ++oldUsable;
        printf("         旧版：近身10秒后拉开距离，可用射击技 %d/8\n", oldUsable);
        CHECK(oldUsable == 0, "旧版全被拉黑 -> 只剩普通攻击（复现用户的bug）");

        // --- 新版 ---
        GameTime::SetMs(100000); g_new.clear();
        for (int round=0; round<3; ++round) {
            for (uint32 sp : shots) {
                if (NewBackedOff(sp)) continue;
                NewMark(sp, SPELL_FAILED_TOO_CLOSE);
            }
            GameTime::AdvanceMs(11000);
        }
        GameTime::AdvanceMs(1000);
        int newUsable=0;
        for (uint32 sp : shots) if (!NewBackedOff(sp)) ++newUsable;
        printf("         新版：同样情况，可用射击技 %d/8\n", newUsable);
        CHECK(newUsable == 8, "新版全部恢复可用（瞬态失败不累计）");
    }

    printf("\n===== 场景2：法师移动中读条被判失败 =====\n");
    {
        std::vector<uint32> casts = {133,116,11366,2948,10};   // 火球/寒冰箭/炎爆/灼烧/暴风雪

        GameTime::SetMs(200000); g_old.clear();
        for (int tick=0; tick<20; ++tick) {
            for (uint32 sp : casts) {
                if (OldBackedOff(sp)) continue;
                OldMark(sp, SPELL_FAILED_MOVING);
            }
            GameTime::AdvanceMs(500);
        }
        GameTime::AdvanceMs(2500);
        int oldOk=0; for (uint32 sp:casts) if(!OldBackedOff(sp)) ++oldOk;
        printf("         旧版：跑动10秒后站定，可用读条技 %d/5\n", oldOk);
        CHECK(oldOk == 0, "旧版全被拉黑 -> 只剩瞬发的火焰冲击（复现bug）");

        GameTime::SetMs(200000); g_new.clear();
        for (int tick=0; tick<20; ++tick) {
            for (uint32 sp : casts) {
                if (NewBackedOff(sp)) continue;
                NewMark(sp, SPELL_FAILED_MOVING);
            }
            GameTime::AdvanceMs(500);
        }
        GameTime::AdvanceMs(1000);
        int newOk=0; for (uint32 sp:casts) if(!NewBackedOff(sp)) ++newOk;
        printf("         新版：同样情况，可用读条技 %d/5\n", newOk);
        CHECK(newOk == 5, "新版站定后立刻全部可用");
    }

    printf("\n===== 场景3：计数超时清零（长时间战斗）=====\n");
    {
        GameTime::SetMs(300000); g_new.clear();
        // 非瞬态失败 4 次（还没到 5 次上限）
        for (int i=0;i<4;++i) { NewMark(500, SPELL_FAILED_INTERRUPTED); GameTime::AdvanceMs(6000); }
        printf("         连续失败 4 次，count=%u\n", g_new[500].count);
        CHECK(g_new[500].count == 4, "非瞬态失败会累计");

        // 隔 40 秒再失败一次 -> 应该清零重来
        GameTime::AdvanceMs(40000);
        NewMark(500, SPELL_FAILED_INTERRUPTED);
        printf("         隔 40 秒后再失败，count=%u\n", g_new[500].count);
        CHECK(g_new[500].count == 1, "超过30秒没失败 -> 计数清零（不会越积越死）");
    }

    printf("\n===== 场景4：真硬失败仍然长退避 =====\n");
    {
        GameTime::SetMs(400000); g_new.clear();
        NewMark(600, SPELL_FAILED_REAGENTS);       // 缺材料
        CHECK(NewBackedOff(600), "缺材料立刻退避");
        GameTime::AdvanceMs(120000);
        CHECK(NewBackedOff(600), "缺材料 2 分钟后仍退避（硬失败5分钟）");

        g_new.clear();
        NewMark(601, SPELL_FAILED_NO_AMMO);        // 猎人没箭
        GameTime::AdvanceMs(60000);
        CHECK(NewBackedOff(601), "没弹药 -> 长退避（不会白试）");
    }

    printf("\n===== 场景5：瞬态退避时长足够短 =====\n");
    {
        GameTime::SetMs(500000); g_new.clear();
        NewMark(700, SPELL_FAILED_TOO_CLOSE);
        CHECK(NewBackedOff(700), "刚失败时退避");
        GameTime::AdvanceMs(1000);
        CHECK(!NewBackedOff(700), "1 秒后就能重试（0.8秒退避）");

        g_new.clear();
        NewMark(701, SPELL_FAILED_NOT_READY);
        GameTime::AdvanceMs(600);
        CHECK(!NewBackedOff(701), "NOT_READY 0.5秒后重试");
    }

    printf("\n===== 场景6：射程预检（不产生失败）=====\n");
    {
        // 模拟 CheckSkill 的射程判定
        auto rangeOk = [](float dist, float minR, float maxR) -> bool {
            if (maxR > 0.0f && dist > maxR) return false;
            if (minR > 0.0f && dist < minR) return false;
            return true;
        };
        // 猎人射击：最小 5 码，最大 35 码
        CHECK(!rangeOk(3.0f, 5.0f, 35.0f),  "怪在3码 -> 预检拦下（不会产生TOO_CLOSE失败）");
        CHECK(rangeOk(20.0f, 5.0f, 35.0f),  "怪在20码 -> 正常放");
        CHECK(!rangeOk(50.0f, 5.0f, 35.0f), "怪在50码 -> 预检拦下");
        // 近战：无最小射程
        CHECK(rangeOk(3.0f, 0.0f, 5.0f),    "近战3码 -> 正常");
    }

    printf("\n===== 场景7：各职业主循环技能数够不够 =====\n");
    {
        struct C { uint8 c; char const* n; };
        C classes[] = {{1,"战士"},{2,"圣骑"},{3,"猎人"},{4,"盗贼"},{5,"牧师"},
                       {6,"死骑"},{7,"萨满"},{8,"法师"},{9,"术士"},{11,"德鲁伊"}};
        bool allOk = true;
        for (auto& cc : classes)
        {
            int minN = 99;
            for (uint8 sp=0; sp<CombatSpec::GetSpecCount(cc.c); ++sp)
                for (uint8 r=1; r<=3; ++r)
                {
                    CombatSpec::BuiltPlan pl;
                    CombatSpec::BuildPlan(cc.c, sp, r, CombatSpec::SCENE_RAID, false, pl);
                    if (int(pl.core.size()) < minN) minN = int(pl.core.size());
                }
            printf("         %-8s 最少 %d 个技能\n", cc.n, minN);
            if (minN < 8) allOk = false;
        }
        CHECK(allOk, "所有职业所有职责 core >= 8 个技能");
    }

    printf("\n===== 场景8：猎人/法师方案完整性 =====\n");
    {
        // 猎人射击专精
        CombatSpec::BuiltPlan h;
        CombatSpec::BuildPlan(3, 1, CombatSpec::ROLE_DPS, CombatSpec::SCENE_RAID, false, h);
        bool hasArcane=false, hasSteady=false, hasSerpent=false;
        for (auto const& s : h.core) {
            if (s.spell==3044)  hasArcane=true;
            if (s.spell==56641) hasSteady=true;
            if (s.spell==1978)  hasSerpent=true;
        }
        printf("         猎人-射击 core %zu 个技能\n", h.core.size());
        CHECK(hasArcane,  "含【奥术射击】（用户点名说没有的）");
        CHECK(hasSteady,  "含【稳固射击】");
        CHECK(hasSerpent, "含【毒蛇钉刺】");

        // 法师火焰
        CombatSpec::BuiltPlan m;
        CombatSpec::BuildPlan(8, 1, CombatSpec::ROLE_DPS, CombatSpec::SCENE_RAID, false, m);
        bool hasFireball=false, hasPyro=false, hasScorch=false, hasBlast=false;
        for (auto const& s : m.core) {
            if (s.spell==133)   hasFireball=true;
            if (s.spell==11366) hasPyro=true;
            if (s.spell==2948)  hasScorch=true;
            if (s.spell==2136)  hasBlast=true;
        }
        printf("         法师-火焰 core %zu 个技能\n", m.core.size());
        CHECK(hasFireball, "含【火球术】");
        CHECK(hasPyro,     "含【炎爆术】");
        CHECK(hasScorch,   "含【灼烧】");
        CHECK(hasBlast,    "含【火焰冲击】（用户说只有这个）");
    }

    printf("\n========================================\n");
    printf("  通过 %d / 失败 %d\n", g_pass, g_fail);
    printf("========================================\n");
    return g_fail ? 1 : 0;
}
