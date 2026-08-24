// 幻化模块真实逻辑测试：槽位别名解析 + 缓存增删查 + 方案保存
#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstdio>

typedef uint8_t uint8; typedef uint32_t uint32;
#define TRANSMOG_MAX_SLOT 19

// ============ 从 CustomTransmog.cpp 原样复制的表与函数 ============
static char const* const g_slotNames[TRANSMOG_MAX_SLOT] =
{
    "头部","颈部","肩部","衬衣","胸甲","腰带","腿部","靴子","护腕","手套",
    "戒指1","戒指2","饰品1","饰品2","披风","主手","副手","远程","战袍"
};

struct SlotAlias { char const* alias; uint8 slot; };
static SlotAlias const g_slotAlias[] =
{
    { "头",     0 }, { "头部",   0 }, { "头盔",   0 }, { "head",      0 },
    { "颈",     1 }, { "颈部",   1 }, { "项链",   1 }, { "neck",      1 },
    { "肩",     2 }, { "肩部",   2 }, { "护肩",   2 }, { "shoulders", 2 },
    { "衬衣",   3 }, { "衬衫",   3 }, { "body",   3 }, { "shirt",     3 },
    { "胸",     4 }, { "胸甲",   4 }, { "上衣",   4 }, { "chest",     4 },
    { "腰",     5 }, { "腰带",   5 }, { "waist",  5 }, { "belt",      5 },
    { "腿",     6 }, { "腿部",   6 }, { "护腿",   6 }, { "legs",      6 },
    { "脚",     7 }, { "靴子",   7 }, { "鞋",     7 }, { "feet",      7 },
    { "腕",     8 }, { "护腕",   8 }, { "wrists", 8 }, { "bracers",   8 },
    { "手",     9 }, { "手套",   9 }, { "hands",  9 }, { "gloves",    9 },
    { "戒指1", 10 }, { "戒指",  10 }, { "finger1", 10 },
    { "戒指2", 11 }, { "finger2", 11 },
    { "饰品1", 12 }, { "饰品",  12 }, { "trinket1", 12 },
    { "饰品2", 13 }, { "trinket2", 13 },
    { "背",    14 }, { "披风",  14 }, { "斗篷",  14 }, { "back",     14 }, { "cloak", 14 },
    { "主手",  15 }, { "武器",  15 }, { "mainhand", 15 }, { "mh",     15 },
    { "副手",  16 }, { "盾",    16 }, { "盾牌",  16 }, { "offhand",  16 }, { "oh",    16 },
    { "远程",  17 }, { "弓",    17 }, { "枪",    17 }, { "ranged",   17 },
    { "战袍",  18 }, { "徽章",  18 }, { "tabard",  18 }
};

static inline bool IsInvisibleSlot(uint8 slot)
{ return slot == 1 || slot == 10 || slot == 11 || slot == 12 || slot == 13; }

static std::string ToLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

static uint8 SlotFromName(std::string const& name)
{
    if (!name.empty() && name.find_first_not_of("0123456789") == std::string::npos)
    {
        uint32 n = uint32(atoi(name.c_str()));
        if (n < TRANSMOG_MAX_SLOT) return uint8(n);
        return TRANSMOG_MAX_SLOT;
    }
    std::string lower = ToLower(name);
    for (auto const& a : g_slotAlias)
        if (lower == ToLower(a.alias)) return a.slot;
    return TRANSMOG_MAX_SLOT;
}

// ============ 缓存逻辑（复制自 SetFakeEntry / RemoveSlot / SaveSet）============
typedef std::array<uint32, TRANSMOG_MAX_SLOT> SlotArray;
static std::unordered_map<uint32, SlotArray> _data;
static std::unordered_map<uint32, std::unordered_map<std::string, SlotArray>> _sets;
static uint32 _maxSets = 10;

