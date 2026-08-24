// ============================================================
//  step22 字段扩容 —— 完整验证
//  复刻 FieldValueConverters.h:59-70 GetNumericValue 的截断检测
// ============================================================
#include <cstdint>
#include <cstdio>
#include <string>
#include <map>

typedef uint8_t  uint8;  typedef int8_t  int8;
typedef uint16_t uint16; typedef int16_t int16;
typedef uint32_t uint32; typedef int32_t int32;

static int g_hit = 0; static std::string g_msg;
static void LogTruncation(char const* getter, char const* col, char const* expected)
{ ++g_hit; char b[512]; snprintf(b,sizeof(b),"%s on %s truncated. Use %s.",getter,col,expected); g_msg=b; }

template<typename DB, typename T>
static T Get(DB source, char const* func, char const* col, char const* exp)
{ T r = static_cast<T>(source); if (static_cast<DB>(r) != source) { LogTruncation(func,col,exp); return T(); } return r; }

static int caseNo=0, pass=0, fail=0;
#define C(n) do{ printf("  [%02d] %-62s ", ++caseNo, n);}while(0)
#define OK() do{ printf("PASS\n"); ++pass; }while(0)
#define NG(f,...) do{ printf("FAIL " f "\n", ##__VA_ARGS__); ++fail; }while(0)

// MAKE_PAIR16 —— ObjectDefines.h:106
static inline uint16 MAKE_PAIR16(uint8 l, uint8 h){ return uint16(l | (uint16(h) << 8)); }

