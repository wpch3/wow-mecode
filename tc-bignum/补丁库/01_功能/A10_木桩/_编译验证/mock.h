/* mock.h —— 复刻 TrinityCore 真实签名，用于 Linux 侧编译验证 */
#ifndef MOCK_H
#define MOCK_H
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <set>
#include <array>
#include <unordered_map>
#include <algorithm>
#include <sstream>
#include <chrono>

typedef uint8_t uint8; typedef uint16_t uint16; typedef uint32_t uint32;
typedef uint64_t uint64; typedef int32_t int32; typedef int64_t int64;
typedef std::chrono::milliseconds Milliseconds;

enum LocaleConstant { LOCALE_enUS=0, DEFAULT_LOCALE=0 };
enum Stats { STAT_STRENGTH=0, STAT_AGILITY, STAT_STAMINA, STAT_INTELLECT, STAT_SPIRIT, MAX_STATS };
// UpdateFields.h 真实偏移（OBJECT_END=0x0006）
enum { OBJECT_END_MOCK = 0x0006 };
enum UpdateFieldsMock {
    UNIT_FIELD_MAXHEALTH            = OBJECT_END_MOCK + 0x001A,   // UpdateFields.h:104
    UNIT_FIELD_STAT0                = OBJECT_END_MOCK + 0x004E,   // :141
    UNIT_FIELD_RESISTANCES          = OBJECT_END_MOCK + 0x005D,   // :156
    UNIT_FIELD_ATTACK_POWER         = OBJECT_END_MOCK + 0x0075,   // :162
    UNIT_FIELD_RANGED_ATTACK_POWER  = OBJECT_END_MOCK + 0x007A,
    UNIT_END_MOCK                   = OBJECT_END_MOCK + 0x008E,
    PLAYER_SHIELD_BLOCK             = UNIT_END_MOCK + 0x037B,   // UpdateFields.h:355
};
enum ReactStates : uint8 { REACT_PASSIVE=0, REACT_DEFENSIVE=1, REACT_AGGRESSIVE=2 };  // UnitDefines.h:408
enum CurrentSpellTypes { CURRENT_MELEE_SPELL=0, CURRENT_GENERIC_SPELL=1 };            // Unit.h:608
enum SpellSchools { SPELL_SCHOOL_NORMAL=0, SPELL_SCHOOL_HOLY=1, MAX_SPELL_SCHOOL=7 };
enum SpellSchoolMask { SPELL_SCHOOL_MASK_NORMAL=1 };
enum WeaponAttackType { BASE_ATTACK=0, OFF_ATTACK=1, RANGED_ATTACK=2 };
enum Powers { POWER_MANA=0 };
enum CombatRating { CR_DODGE=2, CR_PARRY=3, CR_BLOCK=4, CR_HIT_MELEE=5,
                    CR_CRIT_MELEE=8, CR_HASTE_MELEE=17, CR_EXPERTISE=23,
                    CR_ARMOR_PENETRATION=24, MAX_COMBAT_RATING=25 };
// DBCEnums.h:255/258/268
enum AreaFlags { AREA_FLAG_CAPITAL=0x00000100, AREA_FLAG_SANCTUARY=0x00000800,
                 AREA_FLAG_TOWN=0x00200000 };
enum GameobjectTypes { GAMEOBJECT_TYPE_DOOR=0, GAMEOBJECT_TYPE_BUTTON=1 };
enum GOState : uint8 { GO_STATE_ACTIVE=0, GO_STATE_READY=1 };
enum NPCFlags : uint32 {                                    // UnitDefines.h:237起
    UNIT_NPC_FLAG_GOSSIP=0x00000001, UNIT_NPC_FLAG_VENDOR=0x00000080,
    UNIT_NPC_FLAG_REPAIR=0x00001000, UNIT_NPC_FLAG_BANKER=0x00020000,
    UNIT_NPC_FLAG_AUCTIONEER=0x00200000, UNIT_NPC_FLAG_MAILBOX=0x04000000 };
