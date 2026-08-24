/*
 * mock.h —— 复刻 TrinityCore 真实签名的最小桩件，用来在 Linux 上做编译验证。
 * 每个签名都对齐了源码里的行号（注释标出），不是随手写的。
 */
#ifndef MOCK_H
#define MOCK_H

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <algorithm>
#include <sstream>
#include <functional>
#include <chrono>
#include <list>

typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;
typedef int8_t   int8;
typedef int32_t  int32;
typedef int64_t  int64;

typedef std::chrono::milliseconds Milliseconds;

// ---------------- SharedDefines ----------------
enum Classes : uint8
{
    CLASS_WARRIOR = 1, CLASS_PALADIN = 2, CLASS_HUNTER = 3, CLASS_ROGUE = 4,
    CLASS_PRIEST = 5, CLASS_DEATH_KNIGHT = 6, CLASS_SHAMAN = 7, CLASS_MAGE = 8,
    CLASS_WARLOCK = 9, CLASS_DRUID = 11
};

enum SpellSchools { SPELL_SCHOOL_NORMAL = 0, SPELL_SCHOOL_HOLY = 1, MAX_SPELL_SCHOOL = 7 };

// Unit.h:322 起
enum CombatRating
{
    CR_WEAPON_SKILL = 0, CR_DEFENSE_SKILL = 1, CR_DODGE = 2, CR_PARRY = 3, CR_BLOCK = 4,
    CR_HIT_MELEE = 5, CR_HIT_RANGED = 6, CR_HIT_SPELL = 7,
    CR_CRIT_MELEE = 8, CR_CRIT_RANGED = 9, CR_CRIT_SPELL = 10,
    CR_HASTE_MELEE = 17, CR_HASTE_RANGED = 18, CR_HASTE_SPELL = 19,
    CR_EXPERTISE = 23, CR_ARMOR_PENETRATION = 24, MAX_COMBAT_RATING = 25
};

// Player.h:151
enum PlayerSpellState : uint8
{
    PLAYERSPELL_UNCHANGED = 0, PLAYERSPELL_CHANGED = 1, PLAYERSPELL_NEW = 2,
    PLAYERSPELL_REMOVED = 3, PLAYERSPELL_TEMPORARY = 4
};

// Player.h:194
enum ActionButtonType { ACTION_BUTTON_SPELL = 0x00, ACTION_BUTTON_MACRO = 0x40, ACTION_BUTTON_ITEM = 0x80 };

// SpellDefines.h:46
enum SpellInterruptFlags : uint32 { SPELL_INTERRUPT_FLAG_MOVEMENT = 0x01 };

// GossipDef.h:30 / :61
#define GOSSIP_MAX_MENU_ITEMS 32
#define DEFAULT_GOSSIP_MESSAGE 0xffffff
enum GossipOptionIcon { GOSSIP_ICON_CHAT = 0, GOSSIP_ICON_VENDOR = 1, GOSSIP_ICON_TRAINER = 3, GOSSIP_ICON_BATTLE = 9 };
enum GroupMemberFlags { MEMBER_FLAG_MAINTANK = 0x02 };

typedef std::unordered_map<uint32, PlayerSpellState> PlayerTalentMap;

// ---------------- v3 枚举与基础结构（提前声明）----------------
enum Mechanics { MECHANIC_CHARM=1, MECHANIC_DISORIENTED=2, MECHANIC_FEAR=5,
                 MECHANIC_ROOT=7, MECHANIC_SLEEP=10, MECHANIC_SNARE=11,
                 MECHANIC_STUN=12, MECHANIC_FREEZE=13 };
enum Powers { POWER_MANA=0, POWER_RAGE=1, POWER_FOCUS=2, POWER_ENERGY=3,
              POWER_RUNE=5, POWER_RUNIC_POWER=6 };
enum DispelType { DISPEL_NONE=0, DISPEL_MAGIC=1, DISPEL_CURSE=2, DISPEL_DISEASE=3,
                  DISPEL_POISON=4, DISPEL_ALL=7 };
