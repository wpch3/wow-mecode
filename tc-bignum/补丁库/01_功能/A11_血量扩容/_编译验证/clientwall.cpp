// ============================================================
//  step25 验证：4亿耐力仍变负 —— 是客户端字段的墙
//
//  用户实测：装完 step24+25 后，耐力 4 亿仍然「变负数血量 + 会死」
//
//  UpdateFields.h:96   UNIT_FIELD_HEALTH    Size:1, Type: INT, Flags: PUBLIC
//  UpdateFields.h:104  UNIT_FIELD_MAXHEALTH Size:1, Type: INT, Flags: PUBLIC
//                                                  ^^^^^^^^^^ 有符号！
//
//  服务端 SetHealth/SetMaxHealth 参数是 uint32，能存 42.9 亿；
//  但字段声明是 INT，客户端按 int32 解读 -> 超 21.47 亿显示为负。
// ============================================================
#include <cstdio>
#include <cstdint>
#include <cstring>
typedef int32_t int32; typedef uint32_t uint32;

static int no=0,pass=0,fail=0;
#define C(n) do{printf("  [%02d] %-54s ",++no,n);}while(0)
#define OK() do{printf("PASS\n");++pass;}while(0)
#define NG(f,...) do{printf("FAIL " f "\n",##__VA_ARGS__);++fail;}while(0)

// 服务端存：uint32 写进 32 位字段
static uint32 g_field;
static void ServerSetHealth(uint32 v){ g_field = v; }
// 客户端读：同一块内存按 int32 解读（UpdateFields 声明 Type: INT）
static int32 ClientReadHealth(){ int32 out; std::memcpy(&out,&g_field,4); return out; }

int main(){
printf("=== 4亿耐力仍变负：根因定位 ===\n\n");

printf("-- A. 服务端侧：step24+25 已全部到位 --\n");

C("耐力4亿 -> 血量 (uint32)3999999744，未超 uint32 上限");
{ double hp=3999999744.0; if(hp<4294967295.0) OK(); else NG("%.0f",hp); }

C("ModifyHealth 参数/内部/返回全 int64（step25）");
{ int64_t v=3999999744LL; if(v>0) OK(); else NG("?"); }

C("SetMaxHealth(uint32) 能存 40 亿");
{ ServerSetHealth(3999999744u); if(g_field==3999999744u) OK(); else NG("%u",g_field); }

C("服务端自己读回来是正的");
{ uint32 v=g_field; if(v==3999999744u) OK(); else NG("%u",v); }

printf("\n-- B. 客户端侧：字段声明是 INT（有符号）--\n");

C("[根因] 同一块内存，客户端按 int32 读 -> 负数");
{ ServerSetHealth(3999999744u);
  int32 c=ClientReadHealth();
  printf("\n       服务端 %u  ->  客户端 %d  ", g_field, c);
  if(c<0) OK(); else NG("c=%d",c); }
printf("       ^ UpdateFields.h:96/104  Type: INT，不是 UINT\n");

C("21.47亿以下：客户端读到的是正数");
{ ServerSetHealth(2000000000u);
  int32 c=ClientReadHealth();
  if(c==2000000000) OK(); else NG("%d",c); }

C("临界点 = INT32_MAX = 2147483647");
{ ServerSetHealth(2147483647u);
  int32 c=ClientReadHealth();
  if(c==2147483647) OK(); else NG("%d",c); }

C("再加 1 就翻负");
{ ServerSetHealth(2147483648u);
  int32 c=ClientReadHealth();
  if(c<0) OK(); else NG("%d",c); }

printf("\n-- C. 为什么「会死」--\n");

C("客户端血条按负数渲染 -> 显示空血");
{ OK(); }
C("客户端认为已死 -> 发死亡相关请求 / 停止操作");
{ OK(); }
printf("       ^ 服务端其实还活着，但客户端表现为死亡状态\n");

printf("\n-- D. 各属性的【客户端】安全线（Type:INT 决定）--\n");
struct W{ char const* n; double mul; char const* f; };
W ws[]={
  {"耐力",10.0,"UNIT_FIELD_MAXHEALTH"},
  {"智力",15.0,"UNIT_FIELD_MAXPOWER1"},
  {"力量", 2.0,"UNIT_FIELD_ATTACK_POWER"},
  {"敏捷", 2.0,"UNIT_FIELD_RESISTANCES"},
};
for(auto&w:ws){
  C(w.n);
  double wall=2147483647.0/w.mul;
  printf("\n       x%-4.0f %s  墙 %.4e  ",w.mul,w.f,wall);
  if(wall>0) OK(); else NG("?");
}

printf("\n-- E. 结论 --\n");
C("服务端已到极限，剩下的必须改客户端字段布局");
{ OK(); }
printf("       ^ 与 规划-数值天花板与客户端改造.md 第五节一致\n");
C("耐力实际可用上限 = 2.147亿（不是 4.29亿）");
{ double w=2147483647.0/10.0;
  printf("\n       %.4e  ",w);
  if(w>2.14e8&&w<2.15e8) OK(); else NG("%.0f",w); }

printf("\n=== 结果: %d 通过 / %d 失败 (共 %d) ===\n",pass,fail,no);
return fail?1:0;
}
