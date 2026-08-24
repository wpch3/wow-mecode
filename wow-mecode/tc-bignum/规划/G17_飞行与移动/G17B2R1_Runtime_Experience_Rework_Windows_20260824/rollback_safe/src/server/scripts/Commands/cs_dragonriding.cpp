/*
 * G17-B2: complete momentum flight, corrected forward climb, and typed landing.
 *
 * B2 preserves the proven B1R3 authoritative seat chain, B1R4 250 ms indoor
 * enforcement, and B1R5 wrapper-mount resolution. Flight speed is a bounded,
 * smooth seven-stage state machine (250%..1200%) driven by forward motion,
 * dive, climb, turning, braking and idle drag. Skill 3 uses one short forward
 * jump spline and explicitly restores client flight control. Skill 4 suppresses
 * the parachute aura and selects dragon, beast, magic/wind, mechanical-rocket,
 * or generic landing motion while normalizing every temporary movement state.
 */

#include "ScriptMgr.h"
#include "Chat.h"
#include "CombatAI.h"
#include "DBCStores.h"
#include "EventProcessor.h"
#include "GridDefines.h"
#include "Log.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "RBAC.h"
#include "ScriptedCreature.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "TemporarySummon.h"
#include "Vehicle.h"
#include "WorldSession.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <initializer_list>
#include <mutex>
#include <string>
#include <unordered_map>

using namespace Trinity::ChatCommands;

namespace G17Dragonriding
{
constexpr uint32 NPC_DRAGONRIDING_VEHICLE = 1000171;

enum MountArchetype : uint32
{
    ARCHETYPE_GENERIC = 0,
    ARCHETYPE_BEAST = 1,
    ARCHETYPE_DRAGON = 2,
    ARCHETYPE_MAGIC = 3,
    ARCHETYPE_MECHANICAL = 4
};

enum SessionData : uint32
{
    DATA_SOURCE_SPELL = 17010,
    DATA_SOURCE_CREATURE = 17011,
    DATA_SOURCE_DISPLAY = 17012,
    DATA_ARCHETYPE = 17013,
    DATA_MOMENTUM_PERCENT = 17014,
    DATA_SPEED_PERCENT = 17015,
    DATA_FLIGHT_STATE = 17016
};

enum FlightState : uint32
{
    FLIGHT_STATE_NORMAL = 0,
    FLIGHT_STATE_CLIMBING = 1,
    FLIGHT_STATE_STALLING = 2,
    FLIGHT_STATE_LANDING = 3
};

std::mutex AutoPreferenceLock;
std::unordered_map<ObjectGuid::LowType, bool> AutoPreference;

bool IsAutoEnabled(Player const* player)
{
    if (!player)
        return false;

    std::lock_guard<std::mutex> guard(AutoPreferenceLock);
    auto const itr = AutoPreference.find(player->GetGUID().GetCounter());
    return itr == AutoPreference.end() ? true : itr->second;
}

void SetAutoEnabled(Player const* player, bool enabled)
{
    if (!player)
        return;

    std::lock_guard<std::mutex> guard(AutoPreferenceLock);
    AutoPreference[player->GetGUID().GetCounter()] = enabled;
}

void ForgetAutoPreference(Player const* player)
{
    if (!player)
        return;

    std::lock_guard<std::mutex> guard(AutoPreferenceLock);
    AutoPreference.erase(player->GetGUID().GetCounter());
}

char const* ArchetypeName(uint32 archetype)
{
    switch (archetype)
    {
        case ARCHETYPE_BEAST: return "BEAST";
        case ARCHETYPE_DRAGON: return "DRAGON";
        case ARCHETYPE_MAGIC: return "MAGIC";
        case ARCHETYPE_MECHANICAL: return "MECHANICAL";
        default: return "GENERIC";
    }
}

std::string CollectSpellNames(SpellInfo const* spellInfo)
{
    std::string result;
    if (!spellInfo)
        return result;

    for (char const* name : spellInfo->SpellName)
    {
        if (!name || !*name)
            continue;
        result.append(name);
        result.push_back(' ');
    }

    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char value)
    {
        return char(std::tolower(value));
    });
    return result;
}

bool ContainsAny(std::string const& text, std::initializer_list<char const*> tokens)
{
    for (char const* token : tokens)
        if (text.find(token) != std::string::npos)
            return true;
    return false;
}

uint32 InferArchetype(uint32 creatureEntry, SpellInfo const* outerSpell = nullptr,
    SpellInfo const* innerSpell = nullptr)
{
    std::string const names = CollectSpellNames(outerSpell) + CollectSpellNames(innerSpell);

    // Name classification is deliberately multilingual because wrapper spells
    // often hide the inner creature type. Specific mechanical and magical
    // families win before the broad creature-template fallback.
    if (ContainsAny(names, { "rocket", "mechan", "gyrocopter", "flying machine", "chopper",
        "motorcycle", "x-51", "x-53", "火箭", "机械", "飞行器", "摩托", "直升机", "涡轮" }))
        return ARCHETYPE_MECHANICAL;
    if (ContainsAny(names, { "proto-drake", "drake", "dragon", "wyrm", "frostbrood",
        "netherwing", "twilight", "幼龙", "始祖龙", "元龙", "冰霜巨龙", "巨龙", "龙" }))
        return ARCHETYPE_DRAGON;
    if (ContainsAny(names, { "carpet", "broom", "phoenix", "magic", "enchanted", "celestial",
        "nether ray", "headless", "deathcharger", "spectral", "飞毯", "扫帚", "凤凰", "魔法",
        "魔化", "星界", "虚空鳐", "无头骑士", "亡灵", "幽灵", "骷髅" }))
        return ARCHETYPE_MAGIC;
    if (ContainsAny(names, { "horse", "wolf", "tiger", "bear", "raptor", "kodo", "mammoth",
        "elekk", "hawkstrider", "ram", "boar", "豹", "虎", "狼", "熊", "马", "科多兽",
        "猛犸", "迅猛龙", "陆行鸟", "山羊", "野兽" }))
        return ARCHETYPE_BEAST;

    CreatureTemplate const* mountTemplate = sObjectMgr->GetCreatureTemplate(creatureEntry);
    if (!mountTemplate)
        return ARCHETYPE_GENERIC;

    switch (mountTemplate->type)
    {
        case CREATURE_TYPE_DRAGONKIN: return ARCHETYPE_DRAGON;
        case CREATURE_TYPE_MECHANICAL: return ARCHETYPE_MECHANICAL;
        case CREATURE_TYPE_BEAST: return ARCHETYPE_BEAST;
        case CREATURE_TYPE_DEMON:
        case CREATURE_TYPE_ELEMENTAL: return ARCHETYPE_MAGIC;
        default: return ARCHETYPE_GENERIC;
    }
}

bool HasMountAuraMetadata(SpellInfo const* spellInfo)
{
    if (!spellInfo)
        return false;

    // SpellMgr intentionally sets some wrapper mount effects to NONE while
    // retaining their original ApplyAuraName metadata.  Checking the metadata
    // recognizes both direct mounts and wrappers without hard-coding spell IDs.
    for (SpellEffectInfo const& effect : spellInfo->GetEffects())
        if (effect.ApplyAuraName == SPELL_AURA_MOUNTED)
            return true;

    return false;
}

AuraEffect const* FindOwnedMountAura(Player const* player)
{
    if (!player)
        return nullptr;

    for (AuraEffect const* effect : player->GetAuraEffectsByType(SPELL_AURA_MOUNTED))
        if (effect)
            return effect;

    return nullptr;
}

constexpr uint32 SPELL_DRAGON_BREATH = 9573;  // Flame Breath
constexpr uint32 SPELL_ACCELERATE     = 55215; // Its speed aura is suppressed; B2 owns the cap.
constexpr uint32 SPELL_CLIMB          = 52197; // Jump Jets: intercepted only for this vehicle.
constexpr uint32 SPELL_SAFE_LANDING   = 52226; // Visual-free dummy command; no aura.

constexpr int32 ACTION_CLIMB = 1;
constexpr int32 ACTION_LAND = 2;
constexpr int32 ACTION_ACCELERATE = 3;
constexpr uint32 POINT_CLIMB = 17001;
constexpr uint32 POINT_LAND = 17002;
constexpr uint32 POINT_LAND_APPROACH = 17003;

