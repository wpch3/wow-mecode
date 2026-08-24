/*
 * G17-B1R5: generic wrapper-mount conversion and no-parachute blocked-area cleanup.
 *
 * B1R5 preserves the proven B1R3 seat chain and B1R4 250 ms indoor policy.
 * Mount candidates are recognized from retained Mounted-aura metadata even
 * when SpellMgr intentionally disables a wrapper's direct aura effects; the
 * active inner Mounted aura remains the authority for creature and display.
 * Blocked-area exit adds no parachute aura and explicitly normalizes movement.
 * B2 momentum is intentionally not claimed here.
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
    DATA_ARCHETYPE = 17013
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

uint32 InferArchetype(uint32 creatureEntry)
{
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
constexpr uint32 SPELL_ACCELERATE     = 55215; // Burst of Speed: +100% flight speed for 10 sec
constexpr uint32 SPELL_CLIMB          = 52197; // Jump Jets: intercepted only for this vehicle
constexpr uint32 SPELL_SAFE_LANDING   = 53208; // Parachute: also starts controlled landing
constexpr uint32 SPELL_FALL_SAFETY    = 53208;

constexpr int32 ACTION_CLIMB = 1;
constexpr int32 ACTION_LAND  = 2;
constexpr uint32 POINT_CLIMB = 17001;
constexpr uint32 POINT_LAND  = 17002;

constexpr uint32 BREATH_ENERGY_COST = 20;
constexpr uint32 BOOST_ENERGY_COST  = 30;
constexpr uint32 CLIMB_ENERGY_COST  = 25;
constexpr float CLIMB_HEIGHT       = 12.0f;
constexpr uint32 SAFETY_CHECK_INTERVAL_MS = 250;
constexpr uint32 FALL_GUARD_INTERVAL_MS = 100;
constexpr uint32 FALL_GUARD_MAX_CHECKS = 200;
constexpr uint32 FALL_GUARD_START_CHECKS = 10;

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

void CleanupPlayer(Player* player, bool addFallSafety, bool normalizeBlockedExit = false)
{
    Creature* dragon = GetDragon(player);
    if (!dragon)
        return;

    if (addFallSafety && player->IsAlive())
        player->CastSpell(player, SPELL_FALL_SAFETY, true);

    // Normalize the vehicle before unboarding so its temporary flight rate,
    // can-fly flag and disabled gravity cannot survive delayed despawn paths.
    dragon->SetSpeedRate(MOVE_FLIGHT, 1.0f);
    dragon->SetCanFly(false);
    dragon->SetDisableGravity(false);

    player->ExitVehicle();
    if (normalizeBlockedExit)
        NormalizeBlockedExitMovement(player);
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
                    "|cff80dfff[G17-B1] 全坐骑会话已就绪：保留原坐骑外观；1龙息、2加速、3爬升、4安全着陆。|r");
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
                "|cffff4040[G17-B1] 异步入座或控制权建立失败；worldserver已记录G17B1精确诊断。|r");
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
        "G17B1 session start: player={}({}) vehicle={} sourceSpell={} sourceCreature={} display={} archetype={} seat={} auto={}",
        player->GetName(), player->GetGUID().ToString(), vehicleCreature->GetGUID().ToString(),
        sourceSpell, sourceCreature, sourceDisplay, ArchetypeName(archetype), int32(controlSeat), sourceSpell != 0);

    player->EnterVehicle(vehicleCreature, controlSeat);
    player->m_Events.AddEvent(new VerifyBoardingEvent(player, vehicleCreature->GetGUID(), controlSeat),
        player->m_Events.CalculateTime(250ms));

    if (handler)
        handler->SendSysMessage("|cff80dfff[G17-B1] 正在建立全坐骑原生载具控制权……|r");
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
        uint32 const archetype = InferArchetype(creatureEntry);
        if (!creatureEntry || !displayId || IsBlockedArea(_player))
            return true;

        if (!SpawnTypedVehicle(_player, _spellId, creatureEntry, displayId, archetype, nullptr))
        {
            TC_LOG_ERROR("scripts.g17.dragonriding",
                "G17B1 automatic conversion failed: player={}({}) spell={} creature={} display={} archetype={}",
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
        _safetyCleanupStarted = false;
        _safetyCheckTimer = G17Dragonriding::SAFETY_CHECK_INTERVAL_MS;
        me->SetReactState(REACT_PASSIVE);
        me->SetCanFly(true);
        me->SetDisableGravity(true);
        me->SetSpeedRate(MOVE_FLIGHT, 1.5f);
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
            _safetyCleanupStarted = false;
            _safetyCheckTimer = G17Dragonriding::SAFETY_CHECK_INTERVAL_MS;
            me->SetCanFly(true);
            me->SetDisableGravity(true);
            me->SetPowerType(POWER_ENERGY);
            me->SetMaxPower(POWER_ENERGY, 100);
            me->SetPower(POWER_ENERGY, 100);
            return;
        }

        // Do not add an implicit parachute here.  Callers that explicitly
        // require legacy fall safety add it before ExitVehicle; blocked-area
        // cleanup uses the non-visual fall-accounting guard instead.
        me->SetSpeedRate(MOVE_FLIGHT, 1.0f);
        me->SetCanFly(false);
        me->SetDisableGravity(false);
        me->DespawnOrUnsummon(500ms);
    }

    void UpdateAI(uint32 diff) override
    {
        if (_safetyCleanupStarted)
            return;

        if (_safetyCheckTimer > diff)
        {
            _safetyCheckTimer -= diff;
            return;
        }

        _safetyCheckTimer = G17Dragonriding::SAFETY_CHECK_INTERVAL_MS;
        Player* player = G17Dragonriding::GetRider(me);
        if (!player || !G17Dragonriding::IsBlockedArea(player))
            return;

        // PlayerScript::OnUpdateZone is not called when only VMap outdoor state
        // or the sub-area changes inside the same zone. The active vehicle must
        // therefore enforce the same server-authoritative policy continuously.
        _safetyCleanupStarted = true;
        TC_LOG_INFO("scripts.g17.dragonriding",
            "G17B1R4 continuous safety cleanup: player={}({}) dragon={} outdoors={} map={} zone={} area={}",
            player->GetName(), player->GetGUID().ToString(), me->GetGUID().ToString(),
            player->IsOutdoors(), player->GetMapId(), player->GetZoneId(), player->GetAreaId());
        if (WorldSession* session = player->GetSession())
            ChatHandler(session).SendSysMessage(
                "|cffff8040[G17-B1] 已进入禁飞/城市/室内/副本区域，御龙载具已安全清理。|r");
        G17Dragonriding::CleanupPlayer(player, false, true);
    }

    void DoAction(int32 action) override
    {
        using namespace G17Dragonriding;

        if (!GetRider(me))
            return;

        if (action == ACTION_CLIMB)
        {
            if (_landing)
                return;

            Position destination = me->GetPosition();
            destination.m_positionZ += CLIMB_HEIGHT;
            me->SetCanFly(true);
            me->SetDisableGravity(true);
            me->GetMotionMaster()->MoveTakeoff(POINT_CLIMB, destination);
            return;
        }

        if (action != ACTION_LAND || _landing)
            return;

        float const groundZ = me->GetFloorZ();
        if (groundZ <= INVALID_HEIGHT + 1.0f || groundZ > me->GetPositionZ() + 5.0f)
        {
            SendToRider(me, "|cffff4040[G17-B1] 当前下方没有可验证的安全地面，已取消着陆。|r");
            return;
        }

        _landing = true;
        SendToRider(me, "|cff80dfff[G17-B1] 正在安全着陆；到地面后会自动离车。|r");
        Position destination(me->GetPositionX(), me->GetPositionY(), groundZ + 0.5f, me->GetOrientation());
        me->GetMotionMaster()->MoveLand(POINT_LAND, destination);
    }

    void MovementInform(uint32 type, uint32 pointId) override
    {
        using namespace G17Dragonriding;

        if (type != EFFECT_MOTION_TYPE)
            return;

        if (pointId == POINT_CLIMB)
        {
            me->SetCanFly(true);
            me->SetDisableGravity(true);
            return;
        }

        if (pointId != POINT_LAND || !_landing)
            return;

        if (Player* player = GetRider(me))
        {
            player->CastSpell(player, SPELL_FALL_SAFETY, true);
            player->ExitVehicle();
        }

        me->SetCanFly(false);
        me->SetDisableGravity(false);
        me->DespawnOrUnsummon(1s);
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (Player* player = G17Dragonriding::GetRider(me))
        {
            player->CastSpell(player, G17Dragonriding::SPELL_FALL_SAFETY, true);
            player->ExitVehicle();
        }
    }

private:
    bool _landing = false;
    bool _safetyCleanupStarted = false;
    uint32 _safetyCheckTimer = G17Dragonriding::SAFETY_CHECK_INTERVAL_MS;
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
        return G17Dragonriding::CheckEnergyCast(GetCaster(), G17Dragonriding::BOOST_ENERGY_COST);
    }

    void ConsumeEnergy()
    {
        if (G17Dragonriding::IsDragon(GetCaster()))
            GetCaster()->ModifyPower(POWER_ENERGY, -int32(G17Dragonriding::BOOST_ENERGY_COST));
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_g17_dragon_accelerate_energy::CheckCast);
        AfterCast += SpellCastFn(spell_g17_dragon_accelerate_energy::ConsumeEnergy);
    }
};

class spell_g17_dragon_climb : public SpellScript
{
    PrepareSpellScript(spell_g17_dragon_climb);

    SpellCastResult CheckCast()
    {
        return G17Dragonriding::CheckEnergyCast(GetCaster(), G17Dragonriding::CLIMB_ENERGY_COST);
    }

    void ConsumeEnergy()
    {
        if (G17Dragonriding::IsDragon(GetCaster()))
            GetCaster()->ModifyPower(POWER_ENERGY, -int32(G17Dragonriding::CLIMB_ENERGY_COST));
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

    void StartLanding()
    {
        if (Creature* dragon = GetCaster()->ToCreature())
            if (G17Dragonriding::IsDragon(dragon) && dragon->IsAIEnabled())
                dragon->AI()->DoAction(G17Dragonriding::ACTION_LAND);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_g17_dragon_safe_landing::StartLanding);
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
                ChatHandler(session).SendSysMessage("|cffff8040[G17-B1] 已进入禁飞/城市/室内/副本区域，御龙载具已安全清理。|r");
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
            handler->SendSysMessage("|cffff4040[G17-B1] 已在普通坐骑上；请直接重新点击已学坐骑让自动接管工作。|r");
            return true;
        }

        if (!G17Dragonriding::SpawnTypedVehicle(player, 0, 0, 0,
            G17Dragonriding::ARCHETYPE_DRAGON, handler))
            handler->SendSysMessage("|cffff4040[G17-B1] 当前状态或地点不允许建立御龙载具。|r");
        return true;
    }

    static bool HandleMount(ChatHandler* handler, uint32 spellId)
    {
        Player* player = handler->GetPlayer();
        SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
        if (!player || !info || !G17Dragonriding::HasMountAuraMetadata(info) || !player->HasSpell(spellId))
        {
            handler->SendSysMessage("|cffff4040[G17-B1] 必须提供自己已经学会的坐骑法术ID（支持包装法术）。|r");
            return true;
        }

        G17Dragonriding::SetAutoEnabled(player, true);
        player->CastSpell(player, spellId, false);
        handler->PSendSysMessage("|cff80dfff[G17-B1] 已施放拥有的坐骑法术%u；成功上马后会自动接管。|r", spellId);
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
            handler->PSendSysMessage("|cffffff00[G17-B1] auto=%s；用法：.dragon auto on|off|r",
                G17Dragonriding::IsAutoEnabled(player) ? "on" : "off");
            return true;
        }

        handler->PSendSysMessage("|cff80dfff[G17-B1] 全坐骑自动接管已%s。|r",
            G17Dragonriding::IsAutoEnabled(player) ? "开启" : "关闭");
        return true;
    }

    static bool HandleDismiss(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (!G17Dragonriding::GetDragon(player))
        {
            handler->SendSysMessage("|cffffff00[G17-B1] 当前没有御龙载具。|r");
            return true;
        }

        G17Dragonriding::CleanupPlayer(player, true);
        handler->SendSysMessage("|cff80dfff[G17-B1] 御龙载具已安全解除。|r");
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
            handler->PSendSysMessage("|cff80dfff[G17-B1] ACTIVE sourceSpell=%u sourceCreature=%u display=%u type=%s vehicle=%u seat=%d movementSeat=%d controlled=%s energy=%u/%u auto=%s map=%u zone=%u area=%u|r",
                sourceSpell, sourceCreature, sourceDisplay, G17Dragonriding::ArchetypeName(archetype),
                vehicle ? vehicle->GetVehicleInfo()->ID : 0,
                int32(G17Dragonriding::GetAuthoritativePassengerSeatId(vehicle, player)), int32(player->GetTransSeat()),
                dragon->GetCharmerGUID() == player->GetGUID() ? "true" : "false",
                dragon->GetPower(POWER_ENERGY), dragon->GetMaxPower(POWER_ENERGY),
                G17Dragonriding::IsAutoEnabled(player) ? "on" : "off",
                player->GetMapId(), player->GetZoneId(), player->GetAreaId());
        }
        else
            handler->PSendSysMessage("|cffffff00[G17-B1] INACTIVE auto=%s area_allowed=%s|r",
                G17Dragonriding::IsAutoEnabled(player) ? "on" : "off",
                G17Dragonriding::IsBlockedArea(player) ? "false" : "true");
        return true;
    }

    static bool HandleHelp(ChatHandler* handler)
    {
        handler->SendSysMessage("|cff80dfff===== G17-B1 全坐骑自动接管 =====|r");
        handler->SendSysMessage("默认auto=on：直接点击自己已学会的普通坐骑按钮，成功上马后自动转成同模型可控载具。");
        handler->SendSysMessage("地面、飞行、龙类、猛兽、机械和魔法坐骑共用安全接管；无法分类时进入GENERIC而不是拒绝。");
        handler->SendSysMessage(".dragon auto on|off  开关自动接管");
        handler->SendSysMessage(".dragon mount <spellId>  施放自己已拥有的直接/包装坐骑法术并接管");
        handler->SendSysMessage(".dragon summon/dismiss/status  通用测试龙、解除、会话诊断");
        handler->SendSysMessage("技能条仍为R1低速验证页；5档动量、1200%极速和独立战斗页属于下一阶段B2/B3。|r");
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