#define DISPEL_ALL_MASK ((1<<DISPEL_MAGIC)|(1<<DISPEL_CURSE)|(1<<DISPEL_DISEASE)|(1<<DISPEL_POISON))
enum SpellEffects { SPELL_EFFECT_NONE=0, SPELL_EFFECT_DISPEL=38 };
enum SpellCastResult : uint8 {
    SPELL_FAILED_BAD_TARGETS=12, SPELL_FAILED_DONT_REPORT=27, SPELL_FAILED_INTERRUPTED=40,
    SPELL_FAILED_ITEM_NOT_FOUND=44, SPELL_FAILED_LINE_OF_SIGHT=47, SPELL_FAILED_MOVING=51,
    SPELL_FAILED_NOT_BEHIND=57, SPELL_FAILED_NOT_READY=67, SPELL_FAILED_NOT_SHAPESHIFT=68,
    SPELL_FAILED_NO_POWER=85, SPELL_FAILED_ONLY_SHAPESHIFT=94, SPELL_FAILED_OUT_OF_RANGE=97,
    SPELL_FAILED_REAGENTS=100, SPELL_FAILED_SPELL_IN_PROGRESS=105,
    SPELL_FAILED_UNIT_NOT_INFRONT=134, SPELL_CAST_OK=255,
    SPELL_FAILED_TOO_CLOSE=128, SPELL_FAILED_NEED_AMMO=52, SPELL_FAILED_NEED_AMMO_POUCH=53,
    SPELL_FAILED_NEED_EXOTIC_AMMO=54, SPELL_FAILED_NO_AMMO=75, SPELL_FAILED_EQUIPPED_ITEM=28,
    SPELL_FAILED_EQUIPPED_ITEM_CLASS=29, SPELL_FAILED_EQUIPPED_ITEM_CLASS_MAINHAND=30,
    SPELL_FAILED_EQUIPPED_ITEM_CLASS_OFFHAND=31, SPELL_FAILED_NOT_STANDING=66,
    SPELL_FAILED_TARGET_AURASTATE=111, SPELL_FAILED_CASTER_AURASTATE=22 };
enum SpellState { SPELL_STATE_NULL=0, SPELL_STATE_PREPARING=1, SPELL_STATE_CASTING=2 };
enum CurrentSpellTypes : uint8 { CURRENT_MELEE_SPELL=0, CURRENT_GENERIC_SPELL=1, CURRENT_CHANNELED_SPELL=2 };
enum { SPELL_PREVENTION_TYPE_NONE=0, SPELL_PREVENTION_TYPE_SILENCE=1 };
enum LocaleConstant { LOCALE_enUS=0, DEFAULT_LOCALE=0, TOTAL_LOCALES=16 };
namespace GameTime { uint32 GetGameTimeMS(); }
struct SpellEffectInfo { SpellEffects Effect = SPELL_EFFECT_NONE; int32 MiscValue = 0; };

class Map
{
public:
    bool IsDungeon() const { return _dungeon; }             // Map.h:446
    bool IsRaid() const { return _raid; }                   // Map.h:448
    bool IsHeroic() const { return _heroic; }               // Map.h:450
    bool Is25ManRaid() const { return _r25; }               // Map.h:451
    bool IsBattleground() const { return _bg; }             // Map.h:452
    bool _dungeon=false,_raid=false,_heroic=false,_r25=false,_bg=false;
};

// ---------------- ObjectGuid ----------------
struct ObjectGuid
{
    uint64 v = 0;
    static ObjectGuid Empty;
    uint32 GetCounter() const { return uint32(v); }
    bool operator==(ObjectGuid const& o) const { return v == o.v; }
};

// ---------------- DBC ----------------
struct TalentSpellPos { uint16 talent_id = 0; uint8 rank = 0; };          // DBCStructure.h:1975
struct TalentEntry { uint32 ID = 0; uint32 TabID = 0; };                   // DBCStructure.h:1665

template<class T> struct DBCStorage
{
    T const* LookupEntry(uint32) const { static T t; return &t; }
};
extern DBCStorage<TalentEntry> sTalentStore;                               // DBCStores.h:192
TalentSpellPos const* GetTalentSpellPos(uint32);                           // DBCStores.h:41
uint32 const* GetTalentTabPages(uint8 cls);                                // DBCStores.h:67

// ---------------- SpellInfo ----------------
struct SpellCastTimesEntry { uint32 ID = 0; int32 Base = 0; };