constexpr uint32 BREATH_ENERGY_COST = 20;
constexpr uint32 BOOST_ENERGY_COST = 30;
constexpr uint32 CLIMB_ENERGY_COST = 25;
constexpr float CLIMB_FORWARD_DISTANCE = 14.0f;
constexpr float CLIMB_HEIGHT = 8.0f;
constexpr float CLIMB_HORIZONTAL_SPEED = 20.0f;
constexpr float CLIMB_VERTICAL_SPEED = 10.0f;
constexpr uint32 CLIMB_CONTROL_TIMEOUT_MS = 1500;
constexpr uint32 BOOST_DURATION_MS = 4000;
constexpr uint32 SAFETY_CHECK_INTERVAL_MS = 250;
constexpr uint32 MOTION_SAMPLE_INTERVAL_MS = 100;
constexpr uint32 SPEED_PACKET_INTERVAL_MS = 200;
constexpr uint32 FALL_GUARD_INTERVAL_MS = 100;
constexpr uint32 FALL_GUARD_MAX_CHECKS = 200;
constexpr uint32 FALL_GUARD_START_CHECKS = 10;

constexpr uint32 VISUAL_KIT_BOOST = 44;              // Charge trail + dust, one-shot cast kit.
constexpr uint32 VISUAL_KIT_MAGIC_WIND = 11818;      // Blue cyclone state on the landing vehicle.
constexpr uint32 VISUAL_KIT_MECHANICAL_ROCKET = 13481; // Jet-pack flame model attachment.
constexpr uint32 VISUAL_KIT_BEAST_POUNCE = 44;       // Fire-free charge trail + dust.
constexpr uint32 VISUAL_KIT_LANDING_DUST = 1066;     // Dust-cloud landing impact.

constexpr std::array<float, 7> FLIGHT_SPEED_RATES = { 2.5f, 4.0f, 6.0f, 8.0f, 10.0f, 11.0f, 12.0f };
constexpr std::array<float, 7> MOMENTUM_THRESHOLDS = { 0.0f, 0.18f, 0.34f, 0.50f, 0.66f, 0.82f, 0.94f };
constexpr float MIN_MOMENTUM = 0.0f;
constexpr float MAX_MOMENTUM = 1.0f;
constexpr float STALL_ENTER_MOMENTUM = 0.06f;
constexpr float STALL_RECOVER_MOMENTUM = 0.20f;

bool IsDragon(Unit const* unit)
{
    return unit && unit->GetEntry() == NPC_DRAGONRIDING_VEHICLE;
}

Creature* GetDragon(Player* player)
{
    if (!player)
        return nullptr;

    Unit* base = player->GetVehicleBase();
    return IsDragon(base) ? base->ToCreature() : nullptr;
}

int8 GetControllableSeatId(Vehicle const* vehicle)
{
    if (!vehicle)
        return -1;

    for (auto const& seatPair : vehicle->Seats)
        if (seatPair.second.SeatInfo &&
            seatPair.second.SeatInfo->HasFlag(VEHICLE_SEAT_FLAG_CAN_CONTROL))
            return seatPair.first;

    return -1;
}

// Vehicle::Seats is server-authoritative. GetTransSeat() is movement-packet
// state and can transiently return -1 even after VehicleJoinEvent has added
// the passenger and established vehicle charm/control.
int8 GetAuthoritativePassengerSeatId(Vehicle const* vehicle, Unit const* passenger)
{
    if (!vehicle || !passenger)
        return -1;

    for (auto const& seatPair : vehicle->Seats)
        if (vehicle->GetPassenger(seatPair.first) == passenger)
            return seatPair.first;

    return -1;
}

Player* GetRider(Creature* dragon)
{
    if (!dragon)
        return nullptr;

    if (Vehicle* vehicle = dragon->GetVehicleKit())
        for (auto const& seatPair : vehicle->Seats)
            if (seatPair.second.SeatInfo &&
                seatPair.second.SeatInfo->HasFlag(VEHICLE_SEAT_FLAG_CAN_CONTROL))
                if (Unit* passenger = vehicle->GetPassenger(seatPair.first))
                    if (Player* player = passenger->ToPlayer())
                        return player;

    return nullptr;
}

void LogVehicleLayout(Player const* player, Creature const* dragon, Vehicle const* vehicle)
{
    if (!player || !dragon || !vehicle)
        return;

    TC_LOG_INFO("scripts.g17.dragonriding",
        "G17R1 vehicle layout: player={}({}) dragon={} entry={} vehicleId={} seatCount={}",
        player->GetName(), player->GetGUID().ToString(), dragon->GetGUID().ToString(),
        dragon->GetEntry(), vehicle->GetVehicleInfo()->ID, vehicle->Seats.size());

    for (auto const& seatPair : vehicle->Seats)
    {
        VehicleSeatEntry const* seatInfo = seatPair.second.SeatInfo;
        TC_LOG_INFO("scripts.g17.dragonriding",
            "G17R1 vehicle seat: dragon={} index={} seatEntry={} flags=0x{:08X} flagsB=0x{:08X} canControl={} canEnterExit={}",
            dragon->GetGUID().ToString(), int32(seatPair.first), seatInfo ? seatInfo->ID : 0,
            seatInfo ? seatInfo->Flags : 0, seatInfo ? seatInfo->FlagsB : 0,
            seatInfo && seatInfo->HasFlag(VEHICLE_SEAT_FLAG_CAN_CONTROL),
            seatInfo && seatInfo->CanEnterOrExit());
    }
}

bool IsBlockedArea(Player const* player)
{
    if (!player || !player->IsInWorld() || !player->GetMap() || !player->IsOutdoors())
        return true;

    Map const* map = player->GetMap();
    if (map->IsDungeon() || map->IsBattlegroundOrArena())
        return true;

    AreaTableEntry const* areaEntry = sAreaTableStore.LookupEntry(player->GetAreaId());
    AreaTableEntry const* zoneEntry = sAreaTableStore.LookupEntry(player->GetZoneId());
    uint32 const flags = (areaEntry ? areaEntry->Flags : 0) | (zoneEntry ? zoneEntry->Flags : 0);
    return (flags & (AREA_FLAG_NO_FLY_ZONE | AREA_FLAG_INSIDE | AREA_FLAG_ARENA |
        AREA_FLAG_ARENA_INSTANCE | AREA_FLAG_SLAVE_CAPITAL | AREA_FLAG_SLAVE_CAPITAL2 |
        AREA_FLAG_CAPITAL | AREA_FLAG_CITY)) != 0;
}

void SendToRider(Creature* dragon, char const* message)
{
    if (Player* player = GetRider(dragon))
        if (WorldSession* session = player->GetSession())
            ChatHandler(session).SendSysMessage(message);
}

class NonVisualFallGuardEvent : public BasicEvent
{
public:
    NonVisualFallGuardEvent(Player* player, uint32 checksRemaining, bool sawFalling = false)
        : _player(player), _checksRemaining(checksRemaining), _sawFalling(sawFalling) { }

    bool Execute(uint64 /*execTime*/, uint32 /*diff*/) override
    {
        if (!_player || !_player->IsInWorld() || !_player->IsAlive() || !_checksRemaining)
            return true;

        bool const falling = _player->IsFalling();
        _sawFalling = _sawFalling || falling;

        // Reset only fall-damage accounting.  This adds no aura or model and
        // never changes gravity, falling speed, player control or trajectory.
        _player->SetFallInformation(0, _player->GetPositionZ());

        if ((_sawFalling && !falling) ||
            (!_sawFalling && _checksRemaining + FALL_GUARD_START_CHECKS <= FALL_GUARD_MAX_CHECKS))
            return true;

        _player->m_Events.AddEvent(
            new NonVisualFallGuardEvent(_player, _checksRemaining - 1, _sawFalling),
            _player->m_Events.CalculateTime(Milliseconds(FALL_GUARD_INTERVAL_MS)));
        return true;
    }

private:
    Player* _player;
    uint32 _checksRemaining;
    bool _sawFalling;
};

void NormalizeBlockedExitMovement(Player* player)
{
    if (!player)
        return;

    // G17 flight/gravity/speed belongs to the vehicle, never to the rider.
    // These checks are defense in depth against stale client-mover flags.
    if (player->CanFly())
        player->SetCanFly(false);
    if (player->IsGravityDisabled())
        player->SetDisableGravity(false);

    for (UnitMoveType moveType : { MOVE_RUN, MOVE_RUN_BACK, MOVE_SWIM,
        MOVE_SWIM_BACK, MOVE_FLIGHT, MOVE_FLIGHT_BACK })
        player->UpdateSpeed(moveType);

    player->m_Events.AddEvent(
        new NonVisualFallGuardEvent(player, FALL_GUARD_MAX_CHECKS),
        player->m_Events.CalculateTime(Milliseconds(FALL_GUARD_INTERVAL_MS)));
}

