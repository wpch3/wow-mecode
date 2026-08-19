// TrinityCore 类型桩，仅用于语法验证
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <string_view>
#include <cstdio>
#include <cstdlib>
#include <cctype>

typedef uint8_t  uint8;  typedef uint16_t uint16;
typedef uint32_t uint32; typedef uint64_t uint64;
typedef int8_t   int8;   typedef int32_t  int32;  typedef int16_t int16;

#define MAX_ITEM_PROTO_STATS 10
#define MAX_ITEM_PROTO_SOCKETS 3
#define MAX_ITEM_SET_ITEMS 10
#define MAX_ITEM_SET_SPELLS 8
#define NULL_BAG 0
#define NULL_SLOT 255
#define TC_LOG_INFO(a,...) do{}while(0)

enum Classes { CLASS_WARRIOR=1, CLASS_PALADIN=2, CLASS_HUNTER=3, CLASS_ROGUE=4,
  CLASS_PRIEST=5, CLASS_DEATH_KNIGHT=6, CLASS_SHAMAN=7, CLASS_MAGE=8,
  CLASS_WARLOCK=9, CLASS_DRUID=11 };
enum ItemQualities { ITEM_QUALITY_POOR=0, ITEM_QUALITY_NORMAL=1, ITEM_QUALITY_UNCOMMON=2,
  ITEM_QUALITY_RARE=3, ITEM_QUALITY_EPIC=4, ITEM_QUALITY_LEGENDARY=5, ITEM_QUALITY_HEIRLOOM=7 };
enum InventoryTypeE { INVTYPE_HEAD=1, INVTYPE_NECK=2, INVTYPE_SHOULDERS=3, INVTYPE_CHEST=5,
  INVTYPE_WAIST=6, INVTYPE_LEGS=7, INVTYPE_FEET=8, INVTYPE_WRISTS=9, INVTYPE_HANDS=10,
  INVTYPE_FINGER=11, INVTYPE_TRINKET=12, INVTYPE_WEAPON=13, INVTYPE_SHIELD=14,
  INVTYPE_RANGED=15, INVTYPE_CLOAK=16, INVTYPE_2HWEAPON=17, INVTYPE_WEAPONMAINHAND=21,
  INVTYPE_WEAPONOFFHAND=22, INVTYPE_HOLDABLE=23, INVTYPE_AMMO=24,
  INVTYPE_THROWN=25, INVTYPE_RANGEDRIGHT=26, INVTYPE_QUIVER=27, INVTYPE_RELIC=28 };
// 注意：3.3.5 没有 INVTYPE_WAND，魔杖用 INVTYPE_RANGEDRIGHT
enum ItemClassE { ITEM_CLASS_WEAPON=2, ITEM_CLASS_ARMOR=4 };
enum ItemSubclassArmor { ITEM_SUBCLASS_ARMOR_CLOTH=1, ITEM_SUBCLASS_ARMOR_LEATHER=2,
  ITEM_SUBCLASS_ARMOR_MAIL=3, ITEM_SUBCLASS_ARMOR_PLATE=4, ITEM_SUBCLASS_ARMOR_SHIELD=6 };
enum ItemSubclassWeapon { ITEM_SUBCLASS_WEAPON_AXE=0, ITEM_SUBCLASS_WEAPON_AXE2=1,
  ITEM_SUBCLASS_WEAPON_BOW=2, ITEM_SUBCLASS_WEAPON_MACE=4, ITEM_SUBCLASS_WEAPON_MACE2=5,
  ITEM_SUBCLASS_WEAPON_SWORD=7, ITEM_SUBCLASS_WEAPON_STAFF=10, ITEM_SUBCLASS_WEAPON_DAGGER=15,
  ITEM_SUBCLASS_WEAPON_WAND=19 };
enum ItemModType { ITEM_MOD_MANA=0, ITEM_MOD_HEALTH=1, ITEM_MOD_AGILITY=3, ITEM_MOD_STRENGTH=4,
  ITEM_MOD_INTELLECT=5, ITEM_MOD_SPIRIT=6, ITEM_MOD_STAMINA=7, ITEM_MOD_DEFENSE_SKILL_RATING=12,
  ITEM_MOD_DODGE_RATING=13, ITEM_MOD_PARRY_RATING=14, ITEM_MOD_BLOCK_RATING=15,
  ITEM_MOD_CRIT_RATING=32, ITEM_MOD_HASTE_RATING=36, ITEM_MOD_ATTACK_POWER=38,
  ITEM_MOD_MANA_REGENERATION=43, ITEM_MOD_SPELL_POWER=45 };
enum SocketColor { SOCKET_COLOR_META=1, SOCKET_COLOR_RED=2, SOCKET_COLOR_YELLOW=4, SOCKET_COLOR_BLUE=8 };
enum InventoryResult { EQUIP_ERR_OK=0, EQUIP_ERR_ITEM_NOT_FOUND=1 };