static uint32 GetFakeEntry(uint32 g, uint8 slot)
{
    if (slot >= TRANSMOG_MAX_SLOT) return 0;
    auto itr = _data.find(g);
    if (itr == _data.end()) return 0;
    return itr->second[slot];
}
static void RemoveSlot(uint32 g, uint8 slot)
{
    if (slot >= TRANSMOG_MAX_SLOT) return;
    auto itr = _data.find(g);
    if (itr != _data.end())
    {
        itr->second[slot] = 0;
        bool anyLeft = false;
        for (uint32 e : itr->second) if (e) { anyLeft = true; break; }
        if (!anyLeft) _data.erase(itr);
    }
}
static void SetFakeEntry(uint32 g, uint8 slot, uint32 fake)
{
    if (slot >= TRANSMOG_MAX_SLOT) return;
    if (!fake) { RemoveSlot(g, slot); return; }
    auto itr = _data.find(g);
    if (itr == _data.end())
    { SlotArray a{}; a.fill(0); a[slot] = fake; _data[g] = a; }
    else itr->second[slot] = fake;
}
static void ClearAll(uint32 g) { _data.erase(g); }
static bool SaveSet(uint32 g, std::string const& name)
{
    if (name.empty()) return false;
    auto d = _data.find(g);
    if (d == _data.end()) return false;
    auto& sm = _sets[g];
    if (sm.find(name) == sm.end() && sm.size() >= _maxSets) return false;
    sm[name] = d->second;
    return true;
}
static bool LoadSet(uint32 g, std::string const& name)
{
    auto s = _sets.find(g);
    if (s == _sets.end()) return false;
    auto n = s->second.find(name);
    if (n == s->second.end()) return false;
    SlotArray arr = n->second;
    ClearAll(g);
    bool any = false;
    for (uint8 i = 0; i < TRANSMOG_MAX_SLOT; ++i)
        if (arr[i]) { SetFakeEntry(g, i, arr[i]); any = true; }
    return any;
}

// ============ 测试 ============
static int pass = 0, fail = 0;
static void CK(bool c, char const* msg)
{
    if (c) { ++pass; printf("  [OK] %s\n", msg); }
    else   { ++fail; printf("  [!!] %s   <<<< 失败\n", msg); }
}

