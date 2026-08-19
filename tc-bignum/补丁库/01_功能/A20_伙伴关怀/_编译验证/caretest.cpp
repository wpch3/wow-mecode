// ============================================================================
//  step34 伙伴关怀系统 逻辑单元测试
//
//  抽出 bot_companion.cpp 中不依赖 TrinityCore 的纯逻辑：
//      PickText 权重随机 / PickItem 等级匹配 / 虚拟背包增删查 /
//      FindInInventory 的等级+类型双重匹配 / 占位符替换
//
//  重点验证「真的从背包里拿」这条核心保证：
//      给出去了包里就少，包空了就给不出，给失败要还回去
//
//  编译： g++ -std=c++17 -O0 -Wall -Wextra -o caretest caretest.cpp
// ============================================================================

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>

typedef unsigned int  uint32;
typedef unsigned char uint8;

// ---------------------------------------------------------------------------
//  与 bot_companion.h 一致的定义
// ---------------------------------------------------------------------------
enum CompanionCareType : uint8
{
    CARE_TYPE_NONE    = 0,
    CARE_TYPE_FOOD    = 1,
    CARE_TYPE_DRINK   = 2,
    CARE_TYPE_MONEY   = 3,
    CARE_TYPE_CHAT    = 4,
    CARE_TYPE_LEVELUP = 5,
    CARE_TYPE_REVIVE  = 6,
    CARE_TYPE_MAX     = 7
};

struct CompanionText
{
    uint32 Id; uint8 CareType; uint8 BotClass;
    std::string Text; uint32 Emote; uint8 Weight;
};
struct CompanionItem
{
    uint32 Id; uint8 CareType; uint32 ItemId;
    uint8 MinLevel; uint8 MaxLevel; std::string SourceText;
};
struct CompanionInvEntry
{
    uint32 ItemId; uint32 Count; std::string AcquiredFrom;
};

// 可控的伪随机，让测试可复现
static uint32 g_randSeq = 0;
static uint32 g_randForce = 0xFFFFFFFF;
static uint32 urand(uint32 mn, uint32 mx)
{
    if (g_randForce != 0xFFFFFFFF)
        return mn + (g_randForce % (mx - mn + 1));
    g_randSeq = g_randSeq * 1103515245 + 12345;
    return mn + ((g_randSeq >> 16) % (mx - mn + 1));
}

// ---------------------------------------------------------------------------
//  被测：管理器（与 bot_companion.cpp 同构，去掉 DB 调用）
// ---------------------------------------------------------------------------
class Mgr
{
public:
    std::unordered_map<uint8, std::vector<CompanionText>> _texts;
    std::unordered_map<uint8, std::vector<CompanionItem>> _items;
    std::unordered_map<uint32, std::vector<CompanionInvEntry>> _inventories;

    std::string PickText(uint8 careType, uint8 botClass) const
    {
        auto itr = _texts.find(careType);
        if (itr == _texts.end() || itr->second.empty()) return std::string();

        std::vector<CompanionText const*> pool;
        for (auto const& t : itr->second) if (t.BotClass == botClass) pool.push_back(&t);
        if (pool.empty())
            for (auto const& t : itr->second) if (t.BotClass == 0) pool.push_back(&t);
        if (pool.empty()) return std::string();

        uint32 total = 0;
        for (auto t : pool) total += t->Weight;
        if (!total) return pool[0]->Text;

        uint32 roll = urand(0, total - 1);
        for (auto t : pool)
        {
            if (roll < t->Weight) return t->Text;
            roll -= t->Weight;
        }
        return pool.back()->Text;
    }

    CompanionItem const* PickItem(uint8 careType, uint8 level) const
    {
        auto itr = _items.find(careType);
        if (itr == _items.end() || itr->second.empty()) return nullptr;

        CompanionItem const* best = nullptr;
        for (auto const& it : itr->second)
        {
            if (level < it.MinLevel || level > it.MaxLevel) continue;
            if (!best || it.MinLevel > best->MinLevel) best = &it;
        }
        return best;
    }

    void AddToInventory(uint32 g, uint32 id, uint32 c, std::string const& from)
    {
        if (!c || !id) return;
        auto& inv = _inventories[g];
        for (auto& e : inv)
            if (e.ItemId == id) { e.Count += c; return; }
        inv.push_back({id, c, from});
    }