struct _ItemStat { uint32 ItemStatType=0; int32 ItemStatValue=0; };
struct _Socket { uint32 Color=0; uint32 Content=0; };
struct ItemTemplate {
  uint32 ItemId=0, Class=0, SubClass=0, Quality=0, InventoryType=0, ItemLevel=0;
  int32 AllowableClass=-1; uint32 AllowableRace=0xFFFFFFFF;
  uint32 RequiredSkill=0, RequiredSkillRank=0, RequiredSpell=0;
  uint32 RequiredReputationFaction=0, RequiredReputationRank=0;
  uint32 ItemSet=0, HolidayId=0, Armor=0;   // 真名是 HolidayId (ItemTemplate.h:670)
  std::string Name1;
  _ItemStat ItemStat[MAX_ITEM_PROTO_STATS];
  std::array<_Socket, MAX_ITEM_PROTO_SOCKETS> Socket;
};
struct ItemSetEntry { char const* Name[16]={}; uint32 ItemID[MAX_ITEM_SET_ITEMS]={};
  uint32 SetSpellID[MAX_ITEM_SET_SPELLS]={}; uint32 SetThreshold[MAX_ITEM_SET_SPELLS]={};
  uint32 RequiredSkill=0, RequiredSkillRank=0; };
struct DungeonEncounterEntry { uint32 ID=0, MapID=0, Difficulty=0, Bit=0; char const* Name[16]={}; };
struct FactionEntry { char const* Name[16]={}; int32 ReputationIndex=0; };

template<class T> struct DBCStorage {
  uint32 GetNumRows() const { return 0; }
  T const* LookupEntry(uint32) const { return nullptr; }
};
extern DBCStorage<ItemSetEntry> sItemSetStore;
extern DBCStorage<DungeonEncounterEntry> sDungeonEncounterStore;
extern DBCStorage<FactionEntry> sFactionStore;

struct ObjectGuid { uint32 GetCounter() const { return 0; } };
struct Field { uint32 GetUInt32() const {return 0;} uint8 GetUInt8() const {return 0;}
               std::string GetString() const {return "";} };
struct ResultSetImpl { Field* Fetch(){static Field f; return &f;} bool NextRow(){return false;} };
struct QueryResult {
  ResultSetImpl* p=nullptr;
  QueryResult()=default; QueryResult(std::nullptr_t){}
  explicit operator bool() const { return p!=nullptr; }
  ResultSetImpl* operator->() const { static ResultSetImpl r; return &r; }
};
struct DB {
  template<class... A> QueryResult PQuery(char const*, A&&...) { return QueryResult(); }
  template<class... A> void PExecute(char const*, A&&...) {}
  QueryResult Query(char const*) { return QueryResult(); }
};
extern DB CharacterDatabase, WorldDatabase;

struct ItemPosCount{}; typedef std::vector<ItemPosCount> ItemPosCountVec;
struct Item { void SetBinding(bool){} };
struct WorldSession { };
struct ChatHandler {
  ChatHandler()=default;
  explicit ChatHandler(WorldSession*){}
  template<class... A> void PSendSysMessage(char const*, A&&...) {}
  void SendSysMessage(char const*) {}
  int GetSessionDbcLocale() const { return 0; }
};
struct Player {
  ObjectGuid GetGUID() const { return {}; }
  uint8 GetClass() const { return 1; }
  WorldSession* GetSession() const { return nullptr; }
  int GetReputationRank(uint32) const { return 0; }
  void SetReputation(uint32,uint32) {}
  uint16 GetSkillValue(uint32) const { return 0; }
  void SetSkill(uint32,uint16,uint16,uint16) {}
  bool HasSpell(uint32) const { return false; }
  void LearnSpell(uint32,bool,uint32=0) {}
  InventoryResult CanEquipNewItem(uint8,uint16&,uint32,bool) const { return EQUIP_ERR_OK; }
  Item* EquipNewItem(uint16,uint32,bool) { return nullptr; }
  InventoryResult CanStoreNewItem(uint8,uint8,ItemPosCountVec&,uint32,uint32,uint32* =nullptr) const { return EQUIP_ERR_OK; }
  Item* StoreNewItem(ItemPosCountVec const&,uint32,bool,int32=0) { return nullptr; }
  void SendNewItem(Item*,uint32,bool,bool,bool=false,bool=true) {}
};
int32 GenerateItemRandomPropertyId(uint32) { return 0; }
struct ObjectMgr {
  ItemTemplate const* GetItemTemplate(uint32) const { return nullptr; }
  std::unordered_map<uint32,ItemTemplate> const& GetItemTemplateStore() const {
    static std::unordered_map<uint32,ItemTemplate> m; return m; }
};
extern ObjectMgr* sObjectMgr;
namespace Trinity {
  inline std::vector<std::string_view> Tokenize(std::string const&, char, bool) { return {}; }
  template<class T> inline std::optional<T> StringTo(std::string_view) { return std::nullopt; }
}

// ---- Group 桩（真实代码在 Group.h，Player.h 只有前向声明）----
struct GroupReference {
    Player* GetSource() { return nullptr; }
    GroupReference* next() { return nullptr; }
};
struct Group {
    GroupReference* GetFirstMember() { return nullptr; }
};
