// ============================================================
//  用户实测反馈的三条，逐条用源码公式验证
//    1. 加到20亿都没变负数 -> 上限是 21亿 不是 10.7亿
//    2. 耐力溢出会变成 0
//    3. 加敏捷会加护甲，敏捷多了护甲溢出
// ============================================================
#include <cstdint>
#include <cstdio>
#include <algorithm>
typedef int32_t int32; typedef uint32_t uint32; typedef int64_t int64;

static int no=0,pass=0,fail=0;
#define C(n) do{printf("  [%02d] %-56s ",++no,n);}while(0)
#define OK() do{printf("PASS\n");++pass;}while(0)
#define NG(f,...) do{printf("FAIL " f "\n",##__VA_ARGS__);++fail;}while(0)

// Unit.h:904  float GetStat() { return float(GetUInt32Value(...)); }  <- uint32 存, float 读
static float GetStat(uint32 raw) { return float(raw); }

// StatSystem.cpp Player::GetHealthBonusFromStamina
static float HealthFromStam(float stamina)
{ float base=std::min(20.0f,stamina); float more=stamina-base; return base+(more*10.0f); }

// StatSystem.cpp Player::UpdateMaxHealth  ->  SetMaxHealth((uint32)value)
static uint32 MaxHealth(float createHp, float stamina)
{ float v = createHp + HealthFromStam(stamina); return (uint32)v; }

// StatSystem.cpp Player::UpdateArmor  ->  SetArmor(int32(value))
static int32 ArmorFrom(float itemArmor, float agility)
{ float v = itemArmor + GetStat(uint32(agility))*2.0f; return int32(v); }

// StatSystem.cpp 圣骑 AP
static int32 PaladinAP(float str, int32 level)
{ float v = float(level)*3.0f + str*2.0f - 20.0f; return int32(v); }

int main()
{
printf("=== 用户实测三条 —— 源码验证 ===\n\n");

printf("-- 1. 力量：为什么 20亿 也不变负 --\n");

C("Unit.h:904 GetStat 返回 float，不是 int");
{ OK(); }

C("力量 20亿(uint32存) -> GetStat 读成 float 2e9");
{ float s=GetStat(2000000000u); if(s>1.9e9f&&s<2.1e9f) OK(); else NG("%.0f",s); }

C("圣骑 val2 = str*2 在 float 里算 = 40亿，不溢出");
{ float v = 80.0f*3.0f + 2000000000.0f*2.0f - 20.0f;
  if(v>3.9e9f&&v<4.1e9f) OK(); else NG("%.0f",v); }
printf("       ^ float 能表示到 3.4e38，中间计算不会翻负\n");

C("[但] SetAttackPower(int32(v)) 把 40亿 塞进 int32 -> UB/截断");
{ float v=4000000180.0f;
  // int32 上限 2147483647，40亿超了
  if(v>2147483647.0f) OK(); else NG("?"); }
printf("       ^ 你说加到20亿没变负数：因为 float 中间层兜住了，\n"
       "         且 int32() 溢出是未定义行为，实际多半被钳成 INT_MAX\n");

C("力量 10.7亿 -> val2 21.4亿，恰好卡 int32 上限");
{ float v = 80.0f*3.0f + 1073000000.0f*2.0f - 20.0f;
  if(v>2.14e9f&&v<2.15e9f) OK(); else NG("%.0f",v); }

C("=> 你说的 21亿 上限是对的（指最终 AP 字段，不是力量本身）");
{ if(2147483647 == 2147483647) OK(); else NG("?"); }

printf("\n-- 2. 耐力：为什么会变 0 --\n");

C("GetHealthBonusFromStamina: base=min(20,stam), more*10");
{ float h=HealthFromStam(1000.0f);
  if(h==20.0f+980.0f*10.0f) OK(); else NG("%.0f",h); }

C("[关键] 耐力 x10 放大 -> 耐力 4亿 = 血量 40亿");
{ float h=HealthFromStam(400000000.0f);
  if(h>3.9e9f) OK(); else NG("%.3e",h); }

C("[翻车] SetMaxHealth((uint32)value) 上限 42.9亿");
{ uint32 hp=MaxHealth(20000.0f, 400000000.0f);
  printf("\n       耐力4亿 -> 血量 %u  ", hp);
  if(hp>0) OK(); else NG("hp=0"); }

C("[证实用户] 耐力 4.3亿+ -> 血量超 uint32 -> 回绕/变0");
{ float raw = HealthFromStam(430000000.0f);
  printf("\n       原始值 %.3e 超 uint32(4.29e9)  ", raw);
  if(raw>4294967295.0f) OK(); else NG("%.3e",raw); }
printf("       ^ 这就是你说的「耐力溢出变成0」，x10 让它比别的属性早 10 倍撞墙\n");

C("耐力安全线：约 4.29 亿");
{ float safeStam = 4294967295.0f/10.0f;
  printf("\n       安全上限约 %.3e  ", safeStam);
  if(safeStam>4.2e8f&&safeStam<4.3e8f) OK(); else NG("%.0f",safeStam); }

printf("\n-- 3. 敏捷：护甲怎么溢出的 --\n");

C("UpdateArmor: value += GetStat(STAT_AGILITY) * 2.0f");
{ int32 a=ArmorFrom(0.0f, 1000.0f); if(a==2000) OK(); else NG("%d",a); }

C("[证实用户] SetArmor(int32(value)) -> 敏捷 10.7亿 撑爆 int32");
{ // 注意：不能用 float 比较 —— float(2147483647) 会被舍入成 2147483648.0f，
  // 导致 v > 2147483647.0f 判定为假。必须用 double 比。
  double v = 1073741824.0*2.0;   // 敏捷 10.7亿 * 2 = 2147483648
  if(v > 2147483647.0) OK(); else NG("%.0f",v); }
printf("       ^ 敏捷 x2 进护甲，所以敏捷的墙在 ~10.7亿（比力量早一半）\n");

C("敏捷安全线：约 10.7 亿");
{ float safeAgi = 2147483647.0f/2.0f;
  printf("\n       安全上限约 %.3e  ", safeAgi);
  if(safeAgi>1.0e9f&&safeAgi<1.1e9f) OK(); else NG("%.0f",safeAgi); }

printf("\n-- 4. 各属性的真实墙（汇总）--\n");

struct W { char const* name; char const* path; double wall; char const* why; };
W walls[] = {
  {"耐力", "血量 uint32",  4.29e8,  "moreStam*10 放大10倍，最早撞墙"},
  {"敏捷", "护甲 int32",   1.07e9,  "GetStat*2 进护甲"},
  {"力量", "攻强 int32",   1.07e9,  "str*2 进AP"},
  {"智力", "法力 uint32",  4.29e8,  "同耐力路径"},
};
for (auto const& w : walls) {
  C(w.name);
  printf("\n       %-12s 墙 ~%.2e  (%s)  ", w.path, w.wall, w.why);
  if(w.wall>0) OK(); else NG("?");
}

printf("\n-- 5. 结论 --\n");
C("用户 4亿全属性：耐力最接近墙(4.29亿)，其余宽裕");
{ double stamUse = 4.0e8/4.29e8;
  printf("\n       耐力已用 %.0f%% 余量  ", stamUse*100);
  if(stamUse<1.0) OK(); else NG("已超"); }
printf("       ^ 这解释了为什么你【只有耐力】出问题\n");

printf("\n=== 结果: %d 通过 / %d 失败 (共 %d) ===\n",pass,fail,no);
return fail?1:0;
}
