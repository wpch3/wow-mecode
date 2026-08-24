#include "bnstub.h"
#include "bnstub2.h"

std::vector<std::string> g_log;
std::map<std::string,std::string> g_sql;
uint32 g_strictMask = 0;          // worldserver.conf.dist:607 默认值
ObjectMgrStub _omgr;

// ---- 被测：名字校验（从 cs_botrename.cpp 提取核心逻辑）----
struct Res { bool ok; std::string why; };

static Res ValidateName(std::string const& name, bool isPlayerBot)
{
    if(name.empty()) return {false,"名字不能为空"};

    if(!isPlayerBot){
        if(name.size()>100) return {false,"名字太长了（最多100字节）"};
        return {true,""};
    }

    std::string check=name;
    if(!normalizePlayerName(check)) return {false,"名字格式非法"};

    ResponseCodes res = ObjectMgrStub::CheckPlayerName(check, LOCALE_zhCN, true);
    if(res!=CHAR_NAME_SUCCESS){
        char const* why="名字不合法";
        switch(res){
            case CHAR_NAME_TOO_LONG: why="名字太长"; break;
            case CHAR_NAME_TOO_SHORT: why="名字太短"; break;
            case CHAR_NAME_INVALID_CHARACTER: why="含有非法字符"; break;
            case CHAR_NAME_MIXED_LANGUAGES: why="混用了多种语言（中文和英文不能混）"; break;
            case CHAR_NAME_THREE_CONSECUTIVE: why="有三个连续相同的字符"; break;
            case CHAR_NAME_RESERVED: why="是保留名"; break;
            case CHAR_NAME_PROFANE: why="被判定为不雅词汇"; break;
            default: break;
        }
        return {false,why};
    }
    if(_omgr.IsReservedName(check)) return {false,"这是保留名，不能用"};
    return {true,check};
}

// ---- 被测：切词与拼接 ----
static std::vector<std::string> BnTok(char const* args){
    std::vector<std::string> out; if(!args) return out;
    std::string s(args); size_t i=0;
    while(i<s.size()){
        while(i<s.size()&&(s[i]==' '||s[i]=='\t')) ++i;
        if(i>=s.size()) break;
        size_t st=i;
        while(i<s.size()&&s[i]!=' '&&s[i]!='\t') ++i;
        out.push_back(s.substr(st,i-st));
    }
    return out;
}
static std::string BnJoin(std::vector<std::string> const& t,size_t from){
    std::string s;
    for(size_t i=from;i<t.size();++i){ if(!s.empty()) s+=' '; s+=t[i]; }
    return s;
}

static int pass=0,fail=0;
#define CHECK(c,n) do{ if(c){++pass;} else {++fail;printf("  [FAIL] %s\n",n);} }while(0)