class SpellInfo
{
public:
    uint32 Id = 0;
    uint32 InterruptFlags = 0;                       // SpellInfo.h:346
    SpellCastTimesEntry const* CastTimeEntry = nullptr;  // SpellInfo.h:341
    uint32 Dispel = 0;                                      // SpellInfo.h:316
    uint32 PreventionType = SPELL_PREVENTION_TYPE_SILENCE;  // SpellInfo.h:384
    bool _aoe = false;
    bool IsTargetingArea() const { return _aoe; }           // SpellInfo.h:420
    float GetMaxRange(bool = false, void* = nullptr, void* = nullptr) const { return _maxRange; } // SpellInfo.h:484
    float GetMinRange(bool = false) const { return _minRange; }   // SpellInfo.h:483
    float _maxRange = 100.f, _minRange = 0.f;
    char const* SpellName[16] = { "测试法术" };             // SpellInfo.h:377
    std::vector<SpellEffectInfo> _effects;
    std::vector<SpellEffectInfo> const& GetEffects() const { return _effects; }  // SpellInfo.h:507
    uint32 CalcCastTime(void* spell = nullptr) const { (void)spell; return CastTimeEntry ? uint32(CastTimeEntry->Base) : 0; }  // SpellInfo.h:491
};

// ---------------- SpellHistory ----------------
class SpellHistory
{
public:
    bool IsReady(SpellInfo const*, uint32 itemId = 0, bool ignoreCat = false) const { (void)itemId; (void)ignoreCat; return true; }  // SpellHistory.h:79
    bool HasGlobalCooldown(SpellInfo const*) const { return false; }        // SpellHistory.h:132
};

// ---------------- SpellMgr ----------------
class SpellMgr
{
public:
    SpellInfo const* GetSpellInfo(uint32 id) const                          // SpellMgr.h:659
    {
        static std::map<uint32, SpellInfo> cache;
        auto& s = cache[id]; s.Id = id; return &s;
    }
    uint32 GetNextSpellInChain(uint32) const { return 0; }                  // SpellMgr.h:596
    uint32 GetFirstSpellInChain(uint32 id) const { return id; }             // SpellMgr.h:594
};
extern SpellMgr* sSpellMgrInst;
#define sSpellMgr sSpellMgrInst

// ---------------- CastSpell 参数 ----------------
enum TriggerCastFlags : uint32 { TRIGGERED_NONE = 0, TRIGGERED_FULL_MASK = 0x0007FFFF };

struct CastSpellExtraArgs                                                   // SpellDefines.h:340
{
    CastSpellExtraArgs() {}
    CastSpellExtraArgs(bool triggered) : TriggerFlags(triggered ? TRIGGERED_FULL_MASK : TRIGGERED_NONE) {}
    TriggerCastFlags TriggerFlags = TRIGGERED_NONE;
};

// ---------------- EventProcessor ----------------
class EventProcessor                                                        // EventProcessor.h:94
{
public:
    template<class T>
    void AddEventAtOffset(T&& ev, Milliseconds off)                         // EventProcessor.h:109
    {
        (void)off;
        if (depth < 2) { ++depth; ev(); --depth; }   // 只递归 2 层，防测试死循环
    }
    int depth = 0;
};

// ---------------- v3 Aura ----------------
class Aura
{
public:
    SpellInfo const* GetSpellInfo() const { return _si; }   // SpellAuras.h:123
    int32 GetDuration() const { return _duration; }         // SpellAuras.h:156
    int32 GetMaxDuration() const { return _maxDuration; }   // SpellAuras.h:151
    bool IsPermanent() const { return _maxDuration == -1; } // SpellAuras.h:161
    SpellInfo const* _si = nullptr;
    int32 _duration = 600000, _maxDuration = 600000;
};
class AuraApplication
{
public:
    Aura* GetBase() const { return _base; }                 // SpellAuras.h:75
    bool IsPositive() const { return _positive; }           // SpellAuras.h:81
    Aura* _base = nullptr; bool _positive = false;
};

// ---------------- v3.1 Spell（读条信息）----------------
class Spell
{
public:
    uint32 getState() const { return _state; }              // Spell.h:356
    int32 GetTimer() const { return _timer; }               // Spell.h:422
    int32 GetCastTime() const { return _castTime; }         // Spell.h:424
    SpellInfo const* GetSpellInfo() const { return _si; }   // Spell.h:456
    uint32 _state = SPELL_STATE_PREPARING;
    int32 _timer = 2000, _castTime = 2500;
    SpellInfo const* _si = nullptr;
};

class Group;   // 前置声明
// ---------------- Unit / Player ----------------
class Unit;
class Player;

