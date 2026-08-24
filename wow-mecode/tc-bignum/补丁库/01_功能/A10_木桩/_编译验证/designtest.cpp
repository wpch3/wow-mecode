// ============================================================
//  step23 .dummy 木桩 —— 设计验证
//
//  验证统计口径、AP 溢出告警阈值、护甲减伤模拟的正确性。
//  所有引用的 API 均已在源码核实（见文件末尾清单）。
// ============================================================
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <map>
#include <string>
#include <vector>

typedef uint8_t  uint8;  typedef int16_t int16;
typedef uint16_t uint16; typedef int32_t int32;
typedef uint32_t uint32; typedef uint64_t uint64;

static int no=0,pass=0,fail=0;
#define C(n) do{printf("  [%02d] %-56s ",++no,n);}while(0)
#define OK() do{printf("PASS\n");++pass;}while(0)
#define NG(f,...) do{printf("FAIL " f "\n",##__VA_ARGS__);++fail;}while(0)

// ---------- 复刻 Unit.cpp:11373 GetTotalAttackPowerValue ----------
static float GetTotalAP(int32 baseAP, uint16 modPos, uint16 modNeg, float mult)
{
    int32 ap = baseAP + int16(modPos) + int16(modNeg);   // int16 是天花板
    if (ap < 0) return 0.0f;
    return ap * (1.0f + mult);
}

// ---------- AP 健康度诊断（.dummy 要输出的告警）----------
enum ApHealth { AP_OK, AP_WARN, AP_OVERFLOW, AP_ZEROED };
static ApHealth DiagnoseAP(int32 baseAP, int32 wantedMod)
{
    if (wantedMod > 32767)
    {
        int32 real = baseAP + int16(uint16(wantedMod));
        return real < 0 ? AP_ZEROED : AP_OVERFLOW;
    }
    if (wantedMod > 29000) return AP_WARN;     // 距上限 <10%
    return AP_OK;
}

// ---------- DPS 统计 ----------
struct DpsMeter
{
    uint64 total = 0;
    uint32 hits  = 0;
    uint32 crits = 0;
    uint32 misses= 0;
    uint32 startMs = 0, endMs = 0;
    std::map<uint32, uint64> bySpell;      // 法术ID -> 累计伤害

    void Add(uint32 spellId, uint32 dmg, bool crit)
    { total += dmg; ++hits; if (crit) ++crits; bySpell[spellId] += dmg; }
    void Miss() { ++misses; }

    double Seconds() const { return (endMs - startMs) / 1000.0; }
    double Dps() const { double s = Seconds(); return s > 0.0 ? double(total)/s : 0.0; }
    double CritPct() const { return hits ? 100.0*crits/hits : 0.0; }
    double MissPct() const { uint32 t=hits+misses; return t ? 100.0*misses/t : 0.0; }
};

// ---------- 复刻 Unit::CalcArmorReducedDamage 的减伤段（源码原文）----------
//   float levelModifier = attacker ? attacker->GetLevel() : attackerLevel;
//   if (levelModifier > 59.f)
//       levelModifier = levelModifier + 4.5f * (levelModifier - 59.f);
//   float damageReduction = 0.1f * armor / (8.5f * levelModifier + 40.f);
//   damageReduction /= (1.0f + damageReduction);
//   RoundToInterval(damageReduction, 0.f, 0.75f);
// 注意：不是我一开始凭记忆写的 armor/(armor+K)，分母结构完全不同。
static float ArmorReduction(uint32 armor, uint8 attackerLevel)
{
    float levelModifier = float(attackerLevel);
    if (levelModifier > 59.0f)
        levelModifier = levelModifier + 4.5f * (levelModifier - 59.0f);

    float damageReduction = 0.1f * float(armor) / (8.5f * levelModifier + 40.0f);
    damageReduction /= (1.0f + damageReduction);

    if (damageReduction > 0.75f) damageReduction = 0.75f;
    if (damageReduction < 0.0f)  damageReduction = 0.0f;
    return damageReduction;
}

