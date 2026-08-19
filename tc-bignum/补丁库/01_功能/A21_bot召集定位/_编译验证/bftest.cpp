// ============================================================================
//  step35 .bf 逻辑单元测试
//
//  抽出 cs_botfind.cpp 中不依赖 TrinityCore 的纯逻辑：
//      Tok / Lower / IsAllDigit / 参数解析分支 / 排序规则 / 过滤规则
//
//  编译： g++ -std=c++17 -O0 -Wall -Wextra -o bftest bftest.cpp
// ============================================================================

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>

typedef unsigned int uint32;
typedef unsigned char uint8;

// ---------------------------------------------------------------------------
//  被测逻辑（与 cs_botfind.cpp 逐字一致）
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
        if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
    return s;
}

static bool IsAllDigit(std::string const& s)
{
    if (s.empty()) return false;
    for (char c : s)
        if (c < '0' || c > '9') return false;
    return true;
}

// ---------------------------------------------------------------------------
//  伪 bot：只保留排序/过滤需要的字段
// ---------------------------------------------------------------------------
struct FakeBot
{
    uint32 Entry;
    uint32 MapId;
    float  Dist;      // 相对玩家的距离（跨图时无意义）
    uint32 Owner;     // 0 = 无主
    bool   Alive;
    bool   InWorld;
};

static bool IsFreeWanderer(FakeBot const& b)
{
    if (!b.InWorld || !b.Alive) return false;
    if (b.Owner != 0) return false;
    return true;
}

static std::vector<FakeBot> Collect(std::vector<FakeBot> const& all,
                                    uint32 myMap, bool sameMapOnly, float maxDist)
{
    std::vector<FakeBot> out;
    for (auto const& b : all)
    {
        if (!IsFreeWanderer(b)) continue;
        if (sameMapOnly && b.MapId != myMap) continue;
        if (maxDist > 0.0f)
        {
            if (b.MapId != myMap) continue;
            if (b.Dist > maxDist) continue;
        }
        out.push_back(b);
    }

    std::sort(out.begin(), out.end(), [myMap](FakeBot const& a, FakeBot const& b)
    {
        bool aSame = a.MapId == myMap;
        bool bSame = b.MapId == myMap;
        if (aSame != bSame) return aSame;
        if (!aSame) return a.Entry < b.Entry;
        return a.Dist < b.Dist;
    });
    return out;
}

// 参数解析结果
struct ParseResult
{
    enum Mode { HELP, LIST, COME, GOTO, BAD } mode;
    bool   sameMapOnly;
    float  maxDist;
    uint32 count;
    uint32 entry;
    bool   byEntry;
};

static ParseResult Parse(char const* args)
{
    ParseResult r{ ParseResult::LIST, true, 0.0f, 1, 0, false };
    std::vector<std::string> tok = Tok(args);
    if (tok.empty()) return r;

    std::string s0 = Lower(tok[0]);

    if (s0 == "help") { r.mode = ParseResult::HELP; return r; }

    if (s0 == "come")
    {
        r.mode = ParseResult::COME;
        if (tok.size() >= 3 && Lower(tok[1]) == "entry" && IsAllDigit(tok[2]))
        {
            r.byEntry = true;
            r.entry = uint32(atoi(tok[2].c_str()));
        }
        else if (tok.size() >= 2 && IsAllDigit(tok[1]))
        {
            uint32 v = uint32(atoi(tok[1].c_str()));
            r.count = std::max<uint32>(1, std::min<uint32>(10, v));
        }
        return r;
    }

    if (s0 == "goto")
    {
        r.mode = ParseResult::GOTO;
        if (tok.size() >= 3 && Lower(tok[1]) == "entry" && IsAllDigit(tok[2]))
        {
            r.byEntry = true;
            r.entry = uint32(atoi(tok[2].c_str()));
        }
        return r;
    }

    if (s0 == "near")
    {
        r.mode = ParseResult::LIST;
        r.maxDist = (tok.size() >= 2 && IsAllDigit(tok[1]))
                    ? float(atoi(tok[1].c_str())) : 200.0f;
        return r;
    }

    if (s0 == "all")
    {
        r.mode = ParseResult::LIST;
        r.sameMapOnly = false;
        return r;
    }

    r.mode = ParseResult::BAD;
    return r;
}

// ---------------------------------------------------------------------------
//  测试框架
// ---------------------------------------------------------------------------
static int g_pass = 0, g_fail = 0;
static void ck(bool c, char const* w)
{ if (c) ++g_pass; else { ++g_fail; printf("  [FAIL] %s\n", w); } }
static void cku(uint32 got, uint32 want, char const* w)
{ if (got==want) ++g_pass; else { ++g_fail; printf("  [FAIL] %s : got %u want %u\n", w, got, want); } }

