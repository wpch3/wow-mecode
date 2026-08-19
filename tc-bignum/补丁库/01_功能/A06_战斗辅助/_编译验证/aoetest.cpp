/*
 * aoetest.cpp —— 验证 CountNearbyEnemies 的修复
 *
 * 修复前：只数 getAttackers()（正在打我的）
 *   → 站桩 AOE 时怪还没摸到我，攻击者列表为空，AOE 永远放不出来
 * 修复后：用官方格子搜索 Cell::VisitAllObjects
 *   → 半径内所有敌对单位都数得到
 */
#include "mock.h"
#include "CombatSpecData.h"

static int g_pass = 0, g_fail = 0;
static void CHECK(bool ok, char const* what)
{
    if (ok) { ++g_pass; printf("  [OK]   %s\n", what); }
    else    { ++g_fail; printf("  [FAIL] %s\n", what); }
}

// ---- 复刻修复后的 CountNearbyEnemies ----
static uint32 CountNearbyEnemies_NEW(Player* player, float radius)
{
    std::list<Unit*> targets;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck check(player, player, radius);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck>
        searcher(player, targets, check);
    Cell::VisitAllObjects(player, searcher, radius);

    uint32 n = 0;
    for (Unit* u : targets)
    {
        if (!u || !u->IsAlive())         continue;
        if (u->IsTotem() || u->IsCritter()) continue;
        if (!player->IsValidAttackTarget(u)) continue;
        ++n;
    }
    return n;
}

// ---- 复刻修复前的旧逻辑，用于对比 ----
static uint32 CountNearbyEnemies_OLD(Player* player, float radius)
{
    uint32 n = 0;
    if (Unit* v = player->GetVictim())
        if (v->IsAlive()) ++n;
    for (Unit* a : player->getAttackers())
        if (a && a->IsAlive() && player->GetExactDist2d(a) <= radius) ++n;
    return n ? n : 1;
}

int main()
{
    Player p;
    p._class = 8; p._maxhp = 100000; p._hp = 100000;

    printf("\n===== 场景1：站桩 AOE，5 只怪但都还没摸到我 =====\n");
    printf("  （打本最常见：法师提前放暴风雪 / 骑士开奉献拉怪）\n");
    std::vector<Unit> mobs(5);
    g_fakeNearby.clear();
    for (auto& m : mobs) { m._hp = 5000; m._maxhp = 5000; g_fakeNearby.push_back(&m); }
    p._attackers.clear();       // 没有任何怪在打我
    p._victim = nullptr;

    uint32 oldN = CountNearbyEnemies_OLD(&p, 10.0f);
    uint32 newN = CountNearbyEnemies_NEW(&p, 10.0f);
    printf("         旧逻辑数到 %u 只，新逻辑数到 %u 只\n", oldN, newN);
    CHECK(oldN < 2,  "旧逻辑只数到 <2 只 -> AOE 放不出来（这就是 bug）");
    CHECK(newN == 5, "新逻辑正确数到 5 只 -> AOE 能放");

    printf("\n===== 场景2：怪已经在打我了 =====\n");
    for (auto& m : mobs) p._attackers.insert(&m);
    p._victim = &mobs[0];
    oldN = CountNearbyEnemies_OLD(&p, 10.0f);
    newN = CountNearbyEnemies_NEW(&p, 10.0f);
    printf("         旧逻辑数到 %u 只，新逻辑数到 %u 只\n", oldN, newN);
    CHECK(newN == 5, "新逻辑仍是 5 只（不会因为重复计数变多）");
    CHECK(oldN == 6, "旧逻辑重复计了 victim -> 数成 6 只（也是错的）");

    printf("\n===== 场景3：只有 1 只怪，AOE 不该放 =====\n");
    g_fakeNearby.clear();
    g_fakeNearby.push_back(&mobs[0]);
    p._attackers.clear(); p._victim = nullptr;
    newN = CountNearbyEnemies_NEW(&p, 10.0f);
    CHECK(newN == 1, "正确数到 1 只 -> AOE 会被跳过");

    printf("\n===== 场景4：图腾和小动物要排除 =====\n");
    Unit totem, critter, realMob;
    totem._hp = 100; totem._maxhp = 100; totem._totem = true;
    critter._hp = 10; critter._maxhp = 10; critter._critter = true;
    realMob._hp = 5000; realMob._maxhp = 5000;
    g_fakeNearby.clear();
    g_fakeNearby.push_back(&totem);
    g_fakeNearby.push_back(&critter);
    g_fakeNearby.push_back(&realMob);
    newN = CountNearbyEnemies_NEW(&p, 10.0f);
    printf("         3 个单位里只有 1 个是真怪\n");
    CHECK(newN == 1, "图腾/小动物被排除，只数真怪");

    printf("\n===== 场景5：死掉的怪不算 =====\n");
    Unit dead; dead._hp = 0; dead._maxhp = 5000;
    g_fakeNearby.clear();
    g_fakeNearby.push_back(&realMob);
    g_fakeNearby.push_back(&dead);
    newN = CountNearbyEnemies_NEW(&p, 10.0f);
    CHECK(newN == 1, "尸体不计入");

    printf("\n===== 场景6：AOE 阈值联动 =====\n");
    g_fakeNearby.clear();
    for (int i = 0; i < 3; ++i) g_fakeNearby.push_back(&mobs[i]);
    uint32 cnt = CountNearbyEnemies_NEW(&p, 10.0f);
    bool aoeAllowed = (cnt >= 2);
    CHECK(aoeAllowed, "3 只怪 -> AOE 判定通过");

    g_fakeNearby.clear();
    g_fakeNearby.push_back(&mobs[0]);
    cnt = CountNearbyEnemies_NEW(&p, 10.0f);
    aoeAllowed = (cnt >= 2);
    CHECK(!aoeAllowed, "1 只怪 -> AOE 判定拒绝");

    printf("\n========================================\n");
    printf("  通过 %d / 失败 %d\n", g_pass, g_fail);
    printf("========================================\n");
    return g_fail ? 1 : 0;
}
