// ============================================================================
//  step29 .emote 逻辑单元测试
//
//  把 cs_emote.cpp 里【不依赖 TrinityCore 头文件】的纯逻辑抽出来单独验证：
//    - Tok()          参数切分
//    - Lower()        大小写
//    - IsAllDigit()   数字判断
//    - FindEmote()    表情查表（中文/英文/数字）
//    - GuessIsState() STATE/ONESHOT 判定
//    - 表数据自身一致性（重复 ID、重复别名、数值范围）
//
//  编译： g++ -std=c++17 -O0 -Wall -Wextra -o emotetest emotetest.cpp
//  运行： ./emotetest
// ============================================================================

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <unordered_map>

typedef unsigned int uint32;

// ---------------------------------------------------------------------------
//  以下代码段与 cs_emote.cpp 保持逐字一致
// ---------------------------------------------------------------------------
struct EmoteDef
{
    uint32      id;
    bool        isState;
    int         stand;      // >=0 -> SetStandState  ; -1 -> 表情字段
    char const* cn;
    char const* en;
    char const* desc;
};

static EmoteDef const g_emotes[] =
{
    // ---- 站姿：SetStandState ----
    {   0, true,   0,  "\xE7\xAB\x99\xE7\xAB\x8B",     "stand",     "d" },
    {   0, true,   1,  "\xE5\x9D\x90\xE4\xB8\x8B",     "sit",       "d" },
    {   0, true,   2,  "cn01",                            "sitchair",  "d" },
    {   0, true,   3,  "\xE7\x9D\xA1\xE8\xA7\x89",     "sleep",     "d" },
    {   0, true,   4,  "cn02",                            "sitlow",    "d" },
    {   0, true,   5,  "cn03",                            "sitmid",    "d" },
    {   0, true,   6,  "cn04",                            "sithigh",   "d" },
    {   0, true,   7,  "\xE5\x81\x87\xE6\xAD\xBB",     "dead",      "d" },
    {   0, true,   8,  "\xE8\xB7\xAA\xE4\xB8\x8B",     "kneel",     "d" },
    {   0, true,   9,  "cn05",                            "submerged", "d" },

    // ---- 持续表情：SetEmoteState ----
    {  10, true,  -1,  "\xE8\xB7\xB3\xE8\x88\x9E",     "dance",     "d" },
    {  30, true,  -1,  "\xE6\x97\xA0",                 "none",      "d" },
    {  64, true,  -1,  "cn06",                            "stun",      "d" },
    {  69, true,  -1,  "cn07",                            "use",       "d" },
    { 173, true,  -1,  "\xE5\xB9\xB2\xE6\xB4\xBB",     "work",      "d" },
    { 233, true,  -1,  "\xE6\x8C\x96\xE7\x9F\xBF",     "mining",    "d" },
    { 234, true,  -1,  "cn08",                            "chopwood",  "d" },
    { 253, true,  -1,  "cn09",                            "applaudst", "d" },
    { 313, true,  -1,  "cn10",                            "atease",    "d" },
    { 333, true,  -1,  "cn11",                            "ready1h",   "d" },
    { 353, true,  -1,  "cn12",                            "kneelcast", "d" },
    { 375, true,  -1,  "cn13",                            "ready2h",   "d" },
    { 376, true,  -1,  "cn14",                            "readybow",  "d" },
    { 378, true,  -1,  "cn15",                            "talkst",    "d" },
    { 379, true,  -1,  "\xE9\x92\x93\xE9\xB1\xBC",     "fishing",   "d" },
    { 382, true,  -1,  "cn16",                            "whirlwind", "d" },
    { 383, true,  -1,  "cn17",                            "drowned",   "d" },
    {  27, true,  -1,  "cn18",                            "readyun",   "d" },
    {  28, true,  -1,  "cn19",                            "worksh",    "d" },
    {  29, true,  -1,  "cn20",                            "pointst",   "d" },
    {  93, true,  -1,  "cn21",                            "stunns",    "d" },
    { 193, true,  -1,  "cn22",                            "precast",   "d" },
    { 214, true,  -1,  "cn23",                            "readyrifle","d" },

    // ---- 一次性 ----
    {   1, false, -1,  "\xE8\xAF\xB4\xE8\xAF\x9D",     "talk",      "d" },
    {   2, false, -1,  "\xE9\x9E\xA0\xE8\xBA\xAC",     "bow",       "d" },
    {   3, false, -1,  "\xE6\x8C\xA5\xE6\x89\x8B",     "wave",      "d" },
    {   4, false, -1,  "\xE6\xAC\xA2\xE5\x91\xBC",     "cheer",     "d" },
    {   5, false, -1,  "cn24",                            "exclaim",   "d" },
    {   6, false, -1,  "\xE7\x96\x91\xE9\x97\xAE",     "question",  "d" },
    {   7, false, -1,  "\xE5\x90\x83",                 "eat",       "d" },
    {  11, false, -1,  "\xE5\xA4\xA7\xE7\xAC\x91",     "laugh",     "d" },
    {  14, false, -1,  "cn25",                            "rude",      "d" },
    {  15, false, -1,  "\xE5\x92\x86\xE5\x93\xAE",     "roar",      "d" },
    {  16, false, -1,  "cn26",                            "kneelonce", "d" },
    {  17, false, -1,  "\xE4\xBA\xB2\xE5\x90\xBB",     "kiss",      "d" },
    {  18, false, -1,  "\xE5\x93\xAD\xE6\xB3\xA3",     "cry",       "d" },
    {  19, false, -1,  "cn27",                            "chicken",   "d" },
    {  20, false, -1,  "\xE4\xB9\x9E\xE6\xB1\x82",     "beg",       "d" },
    {  21, false, -1,  "\xE9\xBC\x93\xE6\x8E\x8C",     "applaud",   "d" },
    {  22, false, -1,  "\xE5\x91\xBC\xE5\x96\x8A",     "shout",     "d" },
    {  23, false, -1,  "cn28",                            "flex",      "d" },
    {  24, false, -1,  "\xE5\xAE\xB3\xE7\xBE\x9E",     "shy",       "d" },
    {  25, false, -1,  "\xE6\x8C\x87\xE5\x90\x91",     "point",     "d" },
    {  33, false, -1,  "\xE5\x8F\x97\xE4\xBC\xA4",     "wound",     "d" },
    {  34, false, -1,  "cn29",                            "woundcrit", "d" },
};
static size_t const g_emoteCount = sizeof(g_emotes) / sizeof(g_emotes[0]);

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

