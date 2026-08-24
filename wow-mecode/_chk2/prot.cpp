#include <cstdio>
#include <algorithm>
typedef int int32; typedef unsigned uint32;
// 复刻新逻辑
static int32 scaleCast(int32 cast, uint32 castPct, uint32 castFloor,
                       uint32 gcdPct, uint32 gcdFloor, bool notBelowGcd) {
    if (castPct == 100) return cast;
    int32 scaled = int32(float(cast) * float(castPct) / 100.0f);
    scaled = std::max<int32>(scaled, int32(castFloor));
    if (notBelowGcd) {
        int32 gf = std::max<int32>(int32(1500.0f * float(gcdPct)/100.0f), int32(gcdFloor));
        if (cast > gf) scaled = std::max<int32>(scaled, gf);
    }
    return scaled;
}
int main(){
    const uint32 CAST=75, CF=500, GCD=60, GF=300;
    int32 effGcd = std::max<int32>(int32(1500*0.60f), int32(GF));
    printf("======================================================================\n");
    printf("  读条保护验证  (读条75%%  GCD60%%  有效GCD=%dms)\n", effGcd);
    printf("======================================================================\n");
    printf("  %-24s %-8s %-10s %-10s %s\n","技能","原读条","无保护","有保护","说明");
    printf("  --------------------------------------------------------------------\n");
    struct{const char*n;int32 c;} t[]={
        {"火球术 Rank16",3500},{"火球术 Rank5",3500},{"火球术 Rank3",2500},
        {"火球术 Rank1",1500},{"奥术冲击",1500},{"1秒读条技能",1000},
        {"0.8秒读条",800},{"瞬发技能",0},
    };
    for(auto&x:t){
        if(x.c==0){printf("  %-24s %-8d %-10s %-10s 瞬发不受影响\n",x.n,0,"-","-");continue;}
        int32 no=std::max<int32>(int32(x.c*0.75f),int32(CF));
        int32 yes=scaleCast(x.c,CAST,CF,GCD,GF,true);
        const char* note = (no!=yes) ? "<- 保护生效，避免白压" :
                           (x.c<=effGcd ? "原读条<=GCD，不干预" : "");
        printf("  %-24s %-8d %-10d %-10d %s\n",x.n,x.c,no,yes,note);
    }
    printf("\n  >>> 关键：读条 1000ms 的技能，无保护会压到 750ms，\n");
    printf("      但 GCD 是 900ms —— 那 150ms 完全白给，还让读条条一闪而过。\n");
    printf("      有保护后停在 900ms，正好和 GCD 齐平。\n");
    return 0;
}
