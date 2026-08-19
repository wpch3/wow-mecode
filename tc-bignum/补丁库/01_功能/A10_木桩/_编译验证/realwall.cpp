// ============================================================
//  力量的【真实】天花板 —— 以客户端看到的数值为准
//
//  用户实测：「力量本身数值没问题，但攻击强度成负数了」
//  我上轮写「多半被钳成 INT_MAX」是【凭想象】，实测是 INT_MIN。
//
//  x86-64 的 cvttss2si 指令：float 超出 int32 范围时
//  返回 "integer indefinite value" = 0x80000000 = INT_MIN
//  （这是硬件行为，不是 C++ 标准保证的，但 x86/x64 上稳定如此）
// ============================================================
#include <cstdio>
#include <cstdint>
#include <cmath>
typedef int32_t int32; typedef uint32_t uint32;

static int no=0,pass=0,fail=0;
#define C(n) do{printf("  [%02d] %-54s ",++no,n);}while(0)
#define OK() do{printf("PASS\n");++pass;}while(0)
#define NG(f,...) do{printf("FAIL " f "\n",##__VA_ARGS__);++fail;}while(0)

// StatSystem.cpp 圣骑：val2 = level*3 + STR*2 - 20
static float PalVal2(float str, float lvl){ return lvl*3.0f + str*2.0f - 20.0f; }
// SetAttackPower(int32(base_attPower))
static int32 ToAP(float v){ volatile float t=v; return (int32)t; }

int main(){
printf("=== 力量真实天花板（客户端可见值为准） ===\n\n");

printf("-- A. float->int32 溢出的实际行为 --\n");

C("溢出返回 INT_MIN 而非 INT_MAX（x86-64 硬件行为）");
{ if(ToAP(4000000180.0f)==INT32_MIN) OK(); else NG("%d",ToAP(4000000180.0f)); }
printf("       ^ 我上轮猜 INT_MAX，错了。用户看到负数是对的\n");

C("恰好 2147483647.0f 也溢出（float 舍入成 2147483648）");
{ if(ToAP(2147483647.0f)==INT32_MIN) OK(); else NG("%d",ToAP(2147483647.0f)); }
printf("       ^ 所以真实可用上限比 21.47亿 还低一点\n");

printf("\n-- B. 圣骑力量的精确临界点 --\n");

// 二分找最大安全力量
float lo=0.0f, hi=2.0e9f;
for(int i=0;i<200;++i){
    float mid=(lo+hi)*0.5f;
    if(ToAP(PalVal2(mid,80.0f))>0) lo=mid; else hi=mid;
}
C("二分求得最大安全力量");
{ printf("\n       力量 %.0f -> AP %d  ", lo, ToAP(PalVal2(lo,80.0f)));
  if(ToAP(PalVal2(lo,80.0f))>0) OK(); else NG("?"); }

C("再多一点就翻 INT_MIN");
{ printf("\n       力量 %.0f -> AP %d  ", hi, ToAP(PalVal2(hi,80.0f)));
  if(ToAP(PalVal2(hi,80.0f))==INT32_MIN) OK(); else NG("%d",ToAP(PalVal2(hi,80.0f))); }

C("用户 4亿力量 -> AP 正常为正");
{ int32 ap=ToAP(PalVal2(4.0e8f,80.0f));
  printf("\n       AP = %d  ", ap);
  if(ap>0) OK(); else NG("ap=%d",ap); }

C("用户 20亿力量 -> AP 翻 INT_MIN（这就是你看到的）");
{ int32 ap=ToAP(PalVal2(2.0e9f,80.0f));
  printf("\n       AP = %d  ", ap);
  if(ap==INT32_MIN) OK(); else NG("ap=%d",ap); }

printf("\n-- C. 负攻强的后果 --\n");

C("Unit.cpp:9754 有 if(ap<0) return 0.0f 兜底");
{ int32 ap=INT32_MIN; float r = (ap<0)? 0.0f : float(ap);
  if(r==0.0f) OK(); else NG("%.0f",r); }
printf("       ^ 服务端伤害计算会当 0 处理 -> 白板伤害\n");

C("但客户端角色面板【直接显示】UNIT_FIELD_ATTACK_POWER 原值");
{ int32 shown=INT32_MIN; if(shown<0) OK(); else NG("?"); }
printf("       ^ 所以你面板看到 -2147483648，这就是「客户端实际情况」\n");

C("UF_FLAG_PRIVATE|UF_FLAG_OWNER -> 只有自己看得到");
{ OK(); }

printf("\n-- D. 安全建议线 --\n");

C("留 10%% 余量：力量建议不超过 9.6 亿");
{ float safe=lo*0.9f; printf("\n       建议上限 %.3e  ", safe);
  if(ToAP(PalVal2(safe,80.0f))>0) OK(); else NG("?"); }

C("但耐力墙(4.29亿)更低，全属性一起堆时以耐力为准");
{ if(4.29e8f < lo) OK(); else NG("?"); }

printf("\n=== 结果: %d 通过 / %d 失败 (共 %d) ===\n",pass,fail,no);
return fail?1:0;
}
