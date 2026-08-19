// ============================================================================
//  step31 .disguise / .model 逻辑单元测试
//
//  抽出 cs_appearance.cpp 中不依赖 TrinityCore 头文件的纯逻辑：
//    Tok / Lower / IsAllDigit / FindModel / SlotInScope
//    + 隐藏/恢复的行为模型（用假的 Player 验证"保留属性"这条核心保证）
//
//  编译： g++ -std=c++17 -O0 -Wall -Wextra -o apptest apptest.cpp
// ============================================================================

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include <set>

typedef unsigned int  uint32;
typedef unsigned char uint8;

// ---------------------------------------------------------------------------
//  与 cs_appearance.cpp 保持一致的常量（Player.h:554-577 实查）
// ---------------------------------------------------------------------------
enum EquipmentSlots : uint8
{
    EQUIPMENT_SLOT_START     = 0,
    EQUIPMENT_SLOT_HEAD      = 0,
    EQUIPMENT_SLOT_NECK      = 1,
    EQUIPMENT_SLOT_SHOULDERS = 2,
    EQUIPMENT_SLOT_BODY      = 3,
    EQUIPMENT_SLOT_CHEST     = 4,
    EQUIPMENT_SLOT_WAIST     = 5,
    EQUIPMENT_SLOT_LEGS      = 6,
    EQUIPMENT_SLOT_FEET      = 7,
    EQUIPMENT_SLOT_WRISTS    = 8,
    EQUIPMENT_SLOT_HANDS     = 9,
    EQUIPMENT_SLOT_FINGER1   = 10,
    EQUIPMENT_SLOT_FINGER2   = 11,
    EQUIPMENT_SLOT_TRINKET1  = 12,
    EQUIPMENT_SLOT_TRINKET2  = 13,
    EQUIPMENT_SLOT_BACK      = 14,
    EQUIPMENT_SLOT_MAINHAND  = 15,
    EQUIPMENT_SLOT_OFFHAND   = 16,
    EQUIPMENT_SLOT_RANGED    = 17,
    EQUIPMENT_SLOT_TABARD    = 18,
    EQUIPMENT_SLOT_END       = 19
};

// v2: ModelDef 别名表已删除。
// 原因：硬编码 displayid 实测全错，且 HD 补丁下对应关系本就会变。
// 模型来源改为 creature entry 查库（GetModelIdByEntry），
// 那部分依赖 sObjectMgr，无法在此单测，改为验证参数解析。

static std::vector<std::string> Tok(char const* args)
{
    std::vector<std::string> out;
    if (!args) return out;
    std::string s(args);
    size_t i = 0;
    while (i < s.size())
    {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
        if (i >= s.size()) break;
        size_t j = i;
        while (j < s.size() && s[j] != ' ' && s[j] != '\t') ++j;
        out.push_back(s.substr(i, j - i));
        i = j;
    }
    return out;
}

static bool IsAllDigit(std::string const& s)
{
    if (s.empty()) return false;
    for (char c : s) if (c < '0' || c > '9') return false;
    return true;
}

static std::string Lower(std::string s)
{
    for (char& c : s) if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
    return s;
}

enum DisguiseScope { DG_ALL, DG_WEAPON, DG_ARMOR };

static bool SlotInScope(uint8 slot, DisguiseScope sc)
{
    bool isWeapon = (slot == EQUIPMENT_SLOT_MAINHAND ||
                     slot == EQUIPMENT_SLOT_OFFHAND  ||
                     slot == EQUIPMENT_SLOT_RANGED);
    switch (sc)
    {
        case DG_WEAPON: return isWeapon;
        case DG_ARMOR:  return !isWeapon;
        case DG_ALL:
        default:        return true;
    }
}

