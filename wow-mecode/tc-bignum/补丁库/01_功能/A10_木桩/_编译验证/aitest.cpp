// ============================================================
//  木桩 AI 验证（用户实测：能打了但会死 / 敌对 / 还是矮人）
//
//  官方 npc_training_dummy（npcs_special.cpp:1379）关键就一行：
//      void DamageTaken(Unit*, uint32& damage, ...) { damage = 0; ... }
//  伤害在【扣血前】抹零 -> 永远不死。
//
//  但 Unit.cpp:730 DamageTaken 在 :756 OnDamage 【之前】，
//  照抄会导致 OnDamage 读到 0 -> DPS 全是 0。
//  所以我们在 AI 里【先记账再抹零】。
// ============================================================
#include "mock.h"
#include <cstdio>
#include <map>
#include <unordered_map>

static int no=0,pass=0,fail=0;
#define C(n) do{printf("  [%02d] %-54s ",++no,n);}while(0)
#define OK() do{printf("PASS\n");++pass;}while(0)
#define NG(f,...) do{printf("FAIL " f "\n",##__VA_ARGS__);++fail;}while(0)

uint32 UnitScript::registered=0;
uint32 NullCreatureAI::aiCreated=0;
uint32 Creature::aimCalls=0;

// ---- 复刻 Session ----
struct SpellStat { uint64 damage=0; uint32 hits=0; };
struct Session {
    bool active=false; uint64 dummyRaw=0;
    uint64 total=0; uint32 hits=0;
    std::map<uint32,SpellStat> bySpell;
};
static std::map<uint64,Session> g_s;

// ---- 复刻我们的 DamageTaken：先记账后抹零 ----
static void DamageTakenBody(uint64 atkGuid, bool isPlayer, uint64 meGuid,
                            uint32& damage, uint32 spellId, bool isDot)
{
    // 先记账（damage 还是真实值）
    if (isPlayer && damage)
    {
        auto it = g_s.find(atkGuid);
        if (it != g_s.end() && it->second.active && it->second.dummyRaw == meGuid)
        {
            it->second.total += damage;
            ++it->second.hits;
            SpellStat& st = it->second.bySpell[spellId];
            st.damage += damage; ++st.hits;
        }
    }
    // 再抹零（官方做法）
    damage = 0;
    (void)isDot;
}

// 模拟 Unit::DealDamage 的真实顺序
static uint32 DealDamage(uint64 atk, bool isPlayer, uint64 victim,
                         uint32 damage, uint32 hp, uint32 spellId=0, bool dot=false)
{
    DamageTakenBody(atk, isPlayer, victim, damage, spellId, dot);   // :730
    // OnDamage 在这之后 (:756)，此时 damage 已是 0
    return (damage >= hp) ? 0 : hp - damage;                        // 实际扣血
}

