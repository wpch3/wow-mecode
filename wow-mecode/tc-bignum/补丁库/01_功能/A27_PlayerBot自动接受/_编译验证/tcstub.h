// 桩：模拟 TrinityCore API（签名严格照抄实查结果）
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <cstdio>
typedef uint8_t uint8; typedef uint16_t uint16; typedef uint32_t uint32; typedef uint64_t uint64;

extern std::vector<std::string> g_log;

struct ObjectGuid {
    uint64 v=0;
    ObjectGuid(){} explicit ObjectGuid(uint64 x):v(x){}
    bool IsEmpty() const {return v==0;}
    bool operator==(ObjectGuid const&o) const {return v==o.v;}
    bool operator!=(ObjectGuid const&o) const {return v!=o.v;}
    bool operator<(ObjectGuid const&o) const {return v<o.v;}
    std::string ToString() const {return std::to_string(v);}
    typedef uint32 LowType;
    static ObjectGuid const Empty;
};
struct Position{float m_positionX=0,m_positionY=0,m_positionZ=0,m_orientation=0;
    float GetPositionZ() const {return m_positionZ;}};
struct WorldLocation : Position { uint32 mapId=0; };

#define PLAYER_DUEL_ARBITER 177
enum DuelState { DUEL_STATE_CHALLENGED, DUEL_STATE_COUNTDOWN, DUEL_STATE_IN_PROGRESS, DUEL_STATE_COMPLETED };
class Player;
struct DuelInfo {
    DuelInfo(Player* o,Player* i,bool m):Opponent(o),Initiator(i),IsMounted(m){}
    Player* const Opponent; Player* const Initiator; bool const IsMounted;
    DuelState State = DUEL_STATE_CHALLENGED;
};

class WorldPacket { public: uint32 op=0; uint64 payload=0;
    WorldPacket(uint32 o,size_t){op=o;}
    WorldPacket& operator<<(ObjectGuid const& g){payload=g.v;return *this;} };

class Group; class Guild; class WorldSession;

// ---- step42b: 交易 / 尸体释放 ----
#define CMSG_BEGIN_TRADE 0x117
#define PLAYER_FLAGS 1
#define PLAYER_FLAGS_GHOST 0x00000010
enum DeathState { ALIVE=0, JUST_DIED=1, CORPSE=2, DEAD=3, JUST_RESPAWNED=4 };
class TradeData {
public:
    Player* _trader=nullptr; bool accepted=false;
    TradeData(Player*,Player* t):_trader(t){}
    Player* GetTrader() const {return _trader;}
    bool IsAccepted() const {return accepted;}
};


class Group {
public:
    ObjectGuid leader; bool full=false, created=false;
    std::vector<Player*> members, invitees;
    bool addMemberResult=true;
    void RemoveInvite(Player* p);
    void RemoveAllInvites(){invitees.clear(); g_log.push_back("RemoveAllInvites");}
    bool IsFull() const {return full;}
    bool IsCreated() const {return created;}
    ObjectGuid GetLeaderGUID() const {return leader;}
    bool Create(Player* l);
    bool AddMember(Player* p);
    void BroadcastGroupUpdate(){g_log.push_back("BroadcastGroupUpdate");}
};
struct GroupMgrStub { void AddGroup(Group*){g_log.push_back("AddGroup");} };
extern GroupMgrStub _gmgr;
#define sGroupMgr (&_gmgr)

class Guild {
public:
    uint32 id=0; bool acceptCalled=false;
    void HandleAcceptMember(WorldSession*){acceptCalled=true; g_log.push_back("HandleAcceptMember");}
};
struct GuildMgrStub { Guild* g=nullptr; Guild* GetGuildById(uint32 id) const {return (g&&g->id==id)?g:nullptr;} };
extern GuildMgrStub _gumgr;
#define sGuildMgr (&_gumgr)
