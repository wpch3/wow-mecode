// ============================================================================
//  step32 .findmodel 逻辑单元测试
//
//  抽出 cs_modelfind.cpp 中不依赖 TrinityCore 头文件的纯逻辑：
//      Tok / Lower / IsAllDigit / ContainsNoCase / ShortName
//    + 用一张【伪 DBC】验证搜索链路 displayid -> ModelID -> ModelName
//
//  伪 DBC 刻意做成和真实 DBC 一样"脏"：
//      - 索引表有大量空洞（LookupEntry 返回 nullptr）
//      - 有坏行（ModelID 指向 CreatureModelData 里不存在的 ID）
//      - 有多个 displayid 共用同一个 ModelID
//    这三种情况在真机上都会遇到，必须验证不崩、不漏、不重。
//
//  编译： g++ -std=c++17 -O0 -Wall -Wextra -o fmtest fmtest.cpp
// ============================================================================

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>

typedef unsigned int uint32;

// ---------------------------------------------------------------------------
//  被测逻辑（与 cs_modelfind.cpp 逐字一致）
// ---------------------------------------------------------------------------

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

static std::string Lower(std::string s)
{
    for (char& c : s)
        if (c >= 'A' && c <= 'Z')
            c = char(c - 'A' + 'a');
    return s;
}

static bool IsAllDigit(std::string const& s)
{
    if (s.empty()) return false;
    for (char c : s)
        if (c < '0' || c > '9') return false;
    return true;
}

static bool ContainsNoCase(char const* hay, std::string const& needleLower)
{
    if (!hay || !*hay) return false;
    if (needleLower.empty()) return true;
    std::string h = Lower(std::string(hay));
    return h.find(needleLower) != std::string::npos;
}

static std::string Squash(std::string const& s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s)
    {
        if (c >= 'A' && c <= 'Z')      out += char(c - 'A' + 'a');
        else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) out += c;
    }
    return out;
}

static std::string ShortName(char const* full)
{
    if (!full || !*full) return std::string();
    std::string s(full);
    size_t p = s.find_last_of("\\/");
    if (p != std::string::npos) s = s.substr(p + 1);
    p = s.find_last_of('.');
    if (p != std::string::npos) s = s.substr(0, p);
    return s;
}

// ---------------------------------------------------------------------------
//  伪 DBC —— 模拟 DBCStorage 的 LookupEntry / GetNumRows 语义
// ---------------------------------------------------------------------------

struct FakeModelData
{
    uint32      ID;
    char const* ModelName;
};

struct FakeDisplayInfo
{
    uint32 ID;
    uint32 ModelID;
    uint32 ExtendedDisplayInfoID;
    float  CreatureModelScale;
};

static FakeModelData const g_md[] = {
    {  30, "Creature\\MineSpider\\MineSpider.mdx"      },
    { 119, "Creature\\Wolf\\Wolf.mdx"                  },
    { 195, "Creature\\WorgWorg\\Worg.mdx"              },
    { 307, "Character\\Human\\Male\\HumanMale.mdx"     },
    { 308, "Character\\Human\\Female\\HumanFemale.mdx" },
    { 411, "Creature\\Bear\\Bear.mdx"                  },
    { 517, "Creature\\Murloc\\Murloc.mdx"              },
    {2775, "Creature\\Nerubian\\Nerubian.mdx"          },
    {2871, "Creature\\WolfDire\\DireWolf.mdx"          },
    {3167, "Creature\\LichKing\\LichKing.mdx"          },
    {3168, "Creature\\ArthasLichKing\\ArthasLichKing.mdx" },
    {4001, "Character\\NightElf\\Female\\NightElfFemale.mdx" },
};
static uint32 const g_mdCount = sizeof(g_md) / sizeof(g_md[0]);

static FakeModelData const* MDLookup(uint32 id)
{
    for (uint32 i = 0; i < g_mdCount; ++i)
        if (g_md[i].ID == id) return &g_md[i];
    return nullptr;
}

