#include "stub.h"
DBCStorage<ItemSetEntry> sItemSetStore;
DBCStorage<DungeonEncounterEntry> sDungeonEncounterStore;
DBCStorage<FactionEntry> sFactionStore;
DB CharacterDatabase, WorldDatabase;
ObjectMgr* sObjectMgr = nullptr;
#include "gearset_ns.inc"

static int pass=0, fail=0;
#define CHECK(cond, msg) do{ if(cond){printf("  [OK] %s\n",msg);pass++;} else {printf("  [!!] %s\n",msg);fail++;} }while(0)

int main(){
    printf("=== 1. 职业名解析 ===\n");
    CHECK(GearSet::ParseClassName("战士")==CLASS_WARRIOR, "中文 战士");
    CHECK(GearSet::ParseClassName("warrior")==CLASS_WARRIOR, "英文 warrior");
    CHECK(GearSet::ParseClassName("死骑")==CLASS_DEATH_KNIGHT, "简称 死骑");
    CHECK(GearSet::ParseClassName("小德")==CLASS_DRUID, "简称 小德");
    CHECK(GearSet::ParseClassName("xxx")==0, "未知职业返回0");

    printf("=== 2. 护甲类型映射 ===\n");
    CHECK(GearSet::GetArmorSubClassForClass(CLASS_WARRIOR)==ITEM_SUBCLASS_ARMOR_PLATE, "战士->板甲");
    CHECK(GearSet::GetArmorSubClassForClass(CLASS_HUNTER)==ITEM_SUBCLASS_ARMOR_MAIL, "猎人->锁甲");
    CHECK(GearSet::GetArmorSubClassForClass(CLASS_ROGUE)==ITEM_SUBCLASS_ARMOR_LEATHER, "盗贼->皮甲");
    CHECK(GearSet::GetArmorSubClassForClass(CLASS_MAGE)==ITEM_SUBCLASS_ARMOR_CLOTH, "法师->布甲");

    printf("=== 3. 定位评分 ===\n");
    ItemTemplate tankItem; tankItem.ItemLevel=264; tankItem.Quality=4;
    tankItem.ItemStat[0]={ITEM_MOD_STAMINA, 100}; tankItem.Armor=2000;
    ItemTemplate dpsItem; dpsItem.ItemLevel=264; dpsItem.Quality=4;
    dpsItem.ItemStat[0]={ITEM_MOD_STRENGTH, 100};
    double tankScoreT = GearSet::ScoreForRole(&tankItem, GearSet::ROLE_TANK);
    double tankScoreD = GearSet::ScoreForRole(&tankItem, GearSet::ROLE_DPS);
    CHECK(tankScoreT > tankScoreD, "耐力装 坦克分>输出分");
    double dpsScoreD = GearSet::ScoreForRole(&dpsItem, GearSet::ROLE_DPS);
    double dpsScoreH = GearSet::ScoreForRole(&dpsItem, GearSet::ROLE_HEAL);
    CHECK(dpsScoreD > dpsScoreH, "力量装 输出分>治疗分");
    CHECK(GearSet::ScoreForRole(nullptr, GearSet::ROLE_DPS) < 0, "空指针返回负分");

    printf("=== 4. 部位需求 ===\n");
    auto needs = GearSet::GetSlotNeeds();
    uint32 totalPieces=0; for(auto&n:needs) totalPieces+=n.count;
    CHECK(needs.size()==12, "12种部位");
    CHECK(totalPieces==14, "共14件(戒指饰品各2)");

    printf("=== 5. 武器需求 ===\n");
    auto rogueW = GearSet::GetWeaponNeeds(CLASS_ROGUE, GearSet::ROLE_DPS);
    CHECK(rogueW.size()==2, "盗贼双持2把");
    auto warTank = GearSet::GetWeaponNeeds(CLASS_WARRIOR, GearSet::ROLE_TANK);
    bool hasShield=false; for(auto&w:warTank) if(w.invType==INVTYPE_SHIELD) hasShield=true;
    CHECK(hasShield, "战士坦克带盾");
    auto warDps = GearSet::GetWeaponNeeds(CLASS_WARRIOR, GearSet::ROLE_DPS);
    CHECK(warDps[0].invType==INVTYPE_2HWEAPON, "战士输出双手武器");

    printf("=== 6. 宝石选择 ===\n");
    CHECK(GearSet::PickGemForSocket(SOCKET_COLOR_META, GearSet::ROLE_DPS)==GearSet::GEM_META, "meta孔->meta宝石");
    CHECK(GearSet::PickGemForSocket(SOCKET_COLOR_RED, GearSet::ROLE_HEAL)==GearSet::GEM_BLUE, "治疗红孔->蓝宝石");
    CHECK(GearSet::PickGemForSocket(SOCKET_COLOR_YELLOW, GearSet::ROLE_TANK)==GearSet::GEM_BLUE, "坦克黄孔->耐力");

    printf("=== 7. 副本Key编码 ===\n");
    CHECK(GearSet::DungeonKey(631,3) != GearSet::DungeonKey(631,0), "同图不同难度key不同");
    CHECK(GearSet::DungeonKey(631,3) != GearSet::DungeonKey(632,3), "不同图key不同");
    CHECK(GearSet::DungeonKey(631,3) == ((uint64(631)<<8)|3), "key编码正确");

    printf("=== 8. Gossip上限守护 ===\n");
    CHECK(GearSet::PER_PAGE + GearSet::NAV_SLOTS == GearSet::GOSSIP_HARD_LIMIT, "29+3=32不超限");
    CHECK(GearSet::GOSSIP_HARD_LIMIT==32, "硬上限32");

    printf("\n通过 %d 项，失败 %d 项\n", pass, fail);
    return fail?1:0;
}