void CleanupPlayer(Player* player, bool protectFall, bool normalizeBlockedExit = false)
{
    Creature* dragon = GetDragon(player);
    if (!dragon)
        return;

    // Normalize every B2-owned mover rate before unboarding. No cleanup path
    // casts a parachute: alive airborne exits use only the non-visual fall guard.
    dragon->GetMotionMaster()->Clear(MOTION_SLOT_ACTIVE);
    dragon->RemoveAurasDueToSpell(SPELL_ACCELERATE);
    dragon->RemoveAurasDueToSpell(SPELL_SAFE_LANDING);
    for (UnitMoveType moveType : { MOVE_RUN, MOVE_RUN_BACK, MOVE_SWIM, MOVE_SWIM_BACK,
        MOVE_TURN_RATE, MOVE_FLIGHT, MOVE_FLIGHT_BACK, MOVE_PITCH_RATE })
        dragon->SetSpeedRate(moveType, 1.0f);
    dragon->RemoveUnitMovementFlag(MOVEMENTFLAG_WALKING | MOVEMENTFLAG_FALLING |
        MOVEMENTFLAG_FALLING_FAR | MOVEMENTFLAG_FALLING_SLOW | MOVEMENTFLAG_HOVER);
    dragon->SetAnimTier(AnimTier::Ground);
    dragon->SetCanFly(false);
    dragon->SetDisableGravity(false);

    player->ExitVehicle();
    if (normalizeBlockedExit)
        NormalizeBlockedExitMovement(player);
    else if (protectFall && player->IsAlive())
        player->m_Events.AddEvent(
            new NonVisualFallGuardEvent(player, FALL_GUARD_MAX_CHECKS),
            player->m_Events.CalculateTime(Milliseconds(FALL_GUARD_INTERVAL_MS)));
    dragon->DespawnOrUnsummon(500ms);
}

class VerifyBoardingEvent : public BasicEvent
{
public:
    VerifyBoardingEvent(Player* player, ObjectGuid dragonGuid, int8 seatId)
        : _player(player), _dragonGuid(dragonGuid), _seatId(seatId) { }

    bool Execute(uint64 /*execTime*/, uint32 /*diff*/) override
    {
        if (!_player || !_player->IsInWorld())
            return true;

        Creature* dragon = ObjectAccessor::GetCreature(*_player, _dragonGuid);
        Vehicle* vehicle = dragon ? dragon->GetVehicleKit() : nullptr;
        int8 const authoritativeSeat = GetAuthoritativePassengerSeatId(vehicle, _player);
        int8 const movementSeat = _player->GetTransSeat();
        bool const onExpectedVehicle = dragon && _player->GetVehicleBase() == dragon;
        bool const onExpectedSeat = onExpectedVehicle && authoritativeSeat == _seatId;
        bool const hasControl = onExpectedSeat && dragon->GetCharmerGUID() == _player->GetGUID();

        if (hasControl)
        {
            TC_LOG_INFO("scripts.g17.dragonriding",
                "G17B1R3 boarding verified: player={}({}) dragon={} vehicleId={} authoritativeSeat={} movementSeat={} controlled=true",
                _player->GetName(), _player->GetGUID().ToString(), dragon->GetGUID().ToString(),
                vehicle ? vehicle->GetVehicleInfo()->ID : 0, int32(authoritativeSeat), int32(movementSeat));
            if (WorldSession* session = _player->GetSession())
                ChatHandler(session).SendSysMessage(
                    "|cff80dfff[G17-B2R1-SAFE-ROLLBACK] 完整御空已就绪：1龙息、2动量加速、3向前爬升、4无降落伞特色着陆；速度250%-1200%。|r");
            return true;
        }

        TC_LOG_ERROR("scripts.g17.dragonriding",
            "G17B1R3 boarding verification failed: player={}({}) dragonFound={} expectedDragon={} actualVehicle={} expectedSeat={} authoritativeSeat={} movementSeat={} charmer={} map={} zone={} area={}",
            _player->GetName(), _player->GetGUID().ToString(), dragon != nullptr, _dragonGuid.ToString(),
            _player->GetVehicleBase() ? _player->GetVehicleBase()->GetGUID().ToString() : "none",
            int32(_seatId), int32(authoritativeSeat), int32(movementSeat),
            dragon ? dragon->GetCharmerGUID().ToString() : "none",
            _player->GetMapId(), _player->GetZoneId(), _player->GetAreaId());

        if (onExpectedVehicle)
            _player->ExitVehicle();
        if (dragon)
            dragon->DespawnOrUnsummon();
        if (WorldSession* session = _player->GetSession())
            ChatHandler(session).SendSysMessage(
                "|cffff4040[G17-B2R1-SAFE-ROLLBACK] 异步入座或控制权建立失败；worldserver已记录G17B1精确诊断。|r");
        return true;
    }

private:
    Player* _player;
    ObjectGuid _dragonGuid;
    int8 _seatId;
};

bool SpawnTypedVehicle(Player* player, uint32 sourceSpell, uint32 sourceCreature,
    uint32 sourceDisplay, uint32 archetype, ChatHandler* handler)
{
    if (!player || !player->IsAlive() || player->IsInCombat() || player->IsInFlight() ||
        player->GetVehicleBase() || IsBlockedArea(player))
        return false;

    Position spawn = player->GetPosition();
    spawn.m_positionZ += 1.5f;
    TempSummon* vehicleCreature = player->SummonCreature(NPC_DRAGONRIDING_VEHICLE, spawn,
        TEMPSUMMON_MANUAL_DESPAWN);
    if (!vehicleCreature)
        return false;

    vehicleCreature->SetCreatorGUID(player->GetGUID());
    vehicleCreature->SetFacingTo(player->GetOrientation());
    vehicleCreature->SetCanFly(true);
    vehicleCreature->SetDisableGravity(true);
    vehicleCreature->SetAnimTier(AnimTier::Fly);
    vehicleCreature->SetPowerType(POWER_ENERGY);
    vehicleCreature->SetMaxPower(POWER_ENERGY, 100);
    vehicleCreature->SetPower(POWER_ENERGY, 100);

    if (sourceDisplay)
    {
        vehicleCreature->SetDisplayId(sourceDisplay);
        vehicleCreature->SetNativeDisplayId(sourceDisplay);
    }

    Vehicle* vehicle = vehicleCreature->GetVehicleKit();
    if (!vehicle)
    {
        vehicleCreature->DespawnOrUnsummon();
        return false;
    }

    int8 const controlSeat = GetControllableSeatId(vehicle);
    if (controlSeat < 0)
    {
        vehicleCreature->DespawnOrUnsummon();
        return false;
    }

    if (vehicleCreature->IsAIEnabled())
    {
        vehicleCreature->AI()->SetData(DATA_SOURCE_SPELL, sourceSpell);
        vehicleCreature->AI()->SetData(DATA_SOURCE_CREATURE, sourceCreature);
        vehicleCreature->AI()->SetData(DATA_SOURCE_DISPLAY, sourceDisplay);
        vehicleCreature->AI()->SetData(DATA_ARCHETYPE, archetype);
    }

    if (sourceSpell)
    {
        // All prerequisites and the replacement vehicle are ready before the
        // original mount aura is removed.  This prevents failed conversions
        // from dismounting an otherwise valid normal mount.
        player->RemoveAurasDueToSpell(sourceSpell);
        if (player->IsMounted())
            player->RemoveAurasByType(SPELL_AURA_MOUNTED);
    }

    TC_LOG_INFO("scripts.g17.dragonriding",
        "G17B2 session start: player={}({}) vehicle={} sourceSpell={} sourceCreature={} display={} archetype={} seat={} auto={}",
        player->GetName(), player->GetGUID().ToString(), vehicleCreature->GetGUID().ToString(),
        sourceSpell, sourceCreature, sourceDisplay, ArchetypeName(archetype), int32(controlSeat), sourceSpell != 0);

    player->EnterVehicle(vehicleCreature, controlSeat);
    player->m_Events.AddEvent(new VerifyBoardingEvent(player, vehicleCreature->GetGUID(), controlSeat),
        player->m_Events.CalculateTime(250ms));

    if (handler)
        handler->SendSysMessage("|cff80dfff[G17-B2R1-SAFE-ROLLBACK] 正在建立全坐骑原生载具控制权……|r");
    return true;
}

class AutoConvertMountEvent : public BasicEvent
{
public:
    AutoConvertMountEvent(Player* player, uint32 spellId) : _player(player), _spellId(spellId) { }

