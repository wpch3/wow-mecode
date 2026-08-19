// ============================================================
//  木桩隐形 bug 验证（用户实测：提示召唤成功但看不到）
//
//  根因：flags_extra 我填了 130 = CIVILIAN(0x02) + TRIGGER(0x80)
//        我把 NO_XP 记成 0x80，实际 NO_XP 是 0x40，0x80 是 TRIGGER。
//
//  CreatureData.h:182  CREATURE_FLAG_EXTRA_CIVILIAN = 0x00000002
//  CreatureData.h:187  CREATURE_FLAG_EXTRA_NO_XP    = 0x00000040
//  CreatureData.h:188  CREATURE_FLAG_EXTRA_TRIGGER  = 0x00000080
//
//  Creature.cpp:646  // trigger creature is always uninteractible and can not be attacked
//  Creature.cpp:647  if (IsTrigger()) SetUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
//  Creature.cpp:162  return 11686;   <- 触发器的隐形模型
// ============================================================
#include <cstdio>
#include <cstdint>
typedef uint32_t uint32;

static int no=0,pass=0,fail=0;
#define C(n) do{printf("  [%02d] %-54s ",++no,n);}while(0)
#define OK() do{printf("PASS\n");++pass;}while(0)
#define NG(f,...) do{printf("FAIL " f "\n",##__VA_ARGS__);++fail;}while(0)

enum {
    CIVILIAN = 0x02,
    NO_XP    = 0x40,
    TRIGGER  = 0x80,
};
// Creature.h:113
static bool IsTrigger(uint32 fe){ return (fe & TRIGGER)!=0; }

int main(){
printf("=== 木桩隐形 bug 验证 ===\n\n");

printf("-- A. 旧值 130 为什么隐形 --\n");

C("130 = 0x82 = CIVILIAN + TRIGGER");
{ if(130==(CIVILIAN|TRIGGER)) OK(); else NG("?"); }

C("[病根] 130 含 TRIGGER -> IsTrigger()==true");
{ if(IsTrigger(130)) OK(); else NG("?"); }
printf("       ^ Creature.cpp:647 -> SetUnitFlag(UNIT_FLAG_UNINTERACTIBLE)\n"
       "         Creature.cpp:162 -> 模型强制 11686（隐形）\n");

C("130 里【根本没有】NO_XP 位");
{ if(!(130 & NO_XP)) OK(); else NG("?"); }
printf("       ^ 我把 NO_XP 记成 0x80，其实 0x80 是 TRIGGER，NO_XP 是 0x40\n");

printf("\n-- B. 新值 66 正确 --\n");

C("66 = 0x42 = CIVILIAN + NO_XP");
{ if(66==(CIVILIAN|NO_XP)) OK(); else NG("?"); }

C("66 不含 TRIGGER -> 可见可打");
{ if(!IsTrigger(66)) OK(); else NG("?"); }

C("66 含 NO_XP -> 击杀不给经验（本来想要的效果）");
{ if(66 & NO_XP) OK(); else NG("?"); }

C("66 含 CIVILIAN -> 不主动仇恨");
{ if(66 & CIVILIAN) OK(); else NG("?"); }

C("算术校验 2 + 64 == 66");
{ if(2+64==66) OK(); else NG("?"); }

printf("\n-- C. unit_flags 32832 无隐形位 --\n");

C("32832 = 0x40(UNK_6) + 0x8000(CAN_SWIM)，干净");
{ uint32 v=32832; if(v==(0x40u|0x8000u)) OK(); else NG("0x%X",v); }

C("不含 NON_ATTACKABLE(0x02)");
{ if(!(32832u & 0x02u)) OK(); else NG("?"); }

C("不含 IMMUNE_TO_PC(0x100) —— 上一轮已修，玩家打得动");
{ if(!(32832u & 0x100u)) OK(); else NG("?"); }

C("不含 NOT_SELECTABLE(0x2000000)");
{ if(!(32832u & 0x2000000u)) OK(); else NG("?"); }

printf("\n=== 结果: %d 通过 / %d 失败 (共 %d) ===\n",pass,fail,no);
return fail?1:0;
}