//  11 行，其中 16820 是坏行 -> 有效行 10
static FakeDisplayInfo const g_display[] = {
    {   31,   517,    0, 1.00f },  // Murloc
    {  247,   119,    0, 1.00f },  // Wolf
    {  295,   195,    0, 1.00f },  // Worg      注意：路径里没有 "wolf"
    {  388,   411,    0, 1.00f },  // Bear
    {  843,   119,    0, 0.80f },  // Wolf      与 247 共用 ModelID 119
    { 1478,   307, 1234, 1.00f },  // HumanMale     Extra 非0 = 玩家型
    { 1479,   308, 1235, 1.00f },  // HumanFemale
    { 9999,    30,    0, 1.00f },  // MineSpider
    {16820, 99999,    0, 1.00f },  // 坏行：ModelID 在 ModelData 里不存在
    {26232,  2775,    0, 1.00f },  // Nerubian
    {26233,  2871,    0, 1.20f },  // DireWolf
    {30721,  3167,    0, 1.00f },  // LichKing      <- 用户实测搜不到的那个
    {30722,  3168,    0, 1.00f },  // ArthasLichKing
    {31000,  4001, 5001, 1.00f },  // NightElfFemale
};
static uint32 const g_displayCount = sizeof(g_display) / sizeof(g_display[0]);

// 模拟 GetNumRows：索引表大小 = 最大ID+1（中间全是空洞）
static uint32 DIGetNumRows()
{
    uint32 mx = 0;
    for (uint32 i = 0; i < g_displayCount; ++i)
        if (g_display[i].ID > mx) mx = g_display[i].ID;
    return mx + 1;
}

static FakeDisplayInfo const* DILookup(uint32 id)
{
    for (uint32 i = 0; i < g_displayCount; ++i)
        if (g_display[i].ID == id) return &g_display[i];
    return nullptr;   // 空洞
}

struct Hit { uint32 displayId; uint32 modelId; std::string shortName; };

// 被测：搜索主循环（与 cs_modelfind.cpp 中的循环同构）
static std::vector<std::string> g_ignored;   // 一个都没命中的词

static std::vector<Hit> Search(std::string query, uint32 maxShow, uint32* total)
{
    g_ignored.clear();
    std::vector<std::string> needles;
    {
        std::vector<std::string> toks = Tok(query.c_str());
        for (std::string const& t : toks)
        {
            std::string sq = Squash(t);
            if (!sq.empty()) needles.push_back(sq);
        }
    }

    std::vector<Hit> hits;
    *total = 0;
    if (needles.empty()) return hits;

    // 第一遍：算每行命中几个词，同时记录每个词全表命中过没有
    std::vector<bool> everHit(needles.size(), false);
    uint32 best = 0;
    for (uint32 id = 0; id < DIGetNumRows(); ++id)
    {
        FakeDisplayInfo const* d = DILookup(id);
        if (!d) continue;
        FakeModelData const* m = MDLookup(d->ModelID);
        if (!m || !m->ModelName) continue;

        std::string hay = Squash(m->ModelName);
        uint32 n = 0;
        for (size_t i = 0; i < needles.size(); ++i)
            if (hay.find(needles[i]) != std::string::npos) { ++n; everHit[i] = true; }
        if (n > best) best = n;
    }
    if (best == 0) return hits;

    for (size_t i = 0; i < needles.size(); ++i)
        if (!everHit[i]) g_ignored.push_back(needles[i]);

    // 第二遍：只收命中数 == best 的行
    for (uint32 id = 0; id < DIGetNumRows(); ++id)
    {
        FakeDisplayInfo const* d = DILookup(id);
        if (!d) continue;
        FakeModelData const* m = MDLookup(d->ModelID);
        if (!m || !m->ModelName) continue;

        std::string hay = Squash(m->ModelName);
        uint32 n = 0;
        for (std::string const& nd : needles)
            if (hay.find(nd) != std::string::npos) ++n;
        if (n != best) continue;

        ++(*total);
        if (hits.size() < maxShow)
        {
            Hit h; h.displayId=d->ID; h.modelId=d->ModelID;
            h.shortName=ShortName(m->ModelName); hits.push_back(h);
        }
    }
    return hits;
}

// ---------------------------------------------------------------------------
//  测试框架
// ---------------------------------------------------------------------------
static int g_pass = 0, g_fail = 0;

static void ck(bool cond, char const* what)
{
    if (cond) ++g_pass;
    else { ++g_fail; printf("  [FAIL] %s\n", what); }
}

static void ckstr(std::string const& got, char const* want, char const* what)
{
    if (got == want) ++g_pass;
    else { ++g_fail; printf("  [FAIL] %s : got \"%s\" want \"%s\"\n", what, got.c_str(), want); }
}

static void cku(uint32 got, uint32 want, char const* what)
{
    if (got == want) ++g_pass;
    else { ++g_fail; printf("  [FAIL] %s : got %u want %u\n", what, got, want); }
}

