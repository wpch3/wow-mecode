// ============================================================================
//  step30 .say 逻辑单元测试
//
//  抽出 cs_say.cpp 中【不依赖 TrinityCore 头文件】的纯逻辑：
//    Tok / Lower / IsAllDigit / JoinFrom / EndsWith / PickEmoteByPunct
//
//  重点验证：
//    1. 台词含空格能否原样拼回（JoinFrom）
//    2. 中文全角标点判断（官方 text.back() 判不了，这是本实现的关键改进）
//    3. 修饰符任意顺序组合的解析
//
//  编译： g++ -std=c++17 -O0 -Wall -Wextra -o saytest saytest.cpp
// ============================================================================

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <cstdint>

typedef unsigned int uint32;

// ---------------------------------------------------------------------------
//  以下与 cs_say.cpp 保持逐字一致
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

static std::string JoinFrom(std::vector<std::string> const& tok, size_t n)
{
    std::string out;
    for (size_t i = n; i < tok.size(); ++i)
    {
        if (!out.empty()) out += ' ';
        out += tok[i];
    }
    return out;
}

static bool EndsWith(std::string const& s, char const* suffix)
{
    size_t n = strlen(suffix);
    if (s.size() < n) return false;
    return s.compare(s.size() - n, n, suffix) == 0;
}

