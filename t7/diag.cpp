#include <cstdio>
#include <string>
#include <vector>
#include <algorithm>
// 复刻我代码里的搜索逻辑，诊断为什么总是多个匹配
// Utf8FitTo 等价于「子串包含」
static bool FitTo(std::string const& name, std::string const& part){
    return name.find(part) != std::string::npos;
}
int main(){
    // 3.3.5 真实数据：同名物品很常见
    std::vector<std::pair<int,std::string>> db = {
        {32837,"埃辛诺斯战刃"},   // 主手
        {32838,"埃辛诺斯战刃"},   // 副手 —— 同名！
        {30902,"埃辛诺斯之戒"},
        {900001,"测试-十亿之刃"},
        {17182,"奥金斧"},
        {19019,"雷霆之怒"},
    };
    const char* inputs[] = {"埃辛诺斯","埃辛诺斯战刃","32837","之刃"};
    for(auto in : inputs){
        std::vector<int> hit;
        for(auto&p:db) if(FitTo(p.second,in)) hit.push_back(p.first);
        printf("输入 \"%s\" -> 匹配 %zu 项 %s\n", in, hit.size(),
            hit.size()==1?"[直接给]":(hit.empty()?"[未找到]":"[列候选]"));
    }
    printf("\n结论：\n");
    printf("  1. 埃辛诺斯战刃 主手/副手【同名】-> 精确匹配也是2项\n");
    printf("  2. 纯数字 32837 走名称搜索 -> 匹配0项 -> 报未找到\n");
    return 0;
}