int main()
{
    printf("=== step32 .findmodel 逻辑测试 ===\n\n");

    // ---------------- Tok ----------------
    printf("[Tok]\n");
    cku((uint32)Tok("").size(),            0, "空串");
    cku((uint32)Tok(nullptr).size(),       0, "nullptr");
    cku((uint32)Tok("wolf").size(),        1, "单词");
    cku((uint32)Tok("  wolf  ").size(),    1, "前后空格");
    cku((uint32)Tok("id 26232").size(),    2, "两段");
    cku((uint32)Tok("npc  24191").size(),  2, "多空格");
    cku((uint32)Tok("a\tb\tc").size(),     3, "制表符");
    ckstr(Tok("id 26232")[0], "id",        "第一段");
    ckstr(Tok("id 26232")[1], "26232",     "第二段");

    // ---------------- Lower ----------------
    printf("[Lower]\n");
    ckstr(Lower("WOLF"),   "wolf",   "全大写");
    ckstr(Lower("Wolf"),   "wolf",   "首字母大写");
    ckstr(Lower("wolf"),   "wolf",   "已小写");
    ckstr(Lower("W0LF_x"), "w0lf_x", "含数字下划线");
    ckstr(Lower(""),       "",       "空串");

    // ---------------- IsAllDigit ----------------
    printf("[IsAllDigit]\n");
    ck( IsAllDigit("26232"), "纯数字");
    ck( IsAllDigit("0"),     "零");
    ck(!IsAllDigit(""),      "空串非数字");
    ck(!IsAllDigit("26a32"), "含字母");
    ck(!IsAllDigit("-5"),    "负号");
    ck(!IsAllDigit("2.5"),   "小数点");

    // ---------------- ShortName ----------------
    printf("[ShortName]\n");
    ckstr(ShortName("Creature\\Wolf\\Wolf.mdx"),              "Wolf",      "反斜杠路径");
    ckstr(ShortName("Creature/Wolf/Wolf.m2"),                 "Wolf",      "正斜杠路径");
    ckstr(ShortName("Character\\Human\\Male\\HumanMale.mdx"), "HumanMale", "深层路径");
    ckstr(ShortName("Wolf.mdx"),                              "Wolf",      "无目录");
    ckstr(ShortName("Wolf"),                                  "Wolf",      "无扩展名");
    ckstr(ShortName(""),                                      "",          "空串");
    ckstr(ShortName(nullptr),                                 "",          "nullptr");

    // ---------------- ContainsNoCase ----------------
    printf("[ContainsNoCase]\n");
    ck( ContainsNoCase("Creature\\Wolf\\Wolf.mdx", "wolf"),        "小写命中");
    ck( ContainsNoCase("Creature\\Wolf\\Wolf.mdx", Lower("WOLF")), "大写转小写命中");
    ck( ContainsNoCase("Creature\\Wolf\\Wolf.mdx", "creature"),    "匹配目录名");
    ck( ContainsNoCase("Creature\\WolfDire\\DireWolf.mdx", "dire"),"匹配中段");
    ck(!ContainsNoCase("Creature\\Wolf\\Wolf.mdx", "bear"),        "不该命中");
    ck(!ContainsNoCase(nullptr, "wolf"),                           "nullptr 安全");
    ck(!ContainsNoCase("", "wolf"),                                "空串安全");
    ck( ContainsNoCase("anything", ""),                            "空关键字全命中");

    // ---------------- 搜索链路 ----------------
    printf("[Search 链路]\n");
    uint32 total = 0;
    std::vector<Hit> h;

    // wolf 应命中 247 / 843 / 26233(DireWolf)，不含 295(Worg)
    h = Search("wolf", 30, &total);
    cku(total, 3, "搜 wolf 命中3（Worg不算）");
    ck(h.size() == total, "未超上限时 hits == total");
    if (h.size() == 3)
    {
        cku(h[0].displayId,  247, "wolf 第1条 displayid（按ID升序）");
        cku(h[1].displayId,  843, "wolf 第2条");
        cku(h[2].displayId, 26233,"wolf 第3条");
        ckstr(h[2].shortName, "DireWolf", "DireWolf 短名");
    }

    total = 0;
    h = Search("worg", 30, &total);
    cku(total, 1, "搜 worg 只命中1");

    total = 0;
    h = Search("bear", 30, &total);
    cku(total, 1, "搜 bear 命中1");
    if (!h.empty())
    {
        ckstr(h[0].shortName, "Bear", "bear 短名");
        cku(h[0].displayId, 388, "bear displayid");
    }

    total = 0;
    h = Search("human", 30, &total);
    cku(total, 2, "搜 human 命中2（男女）");

    total = 0;
    h = Search("nerubian", 30, &total);
    cku(total, 1, "搜 nerubian 命中1");
    if (!h.empty())
    {
        cku(h[0].displayId, 26232, "nerubian displayid");
        cku(h[0].modelId,    2775, "nerubian modelid");
    }

    total = 0;
    h = Search("zzzznotexist", 30, &total);
    cku(total, 0, "搜不存在的返回0");
    ck(h.empty(), "无命中时列表空");

    // 坏行必须被跳过：全表 11 行，有效 10 行
    total = 0;
    h = Search("mdx", 30, &total);
    cku(total, 13, "搜 mdx 命中13（坏行16820被跳过）");

    // 上限截断
    total = 0;
    h = Search("mdx", 3, &total);
    cku((uint32)h.size(), 3,  "上限截断到3");
    cku(total,            13, "total 仍统计全部13");

    // 大小写无关
    uint32 tUpper = 0, tLower = 0;
    Search("WOLF", 30, &tUpper);
    Search("wolf", 30, &tLower);
    cku(tUpper, tLower, "大小写结果一致");

    // 同一 ModelID 多个 displayid 都要列出
    total = 0;
    h = Search("wolf", 30, &total);
    cku(total, 3, "wolf 命中3（Squash后反斜杠已去掉）");

    // 匹配目录名
    total = 0;
    h = Search("character\\", 30, &total);
    cku(total, 3, "按目录搜 character 命中3");

    // ---------------- v2: Squash ----------------
    printf("[Squash]\n");
    ckstr(Squash("The Lich King"), "thelichking", "去空格转小写");
    ckstr(Squash("LichKing"),      "lichking",    "驼峰");
    ckstr(Squash("lich_king"),     "lichking",    "下划线");
    ckstr(Squash("LICH-KING"),     "lichking",    "减号+大写");
    ckstr(Squash("Creature\\LichKing\\LichKing.mdx"),
          "creaturelichkinglichkingmdx", "整条路径");
    ckstr(Squash(""),              "",            "空串");
    ckstr(Squash("   "),           "",            "纯空格");

    // ---------------- v2: 用户实测的 bug ----------------
    printf("[用户实测: The Lich King]\n");
    uint32 t = 0;
    std::vector<Hit> r;

    r = Search("The Lich King", 30, &t);
    cku(t, 2, "搜 'The Lich King' 命中2（LichKing + ArthasLichKing）");

    r = Search("lichking", 30, &t);
    cku(t, 2, "搜 'lichking' 结果相同");

    r = Search("lich king", 30, &t);
    cku(t, 2, "搜 'lich king' 结果相同");

    r = Search("LICH-KING", 30, &t);
    cku(t, 2, "搜 'LICH-KING' 结果相同");

    r = Search("lich_king", 30, &t);
    cku(t, 2, "搜 'lich_king' 结果相同");

    r = Search("king lich", 30, &t);
    cku(t, 2, "词序颠倒也命中（AND不看顺序）");

    r = Search("arthas lich", 30, &t);
    cku(t, 1, "两词AND：只命中 ArthasLichKing");
    if (!r.empty()) cku(r[0].displayId, 30722, "arthas lich 的 displayid");

    r = Search("night elf female", 30, &t);
    cku(t, 1, "三个词 AND 命中 NightElfFemale");

    r = Search("the", 30, &t);
    cku(t, 0, "只搜 'the'：全表无此串，返回0（正确）");

    // 关键：'the' 混在有效词里时，必须被自动忽略而不是拖垮整个查询
    r = Search("The Lich King", 30, &t);
    ck(g_ignored.size() == 1 && g_ignored[0] == "the",
       "'the' 无命中被记入忽略列表");

    r = Search("lich dragon", 30, &t);
    cku(t, 2, "'dragon' 无命中被忽略，按 'lich' 返回2");
    ck(g_ignored.size() == 1 && g_ignored[0] == "dragon", "报告忽略 dragon");

    r = Search("wolf bear", 30, &t);
    cku(t, 4, "两词都有命中但无交集 -> best=1，wolf(3)+bear(1)=4");

    printf("\n=== %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
