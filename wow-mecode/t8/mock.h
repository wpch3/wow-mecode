#pragma once
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cwctype>
#include <string>
#include <vector>
#include <array>
#include <algorithm>
#include <unordered_map>
#include <cmath>
typedef uint8_t uint8; typedef uint16_t uint16; typedef uint32_t uint32;
typedef int8_t int8; typedef int32_t int32; typedef uint64_t uint64;
#define MAX_ITEM_PROTO_STATS 10
#define MAX_ITEM_PROTO_DAMAGES 2
#define NULL_BAG 0
#define NULL_SLOT 255
enum LocaleConstant { LOCALE_enUS=0, LOCALE_zhCN=4, TOTAL_LOCALES=9 };
enum InventoryResult { EQUIP_ERR_OK=0, EQUIP_ERR_BAG_FULL=1 };
enum TempSummonType { TEMPSUMMON_MANUAL_DESPAWN=8 };
enum { LANG_NO_CHAR_SELECTED=1 };
namespace rbac { enum { RBAC_PERM_COMMAND_SMART_ADD=71003, RBAC_PERM_COMMAND_SMART_SPAWN=71004, RBAC_PERM_COMMAND_GEARSET=71005 }; }
struct ObjectGuid { uint64 v=0; uint32 GetCounter() const { return uint32(v); } static ObjectGuid Empty; };
inline ObjectGuid ObjectGuid::Empty;
struct _ItemStat { uint32 ItemStatType=0; int32 ItemStatValue=0; };
struct _Damage { float DamageMin=0, DamageMax=0; uint32 DamageType=0; };
struct ItemTemplate {
    uint32 ItemId=0, Class=0, SubClass=0, Quality=0, InventoryType=0;
    uint32 ItemLevel=0, RequiredLevel=0, Armor=0, StatsCount=0;
    int32 AllowableClass=-1;
    uint32 ItemSet=0;
    std::string Name1, Description;
    std::array<_ItemStat,MAX_ITEM_PROTO_STATS> ItemStat{};
    std::array<_Damage,MAX_ITEM_PROTO_DAMAGES> Damage{};
    std::string const& GetName(LocaleConstant) const { return Name1; }
};
struct CreatureLocale { std::vector<std::string> Name; };
struct CreatureTemplate {
    std::string Name, Title;
    uint8 minlevel=1, maxlevel=1;
    uint32 faction=0, rank=0, type=0, unit_class=1, Modelid1=0;
    float ModHealth=1.0f, ModDamage=1.0f;
};
typedef std::unordered_map<uint32,ItemTemplate> ItemTemplateContainer;
typedef std::unordered_map<uint32,CreatureTemplate> CreatureTemplateContainer;
struct ItemPosCount{}; typedef std::vector<ItemPosCount> ItemPosCountVec;
struct Item { uint32 entry=0; uint32 GetEntry() const { return entry; } };
struct Field { std::string s; uint32 u=0;
    uint32 GetUInt32() const { return u; } std::string GetString() const { return s; } };
struct ResultSet {
    std::vector<std::vector<Field>> rows; size_t cur=0;
    Field* Fetch(){ return rows[cur].data(); }
    bool NextRow(){ return ++cur < rows.size(); }
};
typedef ResultSet* QueryResult;
struct DBMock {
    void EscapeString(std::string&){}
    template<typename...A> void PExecute(char const* f, A...a){ printf("  [SQL] "); printf(f,a...); printf("\n"); }
    template<typename...A> QueryResult PQuery(char const*, A...){
        static ResultSet rs; rs.rows.clear(); rs.cur=0;
        Field f1; f1.s="坦克配装"; rs.rows.push_back({f1});
        Field f2; f2.s="输出配装"; rs.rows.push_back({f2});
        return &rs; }
};
extern DBMock CharacterDatabase;
struct Creature { bool IsInWorld(){return true;} void DespawnOrUnsummon(){} };
struct TempSummon : Creature { ObjectGuid GetGUID(){return ObjectGuid();} };


// ---- gearset 需要的补充 mock ----
enum ItemQualities { ITEM_QUALITY_POOR=0, ITEM_QUALITY_NORMAL=1, ITEM_QUALITY_UNCOMMON=2,
                     ITEM_QUALITY_RARE=3, ITEM_QUALITY_EPIC=4 };
