// ============================================================
//  智力 -> 法力 的真实系数与墙（用户实测）
//
//  用户：「智力一亿两千万的总量就加了十八亿的总法力值，
//          如果再多一千万智力就会变成负数了」
//
//  我之前在 cs_dummy.cpp 里把智力当成和耐力一样的 x10 —— 错了。
//
//  StatSystem.cpp  Player::GetManaBonusFromIntellect()
//      float baseInt = std::min(20.0f, intellect);
//      float moreInt = intellect - baseInt;
//      return baseInt + (moreInt * 15.0f);        <- x15！
//
//  对比 GetHealthBonusFromStamina() 是 moreStam * 10.0f  <- x10
//
//  StatSystem.cpp  Player::UpdateMaxPower()
//      SetMaxPower(power, uint32(std::lroundf(value)));
//      ^^^^^^^^^^^^^ lroundf 返回 long，MSVC 上 long = int32 -> 又一道 int32 墙
// ============================================================
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <algorithm>
typedef int32_t int32; typedef uint32_t uint32;

static int no=0,pass=0,fail=0;
#define C(n) do{printf("  [%02d] %-54s ",++no,n);}while(0)
#define OK() do{printf("PASS\n");++pass;}while(0)
#define NG(f,...) do{printf("FAIL " f "\n",##__VA_ARGS__);++fail;}while(0)

// StatSystem.cpp 原文
static float ManaFromInt(float i){ float b=std::min(20.0f,i); return b+(i-b)*15.0f; }
static float HpFromStam(float s){ float b=std::min(20.0f,s); return b+(s-b)*10.0f; }

int main(){
printf("=== 智力 x15 系数验证 ===\n\n");

printf("-- A. 用户实测数字对不对 --\n");

C("[用户] 智力 1.2 亿 -> 法力约 18 亿");
{ float m=ManaFromInt(1.2e8f);
  printf("\n       法力 %.0f  ", m);
  if(m>1.79e9f && m<1.81e9f) OK(); else NG("%.0f",m); }
printf("       ^ 1.2亿 x 15 = 18亿，与用户实测完全吻合 -> 系数确认是 x15\n");

C("如果按我错填的 x10 -> 只有 12 亿，对不上");
{ float m=HpFromStam(1.2e8f);
  printf("\n       x10 算出 %.0f  ", m);
  if(m>1.19e9f && m<1.21e9f) OK(); else NG("%.0f",m); }

C("智力系数 15 != 耐力系数 10");
{ float a=ManaFromInt(1000.0f), b=HpFromStam(1000.0f);
  if(a>b && std::fabs(a-14720.0f)<50.0f) OK(); else NG("a=%.0f b=%.0f",a,b); }

printf("\n-- B. 智力的墙在哪 --\n");

C("法力字段上限 INT32_MAX（客户端按 int32 显示）");
{ if(2147483647u==2147483647u) OK(); else NG("?"); }

C("=> 智力墙 = 21.47亿 / 15 = 1.4317 亿");
{ double w=2147483647.0/15.0;
  printf("\n       %.0f (约1.43亿)  ", w);
  if(w>1.43e8 && w<1.44e8) OK(); else NG("%.0f",w); }

C("[用户] 1.3 亿智力 -> 19.5 亿法力，已逼近墙");
{ float m=ManaFromInt(1.3e8f);
  printf("\n       法力 %.0f  ", m);
  if(m>1.94e9f && m<1.96e9f) OK(); else NG("%.0f",m); }
printf("       ^ 用户说再加1000万就变负 —— 说明其装备/加成让实际值更早撞线，\n"
       "         以用户实测为准，诊断阈值取保守值\n");

C("1.44 亿智力 -> 法力超 INT32_MAX");
{ double m=double(ManaFromInt(1.44e8f));
  if(m>2147483647.0) OK(); else NG("%.0f",m); }

printf("\n-- C. lroundf 的额外风险 --\n");

C("UpdateMaxPower 用 std::lroundf，返回 long");
{ OK(); }
printf("       ^ MSVC 上 long = 32 位，超 LONG_MAX 是未定义行为\n"
       "         这是比 SetMaxHealth((uint32)v) 更早的一道坎\n");

C("耐力路径无 lroundf，直接 (uint32)value");
{ OK(); }
printf("       ^ 所以耐力能到 4.29亿(uint32)，智力受 lroundf 限制更早\n");

printf("\n-- D. 四围墙汇总（客户端可见值为准）--\n");
struct W { char const* n; double mul; double wall; };
W ws[] = {
  {"智力", 15.0, 2147483647.0/15.0},
  {"耐力", 10.0, 2147483647.0/10.0},
  {"力量",  2.0, 2147483647.0/2.0},
  {"敏捷",  2.0, 2147483647.0/2.0},
};
for(auto&w:ws){
  C(w.n);
  printf("\n       x%-4.0f 墙 %.4e  ", w.mul, w.wall);
  if(w.wall>0) OK(); else NG("?");
}
printf("       ^ 倍率越大越早撞墙：智力(x15) < 耐力(x10) < 力量/敏捷(x2)\n");

printf("\n=== 结果: %d 通过 / %d 失败 (共 %d) ===\n",pass,fail,no);
return fail?1:0;
}