class WorldObject
{
public:
    EventProcessor m_Events;                                                // Object.h:620
    ObjectGuid GetGUID() const { return _guid; }
    bool IsInWorld() const { return true; }
    float GetExactDist2d(WorldObject const*) const { return 5.0f; }         // Position.h:109
    bool IsWithinLOSInMap(WorldObject const*) const { return true; }        // WorldObject.h:422
    bool IsValidAttackTarget(WorldObject const*, SpellInfo const* = nullptr) const { return true; }  // WorldObject.h:534
    SpellCastResult CastSpell(WorldObject* target, uint32 spellId, CastSpellExtraArgs const& args = {})  // Object.h:532
    { (void)target; (void)args; lastCast = spellId; ++castCount;
      auto it = forcedFail.find(spellId);
      return it != forcedFail.end() ? it->second : SPELL_CAST_OK; }
    static std::map<uint32, SpellCastResult> forcedFail;

    ObjectGuid _guid;
    static uint32 lastCast;
    static uint32 castCount;
};

typedef std::set<Unit*> AttackerSet;

class Unit : public WorldObject
{
public:
    uint8 GetClass() const { return _class; }                               // Unit.h:896
    uint8 GetLevel() const { return _level; }                               // Unit.h:890
    uint32 GetHealth() const { return _hp; }                                // Unit.h:914
    uint32 GetMaxHealth() const { return _maxhp; }                          // Unit.h:915
    void SetHealth(uint32 v) { _hp = v; }                                   // Unit.h:926
    void SetResistance(SpellSchools, int32) { }                             // Unit.h:911
    bool IsAlive() const { return _hp > 0; }
    bool isMoving() const { return _moving; }                               // Unit.h:1771
    bool HasAura(uint32 id, ObjectGuid caster = ObjectGuid::Empty, ObjectGuid item = ObjectGuid::Empty, uint8 m = 0) const  // Unit.h:1417
    { (void)caster; (void)item; (void)m; return _auras.count(id) != 0; }
    uint8 GetComboPoints(Unit const* who = nullptr) const { (void)who; return _combo; }  // Unit.h:1707
    Unit* GetVictim() const { return _victim; }                             // Unit.h:860
    Unit* SelectNearbyTarget(Unit* exclude = nullptr, float d = 5.0f) const { (void)exclude; (void)d; return _nearby; }  // Unit.h:872
    bool IsWithinMeleeRange(Unit const*) const { return _inMelee; }         // Unit.h:845
    bool IsNonMeleeSpellCast(bool withDelayed, bool a = false, bool b = false, bool c = false, bool d = true) const  // Unit.h:1482
    { (void)withDelayed; (void)a; (void)b; (void)c; (void)d; return _casting; }
    SpellHistory* GetSpellHistory() { return &_hist; }                      // Unit.h:1496
    SpellHistory const* GetSpellHistory() const { return &_hist; }
    AttackerSet const& getAttackers() const { return _attackers; }          // Unit.h:858
    std::string GetName() const { return "测试目标"; }
    bool IsTotem() const { return _totem; }        // Unit.h:887
    float GetHealthPct() const { return _maxhp ? 100.f*_hp/_maxhp : 0.f; }  // Unit.h:922
    bool HasAuraWithMechanic(uint32 m) const { return (_mechMask & m) != 0; } // Unit.h:1425
    Powers GetPowerType() const { return _ptype; }          // Unit.h:932
    uint32 GetPower(Powers) const { return _power; }        // Unit.h:935
    uint32 GetMaxPower(Powers) const { return _maxpower; }  // Unit.h:937
    bool IsFriendlyTo(WorldObject const*) const { return true; }  // WorldObject.h:528
    bool IsInMap(WorldObject const*) const { return true; }       // WorldObject.h:413
    std::multimap<uint32, AuraApplication*> const& GetAppliedAuras() const { return _applied; } // Unit.h:1350
    AuraApplication* GetAuraApplication(uint32 id, ObjectGuid = ObjectGuid::Empty,
                                        ObjectGuid = ObjectGuid::Empty, uint8 = 0,
                                        AuraApplication* = nullptr) const   // Unit.h:1407
    { auto it = _applied.find(id); return it != _applied.end() ? it->second : nullptr; }
    Map* GetMap() const { return _map; }                    // WorldObject.h:467
    Spell* GetCurrentSpell(CurrentSpellTypes) const { return _curSpell; }  // Unit.h:1488
    Spell* _curSpell = nullptr;
    uint32 _mechMask = 0, _power = 10000, _maxpower = 10000;
    Powers _ptype = POWER_MANA;
    std::multimap<uint32, AuraApplication*> _applied;
    Map* _map = nullptr;
    bool IsCritter() const { return _critter; }    // Unit.h:1118
    bool _totem = false, _critter = false;

