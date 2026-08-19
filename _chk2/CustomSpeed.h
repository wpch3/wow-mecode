/*
 * ============================================================================
 *  战斗节奏优化 —— CustomSpeed.h
 * ============================================================================
 *
 *  作用：把 GCD / 读条 / 单技能CD 按百分比缩放，让战斗不那么"站桩"，
 *        但保留节奏感 —— 不是 .cheat cooldown 那种一刀切归零。
 *
 *  设计要点：
 *    1. 三层独立缩放，各自可调、各自有地板
 *    2. 修复原版"急速只对法系 GCD 生效"的设计缺陷
 *    3. 单技能CD 分段缩放：长CD少压，保留大招的稀有感
 *    4. PVP（战场/竞技场）自动恢复原版数值
 *    5. 白名单：复活/传送等技能不缩放，避免破坏机制
 *    6. 全部参数走 worldserver.conf.d/speed.conf，改完 .reload config 生效
 *
 *  放置：D:\TrinityCore\src\server\game\Spells\CustomSpeed.h
 * ============================================================================
 */

#ifndef TRINITY_CUSTOMSPEED_H
#define TRINITY_CUSTOMSPEED_H

#include "stub.h"
#include <unordered_map>

class Player;
class SpellInfo;

class TC_GAME_API CustomSpeedMgr
{
    CustomSpeedMgr() { LoadConfig(); }
    ~CustomSpeedMgr() = default;

    CustomSpeedMgr(CustomSpeedMgr const&) = delete;
    CustomSpeedMgr& operator=(CustomSpeedMgr const&) = delete;

public:
    static CustomSpeedMgr* instance();

    // 从 conf 读取全部设置（启动时 + .reload config 时调用）
    void LoadConfig();

    // ---- 三个缩放接口，核心代码只调用这三个 ----
    int32 ScaleGcd(Player const* player, int32 gcd) const;
    int32 ScaleCastTime(Player const* player, SpellInfo const* info, int32 castTime) const;
    // 读条保护用：算出该玩家当前的有效 GCD 下限
    int32 GetEffectiveGcdFloor(Player const* player) const;
    void  ScaleCooldown(Player const* player, SpellInfo const* info,
                        int32& cooldown, int32& categoryCooldown) const;

    // ---- 急速是否影响近战 GCD（修复原版缺陷）----
    bool HasteAffectsMelee() const { return _hasteAffectsMelee; }

    // ---- 给 .speed 指令用 ----
    bool  IsEnabled() const { return _enabled; }
    uint32 GetGcdPct(uint8 cls) const;
    uint32 GetCastPct(uint8 cls) const;
    uint32 GetCdPct(int32 cooldownMs) const;
    uint32 GetGcdFloor() const  { return _gcdFloor; }
    uint32 GetCastFloor() const { return _castFloor; }
    uint32 GetCdFloor() const   { return _cdFloor; }
    bool   GetPvpRestore() const { return _pvpRestore; }

    // 个人临时覆盖（.speed gcd 50），0 表示不覆盖
    void SetPersonalGcd(uint32 guid, uint32 pct);
    void SetPersonalCast(uint32 guid, uint32 pct);
    void SetPersonalCd(uint32 guid, uint32 pct);
    void ClearPersonal(uint32 guid);
    uint32 GetPersonalGcd(uint32 guid) const;
    uint32 GetPersonalCast(uint32 guid) const;
    uint32 GetPersonalCd(uint32 guid) const;

private:
    // 判断该玩家当前是否应当跳过缩放（PVP 保护）
    bool ShouldSkip(Player const* player) const;
    // 判断该法术是否在白名单里（不缩放）
    bool IsExempt(SpellInfo const* info) const;

    bool   _enabled          = false;
    uint32 _gcdPct           = 100;
    uint32 _castPct          = 100;
    uint32 _cdPct            = 100;
    uint32 _gcdFloor         = 300;
    uint32 _castFloor        = 500;
    uint32 _cdFloor          = 3000;
    bool   _hasteAffectsMelee = false;
    bool   _pvpRestore        = true;
    bool   _exemptResurrect   = true;
    bool   _exemptTeleport    = true;
    uint32 _cdShortPct        = 85;   // CD < 30 秒
    uint32 _cdMediumPct       = 80;   // 30 秒 ~ 3 分钟
    uint32 _cdLongPct         = 90;   // > 3 分钟
    uint32 _cdMinScale        = 3000; // 小于这个 CD 不缩放
    bool   _castNotBelowGcd   = true; // 读条不压到低于 GCD（避免白压）

    // 按职业覆盖，0 = 用全局值。下标 = 职业 ID，预留到 32
    uint32 _gcdByClass[32]  = { };
    uint32 _castByClass[32] = { };

    // 个人临时覆盖
    std::unordered_map<uint32, uint32> _personalGcd;
    std::unordered_map<uint32, uint32> _personalCast;
    std::unordered_map<uint32, uint32> _personalCd;
};

#define sCustomSpeed CustomSpeedMgr::instance()

#endif