    bool TakeFromInventory(uint32 g, uint32 id, uint32 c)
    {
        auto itr = _inventories.find(g);
        if (itr == _inventories.end()) return false;
        auto& inv = itr->second;
        for (auto e = inv.begin(); e != inv.end(); ++e)
        {
            if (e->ItemId != id) continue;
            if (e->Count < c) return false;
            e->Count -= c;
            if (e->Count == 0) inv.erase(e);
            return true;
        }
        return false;
    }

    bool HasInInventory(uint32 g, uint32 id, uint32 c = 1) const
    {
        auto itr = _inventories.find(g);
        if (itr == _inventories.end()) return false;
        for (auto const& e : itr->second)
            if (e.ItemId == id && e.Count >= c) return true;
        return false;
    }

    uint32 FindInInventory(uint32 g, uint8 careType, uint8 level, std::string& outFrom) const
    {
        auto invItr = _inventories.find(g);
        if (invItr == _inventories.end()) return 0;
        auto poolItr = _items.find(careType);
        if (poolItr == _items.end()) return 0;

        uint32 bestItem = 0; uint8 bestLvl = 0;
        for (auto const& e : invItr->second)
        {
            if (!e.Count) continue;
            for (auto const& it : poolItr->second)
            {
                if (it.ItemId != e.ItemId) continue;
                if (level < it.MinLevel || level > it.MaxLevel) continue;
                if (it.MinLevel >= bestLvl)
                {
                    bestLvl = it.MinLevel; bestItem = e.ItemId; outFrom = e.AcquiredFrom;
                }
            }
        }
        return bestItem;
    }

    void RestockBot(uint32 g, uint8 level)
    {
        static uint32 const RESTOCK = 5;
        for (uint8 type : { uint8(CARE_TYPE_FOOD), uint8(CARE_TYPE_DRINK) })
        {
            CompanionItem const* it = PickItem(type, level);
            if (!it) continue;
            if (HasInInventory(g, it->ItemId, 1)) continue;
            AddToInventory(g, it->ItemId, RESTOCK, it->SourceText);
        }
    }
};

// 被测：占位符替换（与 bot_ai_companion_patch 中一致）
static std::string Subst(std::string txt, char const* ph, std::string const& val)
{
    size_t pos = txt.find(ph);
    if (pos != std::string::npos) txt.replace(pos, strlen(ph), val);
    return txt;
}

// ---------------------------------------------------------------------------
//  测试框架
// ---------------------------------------------------------------------------
static int g_pass = 0, g_fail = 0;
static void ck(bool c, char const* what)
{ if (c) ++g_pass; else { ++g_fail; printf("  [FAIL] %s\n", what); } }
static void cku(uint32 got, uint32 want, char const* what)
{ if (got==want) ++g_pass; else { ++g_fail; printf("  [FAIL] %s : got %u want %u\n", what, got, want); } }
static void ckstr(std::string const& got, char const* want, char const* what)
{ if (got==want) ++g_pass; else { ++g_fail; printf("  [FAIL] %s : got \"%s\" want \"%s\"\n", what, got.c_str(), want); } }

static Mgr MakeMgr()
{
    Mgr m;
    // 台词
    m._texts[CARE_TYPE_FOOD] = {
        {1, CARE_TYPE_FOOD, 0, "A", 0, 10},
        {2, CARE_TYPE_FOOD, 0, "B", 0, 10},
        {3, CARE_TYPE_FOOD, 5, "MageOnly", 0, 10},   // 职业专属
    };
    m._texts[CARE_TYPE_MONEY] = {
        {4, CARE_TYPE_MONEY, 0, "拿着{gold}金", 0, 10},
    };
    m._texts[CARE_TYPE_CHAT] = {
        {5, CARE_TYPE_CHAT, 0, "闲聊", 0, 10},
    };
    // 物品：三个等级段
    m._items[CARE_TYPE_FOOD] = {
        {1, CARE_TYPE_FOOD, 4540, 1,  15, "面包房"},
        {2, CARE_TYPE_FOOD, 4542, 20, 35, "旅店"},
        {3, CARE_TYPE_FOOD, 8950, 40, 60, "集市"},
    };
    m._items[CARE_TYPE_DRINK] = {
        {4, CARE_TYPE_DRINK, 159,  1,  15, "溪边"},
        {5, CARE_TYPE_DRINK, 1205, 20, 35, "商队"},
    };
    return m;
}