int main()
{
    printf("=== 1. 槽位别名解析 ===\n");
    CK(SlotFromName("头") == 0,        "头 -> 0");
    CK(SlotFromName("头盔") == 0,      "头盔 -> 0");
    CK(SlotFromName("head") == 0,      "head -> 0");
    CK(SlotFromName("HEAD") == 0,      "HEAD 大写 -> 0");
    CK(SlotFromName("胸甲") == 4,      "胸甲 -> 4");
    CK(SlotFromName("披风") == 14,     "披风 -> 14");
    CK(SlotFromName("斗篷") == 14,     "斗篷 -> 14");
    CK(SlotFromName("主手") == 15,     "主手 -> 15");
    CK(SlotFromName("武器") == 15,     "武器 -> 15");
    CK(SlotFromName("MH") == 15,       "MH 大写 -> 15");
    CK(SlotFromName("盾") == 16,       "盾 -> 16");
    CK(SlotFromName("战袍") == 18,     "战袍 -> 18");
    CK(SlotFromName("0") == 0,         "数字 0 -> 0");
    CK(SlotFromName("18") == 18,       "数字 18 -> 18");
    CK(SlotFromName("19") == TRANSMOG_MAX_SLOT, "数字 19 越界 -> 拒绝");
    CK(SlotFromName("999") == TRANSMOG_MAX_SLOT,"数字 999 越界 -> 拒绝");
    CK(SlotFromName("不存在") == TRANSMOG_MAX_SLOT, "无效名 -> 拒绝");
    CK(SlotFromName("") == TRANSMOG_MAX_SLOT,   "空串 -> 拒绝");

    printf("\n=== 2. 不可见槽位识别 ===\n");
    CK(IsInvisibleSlot(1),   "颈部 不可见");
    CK(IsInvisibleSlot(10),  "戒指1 不可见");
    CK(IsInvisibleSlot(11),  "戒指2 不可见");
    CK(IsInvisibleSlot(12),  "饰品1 不可见");
    CK(IsInvisibleSlot(13),  "饰品2 不可见");
    CK(!IsInvisibleSlot(0),  "头部 可见");
    CK(!IsInvisibleSlot(4),  "胸甲 可见");
    CK(!IsInvisibleSlot(15), "主手 可见");
    CK(!IsInvisibleSlot(18), "战袍 可见");

    printf("\n=== 3. 别名表无重复冲突 ===\n");
    {
        bool dup = false;
        size_t n = sizeof(g_slotAlias)/sizeof(g_slotAlias[0]);
        for (size_t i = 0; i < n; ++i)
            for (size_t j = i+1; j < n; ++j)
                if (ToLower(g_slotAlias[i].alias) == ToLower(g_slotAlias[j].alias)
                    && g_slotAlias[i].slot != g_slotAlias[j].slot)
                { printf("     冲突: %s -> %u / %u\n", g_slotAlias[i].alias,
                         g_slotAlias[i].slot, g_slotAlias[j].slot); dup = true; }
        CK(!dup, "别名无一词两义");
        printf("     别名总数 %zu 条\n", n);
    }

    printf("\n=== 4. 槽位名表完整 ===\n");
    {
        bool ok = true;
        for (uint8 i = 0; i < TRANSMOG_MAX_SLOT; ++i)
            if (!g_slotNames[i] || !g_slotNames[i][0]) ok = false;
        CK(ok, "19 个槽位名全部非空");
    }

    printf("\n=== 5. 缓存增删查 ===\n");
    _data.clear();
    CK(GetFakeEntry(100, 0) == 0,       "空缓存查询 -> 0");
    SetFakeEntry(100, 0, 12640);
    CK(GetFakeEntry(100, 0) == 12640,   "设置头部 12640");
    CK(GetFakeEntry(100, 4) == 0,       "未设置的胸甲 -> 0");
    CK(GetFakeEntry(101, 0) == 0,       "别的角色 -> 0");
    SetFakeEntry(100, 4, 12639);
    SetFakeEntry(100, 15, 17182);
    CK(GetFakeEntry(100, 4) == 12639,   "设置胸甲");
    CK(GetFakeEntry(100, 15) == 17182,  "设置主手");
    CK(_data.size() == 1,               "只占 1 条记录");

    printf("\n=== 6. 移除与自动回收 ===\n");
    RemoveSlot(100, 4);
    CK(GetFakeEntry(100, 4) == 0,       "移除胸甲后 -> 0");
    CK(GetFakeEntry(100, 0) == 12640,   "头部不受影响");
    CK(_data.count(100) == 1,           "还有幻化 -> 记录保留");
    RemoveSlot(100, 0);
    RemoveSlot(100, 15);
    CK(_data.count(100) == 0,           "全部移除后 -> 记录自动回收（省内存）");

    printf("\n=== 7. fakeEntry=0 等同移除 ===\n");
    SetFakeEntry(200, 2, 999);
    CK(GetFakeEntry(200, 2) == 999,     "设置肩部");
    SetFakeEntry(200, 2, 0);
    CK(GetFakeEntry(200, 2) == 0,       "设为 0 -> 移除");
    CK(_data.count(200) == 0,           "记录回收");

    printf("\n=== 8. 越界保护 ===\n");
    SetFakeEntry(300, 19, 555);
    CK(_data.count(300) == 0,           "槽位 19 越界 -> 不写入");
    SetFakeEntry(300, 255, 555);
    CK(_data.count(300) == 0,           "槽位 255 越界 -> 不写入");
    CK(GetFakeEntry(300, 19) == 0,      "越界查询 -> 0");
    CK(GetFakeEntry(300, 255) == 0,     "槽位 255 查询 -> 0");

    printf("\n=== 9. 外观方案保存/载入 ===\n");
    _data.clear(); _sets.clear();
    CK(!SaveSet(400, "战斗套"),         "无幻化时保存 -> 失败");
    SetFakeEntry(400, 0, 1111);
    SetFakeEntry(400, 4, 2222);
    CK(SaveSet(400, "战斗套"),          "保存 战斗套");
    SetFakeEntry(400, 0, 3333);
    SetFakeEntry(400, 4, 4444);
    CK(SaveSet(400, "震金套"),          "保存 震金套");
    CK(GetFakeEntry(400,0) == 3333,     "当前是震金套");
    CK(LoadSet(400, "战斗套"),          "载入 战斗套");
    CK(GetFakeEntry(400,0) == 1111,     "头部还原 1111");
    CK(GetFakeEntry(400,4) == 2222,     "胸甲还原 2222");
    CK(!LoadSet(400, "不存在的套"),     "载入不存在方案 -> 失败");
    CK(!SaveSet(400, ""),               "空方案名 -> 拒绝");

    printf("\n=== 10. 方案数量上限 ===\n");
    _data.clear(); _sets.clear();
    SetFakeEntry(500, 0, 777);
    for (int i = 0; i < 10; ++i)
        SaveSet(500, "set" + std::to_string(i));
    CK(_sets[500].size() == 10,         "存满 10 套");
    CK(!SaveSet(500, "set11"),          "第 11 套 -> 拒绝");
    CK(SaveSet(500, "set0"),            "覆盖已有方案 -> 允许（不受上限约束）");
    CK(_sets[500].size() == 10,         "覆盖后仍是 10 套");

    printf("\n=== 11. 载入方案会清掉旧槽位（不残留）===\n");
    _data.clear(); _sets.clear();
    SetFakeEntry(600, 0, 100);
    SetFakeEntry(600, 4, 200);
    SaveSet(600, "两件套");
    SetFakeEntry(600, 15, 300);   // 额外加一件武器
    CK(GetFakeEntry(600,15) == 300,     "当前有武器幻化");
    LoadSet(600, "两件套");
    CK(GetFakeEntry(600,0) == 100,      "头部正确");
    CK(GetFakeEntry(600,4) == 200,      "胸甲正确");
    CK(GetFakeEntry(600,15) == 0,       "武器被清掉（方案里没有）<-关键");

    printf("\n============================\n");
    printf(" 通过 %d / 失败 %d\n", pass, fail);
    printf("============================\n");
    return fail ? 1 : 0;
}
