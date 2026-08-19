/*
 * runtest.cpp —— 逻辑功能测试
 * 覆盖：专精表完整性、rank解析、移动判定、条件过滤、菜单项数、配栏槽位
 */
#include "mock.h"
#include "CombatSpecData.h"

void SetTestPlayer(Player* p);

static int g_pass = 0, g_fail = 0;
static void CHECK(bool ok, char const* what)
{
    if (ok) { ++g_pass; printf("  [OK]   %s\n", what); }
    else    { ++g_fail; printf("  [FAIL] %s\n", what); }
}

int main()
{
    ChatHandler::verbose = false;

    printf("\n===== 1. 专精表结构 =====\n");
    auto const& all = CombatSpec::GetAllSpecs();
    CHECK(all.size() == 31, "共 31 个专精（10职业×3 + 德鲁伊第4）");

    uint8 classes[] = {1,2,3,4,5,6,7,8,9,11};
    for (uint8 c : classes)
    {
        uint8 n = CombatSpec::GetSpecCount(c);
        uint8 want = (c == 11) ? 4 : 3;
        char buf[128];
        snprintf(buf, sizeof(buf), "职业%u 有 %u 个专精（期望 %u）", c, n, want);
        CHECK(n == want, buf);
    }

    printf("\n===== 2. 每个专精数据完整性 =====\n");
    bool allGood = true, allHaveRot = true, allNamed = true;
    uint32 totalSkills = 0;
    for (auto const& sp : all)
    {
        if (sp.rotation.empty()) allHaveRot = false;
        if (!sp.name || !sp.role) allNamed = false;
        totalSkills += uint32(sp.rotation.size() + sp.burst.size()
                            + sp.defensive.size() + sp.buffs.size());
        // 每个技能都要有中文名和非零ID
        for (auto const& g : { sp.rotation, sp.burst, sp.defensive, sp.buffs })
            for (auto const& sk : g)
                if (!sk.spell || !sk.cn || !*sk.cn) allGood = false;
    }
    CHECK(allHaveRot, "每个专精都有主循环");
    CHECK(allNamed,   "每个专精都有名称和定位");
    CHECK(allGood,    "每个技能都有非零ID和中文名");
    printf("         (技能条目总数 %u)\n", totalSkills);

    printf("\n===== 3. 主循环内无重复技能 =====\n");
    bool noDup = true;
    for (auto const& sp : all)
    {
        std::set<uint32> seen;
        for (auto const& sk : sp.rotation)
            if (!seen.insert(sk.spell).second)
            {
                printf("       重复: %s 的 %u\n", sp.name, sk.spell);
                noDup = false;
            }
    }
    CHECK(noDup, "所有专精主循环无重复");

    printf("\n===== 4. 坦克/治疗/DPS 定位齐全 =====\n");
    int tank = 0, heal = 0, dps = 0;
    for (auto const& sp : all)
    {
        std::string r = sp.role;
        if (r == "坦克") ++tank;
        else if (r == "治疗") ++heal;
        else ++dps;
    }
    printf("         坦克 %d / 治疗 %d / DPS %d\n", tank, heal, dps);
    CHECK(tank >= 4, "至少 4 个坦克专精（战/骑/DK/德熊）");
    CHECK(heal >= 5, "至少 5 个治疗专精（骑/牧×2/萨/德）");

    printf("\n===== 5. GetSpec 查询 =====\n");
    CHECK(CombatSpec::GetSpec(1, 0) != nullptr, "战士 spec0 存在");
    CHECK(CombatSpec::GetSpec(11, 3) != nullptr, "德鲁伊 spec3（恢复）存在");
    CHECK(CombatSpec::GetSpec(11, 4) == nullptr, "德鲁伊 spec4 不存在");
    CHECK(CombatSpec::GetSpec(10, 0) == nullptr, "职业10（空缺）无专精");

    printf("\n===== 6. 菜单项数不超 GOSSIP 上限 =====\n");
    bool underLimit = true;
    for (uint8 c : classes)
    {
        // 专精数 + 全专精 + 关闭
        uint32 items = CombatSpec::GetSpecCount(c) + 2;
        if (items > GOSSIP_MAX_MENU_ITEMS) underLimit = false;
    }
    CHECK(underLimit, "所有职业菜单项 <= 32（不会触发 ASSERT）");
    printf("         (德鲁伊最多：4专精+全专精+关闭 = 6 项)\n");

    printf("\n===== 7. 配栏槽位不越界（v3.5：主循环两排24格）=====\n");
    /*
     * v3.5 布局：主循环 0-23(24格) / 爆发 24-35 / 保命 36-47 / 增益功能 48-59
     * 这里按【组装后的 BuiltPlan】校验，才是真实上栏的内容。
     */
    bool slotOk = true;
    size_t maxCore = 0; char const* worstSpec = "";
    uint8 clsList[] = {1,2,3,4,5,6,7,8,9,11};
    for (uint8 c : clsList)
        for (uint8 spi = 0; spi < CombatSpec::GetSpecCount(c); ++spi)
            for (uint8 r = 1; r <= 3; ++r)
            {
                CombatSpec::BuiltPlan pl;
                CombatSpec::BuildPlan(c, spi, r, CombatSpec::SCENE_RAID, false, pl);

                size_t z4 = pl.opener.size();
                for (auto const& u : pl.utility)
                {
                    bool dup = false;
                    for (auto const& o : pl.opener) if (o.spell == u.spell) dup = true;
                    if (!dup) ++z4;
                }
                if (pl.core.size() > maxCore) { maxCore = pl.core.size(); worstSpec = pl.specName; }
                if (pl.core.size()      > 24) slotOk = false;
                if (pl.burst.size()     > 12) slotOk = false;
                if (pl.emergency.size() > 12) slotOk = false;
                if (z4                  > 12) slotOk = false;
            }
    printf("         最大主循环：%s %zu 个（上限24）\n", worstSpec, maxCore);
    CHECK(slotOk, "93种组合各区都放得下，无截断");

    printf("\n===== 8. 移动标记合理性 =====\n");
    // 法师三系都该有读条技标记
    int mageNoMove = 0;
    for (auto const& sp : all)
        if (sp.cls == 8)
            for (auto const& sk : sp.rotation)
                if (sk.flags & CombatSpec::SF_NO_MOVE) ++mageNoMove;
    CHECK(mageNoMove >= 6, "法师有多个读条技标了 SF_NO_MOVE");

    // 盗贼终结技该标 COMBO_FINISH
    int rogueFin = 0;
    for (auto const& sp : all)
        if (sp.cls == 4)
            for (auto const& sk : sp.rotation)
                if (sk.flags & CombatSpec::SF_COMBO_FINISH) ++rogueFin;
    CHECK(rogueFin >= 6, "盗贼终结技标了 SF_COMBO_FINISH");

    // 战士该有斩杀标记
    int warExec = 0;
    for (auto const& sp : all)
        if (sp.cls == 1)
            for (auto const& sk : sp.rotation)
                if (sk.flags & CombatSpec::SF_EXECUTE) ++warExec;
    CHECK(warExec >= 2, "战士斩杀标了 SF_EXECUTE");

    printf("\n===== 9. 全专精汇总去重 =====\n");
    // 模拟 CollectAllSpecSkills 的去重逻辑
    for (uint8 c : {1, 11})
    {
        std::vector<CombatSpec::SpecInfo const*> specs;
        CombatSpec::GetSpecsOfClass(c, specs);
        std::set<uint32> merged;
        uint32 raw = 0;
        for (auto* sp : specs)
            for (auto const& sk : sp->rotation) { ++raw; merged.insert(sk.spell); }
        char buf[128];
        snprintf(buf, sizeof(buf), "职业%u 全专精主循环 %u 条去重后 %zu 条", c, raw, merged.size());
        CHECK(merged.size() > 0 && merged.size() <= raw, buf);
    }

    printf("\n===== 10. 德鲁伊 4 窗口内容各不相同 =====\n");
    auto* d0 = CombatSpec::GetSpec(11, 0);
    auto* d1 = CombatSpec::GetSpec(11, 1);
    auto* d2 = CombatSpec::GetSpec(11, 2);
    auto* d3 = CombatSpec::GetSpec(11, 3);
    CHECK(d0 && d1 && d2 && d3, "德鲁伊 4 个专精都存在");
    if (d0 && d1 && d2 && d3)
    {
        CHECK(d0->rotation[0].spell != d1->rotation[0].spell, "平衡 != 野性猫");
        CHECK(d1->rotation[0].spell != d2->rotation[0].spell, "野性猫 != 野性熊");
        CHECK(d2->rotation[0].spell != d3->rotation[0].spell, "野性熊 != 恢复");
        printf("         平衡:%s  猫:%s  熊:%s  恢复:%s\n",
               d0->rotation[0].cn, d1->rotation[0].cn,
               d2->rotation[0].cn, d3->rotation[0].cn);
    }

    printf("\n========================================\n");
    printf("  通过 %d / 失败 %d\n", g_pass, g_fail);
    printf("========================================\n");
    return g_fail ? 1 : 0;
}
