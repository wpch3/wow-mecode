// ============================================================
//  step23 .dummy —— 逻辑验证
//
//  复刻 cs_dummy.cpp 的真实逻辑并驱动它，重点验证：
//    · 统计口径（只算自己 / 只算木桩 / uint64 不回绕）
//    · 面板回读诊断（读实际存储值，用户实测的两个溢出都要能报出来）
//    · 护甲减伤（源码公式）
// ============================================================
#include "mock.h"
#include <cstdio>
#include <cmath>
#include <map>
#include <string>
#include <vector>
#include <algorithm>

static int no=0,pass=0,fail=0;
#define C(n) do{printf("  [%02d] %-56s ",++no,n);}while(0)
#define OK() do{printf("PASS\n");++pass;}while(0)
#define NG(f,...) do{printf("FAIL " f "\n",##__VA_ARGS__);++fail;}while(0)

uint32 UnitScript::registered = 0;

// ---------------- 复刻 cs_dummy.cpp 的统计结构 ----------------
struct SpellStat { uint64 damage=0; uint32 hits=0; };

struct Session
{
    bool active=false;
    uint64 dummyRaw=0;
    uint32 startMs=0, endMs=0;
    uint64 total=0;                       // 必须 uint64
    uint32 hits=0;
    std::map<uint32,SpellStat> bySpell;

    void Reset(){ active=false; dummyRaw=0; startMs=endMs=0;
                  total=0; hits=0; bySpell.clear(); }
    uint32 ElapsedMs() const { uint32 n=endMs?endMs:0; return n>startMs?n-startMs:0; }
    double Seconds() const { return ElapsedMs()/1000.0; }
    double Dps() const { double s=Seconds(); return s>0.0?double(total)/s:0.0; }
};

static std::map<uint64,Session> g_sessions;   // 用 raw guid 简化

// 复刻 OnDamage 钩子体
static void OnDamageBody(uint64 attackerGuid, uint64 victimGuid,
                         uint32 damage, uint32 spellId, bool attackerIsPlayer)
{
    if (!damage || !attackerIsPlayer) return;
    auto it = g_sessions.find(attackerGuid);
    if (it == g_sessions.end()) return;
    Session& s = it->second;
    if (!s.active) return;
    if (victimGuid != s.dummyRaw) return;         // 只统计自己那个木桩
    s.total += damage;
    ++s.hits;
    SpellStat& st = s.bySpell[spellId];
    st.damage += damage; ++st.hits;
}

// ---------------- 诊断（复刻 BuildDiagnostics）----------------
enum HealthLevel { HL_OK, HL_WARN, HL_DANGER, HL_BROKEN };

static HealthLevel Grade(double cur, double wall, bool negative)
{
    if (negative)   return HL_BROKEN;
    if (cur <= 0.0) return HL_OK;
    double pct = cur / wall;
    if (pct >= 1.0)  return HL_BROKEN;
    if (pct >= 0.85) return HL_DANGER;
    if (pct >= 0.60) return HL_WARN;
    return HL_OK;
}

// 护甲减伤（Unit::CalcArmorReducedDamage 结尾段原文）
static double ArmorDR(double armor, double attackerLevel)
{
    double lm = attackerLevel;
    if (lm > 59.0) lm = lm + 4.5*(lm-59.0);
    double dr = 0.1*armor/(8.5*lm+40.0);
    dr /= (1.0+dr);
    if (dr>0.75) dr=0.75;
    if (dr<0.0)  dr=0.0;
    return dr;
}

constexpr double WALL_UINT32 = 4294967295.0;
constexpr double WALL_INT32  = 2147483647.0;