int main(){
    printf("=== step43 Bot改名 测试 ===\n\n");

    // ===== 中文名（最关心的）=====
    { Res r=ValidateName("阿尔萨斯",true);
      CHECK(r.ok,"T1 【核心】纯中文名可用"); }
    { Res r=ValidateName("霜之哀伤",true);
      CHECK(r.ok,"T2 四字中文名可用"); }
    { Res r=ValidateName("希尔瓦娜斯",true);
      CHECK(r.ok,"T3 五字中文名可用"); }
    { Res r=ValidateName("巫妖王阿尔萨斯米奈希尔",true);   // 11字，没超
      CHECK(r.ok,"T4 11个中文字符可用(边界内)"); }
    { Res r=ValidateName("一二三四五六七八九十十一二三",true); // 13字
      CHECK(!r.ok,"T4b 13个中文字符被拒");
      CHECK(r.why=="名字太长","T4b 报错正确"); }

    // ===== 英文名 =====
    { Res r=ValidateName("Arthas",true);
      CHECK(r.ok,"T5 英文名可用");
      CHECK(r.why=="Arthas","T5 首字母大写规范化"); }
    { Res r=ValidateName("aRTHAS",true);
      CHECK(r.ok&&r.why=="Arthas","T6 大小写被规范化"); }

    // ===== 【关键】中英混用要被拒 =====
    { Res r=ValidateName("阿尔萨斯X",true);
      CHECK(!r.ok,"T7 【重要】中英混用被拒（暴雪原版规则）");
      CHECK(r.why.find("混用")!=std::string::npos,"T7 提示说清楚了"); }
    { Res r=ValidateName("Bot机器人",true);
      CHECK(!r.ok,"T8 英文+中文也被拒"); }

    // ===== 其它规则 =====
    // T9【实测校正】"aaa" 经 normalizePlayerName 变成 "Aaa"，
    //  首字母大写后就【不是】三个相同字符了 -> 官方也是这个行为。
    { Res r=ValidateName("aaa",true);
      CHECK(r.ok,"T9 'aaa'规范化成'Aaa'后合法(与官方一致)");
      CHECK(r.why=="Aaa","T9 确认被规范化"); }
    { Res r=ValidateName("Baaa",true);
      CHECK(!r.ok,"T9b 'Baaa'含真三连被拒");
      CHECK(r.why=="有三个连续相同的字符","T9b 报错正确"); }
    { Res r=ValidateName("A",true);
      CHECK(!r.ok,"T10 太短被拒"); }
    { Res r=ValidateName("",true);
      CHECK(!r.ok,"T11 空名被拒"); }
    { _omgr.reserved.push_back("Admin");
      Res r=ValidateName("Admin",true);
      CHECK(!r.ok,"T12 保留名被拒");
      _omgr.reserved.clear(); }

    // ===== NPCBot（Creature）规则宽松得多 =====
    { Res r=ValidateName("阿尔萨斯X",false);
      CHECK(r.ok,"T13 【对比】Creature允许中英混用"); }
    { Res r=ValidateName("巫妖王 阿尔萨斯·米奈希尔",false);
      CHECK(r.ok,"T14 Creature允许长名和空格"); }
    { Res r=ValidateName("Baaa",false);
      CHECK(r.ok,"T15 Creature允许三连字符"); }
    { std::string longName(101,'x');
      Res r=ValidateName(longName,false);
      CHECK(!r.ok,"T16 Creature超100字节被拒"); }

    // ===== StrictPlayerNames=1 时中文会被拒（配置问题不是bug）=====
    { g_strictMask=1;
      Res r=ValidateName("阿尔萨斯",true);
      CHECK(!r.ok,"T17 StrictPlayerNames=1时中文被拒(符合预期)");
      g_strictMask=0;
      Res r2=ValidateName("阿尔萨斯",true);
      CHECK(r2.ok,"T17 改回0又能用了"); }

    // ===== 切词与带空格的名字 =====
    { auto t=BnTok("player OldName New Name Here");
      CHECK(t.size()==5,"T18 切词正确");
      CHECK(BnJoin(t,2)=="New Name Here","T18 【重要】带空格的名字能拼回来"); }
    { auto t=BnTok("entry 70001 cn 巫妖王");
      CHECK(t.size()==4,"T19 entry模式切词");
      CHECK(t[1]=="70001","T19 entry值正确");
      CHECK(BnJoin(t,3)=="巫妖王","T19 中文名取出正确"); }
    { auto t=BnTok("cn 霜之哀伤");
      CHECK(BnJoin(t,1)=="霜之哀伤","T20 cn模式取名正确"); }
    { auto t=BnTok("   entry   70001   cn   名字   ");
      CHECK(t.size()==4,"T21 多余空格被忽略"); }
    { auto t=BnTok("");
      CHECK(t.empty(),"T22 空参数返回空"); }
    { auto t=BnTok(nullptr);
      CHECK(t.empty(),"T23 nullptr安全"); }

    // ===== UTF8 解析正确性（中文按字符数不是字节数）=====
    { std::wstring w; Utf8toWStr("阿尔萨斯",w);
      CHECK(w.size()==4,"T24 【关键】中文按字符数算(4不是12字节)");
      CHECK(isEastAsianCharacter(w[0]),"T24 识别为东亚字符"); }
    { std::wstring w; Utf8toWStr("Arthas",w);
      CHECK(w.size()==6,"T25 英文字符数正确");
      CHECK(!isEastAsianCharacter(w[0]),"T25 英文不是东亚字符"); }

    // ===== 12字中文正好在边界 =====
    { Res r=ValidateName("一二三四五六七八九十十一",true);
      CHECK(r.ok,"T26 正好12个中文字符可用"); }
    { Res r=ValidateName("一二三四五六七八九十十一二",true);
      CHECK(!r.ok,"T27 13个中文字符被拒"); }

    printf("\n=== 结果: %d/%d 通过 ===\n",pass,pass+fail);
    return fail?1:0;
}