    bool Execute(uint64 /*execTime*/, uint32 /*diff*/) override
    {
        if (!_player || !_player->IsInWorld() || !IsAutoEnabled(_player) || !_player->HasSpell(_spellId) ||
            GetDragon(_player) || _player->GetVehicleBase() || !_player->IsMounted())
            return true;

        // For wrapper mounts, _spellId is the learned outer spell while this
        // effect belongs to the triggered inner spell.  The active aura is the
        // runtime authority; ownership remains anchored to the outer spell.
        AuraEffect const* mountEffect = FindOwnedMountAura(_player);
        if (!mountEffect)
            return true;

        uint32 const creatureEntry = uint32(mountEffect->GetMiscValue());
        uint32 const displayId = _player->GetMountDisplayId();
        SpellInfo const* outerSpell = sSpellMgr->GetSpellInfo(_spellId);
        SpellInfo const* innerSpell = mountEffect->GetSpellInfo();
        uint32 const archetype = InferArchetype(creatureEntry, outerSpell, innerSpell);
        if (!creatureEntry || !displayId || IsBlockedArea(_player))
            return true;

        if (!SpawnTypedVehicle(_player, _spellId, creatureEntry, displayId, archetype, nullptr))
        {
            TC_LOG_ERROR("scripts.g17.dragonriding",
                "G17B2 automatic conversion failed: player={}({}) spell={} creature={} display={} archetype={}",
                _player->GetName(), _player->GetGUID().ToString(), _spellId, creatureEntry,
                displayId, ArchetypeName(archetype));
        }
        return true;
    }

private:
    Player* _player;
    uint32 _spellId;
};

SpellCastResult CheckEnergyCast(Unit* caster, uint32 cost)
{
    if (!IsDragon(caster))
        return SPELL_CAST_OK;

    Creature* dragon = caster->ToCreature();
    if (!dragon || !GetRider(dragon))
        return SPELL_FAILED_CASTER_DEAD;

    if (dragon->GetPower(POWER_ENERGY) < cost)
        return SPELL_FAILED_NO_POWER;

    return SPELL_CAST_OK;
}
}

struct npc_g17_dragonriding_vehicle : public VehicleAI
{
    npc_g17_dragonriding_vehicle(Creature* creature) : VehicleAI(creature) { }

    void Reset() override
    {
        _landing = false;
        _landingApproach = false;
        _climbing = false;
        _stalling = false;
        _safetyCleanupStarted = false;
        _safetyCheckTimer = G17Dragonriding::SAFETY_CHECK_INTERVAL_MS;
        _motionSampleAccumulator = 0;
        _speedPacketTimer = 0;
        _climbControlTimer = 0;
        _boostTimer = 0;
        _orphanGraceTimer = 3000;
        _momentum = 0.10f;
        _currentSpeedRate = G17Dragonriding::FLIGHT_SPEED_RATES.front();
        _targetSpeedRate = _currentSpeedRate;
        _speedTier = 0;
        _notifiedSpeedTier = 0;
        _lastZ = me->GetPositionZ();
        me->SetReactState(REACT_PASSIVE);
        RestoreClientFlightControl(false);
        ApplyMovementRates(_currentSpeedRate, true);
        me->SetPowerType(POWER_ENERGY);
        me->SetMaxPower(POWER_ENERGY, 100);
        me->SetPower(POWER_ENERGY, 100);
    }

    void SetData(uint32 id, uint32 value) override
    {
        switch (id)
        {
            case G17Dragonriding::DATA_SOURCE_SPELL: _sourceSpell = value; break;
            case G17Dragonriding::DATA_SOURCE_CREATURE: _sourceCreature = value; break;
            case G17Dragonriding::DATA_SOURCE_DISPLAY: _sourceDisplay = value; break;
            case G17Dragonriding::DATA_ARCHETYPE: _archetype = value; break;
            default: break;
        }
    }

    uint32 GetData(uint32 id) const override
    {
        switch (id)
        {
            case G17Dragonriding::DATA_SOURCE_SPELL: return _sourceSpell;
            case G17Dragonriding::DATA_SOURCE_CREATURE: return _sourceCreature;
            case G17Dragonriding::DATA_SOURCE_DISPLAY: return _sourceDisplay;
            case G17Dragonriding::DATA_ARCHETYPE: return _archetype;
            case G17Dragonriding::DATA_MOMENTUM_PERCENT: return uint32(std::round(_momentum * 100.0f));
            case G17Dragonriding::DATA_SPEED_PERCENT: return uint32(std::round(_currentSpeedRate * 100.0f));
            case G17Dragonriding::DATA_FLIGHT_STATE:
                if (_landing) return G17Dragonriding::FLIGHT_STATE_LANDING;
                if (_climbing) return G17Dragonriding::FLIGHT_STATE_CLIMBING;
                if (_stalling) return G17Dragonriding::FLIGHT_STATE_STALLING;
                return G17Dragonriding::FLIGHT_STATE_NORMAL;
            default: return 0;
        }
    }

    void PassengerBoarded(Unit* passenger, int8 seatId, bool apply) override
    {
        if (!passenger || passenger->GetTypeId() != TYPEID_PLAYER)
            return;

        Vehicle* vehicle = me->GetVehicleKit();
        if (!vehicle)
        {
            TC_LOG_ERROR("scripts.g17.dragonriding",
                "G17R1 PassengerBoarded has no VehicleKit: dragon={} passenger={} seat={} apply={}",
                me->GetGUID().ToString(), passenger->GetGUID().ToString(), int32(seatId), apply);
            return;
        }

        auto const seat = vehicle->Seats.find(seatId);
        if (seat == vehicle->Seats.end() || !seat->second.SeatInfo ||
            !seat->second.SeatInfo->HasFlag(VEHICLE_SEAT_FLAG_CAN_CONTROL))
        {
            TC_LOG_ERROR("scripts.g17.dragonriding",
                "G17R1 PassengerBoarded ignored non-control seat: dragon={} passenger={} seat={} apply={}",
                me->GetGUID().ToString(), passenger->GetGUID().ToString(), int32(seatId), apply);
            return;
        }

        Player* player = passenger->ToPlayer();
        TC_LOG_INFO("scripts.g17.dragonriding",
            "G17R1 PassengerBoarded: player={}({}) dragon={} seat={} apply={}",
            player->GetName(), player->GetGUID().ToString(), me->GetGUID().ToString(), int32(seatId), apply);
        if (apply)
        {
            _landing = false;
            _landingApproach = false;
            _climbing = false;
            _stalling = false;
            _safetyCleanupStarted = false;
            _safetyCheckTimer = G17Dragonriding::SAFETY_CHECK_INTERVAL_MS;
            _motionSampleAccumulator = 0;
            _speedPacketTimer = 0;
            _climbControlTimer = 0;
            _boostTimer = 0;
            _orphanGraceTimer = 0;
            _momentum = 0.10f;
            _currentSpeedRate = G17Dragonriding::FLIGHT_SPEED_RATES.front();
            _targetSpeedRate = _currentSpeedRate;
            _speedTier = 0;
            _notifiedSpeedTier = 0;
            _lastZ = me->GetPositionZ();
            RestoreClientFlightControl(true);
            ApplyMovementRates(_currentSpeedRate, true);
            me->SetPowerType(POWER_ENERGY);
            me->SetMaxPower(POWER_ENERGY, 100);
            me->SetPower(POWER_ENERGY, 100);
            return;
        }

        NormalizeVehicleForExit();
        me->DespawnOrUnsummon(500ms);
    }

    void UpdateAI(uint32 diff) override
    {
        if (_safetyCleanupStarted)
            return;

        if (_climbing)
        {
            if (_climbControlTimer > diff)
                _climbControlTimer -= diff;
            else
                CompleteClimb();
        }

        if (!_landing && !_climbing)
            UpdateContinuousFlight(diff);

        if (_safetyCheckTimer > diff)
        {
            _safetyCheckTimer -= diff;
            return;
        }
        _safetyCheckTimer = G17Dragonriding::SAFETY_CHECK_INTERVAL_MS;

        Player* player = G17Dragonriding::GetRider(me);
        if (!player)
        {
            if (_orphanGraceTimer > G17Dragonriding::SAFETY_CHECK_INTERVAL_MS)
            {
                _orphanGraceTimer -= G17Dragonriding::SAFETY_CHECK_INTERVAL_MS;
                return;
            }

            _safetyCleanupStarted = true;
            NormalizeVehicleForExit();
            me->DespawnOrUnsummon(500ms);
            return;
        }

        if (!player->IsAlive() || !player->IsInWorld() || player->GetMap() != me->GetMap())
        {
            _safetyCleanupStarted = true;
            G17Dragonriding::CleanupPlayer(player, false, false);
            return;
        }

        if (!G17Dragonriding::IsBlockedArea(player))
            return;

        // PlayerScript::OnUpdateZone is not called when only VMap outdoor state
        // or the sub-area changes inside the same zone. The vehicle therefore
        // retains the proven B1R4 250 ms server-authoritative indoor policy.
        _safetyCleanupStarted = true;
        TC_LOG_INFO("scripts.g17.dragonriding",
            "G17B2 continuous safety cleanup: player={}({}) dragon={} outdoors={} map={} zone={} area={}",
            player->GetName(), player->GetGUID().ToString(), me->GetGUID().ToString(),
            player->IsOutdoors(), player->GetMapId(), player->GetZoneId(), player->GetAreaId());
        if (WorldSession* session = player->GetSession())
            ChatHandler(session).SendSysMessage(
                "|cffff8040[G17-B2R1-SAFE-ROLLBACK] 已进入禁飞/城市/室内/副本区域，御空状态已无降落伞归一化清理。|r");
        G17Dragonriding::CleanupPlayer(player, false, true);
    }

