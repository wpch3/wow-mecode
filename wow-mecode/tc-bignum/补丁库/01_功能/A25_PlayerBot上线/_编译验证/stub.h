// 桩：模拟 TrinityCore 的相关 API（签名与访问段严格照抄实查结果）
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <cstdio>
typedef uint8_t uint8; typedef uint32_t uint32;
#define PLAYER_FLAGS 1
#define PLAYER_FLAGS_IN_PVP 0x00000200

static inline float NormalizeOrientation(float o){ if(o<0){float t=std::fmod(o,6.2831853f);return t+6.2831853f;} return std::fmod(o,6.2831853f);}    

// Position.h:27  struct（默认 public）
struct Position {
    float m_positionX=0,m_positionY=0,m_positionZ=0,m_orientation=0;
    float GetPositionX() const {return m_positionX;}
    float GetPositionY() const {return m_positionY;}
    float GetPositionZ() const {return m_positionZ;}
    float GetOrientation() const {return m_orientation;}
    // Position.h:128
    float GetAbsoluteAngle(float x,float y) const {
        float dx=x-m_positionX, dy=y-m_positionY;
        return NormalizeOrientation(std::atan2(dy,dx));
    }
};
struct WorldLocation : public Position { uint32 mapId=0; };
struct PvPInfo { bool IsHostile=false; };

struct WorldSessionStub;
struct Player;

// 记录调用顺序，用来断言行为
extern std::vector<std::string> g_calls;
