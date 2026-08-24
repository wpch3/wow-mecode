// 验证「真同名判定 + 择优评分」方案
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>
typedef uint32_t uint32; typedef int32_t int32;

struct Item {
    uint32 id; std::string name;
    uint32 cls, sub, quality, invType, ilvl, reqLvl, armor;
    std::vector<std::pair<uint32,int32>> stats;   // (type,value)
    float dpsMin, dpsMax;
    std::string desc;
};

// —— 严格身份判定：只有「功能完全等价」才算同一件 ——
static bool SameIdentity(Item const& a, Item const& b)
{
    if (a.name != b.name)         return false;
    if (a.cls != b.cls)           return false;   // 武器/护甲
    if (a.sub != b.sub)           return false;   // 剑/斧/板甲
    if (a.invType != b.invType)   return false;   // ★主手/副手 关键
    if (a.quality != b.quality)   return false;   // 品质
    if (a.ilvl != b.ilvl)         return false;   // 装等
    if (a.armor != b.armor)       return false;
    if (a.desc != b.desc)         return false;   // 描述
    if (a.stats.size() != b.stats.size()) return false;
    for (size_t i = 0; i < a.stats.size(); ++i)
        if (a.stats[i] != b.stats[i]) return false;
    if (a.dpsMin != b.dpsMin || a.dpsMax != b.dpsMax) return false;
    return true;
}

// —— 评分：数值越高分越高 ——
static double Score(Item const& it)
{
    double s = 0;
    s += it.ilvl * 100.0;              // 装等权重最高
    s += it.quality * 500.0;           // 品质
    for (auto& st : it.stats) s += st.second;   // 属性总和
    s += it.armor;
    s += (it.dpsMin + it.dpsMax) / 2.0 * 10.0;
    return s;
}

static void Analyze(const char* input, std::vector<Item> hits)
{
    printf("\n输入「%s」-> 匹配 %zu 项\n", input, hits.size());
    if (hits.empty()) { printf("  [未找到]\n"); return; }
    if (hits.size() == 1) { printf("  [唯一] 直接给 ID:%u\n", hits[0].id); return; }

    bool allSame = true;
    for (size_t i = 1; i < hits.size(); ++i)
        if (!SameIdentity(hits[0], hits[i])) { allSame = false; break; }

    if (allSame) {
        printf("  [真·完全等价] %zu项功能属性全同 -> 直接给 ID:%u\n", hits.size(), hits[0].id);
        return;
    }
    // 不等价 -> 按分数排序列候选，标注推荐
    std::sort(hits.begin(), hits.end(),
        [](Item const& a, Item const& b){ return Score(a) > Score(b); });
    printf("  [有差异] 按推荐度排序列出：\n");
    for (size_t i = 0; i < hits.size(); ++i) {
        printf("    %s ID:%-7u %s  ilvl=%u 品质=%u 部位=%u 分数=%.0f\n",
            i==0 ? "★推荐" : "      ", hits[i].id, hits[i].name.c_str(),
            hits[i].ilvl, hits[i].quality, hits[i].invType, Score(hits[i]));
    }
}

int main()
{
    // 场景1：埃辛诺斯战刃 —— 同名但主手/副手不同
    Item wg1{32837,"埃辛诺斯战刃",2,7,5,21,156,70,0,{{4,50},{7,40}},214,323,"传说之刃"};
    Item wg2{32838,"埃辛诺斯战刃",2,7,5,22,156,70,0,{{4,50},{7,40}},214,323,"传说之刃"};
    Analyze("埃辛诺斯战刃", {wg1, wg2});

    // 场景2：真正的重复条目（数据库里常见的克隆物品）
    Item dup1{50000,"测试之剑",2,7,4,21,200,80,0,{{4,100}},300,400,"测试"};
    Item dup2{50001,"测试之剑",2,7,4,21,200,80,0,{{4,100}},300,400,"测试"};
    Analyze("测试之剑", {dup1, dup2});

    // 场景3：同名但装等不同（新旧版本）
    Item old1{60000,"符文之剑",2,7,3,21,100,60,0,{{4,20}},100,150,"旧版"};
    Item new1{60001,"符文之剑",2,7,5,21,264,80,0,{{4,500}},800,1200,"新版"};
    Analyze("符文之剑", {old1, new1});

    return 0;
}
