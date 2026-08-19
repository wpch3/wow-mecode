#include "CustomSpeed.h"
#include <cstdio>
// 复刻 CustomSpeed.cpp 的算法做独立验算（不依赖 conf）
static int32 scale(int32 v, uint32 pct, uint32 floor_) {
    if (pct == 100) return v;
    int32 r = int32(float(v) * float(pct) / 100.0f);
    return r > int32(floor_) ? r : int32(floor_);
}
static uint32 cdPct(int32 cd, uint32 s, uint32 m, uint32 l) {
    if (cd < 30*1000) return s;
    if (cd <= 3*60*1000) return m;
    return l;
}
int main() {
    const uint32 GCD=60, CAST=75, CDG=100, S=85, M=80, L=90;
    const uint32 GF=300, CF=500, DF=3000, MIN=3000;

    printf("========================================================================\n");
    printf("  验证 1：GCD 缩放 60%%  (地板 %ums)\n", GF);
    printf("========================================================================\n");
    struct { const char* n; int32 base; } g[] = {
        {"战士/猎人 (物理, 无急速加成)", 1500},
        {"法师 (急速30%%后)",            1153},
        {"盗贼/DK (能量类)",             1000},
    };
    for (auto& x : g)
        printf("  %-32s %4d -> %4d ms   (%.2fx 快)\n",
               x.n, x.base, scale(x.base,GCD,GF), double(x.base)/scale(x.base,GCD,GF));

    printf("\n========================================================================\n");
    printf("  验证 2：读条缩放 75%%  (地板 %ums)\n", CF);
    printf("========================================================================\n");
    struct { const char* n; int32 base; } c[] = {
        {"火球术",     3500}, {"强效治疗", 3000},
        {"暗影箭",     3000}, {"奥术飞弹", 5000},
        {"短读条技能", 1000}, {"极短读条",  600},
    };
    for (auto& x : c) {
        int32 r = scale(x.base,CAST,CF);
        printf("  %-14s %4d -> %4d ms%s\n", x.n, x.base, r,
               (r==int32(CF) && x.base*75/100 < int32(CF)) ? "   <- 触发地板保护" : "");
    }

    printf("\n========================================================================\n");
    printf("  验证 3：单技能CD 分段缩放\n");
    printf("========================================================================\n");
    struct { const char* n; int32 cd; } d[] = {
        {"英勇打击 (无CD)",     0},
        {"法术反射 (2秒)",   2000},
        {"冲锋 (15秒)",     15000},
        {"盾墙 (30秒)",     30000},
        {"寒冰屏障 (5分钟)",300000},
        {"重生 (20分钟)",  1200000},
    };
    for (auto& x : d) {
        if (x.cd <= 0)          { printf("  %-20s %7d -> 不变 (无CD)\n", x.n, x.cd); continue; }
        if (uint32(x.cd) < MIN) { printf("  %-20s %7d -> 不变 (低于 MinScale %ums)\n", x.n, x.cd, MIN); continue; }
        uint32 p = cdPct(x.cd,S,M,L);
        if (CDG != 100) p = p * CDG / 100;
        int32 r = scale(x.cd, p, DF);
        printf("  %-20s %7d -> %7d ms  (用 %u%% 档)  省 %.1f 秒\n",
               x.n, x.cd, r, p, (x.cd-r)/1000.0);
    }

    printf("\n========================================================================\n");
    printf("  验证 4：法系 vs 近战 平衡性（本次改动的核心目的）\n");
    printf("========================================================================\n");
    printf("  %-20s %-10s %-12s %-12s %s\n","职业/技能","原循环","只压GCD","GCD+读条","最终提速");
    printf("  ---------------------------------------------------------------------\n");
    struct { const char* n; int32 cast; int32 gcd; } r[] = {
        {"战士 致死打击",    0, 1500},
        {"盗贼 剔骨",        0, 1000},
        {"法师 火球术",   3500, 1153},
        {"牧师 强效治疗", 3000, 1153},
        {"法师 奥爆",        0, 1153},
    };
    for (auto& x : r) {
        int32 o  = x.cast > x.gcd ? x.cast : x.gcd;
        int32 g1 = scale(x.gcd,GCD,GF);
        int32 a  = x.cast > g1 ? x.cast : g1;
        int32 c1 = x.cast ? scale(x.cast,CAST,CF) : 0;
        int32 b  = c1 > g1 ? c1 : g1;
        printf("  %-20s %-10d %-12d %-12d %.2fx\n", x.n, o, a, b, double(o)/b);
    }
    printf("\n  >>> 只压GCD: 近战1.67x 但法系1.00x (完全没提速)\n");
    printf("  >>> 都压后:  近战1.67x 法系1.33x  差距明显收窄\n");
    return 0;
}
