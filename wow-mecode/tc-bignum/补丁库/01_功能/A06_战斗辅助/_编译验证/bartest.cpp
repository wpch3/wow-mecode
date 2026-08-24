#include "mock.h"
#include "CombatSpecData.h"
int main(){
    uint8 cls[]={1,2,3,4,5,6,7,8,9,11};
    printf("=== 配栏占用检查（每区上限12格）===\n");
    int overflow=0, total=0;
    for(uint8 c:cls) for(uint8 sp=0;sp<CombatSpec::GetSpecCount(c);++sp) for(uint8 r=1;r<=3;++r){
        CombatSpec::BuiltPlan pl;
        CombatSpec::BuildPlan(c,sp,r,CombatSpec::SCENE_RAID,false,pl);
        // 第4区 = opener + utility
        size_t z4 = pl.opener.size();
        for(auto const&u:pl.utility){bool d=false;for(auto const&o:pl.opener)if(o.spell==u.spell)d=true;if(!d)++z4;}
        ++total;
        bool ov = pl.core.size()>24||pl.burst.size()>12||pl.emergency.size()>12||z4>12;
        if(ov){++overflow;
          printf("  溢出 cls%-2d spec%d role%d %-10s core=%zu burst=%zu emerg=%zu zone4=%zu\n",
                 c,sp,r,pl.specName,pl.core.size(),pl.burst.size(),pl.emergency.size(),z4);}
    }
    printf("\n共 %d 种组合，溢出 %d 种\n",total,overflow);
    printf("\n=== 抽样：各区实际占用 ===\n");
    struct S{uint8 c,sp,r;char const*n;};
    S ss[]={{2,0,3,"神圣骑-治疗"},{2,1,1,"防护骑-坦克"},{2,2,2,"惩戒骑-输出"},
            {7,2,3,"恢复萨-治疗"},{8,1,2,"火法-输出"},{11,2,1,"熊德-坦克"}};
    for(auto&x:ss){
        CombatSpec::BuiltPlan pl;
        CombatSpec::BuildPlan(x.c,x.sp,x.r,CombatSpec::SCENE_RAID,false,pl);
        size_t z4=pl.opener.size();
        for(auto const&u:pl.utility){bool d=false;for(auto const&o:pl.opener)if(o.spell==u.spell)d=true;if(!d)++z4;}
        printf("  %-14s 主循环%2zu 爆发%2zu 保命%2zu 增益功能%2zu  (总%zu)\n",
               x.n,pl.core.size(),pl.burst.size(),pl.emergency.size(),z4,
               pl.core.size()+pl.burst.size()+pl.emergency.size()+z4);
    }
    return 0;
}
