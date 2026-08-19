#include "mock.h"
ObjectMgrMock* sObjectMgr = new ObjectMgrMock();
DBMock CharacterDatabase;
namespace Trinity { template<typename...A> inline std::string StringFormat(char const* f, A...a){
    char b[256]; snprintf(b,sizeof(b),f,a...); return b; } }
#include "gear.cpp"
using namespace GearSet;
int main(){
    ChatHandler h; Player* p=h.GetSession()->GetPlayer();
    Map* m = p->GetMap();
    struct C { uint32 id; bool d,r,hc; char const* name; };
    C cases[] = {
        {0,false,false,false,"野外(80级)"},
        {631,false,true,true, "冰冠堡垒 25H"},
        {631,false,true,false,"冰冠堡垒 普通"},
        {603,false,true,true, "奥杜尔 英雄"},
        {533,false,true,false,"纳克萨玛斯"},
        {580,false,true,false,"太阳井"},
        {409,false,true,false,"熔火之心"},
        {601,true,false,true, "魔枢 英雄"},
        {601,true,false,false,"魔枢 普通"},
        {999,true,false,false,"未知副本"},
    };
    printf("=== 副本 -> 推荐装等 ===\n");
    for (auto&c:cases){
        m->id=c.id; m->dungeon=c.d; m->raid=c.r; m->heroic=c.hc;
        printf("  %-16s -> %s / 装等 %u\n", c.name, GetLocationDesc(p), SuggestIlvlForPlayer(p));
    }
    printf("\n=== 等级 -> 推荐装等（野外，向下兼容）===\n");
    m->id=0; m->dungeon=false; m->raid=false; m->heroic=false;
    printf("  说明: mock固定80级，真实环境按 GetLevel() 变化\n");
    printf("  20级->25  40级->45  60级->92  70级->146  80级->232\n");
    printf("  100级->632  255级->3732  (等级255改造后线性放大)\n");
    return 0;
}
