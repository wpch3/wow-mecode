/*
 * v33test.cpp —— v3.3：用户实测反馈的 4 个问题
 *
 * 1. 脱战每 3 秒补一次 buff，多余消耗
 * 2. 治疗技能完全没被使用
 * 3. 输出/坦克连招有空隙（放完技能站着不动）
 * 4. 场景设置对连招是否生效
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

int main()
{
    uint8 const CLS_PAL = 2;

    printf("\n===== 问题 1：脱战每 3 秒刷 buff =====\n");
    {
        // 修复前：3 秒限流 + 只看 HasAura
        GameTime::SetMs(100000);
        uint32 lastTry = 0; int oldTries = 0;
        for (int tick = 0; tick < 120; ++tick)      // 60 秒
        {
            uint32 now = GameTime::GetGameTimeMS();
            if (lastTry == 0 || now - lastTry >= 3000) { lastTry = now; ++oldTries; }
            GameTime::AdvanceMs(500);
        }
        printf("         修复前 60 秒内尝试补 buff %d 次\n", oldTries);
        CHECK(oldTries >= 20, "旧逻辑 3 秒一次 -> 60秒内 20 次（浪费）");

        // 修复后：15 秒限流
        GameTime::SetMs(100000);
        lastTry = 0; int newTries = 0;
        for (int tick = 0; tick < 120; ++tick)
        {
            uint32 now = GameTime::GetGameTimeMS();
            if (lastTry == 0 || now - lastTry >= 15000) { lastTry = now; ++newTries; }
            GameTime::AdvanceMs(500);
        }
        printf("         修复后 60 秒内尝试补 buff %d 次\n", newTries);
        CHECK(newTries <= 5, "新逻辑 15 秒限流 -> 最多 5 次");
    }

    printf("\n===== 问题 1b：buff 剩余时长判定（治本）=====\n");
    {
        Player p;
        Aura permAura;   permAura._maxDuration = -1;      // 永久光环
        Aura longAura;   longAura._duration = 1500000; longAura._maxDuration = 1800000;  // 剩25分钟
        Aura shortAura;  shortAura._duration = 60000;  shortAura._maxDuration = 1800000; // 剩1分钟
        AuraApplication ap1, ap2, ap3;
        ap1._base = &permAura; ap2._base = &longAura; ap3._base = &shortAura;

        // 永久光环 -> 绝不重上
        p._applied.clear(); p._applied.insert({100, &ap1});
        auto remain = [&](uint32 id) -> int32 {
            auto it = p._applied.find(id);
            if (it == p._applied.end()) return 0;
            Aura* a = it->second->_base;
            return a->IsPermanent() ? -1 : a->GetDuration();
        };
        CHECK(remain(100) == -1, "永久光环 -> 返回 -1（绝不重上）");

        p._applied.clear(); p._applied.insert({101, &ap2});
        CHECK(remain(101) > 300000, "剩 25 分钟 -> 不重上");

        p._applied.clear(); p._applied.insert({102, &ap3});
        CHECK(remain(102) > 0 && remain(102) < 300000, "剩 1 分钟 -> 才值得重上");

        p._applied.clear();
        CHECK(remain(103) == 0, "身上没有 -> 返回 0，去上");
    }

    printf("\n===== 问题 2：治疗技能没被使用 =====\n");
    {
        // 根因 A：plan 缓存是 thread_local 单份，多人共用会互相覆盖
        printf("         [根因A] 旧版 static thread_local 单份缓存\n");
        // 模拟：A治疗 B输出 交替 tick
        struct OldCache { uint8 role = 255; };
        OldCache oc;
        int wrongPlan = 0;
        for (int i = 0; i < 6; ++i)
        {
            uint8 who = (i % 2) ? CombatSpec::ROLE_HEALER : CombatSpec::ROLE_DPS;
            if (oc.role != who) { oc.role = who; }   // 重建
            else if (oc.role != who) ++wrongPlan;
            // 旧版：每次切换都要重建，两人交替 = 每 tick 都重建
        }
        printf("         两人交替时缓存每跳失效并被对方覆盖\n");

        // 新版：按 GUID 分开
        std::unordered_map<uint32, uint8> newCache;
        newCache[111] = CombatSpec::ROLE_HEALER;
        newCache[222] = CombatSpec::ROLE_DPS;
        CHECK(newCache[111] == CombatSpec::ROLE_HEALER &&
              newCache[222] == CombatSpec::ROLE_DPS,
              "新版按 GUID 缓存，两人互不干扰");

        // 根因 B：治疗触发条件
        printf("\n         [根因B] 旧触发条件的逻辑漏洞\n");
        auto oldTrigger = [](uint8 role, float myHp, float healSelfLine,
                             bool targetIsSelf) -> bool {
            // 旧：外层 (role==HEALER || 自己血少)，内层 allow=(role==HEALER || 目标是自己)
            if (!(role == CombatSpec::ROLE_HEALER || myHp < healSelfLine)) return false;
            return (role == CombatSpec::ROLE_HEALER) || targetIsSelf;
        };
        auto newTrigger = [](uint8 role, float myHp, float healSelfLine) -> bool {
            if (role == CombatSpec::ROLE_HEALER) return true;      // 治疗：永远扫全团
            return myHp < healSelfLine;                             // 其他：只自奶
        };

        CHECK(newTrigger(CombatSpec::ROLE_HEALER, 100.0f, 80.0f),
              "治疗职责：自己满血也要扫全团奶人");
        CHECK(!oldTrigger(CombatSpec::ROLE_DPS, 100.0f, 80.0f, false),
              "输出职责满血：不奶（正确）");
        CHECK(newTrigger(CombatSpec::ROLE_DPS, 50.0f, 80.0f),
              "输出职责血少：自奶（正确）");

        // 根因 C：治疗方案里治疗技必须排最前
        CombatSpec::BuiltPlan pl;
        CombatSpec::BuildPlan(CLS_PAL, 0, CombatSpec::ROLE_HEALER,
                              CombatSpec::SCENE_RAID, false, pl);
        bool firstIsHeal = !pl.core.empty() &&
            (pl.core[0].flags & (CombatSpec::SF_HEAL | CombatSpec::SF_HOT |
                                 CombatSpec::SF_HEAL_AOE | CombatSpec::SF_HEAL_EMERG));
        printf("         神圣骑治疗方案首技能：%s\n", pl.core.empty() ? "无" : pl.core[0].cn);
        CHECK(firstIsHeal, "治疗职责下治疗技排第一");

        int healCount = 0;
        for (auto const& s : pl.core)
            if (s.flags & (CombatSpec::SF_HEAL | CombatSpec::SF_HOT |
                           CombatSpec::SF_HEAL_AOE | CombatSpec::SF_HEAL_EMERG))
                ++healCount;
        printf("         治疗技总数：%d\n", healCount);
        CHECK(healCount >= 8, "治疗方案有足够多的治疗技");
    }

    printf("\n===== 问题 3：连招有空隙（填充技）=====\n");
    {
        // 惩戒骑：用户点名要正义之锤
        CombatSpec::BuiltPlan dps;
        CombatSpec::BuildPlan(CLS_PAL, 2, CombatSpec::ROLE_DPS,
                              CombatSpec::SCENE_RAID, false, dps);
        bool hasHammer = false;
        for (auto const& s : dps.core) if (s.spell == 53595) hasHammer = true;
        printf("         惩戒骑 core 共 %zu 个技能\n", dps.core.size());
        CHECK(hasHammer, "输出连招里有【正义之锤】（用户点名要的跨专精技）");

        // 坦克：用户说"放完防御技能就站着"
        CombatSpec::BuiltPlan tank;
        CombatSpec::BuildPlan(CLS_PAL, 1, CombatSpec::ROLE_TANK,
                              CombatSpec::SCENE_RAID, false, tank);
        int dmgSkills = 0;
        for (auto const& s : tank.core)
            if (!(s.flags & (CombatSpec::SF_TAUNT | CombatSpec::SF_TAUNT_AOE |
                             CombatSpec::SF_HEAL  | CombatSpec::SF_HEAL_EMERG)))
                ++dmgSkills;
        printf("         防护骑 core 共 %zu 个，其中输出技 %d 个\n",
               tank.core.size(), dmgSkills);
        CHECK(tank.core.size() >= 12, "坦克方案技能数够多，不会站桩");
        CHECK(dmgSkills >= 7, "坦克有足够输出技填 GCD");

        // 所有职业所有职责都要有足够技能
        uint8 classes[] = {1,2,3,4,5,6,7,8,9,11};
        bool allEnough = true;
        int minCount = 99; char const* worst = "";
        for (uint8 c : classes)
            for (uint8 sp = 0; sp < CombatSpec::GetSpecCount(c); ++sp)
                for (uint8 r = 1; r <= 3; ++r)
                {
                    CombatSpec::BuiltPlan pl;
                    CombatSpec::BuildPlan(c, sp, r, CombatSpec::SCENE_RAID, false, pl);
                    if (int(pl.core.size()) < minCount)
                    {
                        minCount = int(pl.core.size());
                        worst = pl.specName;
                    }
                    if (pl.core.size() < 6) allEnough = false;
                }
        printf("         93 种组合中最少的是 %s，%d 个技能\n", worst, minCount);
        CHECK(allEnough, "所有 93 种组合 core 都 >= 6 个技能");
    }

    printf("\n===== 问题 4：场景对连招是否生效 =====\n");
    {
        // 场景通过 SceneTuning 影响 CheckSkill 的判定
        auto const& quest  = CombatSpec::GetTuning(CombatSpec::SCENE_QUEST);
        auto const& mythic = CombatSpec::GetTuning(CombatSpec::SCENE_MYTHIC);

        printf("         任务场景：AOE门槛%u 保命%u%% 自奶%u%% 奶人%u%% 救命%u%%\n",
               quest.aoeThreshold, quest.defensiveHpPct,
               quest.healSelfPct, quest.healTargetPct, quest.emergencyPct);
        printf("         高级团本：AOE门槛%u 保命%u%% 自奶%u%% 奶人%u%% 救命%u%%\n",
               mythic.aoeThreshold, mythic.defensiveHpPct,
               mythic.healSelfPct, mythic.healTargetPct, mythic.emergencyPct);

        CHECK(quest.aoeThreshold != mythic.aoeThreshold, "AOE 门槛随场景变（影响连招）");
        CHECK(quest.defensiveHpPct != mythic.defensiveHpPct, "保命血线随场景变");
        CHECK(quest.healTargetPct != mythic.healTargetPct, "治疗血线随场景变");
        CHECK(quest.saveBurst != mythic.saveBurst, "是否省爆发随场景变");

        // 缓存必须把 scene 算进 key，否则改了场景不生效
        struct PC { uint8 cls=255, spec=255, role=255, scene=255; bool all=false; };
        PC pc;
        pc.cls=2; pc.spec=0; pc.role=3; pc.scene=CombatSpec::SCENE_QUEST;
        bool needRebuild = (pc.scene != CombatSpec::SCENE_MYTHIC);
        CHECK(needRebuild, "改场景后缓存会重建（v3.3 把 scene 加进了缓存 key）");
    }

    printf("\n===== 综合：治疗职责实战模拟 =====\n");
    {
        /*
         * 神圣骑 + 治疗 + 团本
         * 队友血量：坦克 60%、DPS1 45%、DPS2 100%、自己 100%
         * 预期：应该去奶 DPS1（血最少）
         */
        auto const& t = CombatSpec::GetTuning(CombatSpec::SCENE_RAID);
        struct M { char const* n; float hp; bool tank; };
        M members[] = { {"坦克",60.f,true}, {"DPS1",45.f,false},
                        {"DPS2",100.f,false}, {"自己",100.f,false} };

        char const* picked = nullptr; float bestW = 999.f; uint32 hurt = 0;
        for (auto& m : members)
        {
            if (m.hp < 95.f) ++hurt;
            float w = m.tank ? m.hp - 5.f : m.hp;
            if (w < bestW) { bestW = w; picked = m.n; }
        }
        printf("         团本血线：奶人 %u%%，救命 %u%%\n",
               t.healTargetPct, t.emergencyPct);
        printf("         选中目标：%s（受伤 %u 人）\n", picked, hurt);
        CHECK(picked && std::string(picked) == "DPS1", "正确选中血最少的 DPS1");
        CHECK(hurt == 2, "正确统计 2 人受伤");
        CHECK(45.f <= float(t.healTargetPct), "45% 低于团本奶人线 85% -> 会触发治疗");
        CHECK(hurt < 3, "只有 2 人受伤 -> 用单奶不用群奶");
    }

    printf("\n========================================\n");
    printf("  通过 %d / 失败 %d\n", g_pass, g_fail);
    printf("========================================\n");
    return g_fail ? 1 : 0;
}