int main()
{
    printf("=== step35 .bf 逻辑测试 ===\n\n");

    // ---------------- Tok / Lower / IsAllDigit ----------------
    printf("[基础工具]\n");
    cku((uint32)Tok("").size(), 0, "空串");
    cku((uint32)Tok(nullptr).size(), 0, "nullptr");
    cku((uint32)Tok("come 3").size(), 2, "两段");
    cku((uint32)Tok("come  entry   123").size(), 3, "多空格三段");
    ck(IsAllDigit("123"), "纯数字");
    ck(!IsAllDigit("12a"), "含字母");
    ck(!IsAllDigit(""), "空串非数字");

    // ---------------- 参数解析 ----------------
    printf("[参数解析]\n");
    {
        ParseResult r = Parse("");
        ck(r.mode == ParseResult::LIST && r.sameMapOnly && r.maxDist == 0.0f, "空参=列本图");

        r = Parse("help");
        ck(r.mode == ParseResult::HELP, "help");

        r = Parse("all");
        ck(r.mode == ParseResult::LIST && !r.sameMapOnly, "all=全服");

        r = Parse("near");
        ck(r.mode == ParseResult::LIST && r.maxDist == 200.0f, "near默认200码");

        r = Parse("near 50");
        ck(r.mode == ParseResult::LIST && r.maxDist == 50.0f, "near 50");

        r = Parse("come");
        ck(r.mode == ParseResult::COME && r.count == 1 && !r.byEntry, "come默认1个");

        r = Parse("come 5");
        ck(r.mode == ParseResult::COME && r.count == 5, "come 5");

        r = Parse("come 99");
        cku(r.count, 10, "come上限10");

        r = Parse("come 0");
        cku(r.count, 1, "come下限1");

        r = Parse("come entry 70000");
        ck(r.mode == ParseResult::COME && r.byEntry && r.entry == 70000, "come entry");

        r = Parse("goto");
        ck(r.mode == ParseResult::GOTO && !r.byEntry, "goto最近");

        r = Parse("goto entry 70001");
        ck(r.mode == ParseResult::GOTO && r.byEntry && r.entry == 70001, "goto entry");

        r = Parse("COME 3");
        ck(r.mode == ParseResult::COME && r.count == 3, "大小写无关");

        r = Parse("zzz");
        ck(r.mode == ParseResult::BAD, "无法识别的参数");

        // entry 后面不是数字，应退化成"按数量"解析，count保持默认
        r = Parse("come entry abc");
        ck(r.mode == ParseResult::COME && !r.byEntry, "entry后非数字不当entry处理");
    }

    // ---------------- 过滤规则 ----------------
    printf("[过滤]\n");
    std::vector<FakeBot> all = {
        { 70001, 0,  10.0f, 0, true,  true  },   // 本图 近 无主
        { 70002, 0,  50.0f, 0, true,  true  },   // 本图 中 无主
        { 70003, 0, 300.0f, 0, true,  true  },   // 本图 远 无主
        { 70004, 1,   0.0f, 0, true,  true  },   // 别的图 无主
        { 70005, 0,  20.0f, 5, true,  true  },   // 本图 【有主】
        { 70006, 0,  30.0f, 0, false, true  },   // 本图 【死了】
        { 70007, 0,  40.0f, 0, true,  false },   // 本图 【不在世界】
    };

    {
        auto r = Collect(all, 0, true, 0.0f);
        cku((uint32)r.size(), 3, "本图无主活着的=3个");
        ck(r[0].Entry == 70001 && r[1].Entry == 70002 && r[2].Entry == 70003, "按距离排序");
    }
    {
        auto r = Collect(all, 0, false, 0.0f);
        cku((uint32)r.size(), 4, "全服=4个（含跨图）");
        ck(r[3].Entry == 70004, "跨图的排最后");
    }
    {
        auto r = Collect(all, 0, true, 100.0f);
        cku((uint32)r.size(), 2, "100码内=2个");
    }
    {
        auto r = Collect(all, 0, true, 5.0f);
        cku((uint32)r.size(), 0, "5码内=0个");
    }

    // 有主的绝对不能出现
    {
        auto r = Collect(all, 0, false, 0.0f);
        bool hasOwned = false;
        for (auto const& b : r) if (b.Owner != 0) hasOwned = true;
        ck(!hasOwned, "有主的bot被排除");
    }
    // 死的/不在世界的不能出现
    {
        auto r = Collect(all, 0, false, 0.0f);
        bool bad = false;
        for (auto const& b : r) if (!b.Alive || !b.InWorld) bad = true;
        ck(!bad, "死的和不在世界的被排除");
    }

    // 空表安全
    {
        std::vector<FakeBot> none;
        auto r = Collect(none, 0, true, 0.0f);
        cku((uint32)r.size(), 0, "空列表安全");
    }
    // 全是有主的
    {
        std::vector<FakeBot> owned = {
            { 1, 0, 1.0f, 9, true, true },
            { 2, 0, 2.0f, 9, true, true },
        };
        auto r = Collect(owned, 0, true, 0.0f);
        cku((uint32)r.size(), 0, "全有主时返回0");
    }

    printf("\n=== %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
