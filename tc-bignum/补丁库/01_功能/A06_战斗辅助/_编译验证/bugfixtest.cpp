/*
 * bugfixtest.cpp —— v3.1 两个修复的验证
 *
 * 1. 无限释放增益 bug（用户实测：圣骑士一直放强效力量祝福）
 *    根因：CastSpell 返回 SpellCastResult，但代码没检查返回值。
 *          缺材料 -> 每次都 SPELL_FAILED_REAGENTS -> HasAura 永远假 -> 无限重试
 *    修复：失败就记进退避表，短时间不再试
 *
 * 2. 打断优先级：挑蓄力大招打，而不是见读条就打
 */
#include "mock.h"
#include "CombatSpecData.h"

namespace GameTime { void AdvanceMs(uint32 d); }

static int g_pass = 0, g_fail = 0;
static void CHECK(bool ok, char const* what)
{
    if (ok) { ++g_pass; printf("  [OK]   %s\n", what); }
    else    { ++g_fail; printf("  [FAIL] %s\n", what); }
}

// ---- 复刻退避表 ----
struct FailInfo { uint32 untilMs = 0; uint32 count = 0; };
static std::unordered_map<uint64, FailInfo> g_fails;
static uint64 FailKey(uint32 guid, uint32 sp) { return (uint64(guid) << 32) | sp; }

static bool IsBackedOff(uint32 guid, uint32 sp)
{
    auto it = g_fails.find(FailKey(guid, sp));
    return it != g_fails.end() && GameTime::GetGameTimeMS() < it->second.untilMs;
}
static void MarkFailed(uint32 guid, uint32 sp, SpellCastResult res)
{
    uint32 cd;
    switch (res)
    {
        case SPELL_FAILED_REAGENTS: case SPELL_FAILED_ITEM_NOT_FOUND:
        case SPELL_FAILED_BAD_TARGETS: case SPELL_FAILED_NOT_SHAPESHIFT:
        case SPELL_FAILED_ONLY_SHAPESHIFT:      cd = 300000; break;
        case SPELL_FAILED_OUT_OF_RANGE: case SPELL_FAILED_LINE_OF_SIGHT:
        case SPELL_FAILED_UNIT_NOT_INFRONT: case SPELL_FAILED_NOT_BEHIND:
        case SPELL_FAILED_MOVING: case SPELL_FAILED_SPELL_IN_PROGRESS:
        case SPELL_FAILED_NOT_READY:            cd = 2000;   break;
        case SPELL_FAILED_NO_POWER:             cd = 5000;   break;
        default:                                cd = 10000;  break;
    }
    FailInfo& fi = g_fails[FailKey(guid, sp)];
    fi.untilMs = GameTime::GetGameTimeMS() + cd;
    ++fi.count;
    if (fi.count >= 3 && cd < 60000)
        fi.untilMs = GameTime::GetGameTimeMS() + 60000;
}
static void ClearFailed(uint32 guid, uint32 sp) { g_fails.erase(FailKey(guid, sp)); }

// ---- 复刻打断打分 ----
struct CE { Unit* unit=nullptr; uint32 score=0; bool isAoe=false; char const* name="?"; };
static void FindBestInterrupt(Player* me, std::vector<Unit*> const& enemies, CE& out)
{
    out = CE();
    for (Unit* u : enemies)
    {
        if (!u || !u->IsAlive()) continue;
        Spell* sp = u->GetCurrentSpell(CURRENT_GENERIC_SPELL);
        if (!sp || sp->getState() != SPELL_STATE_PREPARING) continue;
        SpellInfo const* si = sp->GetSpellInfo();
        if (!si) continue;
        if (si->PreventionType != SPELL_PREVENTION_TYPE_SILENCE) continue;
        int32 remain = sp->GetTimer();
        int32 total  = sp->GetCastTime();
        if (remain < 300) continue;
        uint32 score = uint32(total > 0 ? total : 1000);
        bool aoe = si->IsTargetingArea();
        if (aoe) score *= 2;
        if (u->GetVictim() == me) score += 2000;
        if (remain > 1000) score += 1000;
        if (score > out.score) { out.unit=u; out.score=score; out.isAoe=aoe; out.name=si->SpellName[0]; }
    }
}