    uint8 _class = 1, _level = 80, _combo = 0;
    uint32 _hp = 100000, _maxhp = 100000;
    bool _moving = false, _casting = false, _inMelee = true;
    Unit* _victim = nullptr; Unit* _nearby = nullptr;
    std::set<uint32> _auras; AttackerSet _attackers; SpellHistory _hist;
};

class Creature : public Unit { };

class GossipMenu                                                            // GossipDef.h:160
{
public:
    uint32 AddMenuItem(int32 menuItemId, GossipOptionIcon icon, std::string const& msg,
                       uint32 sender, uint32 action, std::string const& box, uint32 money, bool coded = false)
    { (void)menuItemId; (void)icon; (void)box; (void)money; (void)coded;
      items.push_back({msg, sender, action}); return uint32(items.size()); }   // GossipDef.h:168
    uint32 GetMenuItemCount() const { return uint32(items.size()); }

    struct It { std::string msg; uint32 sender, action; };
    std::vector<It> items;
};

class PlayerMenu                                                            // GossipDef.h:250
{
public:
    GossipMenu& GetGossipMenu() { return _menu; }                           // GossipDef.h:263
    void ClearMenus() { _menu.items.clear(); }                              // GossipDef.h:268
    void SendGossipMenu(uint32, ObjectGuid) { sent = true; }                // GossipDef.h:273
    void SendCloseGossip() { closed = true; }                               // GossipDef.h:274
    GossipMenu _menu; bool sent = false, closed = false;
};

class WorldSession { public: Player* GetPlayer() const;
    LocaleConstant GetSessionDbcLocale() const { return DEFAULT_LOCALE; }
    Player* _p = nullptr; };

class Player : public Unit
{
public:
    bool HasSpell(uint32 s) const { return _spells.count(s) != 0; }         // Player.h:1443
    void ApplyRatingMod(CombatRating cr, int32 v, bool apply)               // Player.h:1649
    { (void)apply; _ratings[cr] += v; }
    float GetRatingMultiplier(CombatRating) const { return 0.05f; }         // Player.h:1663
    Unit* GetSelectedUnit() const { return _selected; }                     // Player.h:1401
    void SetSelection(ObjectGuid const& g) { _selection = g; }              // Player.h:1405
    void* addActionButton(uint8 b, uint32 a, uint8 t)                       // Player.h:1556
    { (void)t; _buttons[b] = a; return nullptr; }
    void removeActionButton(uint8 b) { _buttons.erase(b); }                 // Player.h:1557
    void SendActionButtons(uint32) const { ++_sendCount; }                  // Player.h:1560
    uint8 GetActiveTalentGroup() const { return 0; }                        // Player.h:1477
    PlayerTalentMap const* GetTalentMap(uint8) const { return &_talents; }  // Player.h:1507
    WorldSession* GetSession() const { return _sess; }
    Group* GetGroup() { return _group; }                    // Player.h:2155
    bool IsInCombat() const { return _inCombat; }
    Group* _group = nullptr;
    bool _inCombat = false;

    std::set<uint32> _spells;
    std::map<CombatRating, int32> _ratings;
    std::map<uint8, uint32> _buttons;
    PlayerTalentMap _talents;
    Unit* _selected = nullptr;
    ObjectGuid _selection;
    WorldSession* _sess = nullptr;
    PlayerMenu* PlayerTalkClass = nullptr;
    mutable uint32 _sendCount = 0;
};

inline Player* WorldSession::GetPlayer() const { return _p; }

// ---------------- v3 Group ----------------
class GroupReference
{
public:
    Player* GetSource() { return _p; }
    GroupReference* next() { return _next; }
    Player* _p = nullptr; GroupReference* _next = nullptr;
};

class GroupBotReference
{
public:
    Creature* GetSource() { return _p; }
    GroupBotReference* next() { return _next; }
    Creature* _p = nullptr; GroupBotReference* _next = nullptr;
};

