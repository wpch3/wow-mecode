#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <algorithm>
typedef uint8_t uint8; typedef uint32_t uint32; typedef int32_t int32;
#define TC_GAME_API
#define TC_LOG_INFO(a,...) do{}while(0)

// 严格对照 SharedDefines.h 的真实值
enum SpellEffects {
    SPELL_EFFECT_TELEPORT_UNITS = 5,
    SPELL_EFFECT_RESURRECT      = 18,
    SPELL_EFFECT_BIND           = 36,
    SPELL_EFFECT_SELF_RESURRECT = 94,
    SPELL_EFFECT_RESURRECT_NEW  = 113,
};

struct ObjectGuid { uint32 GetCounter() const { return 1; } };
class SpellInfo {
public:
    bool HasEffect(SpellEffects) const { return false; }
};
class Player {
public:
    ObjectGuid GetGUID() const { return {}; }
    uint8 GetClass() const { return 1; }           // Unit.h:896
    bool InBattleground() const { return false; }  // Player.h:1965
    bool InArena() const { return false; }         // Player.h:1966
};
struct CfgMgr {
    bool GetBoolDefault(char const*, bool d, bool=false) const { return d; }
    int  GetIntDefault(char const*, int d, bool=false) const { return d; }
};
inline CfgMgr* _cfg() { static CfgMgr c; return &c; }
#define sConfigMgr _cfg()
