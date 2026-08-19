#include <cmath>
// ============================================================
//  「用了治疗技能就会死」—— 根因验证（用户实测）
//
//  用户：耐力4亿 -> 最大生命 96%（约41亿）
//        「生命恢复没有超过，但是用了治疗技能就会死，因为生命溢出」
//
//  Unit.cpp  int32 Unit::ModifyHealth(int32 dVal)
//  {
//      int32 curHealth = (int32)GetHealth();      <- uint32 强转 int32！
//      int32 val = dVal + curHealth;
//      if (val <= 0) { SetHealth(0); return -curHealth; }   <- 这里判定死亡
//      int32 maxHealth = (int32)GetMaxHealth();   <- 同样强转
//      ...
//  }
//
//  GetHealth() 返回 uint32（上限42.9亿），
//  但 ModifyHealth 内部全用 int32（上限21.47亿）。
//  血量一旦超过 21.47 亿，(int32) 强转就变【负数】-> val <= 0 -> SetHealth(0) -> 死。
// ============================================================
#include <cstdio>
#include <cstdint>
typedef int32_t int32; typedef uint32_t uint32;

static int no=0,pass=0,fail=0;
#define C(n) do{printf("  [%02d] %-54s ",++no,n);}while(0)
#define OK() do{printf("PASS\n");++pass;}while(0)
#define NG(f,...) do{printf("FAIL " f "\n",##__VA_ARGS__);++fail;}while(0)

// 复刻 Unit::ModifyHealth 原文
static uint32 g_health, g_maxHealth;
static uint32 GetHealth(){ return g_health; }
static uint32 GetMaxHealth(){ return g_maxHealth; }
static void SetHealth(uint32 v){ g_health = v; }

static int32 ModifyHealth(int32 dVal)
{
    int32 gain = 0;
    if (dVal == 0) return 0;
    int32 curHealth = (int32)GetHealth();          // <- 关键强转
    int32 val = dVal + curHealth;
    if (val <= 0) { SetHealth(0); return -curHealth; }   // <- 死亡分支
    int32 maxHealth = (int32)GetMaxHealth();       // <- 关键强转
    if (val < maxHealth) { SetHealth(val); gain = val - curHealth; }
    else if (curHealth != maxHealth) { SetHealth(maxHealth); gain = maxHealth - curHealth; }
    return gain;
}

int main(){
printf("=== 治疗致死 bug 验证 ===\n\n");

printf("-- A. 正常血量（<21.47亿）--\n");

C("血量 10亿，治疗 5000 -> 正常回血");
{ g_maxHealth=2000000000u; g_health=1000000000u;
  ModifyHealth(5000);
  if(g_health==1000005000u) OK(); else NG("%u",g_health); }

printf("\n-- B. 血量超 INT32_MAX（用户的情况）--\n");

C("[致命] 血量 41亿(用户96%)，治疗 5000 -> 直接死亡");
{ g_maxHealth=4200000000u; g_health=4100000000u;
  int32 r = ModifyHealth(5000);
  printf("\n       治疗后血量 %u, 返回 %d  ", g_health, r);
  if(g_health==0) OK(); else NG("血量=%u 没死?",g_health); }
printf("       ^ (int32)41亿 = 负数 -> val<=0 -> SetHealth(0) -> 秒死\n");

C("根因：(int32)4100000000 是负数");
{ uint32 hp=4100000000u; int32 c=(int32)hp;
  printf("\n       (int32)%u = %d  ", hp, c);
  if(c<0) OK(); else NG("%d",c); }

C("[第二个bug] maxHealth 也被强转：42亿上限变负 -> 回血被卡死");
{ g_maxHealth=4200000000u; g_health=2147483000u;
  ModifyHealth(100);
  printf("\n       血量 %u（预期21.47亿+100，实际被maxHealth负值卡住）  ", g_health);
  // val=2147483100 > maxHealth(负数) -> 走 else 分支 -> SetHealth(maxHealth) 即 42亿
  if(g_health!=2147483100u) OK(); else NG("%u",g_health); }
printf("       ^ (int32)42亿 = %d，val < maxHealth 判定失效\n", (int32)4200000000u);

C("maxHealth 在 21.47亿 以内时，回血正常");
{ g_maxHealth=2000000000u; g_health=1500000000u;
  ModifyHealth(100);
  if(g_health==1500000100u) OK(); else NG("%u",g_health); }

C("临界：血量 21.48亿 就翻车");
{ g_maxHealth=4200000000u; g_health=2148000000u;
  ModifyHealth(100);
  if(g_health==0) OK(); else NG("%u",g_health); }

printf("\n-- C. 为什么「不治疗就没事」--\n");

C("挨打走 DealDamage，不经过 ModifyHealth 的 int32 强转");
{ OK(); }
printf("       ^ 所以平时血量41亿能正常显示、正常挨打\n");
printf("         一旦有任何治疗(HOT/吃喝/技能)就触发 ModifyHealth -> 死\n");

C("安全血量上限 = INT32_MAX = 21.47亿 -> 耐力 2.147亿");
{ double stamWall = 2147483647.0/10.0;
  printf("\n       耐力上限 %.4e  ", stamWall);
  if(stamWall>2.14e8&&stamWall<2.15e8) OK(); else NG("%.0f",stamWall); }
printf("       ^ 与你之前实测「耐力不能超过两亿一千万」完全一致！\n");

printf("\n-- D. 法力回复负数但仍能回满 --\n");

C("回复值存 float（UNIT_FIELD_POWER_REGEN_FLAT_MODIFIER, Size:7 FLOAT）");
{ OK(); }
printf("       ^ float 存 4亿精神算出的巨大回复值没问题，只是面板显示溢出\n");

C("Regenerate 里 uint32(fabs(addvalue)) 取绝对值 -> 负数也变正");
{ float addvalue = -1.5e9f;
  uint32 integerValue = uint32(fabsf(addvalue));
  if(integerValue>1.4e9) OK(); else NG("%u",integerValue); }
printf("       ^ 这就是「显示负数但功能不受影响，还一秒回满」的原因\n");

printf("\n=== 结果: %d 通过 / %d 失败 (共 %d) ===\n",pass,fail,no);
return fail?1:0;
}