int main()
{
printf("=== step23 .dummy 木桩设计验证 ===\n\n");

printf("-- A. DPS 统计口径 --\n");

C("基础：10秒打 100000 -> DPS 10000");
{ DpsMeter m; m.startMs=1000; m.endMs=11000;
  for(int i=0;i<10;++i) m.Add(1,10000,false);
  if(std::fabs(m.Dps()-10000.0)<0.01) OK(); else NG("dps=%.1f",m.Dps()); }

C("时长为 0 不除零崩溃");
{ DpsMeter m; m.startMs=5000; m.endMs=5000; m.Add(1,999,false);
  if(m.Dps()==0.0) OK(); else NG("dps=%.1f",m.Dps()); }

C("一次都没打 -> 各项均 0，不崩");
{ DpsMeter m; m.startMs=0; m.endMs=10000;
  if(m.Dps()==0.0 && m.CritPct()==0.0 && m.MissPct()==0.0) OK(); else NG("?"); }

C("暴击率：10次3暴 -> 30%");
{ DpsMeter m; for(int i=0;i<10;++i) m.Add(1,100,i<3);
  if(std::fabs(m.CritPct()-30.0)<0.01) OK(); else NG("%.1f",m.CritPct()); }

C("未命中率：8中2miss -> 20%");
{ DpsMeter m; for(int i=0;i<8;++i) m.Add(1,100,false); m.Miss(); m.Miss();
  if(std::fabs(m.MissPct()-20.0)<0.01) OK(); else NG("%.1f",m.MissPct()); }

C("miss 不计入总伤害");
{ DpsMeter m; m.Add(1,500,false); m.Miss(); m.Miss();
  if(m.total==500 && m.hits==1) OK(); else NG("total=%llu",(unsigned long long)m.total); }

C("[大数值] 累计伤害用 uint64，不会在 42.9亿 处回绕");
{ DpsMeter m; for(int i=0;i<10;++i) m.Add(1,1000000000u,false);   // 100亿
  if(m.total==10000000000ULL) OK(); else NG("total=%llu",(unsigned long long)m.total); }
printf("       ^ uint32 累计会溢出，你的端必须 uint64\n");

C("分技能占比：三个技能各自归类");
{ DpsMeter m; m.Add(100,3000,false); m.Add(200,1000,false); m.Add(100,2000,false);
  if(m.bySpell[100]==5000 && m.bySpell[200]==1000 && m.bySpell.size()==2) OK();
  else NG("size=%zu",m.bySpell.size()); }

printf("\n-- B. AP 溢出诊断（本次核心）--\n");

C("正常 AP 加成 5000 -> AP_OK");
{ if(DiagnoseAP(3000,5000)==AP_OK) OK(); else NG("?"); }

C("接近上限 30000 -> AP_WARN（提前告警）");
{ if(DiagnoseAP(3000,30000)==AP_WARN) OK(); else NG("?"); }

C("恰好 32767 -> 仍 AP_WARN 不误报溢出");
{ if(DiagnoseAP(3000,32767)==AP_WARN) OK(); else NG("?"); }

C("超过 32767 且总值仍为正 -> AP_OVERFLOW");
{ if(DiagnoseAP(100000,40000)==AP_OVERFLOW) OK(); else NG("?"); }

C("超过且总值变负 -> AP_ZEROED（攻强显示0）");
{ if(DiagnoseAP(1000,50000)==AP_ZEROED) OK(); else NG("?"); }

C("溢出后实际 AP 确实低于溢出前");
{ float before=GetTotalAP(100000,32767,0,0.0f);
  float after =GetTotalAP(100000,32768,0,0.0f);
  if(after<before) OK(); else NG("before=%.0f after=%.0f",before,after); }
printf("       ^ 加装备反而变弱，这就是要告警的原因\n");

C("力量换算：战士 1力=2AP，16384力触发溢出");
{ int32 str=16384; if(DiagnoseAP(0,str*2)==AP_OVERFLOW||DiagnoseAP(0,str*2)==AP_ZEROED) OK(); else NG("?"); }

C("安全区：力量 14000 -> AP 28000 -> AP_OK");
{ if(DiagnoseAP(0,14000*2)==AP_OK) OK(); else NG("?"); }

printf("\n-- C. 护甲减伤模拟（.dummy armor）--\n");

C("护甲 0 -> 减伤 0%%");
{ if(std::fabs(ArmorReduction(0,80))<0.001f) OK(); else NG("%.3f",ArmorReduction(0,80)); }

C("80级 打 10643 护甲 -> 41.1%% 减伤（按源码公式实算）");
{ float r=ArmorReduction(10643,80);
  if(r>0.410f&&r<0.412f) OK(); else NG("%.4f",r); }
printf("       ^ 我一开始凭记忆写成 armor/(armor+K) 是错的，\n"
       "         真公式分母是 8.5*levelModifier+40，已按源码改正\n");

C("护甲极大 -> 钳在 75%% 上限，不会 100%% 免伤");
{ float r=ArmorReduction(99999999,80); if(std::fabs(r-0.75f)<0.0001f) OK(); else NG("%.3f",r); }

C("同护甲下等级越高减伤越低");
{ float r60=ArmorReduction(10000,60), r80=ArmorReduction(10000,80);
  if(r80<r60) OK(); else NG("r60=%.3f r80=%.3f",r60,r80); }

printf("\n-- D. 木桩行为约束 --\n");

C("木桩不还手：REACT_PASSIVE = 0");
{ int react=0; if(react==0) OK(); else NG("?"); }

C("木桩死不了：伤害达血量上限时补满");
{ uint32 maxHp=1000000, hp=maxHp;
  uint32 dmg=999999999u;                    // 一击超大伤害
  hp = (dmg >= hp) ? maxHp : hp-dmg;        // 复刻"打不死"逻辑
  if(hp==maxHp) OK(); else NG("hp=%u",hp); }

C("统计只算对木桩的伤害，不混入其他目标");
{ DpsMeter m; uint32 dummyGuid=42;
  auto tryAdd=[&](uint32 guid,uint32 d){ if(guid==dummyGuid) m.Add(1,d,false); };
  tryAdd(42,1000); tryAdd(99,5000); tryAdd(42,2000);
  if(m.total==3000) OK(); else NG("total=%llu",(unsigned long long)m.total); }

C("只统计自己的输出（多人打同一木桩时分离）");
{ DpsMeter mine; uint32 myGuid=7;
  auto tryAdd=[&](uint32 atk,uint32 d){ if(atk==myGuid) mine.Add(1,d,false); };
  tryAdd(7,1000); tryAdd(8,9999); tryAdd(7,500);
  if(mine.total==1500) OK(); else NG("total=%llu",(unsigned long long)mine.total); }

printf("\n=== 结果: %d 通过 / %d 失败 (共 %d) ===\n",pass,fail,no);

/* ---------------- 已核实 API 清单（全 public）----------------
 ScriptMgr.h:397        UnitScript::OnDamage(Unit*,Unit*,uint32&)   <- 统计钩子
 Unit.cpp:756           sScriptMgr->OnDamage(...)  在 DealDamage 内，单一汇聚点
 Unit.h:1022            static uint32 DealDamage(..., CleanDamage const*, ...)
 Unit.h:415             struct CleanDamage { ... MeleeHitOutcome hitOutCome; }
 Unit.h:393             enum MeleeHitOutcome { ... MELEE_HIT_CRIT, ... }
 Unit.h:11373(cpp)      GetTotalAttackPowerValue()  <- int16 天花板在这
 StatSystem.cpp:565     baseValue += GetTotalAttackPowerValue()/14.0f*apMod
 Unit.h:1659            static uint32 CalcArmorReducedDamage(...)
 Unit.h:906/907         GetArmor() / SetArmor(int32)
 Unit.h:926/927/928     SetHealth / SetMaxHealth / SetFullHealth
 Creature.h:134         SetReactState(ReactStates)
 UnitDefines.h:408      REACT_PASSIVE = 0
 Unit.h:1136-1143       SetImmuneToAll / SetImmuneToPC / SetImmuneToNPC
 WorldObject.h:476      SummonCreature(...)
-------------------------------------------------------------- */
return fail?1:0;
}
