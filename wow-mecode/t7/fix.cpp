#include <cstdio>
#include <string>
#include <vector>
#include <algorithm>
static bool FitTo(std::string const& n, std::string const& p){ return n.find(p)!=std::string::npos; }
static bool IsAllDigit(std::string const& s){
    return !s.empty() && std::all_of(s.begin(),s.end(),[](char c){return c>='0'&&c<='9';});
}
int main(){
    std::vector<std::pair<int,std::string>> db = {
        {32837,"埃辛诺斯战刃"},{32838,"埃辛诺斯战刃"},{30902,"埃辛诺斯之戒"},
        {900001,"测试-十亿之刃"},{17182,"奥金斧"},
    };
    // 新规则：1.纯数字=ID直取  2.完全同名归为一组，取第一个  3.仍多个才列候选
    const char* inputs[]={"32837","埃辛诺斯战刃","埃辛诺斯","不存在的东西"};
    for(auto in:inputs){
        std::string s=in;
        printf("输入 \"%s\" -> ", in);
        if(IsAllDigit(s)){
            int id=atoi(s.c_str());
            bool found=false;
            for(auto&p:db) if(p.first==id) found=true;
            printf("%s\n", found?"[按ID直接给]":"[ID不存在]");
            continue;
        }
        std::vector<std::pair<int,std::string>> hit;
        for(auto&p:db) if(FitTo(p.second,in)) hit.push_back(p);
        if(hit.empty()){ printf("[未找到]\n"); continue; }
        // 检查是否全部同名
        bool allSame=true;
        for(auto&h:hit) if(h.second!=hit[0].second) allSame=false;
        if(hit.size()==1)      printf("[直接给]\n");
        else if(allSame)       printf("[全部同名(%zu项)，取第一个直接给 ID:%d]\n",hit.size(),hit[0].first);
        else                   printf("[列候选 %zu 项]\n",hit.size());
    }
    return 0;
}
