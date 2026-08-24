#include "mock.h"
ObjectMgrMock* sObjectMgr = new ObjectMgrMock();
DBMock CharacterDatabase;
namespace Trinity { template<typename...A> inline std::string StringFormat(char const* f, A...a){
    char b[256]; snprintf(b,sizeof(b),f,a...); return b; } }
#include "gear.cpp"
using namespace GearSet;
int main(){
    // 造各部位装备
    struct Def { uint32 inv; uint32 sub; char const* n; };
    Def defs[] = {
        {INVTYPE_HEAD,ITEM_SUBCLASS_ARMOR_PLATE,"板甲头盔"},
        {INVTYPE_CHEST,ITEM_SUBCLASS_ARMOR_PLATE,"板甲胸铠"},
        {INVTYPE_LEGS,ITEM_SUBCLASS_ARMOR_PLATE,"板甲腿铠"},
        {INVTYPE_HEAD,ITEM_SUBCLASS_ARMOR_CLOTH,"布甲兜帽"},
        {INVTYPE_CHEST,ITEM_SUBCLASS_ARMOR_CLOTH,"布甲长袍"},
        {INVTYPE_NECK,0,"项链"},{INVTYPE_FINGER,0,"戒指"},{INVTYPE_TRINKET,0,"饰品"},
        {INVTYPE_CLOAK,0,"披风"},
    };
    uint32 id=80000;
    for (auto&d:defs) for (int v=0;v<3;++v){
        ItemTemplate t; t.ItemId=id; t.Name1=std::string(d.n)+"_v"+std::to_string(v);
        t.Class=ITEM_CLASS_ARMOR; t.SubClass=d.sub; t.InventoryType=d.inv;
        t.Quality=ITEM_QUALITY_EPIC; t.ItemLevel=200+v*30; t.StatsCount=2;
        t.ItemStat[0].ItemStatType=ITEM_MOD_STAMINA; t.ItemStat[0].ItemStatValue=50+v*20;
        t.ItemStat[1].ItemStatType=ITEM_MOD_STRENGTH; t.ItemStat[1].ItemStatValue=40+v*15;
        t.Armor=1000+v*500;
        sObjectMgr->items[id]=t; ++id;
    }
    ChatHandler h; Player* p=h.GetSession()->GetPlayer();
    printf("=========== 战士/坦克/装等<=260 ===========\n");
    GenerateSet(&h,p,CLASS_WARRIOR,ROLE_TANK,260,false);
    printf("\n=========== 法师/治疗/不限装等 ===========\n");
    GenerateSet(&h,p,CLASS_MAGE,ROLE_HEAL,0,false);
    printf("\n=========== 职业名解析 ===========\n");
    char const* names[]={"战士","圣骑士","dk","法师","小德","不存在"};
    for(auto n:names) printf("  %-8s -> %s\n", n, GetClassName(ParseClass(n)));
    printf("\n=========== 护甲类型映射 ===========\n");
    uint8 cs[]={CLASS_WARRIOR,CLASS_HUNTER,CLASS_ROGUE,CLASS_MAGE};
    char const* an[]={"","布","皮","锁","板","","盾"};
    for(auto c:cs) printf("  %-10s -> %s甲\n", GetClassName(c), an[GetArmorSubClassForClass(c)]);
    return 0;
}
