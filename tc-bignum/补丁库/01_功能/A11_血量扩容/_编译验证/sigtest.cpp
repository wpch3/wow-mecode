// ============================================================
//  step25 签名兼容性验证
//
//  改 ModifyHealth(int32)->int64 后，28 个调用点会不会出问题？
//  这里复刻真实调用形态，确认全部能编译且行为正确。
// ============================================================
#include <cstdio>
#include <cstdint>
#include <algorithm>
#include <limits>
typedef int32_t int32; typedef int64_t int64;
typedef uint32_t uint32; typedef uint64_t uint64;

static int no=0,pass=0,fail=0;
#define C(n) do{printf("  [%02d] %-52s ",++no,n);}while(0)
#define OK() do{printf("PASS\n");++pass;}while(0)
#define NG(f,...) do{printf("FAIL " f "\n",##__VA_ARGS__);++fail;}while(0)

static uint32 g_hp=1000, g_maxhp=1000;
static void SetHealth(uint32 v){ if(g_maxhp<v) v=g_maxhp; g_hp=v; }
static uint32 GetHealth(){ return g_hp; }
static uint32 GetMaxHealth(){ return g_maxhp; }

// step25 后的签名
static int64 ModifyHealth(int64 dVal)
{
    int64 gain=0;
    if(dVal==0) return 0;
    int64 curHealth=(int64)GetHealth();
    int64 val=dVal+curHealth;
    if(val<=0){ SetHealth(0); return -curHealth; }
    int64 maxHealth=(int64)GetMaxHealth();
    if(val<maxHealth){ SetHealth((uint32)val); gain=val-curHealth; }
    else if(curHealth!=maxHealth){ SetHealth((uint32)maxHealth); gain=maxHealth-curHealth; }
    return gain;
}
static int64 GetHealthGain(int64 dVal)
{
    int64 gain=0;
    if(dVal==0) return 0;
    int64 curHealth=(int64)GetHealth();
    int64 val=dVal+curHealth;
    if(val<=0) return -curHealth;
    int64 maxHealth=(int64)GetMaxHealth();
    if(val<maxHealth) gain=dVal;
    else if(curHealth!=maxHealth) gain=maxHealth-curHealth;
    return gain;
}

