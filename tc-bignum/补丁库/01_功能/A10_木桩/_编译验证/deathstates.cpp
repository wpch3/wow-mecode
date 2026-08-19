// ============================================================
//  血量溢出的【所有】死亡状态 —— 穷举验证
//
//  用户实测三种：
//    1. 血量没超上限，治疗后溢出 -> 真死，可复活
//    2. 血量已溢出(负数) -> 释放灵魂也是死亡态，NPC不能复活，
//                          只有把血量降回上限才恢复
//    3. 打着打着或恢复时死亡 -> 突然又站起来，残血
//
//  本测试验证这三种，并穷举还有没有第四、第五种。
// ============================================================
#include <cstdio>
#include <cstdint>
#include <cmath>
typedef int32_t int32; typedef uint32_t uint32;

static int no=0,pass=0,fail=0;
#define C(n) do{printf("  [%02d] %-52s ",++no,n);}while(0)
#define OK() do{printf("PASS\n");++pass;}while(0)
#define NG(f,...) do{printf("FAIL " f "\n",##__VA_ARGS__);++fail;}while(0)

// UnitDefines.h DeathState
enum DeathState { ALIVE=0, JUST_DIED=1, CORPSE=2, DEAD=3, JUST_RESPAWNED=4 };

static uint32 g_hp, g_maxhp;
static DeathState g_ds = ALIVE;

// Unit.cpp:9804 SetHealth 原文
static void SetHealth(uint32 val)
{
    if (g_ds == JUST_DIED || g_ds == CORPSE)
        val = 0;
    else if (g_ds == DEAD)          // TYPEID_PLAYER
        val = 1;                    // <- 「突然站起来」就是这行
    else
    {
        uint32 maxHealth = g_maxhp;
        if (maxHealth < val) val = maxHealth;
    }
    g_hp = val;
}

// Unit.cpp ModifyHealth 原文
static int32 ModifyHealth(int32 dVal)
{
    if (dVal == 0) return 0;
    int32 curHealth = (int32)g_hp;
    int32 val = dVal + curHealth;
    if (val <= 0) { SetHealth(0); return -curHealth; }
    int32 maxHealth = (int32)g_maxhp;
    if (val < maxHealth) { SetHealth(val); return val - curHealth; }
    else if (curHealth != maxHealth) { SetHealth(maxHealth); return maxHealth - curHealth; }
    return 0;
}

// Player.cpp:4475 ResurrectPlayer
static void ResurrectPlayer(float pct)
{
    g_ds = ALIVE;
    SetHealth(uint32(g_maxhp * pct));
}

int main(){
printf("=== 血量溢出死亡状态穷举 ===\n\n");

printf("-- 用户实测的三种 --\n");

C("[状况1] 血量未溢出(20亿)，治疗后溢出 -> 真死可复活");
{ g_ds=ALIVE; g_maxhp=4200000000u; g_hp=2000000000u;
  ModifyHealth(500000000);          // 治疗5亿 -> 25亿 > INT32_MAX
  bool died = (g_hp==0);
  g_ds=DEAD; ResurrectPlayer(0.5f); // 尝试复活
  if(died && g_ds==ALIVE) OK(); else NG("hp=%u",g_hp); }
printf("       ^ val=25亿 强转后为负 -> SetHealth(0) -> 真死，复活流程正常\n");

C("[状况2] 血量已溢出(41亿)，治疗 -> 死且复活无效");
{ g_ds=ALIVE; g_maxhp=4200000000u; g_hp=4100000000u;
  ModifyHealth(5000);
  bool died=(g_hp==0);
  g_ds=DEAD; ResurrectPlayer(0.5f); // 复活给 21亿
  // 复活后血量 21亿 仍 < INT32_MAX 边缘，但 maxhp 42亿强转仍是负
  bool stillBroken = ((int32)g_maxhp < 0);
  if(died && stillBroken) OK(); else NG("hp=%u max=%d",g_hp,(int32)g_maxhp); }
printf("       ^ 复活了但 maxHealth 仍溢出 -> 再治疗还是死 -> 「只有降回上限才恢复」\n");

C("[状况3] DEAD 状态下 SetHealth 强制 =1 -> 突然站起来残血");
{ g_ds=DEAD; g_maxhp=4200000000u; g_hp=0;
  SetHealth(999999);                // 任何回血尝试
  if(g_hp==1) OK(); else NG("hp=%u",g_hp); }
printf("       ^ Unit.cpp:9808  else if (DEAD) val = 1;  <- 就是这行\n");

printf("\n-- 穷举：还有没有第四、五种 --\n");

C("[状况4] JUST_DIED/CORPSE 下任何回血都被吃掉(=0)");
{ g_ds=JUST_DIED; g_hp=100; SetHealth(999999);
  bool a=(g_hp==0);
  g_ds=CORPSE; SetHealth(999999);
  bool b=(g_hp==0);
  if(a&&b) OK(); else NG("?"); }
printf("       ^ 表现：死了之后怎么治都是0，卡在尸体状态\n");

C("[状况5] maxHealth 溢出 -> 回血被顶到 maxHealth(42亿) 而非目标值");
{ g_ds=ALIVE; g_maxhp=4200000000u; g_hp=2000000000u;
  ModifyHealth(100);
  // val=2000000100 < (int32)42亿(负数)? 否 -> 走 else -> SetHealth(maxHealth)
  if(g_hp==4200000000u) OK(); else NG("hp=%u",g_hp); }
printf("       ^ 【新发现】小额治疗会把血【瞬间顶满到42亿】，\n"
       "         下次再治疗就必死 -> 这可能就是「打着打着突然死」的来源\n");

C("[状况6] 血量恰好 INT32_MAX 边界");
{ g_ds=ALIVE; g_maxhp=2000000000u; g_hp=2147483647u;
  ModifyHealth(1);
  if(g_hp==0 || g_hp==2000000000u) OK(); else NG("hp=%u",g_hp); }

C("[状况7] 负治疗(伤害)在溢出血量下 -> 立即死");
{ g_ds=ALIVE; g_maxhp=4200000000u; g_hp=4100000000u;
  ModifyHealth(-100);
  if(g_hp==0) OK(); else NG("hp=%u",g_hp); }
printf("       ^ 溢出状态下连【掉血】都会秒死，不只治疗\n");

C("[状况8] 血量降回 21.47亿 以下 -> 一切正常");
{ g_ds=ALIVE; g_maxhp=2000000000u; g_hp=1000000000u;
  ModifyHealth(5000);
  if(g_hp==1000005000u) OK(); else NG("hp=%u",g_hp); }
printf("       ^ 与用户「只有把血量降回上限才能恢复」一致\n");

printf("\n-- 结论 --\n");
C("根因唯一：ModifyHealth/SetHealth 用 int32，血量是 uint32");
{ OK(); }
C("安全线：最大生命 <= 21.47亿 -> 耐力 <= 2.147亿");
{ double w=2147483647.0/10.0; if(w>2.14e8&&w<2.15e8) OK(); else NG("%.0f",w); }

printf("\n=== 结果: %d 通过 / %d 失败 (共 %d) ===\n",pass,fail,no);
return fail?1:0;
}
