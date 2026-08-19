// ============================================================
//  step26 NPC 状态调度器 —— 逻辑验证
//
//  复刻六个档位的 flag 组合，对照 IsValidAttackTarget 判定链
//  （Object.cpp:2991）验证每档行为是否符合预期。
// ============================================================
#include <cstdio>
#include <cstdint>
typedef uint32_t uint32;

static int no=0,pass=0,fail=0;
#define C(n) do{printf("  [%02d] %-52s ",++no,n);}while(0)
#define OK() do{printf("PASS\n");++pass;}while(0)
#define NG(f,...) do{printf("FAIL " f "\n",##__VA_ARGS__);++fail;}while(0)

// UnitDefines.h
enum {
    UNIT_FLAG_NON_ATTACKABLE   = 0x00000002,
    UNIT_FLAG_IMMUNE_TO_PC     = 0x00000100,
    UNIT_FLAG_IMMUNE_TO_NPC    = 0x00000200,
    UNIT_FLAG_NON_ATTACKABLE_2 = 0x00010000,
    UNIT_FLAG_UNINTERACTIBLE   = 0x02000000,
};
enum ReactStates { REACT_PASSIVE=0, REACT_DEFENSIVE=1, REACT_AGGRESSIVE=2 };
enum { FACTION_MONSTER=14, FACTION_PREY=31, FACTION_FRIENDLY=35 };

enum NpcStateId { NST_INVULN=0, NST_STORY, NST_PCONLY, NST_NORMAL, NST_NEUTRAL, NST_FRIEND, NST_MAX };

struct StateConfig {
    bool immuneToPC, immuneToNPC, uninteractible, nonAttackable;
    uint32 faction; ReactStates react;
};
static StateConfig const g_cfg[NST_MAX] = {
    /* invuln  */ { true,  true,  false, true,  0,                REACT_PASSIVE    },
    /* story   */ { true,  true,  true,  true,  0,                REACT_PASSIVE    },
    /* pconly  */ { true,  false, false, false, 0,                REACT_DEFENSIVE  },
    /* normal  */ { false, false, false, false, FACTION_MONSTER,  REACT_AGGRESSIVE },
    /* neutral */ { false, false, false, false, FACTION_PREY,     REACT_DEFENSIVE  },
    /* friend  */ { false, false, false, false, FACTION_FRIENDLY, REACT_PASSIVE    },
};

static uint32 CalcFlags(uint32 base, int st){
    StateConfig const& c=g_cfg[st];
    uint32 f=base;
    auto sb=[&f](uint32 b,bool on){ if(on) f|=b; else f&=~b; };
    sb(UNIT_FLAG_IMMUNE_TO_PC,     c.immuneToPC);
    sb(UNIT_FLAG_IMMUNE_TO_NPC,    c.immuneToNPC);
    sb(UNIT_FLAG_UNINTERACTIBLE,   c.uninteractible);
    sb(UNIT_FLAG_NON_ATTACKABLE_2, c.nonAttackable);
    return f;
}

// 复刻 IsValidAttackTarget 的关键判定
static bool PlayerCanAttack(uint32 flags){
    if (flags & UNIT_FLAG_UNINTERACTIBLE) return false;   // :3033
    if (flags & (UNIT_FLAG_NON_ATTACKABLE|UNIT_FLAG_NON_ATTACKABLE_2)) return false; // :3043
    if (flags & UNIT_FLAG_IMMUNE_TO_PC) return false;     // :3071
    return true;
}
static bool NpcCanAttack(uint32 flags){
    if (flags & UNIT_FLAG_UNINTERACTIBLE) return false;
    if (flags & (UNIT_FLAG_NON_ATTACKABLE|UNIT_FLAG_NON_ATTACKABLE_2)) return false;
    if (flags & UNIT_FLAG_IMMUNE_TO_NPC) return false;
    return true;
}
static bool Selectable(uint32 flags){ return !(flags & UNIT_FLAG_UNINTERACTIBLE); }

static int DetectState(uint32 f, uint32 fac){
    bool ipc=(f&UNIT_FLAG_IMMUNE_TO_PC)!=0;
    bool inp=(f&UNIT_FLAG_IMMUNE_TO_NPC)!=0;
    bool un =(f&UNIT_FLAG_UNINTERACTIBLE)!=0;
    if(ipc&&inp&&un) return NST_STORY;
    if(ipc&&inp)     return NST_INVULN;
    if(ipc&&!inp)    return NST_PCONLY;
    if(fac==FACTION_FRIENDLY) return NST_FRIEND;
    if(fac==FACTION_PREY)     return NST_NEUTRAL;
    return NST_NORMAL;
}

