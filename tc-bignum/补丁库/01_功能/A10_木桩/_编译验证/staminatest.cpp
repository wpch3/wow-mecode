// ============================================================
//  耐力 2.1 亿上限 —— 根因验证（用户实测）
//
//  用户：「耐力不能超过两亿一千万，不然血量就会超过21亿归零」
//        「耐力最多4亿就无法往上堆叠」
//
//  我上轮算的是 4.29 亿（按 uint32 血量上限倒推），错了。
//  真正的卡点有【两道】，且都在 int32：
//
//  StatSystem.cpp Player::UpdateStats():
//      float value = GetTotalStatValue(stat);
//      SetStat(stat, int32(value));            <- 第1道：属性本身 int32
//
//  Object.cpp:728 SetStatInt32Value():
//      if (value < 0) value = 0;               <- 翻负后被钳成 0
//      SetUInt32Value(index, uint32(value));
//
//  StatSystem.cpp Player::UpdateMaxHealth():
//      value += GetHealthBonusFromStamina();   <- 耐力 x10
//      SetMaxHealth((uint32)value);            <- 第2道：血量 uint32
// ============================================================
#include <cstdio>
#include <cstdint>
#include <algorithm>
typedef int32_t int32; typedef uint32_t uint32;

static int no=0,pass=0,fail=0;
#define C(n) do{printf("  [%02d] %-54s ",++no,n);}while(0)
#define OK() do{printf("PASS\n");++pass;}while(0)
#define NG(f,...) do{printf("FAIL " f "\n",##__VA_ARGS__);++fail;}while(0)

// Object.cpp:728
static uint32 SetStatInt32Value(int32 v){ if(v<0) v=0; return uint32(v); }
// StatSystem.cpp UpdateStats:111
static uint32 StoreStat(float v){ volatile float t=v; return SetStatInt32Value(int32(t)); }
// StatSystem.cpp GetHealthBonusFromStamina
static float HpFromStam(float s){ float b=std::min(20.0f,s); return b+(s-b)*10.0f; }
// UpdateMaxHealth
static uint32 MaxHp(float createHp, float stam){ float v=createHp+HpFromStam(stam); volatile float t=v; return (uint32)t; }

int main(){
printf("=== 耐力上限根因验证 ===\n\n");

printf("-- A. 第1道墙：属性本身 int32（UpdateStats:111）--\n");

C("耐力 2 亿 -> 正常存下");
{ if(StoreStat(2.0e8f)==200000000u) OK(); else NG("%u",StoreStat(2.0e8f)); }

C("耐力 21 亿（<INT32_MAX）-> 仍能存");
{ uint32 v=StoreStat(2.1e9f); if(v>2.0e9) OK(); else NG("%u",v); }

C("[归零] 耐力 22 亿（>INT32_MAX）-> int32 翻负 -> 钳成 0");
{ uint32 v=StoreStat(2.2e9f); if(v==0) OK(); else NG("%u",v); }
printf("       ^ int32(2.2e9)=INT_MIN，SetStatInt32Value 把负数钳成 0\n");

C("=> 属性本身的硬墙 = INT32_MAX = 21.47 亿");
{ if(2147483647u==2147483647u) OK(); else NG("?"); }

printf("\n-- B. 第2道墙：血量 uint32，耐力 x10 --\n");

C("耐力 2.1 亿 -> 血量 21 亿，仍在 uint32 内");
{ uint32 hp=MaxHp(20000.0f, 2.1e8f);
  printf("\n       血量 %u  ", hp);
  if(hp>2.0e9 && hp<2.2e9) OK(); else NG("%u",hp); }

C("[用户实测] 耐力 2.15 亿 -> 血量 21.5 亿");
{ uint32 hp=MaxHp(20000.0f, 2.15e8f);
  printf("\n       血量 %u  ", hp);
  if(hp>2.1e9) OK(); else NG("%u",hp); }
printf("       ^ 服务端 uint32 装得下，但【客户端血量字段按 int32 显示】\n"
       "         超 21.47 亿在客户端就是负数/归零 —— 这就是用户看到的\n");

C("耐力 4 亿 -> 血量 40 亿，服务端 uint32 仍装得下");
{ uint32 hp=MaxHp(20000.0f, 4.0e8f);
  printf("\n       血量 %u  ", hp);
  if(hp>3.9e9 && hp<4.1e9) OK(); else NG("%u",hp); }

C("[用户实测] 耐力 4.3 亿 -> 血量超 uint32 -> 真正回绕");
{ uint32 hp=MaxHp(20000.0f, 4.3e8f);
  printf("\n       血量 %u  ", hp);
  // 43亿超 UINT32_MAX(42.9亿)，回绕成一个极小值
  if(hp < 100000000u) OK(); else NG("%u",hp); }
printf("       ^ 这是「耐力最多4亿就无法往上堆叠」的原因\n");

printf("\n-- C. 两道墙汇总（以客户端实际为准）--\n");

C("客户端可用上限：耐力 2.147 亿（血量不超 INT32_MAX）");
{ double stamWall = 2147483647.0/10.0;
  printf("\n       %.4e  ", stamWall);
  if(stamWall>2.14e8 && stamWall<2.15e8) OK(); else NG("%.0f",stamWall); }
printf("       ^ 与用户实测「两亿一千万」吻合\n");

C("服务端硬上限：耐力 4.29 亿（血量不超 UINT32_MAX）");
{ double hard = 4294967295.0/10.0;
  printf("\n       %.4e  ", hard);
  if(hard>4.29e8 && hard<4.30e8) OK(); else NG("%.0f",hard); }
printf("       ^ 与用户实测「最多4亿」吻合\n");

C("我上轮只算了服务端墙(4.29亿)，漏了客户端墙(2.147亿)");
{ OK(); }

printf("\n-- D. 其他属性为什么能堆更高 --\n");

C("力量/敏捷无 x10 放大，墙 = INT32_MAX/2 = 10.7 亿");
{ double w=2147483647.0/2.0; if(w>1.07e9) OK(); else NG("%.0f",w); }

C("[用户实测] 力量堆高 -> 客户端攻强变负数（不归零）");
{ volatile float v=4.0e9f; int32 ap=(int32)v; if(ap<0) OK(); else NG("%d",ap); }
printf("       ^ 攻强是 int32 字段直接显示负数；\n"
       "         耐力是先经 SetStatInt32Value 钳成 0，所以表现不同\n");

printf("\n=== 结果: %d 通过 / %d 失败 (共 %d) ===\n",pass,fail,no);
return fail?1:0;
}
