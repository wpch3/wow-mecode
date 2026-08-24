#include "mock.h"
ObjectGuid ObjectGuid::Empty;
DBCStorage<AreaTableEntry> sAreaTableStore;
DBCStorage<GemPropertiesEntry> sGemPropertiesStore;
DBCStorage<SpellItemEnchantmentEntry> sSpellItemEnchantmentStore;
SpellMgr* sSpellMgrInst = new SpellMgr();
ObjectMgr* sObjectMgrInst = new ObjectMgr();
uint32 Unit::killCount=0, Unit::castCount=0;
uint32 Creature::despawnCount=0, Creature::deleteCount=0;
uint32 GameObject::stateCount=0;
uint32 Player::summonCount=0;
uint32 Item::enchCount=0;
bool ChatHandler::verbose=false; uint32 ChatHandler::lines=0;
std::vector<std::string> ChatHandler::parsed;
std::vector<Creature*> g_nearCreatures;
std::vector<GameObject*> g_nearGOs;
namespace GameTime { static uint32 g_ms=100000; uint32 GetGameTimeMS(){return g_ms;}
                     void AdvanceMs(uint32 d){g_ms+=d;} void SetMs(uint32 v){g_ms=v;} }
uint32 Player::goSummonCount=0;
