#include "stub.h"

struct Player;

// WorldSession（只保留用到的）
struct WorldSessionStub {
    Player* _p=nullptr;
    void HandleMoveWorldportAck();   // WorldSession.h:829 public(706段)
};

struct Player : public Position {
    uint32 mapId=0, zoneId=0, phase=1;
    bool nearSem=false, farSem=false, inFlight=false, movedByClient=true;
    bool pvp=false; uint32 pflags=0;
    PvPInfo pvpInfo;
    WorldLocation tdest;
    WorldSessionStub sess;
    std::string name;
    bool teleportOk=true;
    Player(){ sess._p=this; }

    uint32 GetMapId() const {return mapId;}
    uint32 GetZoneId() const {return zoneId;}
    void GetZoneAndAreaId(uint32&z,uint32&a) const {z=zoneId;a=zoneId;}
    uint32 GetPhaseMask() const {return phase;}
    float GetCombatReach() const {return 1.5f;}
    WorldSessionStub* GetSession(){return &sess;}

    bool IsBeingTeleportedNear() const {return nearSem;}
    bool IsBeingTeleportedFar()  const {return farSem;}
    bool IsBeingTeleported() const {return nearSem||farSem;}
    void SetSemaphoreTeleportNear(bool v){nearSem=v;}
    void SetSemaphoreTeleportFar(bool v){farSem=v;}
    WorldLocation const& GetTeleportDest() const {return tdest;}
    bool IsMovedByClient() const {return movedByClient;}
    bool IsInFlight() const {return inFlight;}
    void FinishTaxiFlight(){inFlight=false;g_calls.push_back("FinishTaxiFlight");}
    void SaveRecallPosition(){g_calls.push_back("SaveRecallPosition");}

    // Object.h:370  GetFirstCollisionPosition(dist, angle)
    Position GetFirstCollisionPosition(float dist,float angle) const {
        Position p; float a=angle+m_orientation;
        p.m_positionX=m_positionX+dist*std::cos(a);
        p.m_positionY=m_positionY+dist*std::sin(a);
        p.m_positionZ=m_positionZ; return p;
    }
    void GetClosePoint(float&x,float&y,float&z,float size,float d2=0,float rel=0) const {
        float a=m_orientation+rel; float dist=d2+size;
        x=m_positionX+dist*std::cos(a); y=m_positionY+dist*std::sin(a); z=m_positionZ;
    }

    // Player.cpp:1557 —— 模拟真实两分支行为
    bool TeleportTo(uint32 map,float x,float y,float z,float o,uint32 opt=0){
        if(!teleportOk) return false;
        tdest.mapId=map; tdest.m_positionX=x; tdest.m_positionY=y;
        tdest.m_positionZ=z; tdest.m_orientation=o;
        if(map==mapId){ SetSemaphoreTeleportNear(IsMovedByClient());
            g_calls.push_back("TeleportTo_NEAR");
            if(!IsBeingTeleportedNear()) UpdatePosition(tdest,true);
        } else { SetSemaphoreTeleportFar(true); g_calls.push_back("TeleportTo_FAR"); }
        return true;
    }
    // 真实实现：Unit.cpp:13789 UpdatePosition 内部会调 UpdatePositionData()
    // -> Object.cpp:1055 -> ProcessPositionDataChanged -> m_zoneId 被重算
    uint32 destZoneOverride=0xFFFFFFFF;  // 桩：模拟"落点属于哪个区"
    bool UpdatePosition(Position const& p,bool tp=false){
        m_positionX=p.m_positionX;m_positionY=p.m_positionY;
        m_positionZ=p.m_positionZ;m_orientation=p.m_orientation;
        if(destZoneOverride!=0xFFFFFFFF) zoneId=destZoneOverride;  // Object.cpp:1063
        g_calls.push_back("UpdatePosition"); return true;
    }
    void SetFallInformation(uint32,float){g_calls.push_back("SetFallInformation");}
    void UpdateZone(uint32 z,uint32){zoneId=z;g_calls.push_back("UpdateZone");}
    bool IsPvP() const {return pvp;}
    bool HasFlag(int,uint32 f) const {return (pflags&f)!=0;}
    void UpdatePvP(bool,bool){g_calls.push_back("UpdatePvP");}
    void CastSpell(Player*,uint32,bool){g_calls.push_back("CastSpell");}
    void ResummonPetTemporaryUnSummonedIfAny(){g_calls.push_back("ResummonPet");}
    void ProcessDelayedOperations(){g_calls.push_back("ProcessDelayed");}
    void SetPhaseMask(uint32 m,bool){phase=m;g_calls.push_back("SetPhaseMask");}
    void StopMoving(bool force=false){g_calls.push_back("StopMoving");}
};

inline void WorldSessionStub::HandleMoveWorldportAck(){
    if(!_p->IsBeingTeleportedFar()) return;
    _p->SetSemaphoreTeleportFar(false);
    _p->mapId=_p->tdest.mapId;
    _p->UpdatePosition(_p->tdest,true);
    g_calls.push_back("WorldportAck");
}