    void DoAction(int32 action) override
    {
        using namespace G17Dragonriding;

        if (!GetRider(me))
            return;

        if (action == ACTION_ACCELERATE)
        {
            if (_landing)
                return;

            _boostTimer = BOOST_DURATION_MS;
            _momentum = std::min(MAX_MOMENTUM, _momentum + 0.28f);
            me->SendPlaySpellVisualKit(VISUAL_KIT_BOOST, 0);
            if (_stalling)
                RecoverFromStall();
            SendToRider(me, "|cff80dfff[G17-B2R1-SAFE-ROLLBACK] 动量推进已启动：平滑加速、最高封顶1200%，重复施放不会叠加上限。|r");
            return;
        }

        if (action == ACTION_CLIMB)
        {
            if (_landing || _climbing)
                return;

            StartForwardClimb();
            return;
        }

        if (action == ACTION_LAND && !_landing)
            StartTypedLanding();
    }

    void MovementInform(uint32 type, uint32 pointId) override
    {
        using namespace G17Dragonriding;

        if (type != EFFECT_MOTION_TYPE)
            return;

        if (pointId == POINT_CLIMB && _climbing)
        {
            CompleteClimb();
            return;
        }

        if (pointId == POINT_LAND_APPROACH && _landing && _landingApproach)
        {
            _landingApproach = false;
            StartBeastPounce();
            return;
        }

        if (pointId == POINT_LAND && _landing)
            CompleteLanding();
    }

    void JustDied(Unit* /*killer*/) override
    {
        NormalizeVehicleForExit();
        if (Player* player = G17Dragonriding::GetRider(me))
            player->ExitVehicle();
    }

private:
    bool HasControlFlag(uint32 movementFlag) const
    {
        if (me->HasUnitMovementFlag(movementFlag))
            return true;
        if (Player* player = G17Dragonriding::GetRider(me))
            return player->HasUnitMovementFlag(movementFlag);
        return false;
    }

    void ApplyMovementRates(float flightRate, bool force)
    {
        flightRate = std::clamp(flightRate, 1.0f, G17Dragonriding::FLIGHT_SPEED_RATES.back());
        if (!force && std::abs(flightRate - _lastAppliedSpeedRate) < 0.02f)
            return;

        float const backwardRate = std::min(flightRate, 4.0f);
        me->SetSpeedRate(MOVE_RUN, flightRate);
        me->SetSpeedRate(MOVE_RUN_BACK, backwardRate);
        me->SetSpeedRate(MOVE_SWIM, flightRate);
        me->SetSpeedRate(MOVE_SWIM_BACK, backwardRate);
        me->SetSpeedRate(MOVE_FLIGHT, flightRate);
        me->SetSpeedRate(MOVE_FLIGHT_BACK, backwardRate);
        me->SetSpeedRate(MOVE_TURN_RATE, flightRate > 8.0f ? 1.25f : 1.0f);
        me->SetSpeedRate(MOVE_PITCH_RATE, flightRate > 8.0f ? 1.20f : 1.0f);
        _lastAppliedSpeedRate = flightRate;
    }

    void RestoreClientFlightControl(bool clearServerMotion)
    {
        if (clearServerMotion)
            me->GetMotionMaster()->Clear(MOTION_SLOT_ACTIVE);

        me->RemoveUnitMovementFlag(MOVEMENTFLAG_WALKING | MOVEMENTFLAG_FALLING |
            MOVEMENTFLAG_FALLING_FAR | MOVEMENTFLAG_FALLING_SLOW | MOVEMENTFLAG_HOVER);
        me->SetCanFly(true);
        me->SetDisableGravity(true);
        me->SetAnimTier(AnimTier::Fly);
    }

    void NormalizeVehicleForExit()
    {
        _landing = false;
        _landingApproach = false;
        _climbing = false;
        _stalling = false;
        _boostTimer = 0;
        _climbControlTimer = 0;
        _momentum = 0.0f;
        _currentSpeedRate = 1.0f;
        _targetSpeedRate = 1.0f;
        me->GetMotionMaster()->Clear(MOTION_SLOT_ACTIVE);
        me->RemoveAurasDueToSpell(G17Dragonriding::SPELL_ACCELERATE);
        me->RemoveAurasDueToSpell(G17Dragonriding::SPELL_SAFE_LANDING);
        ApplyMovementRates(1.0f, true);
        me->RemoveUnitMovementFlag(MOVEMENTFLAG_WALKING | MOVEMENTFLAG_FALLING |
            MOVEMENTFLAG_FALLING_FAR | MOVEMENTFLAG_FALLING_SLOW | MOVEMENTFLAG_HOVER);
        me->SetAnimTier(AnimTier::Ground);
        me->SetCanFly(false);
        me->SetDisableGravity(false);
    }

    void UpdateContinuousFlight(uint32 diff)
    {
        using namespace G17Dragonriding;

        if (_boostTimer > diff)
            _boostTimer -= diff;
        else
            _boostTimer = 0;

        _motionSampleAccumulator += diff;
        _speedPacketTimer += diff;
        if (_motionSampleAccumulator < G17Dragonriding::MOTION_SAMPLE_INTERVAL_MS)
            return;

        float const dt = std::min(_motionSampleAccumulator, uint32(500)) / 1000.0f;
        _motionSampleAccumulator = 0;

        bool const forward = HasControlFlag(MOVEMENTFLAG_FORWARD);
        bool const backward = HasControlFlag(MOVEMENTFLAG_BACKWARD);
        bool const ascendingFlag = HasControlFlag(MOVEMENTFLAG_ASCENDING) ||
            HasControlFlag(MOVEMENTFLAG_PITCH_UP);
        bool const descendingFlag = HasControlFlag(MOVEMENTFLAG_DESCENDING) ||
            HasControlFlag(MOVEMENTFLAG_PITCH_DOWN);
        bool const turning = HasControlFlag(MOVEMENTFLAG_LEFT) ||
            HasControlFlag(MOVEMENTFLAG_RIGHT) || HasControlFlag(MOVEMENTFLAG_STRAFE_LEFT) ||
            HasControlFlag(MOVEMENTFLAG_STRAFE_RIGHT);

        float const currentZ = me->GetPositionZ();
        float const deltaZ = currentZ - _lastZ;
        _lastZ = currentZ;
        bool const ascending = ascendingFlag || deltaZ > 0.08f;
        bool const descending = descendingFlag || deltaZ < -0.08f;
        bool const idle = !forward && !backward && !ascending && !descending;

        float momentumDelta = 0.0f;
        if (forward)
            momentumDelta += 0.20f;
        if (descending)
            momentumDelta += 0.34f;
        if (ascending)
            momentumDelta -= 0.38f;
        if (backward)
            momentumDelta -= 0.75f;
        if (turning)
            momentumDelta -= 0.10f;
        if (idle)
            momentumDelta -= 0.18f;
        if (_boostTimer)
            momentumDelta += 0.52f;

        _momentum = std::clamp(_momentum + momentumDelta * dt, MIN_MOMENTUM, MAX_MOMENTUM);

        float const floorZ = me->GetFloorZ();
        float const altitude = floorZ > INVALID_HEIGHT + 1.0f ? currentZ - floorZ : 100.0f;
        if (!_stalling && _momentum <= STALL_ENTER_MOMENTUM && altitude > 4.0f && !_boostTimer)
            EnterStall();
        else if (_stalling && (_momentum >= STALL_RECOVER_MOMENTUM || _boostTimer))
            RecoverFromStall();
        else if (_stalling && altitude <= 2.0f)
            RecoverFromStall();

        UpdateSpeedTier();
        float const acceleration = _targetSpeedRate > _currentSpeedRate ? 2.2f : 3.5f;
        float const maxStep = acceleration * dt;
        if (_currentSpeedRate < _targetSpeedRate)
            _currentSpeedRate = std::min(_targetSpeedRate, _currentSpeedRate + maxStep);
        else
            _currentSpeedRate = std::max(_targetSpeedRate, _currentSpeedRate - maxStep);
        _currentSpeedRate = std::clamp(_currentSpeedRate, 1.0f, FLIGHT_SPEED_RATES.back());

        if (_speedPacketTimer >= SPEED_PACKET_INTERVAL_MS)
        {
            _speedPacketTimer = 0;
            ApplyMovementRates(_currentSpeedRate, false);
        }
    }