class Group
{
public:
    GroupReference* GetFirstMember() { return _first; }     // Group.h:257
    GroupBotReference* GetFirstBotMember() { return _firstBot; } // Group.h:202
    uint32 GetMembersCount() const { return _count; }       // Group.h:259
    uint8 GetMemberFlags(ObjectGuid guid) const             // Group.h:242
    { auto it = _flags.find(guid.GetCounter()); return it == _flags.end() ? 0 : it->second; }
    bool isRaidGroup() const { return _raid; }              // Group.h:224
    GroupReference* _first = nullptr;
    GroupBotReference* _firstBot = nullptr;
    std::map<uint32, uint8> _flags;
    uint32 _count = 1; bool _raid = false;
};


// ---------------- ChatHandler ----------------
class ChatHandler
{
public:
    ChatHandler() {}
    explicit ChatHandler(WorldSession* s) : _s(s) {}
    WorldSession* GetSession() { return _s; }
    bool ParseCommands(std::string const& cmd)      // Chat.h:87 public
    { parsed.push_back(cmd); return true; }
    static std::vector<std::string> parsed;
    template<typename... A> void PSendSysMessage(char const* fmt, A... a)
    { if (verbose) { printf("    "); printf(fmt, a...); printf("\n"); } _lines.push_back(fmt); }
    void PSendSysMessage(char const* fmt) { if (verbose) printf("    %s\n", fmt); _lines.push_back(fmt); }
    WorldSession* _s = nullptr;
    static bool verbose;
    static std::vector<std::string> _lines;
};

// ---------------- ScriptMgr ----------------
struct ChatCommand
{
    char const* Name; uint32 Permission; bool AllowConsole;
    bool (*Handler)(ChatHandler*, char const*);
    std::string Help;
};

class ScriptObject { protected: explicit ScriptObject(char const*) {} };
class CommandScript : public ScriptObject
{
public: explicit CommandScript(char const* n) : ScriptObject(n) {}
    virtual std::vector<ChatCommand> GetCommands() const = 0;
};
class PlayerScript : public ScriptObject
{
public: explicit PlayerScript(char const* n) : ScriptObject(n) {}
    virtual void OnGossipSelect(Player*, uint32, uint32, uint32) { }         // ScriptMgr.h:719
    virtual void OnLogout(Player*) { }                                       // ScriptMgr.h:695
};

namespace rbac { enum { RBAC_PERM_COMMAND_COMBATHELPER = 71011 }; }

class ObjectAccessor
{
public:
    static Player* FindPlayer(ObjectGuid const&);
    static Unit* GetUnit(WorldObject const&, ObjectGuid const&);
};

// ---------------- 格子搜索（GridNotifiers.h:959 / :435, CellImpl.h）----------------
namespace Trinity
{
    class AnyUnfriendlyUnitInObjectRangeCheck
    {
    public:
        AnyUnfriendlyUnitInObjectRangeCheck(WorldObject const* obj, Unit const* funit, float range)
            : i_obj(obj), i_funit(funit), i_range(range) { }
        bool operator()(Unit* u) const { (void)u; return true; }
    private:
        WorldObject const* i_obj; Unit const* i_funit; float i_range;
    };

    template<class Check>
    struct UnitListSearcher
    {
        UnitListSearcher(WorldObject const* searcher, std::list<Unit*>& container, Check& check)
            : _c(&container), _chk(&check) { (void)searcher; }
        std::list<Unit*>* _c; Check* _chk;
    };
}

extern std::vector<Unit*> g_fakeNearby;

class Cell
{
public:
    template<class T>
    static void VisitAllObjects(WorldObject const* obj, T& visitor, float radius, bool dont_load = true)
    { (void)obj; (void)radius; (void)dont_load; for (Unit* u : g_fakeNearby) visitor._c->push_back(u); }
};

// ---------------- step20 属性持久化桩 ----------------
class CustomStatPersistMgr
{
public:
    enum StatType : uint8 { TYPE_UNITMOD = 0, TYPE_RATING = 1 };   // CustomStatPersist.h:70
    static CustomStatPersistMgr* instance()
    { static CustomStatPersistMgr m; return &m; }

    bool Enabled() const { return _enabled; }                      // CustomStatPersist.h:76
    void Record(uint32 guidLow, StatType type, uint8 index, float amount)  // CustomStatPersist.h:86
    { records.push_back({guidLow, type, index, amount}); }

    struct Rec { uint32 guid; StatType type; uint8 idx; float amt; };
    std::vector<Rec> records;
    bool _enabled = true;
};
#define sCustomStatPersist CustomStatPersistMgr::instance()

#endif
