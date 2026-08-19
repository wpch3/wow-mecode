#pragma once
#include "tcstub.h"

class WorldSession {
public:
    Player* _p=nullptr; bool hasSocket=false;
    void HandleDuelAcceptedOpcode(WorldPacket& p);
    void HandleBeginTradeOpcode(WorldPacket&){g_log.push_back("BeginTrade");}
    bool PlayerDisconnected() const {return !hasSocket;}
    void HandleMoveWorldportAck();
};

class Player {
public:
    ObjectGuid guid; std::string name="Bot";
    Group* groupInvite=nullptr;
    uint32 guildId=0, guildIdInvited=0;
    std::unique_ptr<DuelInfo> duel;
    bool resReq=false, alive=false, inWorld=true;
    bool summonPending=false;
    bool nearSem=false, farSem=false;
    uint32 zoneId=0;
    ObjectGuid duelArbiter;
    WorldLocation tdest;
    WorldSession sess;
    bool resurrected=false, summoned=false;
    TradeData* tradeData=nullptr;
    DeathState deathState=ALIVE;
    uint32 pflags=0;
    bool repopBuilt=false, repopGraveyard=false;
    bool delayedResurrectPending=false;
    Position pos;
    Player(){ sess._p=this; }

    ObjectGuid GetGUID() const {return guid;}
    std::string const& GetName() const {return name;}
    WorldSession* GetSession(){return &sess;}
    bool IsInWorld() const {return inWorld;}
    bool IsAlive() const {return alive;}

    Group* GetGroupInvite() const {return groupInvite;}
    void SetGroupInvite(Group* g){groupInvite=g;}

    uint32 GetGuildId() const {return guildId;}
    uint32 GetGuildIdInvited() const {return guildIdInvited;}
    void SetGuildIdInvited(uint32 v){guildIdInvited=v;}

    ObjectGuid GetGuidValue(uint16) const {return duelArbiter;}

    bool IsResurrectRequested() const {return resReq;}
    void ResurrectUsingRequestData();

    bool HasSummonPending() const {return summonPending;}
    void SummonIfPossible(bool agree);

    TradeData* GetTradeData() const {return tradeData;}
    DeathState getDeathState() const {return deathState;}
    bool isDead() const {return deathState==DEAD||deathState==CORPSE;}
    bool HasFlag(int,uint32 f) const {return (pflags&f)!=0;}
    void BuildPlayerRepop(){repopBuilt=true; pflags|=PLAYER_FLAGS_GHOST;
                            g_log.push_back("BuildPlayerRepop");}
    void RepopAtGraveyard(){repopGraveyard=true; TeleportTo(0,true);
                            g_log.push_back("RepopAtGraveyard");}

    bool IsBeingTeleportedNear() const {return nearSem;}
    bool IsBeingTeleportedFar() const {return farSem;}
    void SetSemaphoreTeleportNear(bool v){nearSem=v;}
    void SetSemaphoreTeleportFar(bool v){farSem=v;}
    WorldLocation const& GetTeleportDest() const {return tdest;}
    bool UpdatePosition(Position const&,bool){g_log.push_back("UpdatePosition");return true;}
    void SetFallInformation(uint32,float){}
    float GetPositionZ() const {return pos.m_positionZ;}
    void GetZoneAndAreaId(uint32&z,uint32&a) const {z=zoneId;a=zoneId;}
    void UpdateZone(uint32,uint32){g_log.push_back("UpdateZone");}
    void ProcessDelayedOperations(){
        if(delayedResurrectPending){resurrected=true;delayedResurrectPending=false;
            g_log.push_back("DelayedResurrectDone");}
    }
    // 模拟传送：跨地图置far，同地图置near
    void TeleportTo(uint32 map,bool far_){ if(far_) farSem=true; else nearSem=true; tdest.mapId=map; }
};

// ---- 需要 Player 完整定义的实现 ----
inline void Group::RemoveInvite(Player* p){
    for(auto it=invitees.begin();it!=invitees.end();++it) if(*it==p){invitees.erase(it);break;}
    if(p&&p->GetGroupInvite()==this) p->SetGroupInvite(nullptr);
    g_log.push_back("RemoveInvite");
}
inline bool Group::Create(Player* l){created=true;leader=l->GetGUID();g_log.push_back("Create");return true;}
inline bool Group::AddMember(Player* p){ if(!addMemberResult) return false;
    members.push_back(p); g_log.push_back("AddMember"); return true;}

inline void WorldSession::HandleDuelAcceptedOpcode(WorldPacket& p){
    Player* pl=_p;
    if(!pl->duel||pl==pl->duel->Initiator||pl->duel->State!=DUEL_STATE_CHALLENGED) return;
    Player* t=pl->duel->Opponent;
    if(t->GetGuidValue(PLAYER_DUEL_ARBITER).v != p.payload) return;   // 校验GUID
    pl->duel->State=DUEL_STATE_COUNTDOWN;
    if(t->duel) t->duel->State=DUEL_STATE_COUNTDOWN;
    g_log.push_back("DuelAccepted");
}
inline void WorldSession::HandleMoveWorldportAck(){
    if(!_p->IsBeingTeleportedFar()) return;
    _p->SetSemaphoreTeleportFar(false);
    _p->ProcessDelayedOperations();
    g_log.push_back("WorldportAck");
}
inline void Player::ResurrectUsingRequestData(){
    // 照抄 Player.cpp:24005 的真实行为
    TeleportTo(1,false);                      // 先传到尸体（同地图=near）
    if(IsBeingTeleportedNear()||IsBeingTeleportedFar()){
        delayedResurrectPending=true;         // ScheduleDelayedOperation
        g_log.push_back("ResurrectDeferred");
        return;                               // 【真正复活被推迟】
    }
    resurrected=true; g_log.push_back("ResurrectNow");
}
inline void Player::SummonIfPossible(bool agree){
    if(!agree) return;
    if(!summonPending) return;
    summonPending=false; summoned=true;
    TeleportTo(2,true);                       // 召唤通常跨地图
    g_log.push_back("SummonTeleport");
}

// ---- WorldScript 桩 ----
class WorldScript { public: WorldScript(char const*){} virtual ~WorldScript(){}
    virtual void OnUpdate(uint32){} };
struct ChatHandlerStub { ChatHandlerStub(WorldSession*){}
    void PSendSysMessage(char const* f,...){ g_log.push_back("Notify"); } };
#define ChatHandler ChatHandlerStub
namespace ObjectAccessor { Player* FindPlayer(ObjectGuid g); }
