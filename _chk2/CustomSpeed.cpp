/*
 * ============================================================================
 *  战斗节奏优化 —— CustomSpeed.cpp
 *
 *  放置：D:\TrinityCore\src\server\game\Spells\CustomSpeed.cpp
 * ============================================================================
 */

#include "CustomSpeed.h"
#include "stub.h"
#include <algorithm>
#include <string>

CustomSpeedMgr* CustomSpeedMgr::instance()
{
    static CustomSpeedMgr inst;
    return &inst;
}

void CustomSpeedMgr::LoadConfig()
{
    // sConfigMgr->GetIntDefault 可以直接读任意键名，
    // 不需要注册进 World.h 的 WorldIntConfigs 枚举（社区标准做法）
    _enabled           = sConfigMgr->GetBoolDefault("Speed.Enable", false);
    _gcdPct            = uint32(sConfigMgr->GetIntDefault("Speed.Gcd", 100));
    _castPct           = uint32(sConfigMgr->GetIntDefault("Speed.Cast", 100));
    _cdPct             = uint32(sConfigMgr->GetIntDefault("Speed.Cd", 100));
    _gcdFloor          = uint32(sConfigMgr->GetIntDefault("Speed.GcdFloor", 300));
    _castFloor         = uint32(sConfigMgr->GetIntDefault("Speed.CastFloor", 500));
    _cdFloor           = uint32(sConfigMgr->GetIntDefault("Speed.CdFloor", 3000));
    _hasteAffectsMelee = sConfigMgr->GetBoolDefault("Speed.HasteAffectsMelee", false);
    _pvpRestore        = sConfigMgr->GetBoolDefault("Speed.PvpRestoreOriginal", true);
    _exemptResurrect   = sConfigMgr->GetBoolDefault("Speed.Exempt.Resurrect", true);
    _exemptTeleport    = sConfigMgr->GetBoolDefault("Speed.Exempt.Teleport", true);
    _cdShortPct        = uint32(sConfigMgr->GetIntDefault("Speed.Cd.Short", 85));
    _cdMediumPct       = uint32(sConfigMgr->GetIntDefault("Speed.Cd.Medium", 80));
    _cdLongPct         = uint32(sConfigMgr->GetIntDefault("Speed.Cd.Long", 90));
    _cdMinScale        = uint32(sConfigMgr->GetIntDefault("Speed.Cd.MinScale", 3000));
    _castNotBelowGcd   = sConfigMgr->GetBoolDefault("Speed.CastNotBelowGcd", true);

    for (uint8 c = 1; c < 32; ++c)
    {
        _gcdByClass[c]  = uint32(sConfigMgr->GetIntDefault(
                            ("Speed.Gcd.Class"  + std::to_string(c)).c_str(), 0));
        _castByClass[c] = uint32(sConfigMgr->GetIntDefault(
                            ("Speed.Cast.Class" + std::to_string(c)).c_str(), 0));
    }

    if (_enabled)
        TC_LOG_INFO("server.loading",
            ">> 战斗节奏优化已启用：GCD {}% / 读条 {}% / CD {}% "
            "(地板 {}ms/{}ms/{}ms，急速影响近战={}，PVP恢复原版={})",
            _gcdPct, _castPct, _cdPct, _gcdFloor, _castFloor, _cdFloor,
            _hasteAffectsMelee ? "是" : "否", _pvpRestore ? "是" : "否");
    else
        TC_LOG_INFO("server.loading", ">> 战斗节奏优化：已关闭（Speed.Enable = 0）");
}

// ---------------------------------------------------------------------------
//  PVP 保护：战场/竞技场内恢复原版数值
// ---------------------------------------------------------------------------
bool CustomSpeedMgr::ShouldSkip(Player const* player) const
{
    if (!_enabled || !player)
        return true;

    if (_pvpRestore && (player->InBattleground() || player->InArena()))
        return true;

    return false;
}

// ---------------------------------------------------------------------------
//  白名单：这些技能不缩放，避免破坏机制
// ---------------------------------------------------------------------------
bool CustomSpeedMgr::IsExempt(SpellInfo const* info) const
{
    if (!info)
        return true;

    if (_exemptResurrect &&
        (info->HasEffect(SPELL_EFFECT_RESURRECT)
      || info->HasEffect(SPELL_EFFECT_RESURRECT_NEW)
      || info->HasEffect(SPELL_EFFECT_SELF_RESURRECT)))
        return true;

    if (_exemptTeleport &&
        (info->HasEffect(SPELL_EFFECT_TELEPORT_UNITS)
      || info->HasEffect(SPELL_EFFECT_BIND)))
        return true;

    return false;
}

// ---------------------------------------------------------------------------
//  取值：个人覆盖 > 职业覆盖 > 全局
// ---------------------------------------------------------------------------
uint32 CustomSpeedMgr::GetGcdPct(uint8 cls) const
{
    if (cls < 32 && _gcdByClass[cls])
        return _gcdByClass[cls];
    return _gcdPct;
}

uint32 CustomSpeedMgr::GetCastPct(uint8 cls) const
{
    if (cls < 32 && _castByClass[cls])
        return _castByClass[cls];
    return _castPct;
}

// CD 分段：长CD少压，保留大招稀有感
uint32 CustomSpeedMgr::GetCdPct(int32 cooldownMs) const
{
    if (cooldownMs < 30 * 1000)            // < 30 秒
        return _cdShortPct;
    if (cooldownMs <= 3 * 60 * 1000)       // 30 秒 ~ 3 分钟
        return _cdMediumPct;
    return _cdLongPct;                     // > 3 分钟
}