    void UpdateSpeedTier()
    {
        using namespace G17Dragonriding;

        while (_speedTier + 1 < FLIGHT_SPEED_RATES.size() &&
            _momentum >= MOMENTUM_THRESHOLDS[_speedTier + 1] + 0.01f)
            ++_speedTier;
        while (_speedTier > 0 && _momentum < MOMENTUM_THRESHOLDS[_speedTier] - 0.02f)
            --_speedTier;

        _targetSpeedRate = _stalling ? FLIGHT_SPEED_RATES.front() : FLIGHT_SPEED_RATES[_speedTier];
        if (_notifiedSpeedTier == _speedTier)
            return;

        _notifiedSpeedTier = _speedTier;
        if (Player* player = GetRider(me))
            if (WorldSession* session = player->GetSession())
                ChatHandler(session).PSendSysMessage(
                    "|cff80dfff[G17-B2R1-SAFE-ROLLBACK] 动量档位 %u/7：目标飞行速度 %u%%（当前动量%u%%）。|r",
                    uint32(_speedTier + 1), uint32(std::round(_targetSpeedRate * 100.0f)),
                    uint32(std::round(_momentum * 100.0f)));
    }

    void EnterStall()
    {
        _stalling = true;
        me->SetCanFly(true);
        me->SetDisableGravity(false, false);
        me->SetAnimTier(AnimTier::Fly);
        G17Dragonriding::SendToRider(me,
            "|cffffb040[G17-B2R1-SAFE-ROLLBACK] 低动量失速：载具开始下坠；向前、俯冲或使用技能2即可恢复。|r");
    }

    void RecoverFromStall()
    {
        if (!_stalling)
            return;

        _stalling = false;
        _momentum = std::max(_momentum, 0.14f);
        RestoreClientFlightControl(false);
        ApplyMovementRates(_currentSpeedRate, true);
        G17Dragonriding::SendToRider(me,
            "|cff80ff80[G17-B2R1-SAFE-ROLLBACK] 已从失速恢复：上升、下降、转向和前进控制全部恢复。|r");
    }

    void StartForwardClimb()
    {
        using namespace G17Dragonriding;

        if (_stalling)
            RecoverFromStall();

        float const orientation = me->GetOrientation();
        Position destination(
            me->GetPositionX() + std::cos(orientation) * CLIMB_FORWARD_DISTANCE,
            me->GetPositionY() + std::sin(orientation) * CLIMB_FORWARD_DISTANCE,
            me->GetPositionZ() + CLIMB_HEIGHT,
            orientation);

        // A collision-clamped point may shorten forward travel, but the dot
        // product guard never permits a backward destination.
        Position const collision = me->GetFirstCollisionPosition(CLIMB_FORWARD_DISTANCE, 0.0f);
        float const collisionDx = collision.GetPositionX() - me->GetPositionX();
        float const collisionDy = collision.GetPositionY() - me->GetPositionY();
        float const forwardDot = collisionDx * std::cos(orientation) + collisionDy * std::sin(orientation);
        if (forwardDot <= 1.0f)
        {
            SendToRider(me, "|cffffb040[G17-B2R1-SAFE-ROLLBACK] 技能3前方被阻挡：为避免穿墙已取消本次短爬升，方向不会反转。|r");
            RestoreClientFlightControl(true);
            return;
        }
        destination.m_positionX = collision.GetPositionX();
        destination.m_positionY = collision.GetPositionY();

        _climbing = true;
        _climbControlTimer = CLIMB_CONTROL_TIMEOUT_MS;
        _momentum = std::max(0.0f, _momentum - 0.18f);
        RestoreClientFlightControl(true);
        me->GetMotionMaster()->MoveJump(destination, CLIMB_HORIZONTAL_SPEED,
            CLIMB_VERTICAL_SPEED, POINT_CLIMB, true);
        SendToRider(me, "|cff80dfff[G17-B2R1-SAFE-ROLLBACK] 技能3：沿当前朝向向前并向上爬升；短样条结束后立即交还完整控制。|r");
    }

    void CompleteClimb()
    {
        if (!_climbing)
            return;

        _climbing = false;
        _climbControlTimer = 0;
        _momentum = std::max(_momentum, 0.22f);
        _lastZ = me->GetPositionZ();
        RestoreClientFlightControl(true);
        ApplyMovementRates(_currentSpeedRate, true);
        G17Dragonriding::SendToRider(me,
            "|cff80ff80[G17-B2R1-SAFE-ROLLBACK] 爬升完成：飞行姿态、重力、速度及全部客户端控制已显式恢复。|r");
    }

    void StartTypedLanding()
    {
        using namespace G17Dragonriding;

        float const groundZ = me->GetFloorZ();
        if (groundZ <= INVALID_HEIGHT + 1.0f || groundZ > me->GetPositionZ() + 5.0f)
        {
            SendToRider(me, "|cffff4040[G17-B2R1-SAFE-ROLLBACK] 当前下方没有可验证的安全地面，已取消着陆。|r");
            return;
        }

        _landing = true;
        _landingApproach = false;
        _climbing = false;
        _stalling = false;
        _boostTimer = 0;
        _momentum = 0.0f;
        _targetSpeedRate = 1.0f;
        _currentSpeedRate = 1.0f;
        me->GetMotionMaster()->Clear(MOTION_SLOT_ACTIVE);
        me->RemoveAurasDueToSpell(SPELL_ACCELERATE);
        me->RemoveAurasDueToSpell(SPELL_SAFE_LANDING);
        RestoreClientFlightControl(false);
        ApplyMovementRates(1.0f, true);

        Position destination(me->GetPositionX(), me->GetPositionY(), groundZ + 0.5f, me->GetOrientation());
        switch (_archetype)
        {
            case ARCHETYPE_MAGIC:
                me->SendPlaySpellVisualKit(VISUAL_KIT_MAGIC_WIND, 0);
                SendToRider(me, "|cffc080ff[G17-B2R1-SAFE-ROLLBACK] 魔法/风系着陆：旋风托举并平滑卸除高度。|r");
                me->GetMotionMaster()->MoveLand(POINT_LAND, destination, 10.0f);
                break;
            case ARCHETYPE_MECHANICAL:
                me->SendPlaySpellVisualKit(VISUAL_KIT_MECHANICAL_ROCKET, 0);
                SendToRider(me, "|cffffa040[G17-B2R1-SAFE-ROLLBACK] 机械火箭着陆：反推火焰高速制动，地面自动断推。|r");
                me->GetMotionMaster()->MoveLand(POINT_LAND, destination, 28.0f);
                break;
            case ARCHETYPE_BEAST:
                SendToRider(me, "|cffffd080[G17-B2R1-SAFE-ROLLBACK] 野兽跃落：先接近地面，再向前扑跃安全落地。|r");
                if (me->GetPositionZ() - groundZ > 10.0f)
                {
                    _landingApproach = true;
                    Position approach(me->GetPositionX(), me->GetPositionY(), groundZ + 8.0f, me->GetOrientation());
                    me->GetMotionMaster()->MoveLand(POINT_LAND_APPROACH, approach, 24.0f);
                }
                else
                    StartBeastPounce();
                break;
            case ARCHETYPE_DRAGON:
                SendToRider(me, "|cff80dfff[G17-B2R1-SAFE-ROLLBACK] 龙类收翼着陆：受控俯降并以翼压稳定触地。|r");
                me->GetMotionMaster()->MoveLand(POINT_LAND, destination, 18.0f);
                break;
            default:
                SendToRider(me, "|cff80dfff[G17-B2R1-SAFE-ROLLBACK] 通用受控着陆：验证地面后平滑下降。|r");
                me->GetMotionMaster()->MoveLand(POINT_LAND, destination, 15.0f);
                break;
        }
    }

    void StartBeastPounce()
    {
        using namespace G17Dragonriding;

        float const orientation = me->GetOrientation();
        float targetX = me->GetPositionX() + std::cos(orientation) * 6.0f;
        float targetY = me->GetPositionY() + std::sin(orientation) * 6.0f;
        float targetZ = me->GetPositionZ();
        me->UpdateGroundPositionZ(targetX, targetY, targetZ);
        if (targetZ <= INVALID_HEIGHT + 1.0f || targetZ > me->GetPositionZ() + 5.0f)
        {
            targetX = me->GetPositionX();
            targetY = me->GetPositionY();
            targetZ = me->GetFloorZ();
        }

        me->SendPlaySpellVisualKit(VISUAL_KIT_BEAST_POUNCE, 0);
        Position destination(targetX, targetY, targetZ + 0.5f, orientation);
        me->GetMotionMaster()->MoveJump(destination, 14.0f, 8.0f, POINT_LAND, true);
    }