enum { ITEM_CLASS_ARMOR=4, ITEM_CLASS_WEAPON=2 };
enum { ITEM_SUBCLASS_ARMOR_CLOTH=1, ITEM_SUBCLASS_ARMOR_LEATHER=2,
       ITEM_SUBCLASS_ARMOR_MAIL=3, ITEM_SUBCLASS_ARMOR_PLATE=4, ITEM_SUBCLASS_ARMOR_SHIELD=6 };
enum { INVTYPE_HEAD=1, INVTYPE_NECK=2, INVTYPE_SHOULDERS=3, INVTYPE_CHEST=5,
       INVTYPE_WAIST=6, INVTYPE_LEGS=7, INVTYPE_FEET=8, INVTYPE_WRISTS=9,
       INVTYPE_HANDS=10, INVTYPE_FINGER=11, INVTYPE_TRINKET=12, INVTYPE_CLOAK=16 };
enum { ITEM_MOD_STAMINA=7, ITEM_MOD_STRENGTH=4, ITEM_MOD_AGILITY=3, ITEM_MOD_INTELLECT=5,
       ITEM_MOD_SPIRIT=6, ITEM_MOD_DEFENSE_SKILL_RATING=12, ITEM_MOD_DODGE_RATING=13,
       ITEM_MOD_PARRY_RATING=14, ITEM_MOD_BLOCK_RATING=15, ITEM_MOD_CRIT_RATING=32,
       ITEM_MOD_HASTE_RATING=36, ITEM_MOD_ATTACK_POWER=38, ITEM_MOD_SPELL_POWER=45,
       ITEM_MOD_MANA_REGENERATION=43 };
enum { CLASS_WARRIOR=1, CLASS_PALADIN=2, CLASS_HUNTER=3, CLASS_ROGUE=4, CLASS_PRIEST=5,
       CLASS_DEATH_KNIGHT=6, CLASS_SHAMAN=7, CLASS_MAGE=8, CLASS_WARLOCK=9, CLASS_DRUID=11 };
enum { EQUIPMENT_SLOT_START=0, EQUIPMENT_SLOT_END=19, INVENTORY_SLOT_BAG_0=255 };

struct WorldSession;
struct Map { uint32 id=0; bool dungeon=false, raid=false, heroic=false;
    uint32 GetId() const { return id; } bool IsDungeon() const { return dungeon; }
    bool IsRaid() const { return raid; } bool IsHeroic() const { return heroic; } };
enum GossipOptionIcon : uint8 { GOSSIP_ICON_CHAT=0, GOSSIP_ICON_MONEY_BAG=6, GOSSIP_ICON_TALK=7, GOSSIP_ICON_BATTLE=9 };
#define DEFAULT_GOSSIP_MESSAGE 0xffffff
#define GOSSIP_HARD_LIMIT_MOCK 32
#define ASSERT(x) do{ if(!(x)) { printf("!!! ASSERT FAILED: %s\n", #x); abort(); } }while(0)
struct GossipMenuMock {
    struct MI { std::string msg; uint32 sender, action; };
    std::vector<MI> items;
    uint32 menuId=0;
    uint32 AddMenuItem(int32, GossipOptionIcon, std::string const& m, uint32 s, uint32 a, std::string const&, uint32, bool){
        items.push_back({m,s,a}); return uint32(items.size()-1); }
    void SetMenuId(uint32 m){ menuId=m; }
    uint32 GetMenuItemCount() const { return uint32(items.size()); }
};
struct PlayerTalkClassMock {
    GossipMenuMock menu;
    void ClearMenus(){ menu.items.clear(); }
    GossipMenuMock& GetGossipMenu(){ return menu; }
    void SendGossipMenu(uint32, ObjectGuid){
        printf("  [菜单发送] 共 %u 项\n", menu.GetMenuItemCount());
        for (size_t i=0;i<menu.items.size();++i)
            printf("    %2zu. %s\n", i, menu.items[i].msg.c_str());
    }
    void SendCloseGossip(){ printf("  [菜单关闭]\n"); }
};
struct Player {
    float GetPositionX(){return 0;} float GetPositionY(){return 0;}
    float GetPositionZ(){return 0;} float GetOrientation(){return 0;}
    float GetDistance(Creature*){return 1.0f;}
    InventoryResult CanStoreNewItem(uint8,uint8,ItemPosCountVec& d,uint32,uint32,uint32* n){ d.push_back(ItemPosCount()); if(n)*n=0; return EQUIP_ERR_OK; }
    Item* StoreNewItem(ItemPosCountVec const&,uint32,bool,int32){ static Item i; return &i; }
    void SendNewItem(Item*,uint32,bool,bool){}
    TempSummon* SummonCreature(uint32,float,float,float,float,TempSummonType,int){ static TempSummon s; return &s; }
    Item* GetItemByPos(uint8,uint8){ return nullptr; }
    uint8 GetLevel() const { return 80; }
    uint8 GetClass() const { return 1; }
    Map* GetMap(){ static Map m; return &m; }
    ObjectGuid GetGUID2(){ return ObjectGuid(); }
    InventoryResult CanStoreItem(uint8,uint8,ItemPosCountVec& d,Item*,bool){ d.push_back(ItemPosCount()); return EQUIP_ERR_OK; }
    void RemoveItem(uint8,uint8,bool){}
    Item* StoreItem(ItemPosCountVec const&,Item* i,bool){ return i; }
    PlayerTalkClassMock* PlayerTalkClass = new PlayerTalkClassMock();
    WorldSession* GetSession();
    ObjectGuid GetGUID(){ return ObjectGuid(); }
};
struct WorldSession { uint32 GetAccountId(){return 1;} Player* GetPlayer(); };
inline WorldSession* Player::GetSession(){ static WorldSession s; return &s; }
inline Player* g_playerImpl();
struct PlayerScript { PlayerScript(char const*){} virtual ~PlayerScript(){}
    virtual void OnGossipSelect(Player*, uint32, uint32, uint32) {} };