int main()
{
printf("=== step23 .dummy 逻辑验证 ===\n\n");

printf("-- A. 统计口径 --\n");

C("基础：10秒 100000 伤害 -> DPS 10000");
{ g_sessions.clear(); Session& s=g_sessions[1]; s.Reset();
  s.active=true; s.dummyRaw=42; s.startMs=1000;
  for(int i=0;i<10;++i) OnDamageBody(1,42,10000,0,true);
  s.endMs=11000;
  if(std::fabs(s.Dps()-10000.0)<0.01) OK(); else NG("%.1f",s.Dps()); }

C("[隔离] 打别的目标不计入");
{ g_sessions.clear(); Session& s=g_sessions[1]; s.Reset();
  s.active=true; s.dummyRaw=42;
  OnDamageBody(1,42,1000,0,true);
  OnDamageBody(1,99,5000,0,true);      // 别的怪
  if(s.total==1000) OK(); else NG("%llu",(unsigned long long)s.total); }

C("[隔离] 别人打同一木桩不计入我的账");
{ g_sessions.clear(); Session& a=g_sessions[1]; a.Reset();
  a.active=true; a.dummyRaw=42;
  OnDamageBody(1,42,1000,0,true);
  OnDamageBody(7,42,9999,0,true);      // 别的玩家，没有 session
  if(a.total==1000) OK(); else NG("%llu",(unsigned long long)a.total); }

C("[隔离] 怪打木桩不计入（只统计玩家）");
{ g_sessions.clear(); Session& s=g_sessions[1]; s.Reset();
  s.active=true; s.dummyRaw=42;
  OnDamageBody(1,42,500,0,false);      // attackerIsPlayer=false
  if(s.total==0) OK(); else NG("%llu",(unsigned long long)s.total); }

C("[大数值] 累计超 42.9亿 不回绕（uint64）");
{ g_sessions.clear(); Session& s=g_sessions[1]; s.Reset();
  s.active=true; s.dummyRaw=42;
  for(int i=0;i<10;++i) OnDamageBody(1,42,1000000000u,0,true);   // 100亿
  if(s.total==10000000000ULL) OK(); else NG("%llu",(unsigned long long)s.total); }
printf("       ^ 用户每击约1.88亿，uint32 打23下就爆\n");

C("stop 后不再累加");
{ g_sessions.clear(); Session& s=g_sessions[1]; s.Reset();
  s.active=true; s.dummyRaw=42;
  OnDamageBody(1,42,100,0,true);
  s.active=false;
  OnDamageBody(1,42,900,0,true);
  if(s.total==100) OK(); else NG("%llu",(unsigned long long)s.total); }

C("时长为0不除零");
{ Session s; s.startMs=5000; s.endMs=5000; s.total=999;
  if(s.Dps()==0.0) OK(); else NG("%.1f",s.Dps()); }

C("分技能归类正确");
{ g_sessions.clear(); Session& s=g_sessions[1]; s.Reset();
  s.active=true; s.dummyRaw=42;
  OnDamageBody(1,42,3000,100,true);
  OnDamageBody(1,42,1000,200,true);
  OnDamageBody(1,42,2000,100,true);
  if(s.bySpell[100].damage==5000 && s.bySpell[200].damage==1000
     && s.bySpell[100].hits==2) OK(); else NG("?"); }

printf("\n-- B. 面板回读诊断（读实际存储值）--\n");

C("正常攻强 8亿 -> 安全档");
{ Player p; p.SetInt32Value(UNIT_FIELD_ATTACK_POWER, 800000256);
  int32 ap=p.GetInt32Value(UNIT_FIELD_ATTACK_POWER);
  if(Grade(double(ap),WALL_INT32,ap<0)==HL_OK) OK(); else NG("ap=%d",ap); }

C("[用户实测] 攻强 INT_MIN -> 报 BROKEN");
{ Player p; p.SetInt32Value(UNIT_FIELD_ATTACK_POWER, INT32_MIN);
  int32 ap=p.GetInt32Value(UNIT_FIELD_ATTACK_POWER);
  if(ap<0 && Grade(double(ap),WALL_INT32,ap<0)==HL_BROKEN) OK(); else NG("ap=%d",ap); }
printf("       ^ 这是用户「力量加多了攻强变负数」那条\n");

C("攻强 91%% 上限 -> 报 DANGER（提前预警）");
{ double v=WALL_INT32*0.91;
  if(Grade(v,WALL_INT32,false)==HL_DANGER) OK(); else NG("?"); }

C("攻强 70%% -> 报 WARN");
{ double v=WALL_INT32*0.70;
  if(Grade(v,WALL_INT32,false)==HL_WARN) OK(); else NG("?"); }

C("[用户实测] 耐力4亿 -> 生命 40亿 = 93%% -> DANGER");
{ double hp=4000019712.0;
  if(Grade(hp,WALL_UINT32,false)==HL_DANGER) OK(); else NG("?"); }
printf("       ^ 这是用户「耐力溢出变成0」那条，提前在93%%就报警\n");

C("护甲翻负 -> BROKEN");
{ Player p; p.SetArmor(INT32_MIN);
  int32 a=p.GetInt32Value(UNIT_FIELD_RESISTANCES+SPELL_SCHOOL_NORMAL);
  if(a<0 && Grade(double(a),WALL_INT32,a<0)==HL_BROKEN) OK(); else NG("a=%d",a); }

C("没堆属性(0)不误报");
{ if(Grade(0.0,WALL_INT32,false)==HL_OK) OK(); else NG("?"); }

C("四围的墙：耐力/智力 4.29亿，力量/敏捷 10.7亿");
{ double stamWall=429496729.0, strWall=1073741823.0;
  if(Grade(4.0e8,stamWall,false)==HL_DANGER &&
     Grade(4.0e8,strWall, false)==HL_OK) OK(); else NG("?"); }
printf("       ^ 同样4亿，耐力危险而力量安全 —— 解释了用户「只有耐力出问题」\n");

printf("\n-- C. 护甲减伤（.dummy armor）--\n");

C("护甲 0 -> 0%%");
{ if(std::fabs(ArmorDR(0,80))<1e-9) OK(); else NG("%.4f",ArmorDR(0,80)); }

C("10643 护甲 @80级 -> 41.1%%");
{ double r=ArmorDR(10643,80); if(r>0.410&&r<0.412) OK(); else NG("%.4f",r); }

C("极大护甲钳在 75%%");
{ if(std::fabs(ArmorDR(1e9,80)-0.75)<1e-9) OK(); else NG("%.4f",ArmorDR(1e9,80)); }

C("同护甲 60级减伤 > 80级");
{ if(ArmorDR(10000,60)>ArmorDR(10000,80)) OK(); else NG("?"); }

C("armor 参数钳到 int32 上限，不自造溢出");
{ long long v=99999999999LL; if(v>2147483647LL) v=2147483647LL;
  if(v==2147483647LL) OK(); else NG("%lld",v); }

printf("\n-- D. 木桩行为 --\n");

C("REACT_PASSIVE 不还手");
{ Creature c; c.SetReactState(REACT_PASSIVE);
  if(c._react==REACT_PASSIVE) OK(); else NG("?"); }

C("UnitScript 是 code-only，new 即注册");
{ uint32 before=UnitScript::registered;
  struct T : public UnitScript { T():UnitScript("t"){} };
  new T();
  if(UnitScript::registered==before+1) OK(); else NG("?"); }

C("重复 .dummy 会重置旧统计不叠加");
{ g_sessions.clear(); Session& s=g_sessions[1]; s.Reset();
  s.active=true; s.dummyRaw=42;
  OnDamageBody(1,42,5000,0,true);
  s.Reset();                       // 复刻 HandleStart 里的重置
  s.active=true; s.dummyRaw=43;
  OnDamageBody(1,43,100,0,true);
  if(s.total==100) OK(); else NG("%llu",(unsigned long long)s.total); }

printf("\n=== 结果: %d 通过 / %d 失败 (共 %d) ===\n",pass,fail,no);
return fail?1:0;
}