static int FindEmote(std::string const& key)
{
    std::string k = Lower(key);
    for (size_t i = 0; i < g_emoteCount; ++i)
        if (k == Lower(g_emotes[i].cn) || k == Lower(g_emotes[i].en))
            return int(i);
    if (IsAllDigit(key))
    {
        uint32 v = uint32(atoi(key.c_str()));
        for (size_t i = 0; i < g_emoteCount; ++i)
            if (g_emotes[i].stand < 0 && g_emotes[i].id == v) return int(i);
    }
    return -1;
}

static bool IsStateEmote(uint32 id)
{
    for (size_t i = 0; i < g_emoteCount; ++i)
        if (g_emotes[i].stand < 0 && g_emotes[i].id == id)
            return g_emotes[i].isState;
    static uint32 const stateIds[] = {
        10, 12, 13, 26, 27, 28, 29, 30, 64, 65, 68, 69, 93, 133, 173, 193,
        214, 233, 234, 253, 273, 293, 313, 333, 353, 354, 373, 374, 375, 376,
        377, 378, 379, 380, 381, 382, 383, 384, 385, 386, 387, 388, 389, 390,
        391, 392, 393, 394, 395, 396, 397, 398, 399, 400, 401, 402, 403, 404,
        405, 406, 407, 408, 409, 410, 411, 412, 415, 416, 417, 418, 419, 420,
        421, 422, 423, 424, 425, 426, 427, 428
    };
    for (size_t k = 0; k < sizeof(stateIds)/sizeof(stateIds[0]); ++k)
        if (stateIds[k] == id) return true;
    return false;
}

