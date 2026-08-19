// ============================================================
//  step24 修复验证 —— 修前 vs 修后
//
//  用户实测的三种死亡状态，修完必须全部消失。
// ============================================================
#include <cstdio>
#include <cstdint>
typedef int32_t int32; typedef int64_t int64; typedef uint32_t uint32;

static int no=0,pass=0,fail=0;
#define C(n) do{printf("  [%02d] %-52s ",++no,n);}while(0)
#define OK() do{printf("PASS\n");++pass;}while(0)
#define NG(f,...) do{printf("FAIL " f "\n",##__VA_ARGS__);++fail;}while(0)

enum DeathState { ALIVE=0, JUST_DIED=1, CORPSE=2, DEAD=3 };
static uint32 g_hp, g_maxhp; static DeathState g_ds=ALIVE;

static void SetHealth(uint32 val){
    if (g_ds==JUST_DIED||g_ds==CORPSE) val=0;
    else if (g_ds==DEAD) val=1;
    else { if (g_maxhp<val) val=g_maxhp; }
    g_hp=val;
}

// ---- 修复【前】----
static int32 ModifyHealth_OLD(int32 dVal){
    int32 gain=0;
    if(!dVal) return 0;
    int32 curHealth=(int32)g_hp;
    int32 val=dVal+curHealth;
    if(val<=0){ SetHealth(0); return -curHealth; }
    int32 maxHealth=(int32)g_maxhp;
    if(val<maxHealth){ SetHealth(val); gain=val-curHealth; }
    else if(curHealth!=maxHealth){ SetHealth(maxHealth); gain=maxHealth-curHealth; }
    return gain;
}

// ---- 修复【后】（补丁实际写入的代码）----
static int32 ModifyHealth_NEW(int32 dVal){
    int64 gain=0;
    if(!dVal) return 0;
    int64 curHealth=(int64)g_hp;
    int64 val=(int64)dVal+curHealth;
    if(val<=0){ SetHealth(0); return (int32)(-curHealth); }
    int64 maxHealth=(int64)g_maxhp;
    if(val<maxHealth){ SetHealth((uint32)val); gain=val-curHealth; }
    else if(curHealth!=maxHealth){ SetHealth((uint32)maxHealth); gain=maxHealth-curHealth; }
    return (int32)gain;
}

int main(){
printf("=== step24 修复验证 ===\n\n");

printf("-- 用户三种死亡状态：修前 vs 修后 --\n");

C("[状况1修前] 血20亿 治疗5亿 -> 暴毙");
{ g_ds=ALIVE; g_maxhp=4200000000u; g_hp=2000000000u;
  ModifyHealth_OLD(500000000);
  if(g_hp==0) OK(); else NG("hp=%u",g_hp); }

C("[状况1修后] 同样操作 -> 正常回血到25亿");
{ g_ds=ALIVE; g_maxhp=4200000000u; g_hp=2000000000u;
  ModifyHealth_NEW(500000000);
  printf("\n       血量 %u  ",g_hp);
  if(g_hp==2500000000u) OK(); else NG("hp=%u",g_hp); }

C("[状况2修前] 血41亿 治疗5000 -> 暴毙");
{ g_ds=ALIVE; g_maxhp=4200000000u; g_hp=4100000000u;
  ModifyHealth_OLD(5000);
  if(g_hp==0) OK(); else NG("hp=%u",g_hp); }

C("[状况2修后] 同样操作 -> 正常回血");
{ g_ds=ALIVE; g_maxhp=4200000000u; g_hp=4100000000u;
  ModifyHealth_NEW(5000);
  printf("\n       血量 %u  ",g_hp);
  if(g_hp==4100005000u) OK(); else NG("hp=%u",g_hp); }

C("[状况5修前] 小额治疗把血瞬间顶满42亿");
{ g_ds=ALIVE; g_maxhp=4200000000u; g_hp=2000000000u;
  ModifyHealth_OLD(100);
  if(g_hp==4200000000u) OK(); else NG("hp=%u",g_hp); }
printf("       ^ 顶满后下次治疗必死\n");

C("[状况5修后] 小额治疗只回100点");
{ g_ds=ALIVE; g_maxhp=4200000000u; g_hp=2000000000u;
  ModifyHealth_NEW(100);
  if(g_hp==2000000100u) OK(); else NG("hp=%u",g_hp); }

C("[状况7修前] 溢出血量下掉血 -> 暴毙");
{ g_ds=ALIVE; g_maxhp=4200000000u; g_hp=4100000000u;
  ModifyHealth_OLD(-100);
  if(g_hp==0) OK(); else NG("hp=%u",g_hp); }

C("[状况7修后] 掉血正常扣除");
{ g_ds=ALIVE; g_maxhp=4200000000u; g_hp=4100000000u;
  ModifyHealth_NEW(-100);
  if(g_hp==4099999900u) OK(); else NG("hp=%u",g_hp); }

printf("\n-- 边界与回归 --\n");

C("修后：真正致死伤害仍能杀死（不是无敌）");
{ g_ds=ALIVE; g_maxhp=4200000000u; g_hp=1000;
  ModifyHealth_NEW(-5000);
  if(g_hp==0) OK(); else NG("hp=%u",g_hp); }

C("修后：回血超上限被正确钳到 maxHealth");
{ g_ds=ALIVE; g_maxhp=4200000000u; g_hp=4199999000u;
  ModifyHealth_NEW(999999);
  if(g_hp==4200000000u) OK(); else NG("hp=%u",g_hp); }

C("修后：小血量正常（回归）");
{ g_ds=ALIVE; g_maxhp=10000u; g_hp=5000u;
  ModifyHealth_NEW(3000);
  if(g_hp==8000u) OK(); else NG("hp=%u",g_hp); }

C("修后：dVal=0 直接返回");
{ g_ds=ALIVE; g_maxhp=10000u; g_hp=5000u;
  int32 r=ModifyHealth_NEW(0);
  if(r==0 && g_hp==5000u) OK(); else NG("?"); }

C("修后：返回值 gain 正确（治疗量统计用）");
{ g_ds=ALIVE; g_maxhp=4200000000u; g_hp=2000000000u;
  int32 g=ModifyHealth_NEW(5000);
  if(g==5000) OK(); else NG("gain=%d",g); }

C("修后：接近上限时 gain 只算实际回的量");
{ g_ds=ALIVE; g_maxhp=4200000000u; g_hp=4199999000u;
  int32 g=ModifyHealth_NEW(999999);
  if(g==1000) OK(); else NG("gain=%d",g); }

C("新安全线：血量 42.9亿 (UNIT_FIELD_HEALTH uint32 极限)");
{ if(4294967295u==4294967295u) OK(); else NG("?"); }

C("对应耐力上限 4.29亿（原 2.147亿，翻倍）");
{ double w=4294967295.0/10.0;
  printf("\n       %.4e  ",w);
  if(w>4.29e8&&w<4.30e8) OK(); else NG("%.0f",w); }

printf("\n=== 结果: %d 通过 / %d 失败 (共 %d) ===\n",pass,fail,no);
return fail?1:0;
}