// ---------------------------------------------------------------------------
//  GCD 缩放
//  插入点：Spell.cpp TriggerGlobalCooldown()，AddGlobalCooldown 之前
// ---------------------------------------------------------------------------
int32 CustomSpeedMgr::ScaleGcd(Player const* player, int32 gcd) const
{
    if (gcd <= 0 || ShouldSkip(player))
        return gcd;

    uint32 pct = GetPersonalGcd(player->GetGUID().GetCounter());
    if (!pct)
        pct = GetGcdPct(player->GetClass());

    if (pct == 100)
        return gcd;

    gcd = int32(float(gcd) * float(pct) / 100.0f);
    return std::max<int32>(gcd, int32(_gcdFloor));
}

// ---------------------------------------------------------------------------
//  算出该玩家当前的有效 GCD（用作读条的下限）
//
//  原理：读条比 GCD 短的那部分是白给的——技能念完了但 GCD 没转好，
//        还是得干等。所以把读条压到低于 GCD 毫无意义，
//        反而会让读条条一闪而过，观感变差。
// ---------------------------------------------------------------------------
int32 CustomSpeedMgr::GetEffectiveGcdFloor(Player const* player) const
{
    if (!player)
        return int32(_castFloor);

    // 标准 GCD 1500ms 缩放后的值
    uint32 pct = GetPersonalGcd(player->GetGUID().GetCounter());
    if (!pct)
        pct = GetGcdPct(player->GetClass());

    int32 gcd = int32(1500.0f * float(pct) / 100.0f);
    return std::max<int32>(gcd, int32(_gcdFloor));
}

// ---------------------------------------------------------------------------
//  读条缩放
//  插入点：Spell.cpp prepare()，CalcCastTime() 之后
//
//  为什么读条也要压：近战技能瞬发，卡在 GCD；法系主力要读条，
//  卡在读条时间。只压 GCD 的话，法师火球术 3.5 秒一点没变快，
//  近战却快了 1.67 倍 —— 法系会被落下。
// ---------------------------------------------------------------------------
int32 CustomSpeedMgr::ScaleCastTime(Player const* player, SpellInfo const* info, int32 castTime) const
{
    if (castTime <= 0 || ShouldSkip(player) || IsExempt(info))
        return castTime;

    uint32 pct = GetPersonalCast(player->GetGUID().GetCounter());
    if (!pct)
        pct = GetCastPct(player->GetClass());

    if (pct == 100)
        return castTime;

    int32 scaled = int32(float(castTime) * float(pct) / 100.0f);

    // 保护一：不低于读条地板
    scaled = std::max<int32>(scaled, int32(_castFloor));

    // 保护二：不压到低于 GCD（低于也没用，反正要等 GCD 转好）
    // 但如果原始读条本来就比 GCD 短（如瞬发/短读条技能），就不干预
    if (_castNotBelowGcd)
    {
        int32 gcdFloor = GetEffectiveGcdFloor(player);
        if (castTime > gcdFloor)                 // 原读条 > GCD 才需要保护
            scaled = std::max<int32>(scaled, gcdFloor);
    }

    return scaled;
}

// ---------------------------------------------------------------------------
//  单技能 CD 缩放
//  插入点：SpellHistory.cpp StartCooldown()，"replace negative cooldowns" 之前
// ---------------------------------------------------------------------------
void CustomSpeedMgr::ScaleCooldown(Player const* player, SpellInfo const* info,
                                   int32& cooldown, int32& categoryCooldown) const
{
    if (ShouldSkip(player) || IsExempt(info))
        return;

    uint32 personal = GetPersonalCd(player->GetGUID().GetCounter());

    auto apply = [&](int32& cd)
    {
        if (cd <= 0 || uint32(cd) < _cdMinScale)   // 太短的 CD 不动
            return;

        uint32 pct = personal ? personal : GetCdPct(cd);
        if (pct == 100)
            return;

        // 全局 Speed.Cd 作为总开关再乘一次（100 时无影响）
        if (!personal && _cdPct != 100)
            pct = pct * _cdPct / 100;

        int32 scaled = int32(float(cd) * float(pct) / 100.0f);
        cd = std::max<int32>(scaled, int32(_cdFloor));
    };

    apply(cooldown);
    apply(categoryCooldown);
}

// ---------------------------------------------------------------------------
//  个人临时覆盖
// ---------------------------------------------------------------------------
void CustomSpeedMgr::SetPersonalGcd(uint32 guid, uint32 pct)
{
    if (pct) _personalGcd[guid] = pct; else _personalGcd.erase(guid);
}
void CustomSpeedMgr::SetPersonalCast(uint32 guid, uint32 pct)
{
    if (pct) _personalCast[guid] = pct; else _personalCast.erase(guid);
}
void CustomSpeedMgr::SetPersonalCd(uint32 guid, uint32 pct)
{
    if (pct) _personalCd[guid] = pct; else _personalCd.erase(guid);
}
void CustomSpeedMgr::ClearPersonal(uint32 guid)
{
    _personalGcd.erase(guid);
    _personalCast.erase(guid);
    _personalCd.erase(guid);
}
uint32 CustomSpeedMgr::GetPersonalGcd(uint32 guid) const
{
    auto it = _personalGcd.find(guid);
    return it != _personalGcd.end() ? it->second : 0;
}
uint32 CustomSpeedMgr::GetPersonalCast(uint32 guid) const
{
    auto it = _personalCast.find(guid);
    return it != _personalCast.end() ? it->second : 0;
}
uint32 CustomSpeedMgr::GetPersonalCd(uint32 guid) const
{
    auto it = _personalCd.find(guid);
    return it != _personalCd.end() ? it->second : 0;
}