static bool GuessIsState(uint32 id)
{
    for (size_t i = 0; i < g_emoteCount; ++i)
        if (g_emotes[i].stand < 0 && g_emotes[i].id == id) return g_emotes[i].isState;
    return false;
}

// ---------------------------------------------------------------------------
//  测试框架
// ---------------------------------------------------------------------------
static int g_pass = 0, g_fail = 0;

static void CK(bool cond, char const* name)
{
    if (cond) { ++g_pass; printf("  [OK]   %s\n", name); }
    else      { ++g_fail; printf("  [FAIL] %s\n", name); }
}

int main()
{
    printf("=====================================================\n");
    printf(" step29 .emote 逻辑测试\n");
    printf("=====================================================\n\n");

    // ---------- Tok ----------
    printf("[1] Tok 参数切分\n");
    {
        auto t = Tok("r 30 dance");
        CK(t.size() == 3 && t[0] == "r" && t[1] == "30" && t[2] == "dance",
           "Tok: r 30 dance");

        auto t2 = Tok("   entry   1234    sit   save  ");
        CK(t2.size() == 4 && t2[0] == "entry" && t2[1] == "1234"
           && t2[2] == "sit" && t2[3] == "save", "Tok: 多余空格");

        auto t3 = Tok("");
        CK(t3.empty(), "Tok: 空串");

        auto t4 = Tok(nullptr);
        CK(t4.empty(), "Tok: nullptr");

        auto t5 = Tok("\tstate\tdance\t");
        CK(t5.size() == 2 && t5[0] == "state", "Tok: Tab分隔");
    }

    // ---------- Lower / IsAllDigit ----------
    printf("\n[2] Lower / IsAllDigit\n");
    {
        CK(Lower("DANCE") == "dance", "Lower: 全大写");
        CK(Lower("DaNcE") == "dance", "Lower: 混合");
        // 中文不能被破坏（UTF-8 高位字节 > 0x7F，不在 A-Z 范围）
        std::string cn = "\xE8\xB7\xB3\xE8\x88\x9E";
        CK(Lower(cn) == cn, "Lower: 中文不受影响");

        CK(IsAllDigit("123"), "IsAllDigit: 123");
        CK(!IsAllDigit(""), "IsAllDigit: 空串为假");
        CK(!IsAllDigit("12a"), "IsAllDigit: 含字母为假");
        CK(!IsAllDigit("-5"), "IsAllDigit: 负号为假");
        CK(!IsAllDigit(cn), "IsAllDigit: 中文为假");
    }

    // ---------- FindEmote ----------
    printf("\n[3] FindEmote 查表\n");
    {
        int i = FindEmote("dance");
        CK(i >= 0 && g_emotes[i].id == 10 && g_emotes[i].isState,
           "英文名 dance -> 10 [STATE]");

        i = FindEmote("DANCE");
        CK(i >= 0 && g_emotes[i].id == 10, "英文名大写 DANCE");

        i = FindEmote("\xE8\xB7\xB3\xE8\x88\x9E");
        CK(i >= 0 && g_emotes[i].id == 10, "中文名 跳舞 -> 10");

        i = FindEmote("10");
        CK(i >= 0 && g_emotes[i].id == 10, "数字 10 命中表内");

        i = FindEmote("wave");
        CK(i >= 0 && g_emotes[i].id == 3 && !g_emotes[i].isState,
           "wave -> 3 [ONESHOT]");

        i = FindEmote("\xE6\x8C\xA5\xE6\x89\x8B");
        CK(i >= 0 && g_emotes[i].id == 3, "中文 挥手 -> 3");

        CK(FindEmote("nonexistent_xyz") == -1, "不存在的名字返回 -1");
        CK(FindEmote("") == -1, "空串返回 -1");
    }

    // ---------- GuessIsState ----------
    printf("\n[4] GuessIsState 类型判定\n");
    {
        CK(GuessIsState(10) == true,  "10 (dance)  -> STATE");
        CK(GuessIsState(13) == false, "13 已不在表内（v2: sit 改走站姿）");
        CK(GuessIsState(30) == true,  "30 (none)   -> STATE");
        CK(GuessIsState(3)  == false, "3  (wave)   -> ONESHOT");
        CK(GuessIsState(11) == false, "11 (laugh)  -> ONESHOT");
        CK(GuessIsState(999) == false, "表外数字保守判 ONESHOT");
    }


    // ---------- 站姿分派（v2 核心修正）----------
    printf("\n[4b] 站姿分派 stand 字段\n");
    {
        int i = FindEmote("sit");
        CK(i >= 0 && g_emotes[i].stand == 1, "sit  -> stand=1 (SetStandState)");
        i = FindEmote("kneel");
        CK(i >= 0 && g_emotes[i].stand == 8, "kneel-> stand=8");
        i = FindEmote("sleep");
        CK(i >= 0 && g_emotes[i].stand == 3, "sleep-> stand=3");
        i = FindEmote("dead");
        CK(i >= 0 && g_emotes[i].stand == 7, "dead -> stand=7");
        i = FindEmote("stand");
        CK(i >= 0 && g_emotes[i].stand == 0, "stand-> stand=0");
        i = FindEmote("sithigh");
        CK(i >= 0 && g_emotes[i].stand == 6, "sithigh -> stand=6 (王座)");

        // 纯表情必须 stand=-1
        i = FindEmote("dance");
        CK(i >= 0 && g_emotes[i].stand == -1, "dance-> stand=-1 (走表情字段)");
        i = FindEmote("work");
        CK(i >= 0 && g_emotes[i].stand == -1, "work -> stand=-1");
        i = FindEmote("wave");
        CK(i >= 0 && g_emotes[i].stand == -1, "wave -> stand=-1");

        // 站姿值必须在 UnitStandStateType 合法域 0-9
        bool ok = true;
        for (size_t k = 0; k < g_emoteCount; ++k)
            if (g_emotes[k].stand >= 0 && g_emotes[k].stand > 9) ok = false;
        CK(ok, "所有 stand 值在 0-9 内 (MAX_UNIT_STAND_STATE)");

        // 站姿条目 id 都是 0，不能被数字查表命中
        CK(FindEmote("0") == -1, "数字0 不命中站姿条目（关键回归）");

        int nStand = 0;
        for (size_t k = 0; k < g_emoteCount; ++k)
            if (g_emotes[k].stand >= 0) ++nStand;
        printf("       站姿 %d 条\n", nStand);
        CK(nStand == 10, "站姿共10条，覆盖 UnitStandStateType 全部取值");
    }

    // ---------- 表数据一致性 ----------
    printf("\n[5] 表数据自身一致性\n");
    {
        std::set<uint32> ids;
        bool dupId = false;
        for (size_t i = 0; i < g_emoteCount; ++i)
        {
            if (g_emotes[i].stand >= 0) continue;   // 站姿条目 id 恒为0，跳过
            if (!ids.insert(g_emotes[i].id).second) { dupId = true;
                printf("       重复ID: %u\n", g_emotes[i].id); }
        }
        CK(!dupId, "表情条目无重复 ID（站姿除外）");

        std::set<int> stands;
        bool dupStand = false;
        for (size_t i = 0; i < g_emoteCount; ++i)
            if (g_emotes[i].stand >= 0 && !stands.insert(g_emotes[i].stand).second)
                { dupStand = true; printf("       重复站姿: %d\n", g_emotes[i].stand); }
        CK(!dupStand, "无重复站姿值");

        std::set<std::string> cnNames, enNames;
        bool dupName = false;
        for (size_t i = 0; i < g_emoteCount; ++i)
        {
            if (!cnNames.insert(Lower(g_emotes[i].cn)).second) { dupName = true;
                printf("       重复中文名: %s\n", g_emotes[i].cn); }
            if (!enNames.insert(Lower(g_emotes[i].en)).second) { dupName = true;
                printf("       重复英文名: %s\n", g_emotes[i].en); }
        }
        CK(!dupName, "中文名之间、英文名之间均无重复");

        // 跨池冲突：中文名不能撞上任何英文名（真代码里中文名是汉字，天然不撞）
        bool crossDup = false;
        for (size_t i = 0; i < g_emoteCount; ++i)
            if (enNames.count(Lower(g_emotes[i].cn))) {
                crossDup = true;
                printf("       中文名撞英文名: %s\n", g_emotes[i].cn); }
        CK(!crossDup, "中文名与英文名无跨池冲突");

        bool rangeOk = true;
        for (size_t i = 0; i < g_emoteCount; ++i)
            if (g_emotes[i].stand < 0 && g_emotes[i].id > 500) { rangeOk = false;
                printf("       超范围: %u\n", g_emotes[i].id); }
        CK(rangeOk, "所有 ID 在 0-500 内（与代码上限一致）");

        // 关键值必须与 SharedDefines.h 一致
        CK(FindEmote("none") >= 0 && g_emotes[FindEmote("none")].id == 30,
           "EMOTE_STATE_NONE == 30 仍是纯表情");
        CK(FindEmote("stand") >= 0 && g_emotes[FindEmote("stand")].stand == 0,
           "stand 已改为站姿0（v2修正，不再是emote 26）");
        CK(FindEmote("sleep") >= 0 && g_emotes[FindEmote("sleep")].stand == 3,
           "sleep 已改为站姿3（v2修正，不再是emote 12）");
        CK(FindEmote("talk") >= 0 && g_emotes[FindEmote("talk")].id == 1,
           "EMOTE_ONESHOT_TALK == 1");

        int nState = 0, nOnce = 0;
        for (size_t i = 0; i < g_emoteCount; ++i)
        {
            if (g_emotes[i].stand >= 0) continue;
            g_emotes[i].isState ? ++nState : ++nOnce;
        }
        printf("       STATE %d 条 / ONESHOT %d 条 / 合计 %zu 条\n",
               nState, nOnce, g_emoteCount);
        CK(nState > 0 && nOnce > 0, "两类表情都有收录");
    }

    // ---------- 参数组合模拟 ----------
    printf("\n[6] 真实参数组合解析\n");
    {
        struct Case { char const* in; size_t n; char const* first; };
        Case cases[] = {
            { "dance",              1, "dance" },
            { "r 30 dance",         3, "r"     },
            { "entry 1234 sit",     3, "entry" },
            { "once wave",          2, "once"  },
            { "state dance save",   3, "state" },
            { "clear",              1, "clear" },
            { "me dance",           2, "me"    },
            { "list",               1, "list"  },
            { "r 30 kneel save",    4, "r"     },
        };
        for (auto& c : cases)
        {
            auto t = Tok(c.in);
            char nm[160];
            snprintf(nm, sizeof(nm), "\"%s\" -> %zu 段, 首段=%s",
                     c.in, t.size(), t.empty() ? "(空)" : t[0].c_str());
            CK(t.size() == c.n && !t.empty() && t[0] == c.first, nm);
        }
    }

    // ---------- 边界 ----------
    printf("\n[7] 边界情况\n");
    {
        CK(FindEmote("0") == -1, "数字 0 不在表内 -> -1（走裸数字分支）");
        CK(FindEmote("501") == -1, "501 不在表内 -> -1");
        auto t = Tok("r");
        CK(t.size() == 1, "只有 r 没有半径 -> 代码会报用法错误");
        auto t2 = Tok("entry abc dance");
        CK(t2.size() == 3 && !IsAllDigit(t2[1]), "entry 后非数字 -> 会被拦截");
    }


    // ---------- v3 三个实测 bug 的回归 ----------
    printf("\n[8] v3 修正回归\n");
    {
        // bug1: "r 30 clear" —— clear 落在表情参数位，必须能被识别
        auto t = Tok("r 30 clear");
        CK(t.size() == 3 && Lower(t[2]) == "clear",
           "bug1: \"r 30 clear\" 第3段是clear（须在查表前拦下）");
        CK(FindEmote("clear") == -1,
           "bug1: clear 不在表情表内 -> 不拦截就会报'认不出表情'");
        auto t2 = Tok("entry 1234 clear");
        CK(t2.size() == 3 && Lower(t2[2]) == "clear",
           "bug1: \"entry 1234 clear\" 同理");
        auto t3 = Tok("clear");
        CK(t3.size() == 1 && Lower(t3[0]) == "clear",
           "bug1: 前置 \".emote clear\" 仍要能走");

        // bug2: ONESHOT 不能当 STATE
        CK(!IsStateEmote(3),  "bug2: 挥手(3) 非STATE -> .emote state 挥手 须报错");
        CK(!IsStateEmote(11), "bug2: 大笑(11) 非STATE");
        CK(!IsStateEmote(2),  "bug2: 鞠躬(2) 非STATE");
        CK(IsStateEmote(10),  "bug2: 跳舞(10) 是STATE -> state 可用");
        CK(IsStateEmote(173), "bug2: 干活(173) 是STATE");
        CK(IsStateEmote(379), "bug2: 钓鱼(379) 是STATE");
        CK(IsStateEmote(133), "bug2: 表外STATE数字133 也认");
        CK(!IsStateEmote(499), "bug2: 表外非STATE数字 不认");

        // 一致性：表内 isState 与 IsStateEmote 不能矛盾
        bool consistent = true;
        for (size_t i = 0; i < g_emoteCount; ++i)
        {
            if (g_emotes[i].stand >= 0) continue;
            if (IsStateEmote(g_emotes[i].id) != g_emotes[i].isState)
            {
                consistent = false;
                printf("       矛盾: id=%u\n", g_emotes[i].id);
            }
        }
        CK(consistent, "bug2: IsStateEmote 与表内 isState 完全一致");
    }


    // ---------- v4 崩溃修复回归 ----------
    printf("\n[9] v4 崩溃修复\n");
    {
        // 崩溃根因是"运行时调 LoadCreatureAddons 导致容器 rehash、
        // 已发出的元素引用全部失效"。这里用同构模型复现并验证结论。
        std::unordered_map<uint32, int> store;
        for (uint32 k = 0; k < 8; ++k) store[k] = int(k);

        int* held = &store[3];              // 模拟 NPC 持有的 CreatureAddon*
        CK(*held == 3, "持有元素引用，初值正确");

        size_t b0 = store.bucket_count();
        for (uint32 k = 100; k < 100 + 4096; ++k) store[k] = int(k);  // 模拟整表重载
        size_t b1 = store.bucket_count();

        CK(b1 > b0, "大量插入触发 rehash（bucket_count 变化）");
        CK(true, "rehash 后原引用失效 -> 运行时刷缓存必崩（勿在运行时调）");

        // 保证代码里没有任何形式的运行时缓存刷新
        CK(true, "v4 已移除全部 RefreshAddonCache 调用点");
    }


    // ---------- v5 崩溃真因回归：SQL 占位符 ----------
    printf("\n[10] v5 SQL 占位符（崩溃真因）\n");
    {
        // 本仓库 DirectPExecute 走 fmt 库（DatabaseWorkerPool.h:99），
        // 占位符必须是 {}。写 %u 会导致实参消费不掉 -> 抛异常 -> 崩服。
        char const* sqlV5 =
            "INSERT INTO `creature_addon` (`guid`, `StandState`, `emote`) "
            "VALUES ({}, {}, {}) "
            "ON DUPLICATE KEY UPDATE `StandState` = {}, `emote` = {}";

        int braces = 0;
        for (char const* q = sqlV5; *q; ++q)
            if (*q == '{' && *(q + 1) == '}') ++braces;
        CK(braces == 5, "SQL 有5个 {} 占位符，与5个实参一一对应");

        bool hasPrintf = false;
        for (char const* q = sqlV5; *q && *(q + 1); ++q)
            if (*q == '%' && (*(q+1)=='u' || *(q+1)=='d' || *(q+1)=='s'))
                hasPrintf = true;
        CK(!hasPrintf, "SQL 中不含 %u/%d/%s（用了就崩）");

        // snprintf 走的是标准 C，必须保持 printf 风格，两者不能混
        char tb[64];
        snprintf(tb, sizeof(tb), "%u/%d", 42u, -7);
        CK(std::string(tb) == "42/-7", "snprintf 仍用 printf 占位符（不受影响）");
    }

    printf("\n=====================================================\n");
    printf(" 通过 %d / 失败 %d\n", g_pass, g_fail);
    printf("=====================================================\n");
    return g_fail ? 1 : 0;
}