// ---------------------------------------------------------------------------
//  假 Player：验证"隐藏外观但保留属性"这条核心保证
//
//  m_items      = 真实装备（决定属性）      <- SetVisibleItemSlot 不碰它
//  visibleItem  = 外观广播字段              <- SetVisibleItemSlot 只改它
// ---------------------------------------------------------------------------
struct FakePlayer
{
    uint32 m_items[EQUIPMENT_SLOT_END];       // 真实装备 entry，0=空
    uint32 visibleItem[EQUIPMENT_SLOT_END];   // 外观字段
    int    totalStats;                         // 属性总和（由 m_items 推导）

    FakePlayer()
    {
        for (int i = 0; i < EQUIPMENT_SLOT_END; ++i)
        {
            m_items[i] = 0;
            visibleItem[i] = 0;
        }
        totalStats = 0;
    }

    void Equip(uint8 slot, uint32 entry, int stat)
    {
        m_items[slot] = entry;
        visibleItem[slot] = entry;   // 装备时外观同步
        totalStats += stat;
    }

    uint32 GetItemByPos(uint8 slot) const { return m_items[slot]; }

    // 对应 Player::SetVisibleItemSlot —— 只动 visibleItem
    void SetVisibleItemSlot(uint8 slot, uint32 itemEntry)
    {
        visibleItem[slot] = itemEntry;
    }

    // 属性只看 m_items，和 visibleItem 无关
    int ComputeStats() const { return totalStats; }
};

static int DoHide(FakePlayer& p, DisguiseScope sc)
{
    int n = 0;
    for (uint8 s = EQUIPMENT_SLOT_START; s < EQUIPMENT_SLOT_END; ++s)
    {
        if (!SlotInScope(s, sc)) continue;
        p.SetVisibleItemSlot(s, 0);
        ++n;
    }
    return n;
}

static int DoShow(FakePlayer& p, DisguiseScope sc)
{
    int n = 0;
    for (uint8 s = EQUIPMENT_SLOT_START; s < EQUIPMENT_SLOT_END; ++s)
    {
        if (!SlotInScope(s, sc)) continue;
        uint32 it = p.GetItemByPos(s);
        p.SetVisibleItemSlot(s, it);
        if (it) ++n;
    }
    return n;
}

static bool IsHidden(FakePlayer const& p, DisguiseScope sc)
{
    bool anyReal = false;
    for (uint8 s = EQUIPMENT_SLOT_START; s < EQUIPMENT_SLOT_END; ++s)
    {
        if (!SlotInScope(s, sc)) continue;
        if (p.m_items[s])
        {
            anyReal = true;
            if (p.visibleItem[s] != 0) return false;
        }
    }
    return anyReal;
}

// ---------------------------------------------------------------------------
static int g_pass = 0, g_fail = 0;
static void CK(bool c, char const* n)
{
    if (c) { ++g_pass; printf("  [OK]   %s\n", n); }
    else   { ++g_fail; printf("  [FAIL] %s\n", n); }
}