enum { FACTION_FRIENDLY=35, FACTION_MONSTER=14, FACTION_PREY=31 };  // SharedDefines.h:242/245/247
enum DamageEffectType { DIRECT_DAMAGE=0, DOT=1 };                               // SharedDefines.h:247
enum TypeID { TYPEID_UNIT=3, TYPEID_PLAYER=4 };
enum TempSummonType { TEMPSUMMON_MANUAL_DESPAWN=8, TEMPSUMMON_TIMED_DESPAWN=3 };
enum GOSummonType { GO_SUMMON_TIMED_OR_CORPSE_DESPAWN=0 };
enum { GAMEOBJECT_TYPE_MAILBOX=19 };                        // SharedDefines.h:1674
typedef std::chrono::seconds Seconds;
struct QuaternionData {                                     // Position.h
    float x=0,y=0,z=0,w=1;
    static QuaternionData fromEulerAnglesZYX(float,float,float){ return {}; } };
enum EnchantmentSlot : uint16 { PERM_ENCHANTMENT_SLOT=0, SOCK_ENCHANTMENT_SLOT=2 };
enum ItemClass { ITEM_CLASS_GEM=3 };
enum SocketColor { SOCKET_NONE=0 };
#define MAX_ITEM_PROTO_SOCKETS 3
enum EquipSlots { EQUIPMENT_SLOT_START=0, EQUIPMENT_SLOT_END=19 };
enum { INVENTORY_SLOT_BAG_0=255 };

struct ObjectGuid { uint64 v=0; static ObjectGuid Empty;
    uint32 GetCounter() const { return uint32(v); } };

// DBCStructure.h:176
struct AreaTableEntry { uint32 ID=0; uint32 Flags=0; char const* AreaName[16]={nullptr}; };
struct GemPropertiesEntry { uint32 ID=0; uint32 EnchantID=0; uint32 Type=0; };
struct SpellItemEnchantmentEntry { uint32 ID=0; };
struct SpellInfo { uint32 Id=0; char const* SpellName[16]={nullptr}; };
class Spell { public:                                            // Spell.h:456
    SpellInfo const* GetSpellInfo() const { return m_spellInfo; }
    SpellInfo const* m_spellInfo=nullptr; };

template<class T> struct DBCStorage {
    std::map<uint32,T> data;
    T const* LookupEntry(uint32 id) const { auto i=data.find(id); return i!=data.end()?&i->second:nullptr; }
};
extern DBCStorage<AreaTableEntry> sAreaTableStore;
extern DBCStorage<GemPropertiesEntry> sGemPropertiesStore;
extern DBCStorage<SpellItemEnchantmentEntry> sSpellItemEnchantmentStore;

namespace GameTime { uint32 GetGameTimeMS(); }

struct _Socket { uint32 Color=0; };
struct ItemTemplate {
    uint32 ItemId=0, Class=0, GemProperties=0;
    std::array<_Socket,MAX_ITEM_PROTO_SOCKETS> Socket{};
};

class SpellMgr { public:
    SpellInfo const* GetSpellInfo(uint32 id) const {
        static std::map<uint32,SpellInfo> c; auto&s=c[id]; s.Id=id;
        if(!s.SpellName[0])
            s.SpellName[0]="测试法术";
        return id?&s:nullptr; } };
extern SpellMgr* sSpellMgrInst;
#define sSpellMgr sSpellMgrInst

// GameObjectData.h:36 / :658 ；SharedDefines.h:1674 GAMEOBJECT_TYPE_MAILBOX=19
struct GameObjectTemplate { uint32 entry=0; uint32 type=0; };
typedef std::map<uint32,GameObjectTemplate> GameObjectTemplateContainer;

class ObjectMgr { public:
    ItemTemplate const* GetItemTemplate(uint32 id) const {
        static std::map<uint32,ItemTemplate> c; auto i=c.find(id);
        return i!=c.end()?&i->second:nullptr; }
    struct CreatureTemplate { uint32 Entry=0; };
    CreatureTemplate const* GetCreatureTemplate(uint32 id) const {
        static std::map<uint32,CreatureTemplate> c;
        auto i=c.find(id); return i!=c.end()?&i->second:nullptr; }
    std::map<uint32,CreatureTemplate>& CTStore() {
        static std::map<uint32,CreatureTemplate> s2; return s2; }
    std::map<uint32,ItemTemplate>& Store() { static std::map<uint32,ItemTemplate> s; return s; }
    // ObjectMgr.h:974-975
    GameObjectTemplate const* GetGameObjectTemplate(uint32 entry) const {
        auto& s = GOStore(); auto i=s.find(entry);
        return i!=s.end()?&i->second:nullptr; }
    GameObjectTemplateContainer const& GetGameObjectTemplates() const { return GOStore(); }
    static GameObjectTemplateContainer& GOStore() {
        static GameObjectTemplateContainer s; return s; } };
