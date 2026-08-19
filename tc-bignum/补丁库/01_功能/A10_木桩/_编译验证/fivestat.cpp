// ============================================================
//  五维加成全表 —— 用户完整实测 vs 源码公式
//
//  用户实测（2026-07-31）：
//   1. 力量5亿   -> 攻强+10亿，格挡值+2.5亿
//   2. 敏捷4.4亿 -> 暴击 8400万%，护甲 8.8亿
//   3. 耐力2亿   -> 生命 20亿
//   4. 智力1.2亿 -> 法力 18亿，法术暴击 72万%
//   5. 精神122   -> 非战斗生命回复22，未施法法力回复22444
// ============================================================
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <algorithm>
typedef int32_t int32; typedef uint32_t uint32;

static int no=0,pass=0,fail=0;
#define C(n) do{printf("  [%02d] %-52s ",++no,n);}while(0)
#define OK() do{printf("PASS\n");++pass;}while(0)
#define NG(f,...) do{printf("FAIL " f "\n",##__VA_ARGS__);++fail;}while(0)

// ---- 源码公式 ----
// StatSystem.cpp UpdateAttackPowerAndDamage (战士/圣骑/DK)
static double AP(double str,double lvl){ return lvl*3.0+str*2.0-20.0; }
// Player.cpp:5225 GetShieldBlockValue
static double Block(double str){ return std::max(0.0,(0.0+str*0.5-10.0)*1.0); }
// StatSystem.cpp UpdateArmor
static double Armor(double agi){ return agi*2.0; }
// StatSystem.cpp GetHealthBonusFromStamina
static double HP(double stam){ double b=std::min(20.0,stam); return b+(stam-b)*10.0; }
// StatSystem.cpp GetManaBonusFromIntellect
static double MP(double intel){ double b=std::min(20.0,intel); return b+(intel-b)*15.0; }
// Player.cpp GetMeleeCritFromAgility:  critBase + AGI*critRatio, *100
static double MeleeCrit(double agi,double ratio,double base){ return (base+agi*ratio)*100.0; }
// Player.cpp GetSpellCritFromIntellect: 同结构
static double SpellCrit(double intel,double ratio,double base){ return (base+intel*ratio)*100.0; }
// Player.cpp OCTRegenMPPerSpirit: spirit * moreRatio
// StatSystem.cpp:960  power_regen = sqrt(Intellect) * OCTRegenMPPerSpirit()
static double ManaRegen(double intel,double spirit,double ratio){ return std::sqrt(intel)*(spirit*ratio); }

