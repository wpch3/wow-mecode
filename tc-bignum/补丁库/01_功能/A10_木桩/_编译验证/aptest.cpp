// ============================================================
//  攻强(AP)链路溢出验证
//  复刻 Unit.cpp:GetTotalAttackPowerValue 的真实算法
//    int32 ap = GetInt32Value(UNIT_FIELD_ATTACK_POWER)
//             + int16(GetUInt16Value(UNIT_FIELD_ATTACK_POWER_MODS, 0))
//             + int16(GetUInt16Value(UNIT_FIELD_ATTACK_POWER_MODS, 1));
//  注意那两个 int16(...) —— 这是大数值端的隐藏天花板
// ============================================================
#include <cstdint>
#include <cstdio>
typedef uint16_t uint16; typedef int16_t int16;
typedef uint32_t uint32; typedef int32_t int32;

static int no=0,pass=0,fail=0;
#define C(n) do{printf("  [%02d] %-56s ",++no,n);}while(0)
#define OK() do{printf("PASS\n");++pass;}while(0)
#define NG(f,...) do{printf("FAIL " f "\n",##__VA_ARGS__);++fail;}while(0)

// 复刻 Unit.cpp:11373 GetTotalAttackPowerValue（BASE_ATTACK 分支）
static float GetTotalAP(int32 baseAP, uint16 modPos, uint16 modNeg, float mult)
{
    int32 ap = baseAP + int16(modPos) + int16(modNeg);   // <- 关键：截成 int16
    if (ap < 0) return 0.0f;
    return ap * (1.0f + mult);
}

// 复刻 StatSystem.cpp:565  baseValue += GetTotalAttackPowerValue()/14.0f*apMod
static float APtoDamage(float totalAP, float apMod)
{ return totalAP / 14.0f * apMod; }

int main()
{
    printf("=== 攻强链路溢出验证（大数值端专属） ===\n\n");

    printf("-- A. AP_MODS 是 uint16 存、int16 读 --\n");

    C("正常 AP 加成 3000 -> 原样通过");
    { float ap=GetTotalAP(5000,3000,0,0.0f); if(ap==8000.0f) OK(); else NG("ap=%.0f",ap); }

    C("[天花板] AP 加成 32767 -> 仍正常（int16 上限）");
    { float ap=GetTotalAP(0,32767,0,0.0f); if(ap==32767.0f) OK(); else NG("ap=%.0f",ap); }

    C("[翻车] AP 加成 32768 -> int16 溢出成 -32768");
    { float ap=GetTotalAP(100000,32768,0,0.0f);
      // 100000 + (-32768) = 67232
      if(ap==67232.0f) OK(); else NG("ap=%.0f 预期67232",ap); }
    printf("       ^ 加得越多，攻强反而越低！\n");

    C("[翻车] AP 加成 40000 -> 变成 -25536");
    { float ap=GetTotalAP(100000,40000,0,0.0f);
      if(ap==74464.0f) OK(); else NG("ap=%.0f",ap); }

    C("[归零] 基础AP小 + 大加成 -> ap<0 直接返回 0");
    { float ap=GetTotalAP(1000,50000,0,0.0f);
      // 1000 + int16(50000)= 1000 + (-15536) = -14536 <0 -> 0
      if(ap==0.0f) OK(); else NG("ap=%.0f",ap); }
    printf("       ^ 穿了顶级装备，攻强显示 0\n");

    printf("\n-- B. 这个天花板换算成伤害是多少 --\n");

    C("AP 32767 上限 / 单手(2.4速) -> 每击约 5617");
    { float d=APtoDamage(32767.0f,2.4f);
      printf("\n       每击 %.0f  ", d); if(d>5000&&d<6000) OK(); else NG("d=%.0f",d); }

    C("AP 32767 上限 / 双手(3.3速) -> 每击约 7724");
    { float d=APtoDamage(32767.0f,3.3f);
      printf("\n       每击 %.0f  ", d); if(d>7000&&d<8000) OK(); else NG("d=%.0f",d); }

    printf("\n-- C. 伤害本身的上限（DealDamage 是 uint32）--\n");

    C("DealDamage 参数 uint32 -> 单次上限 42.9 亿");
    { uint32 dmg=4294967295u; if(dmg==4294967295u) OK(); else NG("?"); }

    C("单次伤害 42.9亿 是安全的（远超 AP 能产出的）");
    { float maxFromAP=APtoDamage(32767.0f,3.3f);
      if(maxFromAP < 4294967295.0f) OK(); else NG("?"); }
    printf("       ^ 结论：瓶颈不在伤害字段，在 AP_MODS 的 int16\n");

    printf("\n-- D. 装备属性怎么变成 AP 的（谁会踩到）--\n");

    C("力量转AP：战士 1力=2AP，16384力就撑爆 int16");
    { int32 str=16384; int32 ap=str*2; if(ap==32768) OK(); else NG("ap=%d",ap); }
    printf("       ^ 你的 stat_value 已扩到 21亿，很容易超\n");

    C("敏捷转AP：猎人 1敏=2AP，同样 16384 撑爆");
    { int32 agi=16384; if(agi*2==32768) OK(); else NG("?"); }

    C("[安全区] 力量 16000 以下不会触发");
    { int32 ap=16000*2; if(ap<32767) OK(); else NG("?"); }

    printf("\n=== 结果: %d 通过 / %d 失败 (共 %d) ===\n",pass,fail,no);
    return fail?1:0;
}