int main()
{
    printf("\n===== BUG 1：圣骑士无限放强效力量祝福 =====\n");
    printf("  场景：25782 缺材料，服务端每次返回 SPELL_FAILED_REAGENTS\n\n");

    Player p; p._class = 2; p._guid.v = 777;
    uint32 const BLESSING = 25782;
    WorldObject::forcedFail[BLESSING] = SPELL_FAILED_REAGENTS;

    // --- 修复前：不检查返回值 ---
    WorldObject::castCount = 0;
    for (int tick = 0; tick < 20; ++tick)
    {
        // 旧逻辑：HasAura 假 -> 直接放 -> 不看结果
        if (!p.HasAura(BLESSING))
            p.CastSpell(&p, BLESSING);
    }
    uint32 oldCasts = WorldObject::castCount;
    printf("         修复前 20 跳内尝试了 %u 次（无限刷屏）\n", oldCasts);
    CHECK(oldCasts == 20, "旧逻辑确实每跳都重试 -> 这就是 bug");

    // --- 修复后：检查返回值 + 退避 ---
    g_fails.clear();
    WorldObject::castCount = 0;
    for (int tick = 0; tick < 20; ++tick)
    {
        if (IsBackedOff(777, BLESSING)) continue;
        if (p.HasAura(BLESSING)) continue;
        SpellCastResult r = p.CastSpell(&p, BLESSING);
        if (r != SPELL_CAST_OK) MarkFailed(777, BLESSING, r);
        else ClearFailed(777, BLESSING);
    }
    uint32 newCasts = WorldObject::castCount;
    printf("         修复后 20 跳内只尝试了 %u 次\n", newCasts);
    CHECK(newCasts == 1, "新逻辑只试 1 次就退避，不再刷屏");

    printf("\n===== 退避时长分档 =====\n");
    g_fails.clear();
    MarkFailed(777, 100, SPELL_FAILED_REAGENTS);
    CHECK(IsBackedOff(777,100), "缺材料 -> 立刻退避");
    GameTime::AdvanceMs(60000);
    CHECK(IsBackedOff(777,100), "缺材料 60 秒后仍在退避（硬失败5分钟）");

    g_fails.clear();
    MarkFailed(777, 101, SPELL_FAILED_OUT_OF_RANGE);
    CHECK(IsBackedOff(777,101), "距离远 -> 短退避");
    GameTime::AdvanceMs(2500);
    CHECK(!IsBackedOff(777,101), "2.5 秒后恢复（走近就能放）");

    g_fails.clear();
    MarkFailed(777, 102, SPELL_FAILED_NO_POWER);
    GameTime::AdvanceMs(3000);
    CHECK(IsBackedOff(777,102), "没蓝 3 秒内仍退避");
    GameTime::AdvanceMs(3000);
    CHECK(!IsBackedOff(777,102), "没蓝 6 秒后恢复（等回蓝）");

    printf("\n===== 反复失败拉长退避 =====\n");
    g_fails.clear();
    for (int i = 0; i < 3; ++i) { MarkFailed(777, 103, SPELL_FAILED_INTERRUPTED); }
    GameTime::AdvanceMs(11000);
    CHECK(IsBackedOff(777,103), "连续失败3次后退避拉长到60秒");

    printf("\n===== 成功后清除退避 =====\n");
    g_fails.clear();
    MarkFailed(777, 104, SPELL_FAILED_OUT_OF_RANGE);
    CHECK(IsBackedOff(777,104), "失败后处于退避");
    ClearFailed(777, 104);
    CHECK(!IsBackedOff(777,104), "成功一次后立刻清除退避");

    printf("\n===== 正常技能不受影响 =====\n");
    WorldObject::forcedFail.clear();
    g_fails.clear();
    WorldObject::castCount = 0;
    for (int tick = 0; tick < 10; ++tick)
    {
        if (IsBackedOff(777, 200)) continue;
        SpellCastResult r = p.CastSpell(&p, 200);
        if (r != SPELL_CAST_OK) MarkFailed(777, 200, r);
        else ClearFailed(777, 200);
    }
    printf("         能成功的技能 10 跳放了 %u 次\n", WorldObject::castCount);
    CHECK(WorldObject::castCount == 10, "成功的技能每跳照放，退避不误伤");

    printf("\n===== BUG 2：打断优先蓄力大招 =====\n");
    Unit e1, e2, e3;
    SpellInfo s1, s2, s3;
    Spell c1, c2, c3;

    // e1: 短读条小技能 1 秒
    s1.PreventionType = SPELL_PREVENTION_TYPE_SILENCE; s1.SpellName[0] = "小火球";
    c1._si=&s1; c1._castTime=1000; c1._timer=800; e1._curSpell=&c1;

    // e2: 长读条蓄力大招 5 秒
    s2.PreventionType = SPELL_PREVENTION_TYPE_SILENCE; s2.SpellName[0] = "毁灭陨石";
    c2._si=&s2; c2._castTime=5000; c2._timer=4000; e2._curSpell=&c2;

    // e3: 中等读条但是群体技 3 秒
    s3.PreventionType = SPELL_PREVENTION_TYPE_SILENCE; s3.SpellName[0] = "烈焰风暴"; s3._aoe=true;
    c3._si=&s3; c3._castTime=3000; c3._timer=2500; e3._curSpell=&c3;

    CE best;
    FindBestInterrupt(&p, {&e1}, best);
    printf("         只有小技能 -> 打断 %s\n", best.name);
    CHECK(best.unit==&e1, "只有一个目标时就打它");

    FindBestInterrupt(&p, {&e1,&e2}, best);
    printf("         小火球(1秒) vs 毁灭陨石(5秒) -> 打断 %s\n", best.name);
    CHECK(best.unit==&e2, "优先打断读条更长的蓄力大招");

    FindBestInterrupt(&p, {&e1,&e2,&e3}, best);
    printf("         再加 烈焰风暴(3秒群体) -> 打断 %s (分数%u)\n", best.name, best.score);
    CHECK(best.unit==&e3, "群体技 x2 加权后超过 5 秒单体");
    CHECK(best.isAoe, "正确标记为群体技");

    printf("\n===== 打断的边界情况 =====\n");
    Spell almost; SpellInfo si4;
    si4.PreventionType = SPELL_PREVENTION_TYPE_SILENCE; si4.SpellName[0]="快好了";
    almost._si=&si4; almost._castTime=3000; almost._timer=100;   // 只剩 0.1 秒
    Unit e4; e4._curSpell=&almost;
    FindBestInterrupt(&p, {&e4}, best);
    CHECK(best.unit==nullptr, "剩余读条 <0.3 秒 -> 不浪费打断");

    Spell unstop; SpellInfo si5;
    si5.PreventionType = SPELL_PREVENTION_TYPE_NONE; si5.SpellName[0]="无法打断";
    unstop._si=&si5; unstop._castTime=5000; unstop._timer=4000;
    Unit e5; e5._curSpell=&unstop;
    FindBestInterrupt(&p, {&e5}, best);
    CHECK(best.unit==nullptr, "打不断的法术 -> 不浪费打断技");

    Unit e6;   // 没在读条
    FindBestInterrupt(&p, {&e6}, best);
    CHECK(best.unit==nullptr, "没读条 -> 不触发打断");

    printf("\n===== 冲我来的加权 =====\n");
    Unit e7, e8; SpellInfo s7, s8; Spell c7, c8;
    s7.PreventionType=SPELL_PREVENTION_TYPE_SILENCE; s7.SpellName[0]="打别人";
    c7._si=&s7; c7._castTime=3000; c7._timer=2000; e7._curSpell=&c7;
    s8.PreventionType=SPELL_PREVENTION_TYPE_SILENCE; s8.SpellName[0]="打我的";
    c8._si=&s8; c8._castTime=3000; c8._timer=2000; e8._curSpell=&c8;
    e8._victim = &p;    // 正在打我
    FindBestInterrupt(&p, {&e7,&e8}, best);
    printf("         同样3秒读条，一个打我一个打别人 -> 打断 %s\n", best.name);
    CHECK(best.unit==&e8, "同等条件下优先打断冲我来的");

    printf("\n========================================\n");
    printf("  通过 %d / 失败 %d\n", g_pass, g_fail);
    printf("========================================\n");
    return g_fail ? 1 : 0;
}