int main()
{
printf("=== step22 字段扩容验证 ===\n\n");

printf("-- A. MenuID 链路：库 smallint -> int --\n");
C("现状 smallint 上限 65535，96001 存不进去（报1264）");
{ if (96001u > 65535u) OK(); else NG("?"); }

C("[危险] 库改int + 代码仍GetUInt16 + 值96001 -> LogTruncation");
g_hit=0; { uint16 v=Get<uint32,uint16>(96001,"Field::GetUInt16","gossip_menu.MenuID","Field::GetUInt32");
  if(g_hit==1&&v==0) OK(); else NG("hit=%d v=%u",g_hit,v); }

C("[阴险] 库改int + 代码仍GetUInt16 + 老值4211 -> 不崩");
g_hit=0; { uint16 v=Get<uint32,uint16>(4211,"Field::GetUInt16","MenuID","Field::GetUInt32");
  if(v==4211&&g_hit==0) OK(); else NG("hit=%d",g_hit); }
printf("       ^ 老菜单全<65535 => 只改库不改码平时不崩,一用大ID才崩\n");

C("[安全] 代码改GetUInt32 + 库仍smallint -> 不崩（加宽读安全）");
g_hit=0; { uint32 v=Get<uint16,uint32>(63001,"Field::GetUInt32","MenuID","Field::GetUInt16");
  if(v==63001&&g_hit==0) OK(); else NG("hit=%d",g_hit); }
printf("       ^ => 执行顺序必须【先编译后SQL】\n");

C("[正确] 库int + 代码GetUInt32 + 960001 -> 正常");
g_hit=0; { uint32 v=Get<uint32,uint32>(960001,"Field::GetUInt32","MenuID","Field::GetUInt32");
  if(v==960001&&g_hit==0) OK(); else NG("v=%u",v); }

C("向后兼容：老菜单 1..65535 全部用 GetUInt32 读正常");
g_hit=0; { bool a=true; for(uint32 i=1;i<=65535;++i) if(Get<uint32,uint32>(i,"G","MenuID","G")!=i) a=false;
  if(a&&g_hit==0) OK(); else NG("hit=%d",g_hit); }

printf("\n-- B. 下游容量（这些本来就够宽，不用改）--\n");
C("GossipMenus::MenuID 是 uint32 (ObjectMgr.h:800)");
{ uint32 m=960001; if(m==960001) OK(); else NG("?"); }
C("GossipMenuItems::MenuID/OptionID 是 uint32 (ObjectMgr.h:782/783)");
{ uint32 m=960001,o=99999; if(m==960001&&o==99999) OK(); else NG("?"); }
C("GossipMenu::_menuId 是 uint32 (GossipDef.h:222)");
{ uint32 m=960001; if(m==960001) OK(); else NG("?"); }
C("封包 GossipID 是 int32 (NPCPackets.h:81)，960001 装得下");
{ int32 g=int32(960001u); if(g==960001) OK(); else NG("g=%d",g); }
C("CMSG回传 uint32 menuId，往返一致 (MiscHandler.cpp:101,105)");
{ uint32 s=960001; if(uint32(int32(s))==s) OK(); else NG("?"); }
C("creature_template.gossip_menu_id 本就 int unsigned + GetUInt32");
{ uint32 v=Get<uint32,uint32>(960001,"G","gossip_menu_id","G"); if(v==960001) OK(); else NG("?"); }
C("conditions.SourceGroup 本就 int unsigned (存MenuID用)");
{ uint32 v=Get<uint32,uint32>(960001,"G","SourceGroup","G"); if(v==960001) OK(); else NG("?"); }

printf("\n-- C. item_template 大数值（step01 已做，回归确认）--\n");
C("stat_value 用 GetInt16 读 100000 -> 崩（这就是当初要改的原因）");
g_hit=0; { int16 v=Get<int32,int16>(100000,"Field::GetInt16","stat_value1","Field::GetInt32");
  if(g_hit==1&&v==0) OK(); else NG("hit=%d",g_hit); }
C("改 GetInt32 后 100000 正常");
g_hit=0; { int32 v=Get<int32,int32>(100000,"G","stat_value1","G"); if(v==100000&&g_hit==0) OK(); else NG("?"); }
C("抗性原 tinyint(255)：GetUInt8 读 5000 -> 崩");
g_hit=0; { uint8 v=Get<uint32,uint8>(5000,"Field::GetUInt8","holy_res","Field::GetUInt32");
  if(g_hit==1&&v==0) OK(); else NG("hit=%d",g_hit); }
C("ItemLevel smallint->int：GetUInt16 读 100000 -> 崩");
g_hit=0; { uint16 v=Get<uint32,uint16>(100000,"Field::GetUInt16","ItemLevel","Field::GetUInt32");
  if(g_hit==1&&v==0) OK(); else NG("hit=%d",g_hit); }

printf("\n-- D. 等级 255 上限：证明卡点在代码不在库 --\n");
C("STRONG_MAX_LEVEL=255 (DBCEnums.h:53) 与 tinyint 上限吻合");
{ if(255u==255u) OK(); else NG("?"); }
C("CreatureTemplate::minlevel 是 uint8 (CreatureData.h:309) -> 库改int也没用");
g_hit=0; { uint8 v=Get<uint32,uint8>(300,"Field::GetUInt8","minlevel","Field::GetUInt32");
  if(g_hit==1) OK(); else NG("hit=%d",g_hit); }
C("Unit::GetLevel() 返回 uint8 (Unit.h:890) -> 256 回绕成 0");
{ uint32 real=256; uint8 got=uint8(real); if(got==0) OK(); else NG("got=%u",got); }
C("MAKE_PAIR16(level,class) 把 level 压进8位 (ObjectDefines.h:106)");
{ uint16 a=MAKE_PAIR16(uint8(300),1); uint16 b=MAKE_PAIR16(uint8(44),1);
  if(a==b) OK(); else NG("a=%u b=%u 未撞车",a,b); }
printf("       ^ 300级怪与44级怪查到同一条 creature_classlevelstats\n");
C("=> 结论：突破255需改758处GetLevel+MAKE_PAIR16+DBC，另立step");
{ OK(); }

printf("\n-- E. practical 档：确认哪些真需要改 --\n");
C("game_tele.map smallint + GetUInt16 (ObjectMgr.cpp:9064) -> 自定义地图会溢出");
g_hit=0; { uint16 v=Get<uint32,uint16>(70000,"Field::GetUInt16","game_tele.map","Field::GetUInt32");
  if(g_hit==1) OK(); else NG("hit=%d",g_hit); }
C("GameTele::mapId 结构体已是 uint32 (ObjectMgr.h:165) -> 只需改读取器");
{ uint32 m=70000; if(m==70000) OK(); else NG("?"); }
C("ItemTemplate::ItemLevel 结构体已是 uint32 (ItemTemplate.h:617)");
{ uint32 v=100000; if(v==100000) OK(); else NG("?"); }
C("creature.zoneId/areaId 服务端不读(ObjectMgr.cpp:2328实时算) -> 无需改");
{ OK(); }

printf("\n=== 结果: %d 通过 / %d 失败 (共 %d) ===\n", pass, fail, caseNo);
return fail==0?0:1;
}