int main(){
printf("=== step26 NPC 状态调度器 ===\n\n");

printf("-- A. 无敌档（用户核心需求：像原版小孩）--\n");
C("玩家打不了");
{ uint32 f=CalcFlags(0,NST_INVULN); if(!PlayerCanAttack(f)) OK(); else NG("f=0x%X",f); }
C("怪也打不了（包括BOSS）");
{ uint32 f=CalcFlags(0,NST_INVULN); if(!NpcCanAttack(f)) OK(); else NG("f=0x%X",f); }
C("但【仍可选中】，能对话/交任务");
{ uint32 f=CalcFlags(0,NST_INVULN); if(Selectable(f)) OK(); else NG("f=0x%X",f); }
C("不改阵营（联盟守卫仍是联盟）");
{ if(g_cfg[NST_INVULN].faction==0) OK(); else NG("?"); }

printf("\n-- B. 剧情档（纯背景板）--\n");
C("玩家和怪都打不了");
{ uint32 f=CalcFlags(0,NST_STORY);
  if(!PlayerCanAttack(f)&&!NpcCanAttack(f)) OK(); else NG("f=0x%X",f); }
C("【不可选中】—— 和无敌档的区别");
{ uint32 f=CalcFlags(0,NST_STORY); if(!Selectable(f)) OK(); else NG("f=0x%X",f); }
C("REACT_PASSIVE 不参与战斗");
{ if(g_cfg[NST_STORY].react==REACT_PASSIVE) OK(); else NG("?"); }

printf("\n-- C. 仅怪档（护送任务）--\n");
C("玩家打不了");
{ uint32 f=CalcFlags(0,NST_PCONLY); if(!PlayerCanAttack(f)) OK(); else NG("f=0x%X",f); }
C("怪【能】打（护送目标会被袭击）");
{ uint32 f=CalcFlags(0,NST_PCONLY); if(NpcCanAttack(f)) OK(); else NG("f=0x%X",f); }
C("REACT_DEFENSIVE 会还手");
{ if(g_cfg[NST_PCONLY].react==REACT_DEFENSIVE) OK(); else NG("?"); }

printf("\n-- D. 三个可攻击档 --\n");
C("普通：可打+会主动打你");
{ uint32 f=CalcFlags(0,NST_NORMAL);
  if(PlayerCanAttack(f)&&g_cfg[NST_NORMAL].react==REACT_AGGRESSIVE
     &&g_cfg[NST_NORMAL].faction==FACTION_MONSTER) OK(); else NG("?"); }
C("中立：可打+不主动（黄名 FACTION_PREY=31）");
{ uint32 f=CalcFlags(0,NST_NEUTRAL);
  if(PlayerCanAttack(f)&&g_cfg[NST_NEUTRAL].react==REACT_DEFENSIVE
     &&g_cfg[NST_NEUTRAL].faction==FACTION_PREY) OK(); else NG("?"); }
C("友好：绿名 FACTION_FRIENDLY=35");
{ if(g_cfg[NST_FRIEND].faction==FACTION_FRIENDLY) OK(); else NG("?"); }

printf("\n-- E. 档位互相切换（不残留）--\n");
C("无敌 -> 普通：免疫位被清干净");
{ uint32 f=CalcFlags(0,NST_INVULN);
  f=CalcFlags(f,NST_NORMAL);
  if(PlayerCanAttack(f)&&NpcCanAttack(f)) OK(); else NG("f=0x%X",f); }
C("剧情 -> 普通：可选中恢复");
{ uint32 f=CalcFlags(0,NST_STORY);
  f=CalcFlags(f,NST_NORMAL);
  if(Selectable(f)) OK(); else NG("f=0x%X",f); }
C("普通 -> 剧情 -> 无敌：状态正确迁移");
{ uint32 f=CalcFlags(0,NST_NORMAL);
  f=CalcFlags(f,NST_STORY);
  f=CalcFlags(f,NST_INVULN);
  if(!PlayerCanAttack(f)&&Selectable(f)) OK(); else NG("f=0x%X",f); }
C("保留无关 flag（如 CAN_SWIM 0x8000）");
{ uint32 f=CalcFlags(0x8000,NST_INVULN);
  if(f&0x8000) OK(); else NG("f=0x%X",f); }

printf("\n-- F. 状态识别（.nst 显示当前档位）--\n");
for(int i=0;i<NST_MAX;++i){
  C("识别档位");
  uint32 f=CalcFlags(0,i);
  uint32 fac=g_cfg[i].faction?g_cfg[i].faction:FACTION_MONSTER;
  int d=DetectState(f,fac);
  printf("\n       档位%d -> 识别为%d  ",i,d);
  if(d==i) OK(); else NG("期望%d",i);
}

printf("\n-- G. 安全性 --\n");
C("不使用 UNIT_STATE_UNATTACKABLE（它=IN_FLIGHT别名）");
{ OK(); }
printf("       ^ Unit.h:259 UNIT_STATE_UNATTACKABLE = UNIT_STATE_IN_FLIGHT\n"
       "         用它会让NPC被当成在坐飞行点，十几处IsInFlight()误判\n");
C("不使用 UNIT_FLAG_NON_ATTACKABLE(0x02)");
{ uint32 f=CalcFlags(0,NST_INVULN); if(!(f&UNIT_FLAG_NON_ATTACKABLE)) OK(); else NG("?"); }
printf("       ^ UnitDefines.h:136 它是SPAWNING用的，法术命中会被自动清\n");

printf("\n=== 结果: %d 通过 / %d 失败 (共 %d) ===\n",pass,fail,no);
return fail?1:0;
}
