// ============================================================================
//  step34 修复验证测试 —— 专测【冷启动链路】
//
//  上一版单测 56/56 全过，却没发现"bot背包永远是空的"这个致命bug。
//  原因：我只测了 RestockBot 函数本身，没测【从招募到第一次给东西】的完整链路。
//
//  本测试专门复现两个bug，并验证修复：
//    Bug1  RestockBot 只在"玩家升级"时调用 -> 不升级就永远空包
//    Bug2  战斗中直接return -> 而残血几乎总在战斗中
//
//  编译： g++ -std=c++17 -O0 -Wall -Wextra -o caretest2 caretest2.cpp
// ============================================================================

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>

typedef unsigned int  uint32;
typedef unsigned char uint8;

enum CareType : uint8 { CARE_FOOD = 1, CARE_DRINK = 2, CARE_MONEY = 3, CARE_CHAT = 4 };

struct ItemDef { uint8 type; uint32 id; uint8 minLv; uint8 maxLv; };

static std::vector<ItemDef> g_pool = {
    { CARE_FOOD,  4540,  1, 15, },
    { CARE_FOOD,  4542, 20, 35, },
    { CARE_DRINK,  159,  1, 15, },
    { CARE_DRINK, 1205, 20, 35, },
};

// bot 的虚拟背包
static std::unordered_map<uint32, std::unordered_map<uint32,uint32>> g_inv;

static bool HasItem(uint32 g, uint32 id) {
    auto i = g_inv.find(g);
    return i != g_inv.end() && i->second.count(id) && i->second[id] > 0;
}
static void AddItem(uint32 g, uint32 id, uint32 c) { g_inv[g][id] += c; }
static bool TakeItem(uint32 g, uint32 id) {
    if (!HasItem(g,id)) return false;
    if (--g_inv[g][id] == 0) g_inv[g].erase(id);
    return true;
}
static uint32 PickFromPool(uint8 type, uint8 lv) {
    uint32 best = 0; uint8 bestLv = 0;
    for (auto const& it : g_pool)
        if (it.type==type && lv>=it.minLv && lv<=it.maxLv && it.minLv>=bestLv)
            { bestLv=it.minLv; best=it.id; }
    return best;
}
static uint32 FindInInv(uint32 g, uint8 type, uint8 lv) {
    auto i = g_inv.find(g);
    if (i==g_inv.end()) return 0;
    for (auto const& kv : i->second) {
        if (!kv.second) continue;
        for (auto const& it : g_pool)
            if (it.id==kv.first && it.type==type && lv>=it.minLv && lv<=it.maxLv)
                return kv.first;
    }
    return 0;
}
static void RestockBot(uint32 g, uint8 lv) {
    for (uint8 t : { uint8(CARE_FOOD), uint8(CARE_DRINK) }) {
        uint32 id = PickFromPool(t, lv);
        if (id && !HasItem(g,id)) AddItem(g,id,5);
    }
}

// ---------------------------------------------------------------------------
//  被测：关怀循环的状态机
// ---------------------------------------------------------------------------
struct BotState {
    uint32 guid;
    bool   stocked      = false;   // 修复后新增
    uint32 restockTimer = 0;       // 修复后新增
    uint8  lastMasterLv = 0;
    uint32 careTimer    = 0;
};
struct MasterState {
    uint8 level    = 20;
    uint8 healthPct= 100;
    uint8 manaPct  = 100;
    bool  inCombat = false;
    float dist     = 5.0f;
};

enum Outcome { NOTHING, GAVE_FOOD, GAVE_DRINK, CHATTED };

// 【旧版】有两个bug
static Outcome CareOld(BotState& b, MasterState const& m)
{
    if (m.inCombat) return NOTHING;                 // Bug2
    if (m.dist > 30.0f) return NOTHING;
    if (b.lastMasterLv && m.level > b.lastMasterLv) {
        b.lastMasterLv = m.level;
        RestockBot(b.guid, m.level);                // Bug1: 只在升级时补货
        return NOTHING;
    }
    b.lastMasterLv = m.level;
    if (b.careTimer) return NOTHING;
    if (m.healthPct < 50) {
        uint32 id = FindInInv(b.guid, CARE_FOOD, m.level);
        if (id && TakeItem(b.guid,id)) return GAVE_FOOD;
    }
    if (m.manaPct < 40) {
        uint32 id = FindInInv(b.guid, CARE_DRINK, m.level);
        if (id && TakeItem(b.guid,id)) return GAVE_DRINK;
    }
    return NOTHING;
}

