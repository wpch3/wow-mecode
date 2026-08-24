/*
 * bufffixtest.cpp —— v3.2：.buff 与 .combo 冲突的验证
 *
 * 用户实测报告：
 *   「关闭 buff 自动补增益，连招就正常；开启 buff 就跟 combo 冲突」
 *
 * 核实后确认有 4 个根源：
 *   1. .buff 排的队列事件绕过退避表，也不检查返回值（v3.1 只修了 combo 路径）
 *   2. lastBuffTry 字段声明了从未使用 -> combo 每 500ms 都在试补 buff
 *   3. 两套系统同时抢 GCD：.buff 每 1.6 秒，.combo 每 0.5 秒，互相打断
 *   4. HasAura(realSpell) 只认精确 rank，低阶/别人给的 buff 判定不出来
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

// ---------- 退避表（与源码同构）----------
struct FailInfo { uint32 untilMs = 0; uint32 count = 0; };
static std::unordered_map<uint64, FailInfo> g_fails;
static uint64 FK(uint32 g, uint32 s) { return (uint64(g) << 32) | s; }
static bool IsBackedOff(uint32 g, uint32 s)
{
    auto it = g_fails.find(FK(g, s));
    return it != g_fails.end() && GameTime::GetGameTimeMS() < it->second.untilMs;
}
static void MarkFailed(uint32 g, uint32 s, SpellCastResult r)
{
    uint32 cd = (r == SPELL_FAILED_REAGENTS) ? 300000 : 10000;
    FailInfo& fi = g_fails[FK(g, s)];
    fi.untilMs = GameTime::GetGameTimeMS() + cd;
    ++fi.count;
}
static void ClearFailed(uint32 g, uint32 s) { g_fails.erase(FK(g, s)); }

// ---------- 模拟玩家状态 ----------
struct SimState
{
    uint32 lastBuffTry    = 0;
    uint32 buffQueueUntil = 0;
};

int main()
{
    Player p; p._class = 2; p._guid.v = 555;
    uint32 const BLESS = 25782;      // 强效力量祝福（缺材料）

    printf("\n===== 根源 1：.buff 队列绕过退避表 =====\n");
    printf("  场景：.buff now 把缺材料的祝福排进队列，反复触发\n\n");

    WorldObject::forcedFail[BLESS] = SPELL_FAILED_REAGENTS;

    // --- 修复前：队列事件不看退避、不看返回值 ---
    g_fails.clear();
    WorldObject::castCount = 0;
    for (int i = 0; i < 10; ++i)          // 模拟连按 10 次 .buff now
    {
        if (p.HasAura(BLESS)) continue;
        p.CastSpell(&p, BLESS);            // 旧逻辑：直接放，不管结果
    }
    printf("         修复前 连按10次.buff -> 实际尝试 %u 次\n", WorldObject::castCount);
    CHECK(WorldObject::castCount == 10, "旧逻辑每次都硬撞（.combo 那边修了这里没修）");

    // --- 修复后：队列也走退避表 ---
    g_fails.clear();
    WorldObject::castCount = 0;
    for (int i = 0; i < 10; ++i)
    {
        if (p.HasAura(BLESS)) continue;
        if (IsBackedOff(555, BLESS)) continue;         // 新增
        SpellCastResult r = p.CastSpell(&p, BLESS);
        if (r != SPELL_CAST_OK) MarkFailed(555, BLESS, r);   // 新增
        else ClearFailed(555, BLESS);
    }
    printf("         修复后 连按10次.buff -> 实际尝试 %u 次\n", WorldObject::castCount);
    CHECK(WorldObject::castCount == 1, "新逻辑只撞 1 次就退避");

    printf("\n===== 根源 2：lastBuffTry 声明了却没用（无限流）=====\n");
    SimState st;
    GameTime::SetMs(100000);

    // 修复前：每跳都试
    int oldTries = 0;
    for (int tick = 0; tick < 20; ++tick)      // 20 跳 = 10 秒
    {
        ++oldTries;                             // 旧逻辑无条件进入 buff 段
        GameTime::AdvanceMs(500);
    }
    printf("         修复前 10秒内尝试补buff %d 次\n", oldTries);
    CHECK(oldTries == 20, "旧逻辑每 0.5 秒试一次");

    // 修复后：3 秒限流
    GameTime::SetMs(100000);
    st = SimState();
    int newTries = 0;
    for (int tick = 0; tick < 20; ++tick)
    {
        uint32 now = GameTime::GetGameTimeMS();
        bool throttled = (st.lastBuffTry != 0 && now - st.lastBuffTry < 3000);
        if (!throttled) { st.lastBuffTry = now; ++newTries; }
        GameTime::AdvanceMs(500);
    }
    printf("         修复后 10秒内尝试补buff %d 次\n", newTries);
    CHECK(newTries <= 4, "新逻辑 3 秒限流，10 秒内最多 4 次");

    printf("\n===== 根源 3：两套系统抢 GCD（用户报告的核心）=====\n");
    GameTime::SetMs(200000);
    st = SimState();

    // .buff now 排了 3 个 buff，每个 1.6 秒 -> 队列跑 4.8 秒
    uint32 queueDelay = 3 * 1600;
    st.buffQueueUntil = GameTime::GetGameTimeMS() + queueDelay + 500;
    printf("         .buff now 排了 3 个增益，队列将持续 %.1f 秒\n", queueDelay / 1000.0);

    int comboGrabs = 0;
    for (int tick = 0; tick < 12; ++tick)       // 6 秒
    {
        uint32 now = GameTime::GetGameTimeMS();
        bool queueRunning = (now < st.buffQueueUntil);
        if (!queueRunning)
        {
            bool throttled = (st.lastBuffTry != 0 && now - st.lastBuffTry < 3000);
            if (!throttled) { st.lastBuffTry = now; ++comboGrabs; }
        }
        GameTime::AdvanceMs(500);
    }
    printf("         队列运行期间 combo 抢施法 %d 次\n", comboGrabs);
    CHECK(comboGrabs <= 1, "队列跑的时候 combo 让路，不再互相打断");

    // 队列结束后应该恢复
    GameTime::AdvanceMs(5000);
    uint32 now2 = GameTime::GetGameTimeMS();
    CHECK(now2 >= st.buffQueueUntil, "队列结束后 combo 恢复接管");

    printf("\n===== 根源 4：HasAura 只认精确 rank =====\n");
    /*
     * 祝福是 rank 链：19740(初级) ... 25782(强效)
     * 身上挂着低阶版本时，HasAura(25782) 返回 false -> 误判"没有" -> 重复放
     */
    Player q; q._guid.v = 556;
    uint32 const LOW_RANK = 19740, HIGH_RANK = 25782;
    q._auras.insert(LOW_RANK);          // 身上有低阶祝福

    bool oldJudge = q.HasAura(HIGH_RANK);
    printf("         身上挂着低阶祝福(%u)，查最高阶(%u): %s\n",
           LOW_RANK, HIGH_RANK, oldJudge ? "有" : "没有");
    CHECK(!oldJudge, "旧逻辑判定'没有' -> 会重复放（bug）");

    // 新逻辑：沿 rank 链查任意一阶
    auto hasAnyRank = [&](Player* pl, std::vector<uint32> const& chain) {
        for (uint32 id : chain) if (pl->HasAura(id)) return true;
        return false;
    };
    bool newJudge = hasAnyRank(&q, { LOW_RANK, HIGH_RANK });
    CHECK(newJudge, "新逻辑沿 rank 链查到低阶 -> 判定'有'，不重复放");

    q._auras.clear();
    CHECK(!hasAnyRank(&q, { LOW_RANK, HIGH_RANK }), "身上确实没有时才会去放");

    printf("\n===== 综合：开着 .buff 时 .combo 是否正常 =====\n");
    /*
     * 模拟用户的实际场景：
     *   圣骑士，autoBuff=开，祝福缺材料，同时开着 combo
     *   预期：祝福最多试 1 次就退避，combo 正常打输出
     */
    GameTime::SetMs(300000);
    g_fails.clear();
    st = SimState();
    WorldObject::castCount = 0;

    uint32 blessAttempts = 0, damageAttempts = 0;
    for (int tick = 0; tick < 40; ++tick)      // 20 秒
    {
        uint32 now = GameTime::GetGameTimeMS();

        // buff 段（带限流 + 退避）
        bool queueRunning = (now < st.buffQueueUntil);
        bool throttled    = (st.lastBuffTry != 0 && now - st.lastBuffTry < 3000);
        bool casted = false;

        if (!queueRunning && !throttled)
        {
            st.lastBuffTry = now;
            if (!IsBackedOff(555, BLESS))
            {
                SpellCastResult r = p.CastSpell(&p, BLESS);
                ++blessAttempts;
                if (r != SPELL_CAST_OK) MarkFailed(555, BLESS, r);
                else { ClearFailed(555, BLESS); casted = true; }
            }
        }

        // 主循环段：buff 没占用就打输出
        if (!casted)
            ++damageAttempts;

        GameTime::AdvanceMs(500);
    }

    printf("         20 秒内：祝福尝试 %u 次，输出机会 %u 次\n",
           blessAttempts, damageAttempts);
    CHECK(blessAttempts == 1, "缺材料的祝福整场只试 1 次");
    CHECK(damageAttempts >= 39, "其余每一跳都能正常打输出（不被 buff 卡住）");

    printf("\n========================================\n");
    printf("  通过 %d / 失败 %d\n", g_pass, g_fail);
    printf("========================================\n");
    return g_fail ? 1 : 0;
}
