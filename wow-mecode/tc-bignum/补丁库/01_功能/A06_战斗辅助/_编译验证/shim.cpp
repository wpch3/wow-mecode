#include "mock.h"
ObjectGuid ObjectGuid::Empty;
DBCStorage<TalentEntry> sTalentStore;
SpellMgr* sSpellMgrInst = new SpellMgr();
uint32 WorldObject::lastCast = 0;
uint32 WorldObject::castCount = 0;
bool ChatHandler::verbose = true;
std::vector<std::string> ChatHandler::_lines;
static Player* g_player = nullptr;
void SetTestPlayer(Player* p) { g_player = p; }
Player* ObjectAccessor::FindPlayer(ObjectGuid const&) { return g_player; }
Unit* ObjectAccessor::GetUnit(WorldObject const&, ObjectGuid const& guid)
{
    if (!g_player)
        return nullptr;
    if (g_player->GetGUID() == guid)
        return g_player;
    if (g_player->_selected && g_player->_selected->GetGUID() == guid)
        return g_player->_selected;
    if (Group* group = g_player->GetGroup())
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            if (ref->GetSource() && ref->GetSource()->GetGUID() == guid)
                return ref->GetSource();
        for (GroupBotReference* ref = group->GetFirstBotMember(); ref; ref = ref->next())
            if (ref->GetSource() && ref->GetSource()->GetGUID() == guid)
                return ref->GetSource();
    }
    return nullptr;
}
TalentSpellPos const* GetTalentSpellPos(uint32) { return nullptr; }
uint32 const* GetTalentTabPages(uint8) { static uint32 t[3] = {161,163,164}; return t; }
std::vector<Unit*> g_fakeNearby;
std::map<uint32, SpellCastResult> WorldObject::forcedFail;
namespace GameTime { static uint32 g_ms = 100000; uint32 GetGameTimeMS() { return g_ms; }
                     void AdvanceMs(uint32 d) { g_ms += d; }
                     void SetMs(uint32 v) { g_ms = v; } }
std::vector<std::string> ChatHandler::parsed;