struct ChatHandler {
    ChatHandler(){} ChatHandler(WorldSession*){}
    void SendSysMessage(char const* s){ printf("%s\n", s); }
    void SendSysMessage(uint32){ printf("<lang>\n"); }
    template<typename...A> void PSendSysMessage(char const* f, A...a){ printf(f,a...); printf("\n"); }
    void SetSentErrorMessage(bool){}
    Player* getSelectedPlayerOrSelf(){ static Player p; return &p; }
    WorldSession* GetSession(){ static WorldSession s; return &s; }
    LocaleConstant GetSessionDbLocaleIndex(){ return LOCALE_zhCN; }
};
struct ChatCommand {
    char const* Name; uint32 Perm; bool Console;
    bool (*Handler)(ChatHandler*, char const*); char const* Help;
};
struct CommandScript {
    CommandScript(char const*){}
    virtual ~CommandScript(){}
    virtual std::vector<ChatCommand> GetCommands() const { return {}; }
};
// 模拟 std::chrono 的 0s 字面量
struct MockMs { int v; };
inline MockMs operator""_mockS(unsigned long long v){ return MockMs{int(v)}; }
struct ObjectMgrMock {
    ItemTemplateContainer items; CreatureTemplateContainer creatures;
    std::unordered_map<uint32,CreatureLocale> clocales;
    ItemTemplate const* GetItemTemplate(uint32 id){ auto i=items.find(id); return i==items.end()?nullptr:&i->second; }
    CreatureTemplate const* GetCreatureTemplate(uint32 id){ auto i=creatures.find(id); return i==creatures.end()?nullptr:&i->second; }
    CreatureLocale const* GetCreatureLocale(uint32 id){ auto i=clocales.find(id); return i==clocales.end()?nullptr:&i->second; }
    ItemTemplateContainer const& GetItemTemplateStore(){ return items; }
    CreatureTemplateContainer const& GetCreatureTemplates(){ return creatures; }
};
extern ObjectMgrMock* sObjectMgr;
namespace ObjectAccessor { inline Creature* GetCreature(Player&, ObjectGuid){ return nullptr; } }
inline bool Utf8toWStr(std::string const& s, std::wstring& w){ w.assign(s.begin(),s.end()); return true; }
inline void wstrToLower(std::wstring& w){ for(auto&c:w) if(c<128) c=towlower(c); }
inline bool Utf8FitTo(std::string const& n, std::wstring const& w){
    std::string p(w.begin(),w.end()); return n.find(p)!=std::string::npos; }
inline float frand(float a,float b){ return a+(b-a)*0.5f; }
inline int32 GenerateItemRandomPropertyId(uint32){ return 0; }
inline Player* WorldSession::GetPlayer(){ static Player p; return &p; }