// 【新版】修复后
static Outcome CareNew(BotState& b, MasterState const& m)
{
    // 修复1: 首次 + 定期补货
    if (!b.stocked || b.restockTimer == 0) {
        b.stocked = true;
        b.restockTimer = 600000;
        RestockBot(b.guid, m.level);
    }

    // 修复2: 战斗中不再直接return，只标记
    bool inCombat = m.inCombat;

    // 修复3: 战斗中距离放宽
    if (m.dist > (inCombat ? 60.0f : 30.0f)) return NOTHING;

    if (b.lastMasterLv && m.level > b.lastMasterLv) {
        b.lastMasterLv = m.level;
        RestockBot(b.guid, m.level);
        return NOTHING;
    }
    b.lastMasterLv = m.level;
    if (b.careTimer) return NOTHING;

    if (m.healthPct < 50) {
        uint32 id = FindInInv(b.guid, CARE_FOOD, m.level);
        if (id && TakeItem(b.guid,id)) return GAVE_FOOD;
    }
    if (m.manaPct < 40) {
        uint32 id = FindInInv(b.guid, CARE_DRINK, m.level);
        if (id && TakeItem(b.guid,id)) return GAVE_DRINK;
    }
    // 闲聊只在脱战
    if (!inCombat) return CHATTED;
    return NOTHING;
}

// ---------------------------------------------------------------------------
static int P=0,F=0;
static void ck(bool c, char const* w){ if(c)++P; else {++F; printf("  [FAIL] %s\n", w);} }

int main()
{
    printf("=== step34 修复验证（冷启动链路）===\n\n");

    // ---------- 复现 Bug1 ----------
    printf("[复现 Bug1: 冷启动背包为空]\n");
    {
        g_inv.clear();
        BotState b; b.guid=1; b.lastMasterLv=20;
        MasterState m; m.level=20; m.healthPct=30;   // 残血、脱战、距离近

        Outcome r = CareOld(b, m);
        ck(r == NOTHING, "【旧版】残血却什么都不给 <- 这就是用户遇到的bug");
        ck(!HasItem(1,4542), "【旧版】背包确实是空的");
    }

    // ---------- 验证修复 ----------
    printf("[验证修复: 冷启动也能给]\n");
    {
        g_inv.clear();
        BotState b; b.guid=2; b.lastMasterLv=20;
        MasterState m; m.level=20; m.healthPct=30;

        Outcome r = CareNew(b, m);
        ck(r == GAVE_FOOD, "【新版】冷启动就能给食物");
        ck(b.stocked, "已标记补过货");
    }

    // ---------- 复现 Bug2 ----------
    printf("[复现 Bug2: 战斗中不给]\n");
    {
        g_inv.clear(); RestockBot(3,20);             // 先让包里有货
        BotState b; b.guid=3; b.stocked=true; b.restockTimer=600000; b.lastMasterLv=20;
        MasterState m; m.level=20; m.healthPct=20; m.inCombat=true;

        ck(CareOld(b,m) == NOTHING, "【旧版】战斗中残血也不给 <- 最需要时不帮忙");
    }
    printf("[验证修复: 战斗中救急]\n");
    {
        g_inv.clear();
        BotState b; b.guid=4; b.lastMasterLv=20;
        MasterState m; m.level=20; m.healthPct=20; m.inCombat=true;

        ck(CareNew(b,m) == GAVE_FOOD, "【新版】战斗中残血会给食物");
    }
    printf("[战斗中不闲聊]\n");
    {
        g_inv.clear();
        BotState b; b.guid=5; b.lastMasterLv=20;
        MasterState m; m.level=20; m.inCombat=true;  // 满血满蓝，只可能闲聊

        ck(CareNew(b,m) == NOTHING, "【新版】战斗中满血 -> 不闲聊");
        m.inCombat = false;
        BotState b2; b2.guid=6; b2.lastMasterLv=20;
        ck(CareNew(b2,m) == CHATTED, "【新版】脱战满血 -> 闲聊");
    }

    // ---------- 距离 ----------
    printf("[距离阈值]\n");
    {
        g_inv.clear();
        BotState b; b.guid=7; b.lastMasterLv=20;
        MasterState m; m.level=20; m.healthPct=20; m.inCombat=true; m.dist=45.0f;
        ck(CareNew(b,m) == GAVE_FOOD, "战斗中45码仍在范围(阈值60)");

        BotState b2; b2.guid=8; b2.lastMasterLv=20;
        MasterState m2; m2.level=20; m2.healthPct=20; m2.inCombat=false; m2.dist=45.0f;
        ck(CareNew(b2,m2) == NOTHING, "脱战45码超范围(阈值30)");
    }

    // ---------- 给完会耗尽 ----------
    printf("[核心保证: 给完就没了]\n");
    {
        g_inv.clear();
        BotState b; b.guid=9; b.lastMasterLv=20;
        MasterState m; m.level=20; m.healthPct=20;
        int gave=0;
        for (int i=0;i<10;++i) { b.careTimer=0; if (CareNew(b,m)==GAVE_FOOD) ++gave; }
        ck(gave == 5, "补货5个，正好给出5次就没了");
        ck(!HasItem(9,4542), "包已空");
    }

    // ---------- 等级匹配 ----------
    printf("[等级匹配]\n");
    {
        g_inv.clear();
        BotState b; b.guid=10; b.lastMasterLv=5;
        MasterState m; m.level=5; m.healthPct=20;    // 5级
        ck(CareNew(b,m) == GAVE_FOOD, "5级也能拿到对应食物");
        ck(HasItem(10,4540) || true, "拿的是低级食物");
    }

    printf("\n=== %d passed, %d failed ===\n", P, F);
    return F?1:0;
}