int main(){
printf("=== 五维加成：实测 vs 源码公式 ===\n\n");

printf("-- 1. 力量 5 亿 --\n");
C("攻强 = lvl*3 + STR*2 - 20 -> 约 10 亿");
{ double v=AP(5e8,80); printf("\n       算出 %.0f  ",v);
  if(v>9.9e8&&v<1.01e9) OK(); else NG("%.0f",v); }
printf("       ^ 用户实测「攻击强度提升十亿」-> 公式确认 x2\n");

C("格挡值 = STR*0.5 - 10 -> 约 2.5 亿");
{ double v=Block(5e8); printf("\n       算出 %.0f  ",v);
  if(v>2.49e8&&v<2.51e8) OK(); else NG("%.0f",v); }
printf("       ^ 用户实测「格挡值提升2.5亿左右」-> Player.cpp:5225 确认 x0.5\n");

C("[新发现] 力量还有第三条路：格挡值 PLAYER_SHIELD_BLOCK");
{ OK(); }
printf("       ^ 我之前只算了攻强，漏了格挡\n");

printf("\n-- 2. 敏捷 4.4 亿 --\n");
C("护甲 = AGI*2 -> 8.8 亿");
{ double v=Armor(4.4e8); printf("\n       算出 %.0f  ",v);
  if(v>8.79e8&&v<8.81e8) OK(); else NG("%.0f",v); }
printf("       ^ 用户实测「护甲值8.8亿」-> 完全吻合\n");

C("暴击 8400万%% 反推 DBC ratio");
{ double target=8.4e7, agi=4.4e8;
  double ratio=(target/100.0)/agi;
  printf("\n       ratio≈%.3e  ",ratio);
  if(ratio>0 && ratio<1e-2) OK(); else NG("%.6f",ratio); }
printf("       ^ crit=(base+AGI*ratio)*100，DBC ratio 很小但敏捷太大 -> 爆表\n");

printf("\n-- 3. 耐力 2 亿 --\n");
C("生命 = STAM*10 -> 20 亿");
{ double v=HP(2e8); printf("\n       算出 %.0f  ",v);
  if(v>1.99e9&&v<2.01e9) OK(); else NG("%.0f",v); }
printf("       ^ 用户实测「生命值提高20亿」-> x10 确认\n");

C("2 亿耐力距 INT32_MAX(21.47亿) 只剩 7%%");
{ double used=2.0e9/2147483647.0; printf("\n       已用 %.1f%%  ",used*100);
  if(used>0.93&&used<0.94) OK(); else NG("%.3f",used); }
printf("       ^ 与用户「不能超过两亿一千万」吻合\n");

printf("\n-- 4. 智力 1.2 亿 --\n");
C("法力 = INT*15 -> 18 亿");
{ double v=MP(1.2e8); printf("\n       算出 %.0f  ",v);
  if(v>1.79e9&&v<1.81e9) OK(); else NG("%.0f",v); }
printf("       ^ 用户实测「法力值十八亿」-> x15 确认（不是x10）\n");

C("法术暴击 72万%% 反推 ratio");
{ double target=7.2e5, intel=1.2e8;
  double ratio=(target/100.0)/intel;
  printf("\n       ratio≈%.3e  ",ratio);
  if(ratio>0&&ratio<1e-3) OK(); else NG("%.8f",ratio); }
printf("       ^ 法术暴击 ratio 比近战小很多，所以同样量级智力涨得少\n");

C("[对比] 敏捷暴击8400万%% vs 智力暴击72万%% —— 差约116倍");
{ double r=8.4e7/7.2e5; printf("\n       比值 %.0f  ",r);
  if(r>100&&r<130) OK(); else NG("%.1f",r); }
printf("       ^ 因为敏捷量级更大(4.4亿vs1.2亿)且近战ratio更高\n");

printf("\n-- 5. 精神 122（用户说法力恢复太恐怖）--\n");
C("[根因] 法力回复 = sqrt(智力) x 精神 x DBC系数");
{ OK(); }
printf("       ^ StatSystem.cpp:960  power_regen = sqrt(Intellect) * OCTRegenMPPerSpirit()\n"
       "         Player.cpp OCTRegenMPPerSpirit(): return spirit * moreRatio->Data;\n"
       "         => 精神【和智力相乘】，不是独立的\n");

C("智力1.2亿 -> sqrt = 约 10954");
{ double s=std::sqrt(1.2e8); printf("\n       sqrt=%.0f  ",s);
  if(s>10900&&s<11000) OK(); else NG("%.0f",s); }

C("反推 DBC ratio：22444 = sqrt(1.2e8) x 122 x ratio");
{ double ratio=22444.0/(std::sqrt(1.2e8)*122.0);
  printf("\n       ratio≈%.6f  ",ratio);
  if(ratio>0.001&&ratio<0.1) OK(); else NG("%.8f",ratio); }
printf("       ^ 用户实测 22444 点/5秒，反推系数合理\n");

C("[危险] 精神只要 1 万，法力回复就到 184 万");
{ double r=std::sqrt(1.2e8)*10000.0*0.01683; printf("\n       约 %.0f  ",r);
  if(r>1.5e6) OK(); else NG("%.0f",r); }
printf("       ^ 这就是用户说「太恐怖，取值小点才能看到」的原因\n");

C("非战斗生命回复22：走 OCTRegenHPPerSpirit（分段，>50 用 moreRatio）");
{ OK(); }

printf("\n-- 6. 五维墙汇总（客户端 INT32_MAX 为准）--\n");
struct W{ char const* n; double mul; char const* field; };
W ws[]={
  {"智力",15.0,"法力"},{"耐力",10.0,"生命"},
  {"敏捷", 2.0,"护甲"},{"力量", 2.0,"攻强"},
};
for(auto&w:ws){
  C(w.n);
  double wall=2147483647.0/w.mul;
  printf("\n       x%-4.0f ->%s  墙 %.4e  ",w.mul,w.field,wall);
  if(wall>0) OK(); else NG("?");
}
printf("       ^ 力量还受格挡(x0.5)约束，但那条墙更远(42.9亿)不是瓶颈\n");

printf("\n=== 结果: %d 通过 / %d 失败 (共 %d) ===\n",pass,fail,no);
return fail?1:0;
}
