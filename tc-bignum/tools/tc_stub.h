#pragma once
#include <string>
#include <vector>
#include <cstdio>
#include <cstdint>
#include <set>
typedef uint8_t uint8; typedef uint32_t uint32; typedef uint64_t uint64; typedef int8_t int8;
enum AccountTypes { SEC_PLAYER=0, SEC_MODERATOR=1, SEC_GAMEMASTER=2, SEC_ADMINISTRATOR=3 };
enum TypeID { TYPEID_UNIT=3, TYPEID_PLAYER=4 };
namespace rbac { enum { RBAC_PERM_COMMAND_WORLDTOOLS=71012 }; }
struct ObjectGuid { uint32 GetCounter() const {return 0;} uint64 GetRawValue() const {return 0;} bool IsPlayer() const{return true;} };
struct Field { uint32 GetUInt32() const {return 0;} const char* GetCString() const {return "";} std::string GetString() const {return "";} };
struct ResultSet { Field* Fetch(){static Field f;return &f;} bool NextRow(){return false;} Field& operator[](int){static Field f;return f;} };
typedef ResultSet* QueryResult;
struct DB { template<typename...A> QueryResult PQuery(const char*,A&&...){return nullptr;} template<typename...A> void PExecute(const char*,A&&...){} QueryResult Query(const char*){return nullptr;} };
extern DB WorldDatabase, CharacterDatabase;
struct Player; struct WorldSession { AccountTypes GetSecurity(){return SEC_PLAYER;} Player* GetPlayer(); };
struct Player { ObjectGuid GetGUID(){return {};} uint32 GetPhaseMaskForSpawn(){return 1;} };
struct Map { uint32 GetId(){return 0;} uint8 GetSpawnMode(){return 0;} bool Instanceable(){return false;} };
struct bot_ai { void UnsetWanderer(){} void SetWanderer(){} bool IsWanderer(){return true;} };
struct CreatureTemplate { uint32 faction=0; };
struct Creature; 
struct Unit { TypeID GetTypeId(){return TYPEID_UNIT;} Creature* ToCreature(); };
struct Creature : Unit {
  bool IsNPCBot(){return true;} bool IsWandererBot(){return true;}
  uint32 GetEntry(){return 0;} ObjectGuid GetGUID(){return {};}
  std::string GetName(){return "";} Map* GetMap(){return nullptr;}
  uint32 GetSpawnId(){return 0;} bot_ai* GetBotAI(){return nullptr;}
  CreatureTemplate const* GetCreatureTemplate(){return nullptr;}
  void CombatStop(){} void AddObjectToRemoveList(){} void SaveToDB(uint32,uint8,uint32){}
  static bool DeleteFromDB(uint32){return true;}
};
inline Creature* Unit::ToCreature(){return (Creature*)this;}
struct ChatHandler {
  ChatHandler(WorldSession*){} 
  void SendSysMessage(const char*){} void SetSentErrorMessage(bool){}
  Unit* getSelectedUnit(){return nullptr;} WorldSession* GetSession(){return nullptr;}
};
struct NpcBotData { uint32 owner=0; };
struct NpcBotExtras { uint8 race=1, bclass=1; };
enum NpcBotDataUpdateType { NPCBOT_UPDATE_ERASE=1 };
namespace BotDataMgr {
  NpcBotData const* SelectNpcBotData(uint32); NpcBotExtras const* SelectNpcBotExtras(uint32);
  void AddNpcBotData(uint32,uint32,uint8,uint32); void UpdateNpcBotData(uint32,NpcBotDataUpdateType,void*);
  uint8 SelectSpecForClass(uint8); uint32 DefaultRolesForClass(uint8,uint8);
  Creature const* FindBot(uint32);
}
struct OM { void AddCreatureToGrid(uint32,void*){} void* GetCreatureData(uint32){return nullptr;} };
extern OM* sObjectMgr;
struct ChatCommand { const char* n; uint32 p; bool c; bool(*h)(ChatHandler*,const char*); const char* e; };
class CommandScript { public: CommandScript(const char*){} virtual std::vector<ChatCommand> GetCommands() const = 0; virtual ~CommandScript(){} };


inline Player* WorldSession::GetPlayer(){return nullptr;}