extern ObjectMgr* sObjectMgrInst;
#define sObjectMgr sObjectMgrInst

class Item { public:
    ItemTemplate const* GetTemplate() const { return _t; }             // Item.h:76
    void SetEnchantment(EnchantmentSlot, uint32, uint32, uint32,
                        ObjectGuid = ObjectGuid::Empty) { ++enchCount; } // Item.h:146
    ItemTemplate const* _t=nullptr; static uint32 enchCount; };

class Player; class Unit; class Creature; class Group; class TempSummon; class Spell;

// PassiveAI.h:53 / UnitAI.h:225
class NullCreatureAI { public:
    explicit NullCreatureAI(Creature* c) : me(c) { ++aiCreated; }
    virtual ~NullCreatureAI() = default;
    virtual void DamageTaken(Unit*, uint32&, DamageEffectType, SpellInfo const*) {}
    virtual void UpdateAI(uint32) {}
    Creature* me;
    static uint32 aiCreated; };

// ScriptMgr.h:395  class TC_GAME_API UnitScript : public ScriptObject
class UnitScript { public:
    explicit UnitScript(char const* n) : _name(n) { ++registered; }
    virtual ~UnitScript() = default;
    virtual void OnDamage(Unit*, Unit*, uint32&) {}              // ScriptMgr.h:397
    char const* _name;
    static uint32 registered; };

class Map { public:
    bool IsDungeon() const { return _d; }   // Map.h:446
    bool IsRaid() const { return _r; }      // Map.h:448
    bool _d=false,_r=false; };

class WorldObject { public:
    ObjectGuid GetGUID() const { return _guid; }
    uint32 GetZoneId() const { return _zone; }   // WorldObject.h:389
    uint32 GetAreaId() const { return _area; }   // WorldObject.h:390
    float GetPositionX() const { return 0.f; }
    float GetPositionY() const { return 0.f; }
    float GetPositionZ() const { return 0.f; }
    float GetOrientation() const { return 0.f; }
    Map* GetMap() const { return _map; }         // WorldObject.h:467
    bool IsInWorld() const { return true; }
    bool IsInMap(WorldObject const*) const { return true; }   // WorldObject.h:413
    bool IsFriendlyTo(WorldObject const*) const { return _friendly; }  // WorldObject.h:528
    std::string const& GetName() const { return _name; }        // WorldObject.h:398
    void SetName(std::string n) { _name=std::move(n); }         // WorldObject.h:399
    TypeID GetTypeId() const { return _tid; }
    ObjectGuid _guid; uint32 _zone=0,_area=0; Map* _map=nullptr; bool _friendly=false;
    std::string _name="测试单位"; TypeID _tid=TYPEID_UNIT; };