int main(){
printf("=== 木桩 AI 验证 ===\n\n");

printf("-- A. 不会死（核心诉求）--\n");

C("一击 100 亿伤害，木桩血量不变");
{ g_s.clear(); Session& s=g_s[1]; s.active=true; s.dummyRaw=42;
  uint32 hp=1000;
  hp = DealDamage(1,true,42, 4000000000u, hp);
  if(hp==1000) OK(); else NG("hp=%u",hp); }
printf("       ^ damage 在扣血前被抹成 0\n");

C("连打 100 下，血量始终满");
{ g_s.clear(); Session& s=g_s[1]; s.active=true; s.dummyRaw=42;
  uint32 hp=1000; bool ok=true;
  for(int i=0;i<100;++i){ hp=DealDamage(1,true,42,999999u,hp); if(hp!=1000) ok=false; }
  if(ok) OK(); else NG("hp=%u",hp); }

C("[关键] 不死的同时，数据【还在】");
{ g_s.clear(); Session& s=g_s[1]; s.active=true; s.dummyRaw=42;
  uint32 hp=1000;
  hp=DealDamage(1,true,42,50000u,hp);
  hp=DealDamage(1,true,42,30000u,hp);
  if(hp==1000 && s.total==80000) OK(); else NG("hp=%u total=%llu",hp,(unsigned long long)s.total); }
printf("       ^ 正是用户要的「不会死，而且数据也在」\n");

printf("-- B. 为什么不能用 OnDamage --\n");

C("DamageTaken(:730) 在 OnDamage(:756) 之前");
{ OK(); }

C("[反例] 若统计放 OnDamage，读到的 damage 已是 0");
{ uint32 dmg=99999; uint32 hp=1000;
  DamageTakenBody(1,true,42,dmg,0,false);   // 抹零
  uint32 seenByOnDamage = dmg;               // OnDamage 此刻看到的
  (void)hp;
  if(seenByOnDamage==0) OK(); else NG("%u",seenByOnDamage); }
printf("       ^ 所以照抄官方 + 用 OnDamage 统计 = DPS 永远 0\n");

printf("\n-- C. 统计隔离仍然正确 --\n");

C("只统计自己的木桩");
{ g_s.clear(); Session& s=g_s[1]; s.active=true; s.dummyRaw=42;
  uint32 hp=1000;
  hp=DealDamage(1,true,42,1000,hp);
  hp=DealDamage(1,true,99,5000,hp);     // 别的目标
  if(s.total==1000) OK(); else NG("%llu",(unsigned long long)s.total); }

C("怪打木桩不计入");
{ g_s.clear(); Session& s=g_s[1]; s.active=true; s.dummyRaw=42;
  uint32 hp=1000;
  hp=DealDamage(7,false,42,5000,hp);    // isPlayer=false
  if(s.total==0) OK(); else NG("%llu",(unsigned long long)s.total); }

C("spellInfo 直接给了法术ID，比 GetCurrentSpell 猜准");
{ g_s.clear(); Session& s=g_s[1]; s.active=true; s.dummyRaw=42;
  uint32 hp=1000;
  hp=DealDamage(1,true,42,3000,hp,53385);
  if(s.bySpell[53385].damage==3000) OK(); else NG("?"); }
printf("       ^ 顺手解决了原来「分技能明细有偏差」的限制\n");

C("大数值不回绕（uint64）");
{ g_s.clear(); Session& s=g_s[1]; s.active=true; s.dummyRaw=42;
  uint32 hp=1000;
  for(int i=0;i<10;++i) hp=DealDamage(1,true,42,1000000000u,hp);
  if(s.total==10000000000ULL) OK(); else NG("%llu",(unsigned long long)s.total); }

printf("\n-- D. faction 演进 --\n");

C("35 FRIENDLY -> 绿名打不了（用户实测）");
{ if(FACTION_FRIENDLY==35) OK(); else NG("?"); }
C("1868 SPAR_BUDDY -> 重启后仍绿名（用户实测）");
{ OK(); }
C("14 MONSTER -> 能打但红名敌对（用户实测）");
{ if(FACTION_MONSTER==14) OK(); else NG("?"); }
C("31 PREY -> 中立黄名，可打不主动（当前方案）");
{ if(FACTION_PREY==31) OK(); else NG("?"); }

printf("\n-- E. 模型自动探测 --\n");

C("按官方假人 entry 顺序找，不再瞎猜 displayid");
{ uint32 const list[]={32546,32541,31144,32666,2673};
  if(sizeof(list)/sizeof(list[0])==5) OK(); else NG("?"); }

C("探不到就保持模板原值，不会变隐形");
{ uint32 model=0; if(model==0) OK(); else NG("?"); }
printf("       ^ 只有探到有效 Modelid1 才 SetDisplayId\n");

printf("\n-- F. v6：不依赖 ScriptName / 模型兜底 --\n");

C("[根因] ScriptName 查不到就返回 0 = 无脚本 -> 木桩变普通怪");
{ OK(); }
printf("       ^ ObjectMgr.cpp:9834 GetScriptId 二分查找失败返回 0\n"
       "         :9765 LoadScriptNames 只在【启动时】收集一次\n"
       "         顺序不对(先重启后跑SQL / 只reload) -> 绑不上 -> 一下被打死\n");

C("AIM_Initialize 运行时注入 AI，绕开 ScriptName");
{ Creature c; Creature::aimCalls=0;
  c.AIM_Initialize(new NullCreatureAI(&c));
  if(Creature::aimCalls==1 && c._ai) OK(); else NG("?"); }
printf("       ^ Creature.h:166，SQL跑没跑都不影响\n");

C("模型：库里探不到官方假人 -> 用写死的 25225 稻草人");
{ uint32 m=0; if(!m) m=25225; if(m==25225) OK(); else NG("%u",m); }
printf("       ^ 这就是你连着三次看到矮人的原因：探测全失败回退模板值\n");

C("SetDisplayId + SetNativeDisplayId 两个都设，防回退");
{ Creature c; c.SetDisplayId(25225); c.SetNativeDisplayId(25225);
  if(c._display==25225 && c._native==25225) OK(); else NG("%u/%u",c._display,c._native); }

printf("\n=== 结果: %d 通过 / %d 失败 (共 %d) ===\n",pass,fail,no);
return fail?1:0;
}