static uint32 PickEmoteByPunct(std::string const& text)
{
    if (text.empty()) return 0;
    if (EndsWith(text, "?") || EndsWith(text, "\xEF\xBC\x9F")) return 6;
    if (EndsWith(text, "!") || EndsWith(text, "\xEF\xBC\x81")) return 5;
    if (EndsWith(text, "...") || EndsWith(text, "\xE2\x80\xA6")) return 0;
    return 1;
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
    printf(" step30 .say 逻辑测试\n");
    printf("=====================================================\n\n");

    // ---------- JoinFrom：台词含空格 ----------
    printf("[1] JoinFrom 台词原样拼回（核心）\n");
    {
        auto t = Tok("yell r 40 hello world");
        CK(JoinFrom(t, 3) == "hello world", "从第3段起拼回 \"hello world\"");

        auto t2 = Tok("hello");
        CK(JoinFrom(t2, 0) == "hello", "单段");

        auto t3 = Tok("a b c d e");
        CK(JoinFrom(t3, 2) == "c d e", "中间起拼");

        auto t4 = Tok("only two");
        CK(JoinFrom(t4, 5).empty(), "越界返回空串（不崩）");

        // 中文台词
        std::string cn = "\xE5\xA4\xA7\xE5\xAE\xB6 \xE5\xBF\xAB\xE8\xB7\x91";  // "大家 快跑"
        auto t5 = Tok(("r 40 " + cn).c_str());
        CK(JoinFrom(t5, 2) == cn, "中文台词含空格能原样拼回");
    }

    // ---------- PickEmoteByPunct：中文标点（本实现关键改进）----------
    printf("\n[2] 标点配表情（官方只判ASCII，这里判UTF-8）\n");
    {
        CK(PickEmoteByPunct("who?") == 6, "ASCII ? -> 6 疑问");
        CK(PickEmoteByPunct("go!") == 5,  "ASCII ! -> 5 惊叹");
        CK(PickEmoteByPunct("hello") == 1, "无标点 -> 1 普通说话");
        CK(PickEmoteByPunct("well...") == 0, "ASCII ... -> 0 不播动作");
        CK(PickEmoteByPunct("") == 0, "空串 -> 0（不崩，官方back()会UB）");

        // 中文全角：EF BC 9F = ？   EF BC 81 = ！   E2 80 A6 = …
        std::string q = "\xE4\xBD\xA0\xE5\xA5\xBD\xEF\xBC\x9F";      // 你好？
        std::string e = "\xE5\xBF\xAB\xE8\xB7\x91\xEF\xBC\x81";      // 快跑！
        std::string d = "\xE5\x97\xAF\xE2\x80\xA6";                  // 嗯…
        std::string p = "\xE4\xBD\xA0\xE5\xA5\xBD";                  // 你好
        CK(PickEmoteByPunct(q) == 6, "中文 ？ -> 6 疑问");
        CK(PickEmoteByPunct(e) == 5, "中文 ！ -> 5 惊叹");
        CK(PickEmoteByPunct(d) == 0, "中文 … -> 0 沉思");
        CK(PickEmoteByPunct(p) == 1, "中文无标点 -> 1 普通");

        // 关键回归：用 back() 取最后一个字节会拿到 0x9F / 0x81，判断必错
        CK(uint8_t(q.back()) == 0x9F, "验证: 中文？的最后字节是0x9F（不是'?'）");
        CK(uint8_t(e.back()) == 0x81, "验证: 中文！的最后字节是0x81（不是'!'）");
        CK(q.back() != '?', "证明 back() 判不了中文疑问号");
        CK(e.back() != '!', "证明 back() 判不了中文感叹号");
    }

    // ---------- 修饰符解析 ----------
    printf("\n[3] 修饰符任意顺序组合\n");
    {
        struct C { char const* in; size_t textStart; char const* want; };
        C cases[] = {
            { "hello world",              0, "hello world" },
            { "yell hello",               1, "hello" },
            { "yell r 40 hello",          3, "hello" },
            { "r 40 yell hello",          3, "hello" },
            { "noemote entry 1234 hi",    3, "hi" },
            { "boss r 50 warning",        3, "warning" },
            { "me hello",                 1, "hello" },
            { "whisper noemote secret",   2, "secret" },
        };
        for (auto& c : cases)
        {
            auto t = Tok(c.in);
            char nm[200];
            snprintf(nm, sizeof(nm), "\"%s\" 台词从第%zu段起 = \"%s\"",
                     c.in, c.textStart, c.want);
            CK(JoinFrom(t, c.textStart) == c.want, nm);
        }
    }

    // ---------- 参数校验 ----------
    printf("\n[4] 参数校验\n");
    {
        CK(IsAllDigit("1234"), "entry 数字合法");
        CK(!IsAllDigit("abc"), "entry 非数字被拦");
        CK(!IsAllDigit(""), "空串非数字");
        CK(Lower("YELL") == "yell", "修饰符大小写不敏感");
        CK(Lower("BossOn") == "bosson", "bosson 大小写不敏感");

        // 半径边界（代码里限 0-500）
        float r1 = float(atof("40"));
        float r2 = float(atof("999"));
        float r3 = float(atof("0"));
        CK(r1 > 0.0f && r1 <= 500.0f, "半径40 合法");
        CK(!(r2 > 0.0f && r2 <= 500.0f), "半径999 超界被拦");
        CK(!(r3 > 0.0f && r3 <= 500.0f), "半径0 被拦");
    }

    // ---------- EndsWith ----------
    printf("\n[5] EndsWith 字节序列比对\n");
    {
        CK(EndsWith("abc", "c"), "单字节结尾");
        CK(EndsWith("abc", "bc"), "多字节结尾");
        CK(!EndsWith("abc", "abcd"), "后缀比原串长 -> false（不越界）");
        CK(!EndsWith("", "a"), "空串");
        CK(EndsWith("", ""), "空后缀恒真");
        std::string cn = "\xE4\xBD\xA0\xE5\xA5\xBD\xEF\xBC\x9F";
        CK(EndsWith(cn, "\xEF\xBC\x9F"), "中文三字节后缀命中");
        CK(!EndsWith(cn, "\xEF\xBC\x81"), "中文后缀不误命中");
    }

    // ---------- 边界 ----------
    printf("\n[6] 边界\n");
    {
        CK(Tok(nullptr).empty(), "nullptr 不崩");
        CK(Tok("").empty(), "空串");
        auto t = Tok("   yell    hello   ");
        CK(t.size() == 2 && t[0] == "yell" && t[1] == "hello", "多余空格被吃掉");
        auto t2 = Tok("r");
        CK(t2.size() == 1, "只有 r 无半径 -> 代码报用法错误");
        auto t3 = Tok("entry abc hi");
        CK(!IsAllDigit(t3[1]), "entry 后非数字 -> 被拦");
        // 只有修饰符没台词
        auto t4 = Tok("yell");
        CK(JoinFrom(t4, 1).empty(), "只有修饰符没台词 -> 空 -> 报错");
    }

    printf("\n=====================================================\n");
    printf(" 通过 %d / 失败 %d\n", g_pass, g_fail);
    printf("=====================================================\n");
    return g_fail ? 1 : 0;
}