    void CompleteLanding()
    {
        using namespace G17Dragonriding;

        me->SendPlaySpellVisualKit(VISUAL_KIT_LANDING_DUST, 0);
        Player* player = GetRider(me);
        NormalizeVehicleForExit();
        if (player)
            player->ExitVehicle();
        me->DespawnOrUnsummon(500ms);
    }

    bool _landing = false;
    bool _landingApproach = false;
    bool _climbing = false;
    bool _stalling = false;
    bool _safetyCleanupStarted = false;
    uint32 _safetyCheckTimer = G17Dragonriding::SAFETY_CHECK_INTERVAL_MS;
    uint32 _motionSampleAccumulator = 0;
    uint32 _speedPacketTimer = 0;
    uint32 _climbControlTimer = 0;
    uint32 _boostTimer = 0;
    uint32 _orphanGraceTimer = 3000;
    float _momentum = 0.10f;
    float _currentSpeedRate = G17Dragonriding::FLIGHT_SPEED_RATES.front();
    float _targetSpeedRate = G17Dragonriding::FLIGHT_SPEED_RATES.front();
    float _lastAppliedSpeedRate = 0.0f;
    float _lastZ = 0.0f;
    size_t _speedTier = 0;
    size_t _notifiedSpeedTier = 0;
    uint32 _sourceSpell = 0;
    uint32 _sourceCreature = 0;
    uint32 _sourceDisplay = 0;
    uint32 _archetype = G17Dragonriding::ARCHETYPE_DRAGON;
};

class spell_g17_dragon_breath_energy : public SpellScript
{
    PrepareSpellScript(spell_g17_dragon_breath_energy);

    SpellCastResult CheckCast()
    {
        return G17Dragonriding::CheckEnergyCast(GetCaster(), G17Dragonriding::BREATH_ENERGY_COST);
    }

    void ConsumeEnergy()
    {
        if (G17Dragonriding::IsDragon(GetCaster()))
            GetCaster()->ModifyPower(POWER_ENERGY, -int32(G17Dragonriding::BREATH_ENERGY_COST));
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_g17_dragon_breath_energy::CheckCast);
        AfterCast += SpellCastFn(spell_g17_dragon_breath_energy::ConsumeEnergy);
    }
};

class spell_g17_dragon_accelerate_energy : public SpellScript
{
    PrepareSpellScript(spell_g17_dragon_accelerate_energy);

    SpellCastResult CheckCast()
    {
        SpellCastResult const result = G17Dragonriding::CheckEnergyCast(
            GetCaster(), G17Dragonriding::BOOST_ENERGY_COST);
        if (result != SPELL_CAST_OK)
            return result;

        if (Creature* dragon = GetCaster()->ToCreature())
            if (dragon->IsAIEnabled() &&
                dragon->AI()->GetData(G17Dragonriding::DATA_FLIGHT_STATE) == G17Dragonriding::FLIGHT_STATE_LANDING)
                return SPELL_FAILED_NOT_READY;
        return SPELL_CAST_OK;
    }

    void HandleBoost(SpellEffIndex effectIndex)
    {
        if (!G17Dragonriding::IsDragon(GetCaster()))
            return;

        // Suppress Aura 210 so DBC speed cannot multiply B2 above 1200%.
        PreventHitDefaultEffect(effectIndex);
        if (Creature* dragon = GetCaster()->ToCreature())
            if (dragon->IsAIEnabled())
                dragon->AI()->DoAction(G17Dragonriding::ACTION_ACCELERATE);
    }

    void ConsumeEnergy()
    {
        if (G17Dragonriding::IsDragon(GetCaster()))
            GetCaster()->ModifyPower(POWER_ENERGY, -int32(G17Dragonriding::BOOST_ENERGY_COST));
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_g17_dragon_accelerate_energy::CheckCast);
        OnEffectHit += SpellEffectFn(spell_g17_dragon_accelerate_energy::HandleBoost, EFFECT_0, SPELL_EFFECT_ANY);
        AfterCast += SpellCastFn(spell_g17_dragon_accelerate_energy::ConsumeEnergy);
    }
};

class spell_g17_dragon_climb : public SpellScript
{
    PrepareSpellScript(spell_g17_dragon_climb);

    SpellCastResult CheckCast()
    {
        SpellCastResult const result = G17Dragonriding::CheckEnergyCast(
            GetCaster(), G17Dragonriding::CLIMB_ENERGY_COST);
        if (result != SPELL_CAST_OK)
            return result;

        if (Creature* dragon = GetCaster()->ToCreature())
            if (dragon->IsAIEnabled())
            {
                uint32 const state = dragon->AI()->GetData(G17Dragonriding::DATA_FLIGHT_STATE);
                if (state == G17Dragonriding::FLIGHT_STATE_CLIMBING ||
                    state == G17Dragonriding::FLIGHT_STATE_LANDING)
                    return SPELL_FAILED_NOT_READY;
            }
        return SPELL_CAST_OK;
    }

    void ConsumeEnergy()
    {
        if (Creature* dragon = GetCaster()->ToCreature())
            if (G17Dragonriding::IsDragon(dragon) && dragon->IsAIEnabled() &&
                dragon->AI()->GetData(G17Dragonriding::DATA_FLIGHT_STATE) == G17Dragonriding::FLIGHT_STATE_CLIMBING)
                dragon->ModifyPower(POWER_ENERGY, -int32(G17Dragonriding::CLIMB_ENERGY_COST));
    }

    void HandleJump(SpellEffIndex effectIndex)
    {
        if (!G17Dragonriding::IsDragon(GetCaster()))
            return;

        PreventHitDefaultEffect(effectIndex);
        if (Creature* dragon = GetCaster()->ToCreature())
            if (dragon->IsAIEnabled())
                dragon->AI()->DoAction(G17Dragonriding::ACTION_CLIMB);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_g17_dragon_climb::CheckCast);
        OnEffectHit += SpellEffectFn(spell_g17_dragon_climb::HandleJump, EFFECT_0, SPELL_EFFECT_ANY);
        AfterCast += SpellCastFn(spell_g17_dragon_climb::ConsumeEnergy);
    }
};

class spell_g17_dragon_safe_landing : public SpellScript
{
    PrepareSpellScript(spell_g17_dragon_safe_landing);

    void StartLanding(SpellEffIndex effectIndex)
    {
        if (!G17Dragonriding::IsDragon(GetCaster()))
            return;

        // The safety-floor action-bar command is visual-free and has no aura.
        PreventHitDefaultEffect(effectIndex);
        if (Creature* dragon = GetCaster()->ToCreature())
            if (dragon->IsAIEnabled())
                dragon->AI()->DoAction(G17Dragonriding::ACTION_LAND);
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_g17_dragon_safe_landing::StartLanding,
            EFFECT_0, SPELL_EFFECT_ANY);
    }
};

class g17_dragonriding_playerscript : public PlayerScript
{
public:
    g17_dragonriding_playerscript() : PlayerScript("g17_dragonriding_playerscript") { }

    void OnSpellCast(Player* player, Spell* spell, bool /*skipCheck*/) override
    {
        if (!player || !spell || !G17Dragonriding::IsAutoEnabled(player) || player->IsMounted() ||
            G17Dragonriding::GetDragon(player) || player->GetVehicleBase())
            return;

        SpellInfo const* info = spell->GetSpellInfo();
        if (!info || !G17Dragonriding::HasMountAuraMetadata(info) || !player->HasSpell(info->Id))
            return;

        player->m_Events.AddEvent(new G17Dragonriding::AutoConvertMountEvent(player, info->Id),
            player->m_Events.CalculateTime(100ms));
    }

    void OnPVPKill(Player* /*killer*/, Player* killed) override
    {
        G17Dragonriding::CleanupPlayer(killed, true);
    }

    void OnPlayerKilledByCreature(Creature* /*killer*/, Player* killed) override
    {
        G17Dragonriding::CleanupPlayer(killed, true);
    }

    void OnLogout(Player* player) override
    {
        G17Dragonriding::CleanupPlayer(player, true);
        G17Dragonriding::ForgetAutoPreference(player);
    }

    void OnPlayerRepop(Player* player) override
    {
        G17Dragonriding::CleanupPlayer(player, true);
    }