int main()
{
    printf("=== step34 伙伴关怀 逻辑测试 ===\n\n");
    Mgr m = MakeMgr();

    // ---------------- PickText ----------------
    printf("[PickText]\n");
    ck(!m.PickText(CARE_TYPE_FOOD, 0).empty(), "通用台词能取到");
    ckstr(m.PickText(CARE_TYPE_FOOD, 5), "MageOnly", "职业专属优先于通用");
    ck(m.PickText(CARE_TYPE_REVIVE, 0).empty(), "没有该类型返回空串");
    ck(m.PickText(99, 0).empty(), "非法类型返回空串");
    {
        // 职业7没有专属，应回落到通用
        std::string t = m.PickText(CARE_TYPE_FOOD, 7);
        ck(t == "A" || t == "B", "无专属时回落通用");
    }

    // ---------------- PickItem 等级匹配 ----------------
    printf("[PickItem]\n");
    ck(m.PickItem(CARE_TYPE_FOOD, 5)  != nullptr, "5级能拿到食物");
    cku(m.PickItem(CARE_TYPE_FOOD, 5)->ItemId,  4540, "5级拿低级面包");
    cku(m.PickItem(CARE_TYPE_FOOD, 25)->ItemId, 4542, "25级拿中级面包");
    cku(m.PickItem(CARE_TYPE_FOOD, 50)->ItemId, 8950, "50级拿高级面包");
    ck(m.PickItem(CARE_TYPE_FOOD, 17) == nullptr, "等级落在空档返回null");
    ck(m.PickItem(CARE_TYPE_FOOD, 80) == nullptr, "超出所有段返回null");
    ck(m.PickItem(CARE_TYPE_MONEY, 10) == nullptr, "没有物品池的类型返回null");
    ckstr(m.PickItem(CARE_TYPE_FOOD, 50)->SourceText, "集市", "来源描述正确");

    // ---------------- 虚拟背包基础 ----------------
    printf("[背包 基础]\n");
    uint32 const G = 1001;
    ck(!m.HasInInventory(G, 4540), "初始背包为空");
    { std::string tmpFrom; cku(m.FindInInventory(G, CARE_TYPE_FOOD, 5, tmpFrom), 0, "空包找不到东西"); }

    m.AddToInventory(G, 4540, 3, "面包房");
    ck(m.HasInInventory(G, 4540, 3), "加3个后有3个");
    ck(!m.HasInInventory(G, 4540, 4), "没有4个");

    m.AddToInventory(G, 4540, 2, "面包房");
    ck(m.HasInInventory(G, 4540, 5), "同物品叠加到5");

    // ---------------- 【核心】给出去包里就要少 ----------------
    printf("[背包 核心保证：真的从包里拿]\n");
    ck(m.TakeFromInventory(G, 4540, 1), "取出1个成功");
    ck(m.HasInInventory(G, 4540, 4), "取后剩4");
    ck(!m.HasInInventory(G, 4540, 5), "确实少了1");

    ck(!m.TakeFromInventory(G, 4540, 99), "取超量失败");
    ck(m.HasInInventory(G, 4540, 4), "失败后数量不变");

    ck(m.TakeFromInventory(G, 4540, 4), "全部取完");
    ck(!m.HasInInventory(G, 4540, 1), "包空了");
    ck(!m.TakeFromInventory(G, 4540, 1), "空包取不出");

    ck(!m.TakeFromInventory(G, 99999, 1), "取不存在的物品失败");
    ck(!m.TakeFromInventory(9999, 4540, 1), "不存在的bot取不出");

    // ---------------- 给失败要还回去 ----------------
    printf("[背包 回滚]\n");
    m.AddToInventory(G, 4542, 2, "旅店");
    ck(m.TakeFromInventory(G, 4542, 1), "先扣掉");
    ck(m.HasInInventory(G, 4542, 1), "剩1");
    m.AddToInventory(G, 4542, 1, "旅店");        // 模拟给失败回滚
    ck(m.HasInInventory(G, 4542, 2), "回滚后恢复到2");

    // ---------------- FindInInventory 双重匹配 ----------------
    printf("[FindInInventory]\n");
    {
        Mgr m2 = MakeMgr();
        uint32 const B = 2002;
        std::string from;

        m2.AddToInventory(B, 4540, 5, "面包房");   // 1-15级食物
        m2.AddToInventory(B, 1205, 5, "商队");     // 20-35级水

        cku(m2.FindInInventory(B, CARE_TYPE_FOOD, 5, from), 4540, "5级找到低级面包");
        ckstr(from, "面包房", "来源正确带出");

        cku(m2.FindInInventory(B, CARE_TYPE_FOOD, 50, from), 0, "50级找不到（包里只有低级的）");
        cku(m2.FindInInventory(B, CARE_TYPE_DRINK, 5, from), 0, "5级找不到水（包里的是20级起）");
        cku(m2.FindInInventory(B, CARE_TYPE_DRINK, 25, from), 1205, "25级找到水");
        ckstr(from, "商队", "水的来源正确");
        cku(m2.FindInInventory(B, CARE_TYPE_MONEY, 25, from), 0, "钱不是物品，找不到");
    }

    // ---------------- 有物品但等级不符 ----------------
    printf("[等级隔离]\n");
    {
        Mgr m3 = MakeMgr();
        uint32 const B = 3003;
        std::string from;
        m3.AddToInventory(B, 8950, 5, "集市");     // 40-60级食物
        cku(m3.FindInInventory(B, CARE_TYPE_FOOD, 10, from), 0, "低级角色拿不出高级食物");
        cku(m3.FindInInventory(B, CARE_TYPE_FOOD, 45, from), 8950, "45级可以");
    }

    // ---------------- RestockBot ----------------
    printf("[RestockBot]\n");
    {
        Mgr m4 = MakeMgr();
        uint32 const B = 4004;
        m4.RestockBot(B, 25);
        ck(m4.HasInInventory(B, 4542, 5), "补货给了25级食物x5");
        ck(m4.HasInInventory(B, 1205, 5), "补货给了25级水x5");
        ck(!m4.HasInInventory(B, 4540, 1), "没给不匹配等级的");

        // 重复补货不叠加
        m4.RestockBot(B, 25);
        ck(m4.HasInInventory(B, 4542, 5) && !m4.HasInInventory(B, 4542, 6), "已有则不重复补");

        // 等级变了补新的
        m4.RestockBot(B, 50);
        ck(m4.HasInInventory(B, 8950, 5), "升级后补高级食物");
    }

    // ---------------- 占位符替换 ----------------
    printf("[占位符]\n");
    ckstr(Subst("拿着{gold}金", "{gold}", "5"), "拿着5金", "gold替换");
    ckstr(Subst("我包里还有{item}", "{item}", "面包"), "我包里还有面包", "item替换");
    ckstr(Subst("没有占位符", "{item}", "面包"), "没有占位符", "无占位符不变");
    ckstr(Subst("{item}和{item}", "{item}", "X"), "X和{item}", "只替换第一个");
    ckstr(Subst("", "{item}", "面包"), "", "空串安全");

    // ---------------- 多bot隔离 ----------------
    printf("[多bot隔离]\n");
    {
        Mgr m5 = MakeMgr();
        m5.AddToInventory(111, 4540, 3, "A");
        m5.AddToInventory(222, 4540, 7, "B");
        ck(m5.HasInInventory(111, 4540, 3) && !m5.HasInInventory(111, 4540, 4), "bot111有3个");
        ck(m5.HasInInventory(222, 4540, 7), "bot222有7个");
        m5.TakeFromInventory(111, 4540, 3);
        ck(!m5.HasInInventory(111, 4540, 1), "111取空");
        ck(m5.HasInInventory(222, 4540, 7), "222不受影响");
    }

    // ---------------- 边界 ----------------
    printf("[边界]\n");
    {
        Mgr m6 = MakeMgr();
        m6.AddToInventory(555, 4540, 0, "x");
        ck(!m6.HasInInventory(555, 4540, 1), "加0个不生效");
        m6.AddToInventory(555, 0, 5, "x");
        ck(!m6.HasInInventory(555, 0, 1), "itemId=0不生效");
    }

    printf("\n=== %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