int main(){
printf("=== step25 签名兼容性 ===\n\n");

printf("-- A. 28个调用点的真实形态 --\n");

C("me->ModifyHealth(-5)                  字面量");
{ g_maxhp=1000; g_hp=500; ModifyHealth(-5);
  if(g_hp==495) OK(); else NG("%u",g_hp); }

C("ModifyHealth(int32(CountPctFromMaxHealth(3)))  int32强转");
{ g_maxhp=1000; g_hp=500; int32 v=30; ModifyHealth(int32(v));
  if(g_hp==530) OK(); else NG("%u",g_hp); }

C("ModifyHealth(-(int32)damage)          负int32");
{ g_maxhp=1000; g_hp=500; uint32 dmg=100; ModifyHealth(-(int32)dmg);
  if(g_hp==400) OK(); else NG("%u",g_hp); }

C("ModifyHealth(-(int64)damage)          负int64（step25新）");
{ g_maxhp=1000; g_hp=500; uint32 dmg=100; ModifyHealth(-(int64)dmg);
  if(g_hp==400) OK(); else NG("%u",g_hp); }

C("ModifyHealth(-std::min<int32>(a,b))   模板推导");
{ g_maxhp=1000; g_hp=500; int32 dmg=50, hp=499;
  ModifyHealth(-std::min<int32>(dmg, hp));
  if(g_hp==450) OK(); else NG("%u",g_hp); }

C("ModifyHealth(missingHealth)           int32变量隐式提升");
{ g_maxhp=1000; g_hp=500; int32 missing=200; ModifyHealth(missing);
  if(g_hp==700) OK(); else NG("%u",g_hp); }

C("ModifyHealth(amt) 其中 amt 是 int32");
{ g_maxhp=1000; g_hp=500; int32 amt=-100; ModifyHealth(amt);
  if(g_hp==400) OK(); else NG("%u",g_hp); }

C("std::max<int32>(1 - GetHealth(), amt) 混合表达式");
{ g_maxhp=1000; g_hp=500; int32 amt=100;
  ModifyHealth(std::max<int32>(1 - (int32)GetHealth(), amt));
  if(g_hp==600) OK(); else NG("%u",g_hp); }

printf("\n-- B. 返回值使用形态 --\n");

C("int64 gain = ModifyHealth(...)        新形态");
{ g_maxhp=1000; g_hp=500; int64 gain=ModifyHealth(200);
  if(gain==200) OK(); else NG("%lld",(long long)gain); }

C("OnHeal 转接：int64 -> uint32 钳制");
{ int64 gain=5000000000LL;   // 50亿，超 uint32
  uint32 hook = (gain>0) ? uint32(std::min<int64>(gain, std::numeric_limits<uint32>::max())) : 0u;
  if(hook==4294967295u) OK(); else NG("%u",hook); }

C("OnHeal 转接：正常值原样传");
{ int64 gain=12345; 
  uint32 hook = (gain>0) ? uint32(std::min<int64>(gain, std::numeric_limits<uint32>::max())) : 0u;
  if(hook==12345u) OK(); else NG("%u",hook); }

C("OnHeal 转接：负gain -> 0");
{ int64 gain=-500;
  uint32 hook = (gain>0) ? uint32(std::min<int64>(gain, std::numeric_limits<uint32>::max())) : 0u;
  if(hook==0u) OK(); else NG("%u",hook); }

C("[危险验证] 若直接 (uint32&)int64 会读错");
{ int64 gain=0x0000000100000005LL;   // 高位有值
  uint32 wrong = *(uint32*)&gain;    // 小端只读低4字节
  if(wrong==5) OK(); else NG("%u",wrong); }
printf("       ^ 所以必须用临时变量转接，不能强转引用\n");

C("uint32(-GetHealthGain(-damage) * mult) 生命偷取形态");
{ g_maxhp=1000; g_hp=500; int32 damage=200; float mult=0.5f;
  uint32 healthGain = uint32(-GetHealthGain(-damage) * mult);
  if(healthGain==100) OK(); else NG("%u",healthGain); }

printf("\n-- C. 大数值：这才是修复目的 --\n");

C("[核心] 血上限40亿，治疗30亿 -> 一次到位");
{ g_maxhp=4000000000u; g_hp=1000000000u;
  ModifyHealth(3000000000LL);
  printf("\n       血量 %u  ",g_hp);
  if(g_hp==4000000000u) OK(); else NG("%u",g_hp); }
printf("       ^ 修前参数int32，30亿被截断成负数 -> 暴毙\n");

C("[核心] 回血量 25亿 不被截断");
{ g_maxhp=4000000000u; g_hp=100;
  int64 gain=ModifyHealth(2500000000LL);
  printf("\n       实回 %lld  ",(long long)gain);
  if(gain==2500000000LL) OK(); else NG("%lld",(long long)gain); }

C("[用户场景] 耐力4亿=血上限40亿，能回到满血");
{ g_maxhp=4000000000u; g_hp=2100000000u;   // 卡在21亿
  ModifyHealth(1900000000LL);
  printf("\n       血量 %u  ",g_hp);
  if(g_hp==4000000000u) OK(); else NG("%u",g_hp); }
printf("       ^ 这就是「测不到40亿血」的修复\n");

C("超上限仍正确钳制（不是无脑加）");
{ g_maxhp=4000000000u; g_hp=3999999000u;
  ModifyHealth(999999999LL);
  if(g_hp==4000000000u) OK(); else NG("%u",g_hp); }

C("致死伤害仍能杀死（没变无敌）");
{ g_maxhp=4000000000u; g_hp=1000;
  ModifyHealth(-5000LL);
  if(g_hp==0) OK(); else NG("%u",g_hp); }

printf("\n=== 结果: %d 通过 / %d 失败 (共 %d) ===\n",pass,fail,no);
return fail?1:0;
}