    void OnMapChanged(Player* player) override
    {
        G17Dragonriding::CleanupPlayer(player, true);
    }

    void OnUpdateZone(Player* player, uint32 /*newZone*/, uint32 /*newArea*/) override
    {
        if (G17Dragonriding::GetDragon(player) && G17Dragonriding::IsBlockedArea(player))
        {
            if (WorldSession* session = player->GetSession())
                ChatHandler(session).SendSysMessage("|cffff8040[G17-B2R1-SAFE-ROLLBACK] 已进入禁飞/城市/室内/副本区域，御龙载具已安全清理。|r");
            G17Dragonriding::CleanupPlayer(player, false, true);
        }
    }
};

class dragonriding_commandscript : public CommandScript
{
public:
    dragonriding_commandscript() : CommandScript("dragonriding_commandscript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable dragonCommandTable =
        {
            { "summon",  HandleSummon,  rbac::RBAC_PERM_COMMAND_HELP, Console::No },
            { "mount",   HandleMount,   rbac::RBAC_PERM_COMMAND_HELP, Console::No },
            { "auto",    HandleAuto,    rbac::RBAC_PERM_COMMAND_HELP, Console::No },
            { "dismiss", HandleDismiss, rbac::RBAC_PERM_COMMAND_HELP, Console::No },
            { "status",  HandleStatus,  rbac::RBAC_PERM_COMMAND_HELP, Console::No },
            { "help",    HandleHelp,    rbac::RBAC_PERM_COMMAND_HELP, Console::No },
        };

        static ChatCommandTable commandTable =
        {
            { "dragon", dragonCommandTable },
        };
        return commandTable;
    }

    static bool HandleSummon(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        if (player->IsMounted())
        {
            handler->SendSysMessage("|cffff4040[G17-B2R1-SAFE-ROLLBACK] 已在普通坐骑上；请直接重新点击已学坐骑让自动接管工作。|r");
            return true;
        }

        if (!G17Dragonriding::SpawnTypedVehicle(player, 0, 0, 0,
            G17Dragonriding::ARCHETYPE_DRAGON, handler))
            handler->SendSysMessage("|cffff4040[G17-B2R1-SAFE-ROLLBACK] 当前状态或地点不允许建立御龙载具。|r");
        return true;
    }

    static bool HandleMount(ChatHandler* handler, uint32 spellId)
    {
        Player* player = handler->GetPlayer();
        SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
        if (!player || !info || !G17Dragonriding::HasMountAuraMetadata(info) || !player->HasSpell(spellId))
        {
            handler->SendSysMessage("|cffff4040[G17-B2R1-SAFE-ROLLBACK] 必须提供自己已经学会的坐骑法术ID（支持包装法术）。|r");
            return true;
        }

        G17Dragonriding::SetAutoEnabled(player, true);
        player->CastSpell(player, spellId, false);
        handler->PSendSysMessage("|cff80dfff[G17-B2R1-SAFE-ROLLBACK] 已施放拥有的坐骑法术%u；成功上马后会自动接管。|r", spellId);
        return true;
    }

    static bool HandleAuto(ChatHandler* handler, std::string mode)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        if (mode == "on")
            G17Dragonriding::SetAutoEnabled(player, true);
        else if (mode == "off")
            G17Dragonriding::SetAutoEnabled(player, false);
        else
        {
            handler->PSendSysMessage("|cffffff00[G17-B2R1-SAFE-ROLLBACK] auto=%s；用法：.dragon auto on|off|r",
                G17Dragonriding::IsAutoEnabled(player) ? "on" : "off");
            return true;
        }

        handler->PSendSysMessage("|cff80dfff[G17-B2R1-SAFE-ROLLBACK] 全坐骑自动接管已%s。|r",
            G17Dragonriding::IsAutoEnabled(player) ? "开启" : "关闭");
        return true;
    }

    static bool HandleDismiss(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (!G17Dragonriding::GetDragon(player))
        {
            handler->SendSysMessage("|cffffff00[G17-B2R1-SAFE-ROLLBACK] 当前没有御龙载具。|r");
            return true;
        }

        G17Dragonriding::CleanupPlayer(player, true);
        handler->SendSysMessage("|cff80dfff[G17-B2R1-SAFE-ROLLBACK] 御龙载具已安全解除。|r");
        return true;
    }

    static bool HandleStatus(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (Creature* dragon = G17Dragonriding::GetDragon(player))
        {
            Vehicle* vehicle = dragon->GetVehicleKit();
            uint32 const sourceSpell = dragon->IsAIEnabled() ? dragon->AI()->GetData(G17Dragonriding::DATA_SOURCE_SPELL) : 0;
            uint32 const sourceCreature = dragon->IsAIEnabled() ? dragon->AI()->GetData(G17Dragonriding::DATA_SOURCE_CREATURE) : 0;
            uint32 const sourceDisplay = dragon->IsAIEnabled() ? dragon->AI()->GetData(G17Dragonriding::DATA_SOURCE_DISPLAY) : 0;
            uint32 const archetype = dragon->IsAIEnabled() ? dragon->AI()->GetData(G17Dragonriding::DATA_ARCHETYPE) : 0;
            uint32 const momentum = dragon->IsAIEnabled() ? dragon->AI()->GetData(G17Dragonriding::DATA_MOMENTUM_PERCENT) : 0;
            uint32 const speed = dragon->IsAIEnabled() ? dragon->AI()->GetData(G17Dragonriding::DATA_SPEED_PERCENT) : 0;
            uint32 const flightState = dragon->IsAIEnabled() ? dragon->AI()->GetData(G17Dragonriding::DATA_FLIGHT_STATE) : 0;
            handler->PSendSysMessage("|cff80dfff[G17-B2R1-SAFE-ROLLBACK] ACTIVE sourceSpell=%u sourceCreature=%u display=%u type=%s vehicle=%u seat=%d movementSeat=%d controlled=%s energy=%u/%u momentum=%u%% speed=%u%% state=%u auto=%s map=%u zone=%u area=%u|r",
                sourceSpell, sourceCreature, sourceDisplay, G17Dragonriding::ArchetypeName(archetype),
                vehicle ? vehicle->GetVehicleInfo()->ID : 0,
                int32(G17Dragonriding::GetAuthoritativePassengerSeatId(vehicle, player)), int32(player->GetTransSeat()),
                dragon->GetCharmerGUID() == player->GetGUID() ? "true" : "false",
                dragon->GetPower(POWER_ENERGY), dragon->GetMaxPower(POWER_ENERGY), momentum, speed,
                flightState, G17Dragonriding::IsAutoEnabled(player) ? "on" : "off",
                player->GetMapId(), player->GetZoneId(), player->GetAreaId());
        }
        else
            handler->PSendSysMessage("|cffffff00[G17-B2R1-SAFE-ROLLBACK] INACTIVE auto=%s area_allowed=%s|r",
                G17Dragonriding::IsAutoEnabled(player) ? "on" : "off",
                G17Dragonriding::IsBlockedArea(player) ? "false" : "true");
        return true;
    }

    static bool HandleHelp(ChatHandler* handler)
    {
        handler->SendSysMessage("|cff80dfff===== G17-B2 完整御空术 =====|r");
        handler->SendSysMessage("默认auto=on：直接点击已学坐骑；直接法术和包装/内层坐骑都会保留外观并自动接管。");
        handler->SendSysMessage("七档动量速度250%-1200%：前进和俯冲蓄势，拉升、转向、后退制动和停滞消耗动量；速度平滑变化且不能无限叠加。");
        handler->SendSysMessage("低动量会进入可恢复失速；向前、俯冲或技能2恢复。技能3只短暂接管并沿朝向向前上方爬升，结束显式恢复控制。");
        handler->SendSysMessage("技能4绝不添加降落伞；魔法/风、野兽跃落、机械火箭、龙类收翼和通用类型使用不同着陆运动与表现。");
        handler->SendSysMessage(".dragon auto on|off；.dragon mount <spellId>；.dragon summon/dismiss/status");
        handler->SendSysMessage("室内、禁区、死亡、离车、换图和异常中断均归一化Vehicle、重力、飞行姿态与全部临时速度。|r");
        return true;
    }};

void AddSC_dragonriding_commandscript()
{
    new dragonriding_commandscript();
    new g17_dragonriding_playerscript();
    RegisterCreatureAI(npc_g17_dragonriding_vehicle);
    RegisterSpellScript(spell_g17_dragon_breath_energy);
    RegisterSpellScript(spell_g17_dragon_accelerate_energy);
    RegisterSpellScript(spell_g17_dragon_climb);
    RegisterSpellScript(spell_g17_dragon_safe_landing);
}