class Unit : public WorldObject { public:
    bool IsAlive() const { return _hp>0; }
    bool IsPet() const { return _pet; }
    bool IsTotem() const { return _totem; }
    uint32 GetMaxHealth() const { return _maxhp; }
    uint32 GetMaxPower(Powers) const { return _maxmana; }
    uint32 GetArmor() const { return 5000; }                    // Unit.h:906
    uint32 GetResistance(SpellSchools) const { return 100; }
    float GetStat(Stats) const { return 100.f; }                // Unit.h:904
    float GetTotalStatValue(Stats) const { return 500.f; }      // Unit.h:1538
    float GetTotalAttackPowerValue(WeaponAttackType) const { return 2000.f; } // Unit.h:1562
    int32 SpellBaseDamageBonusDone(SpellSchoolMask) const { return 1500; }    // Unit.h:1624
    void CastSpell(Unit*, uint32, bool) { ++castCount; }
    static void Kill(Unit*, Unit* victim, bool = true) { victim->_hp=0; ++killCount; }  // Unit.h:1023
    TempSummon* ToTempSummon() { return _isTemp?(TempSummon*)this:nullptr; }  // Unit.h:1806
    // Object.h:114-117  字段实际存储（诊断要读回真实值）
    std::map<uint16,int32> _fields;
    int32  GetInt32Value(uint16 i) const { auto it=_fields.find(i); return it!=_fields.end()?it->second:0; }
    uint32 GetUInt32Value(uint16 i) const { return uint32(GetInt32Value(i)); }
    void   SetInt32Value(uint16 i, int32 v) { _fields[i]=v; }
    void   SetArmor(int32 v) { SetInt32Value(UNIT_FIELD_RESISTANCES+SPELL_SCHOOL_NORMAL, v); }  // Unit.h:907
    void   SetImmuneToNPC(bool) {}                              // Unit.h:1142
    void   SetImmuneToAll(bool) {}                              // Unit.h:1136
    void   SetDisplayId(uint32 m) { _display=m; }               // Unit.h:1595
    void   SetNativeDisplayId(uint32 m) { _native=m; }          // Unit.h:1598
    uint32 _display=0,_native=0;
    uint8  GetLevel() const { return _level; }                  // Unit.h:890
    uint8  _level=80;
    Spell* GetCurrentSpell(CurrentSpellTypes) const { return _curSpell; }  // Unit.h:1488
    Spell* _curSpell=nullptr;
    uint32 _hp=1000,_maxhp=1000,_maxmana=1000;
    bool _pet=false,_totem=false,_isTemp=false;
    void SetFaction(uint32 f) { _faction=f; }                   // Unit.h:975
    void ReplaceAllNpcFlags(NPCFlags f) { _npcFlags=f; }        // Unit.h:1100
    uint32 _faction=0; uint32 _npcFlags=0;
    static uint32 killCount, castCount; };

class TempSummon : public Unit { };

class Creature : public Unit { public:
    void SetReactState(ReactStates s) { _react=s; }             // Creature.h:134
    bool AIM_Initialize(NullCreatureAI* ai) { _ai=ai; ++aimCalls; return true; }  // Creature.h:166
    NullCreatureAI* _ai=nullptr;
    static uint32 aimCalls;
    ReactStates _react=REACT_AGGRESSIVE;
    uint32 GetSpawnId() const { return _spawnId; }              // Creature.h:98
    bool IsDungeonBoss() const { return _boss; }                // Creature.h:129
    bool IsTrigger() const { return _trigger; }                 // Creature.h:113
    bool IsNPCBotOrPet() const { return _bot; }                 // Creature.h:394
    void DespawnOrUnsummon(Milliseconds = Milliseconds(0)) { ++despawnCount; }  // Creature.h:268
    static bool DeleteFromDB(uint32) { ++deleteCount; return true; }  // Creature.h:222
    uint32 _spawnId=0; bool _boss=false,_trigger=false,_bot=false;
    static uint32 despawnCount, deleteCount; };

class GameObject : public WorldObject { public:
    GameobjectTypes GetGoType() const { return _t; }   // GameObject.h:176
    void SetGoState(GOState s) { _state=s; ++stateCount; }  // GameObject.h:179
    GameobjectTypes _t=GAMEOBJECT_TYPE_DOOR; GOState _state=GO_STATE_READY;
    static uint32 stateCount; };

class GroupReference { public:
    Player* GetSource() { return _p; }
    GroupReference* next() { return _n; }
    Player* _p=nullptr; GroupReference* _n=nullptr; };

class GroupBotReference { public:                     // GroupRefManager.h:48
    Creature* GetSource() { return _c; }              // Reference.h:96
    GroupBotReference* next() { return _n; }          // GroupRefManager.h:55
    Creature* _c=nullptr; GroupBotReference* _n=nullptr; };

class Group { public:
    GroupReference* GetFirstMember() { return _f; }        // Group.h:257
    GroupBotReference* GetFirstBotMember() { return _bf; } // Group.h:202
    uint32 GetMembersCount() const { return _c; }          // Group.h:259
    GroupReference* _f=nullptr; GroupBotReference* _bf=nullptr; uint32 _c=1; };

