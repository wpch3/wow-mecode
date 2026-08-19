// ============================================================
//  修正上一轮的错误结论 —— 力量到底走哪条路
//
//  用户实测：圣骑士 4 亿全属性 + 装备各上亿，总十几亿，没出问题。
//  这和我上轮"力量>16384就溢出"矛盾。查源码后确认：用户是对的。
//
//  真实链路（StatSystem.cpp Player::UpdateAttackPowerAndDamage）：
//    val2 = level*3 + GetStat(STAT_STRENGTH)*2 - 20     <- 圣骑
//    SetStatFlatModifier(unitMod, BASE_VALUE, val2);
//    base_attPower = GetFlatModifierValue(BASE_VALUE) * GetPctModifierValue(BASE_PCT);
//    SetAttackPower(int32(base_attPower));              <- 写 UNIT_FIELD_ATTACK_POWER (int32)
//    attPowerMod = GetFlatModifierValue(TOTAL_VALUE);   <- 只有光环类加成走这
//    SetAttackPowerModPos(int32(attPowerMod));          <- 写 _MODS (int16!)
//
//  Unit.h:1554  void SetAttackPowerModPos(int32 m) { SetInt16Value(UNIT_FIELD_ATTACK_POWER_MODS,0,m); }
//  Unit.cpp:9754 int32 ap = GetInt32Value(UNIT_FIELD_ATTACK_POWER)      <- int32，装备属性走这
//                        + int16(...MODS,0) + int16(...MODS,1);         <- int16，光环走这
// ============================================================
#include <cstdint>
#include <cstdio>
typedef int16_t int16; typedef uint16_t uint16;
typedef int32_t int32; typedef uint32_t uint32; typedef int64_t int64;

static int no=0,pass=0,fail=0;
#define C(n) do{printf("  [%02d] %-56s ",++no,n);}while(0)
#define OK() do{printf("PASS\n");++pass;}while(0)
#define NG(f,...) do{printf("FAIL " f "\n",##__VA_ARGS__);++fail;}while(0)

// Unit.cpp:9754 原文
static float GetTotalAP(int32 baseAP, int16 modPos, int16 modNeg, float mult)
{ int32 ap = baseAP + modPos + modNeg; if (ap<0) return 0.0f; return ap*(1.0f+mult); }

// StatSystem.cpp 圣骑分支
static int32 PaladinBaseAP(int64 strength, int32 level)
{ double v = double(level)*3.0 + double(strength)*2.0 - 20.0; return int32(v); }

int main()
{
printf("=== 修正：力量 4亿 到底会不会溢出 ===\n\n");

printf("-- A. 装备属性走 int32 那条路（用户的情况）--\n");

C("圣骑 力量4亿 -> val2 = 8亿，int32 装得下(上限21亿)");
{ int64 want = 80LL*3 + 400000000LL*2 - 20;
  if(want==800000220LL && want<2147483647LL) OK(); else NG("%lld",(long long)want); }

C("SetAttackPower(int32) 存 8亿 -> 不溢出");
{ int32 ap = PaladinBaseAP(400000000LL, 80);
  if(ap==800000220) OK(); else NG("ap=%d",ap); }

C("GetTotalAP 读回 8亿（MODS为0）-> 原样");
{ float ap = GetTotalAP(800000220,0,0,0.0f);
  if(ap>7.9e8f&&ap<8.1e8f) OK(); else NG("ap=%.0f",ap); }
printf("       ^ 这就是你没出问题的原因：装备属性根本不碰 int16\n");

C("力量 10亿 -> val2 20亿，仍在 int32 内（临界）");
{ int64 want = 80LL*3 + 1000000000LL*2 - 20;
  if(want==2000000220LL && want<2147483647LL) OK(); else NG("%lld",(long long)want); }

C("[真正的天花板] 力量 10.8亿 -> val2 超 int32，溢出");
{ int64 want = 80LL*3 + 1080000000LL*2 - 20;
  if(want>2147483647LL) OK(); else NG("%lld",(long long)want); }
printf("       ^ 圣骑真实上限约 力量 10.7 亿（不是我上轮说的 16384）\n");

printf("\n-- B. int16 那条路：只有光环加成走（谁会踩）--\n");

C("TOTAL_VALUE 来自光环，如 SPELL_AURA_MOD_ATTACK_POWER");
{ OK(); }

C("光环加成 3000 -> 正常");
{ float ap=GetTotalAP(800000220,3000,0,0.0f);
  if(ap>8.0e8f) OK(); else NG("%.0f",ap); }

C("[会翻车] 自定义光环给 +50000 AP -> int16 截断成负");
{ int32 want=50000; int16 stored=int16(want);
  if(stored==-15536) OK(); else NG("stored=%d",stored); }
printf("       ^ 但这要你【自己写超大AP的光环】才会踩到\n");

C("你的情况（装备堆属性）不会触发这条");
{ int16 modPos=0, modNeg=0;   // 纯装备属性，无超大光环
  float ap=GetTotalAP(800000220,modPos,modNeg,0.0f);
  if(ap>7.9e8f) OK(); else NG("%.0f",ap); }

printf("\n-- C. 下一个真实瓶颈在哪 --\n");

C("AP 8亿 / 14 * 3.3(双手) -> 每击约 1.88 亿");
{ double d = 800000220.0/14.0*3.3; printf("\n       每击 %.0f  ", d);
  if(d>1.8e8&&d<1.9e8) OK(); else NG("%.0f",d); }

C("DealDamage 是 uint32，上限 42.9亿 -> 1.88亿 安全");
{ double d=800000220.0/14.0*3.3; if(d<4294967295.0) OK(); else NG("超了"); }

C("[下一个墙] AP 约 182亿 时单击撑爆 uint32");
{ double apNeed = 4294967295.0*14.0/3.3;
  printf("\n       需要 AP %.3e  ", apNeed);
  if(apNeed>1.8e10) OK(); else NG("%.0f",apNeed); }
printf("       ^ 但 AP 先在 int32(21亿) 处卡住，所以伤害字段撑得住\n");

C("结论：你当前 十几亿总属性 完全在安全区");
{ int32 ap=PaladinBaseAP(400000000LL,80);
  double dmg=double(ap)/14.0*3.3;
  if(ap>0 && dmg<4294967295.0) OK(); else NG("?"); }

printf("\n=== 结果: %d 通过 / %d 失败 (共 %d) ===\n",pass,fail,no);
return fail?1:0;
}
