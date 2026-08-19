#include <cstdio>
#include <cctype>
#include <string>
#include <string_view>
#include <map>
#include <algorithm>
// 完全照抄 Util.cpp:706/717
bool StringEqualI(std::string_view a, std::string_view b){
    return std::equal(a.begin(),a.end(),b.begin(),b.end(),
        [](char c1,char c2){ return std::tolower(c1)==std::tolower(c2); }); }
bool StringCompareLessI(std::string_view a, std::string_view b){
    return std::lexicographical_compare(a.begin(),a.end(),b.begin(),b.end(),
        [](char c1,char c2){ return std::tolower(c1)<std::tolower(c2); }); }
struct LessI { bool operator()(std::string_view a,std::string_view b) const {
    return StringCompareLessI(a,b);} };
bool StartsWithI(std::string_view h,std::string_view n){
    return StringEqualI(h.substr(0,n.length()),n); }

int main(){
    using Map=std::map<std::string_view,int,LessI>;
    // 场景A：源文件UTF-8，客户端UTF-8 -> 应该能匹配
    { Map m; m["\xE4\xBF\xA1\xE6\x81\xAF"]=1;  // "信息" UTF-8
      std::string_view tok="\xE4\xBF\xA1\xE6\x81\xAF";
      auto it=m.lower_bound(tok);
      bool ok = it!=m.end() && StartsWithI(it->first,tok);
      printf("A. 源码UTF-8 + 客户端UTF-8 : %s\n", ok?"匹配成功":"匹配失败"); }

    // 场景B：源文件GBK，客户端UTF-8 -> 必然失败
    { Map m; m["\xD0\xC5\xCF\xA2"]=1;          // "信息" GBK
      std::string_view tok="\xE4\xBF\xA1\xE6\x81\xAF";  // 客户端发UTF-8
      auto it=m.lower_bound(tok);
      bool ok = it!=m.end() && StartsWithI(it->first,tok);
      printf("B. 源码GBK  + 客户端UTF-8 : %s  <- 用户的情况\n", ok?"匹配成功":"匹配失败"); }

    // 场景C：函数内字符串比较（.pbot的做法），源码GBK
    { std::string cmdArg="\xE8\xBF\x87\xE6\x9D\xA5";      // 客户端发来"过来"UTF-8
      std::string literal_gbk="\xB9\xFD\xC0\xB4";          // 源码GBK里的"过来"
      std::string literal_utf8="\xE8\xBF\x87\xE6\x9D\xA5"; // 源码UTF-8里的"过来"
      printf("C1. .pbot 源码GBK  比较: %s\n", (cmdArg==literal_gbk)?"相等":"不等");
      printf("C2. .pbot 源码UTF8 比较: %s  <- 用户.pbot能用说明是这种\n",
             (cmdArg==literal_utf8)?"相等":"不等"); }
    return 0;
}