class WorldSession { public:
    Player* GetPlayer() const;
    LocaleConstant GetSessionDbcLocale() const { return DEFAULT_LOCALE; }
    Player* _p=nullptr; };

class Player : public Unit { public:
    WorldSession* GetSession() const { return _s; }
    Group* GetGroup() { return _g; }                            // Player.h:2155
    Item* GetItemByPos(uint8, uint8 slot) const {               // Player.h:1082
        auto i=_items.find(slot); return i!=_items.end()?i->second:nullptr; }
    void ApplyEnchantment(Item*, EnchantmentSlot, bool, bool=true, bool=false) {} // Player.h:1205
    float GetRatingMultiplier(CombatRating) const { return 0.05f; }
    float GetRatingBonusValue(CombatRating) const { return 12.5f; }  // Player.h:1664
    TempSummon* SummonCreature(uint32, float,float,float,float=0,
        TempSummonType=TEMPSUMMON_MANUAL_DESPAWN, Milliseconds=Milliseconds(0),
        ObjectGuid=ObjectGuid::Empty) { ++summonCount; static TempSummon t; return &t; }  // WorldObject.h:476
    GameObject* SummonGameObject(uint32, float,float,float,float,
        QuaternionData const&, Seconds,
        GOSummonType=GO_SUMMON_TIMED_OR_CORPSE_DESPAWN)     // WorldObject.h:478
    { ++goSummonCount; static GameObject g; return &g; }
    static uint32 goSummonCount;
    WorldSession* _s=nullptr; Group* _g=nullptr;
    Player(){ _tid=TYPEID_PLAYER; }
    std::map<uint8,Item*> _items;
    static uint32 summonCount; };

inline Player* WorldSession::GetPlayer() const { return _p; }

class ChatHandler { public:
    ChatHandler() {} explicit ChatHandler(WorldSession* s):_s(s){}
    WorldSession* GetSession() { return _s; }
    LocaleConstant GetSessionDbcLocale() const { return DEFAULT_LOCALE; }
    template<typename...A> void PSendSysMessage(char const* f, A...a)
    { if(verbose){printf("    ");printf(f,a...);printf("\n");} ++lines; }
    void PSendSysMessage(char const* f){ if(verbose)printf("    %s\n",f); ++lines; }
    bool ParseCommands(std::string const& c){ parsed.push_back(c); return true; }
    WorldSession* _s=nullptr;
    static bool verbose; static uint32 lines; static std::vector<std::string> parsed; };

// 格子搜索
namespace Trinity {
    class AnyUnitInObjectRangeCheck { public:
        AnyUnitInObjectRangeCheck(WorldObject const*, float) {} };
    class GameObjectInRangeCheck { public:
        GameObjectInRangeCheck(float,float,float,float,uint32=0) {} };
    template<class C> struct CreatureListSearcher {
        CreatureListSearcher(WorldObject const*, std::list<Creature*>& c, C&):_c(&c){}
        std::list<Creature*>* _c; };
    template<class C> struct GameObjectListSearcher {
        GameObjectListSearcher(WorldObject const*, std::list<GameObject*>& c, C&):_c(&c){}
        std::list<GameObject*>* _c; };
}
extern std::vector<Creature*> g_nearCreatures;
extern std::vector<GameObject*> g_nearGOs;

class Cell { public:
    template<class T> static void VisitAllObjects(WorldObject const*, T& v, float, bool=true)
    { Fill(v); }
    template<class C> static void Fill(Trinity::CreatureListSearcher<C>& s)
    { for(Creature* c:g_nearCreatures) s._c->push_back(c); }
    template<class C> static void Fill(Trinity::GameObjectListSearcher<C>& s)
    { for(GameObject* g:g_nearGOs) s._c->push_back(g); } };

struct ChatCommand { char const* Name; uint32 Permission; bool AllowConsole;
    bool (*Handler)(ChatHandler*, char const*); std::string Help; };
class ScriptObject { protected: explicit ScriptObject(char const*){} };
class CommandScript : public ScriptObject { public:
    explicit CommandScript(char const* n):ScriptObject(n){}
    virtual std::vector<ChatCommand> GetCommands() const = 0; };
namespace rbac { enum { RBAC_PERM_COMMAND_WORLDTOOLS = 71012 }; }
#endif