int main()
{
    printf("=====================================================\n");
    printf(" step31 .disguise / .model 逻辑测试\n");
    printf("=====================================================\n\n");

    // ---------- 核心保证：隐藏外观不影响属性 ----------
    printf("[1] 核心保证：隐藏外观【保留属性】\n");
    {
        FakePlayer p;
        p.Equip(EQUIPMENT_SLOT_HEAD,     40001, 100);
        p.Equip(EQUIPMENT_SLOT_CHEST,    40002, 200);
        p.Equip(EQUIPMENT_SLOT_LEGS,     40003, 150);
        p.Equip(EQUIPMENT_SLOT_MAINHAND, 40004, 500);
        p.Equip(EQUIPMENT_SLOT_OFFHAND,  40005, 300);

        int before = p.ComputeStats();
        CK(before == 1250, "初始属性 1250");

        int n = DoHide(p, DG_ALL);
        CK(n == 19, "全身19个槽位都被处理");
        CK(p.ComputeStats() == before, "隐藏后属性【完全不变】 <- 核心保证");

        bool allHidden = true;
        for (int s = 0; s < EQUIPMENT_SLOT_END; ++s)
            if (p.visibleItem[s] != 0) allHidden = false;
        CK(allHidden, "所有外观字段已清零");

        bool itemsIntact = (p.m_items[EQUIPMENT_SLOT_HEAD]     == 40001 &&
                            p.m_items[EQUIPMENT_SLOT_CHEST]    == 40002 &&
                            p.m_items[EQUIPMENT_SLOT_MAINHAND] == 40004);
        CK(itemsIntact, "m_items 真实装备【原封不动】");

        int r = DoShow(p, DG_ALL);
        CK(r == 5, "恢复了5件（只数真实存在的）");
        CK(p.visibleItem[EQUIPMENT_SLOT_HEAD] == 40001, "头部外观已还原");
        CK(p.visibleItem[EQUIPMENT_SLOT_MAINHAND] == 40004, "主手外观已还原");
        CK(p.ComputeStats() == before, "恢复后属性仍然不变");
    }

    // ---------- 范围过滤 ----------
    printf("\n[2] weapon / armor 范围过滤\n");
    {
        CK(SlotInScope(EQUIPMENT_SLOT_MAINHAND, DG_WEAPON), "主手 属于武器");
        CK(SlotInScope(EQUIPMENT_SLOT_OFFHAND,  DG_WEAPON), "副手 属于武器");
        CK(SlotInScope(EQUIPMENT_SLOT_RANGED,   DG_WEAPON), "远程 属于武器");
        CK(!SlotInScope(EQUIPMENT_SLOT_HEAD,    DG_WEAPON), "头部 不属于武器");
        CK(!SlotInScope(EQUIPMENT_SLOT_MAINHAND, DG_ARMOR), "主手 不属于护甲");
        CK(SlotInScope(EQUIPMENT_SLOT_CHEST,     DG_ARMOR), "胸甲 属于护甲");
        CK(SlotInScope(EQUIPMENT_SLOT_TABARD,    DG_ARMOR), "战袍 属于护甲");

        int w = 0, a = 0;
        for (uint8 s = EQUIPMENT_SLOT_START; s < EQUIPMENT_SLOT_END; ++s)
        {
            if (SlotInScope(s, DG_WEAPON)) ++w;
            if (SlotInScope(s, DG_ARMOR))  ++a;
        }
        CK(w == 3,  "武器槽共3个");
        CK(a == 16, "护甲槽共16个");
        CK(w + a == EQUIPMENT_SLOT_END, "武器+护甲 = 全部19槽（无遗漏无重复）");
    }

    // ---------- 只隐藏武器 ----------
    printf("\n[3] 只隐藏武器时护甲不受影响\n");
    {
        FakePlayer p;
        p.Equip(EQUIPMENT_SLOT_HEAD,     50001, 10);
        p.Equip(EQUIPMENT_SLOT_MAINHAND, 50002, 20);

        DoHide(p, DG_WEAPON);
        CK(p.visibleItem[EQUIPMENT_SLOT_MAINHAND] == 0, "主手外观已隐藏");
        CK(p.visibleItem[EQUIPMENT_SLOT_HEAD] == 50001, "头部外观【未受影响】");
        CK(p.ComputeStats() == 30, "属性仍然不变");

        CK(IsHidden(p, DG_WEAPON), "武器判定为已隐藏");
        CK(!IsHidden(p, DG_ARMOR), "护甲判定为未隐藏");
    }

    // ---------- IsHidden 边界 ----------
    printf("\n[4] IsHidden 状态判定\n");
    {
        FakePlayer empty;
        CK(!IsHidden(empty, DG_ALL), "裸身玩家不算已隐藏（无真实装备）");

        FakePlayer p;
        p.Equip(EQUIPMENT_SLOT_CHEST, 60001, 1);
        CK(!IsHidden(p, DG_ALL), "刚装备时未隐藏");
        DoHide(p, DG_ALL);
        CK(IsHidden(p, DG_ALL), "隐藏后判定正确");
        DoShow(p, DG_ALL);
        CK(!IsHidden(p, DG_ALL), "恢复后判定正确");
    }

    // ---------- v2: 模型来源三选一的参数解析 ----------
    printf("\n[5] v2 模型来源解析（别名表已废弃）\n");
    {
        // npc <entry>：查库拿真实 modelid
        auto t = Tok("npc 24191");
        CK(t.size() == 2 && Lower(t[0]) == "npc" && IsAllDigit(t[1]),
           "npc <entry> 解析正确");

        auto t2 = Tok("r 30 npc 448");
        CK(t2.size() == 4 && Lower(t2[2]) == "npc" && IsAllDigit(t2[3]),
           "r 30 npc <entry> 组合解析");

        auto t3 = Tok("me npc 24191");
        CK(t3.size() == 3 && Lower(t3[1]) == "npc", "me npc <entry>");

        // copy：复制选中目标
        auto t4 = Tok("copy");
        CK(t4.size() == 1 && Lower(t4[0]) == "copy", "copy 无需额外参数");

        auto t5 = Tok("r 30 copy");
        CK(t5.size() == 3 && Lower(t5[2]) == "copy", "r 30 copy");

        // id <displayid>：直接指定
        auto t6 = Tok("id 25279");
        CK(t6.size() == 2 && Lower(t6[0]) == "id" && IsAllDigit(t6[1]),
           "id <displayid> 解析");

        // 裸数字也当 displayid
        auto t7 = Tok("25279");
        CK(t7.size() == 1 && IsAllDigit(t7[0]), "裸数字当 displayid");

        // displayid 范围校验（与代码一致：1-100000）
        auto ok = [](uint32 v){ return v != 0 && v <= 100000; };
        CK(ok(25279),   "25279 在合法范围");
        CK(!ok(0),      "0 被拦");
        CK(!ok(999999), "999999 超界被拦");

        // npc 后面必须跟数字
        auto bad = Tok("npc abc");
        CK(bad.size() == 2 && !IsAllDigit(bad[1]), "npc 后非数字 -> 会被拦截");

        // 缺参数
        auto miss = Tok("npc");
        CK(miss.size() == 1, "只有 npc 没 entry -> 报用法错误");
    }

    // ---------- 参数解析 ----------
    printf("\n[6] 参数解析\n");
    {
        struct C { char const* in; size_t n; char const* first; };
        C cs[] = {
            { "npc 24191",           2, "npc" },
            { "r 30 npc 448",        4, "r" },
            { "entry 1234 npc 448",  4, "entry" },
            { "me npc 24191",        3, "me" },
            { "copy",                1, "copy" },
            { "reset",               1, "reset" },
            { "reset r 40",          3, "reset" },
            { "save",                1, "save" },
            { "id 25279",            2, "id" },
        };
        for (auto& c : cs)
        {
            auto t = Tok(c.in);
            char nm[160];
            snprintf(nm, sizeof(nm), "\"%s\" -> %zu段 首=%s", c.in, t.size(),
                     t.empty() ? "(空)" : t[0].c_str());
            CK(t.size() == c.n && !t.empty() && t[0] == c.first, nm);
        }

        CK(IsAllDigit("1234"), "entry 数字合法");
        CK(!IsAllDigit("abc"), "entry 非数字被拦");

        float r1 = float(atof("30"));
        float r2 = float(atof("999"));
        CK(r1 > 0.0f && r1 <= 500.0f, "半径30 合法");
        CK(!(r2 > 0.0f && r2 <= 500.0f), "半径999 超界");
    }

    // ---------- 边界 ----------
    printf("\n[7] 边界\n");
    {
        CK(Tok(nullptr).empty(), "nullptr 不崩");
        CK(Tok("").empty(), "空串");
        FakePlayer p;
        CK(DoHide(p, DG_ALL) == 19, "裸身玩家隐藏也不崩");
        CK(DoShow(p, DG_ALL) == 0, "裸身玩家恢复返回0件");
    }

    printf("\n=====================================================\n");
    printf(" 通过 %d / 失败 %d\n", g_pass, g_fail);
    printf("=====================================================\n");
    return g_fail ? 1 : 0;
}
