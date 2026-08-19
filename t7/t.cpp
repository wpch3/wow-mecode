// 验证 cs_smartadd 的核心算法：逗号拆分、数量解析、xN 解析
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>
typedef uint32_t uint32;

static std::vector<std::string> SplitByComma(std::string const& input)
{
    std::vector<std::string> parts;
    std::string cur;
    for (char c : input)
    {
        if (c == ',') { if (!cur.empty()) parts.push_back(cur); cur.clear(); }
        else cur += c;
    }
    if (!cur.empty()) parts.push_back(cur);
    for (auto& s : parts)
    {
        size_t b = s.find_first_not_of(" \t");
        size_t e = s.find_last_not_of(" \t");
        s = (b == std::string::npos) ? "" : s.substr(b, e - b + 1);
    }
    parts.erase(std::remove_if(parts.begin(), parts.end(),
        [](std::string const& s){ return s.empty(); }), parts.end());
    return parts;
}

static void ParseCount(std::string const& input, std::string& namePart, uint32& count)
{
    namePart = input; count = 1;
    size_t lastSpace = input.find_last_of(' ');
    if (lastSpace != std::string::npos)
    {
        std::string tail = input.substr(lastSpace + 1);
        bool allDigit = !tail.empty() &&
            std::all_of(tail.begin(), tail.end(), [](char c){ return c>='0'&&c<='9'; });
        if (allDigit)
        {
            count = uint32(atoi(tail.c_str()));
            if (count == 0) count = 1;
            namePart = input.substr(0, lastSpace);
        }
    }
}

static void ParseAmountX(std::string const& input, std::string& namePart, uint32& amount)
{
    namePart = input; amount = 1;
    size_t lastSpace = input.find_last_of(' ');
    if (lastSpace != std::string::npos)
    {
        std::string tail = input.substr(lastSpace + 1);
        if (tail.size() >= 2 && (tail[0]=='x'||tail[0]=='X'))
        {
            std::string numStr = tail.substr(1);
            bool allDigit = std::all_of(numStr.begin(), numStr.end(),
                [](char c){ return c>='0'&&c<='9'; });
            if (allDigit)
            {
                amount = uint32(atoi(numStr.c_str()));
                if (amount==0) amount=1;
                if (amount>50) amount=50;
                namePart = input.substr(0, lastSpace);
            }
        }
    }
}

int main(){
    printf("=== 1. 逗号拆分（含中文/空格）===\n");
    const char* cases[] = {
        "火焰之击, 霜之哀伤, 灰烬使者",
        "熊,狼,  豺狼人",
        "单个物品",
        "带空格 的名字, 另一个"
    };
    for (auto c : cases) {
        auto v = SplitByComma(c);
        printf("  \"%s\"\n    -> %zu项: ", c, v.size());
        for (auto& s : v) printf("[%s] ", s.c_str());
        printf("\n");
    }

    printf("\n=== 2. .add 数量解析 ===\n");
    const char* addCases[] = {
        "埃辛诺斯战刃 5", "埃辛诺斯战刃", "回城卷轴 20",
        "物品名带 2 个空格", "测试 0"
    };
    for (auto c : addCases) {
        std::string n; uint32 cnt;
        ParseCount(c, n, cnt);
        printf("  \"%s\" -> 名称=[%s] 数量=%u\n", c, n.c_str(), cnt);
    }

    printf("\n=== 3. .spawn xN 解析 ===\n");
    const char* spCases[] = {
        "石爪豺狼人 x5", "石爪豺狼人", "熊 X10",
        "怪物 x999", "怪物 x0", "名字里有x 的怪"
    };
    for (auto c : spCases) {
        std::string n; uint32 a;
        ParseAmountX(c, n, a);
        printf("  \"%s\" -> 名称=[%s] 数量=%u\n", c, n.c_str(), a);
    }
    return 0;
}
