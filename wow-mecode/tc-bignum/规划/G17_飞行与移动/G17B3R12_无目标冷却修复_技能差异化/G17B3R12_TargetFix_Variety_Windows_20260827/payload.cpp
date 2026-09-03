/*
 * G17-B3R12: no-target hidden-cooldown fix + skill variety #1/#2.
 *   - combat-skill CheckCast now pre-validates the target (full resolution
 *     chain, triggered casts exempt): a targetless press fails CLEANLY with
 *     SPELL_FAILED_BAD_TARGETS instead of executing, printing the chat hint
 *     and silently consuming the core GCD (user-reported: "not ready" with
 *     no UI swirl = unusable).
 *   - new 990029 突袭·俯冲打击 (combat slot 7): first AREA skill, AoE burst
 *     around the target; 990030 御风姿态 (movement slot 7): stance toggle
 *     (+15% turn, doubled regen).  Both DUMMY carriers like the others.
 *   - slot 7 is now page-pure (was the crossed generator/dive fillers the
 *     user reported as 换错了).
 *
 * G17-B3R11 heritage (land-mount flight animation fix, user report: 陆地坐骑飞行时
 * 双脚蹬个不停).  Land-mount models (BEAST/GENERIC) lack Fly-tier anims so the
 * client plays their run cycle in flight; a server EMOTE STATE (stand pose)
 * overrides the movement animation - legs freeze while airborne and restore
 * on the ground/exit.  Flying-model archetypes are untouched.
 *
 * G17-B3R10 heritage (eight filled slots per page, switch pinned at slot 8):
 *
 * User-verified with the C11 client mod: the stock bar now renders 8 native
 * buttons (7 filled by B3R8 + slot 8 empty).  B3-R10 fills ALL eight on both
 * pages and pins the page switch at slot 8 (fixed position, user directive):
 *   movement: 拉升 俯冲 推进 冲刺 着陆 | 制动@6 生成器@7 | 切页@8
 *   combat:   技0..技4                     | 制动@6 俯冲@7 | 切页@8
 * Plus instant chat feedback on every page switch (and a mount hint) so a
 * failed switch is visible immediately instead of silently nothing.
 *
 * G17-B3R9 heritage (full player-cast support):
 *
 * The custom client UI (G17DragonRide addon) casts every dragonriding skill
 * as the PLAYER through secure spell buttons (CastSpellByID).  The combat
 * carriers and the four G17 carriers already accept player casters; the
 * three REAL movement skills did not.  B3-R9:
 *   - spell_g17_dragon_accelerate_energy / _climb / _safe_landing now use
 *     ResolveDragonFromCaster (dual caster), so player casts trigger the
 *     same AI actions (boost/dash/landing) and drain dragon energy.
 *   - CheckEnergyCast resolves the dragon from either caster, so the energy
 *     gate applies to the player path too (no free spam).
 *   - 55215/52197 get SPELL_ATTR0_CASTABLE_WHILE_MOUNTED at runtime (the
 *     proven B2R3 const_cast pattern); 52226 already has it.
 *
 * G17-B3R8 heritage (slot-layout safety fix + vehicle cooldown UI packets):
 *
 * B3-R7's layout put the page switch at m_spells[6] (slot 7).  Live testing
 * showed the 3.3.5 client only fills/renders 6 vehicle bonus slots, so the
 * page-switch button vanished from the stock bar.  B3-R8 keeps the page
 * switch at slot 6 on BOTH pages (always visible) and moves the brake to
 * slot 7 (G17DragonBar v6 shows it natively when the client fills slot 7,
 * or via its secure spell-cast fallback otherwise).
 *
 * On top of B3R7 (f2360d7e) / B3R6 (3fdb46e8):
 *   - both pages now use 7 m_spells slots (brake 990028 is back on the bar;
 *     page switch stays last).  The stock client VehicleMenuBar still renders
 *     the first 6; the G17DragonBar addon (G17-C10) renders all bonus-bar
 *     slots, so the 7th shows there.  m_spells[7] stays 0 (MAX_CREATURE_SPELLS=8).
 *   - after every successful combat skill (and the page switch) the rider's
 *     client receives SMSG_SPELL_COOLDOWN twice (dragon GUID + player GUID
 *     variants, built with SpellHistory::BuildCooldownPacket) so the vehicle
 *     bar buttons finally render the cooldown swirl.  Root cause: with DBC
 *     RecoveryTime=0 the client has no local prediction, and vehicle
 *     creature casters never hit the player-session packet path in
 *     SpellHistory::StartCooldown.
 *
 * G17-B2R2 heritage (runtime experience final fix for boost, dash and landing):
 *
 * B2R2 preserves B1R3 authoritative seating, B1R4 indoor enforcement, B1R5
 * wrapper-mount resolution and B2's bounded 250%-1200% momentum model. It is
 * the follow-up to B2R1 and fixes three real runtime defects:
 *   - Skill 2 (boost) now has richer layered feedback (launch burst + charge
 *     trail, a slower mid-boost gust, a top-speed impact ring and a shutdown
 *     burst) instead of a single thin streamer.
 *   - Skill 3 (dash/climb) no longer launches along a stale smoothed travel
 *     heading. The whole 7-node path is anchored to the rider's facing at cast
 *     time, so the dash is a straight forward surge in the direction the player
 *     is actually looking. This removes the fixed-direction curve / U-turn.
 *   - Skill 4 (landing) uses the real quest-item command 52226 "飞行器着陆" and
 *     explicitly allows the cast from the G17 dragon, suppressing the native
 *     dummy effect and starting landing from both OnEffectHit and AfterCast so
 *     it works for every mount type.
 * Landing profiles stay type-gated: flat magic/wind glide, long dragon slope,
 * mechanical reverse-thrust approach, and fire-free beast pounce.
 */

#include "ScriptMgr.h"
#include "Chat.h"
#include "CombatAI.h"
#include "DBCStores.h"
#include "EventProcessor.h"
#include "GridDefines.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "G3D/Vector3.h"
#include "Log.h"
#include "Map.h"
#include "MotionMaster.h"
#include "MoveSplineInit.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "RBAC.h"
#include "ScriptedCreature.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellInfo.h"
#include "SpellHistory.h"
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
#include <vector>

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
    DATA_FLIGHT_STATE = 17016,
    DATA_BOOST_ACTIVE = 17017
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

constexpr uint32 SPELL_DRAGON_BREATH = 9573;   // Flame Breath.
constexpr uint32 SPELL_ACCELERATE     = 55215;  // Aura is suppressed; G17 owns the hard cap.
constexpr uint32 SPELL_CLIMB          = 52197;  // Jump Jets, replaced by a bounded smooth arc.
constexpr uint32 SPELL_SAFE_LANDING   = 52226;  // Land Flying Machine: dummy, no aura or visual.

// ---- B3-R1 combat (client carriers 990000-990024, defined by G17B3_skill_table) ----
constexpr uint32 COMBAT_SPELL_BASE   = 990000;
constexpr uint32 COMBAT_SPELL_COUNT  = 25;
constexpr uint32 COMBAT_ENERGY_COST  = 12;
constexpr uint32 COMBAT_STUN_MS      = 2000;
// per-slot base damage (slot 2 = defense/heal)
constexpr uint32 COMBAT_BASE_DAMAGE[5] = { 70, 50, 0, 35, 180 };
constexpr uint32 COMBAT_CD_MS[5]       = { 4000, 6000, 20000, 10000, 60000 };
constexpr uint32 COMBAT_BURST_ENERGY   = 30;
constexpr float  COMBAT_HEAL_PCT       = 0.18f;
constexpr float  COMBAT_BURST_MULT     = 2.6f;
constexpr float  COMBAT_PLAYER_FACTOR  = 0.45f;

// ---- B3-R2 multi-page vehicle skill bar ----
// The vehicle action bar is Creature::m_spells[0..7] (MAX_CREATURE_SPELLS = 8;
// the two remaining packet slots stay empty) and is (re)delivered to the rider
// with the public Player::VehicleSpellInitialize() -> SMSG_PET_SPELLS.  Two
// pages: movement (pure flight skills, no attack) and per-archetype combat;
// switching swaps m_spells and re-sends the bar.  Energy/cooldowns live on
// the vehicle, so a page switch never resets resources (design doc 5.1/5.2).
constexpr uint32 SPELL_PAGE_SWITCH  = 990025; // 切换技能页 (both pages, last button)
constexpr uint32 SPELL_ASCEND       = 990026; // 拉升: powered steep climb
constexpr uint32 SPELL_DIVE         = 990027; // 俯冲: powered dive, restores energy
constexpr uint32 SPELL_GLIDE_BRAKE  = 990028; // 滑翔/制动: air brake
constexpr uint32 COMBAT_PAGE_SLOTS  = 5;      // per-archetype combat skills
constexpr float  COMBAT_MAX_RANGE  = 40.0f;  // attack legality: max distance
// B3-R4: damage scales with the rider's level so the skills stay relevant at
// endgame (user report: level 80 in full gear saw only tens of damage).
constexpr uint32 COMBAT_DAMAGE_PER_LEVEL = 15;  // slots 0-3 bonus per level
constexpr uint32 COMBAT_BURST_PER_LEVEL  = 45;  // finisher bonus per level
// B3-R5: the mount fights alongside the rider with REAL casts (user report:
// auto damage happened but no visible spellcasting).  Every tick the dragon
// triggered-casts the archetype GENERATOR (slot 0: free, +8 energy, half
// damage, rider-attributed) at the rider's target - the client renders the
// full cast with the carrier's SpellVisual from the C6 client patch.
constexpr uint32 AUTOCOMBAT_INTERVAL_MS = 4500;
constexpr uint32 AUTOCOMBAT_RANGE       = 40;
constexpr int32  ACTION_ASCEND      = 4;
constexpr int32  ACTION_DIVE        = 5;
constexpr int32  ACTION_GLIDE_BRAKE = 6;
constexpr int32  ACTION_PAGE_SWITCH = 7;
constexpr int32  ACTION_WIND_STANCE = 8;   // B3-R12 御风姿态 toggle

// ---- B3-R12 skill variety #1/#2 (client carriers 990029/990030) ----
constexpr uint32 SPELL_SWOOP_STRIKE     = 990029; // 突袭·俯冲打击 (combat slot 7: first AoE)
constexpr uint32 SPELL_WIND_STANCE      = 990030; // 御风姿态 (movement slot 7: stance toggle)
constexpr uint32 SWOOP_CD_MS            = 12000;
constexpr uint32 SWOOP_ENERGY           = 15;
constexpr float  SWOOP_RADIUS           = 8.0f;
constexpr uint32 SWOOP_BASE_DAMAGE      = 60;
constexpr uint32 SWOOP_DMG_PER_LEVEL    = 12;
constexpr uint32 WIND_STANCE_CD_MS      = 1000;   // toggle anti-spam
constexpr float  WIND_STANCE_TURN_BONUS = 1.15f;  // +15% turn rate while active
constexpr uint32 DATA_SKILL_PAGE    = 17018;
constexpr uint32 ASCEND_ENERGY_COST  = 20;
constexpr int32  DIVE_ENERGY_GAIN    = 15;    // diving is the recovery loop
constexpr float ASCEND_FORWARD_DISTANCE = 14.0f;
constexpr float ASCEND_HEIGHT          = 16.0f;
constexpr float ASCEND_SPLINE_SPEED    = 22.0f;
constexpr float DIVE_FORWARD_DISTANCE  = 20.0f;
constexpr float DIVE_DEPTH             = 14.0f;
constexpr float DIVE_MIN_ALTITUDE      = 3.0f;   // never dive into the ground
constexpr float DIVE_SPLINE_SPEED      = 26.0f;
constexpr float DIVE_MOMENTUM_GAIN     = 0.30f;
constexpr float GLIDE_BRAKE_MOMENTUM   = 0.30f;
constexpr uint32 COMBAT_PAGE_SPEED_TIER_CAP = 2; // combat page caps speed at 600%
constexpr uint32 PASSIVE_ENERGY_REGEN_MS    = 2000; // +1 energy / 2s in flight
constexpr int32  GENERATOR_ENERGY_GAIN      = 8;    // combat slot-0 generator
constexpr uint32 PAGE_SWITCH_CD_MS          = 1000; // page switch internal cd

constexpr int32 ACTION_CLIMB = 1;
constexpr int32 ACTION_LAND = 2;
constexpr int32 ACTION_ACCELERATE = 3;
constexpr uint32 POINT_CLIMB = 17001;
constexpr uint32 POINT_LAND = 17002;
constexpr uint32 POINT_LAND_APPROACH = 17003;

constexpr uint32 BREATH_ENERGY_COST = 20;
constexpr uint32 BOOST_ENERGY_COST = 30;
constexpr uint32 CLIMB_ENERGY_COST = 25;
constexpr float CLIMB_FORWARD_DISTANCE = 20.0f;
constexpr float CLIMB_HEIGHT = 8.0f;
constexpr float CLIMB_SPLINE_SPEED_MIN = 18.0f;
constexpr float CLIMB_SPLINE_SPEED_MAX = 32.0f;
constexpr float CLIMB_MAX_YAW_DELTA = 0.70f; // 40 degrees per cast, never a hairpin.
constexpr uint32 CLIMB_CONTROL_TIMEOUT_MS = 2600;
constexpr uint32 BOOST_DURATION_MS = 4000;
constexpr uint32 BOOST_TRAIL_INTERVAL_MS = 550;
constexpr uint32 BOOST_GUST_INTERVAL_MS = 1100;
constexpr uint32 LANDING_TIMEOUT_MS = 25000;  // B3-R2d: tighter failsafe (was 45s; landings are now ~2x faster)
constexpr uint32 SAFETY_CHECK_INTERVAL_MS = 250;
constexpr uint32 MOTION_SAMPLE_INTERVAL_MS = 100;
constexpr uint32 SPEED_PACKET_INTERVAL_MS = 200;
constexpr uint32 FALL_GUARD_INTERVAL_MS = 100;
constexpr uint32 FALL_GUARD_MAX_CHECKS = 200;
constexpr uint32 FALL_GUARD_START_CHECKS = 10;

// Every B2R2 visual kit is a client-side one-shot with no aura and no damage.
// Only SpellVisualKit IDs already audited against the project's real 3.3.5a
// zhCN SpellVisualKit.dbc (see the B2R1 visual-kit audit evidence) are used.
// "Richer" feedback comes from layering and timing these known-good kits, not
// from inventing new IDs that might not exist in the client.  The mechanical
// jet flame stays type-gated to mechanical/rocket archetypes only.
constexpr uint32 VISUAL_KIT_BOOST_LAUNCH      = 44;    // ChargeTrail + dust + launch sound.
constexpr uint32 VISUAL_KIT_SPEED_TRAIL       = 696;   // Generic RibbonTrail sustained-speed pulse.
constexpr uint32 VISUAL_KIT_WIND_BURST        = 13709; // One-shot Wind Shear impact + sound.
constexpr uint32 VISUAL_KIT_MECHANICAL_THRUST = 13481; // Mechanical-only jet-pack flame attach.
constexpr uint32 VISUAL_KIT_LANDING_DUST      = 1066;  // Fire-free contact dust.
// B2R2 reuses the audited kits for denser layered feedback:
constexpr uint32 VISUAL_KIT_BURST_EXTRA       = 44;    // Second ChargeTrail burst (double launch).
constexpr uint32 VISUAL_KIT_TRAIL_PULSE       = 696;   // Faster ribbon-trail pulse.
constexpr uint32 VISUAL_KIT_IMPACT_RING       = 13709; // Wind-shear impact ring (top speed / end).

constexpr std::array<float, 7> FLIGHT_SPEED_RATES = { 2.5f, 4.0f, 6.0f, 8.0f, 10.0f, 11.0f, 12.0f };
constexpr std::array<float, 7> MOMENTUM_THRESHOLDS = { 0.0f, 0.18f, 0.34f, 0.50f, 0.66f, 0.82f, 0.94f };
constexpr float MIN_MOMENTUM = 0.0f;
constexpr float MAX_MOMENTUM = 1.0f;
constexpr float STALL_ENTER_MOMENTUM = 0.06f;
constexpr float STALL_RECOVER_MOMENTUM = 0.20f;

constexpr float PI = 3.14159265358979323846f;

float NormalizeRadians(float angle)
{
    while (angle > PI)
        angle -= 2.0f * PI;
    while (angle < -PI)
        angle += 2.0f * PI;
    return angle;
}

float SmoothStep(float value)
{
    value = std::clamp(value, 0.0f, 1.0f);
    return value * value * (3.0f - 2.0f * value);
}

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

// B3-R3: resolve the G17 dragon from EITHER caster type.  Vehicle-bar
// buttons are cast BY THE DRAGON (creature caster); skill-panel casts come
// from the SEATED RIDER (player caster).  Both paths must reach the same
// vehicle AI and the same energy pool.  The server explicitly allows rider
// casts while seated because our 29 carriers carry Attributes 0x100
// (SPELL_ATTR0_CASTABLE_WHILE_MOUNTED): SpellInfo::CheckVehicle only rejects
// vehicle casts when NEITHER that attribute nor ATTR6_CASTABLE_WHILE_ON_
// VEHICLE is present (fork SpellInfo.cpp:1911).
Creature* ResolveDragonFromCaster(Unit* caster)
{
    if (!caster)
        return nullptr;

    if (Creature* dragon = caster->ToCreature())
        return IsDragon(dragon) ? dragon : nullptr;

    if (Player* player = caster->ToPlayer())
        return GetDragon(player);

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

// B3-R1: forward declaration.  RevokeCombatSkills is defined further below
// with the other combat helpers, but CleanupPlayer (and every vehicle exit
// path) calls it BEFORE that definition point, so it must be declared here
// (real MSVC error C3861 at the first call site in the previous build).
void RevokeCombatSkills(Player* player);

// B3-R1/R0: "leave vehicle without pressing landing" left the player in a
// flying/stale movement state (user-reported: flight effects never go away).
// The vehicle AI normalized itself, but the RIDER kept CAN_FLY / DISABLE_GRAVITY
// / HOVER and flight speeds.  This is the single authoritative rider cleanup,
// called from every exit path (PassengerBoarded apply=false, CleanupPlayer,
// JustDied) after unboarding.
void NormalizeRiderAfterExit(Player* player)
{
    if (!player)
        return;

    // Order matters: SetCanFly(false) removes CAN_FLY|FLYING|ASCENDING|DESCENDING
    // and sends the client unset packets; SetDisableGravity(false) sends the
    // gravity packet.  Then clear any residual hover / transport flags.
    if (player->CanFly())
        player->SetCanFly(false);
    if (player->IsGravityDisabled())
        player->SetDisableGravity(false);
    player->RemoveUnitMovementFlag(MOVEMENTFLAG_HOVER | MOVEMENTFLAG_ONTRANSPORT |
        MOVEMENTFLAG_WATERWALKING | MOVEMENTFLAG_FALLING | MOVEMENTFLAG_FALLING_FAR |
        MOVEMENTFLAG_FALLING_SLOW);

    // Recompute ALL mover rates touched by the G17 session (incl. flight).
    for (UnitMoveType moveType : { MOVE_RUN, MOVE_RUN_BACK, MOVE_SWIM,
        MOVE_SWIM_BACK, MOVE_TURN_RATE, MOVE_FLIGHT, MOVE_FLIGHT_BACK,
        MOVE_PITCH_RATE })
        player->UpdateSpeed(moveType);

    player->SetCanFly(false);
    player->SetDisableGravity(false);
    player->SetAnimTier(AnimTier::Ground);
    player->ClearUnitState(UNIT_STATE_MOVING);
    if (player->GetMotionMaster())
        player->GetMotionMaster()->Clear(MOTION_SLOT_ACTIVE);
    TC_LOG_INFO("scripts.g17.dragonriding",
        "G17B3R1 rider normalized after exit: player={}({})",
        player->GetName(), player->GetGUID().ToString());
}

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
    RevokeCombatSkills(player);
    NormalizeRiderAfterExit(player);
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
                    "|cff80dfff[G17-B2R3 build 2026-08-24 已加载] 1龙息 2增强推进 3朝向冲刺 4飞行器着陆(52226)；若技能异常请回报worldserver.log中G17-B2R3行。|r");
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
                "|cffff4040[G17-B2R1] 异步入座或控制权建立失败；worldserver已记录G17B1精确诊断。|r");
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
        handler->SendSysMessage("|cff80dfff[G17-B2R1] 正在建立全坐骑原生载具控制权……|r");
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

// B2R3: 52226 "飞行器着陆" is a quest-item command authored for the original
// Flying Machine.  Its DBC carries RequiresSpellFocus=1553 and
// CasterAuraSpell=52255 (plus the spell-focus/aura requirements of the
// quest flow).  The G17 dragon has neither the focus object nor that aura,
// so Spell::CheckCast returns SPELL_FAILED_REQUIRES_SPELL_FOCUS /
// SPELL_FAILED_CASTER_AURASTATE BEFORE any spell-script hook can run -
// which is why skill 4 stayed "completely unusable" no matter what
// OnCheckCast did.  The fork's `spell_dbc` overlay table has no columns for
// these two fields, so a SQL override can never clear them; the only
// reliable fix is a server-side runtime sanitization of the loaded
// SpellInfo.  We clear every cast gate that could reject the dragon cast and
// keep the original dummy effect so the client button/tooltip is untouched.
void EnsureLandingCommandCastable()
{
    static bool done = false;
    if (done)
        return;

    SpellInfo const* raw = sSpellMgr->GetSpellInfo(SPELL_SAFE_LANDING);
    if (!raw)
    {
        // Scripts may be registered before SpellInfo loading in some builds;
        // the call is repeated from PassengerBoarded/Reset after loading.
        TC_LOG_ERROR("scripts.g17.dragonriding",
            "G17-B2R3 landing command %u not loaded yet; sanitizer will retry.", SPELL_SAFE_LANDING);
        return;
    }

    // SpellInfo fields are public in this fork; the underlying object is the
    // runtime copy used by every Spell::CheckCast, so clearing here is the
    // authoritative server-side stance for this spell id.
    SpellInfo* info = const_cast<SpellInfo*>(raw);
    info->RequiresSpellFocus = 0;      // DBC 1553: flying-machine focus object.
    info->CasterAuraSpell = 0;         // DBC 52255: "Flying Machine" caster aura.
    info->TargetAuraSpell = 0;
    info->ExcludeCasterAuraSpell = 0;
    info->ExcludeTargetAuraSpell = 0;
    info->CasterAuraState = 0;
    info->TargetAuraState = 0;
    info->ExcludeCasterAuraState = 0;
    info->ExcludeTargetAuraState = 0;
    info->TargetCreatureType = 0;
    info->Stances = 0;
    info->StancesNot = 0;
    info->EquippedItemClass = -1;
    info->EquippedItemSubClassMask = 0;
    info->EquippedItemInventoryTypeMask = 0;
    for (int32& reagent : info->Reagent)
        reagent = 0;
    for (uint32& count : info->ReagentCount)
        count = 0;
    for (uint32& totem : info->Totem)
        totem = 0;
    for (uint32& category : info->TotemCategory)
        category = 0;

    done = true;
    TC_LOG_INFO("server.loading",
        ">> G17-B2R3 landing command %u cast-gates cleared (focus/aura/item/stance); OnCheckCast+AfterCast hooks remain active.", SPELL_SAFE_LANDING);
}

bool IsCombatSkill(uint32 spellId)
{
    return spellId >= COMBAT_SPELL_BASE &&
           spellId < COMBAT_SPELL_BASE + COMBAT_SPELL_COUNT;
}

uint32 CombatArchetype(uint32 spellId)
{
    if (!IsCombatSkill(spellId))
        return 0xFFFFFFFFu;
    return (spellId - COMBAT_SPELL_BASE) / 5u;   // 0 generic,1 beast,2 dragon,3 magic,4 mech order is table order; map to our enum below
}

uint32 CombatSlot(uint32 spellId)
{
    return (spellId - COMBAT_SPELL_BASE) % 5u;
}

// Table order in G17B3_skill_table: DRAGON(0..4), BEAST(5..9), MAGIC(10..14),
// MECHANICAL(15..19), GENERIC(20..24).  Map table-block -> MountArchetype.
uint32 CombatArchetypeToMount(uint32 block)
{
    switch (block)
    {
        case 0: return ARCHETYPE_DRAGON;
        case 1: return ARCHETYPE_BEAST;
        case 2: return ARCHETYPE_MAGIC;
        case 3: return ARCHETYPE_MECHANICAL;
        case 4: return ARCHETYPE_GENERIC;
        default: return ARCHETYPE_GENERIC;
    }
}

float CombatDamageMultiplier(uint32 mountArchetype)
{
    switch (mountArchetype)
    {
        case ARCHETYPE_DRAGON:     return 1.00f;
        case ARCHETYPE_BEAST:      return 1.10f;
        case ARCHETYPE_MAGIC:      return 1.00f;
        case ARCHETYPE_MECHANICAL: return 1.20f;
        case ARCHETYPE_GENERIC:    return 0.90f;
        default:                   return 1.00f;
    }
}

SpellSchoolMask CombatSchool(uint32 mountArchetype)
{
    switch (mountArchetype)
    {
        case ARCHETYPE_MAGIC: return SPELL_SCHOOL_MASK_ARCANE;
        case ARCHETYPE_DRAGON: return SPELL_SCHOOL_MASK_FIRE;
        default:              return SPELL_SCHOOL_MASK_NORMAL;
    }
}

// B3-R2: the archetype block for a mount archetype (table order in
// G17B3_skill_table: DRAGON 0..4, BEAST 5..9, MAGIC 10..14, MECH 15..19,
// GENERIC 20..24).
uint32 ArchetypeBlock(uint32 mountArchetype)
{
    switch (mountArchetype)
    {
        case ARCHETYPE_DRAGON:     return 0;
        case ARCHETYPE_BEAST:      return 1;
        case ARCHETYPE_MAGIC:      return 2;
        case ARCHETYPE_MECHANICAL: return 3;
        default:                   return 4;
    }
}

// B3-R2 page 0: pure movement.  No attack skill mixes into the flight page
// (user requirement: the old bar had the 9573 fire breath on it).
// B3-R2d: the stock 3.3.5 client VehicleMenuBar renders a FIXED 6 spell
// buttons (verified live: 7 sent -> 6 shown), so each page is laid out for
// exactly 6 visible buttons.  The brake skill left the bar (the S key already
// bleeds momentum hard); it stays registered for the future client-UI
// extension.  Total skill pool is unbounded through page switching.
void WriteMovementPage(Creature* dragon, uint32 mountArchetype)
{
    // B3-R10: all eight slots filled for the C11 8-button client bar.
    // User directive: the page switch lives at slot 8 (last button, fixed
    // position on BOTH pages for muscle memory); brake at slot 6; the free
    // archetype GENERATOR (slot-0 combat skill: no cost, +8 energy, light
    // damage) rides at slot 7 so the rider can build energy mid-flight.
    dragon->m_spells[0] = SPELL_ASCEND;       // 拉升
    dragon->m_spells[1] = SPELL_DIVE;         // 俯冲 (energy recovery)
    dragon->m_spells[2] = SPELL_ACCELERATE;   // 增强推进
    dragon->m_spells[3] = SPELL_CLIMB;        // 朝向冲刺
    dragon->m_spells[4] = SPELL_SAFE_LANDING; // 飞行器着陆
    dragon->m_spells[5] = SPELL_GLIDE_BRAKE;  // 滑翔/制动 (slot 6, both pages)
    dragon->m_spells[6] = SPELL_WIND_STANCE;  // B3-R12: 御风姿态@7 - a FLIGHT skill on
                                              // the flight page (user report: the crossed
                                              // generator here read as a wrong-page skill)
    dragon->m_spells[7] = SPELL_PAGE_SWITCH;  // 切换技能页 (slot 8, ALWAYS last)
}

// B3-R2 page 1: the archetype combat set (5 skills + the switch button).
void WriteCombatPage(Creature* dragon, uint32 mountArchetype)
{
    uint32 const block = ArchetypeBlock(mountArchetype);
    for (uint32 i = 0; i < COMBAT_PAGE_SLOTS; ++i)
        dragon->m_spells[i] = COMBAT_SPELL_BASE + block * 5u + i;
    dragon->m_spells[5] = SPELL_GLIDE_BRAKE;  // 滑翔/制动 (slot 6, both pages)
    dragon->m_spells[6] = SPELL_SWOOP_STRIKE; // B3-R12: 突袭@7 - an ATTACK skill on the
                                              // attack page (was: dive, the crossed filler)
    dragon->m_spells[7] = SPELL_PAGE_SWITCH;  // 切换技能页 (slot 8, ALWAYS last)
}

// B3-R3 (DF-style skill panel): while mounted, the dragonriding skills ALSO
// live in the rider's own spellbook so they can be cast from the player's
// bars / the G17DragonBar addon bar (12+ slots, no client UI modification).
// The server accepts rider casts while seated because every carrier carries
// Attributes 0x100 (see ResolveDragonFromCaster).  Everything is unlearned
// on exit/login by RevokeCombatSkills below.
void GrantSkillPanel(Player* player, uint32 mountArchetype)
{
    if (!player)
        return;

    uint32 const block = ArchetypeBlock(mountArchetype);
    for (uint32 i = 0; i < COMBAT_PAGE_SLOTS; ++i)
    {
        uint32 const sid = COMBAT_SPELL_BASE + block * 5u + i;
        if (!player->HasSpell(sid))
            player->LearnSpell(sid, false);
    }
    for (uint32 sid : { SPELL_PAGE_SWITCH, SPELL_ASCEND, SPELL_DIVE, SPELL_GLIDE_BRAKE })
        if (!player->HasSpell(sid))
            player->LearnSpell(sid, false);

    TC_LOG_INFO("scripts.g17.dragonriding",
        "G17B3R3 skill panel granted: player={}({}) archetypeBlock={} spells=9 (4 movement + 5 combat)",
        player->GetName(), player->GetGUID().ToString(), block);
}

// Migration cleanup: B3-R1 learned the 25 carriers into the player spellbook
// (unusable there while riding).  B3-R2 keeps them ONLY on the vehicle bar, so
// any lingering learned copies are removed on every exit path and at login.
void RevokeCombatSkills(Player* player)
{
    if (!player)
        return;
    for (uint32 i = 0; i < COMBAT_SPELL_COUNT; ++i)
    {
        uint32 const sid = COMBAT_SPELL_BASE + i;
        if (player->HasSpell(sid))
            player->RemoveSpell(sid, false, false);
    }
    // The four B3-R2 bar spells must never linger in a spellbook either.
    for (uint32 sid : { SPELL_PAGE_SWITCH, SPELL_ASCEND, SPELL_DIVE, SPELL_GLIDE_BRAKE })
        if (player->HasSpell(sid))
            player->RemoveSpell(sid, false, false);
}

SpellCastResult CheckEnergyCast(Unit* caster, uint32 cost)
{
    // B3-R9: the energy gate now applies to BOTH cast paths - the vehicle
    // bar (dragon caster) and the player-cast buttons of the custom UI.
    Creature* dragon = ResolveDragonFromCaster(caster);
    if (!dragon)
        return SPELL_CAST_OK;

    if (!GetRider(dragon))
        return SPELL_FAILED_CASTER_DEAD;

    if (dragon->GetPower(POWER_ENERGY) < cost)
        return SPELL_FAILED_NO_POWER;

    return SPELL_CAST_OK;
}

// B3-R1: controlled stun release (never leaves the target stunned forever).
// Defined here, BEFORE its first use in ExecuteCombatSkill below, so that
// `new CombatStunReleaseEvent(...)` sees a complete type (real MSVC errors
// C2061/C2660/C2143/C2059 at the call site in the previous build).
class CombatStunReleaseEvent : public BasicEvent
{
public:
    CombatStunReleaseEvent(ObjectGuid casterGuid, ObjectGuid targetGuid)
        : _casterGuid(casterGuid), _targetGuid(targetGuid) { }

    bool Execute(uint64 /*now*/, uint32 /*diff*/) override
    {
        // Resolve through the caster (must be a valid in-world WorldObject);
        // ObjectAccessor::GetUnit requires a non-null searcher in this fork.
        // NOTE: GetUnit takes (const WorldObject&, ObjectGuid const&), so the
        // Player* must be dereferenced (real MSVC error C2664 before).
        Player* caster = ObjectAccessor::FindPlayer(_casterGuid);
        Unit* target = caster ? ObjectAccessor::GetUnit(*caster, _targetGuid) : nullptr;
        if (target && target->IsInWorld())
        {
            target->RemoveUnitFlag(UNIT_FLAG_STUNNED);
            target->ClearUnitState(UNIT_STATE_STUNNED);
        }
        return true;
    }

private:
    ObjectGuid _casterGuid;
    ObjectGuid _targetGuid;
};

// B3-R7: the client no longer predicts cooldowns locally (C9 zeroed DBC
// RecoveryTime to kill the phantom cooldown), and vehicle-creature casters
// never reach the player-session packet path inside
// SpellHistory::StartCooldown (only the MOD_COOLDOWN branch sends there, and
// GetPlayerOwner() of a vehicle has no session of its own).  Mirror the pet
// path explicitly: tell the RIDER's client the spell went on cooldown, once
// with the DRAGON's GUID (pet/vehicle cooldown cache) and once with the
// PLAYER's GUID (action-slot cooldown cache), so the bonus-bar action slots
// behind the vehicle bar buttons render the swirl whichever cache the 3.3.5
// client consults.  Built with the stock SpellHistory::BuildCooldownPacket so
// the wire format can never drift from the core.
void SendVehicleCooldownPackets(Player* rider, Unit* owner, uint32 spellId, uint32 cdMs)
{
    if (!rider || !owner || !cdMs)
        return;
    if (WorldSession* session = rider->GetSession())
    {
        WorldPacket data;
        owner->GetSpellHistory()->BuildCooldownPacket(data, SPELL_COOLDOWN_FLAG_NONE, spellId, cdMs);
        session->SendPacket(&data);
        if (rider != owner)
        {
            WorldPacket self;
            rider->GetSpellHistory()->BuildCooldownPacket(self, SPELL_COOLDOWN_FLAG_NONE, spellId, cdMs);
            session->SendPacket(&self);
        }
    }
}

// B3-R2: execute one combat carrier.  The spell is now cast BY THE VEHICLE
// (vehicle action bar button), so the caster is the dragon and the effects are
// attributed to the RIDER (damage/heal/kill credit, design doc 5.3).  Slot 0
// is the archetype generator: free, restores energy, light damage.  Cooldowns
// go on the vehicle spell history so the bar buttons grey out and survive a
// page switch (resources are never reset by switching pages).
void ExecuteCombatSkill(Creature* dragon, Spell* spell)
{
    if (!dragon || !spell || !dragon->IsAIEnabled())
        return;
    Player* player = GetRider(dragon);
    if (!player)
        return;
    SpellInfo const* info = spell->GetSpellInfo();
    if (!info)
        return;

    uint32 const block = CombatArchetype(info->Id);
    uint32 const arch = CombatArchetypeToMount(block);
    uint32 const slot = CombatSlot(info->Id);
    uint32 const cost = (slot == 4) ? COMBAT_BURST_ENERGY : COMBAT_ENERGY_COST;

    // Target resolution FIRST (nothing is consumed before legality passes):
    // explicit cast target, else the rider's selection, else the dragon's
    // current victim.  Slot 2 (defense/heal) never needs a target.
    Unit* target = spell->m_targets.GetUnitTarget();
    if (!target || target == dragon)
        target = player->GetSelectedUnit();
    if (!target)
        target = dragon->GetVictim();
    if (target == player || (target && target->IsFriendlyTo(player)))
        target = nullptr;

    if (slot != 2 && !target)
    {
        if (WorldSession* session = player->GetSession())
            ChatHandler(session).PSendSysMessage("|cffff8040[G17-B3] %s 需要敌对目标。|r",
                info->SpellName[0] ? info->SpellName[0] : "御龙战斗技能");
        return;
    }

    // Legality (design doc 5.3): attacks may not bypass distance or LOS.
    if (target && (target->GetMap() != player->GetMap() ||
        !player->IsWithinDist(target, COMBAT_MAX_RANGE) ||
        !player->IsWithinLOS(target->GetPositionX(), target->GetPositionY(), target->GetPositionZ())))
    {
        if (WorldSession* session = player->GetSession())
            ChatHandler(session).PSendSysMessage("|cffff8040[G17-B3] 目标超出40码或视线受阻。|r");
        return;
    }

    // Energy: slot 0 generates, the other slots consume (vehicle power pool).
    if (slot == 0)
        dragon->ModifyPower(POWER_ENERGY, GENERATOR_ENERGY_GAIN);
    else
        dragon->ModifyPower(POWER_ENERGY, -int32(cost));

    // Cooldown on the vehicle AND the rider (dual-cast paths share gates;
    // the vehicle bar greys via the vehicle history, the skill panel via the
    // player history, and neither can bypass the other).
    dragon->GetSpellHistory()->AddCooldown(info->Id, 0, Milliseconds(COMBAT_CD_MS[slot]));
    player->GetSpellHistory()->AddCooldown(info->Id, 0, Milliseconds(COMBAT_CD_MS[slot]));
    SendVehicleCooldownPackets(player, dragon, info->Id, COMBAT_CD_MS[slot]); // B3-R7 UI cooldown

    float const mult = CombatDamageMultiplier(arch) *
        ((target && target->IsPlayer()) ? COMBAT_PLAYER_FACTOR : 1.0f);
    SpellSchoolMask const school = CombatSchool(arch);
    uint32 const cdMs = COMBAT_CD_MS[slot];

    WorldSession* session = player->GetSession();
    if (slot == 2)
    {
        // Defense/assist: heal + cleanse common movement-imparing mechanics.
        uint32 const heal = uint32(float(player->GetMaxHealth()) * COMBAT_HEAL_PCT);
        if (heal)
        {
            HealInfo healInfo(player, player, heal, info, SPELL_SCHOOL_MASK_NORMAL);
            Unit::DealHeal(healInfo);
        }
        player->RemoveAurasWithMechanic(
            (1u << MECHANIC_ROOT) | (1u << MECHANIC_FEAR) | (1u << MECHANIC_FREEZE) |
            (1u << MECHANIC_STUN) | (1u << MECHANIC_SILENCE) | (1u << MECHANIC_HORROR),
            AURA_REMOVE_BY_DEFAULT, info->Id);
        player->SendPlaySpellVisualKit(VISUAL_KIT_TRAIL_PULSE, 0);
        if (session)
            ChatHandler(session).PSendSysMessage("|cff80ff80[G17-B3] %s：治疗自身并净化控制效果。|r",
                info->SpellName[0] ? info->SpellName[0] : "御龙战斗技能");
    }
    else
    {
        uint32 const base = slot == 0 ? (COMBAT_BASE_DAMAGE[slot] / 2u) : COMBAT_BASE_DAMAGE[slot];
        uint32 const levelBonus = player->GetLevel() *
            (slot == 4 ? COMBAT_BURST_PER_LEVEL : COMBAT_DAMAGE_PER_LEVEL);
        float const factor = (slot == 4) ? COMBAT_BURST_MULT : 1.0f;
        uint32 const amount = uint32(float(base + levelBonus) * mult * factor);
        // Attacker = rider: kill/quest/threat credit stays with the player.
        Unit::DealDamage(player, target, amount, nullptr, DIRECT_DAMAGE, school, info, true);

        // B3-R4: visible feedback - the dragon swings at the target and an
        // audited one-shot impact kit fires on it; the damage packet itself
        // drives the target flash and floating combat text.
        dragon->SendMeleeAttackStart(target);
        dragon->SendMeleeAttackStop(target);
        target->SendPlaySpellVisualKit(VISUAL_KIT_IMPACT_RING, 0);

        if (slot == 3)
        {
            // Control follow-up: interrupt + short stun with guaranteed release.
            target->CastStop();
            target->SetUnitFlag(UNIT_FLAG_STUNNED);
            target->AddUnitState(UNIT_STATE_STUNNED);
            player->m_Events.AddEvent(
                new CombatStunReleaseEvent(player->GetGUID(), target->GetGUID()),
                player->m_Events.CalculateTime(Milliseconds(G17Dragonriding::COMBAT_STUN_MS)));
            TC_LOG_INFO("scripts.g17.dragonriding",
                "G17B3R2 combat stun: caster={} target={} duration=%u",
                player->GetGUID().ToString(), target->GetGUID().ToString(), COMBAT_STUN_MS);
        }
        // B3-R5: chat feedback only for player-initiated casts (the mount's
        // auto generator every 4.5s would otherwise spam the chat frame).
        if (session && !spell->IsTriggered())
        {
            if (slot == 0)
                ChatHandler(session).PSendSysMessage("|cff80dfff[G17-B3] %s：%u 点伤害，龙能量+%d。|r",
                    info->SpellName[0] ? info->SpellName[0] : "御龙战斗技能", amount, GENERATOR_ENERGY_GAIN);
            else
                ChatHandler(session).PSendSysMessage("|cff80dfff[G17-B3] %s：%u 点伤害。|r",
                    info->SpellName[0] ? info->SpellName[0] : "御龙战斗技能", amount);
        }
    }

    TC_LOG_INFO("scripts.g17.dragonriding",
        "G17B3R2 combat: player={}({}) spell=%u block=%u slot=%u energy={} cd=%u",
        player->GetName(), player->GetGUID().ToString(), info->Id, block, slot,
        dragon->GetPower(POWER_ENERGY), cdMs);
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
        _windStance = false; // B3-R12
        _safetyCleanupStarted = false;
        _safetyCheckTimer = G17Dragonriding::SAFETY_CHECK_INTERVAL_MS;
        _motionSampleAccumulator = 0;
        _speedPacketTimer = 0;
        _climbControlTimer = 0;
        _boostTimer = 0;
        _boostTrailTimer = 0;
        _boostGustTimer = 0;
        _landingTimer = 0;
        _landingSegments = 0;
        _boostTopSpeedAnnounced = false;
        _orphanGraceTimer = 3000;
        _momentum = 0.10f;
        _currentSpeedRate = G17Dragonriding::FLIGHT_SPEED_RATES.front();
        _targetSpeedRate = _currentSpeedRate;
        _currentTurnRate = 1.0f;
        _speedTier = 0;
        _notifiedSpeedTier = 0;
        _combatPage = false;
        _energyRegenTimer = 0;
        _autoCombatTimer = 0;
        _lastZ = me->GetPositionZ();
        _lastSampleX = me->GetPositionX();
        _lastSampleY = me->GetPositionY();
        _smoothedTravelHeading = me->GetOrientation();
        _travelHeadingReady = false;
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
            case G17Dragonriding::DATA_BOOST_ACTIVE: return _boostTimer ? 1 : 0;
            case G17Dragonriding::DATA_SKILL_PAGE: return _combatPage ? 1 : 0;
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
            // Belt-and-suspenders: if the AddSC call ran before SpellInfo was
            // fully loaded, sanitize now (the passenger can only press skill 4
            // after boarding, which is guaranteed post-load).
            G17Dragonriding::EnsureLandingCommandCastable();
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
            _boostTrailTimer = 0;
            _boostGustTimer = 0;
            _landingTimer = 0;
            _landingSegments = 0;
            _boostTopSpeedAnnounced = false;
            _orphanGraceTimer = 0;
            _momentum = 0.10f;
            _currentSpeedRate = G17Dragonriding::FLIGHT_SPEED_RATES.front();
            _targetSpeedRate = _currentSpeedRate;
            _currentTurnRate = 1.0f;
            _speedTier = 0;
            _notifiedSpeedTier = 0;
            _lastZ = me->GetPositionZ();
            _lastSampleX = me->GetPositionX();
            _lastSampleY = me->GetPositionY();
            _smoothedTravelHeading = me->GetOrientation();
            _travelHeadingReady = false;
            RestoreClientFlightControl(true);
            ApplyMovementRates(_currentSpeedRate, true);
            me->SetPowerType(POWER_ENERGY);
            me->SetMaxPower(POWER_ENERGY, 100);
            me->SetPower(POWER_ENERGY, 100);
            // B3-R2: the skill bar lives on the VEHICLE (8 slots), not in the
            // player spellbook.  Boarding starts on the pure movement page and
            // re-sends SMSG_PET_SPELLS so the client bar is correct at once.
            _combatPage = false;
            _energyRegenTimer = 0;
            _autoCombatTimer = 0;
            G17Dragonriding::WriteMovementPage(me, _archetype);
            player->VehicleSpellInitialize();
            // B3-R6 PERFORMANCE FIX: GrantSkillPanel removed from boarding.
            // The player-cast path is proven broken in 3.3.5 (client diagnostic
            // showed IsKnown=false for ALL G17 spells; LearnSpell only
            // registers server-side, the client never recognizes them).
            // Learning 9 spells on every mount = 9 network packets in one
            // tick = the mount lag the user reported. OnLogin cleanup handles
            // any leftovers from old B3R3 runs.
            TC_LOG_DEBUG("scripts.g17.dragonriding",
                "G17B3R6 movement page installed: player={}({}) archetype={} bar=6visible(8slots-cap)",
                player->GetName(), player->GetGUID().ToString(),
                G17Dragonriding::ArchetypeName(GetData(G17Dragonriding::DATA_ARCHETYPE)));
            // Migration: unlearn any B3-R1 spellbook leftovers immediately.
            G17Dragonriding::RevokeCombatSkills(player);
            if (WorldSession* session = player->GetSession())
                ChatHandler(session).PSendSysMessage(
                    "|cff80dfff[G17] 御龙术就绪：|r1-5 技能｜|cff80ff806|r 制动｜|cff80ff807|r 能量生成｜|cff80ff808|r 切换技能页");
            return;
        }

        NormalizeVehicleForExit();
        // B3-R6: RevokeCombatSkills removed from dismount (lag fix - 29
        // RemoveSpell packets per dismount). OnLogin handles old leftovers.
        G17Dragonriding::NormalizeRiderAfterExit(passenger->ToPlayer());
        me->DespawnOrUnsummon(500ms);
    }

    // B3-R5: the mount fights alongside the rider with a REAL cast.  While
    // the rider is in combat (and not in a BG/arena), the dragon periodically
    // triggered-casts the archetype GENERATOR (slot 0 - free, restores 8
    // energy, half damage, full SpellVisual on the client) at the rider's
    // selected/victim target.  Damage/kill credit goes to the RIDER via the
    // shared ExecuteCombatSkill path; the cooldown lives on both histories.
    void UpdateAutoCombat(uint32 diff)
    {
        using namespace G17Dragonriding;

        if (_autoCombatTimer > diff)
        {
            _autoCombatTimer -= diff;
            return;
        }
        _autoCombatTimer = G17Dragonriding::AUTOCOMBAT_INTERVAL_MS;

        Player* rider = G17Dragonriding::GetRider(me);
        if (!rider || !rider->IsInCombat() || rider->InBattleground() || rider->InArena())
            return;

        Unit* victim = rider->GetSelectedUnit();
        if (!victim)
            victim = rider->GetVictim();
        if (!victim || !victim->IsAlive() || victim == rider ||
            victim->IsFriendlyTo(rider) ||
            !rider->IsWithinDist(victim, float(G17Dragonriding::AUTOCOMBAT_RANGE)) ||
            !rider->IsWithinLOS(victim->GetPositionX(), victim->GetPositionY(), victim->GetPositionZ()))
            return;

        // Real triggered cast: the client renders the dragon's cast animation
        // and the carrier's SpellVisual (C6 client patch); the SpellScript
        // executes the rider-attributed damage, the +8 energy gain and the
        // cooldowns.  A failed cast (target lost mid-tick) is silently skipped.
        uint32 const generator = G17Dragonriding::COMBAT_SPELL_BASE +
            G17Dragonriding::ArchetypeBlock(_archetype) * 5u;
        me->CastSpell(victim, generator, true);
        TC_LOG_DEBUG("scripts.g17.dragonriding",
            "G17B3R5 auto combat cast: rider={}({}) target={} spell={}",
            rider->GetName(), rider->GetGUID().ToString(), victim->GetGUID().ToString(), generator);
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
                CompleteClimb(true);
        }

        if (_landing)
        {
            if (_landingTimer > diff)
                _landingTimer -= diff;
            else
                AbortLanding("着陆路径超时：已恢复飞行控制，未强制下坠。", true);
        }

        if (!_landing && !_climbing)
            UpdateContinuousFlight(diff);

        UpdateAutoCombat(diff);

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
                "|cffff8040[G17-B2R1] 已进入禁飞/城市/室内/副本区域，御空状态已无降落伞归一化清理。|r");
        G17Dragonriding::CleanupPlayer(player, false, true);
    }

    void DoAction(int32 action) override
    {
        using namespace G17Dragonriding;

        if (!GetRider(me))
            return;

        if (action == ACTION_ACCELERATE)
        {
            if (_landing || _boostTimer)
                return;

            _boostTimer = BOOST_DURATION_MS;
            _boostTrailTimer = 1;
            _boostGustTimer = BOOST_GUST_INTERVAL_MS;
            _boostGustPulseCount = 0;
            _boostTopSpeedAnnounced = false;
            _momentum = std::min(MAX_MOMENTUM, _momentum + 0.28f);
            // Layered launch feedback using only audited 3.3.5a client kits: a
            // double ChargeTrail burst for a strong launch read, plus the
            // ribbon trail starter.  Mechanical mounts also fire the jet flame;
            // all are client-side one-shots (no aura, no damage, no state).
            me->SendPlaySpellVisualKit(VISUAL_KIT_BOOST_LAUNCH, 0);
            me->SendPlaySpellVisualKit(VISUAL_KIT_BURST_EXTRA, 0);
            me->SendPlaySpellVisualKit(VISUAL_KIT_TRAIL_PULSE, 0);
            if (_archetype == ARCHETYPE_MECHANICAL)
                me->SendPlaySpellVisualKit(VISUAL_KIT_MECHANICAL_THRUST, 0);
            else
                // Non-mechanical mounts get an extra wind-burst punch on launch.
                me->SendPlaySpellVisualKit(VISUAL_KIT_WIND_BURST, 0);
            if (_stalling)
                RecoverFromStall();
            SendToRider(me,
                "|cff80dfff[G17-B2R2] 高速推进启动：双层冲刺爆发、持续尾流与气浪已触发，正在平滑升档（硬上限1200%）。|r");
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

        // ---- B3-R2 multi-page skill bar ----
        if (action == ACTION_PAGE_SWITCH)
        {
            ApplySkillPage(!_combatPage);
            return;
        }

        // ---- B3-R12 御风姿态 toggle ----
        if (action == G17Dragonriding::ACTION_WIND_STANCE)
        {
            _windStance = !_windStance;
            ApplyMovementRates(_currentSpeedRate, true);
            if (Player* rider = GetRider(me))
                if (WorldSession* session = rider->GetSession())
                    ChatHandler(session).PSendSysMessage(_windStance
                        ? "|cff80dfff[G17] 御风姿态：开启（转向+15%%，能量回复加倍）。|r"
                        : "|cff80dfff[G17] 御风姿态：关闭。|r");
            return;
        }

        if (action == ACTION_ASCEND)
        {
            if (_landing || _climbing)
                return;
            StartAscend();
            return;
        }

        if (action == ACTION_DIVE)
        {
            if (_landing || _climbing)
                return;
            StartDive();
            return;
        }

        if (action == ACTION_GLIDE_BRAKE)
        {
            if (_landing || _climbing)
                return;
            StartGlideBrake();
            return;
        }
    }

    // B3-R2: swap the vehicle action bar between the movement page and the
    // archetype combat page.  m_spells[] is rewritten and SMSG_PET_SPELLS is
    // re-sent via the public Player::VehicleSpellInitialize(); energy and
    // cooldowns live on the vehicle, so switching never resets resources.
    // The combat page caps speed at tier COMBAT_PAGE_SPEED_TIER_CAP (600%).
    void ApplySkillPage(bool combatPage)
    {
        using namespace G17Dragonriding;

        _combatPage = combatPage;
        if (combatPage)
        {
            WriteCombatPage(me, _archetype);
            // Drop out of any boost when entering combat stance.
            if (_boostTimer)
            {
                _boostTimer = 0;
                _boostTrailTimer = 0;
                _boostGustTimer = 0;
            }
        }
        else
            WriteMovementPage(me, _archetype);

        if (Player* rider = GetRider(me))
        {
            rider->VehicleSpellInitialize();
            // B3-R10: instant visible feedback - if this line never prints,
            // the switch cast never reached the server.
            if (WorldSession* session = rider->GetSession())
                ChatHandler(session).PSendSysMessage("|cff80dfff[G17] 已切换到：|r|cff80ff80%s|r（按第8格再切换）",
                    _combatPage ? "战斗技能页" : "移动技能页");
        }

        // Re-evaluate the speed tier under the new cap and push rates now.
        _notifiedSpeedTier = 0xFFFFFFFFu;
        UpdateSpeedTier();
        ApplyMovementRates(_currentSpeedRate, true);

        SendToRider(me, _combatPage
            ? "|cffff9f4f[G17-B3R2] 已切换到战斗技能页：类型攻击五连 + 切页按钮；飞行速度上限降至600%。|r"
            : "|cff80dfff[G17-B3R2] 已切换到移动技能页：拉升/俯冲/推进/冲刺/制动/着陆；速度上限恢复。|r");
        TC_LOG_INFO("scripts.g17.dragonriding",
            "G17B3R2 page switch: player={} page={} archetype={}",
            GetRider(me) ? GetRider(me)->GetName() : std::string(),
            _combatPage ? "COMBAT" : "MOVEMENT",
            ArchetypeName(_archetype));
    }

    // B3-R2 拉升: powered steep climb along the current facing.  Costs energy,
    // converts it into altitude, keeps a floor of momentum so the climb never
    // stalls on exit.
    void StartAscend()
    {
        using namespace G17Dragonriding;

        if (_stalling)
            RecoverFromStall();

        RestoreClientFlightControl(true);
        me->StopMoving();
        float const heading = ResolveFacingHeading();
        me->SetFacingTo(heading);
        me->SetOrientation(heading);

        float const startX = me->GetPositionX();
        float const startY = me->GetPositionY();
        float const startZ = me->GetPositionZ();

        Movement::PointsArray path;
        path.push_back(G3D::Vector3(startX, startY, startZ));
        path.push_back(G3D::Vector3(
            startX + std::cos(heading) * (ASCEND_FORWARD_DISTANCE * 0.5f),
            startY + std::sin(heading) * (ASCEND_FORWARD_DISTANCE * 0.5f),
            startZ + ASCEND_HEIGHT * 0.55f));
        path.push_back(G3D::Vector3(
            startX + std::cos(heading) * ASCEND_FORWARD_DISTANCE,
            startY + std::sin(heading) * ASCEND_FORWARD_DISTANCE,
            startZ + ASCEND_HEIGHT));

        _climbing = true;   // reuse the proven spline-maneuver state machine
        _climbControlTimer = CLIMB_CONTROL_TIMEOUT_MS;
        _climbExitHeading = heading;
        _momentum = std::max(_momentum, 0.14f);
        me->GetMotionMaster()->LaunchMoveSpline(
            [path, heading](Movement::MoveSplineInit& init)
            {
                init.MovebyPath(path);
                init.SetFly();
                init.SetVelocity(ASCEND_SPLINE_SPEED);
                init.SetFacing(heading);
                init.SetOrientationFixed(true);
            }, POINT_CLIMB);
        me->SendPlaySpellVisualKit(VISUAL_KIT_TRAIL_PULSE, 0);
        SendToRider(me, "|cff80dfff[G17-B3R2] 拉升：消耗龙能量沿当前朝向急速爬升。|r");
    }

    // B3-R2 俯冲: powered dive along the current facing.  RESTORES energy
    // (the dragonriding recovery loop: climb high, dive to regain), boosts
    // momentum, and never dips below the floor + safety margin.
    void StartDive()
    {
        using namespace G17Dragonriding;

        if (_stalling)
            RecoverFromStall();

        RestoreClientFlightControl(true);
        me->StopMoving();
        float const heading = ResolveFacingHeading();
        me->SetFacingTo(heading);
        me->SetOrientation(heading);

        float const startX = me->GetPositionX();
        float const startY = me->GetPositionY();
        float const startZ = me->GetPositionZ();

        float targetZ = startZ - DIVE_DEPTH;
        float const floorZ = me->GetFloorZ();
        if (floorZ > INVALID_HEIGHT + 1.0f && targetZ < floorZ + DIVE_MIN_ALTITUDE)
            targetZ = floorZ + DIVE_MIN_ALTITUDE;
        if (targetZ >= startZ)
            targetZ = startZ - 2.0f;

        Movement::PointsArray path;
        path.push_back(G3D::Vector3(startX, startY, startZ));
        path.push_back(G3D::Vector3(
            startX + std::cos(heading) * (DIVE_FORWARD_DISTANCE * 0.5f),
            startY + std::sin(heading) * (DIVE_FORWARD_DISTANCE * 0.5f),
            startZ + (targetZ - startZ) * 0.55f));
        path.push_back(G3D::Vector3(
            startX + std::cos(heading) * DIVE_FORWARD_DISTANCE,
            startY + std::sin(heading) * DIVE_FORWARD_DISTANCE,
            targetZ));

        _climbing = true;   // same spline-maneuver completion machinery
        _climbControlTimer = CLIMB_CONTROL_TIMEOUT_MS;
        _climbExitHeading = heading;
        _momentum = std::min(MAX_MOMENTUM, _momentum + DIVE_MOMENTUM_GAIN);

        int32 const gain = DIVE_ENERGY_GAIN;
        me->ModifyPower(POWER_ENERGY, gain);

        me->GetMotionMaster()->LaunchMoveSpline(
            [path, heading](Movement::MoveSplineInit& init)
            {
                init.MovebyPath(path);
                init.SetFly();
                init.SetVelocity(DIVE_SPLINE_SPEED);
                init.SetFacing(heading);
                init.SetOrientationFixed(true);
            }, POINT_CLIMB);
        me->SendPlaySpellVisualKit(VISUAL_KIT_WIND_BURST, 0);
        SendToRider(me, "|cff80ff80[G17-B3R2] 俯冲：动量提升，龙能量已恢复。|r");
    }

    // B3-R2 滑翔/制动: instant air brake.  Bleeds momentum and drops the speed
    // tier so the mount slows for precision approach or tight turns.
    void StartGlideBrake()
    {
        using namespace G17Dragonriding;

        _momentum = std::min(_momentum, GLIDE_BRAKE_MOMENTUM);
        if (_speedTier > 1)
            _speedTier = 1;
        _targetSpeedRate = FLIGHT_SPEED_RATES[_speedTier];
        _notifiedSpeedTier = 0xFFFFFFFFu;
        UpdateSpeedTier();
        ApplyMovementRates(_currentSpeedRate, true);
        me->SendPlaySpellVisualKit(VISUAL_KIT_IMPACT_RING, 0);
        SendToRider(me, "|cffb0ffb0[G17-B3R2] 滑翔制动：动量已削减，飞行速度正在下降。|r");
    }

    void MovementInform(uint32 type, uint32 pointId) override
    {
        using namespace G17Dragonriding;

        if (type != EFFECT_MOTION_TYPE)
            return;

        if (pointId == POINT_CLIMB && _climbing)
        {
            CompleteClimb(false);
            return;
        }

        if (pointId == POINT_LAND_APPROACH && _landing && _landingApproach)
        {
            _landingApproach = false;
            StartLandingSegment();
            return;
        }

        if (pointId == POINT_LAND && _landing)
            CompleteLanding();
    }

    void JustDied(Unit* /*killer*/) override
    {
        NormalizeVehicleForExit();
        if (Player* player = G17Dragonriding::GetRider(me))
        {
            player->ExitVehicle();
            G17Dragonriding::RevokeCombatSkills(player);
            G17Dragonriding::NormalizeRiderAfterExit(player);
        }
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
        float const speedBlend = std::clamp((flightRate - 2.5f) / 9.5f, 0.0f, 1.0f);
        float const targetTurnRate = 1.0f - 0.18f * speedBlend;
        float const turnStep = force ? 0.08f : 0.04f;
        if (flightRate <= 1.01f)
            _currentTurnRate = 1.0f;
        else if (_currentTurnRate < targetTurnRate)
            _currentTurnRate = std::min(targetTurnRate, _currentTurnRate + turnStep);
        else
            _currentTurnRate = std::max(targetTurnRate, _currentTurnRate - turnStep);

        if (!force && std::abs(flightRate - _lastAppliedSpeedRate) < 0.02f &&
            std::abs(_currentTurnRate - targetTurnRate) < 0.01f)
            return;

        float const backwardRate = std::min(flightRate, 4.0f);
        me->SetSpeedRate(MOVE_RUN, flightRate);
        me->SetSpeedRate(MOVE_RUN_BACK, backwardRate);
        me->SetSpeedRate(MOVE_SWIM, flightRate);
        me->SetSpeedRate(MOVE_SWIM_BACK, backwardRate);
        me->SetSpeedRate(MOVE_FLIGHT, flightRate);
        me->SetSpeedRate(MOVE_FLIGHT_BACK, backwardRate);
        // Never amplify angular velocity at high speed. The 1.00 -> 0.82
        // envelope changes in 0.04 steps and remains responsive without a cut.
        me->SetSpeedRate(MOVE_TURN_RATE,
            _currentTurnRate * (_windStance ? G17Dragonriding::WIND_STANCE_TURN_BONUS : 1.0f));
        me->SetSpeedRate(MOVE_PITCH_RATE, std::max(0.90f, _currentTurnRate));
        _lastAppliedSpeedRate = flightRate;
    }

    // B3-R11: land-mount models (BEAST/GENERIC archetypes - horses, wolves...)
    // have no Fly-tier animations.  The 3.3.5 client falls back to the GROUND
    // tier for them, so the mount's legs pedal the run cycle through the whole
    // flight (user report: 双脚蹬个不停).  AnimTier cannot fix this (fork docs:
    // Swim is not client-handled; Fly falls back to ground).  A server-set
    // EMOTE STATE, however, overrides the movement animation of non-self
    // units - freeze the model in its stand pose while airborne, clear it on
    // the ground.  Flying-model archetypes keep their native fly animations.
    void UpdateFlightEmote(bool airborne)
    {
        // r1a fix: the AI class lives OUTSIDE namespace G17Dragonriding - the
        // bare ARCHETYPE_* ids were undeclared here (real MSVC C2065 x2).
        if (_archetype == G17Dragonriding::ARCHETYPE_BEAST ||
            _archetype == G17Dragonriding::ARCHETYPE_GENERIC)
            me->SetEmoteState(airborne ? EMOTE_STATE_STAND : EMOTE_ONESHOT_NONE);
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
        UpdateFlightEmote(true); // B3-R11: freeze land-mount legs in flight
    }

    void NormalizeVehicleForExit()
    {
        _landing = false;
        _landingApproach = false;
        _climbing = false;
        _stalling = false;
        _boostTimer = 0;
        _boostTrailTimer = 0;
        _boostGustTimer = 0;
        _boostTopSpeedAnnounced = false;
        _landingTimer = 0;
        _landingSegments = 0;
        _climbControlTimer = 0;
        _momentum = 0.0f;
        _currentSpeedRate = 1.0f;
        _targetSpeedRate = 1.0f;
        _currentTurnRate = 1.0f;
        me->GetMotionMaster()->Clear(MOTION_SLOT_ACTIVE);
        me->RemoveAurasDueToSpell(G17Dragonriding::SPELL_ACCELERATE);
        me->RemoveAurasDueToSpell(G17Dragonriding::SPELL_SAFE_LANDING);
        ApplyMovementRates(1.0f, true);
        me->RemoveUnitMovementFlag(MOVEMENTFLAG_WALKING | MOVEMENTFLAG_FALLING |
            MOVEMENTFLAG_FALLING_FAR | MOVEMENTFLAG_FALLING_SLOW | MOVEMENTFLAG_HOVER);
        me->SetAnimTier(AnimTier::Ground);
        me->SetCanFly(false);
        me->SetDisableGravity(false);
        UpdateFlightEmote(false); // B3-R11: restore normal ground animations
    }

    void UpdateContinuousFlight(uint32 diff)
    {
        using namespace G17Dragonriding;

        bool const wasBoosting = _boostTimer != 0;
        if (_boostTimer > diff)
            _boostTimer -= diff;
        else
            _boostTimer = 0;

        if (_boostTimer)
        {
            if (_boostTrailTimer > diff)
                _boostTrailTimer -= diff;
            else
            {
                _boostTrailTimer = BOOST_TRAIL_INTERVAL_MS;
                me->SendPlaySpellVisualKit(VISUAL_KIT_SPEED_TRAIL, 0);
            }
            // A second, slower pulse layers an alternating wind burst over the
            // ribbon trail so the boost has a visible body instead of a single
            // thin streamer.  Kits alternate between wind burst and an extra
            // ribbon pulse; both are already-audited one-shot visuals.
            if (_boostGustTimer > diff)
                _boostGustTimer -= diff;
            else
            {
                _boostGustTimer = BOOST_GUST_INTERVAL_MS;
                ++_boostGustPulseCount;
                if ((_boostGustPulseCount & 1u) != 0u)
                    me->SendPlaySpellVisualKit(VISUAL_KIT_WIND_BURST, 0);
                else
                    me->SendPlaySpellVisualKit(VISUAL_KIT_TRAIL_PULSE, 0);
            }
        }
        else if (wasBoosting)
        {
            _boostTrailTimer = 0;
            _boostGustTimer = 0;
            _boostGustPulseCount = 0;
            _boostTopSpeedAnnounced = false;
            // Shutdown feedback: two wind-burst/impact one-shots so the end of
            // the boost reads on screen instead of silently cutting off.
            me->SendPlaySpellVisualKit(VISUAL_KIT_WIND_BURST, 0);
            me->SendPlaySpellVisualKit(VISUAL_KIT_IMPACT_RING, 0);
            SendToRider(me,
                "|cff80ff80[G17-B2R2] 高速推进结束：结束风爆与冲击环已触发、持续尾流停止；保留当前动量并自然换档。|r");
        }

        _motionSampleAccumulator += diff;
        _speedPacketTimer += diff;

        // B3-R2: slow passive energy regen so a rider is never permanently
        // grounded; the fast paths are diving (+15) and the combat-page
        // generator slot (+8 per use).
        _energyRegenTimer += diff;
        if (_energyRegenTimer >= (_windStance
                ? G17Dragonriding::PASSIVE_ENERGY_REGEN_MS / 2u
                : G17Dragonriding::PASSIVE_ENERGY_REGEN_MS))
        {
            _energyRegenTimer = 0;
            if (me->GetPower(POWER_ENERGY) < me->GetMaxPower(POWER_ENERGY))
                me->ModifyPower(POWER_ENERGY, 1);
        }

        if (_motionSampleAccumulator < G17Dragonriding::MOTION_SAMPLE_INTERVAL_MS)
            return;

        float const dt = std::min(_motionSampleAccumulator, uint32(500)) / 1000.0f;
        _motionSampleAccumulator = 0;

        float const sampleX = me->GetPositionX();
        float const sampleY = me->GetPositionY();
        float const travelDx = sampleX - _lastSampleX;
        float const travelDy = sampleY - _lastSampleY;
        if (travelDx * travelDx + travelDy * travelDy > 0.04f)
        {
            float const measuredHeading = std::atan2(travelDy, travelDx);
            if (!_travelHeadingReady)
            {
                _smoothedTravelHeading = measuredHeading;
                _travelHeadingReady = true;
            }
            else
            {
                float const maxHeadingStep = 1.8f * dt;
                float const headingDelta = std::clamp(NormalizeRadians(
                    measuredHeading - _smoothedTravelHeading), -maxHeadingStep, maxHeadingStep);
                _smoothedTravelHeading = NormalizeRadians(_smoothedTravelHeading + headingDelta);
            }
            _lastSampleX = sampleX;
            _lastSampleY = sampleY;
        }

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
        float const acceleration = _targetSpeedRate > _currentSpeedRate ?
            (_boostTimer ? 2.8f : 2.2f) : 3.5f;
        float const maxStep = acceleration * dt;
        if (_currentSpeedRate < _targetSpeedRate)
            _currentSpeedRate = std::min(_targetSpeedRate, _currentSpeedRate + maxStep);
        else
            _currentSpeedRate = std::max(_targetSpeedRate, _currentSpeedRate - maxStep);
        _currentSpeedRate = std::clamp(_currentSpeedRate, 1.0f, FLIGHT_SPEED_RATES.back());

        if (_boostTimer && !_boostTopSpeedAnnounced && _currentSpeedRate >= 11.5f)
        {
            _boostTopSpeedAnnounced = true;
            // Top-speed punch: two wind-burst/impact one-shots so crossing into
            // the top tier is unmistakable, not just a chat line.
            me->SendPlaySpellVisualKit(VISUAL_KIT_WIND_BURST, 0);
            me->SendPlaySpellVisualKit(VISUAL_KIT_IMPACT_RING, 0);
            SendToRider(me,
                "|cffffff80[G17-B2R2] 已进入极速段：冲击环已触发，当前接近1200%硬上限，持续尾流仍在工作。|r");
        }

        if (_speedPacketTimer >= SPEED_PACKET_INTERVAL_MS)
        {
            _speedPacketTimer = 0;
            ApplyMovementRates(_currentSpeedRate, false);
        }
    }

    void UpdateSpeedTier()
    {
        using namespace G17Dragonriding;

        // B3-R2: the combat page caps the speed tier (design doc 7.2).
        size_t const maxTier = _combatPage ? COMBAT_PAGE_SPEED_TIER_CAP
                                           : FLIGHT_SPEED_RATES.size() - 1;

        while (_speedTier + 1 <= maxTier &&
            _momentum >= MOMENTUM_THRESHOLDS[_speedTier + 1] + 0.01f)
            ++_speedTier;
        while (_speedTier > maxTier ||
            (_speedTier > 0 && _momentum < MOMENTUM_THRESHOLDS[_speedTier] - 0.02f))
            --_speedTier;

        _targetSpeedRate = _stalling ? FLIGHT_SPEED_RATES.front() : FLIGHT_SPEED_RATES[_speedTier];
        if (_notifiedSpeedTier == _speedTier)
            return;

        _notifiedSpeedTier = _speedTier;
        if (_speedTier >= 4)
            me->SendPlaySpellVisualKit(VISUAL_KIT_SPEED_TRAIL, 0);
        if (Player* player = GetRider(me))
            if (WorldSession* session = player->GetSession())
                ChatHandler(session).PSendSysMessage(
                    "|cff80dfff[G17-B2R1] 动量档位 %u/7：目标飞行速度 %u%%（当前动量%u%%）。|r",
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
            "|cffffb040[G17-B2R1] 低动量失速：载具开始下坠；向前、俯冲或使用技能2即可恢复。|r");
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
            "|cff80ff80[G17-B2R1] 已从失速恢复：上升、下降、转向和前进控制全部恢复。|r");
    }

    // B2R2: The dash MUST follow the direction the player is actually facing at
    // cast time, not a stale smoothed travel heading.  Using the old
    // _smoothedTravelHeading (which lingers from the previous dash or from
    // momentum sampling) as the path baseline caused the vehicle to launch in an
    // old direction and slowly curve toward the current heading, producing the
    // user-reported "forced U-turn / 增强掉头" when the player had turned.  We
    // now anchor both the start tangent and the exit tangent to the rider's
    // current orientation so the dash is always a straight forward surge.
    float ResolveFacingHeading() const
    {
        if (Player const* rider = G17Dragonriding::GetRider(const_cast<Creature*>(me)))
            return rider->GetOrientation();
        return me->GetOrientation();
    }

    bool BuildClimbPath(float distance, Movement::PointsArray& path, float& exitHeading)
    {
        using namespace G17Dragonriding;

        float const startX = me->GetPositionX();
        float const startY = me->GetPositionY();
        float const startZ = me->GetPositionZ();
        // Anchor the whole dash to the facing at cast time.  There is no
        // residual turn blend because that is precisely what created the
        // fixed-direction curve/U-turn; the dash is a straight forward surge.
        float const facingHeading = ResolveFacingHeading();

        path.clear();
        path.reserve(8);
        // MoveSplineInit::Launch overwrites element zero with the current position.
        // Reserving it explicitly prevents the first real curve node being lost.
        path.emplace_back(startX, startY, startZ);

        float x = startX;
        float y = startY;
        constexpr uint32 nodeCount = 7;
        for (uint32 node = 1; node <= nodeCount; ++node)
        {
            float const t = float(node) / float(nodeCount);
            // Every node advances along the same facing heading; only altitude
            // is curved.  This guarantees no horizontal yaw at all, so the
            // vehicle can never turn away from where the player is looking.
            float const stepDistance = distance / float(nodeCount);
            x += std::cos(facingHeading) * stepDistance;
            y += std::sin(facingHeading) * stepDistance;
            float const z = startZ + CLIMB_HEIGHT * SmoothStep(t);
            if (!me->IsWithinLOS(x, y, z))
                return false;
            path.emplace_back(x, y, z);
        }

        exitHeading = facingHeading;
        return true;
    }

    void StartForwardClimb()
    {
        using namespace G17Dragonriding;

        if (_stalling)
            RecoverFromStall();

        // B2R3 anti-reverse: kill any residual motion BEFORE sampling the
        // dash path.  R2 built the path while the previous move (often a
        // backward brake from the momentum model) was still active and only
        // cleared server motion afterwards; the old spline was then finalized
        // at its back-most endpoint while the path still used pre-clear
        // coordinates, which the client rendered as a short reverse before
        // the forward surge.  Stopping first (StopMoving finalizes the
        // position from the ongoing spline) and sampling afterwards makes the
        // dash start from the real current spot with zero residual velocity.
        RestoreClientFlightControl(true);
        me->StopMoving();
        float exitHeading = ResolveFacingHeading();
        // CRITICAL: force the vehicle to face the dash direction BEFORE the
        // path is sampled and keep orientation fixed along it.
        me->SetFacingTo(exitHeading);
        me->SetOrientation(exitHeading);

        Movement::PointsArray path;
        float acceptedDistance = 0.0f;
        for (float distance : { CLIMB_FORWARD_DISTANCE, 12.0f, 7.0f })
        {
            if (BuildClimbPath(distance, path, exitHeading))
            {
                acceptedDistance = distance;
                break;
            }
        }

        if (acceptedDistance <= 0.0f)
        {
            SendToRider(me,
                "|cffffb040[G17-B2R3] 技能3冲刺路径被障碍阻挡：已取消且保持客户端飞行控制。|r");
            RestoreClientFlightControl(true);
            return;
        }

        _climbing = true;
        _climbControlTimer = CLIMB_CONTROL_TIMEOUT_MS;
        _climbExitHeading = exitHeading;
        _momentum = std::max(0.0f, _momentum - 0.18f);
        float const splineSpeed = std::clamp(16.0f + _currentSpeedRate * 1.35f,
            CLIMB_SPLINE_SPEED_MIN, CLIMB_SPLINE_SPEED_MAX);
        me->GetMotionMaster()->LaunchMoveSpline(
            [path, splineSpeed, exitHeading](Movement::MoveSplineInit& init)
            {
                init.MovebyPath(path);
                init.SetFly();
                init.SetVelocity(splineSpeed);
                init.SetFacing(exitHeading);
                init.SetOrientationFixed(true);
            }, POINT_CLIMB);
        SendToRider(me,
            "|cff80dfff[G17-B2R3] 技能3：沿当前朝向直线向前冲刺爬升，无额外转向、无倒车。|r");
    }

    void CompleteClimb(bool timedOut)
    {
        if (!_climbing)
            return;

        _climbing = false;
        _climbControlTimer = 0;
        _momentum = std::max(_momentum, 0.22f);
        _lastZ = me->GetPositionZ();
        _lastSampleX = me->GetPositionX();
        _lastSampleY = me->GetPositionY();
        _smoothedTravelHeading = _climbExitHeading;
        _travelHeadingReady = true;
        // A normal MovementInform means the spline already ended at its tangent;
        // do not Clear() and create a second orientation discontinuity. Timeout
        // remains fail-safe and clears the orphaned generator before handoff.
        RestoreClientFlightControl(timedOut);
        ApplyMovementRates(_currentSpeedRate, true);
        G17Dragonriding::SendToRider(me, timedOut
            ? "|cffffb040[G17-B2R2] 冲刺回调超时：已清除服务端运动并恢复全部控制。|r"
            : "|cff80ff80[G17-B2R2] 冲刺完成：朝向、姿态、速度与客户端控制已平滑交接。|r");
    }

    struct LandingProfile
    {
        float preferredDistance;
        float minimumDistance;
        float maximumDescent;
        float splineSpeed;
        uint32 nodeCount;
        bool orientationFixed;
    };

    LandingProfile GetLandingProfile() const
    {
        using namespace G17Dragonriding;

        // B3-R2d: faster, steeper, more coherent landings (user request:
        // "降落再快点，再有连贯性一点").  Descent per segment roughly doubled
        // and horizontal run shortened, so the glide angle steepens from ~22
        // degrees to ~40-45 degrees and segment count roughly halves; spline
        // speeds raised to match.  Profiles stay obstacle-guarded.
        switch (_archetype)
        {
            case ARCHETYPE_MAGIC:       return { 18.0f, 7.0f, 18.0f, 18.0f, 6, true };
            case ARCHETYPE_DRAGON:      return { 28.0f, 12.0f, 26.0f, 26.0f, 8, false };
            case ARCHETYPE_MECHANICAL:  return { 22.0f, 9.0f, 22.0f, 24.0f, 7, false };
            case ARCHETYPE_BEAST:       return { 14.0f, 6.0f, 14.0f, 20.0f, 6, false };
            default:                    return { 20.0f, 8.0f, 20.0f, 22.0f, 7, false };
        }
    }

    bool BuildLandingPath(LandingProfile const& profile, Movement::PointsArray& path,
        bool& finalSegment)
    {
        using namespace G17Dragonriding;

        float const startX = me->GetPositionX();
        float const startY = me->GetPositionY();
        float const startZ = me->GetPositionZ();
        float const baseHeading = me->GetOrientation();
        constexpr std::array<float, 5> headingOffsets = { 0.0f, -0.30f, 0.30f, -0.60f, 0.60f };

        for (float headingOffset : headingOffsets)
        {
            float const candidateHeading = NormalizeRadians(baseHeading + headingOffset);
            Position const collision = me->GetFirstCollisionPosition(
                profile.preferredDistance, headingOffset);
            float const collisionDx = collision.GetPositionX() - startX;
            float const collisionDy = collision.GetPositionY() - startY;
            float safeDistance = collisionDx * std::cos(candidateHeading) +
                collisionDy * std::sin(candidateHeading);
            if (safeDistance < profile.preferredDistance - 0.5f)
                safeDistance -= 1.5f;
            safeDistance = std::min(safeDistance, profile.preferredDistance);
            if (safeDistance < profile.minimumDistance)
                continue;

            std::vector<std::array<float, 2>> horizontalNodes;
            horizontalNodes.reserve(profile.nodeCount);
            float x = startX;
            float y = startY;
            for (uint32 node = 1; node <= profile.nodeCount; ++node)
            {
                float const t = float(node) / float(profile.nodeCount);
                float const heading = NormalizeRadians(baseHeading + headingOffset * SmoothStep(t));
                float const stepDistance = safeDistance / float(profile.nodeCount);
                x += std::cos(heading) * stepDistance;
                y += std::sin(heading) * stepDistance;
                horizontalNodes.push_back({ x, y });
            }

            float endpointGround = startZ;
            me->UpdateGroundPositionZ(horizontalNodes.back()[0], horizontalNodes.back()[1], endpointGround);
            if (endpointGround <= INVALID_HEIGHT + 1.0f || endpointGround > startZ + 4.0f)
                continue;

            float const groundEndpointZ = endpointGround + 0.55f;
            float const fullDrop = startZ - groundEndpointZ;
            finalSegment = fullDrop <= profile.maximumDescent + 1.0f;
            float const endpointZ = finalSegment ? groundEndpointZ :
                std::max(endpointGround + 3.0f, startZ - profile.maximumDescent);
            if (endpointZ >= startZ - 0.4f && !finalSegment)
                continue;

            path.clear();
            path.reserve(profile.nodeCount + 1);
            // The first element is intentionally reserved for Launch()'s current-position rewrite.
            path.emplace_back(startX, startY, startZ);
            bool safe = true;
            for (uint32 node = 1; node <= profile.nodeCount; ++node)
            {
                float const t = float(node) / float(profile.nodeCount);
                float z = startZ + (endpointZ - startZ) * SmoothStep(t);
                if (_archetype == ARCHETYPE_BEAST && finalSegment)
                    z += std::sin(PI * t) * 1.6f;

                float nodeGround = startZ;
                me->UpdateGroundPositionZ(horizontalNodes[node - 1][0], horizontalNodes[node - 1][1], nodeGround);
                float const clearance = finalSegment ? 0.55f + (1.0f - t) * 1.75f : 2.5f;
                if (nodeGround <= INVALID_HEIGHT + 1.0f || z < nodeGround + clearance ||
                    !me->IsWithinLOS(horizontalNodes[node - 1][0], horizontalNodes[node - 1][1], z))
                {
                    safe = false;
                    break;
                }
                path.emplace_back(horizontalNodes[node - 1][0], horizontalNodes[node - 1][1], z);
            }

            if (safe)
                return true;
        }

        return false;
    }

    void LaunchLandingPath(Movement::PointsArray const& path, LandingProfile const& profile,
        bool finalSegment)
    {
        uint32 const pointId = finalSegment ? G17Dragonriding::POINT_LAND :
            G17Dragonriding::POINT_LAND_APPROACH;
        me->GetMotionMaster()->LaunchMoveSpline(
            [path, profile](Movement::MoveSplineInit& init)
            {
                init.MovebyPath(path);
                init.SetFly();
                init.SetVelocity(profile.splineSpeed);
                init.SetOrientationFixed(profile.orientationFixed);
            }, pointId);
    }

    void StartTypedLanding()
    {
        using namespace G17Dragonriding;

        float const groundZ = me->GetFloorZ();
        if (groundZ <= INVALID_HEIGHT + 1.0f || groundZ > me->GetPositionZ() + 5.0f)
        {
            SendToRider(me,
                "|cffff4040[G17-B2R1] 当前下方没有可验证地面：已取消着陆且不施加任何缓落法术。|r");
            return;
        }

        _landing = true;
        _landingApproach = false;
        _climbing = false;
        _stalling = false;
        _boostTimer = 0;
        _boostTrailTimer = 0;
        _boostGustTimer = 0;
        _boostTopSpeedAnnounced = false;
        _landingTimer = LANDING_TIMEOUT_MS;
        _landingSegments = 0;
        _momentum = 0.0f;
        _targetSpeedRate = 1.0f;
        _currentSpeedRate = 1.0f;
        me->GetMotionMaster()->Clear(MOTION_SLOT_ACTIVE);
        me->RemoveAurasDueToSpell(SPELL_ACCELERATE);
        me->RemoveAurasDueToSpell(SPELL_SAFE_LANDING);
        RestoreClientFlightControl(false);
        ApplyMovementRates(1.0f, true);

        switch (_archetype)
        {
            case ARCHETYPE_MAGIC:
                SendToRider(me,
                    "|cffc080ff[G17-B2R1] 魔法/风系：保持平姿，以分段风托曲线向前卸除高度。|r");
                break;
            case ARCHETYPE_DRAGON:
                SendToRider(me,
                    "|cff80dfff[G17-B2R1] 龙系：进入长距离多段斜坡，末段自动拉平触地。|r");
                break;
            case ARCHETYPE_MECHANICAL:
                SendToRider(me,
                    "|cffffa040[G17-B2R1] 机械/火箭：带俯仰角前进，分段反推并平滑拉平。|r");
                break;
            case ARCHETYPE_BEAST:
                SendToRider(me,
                    "|cffffd080[G17-B2R1] 猛兽：先前向降高，末段无火焰跃落/扑落。|r");
                break;
            default:
                SendToRider(me,
                    "|cff80dfff[G17-B2R1] 通用：沿避障多点曲线向前着陆。|r");
                break;
        }

        StartLandingSegment();
    }

    void StartLandingSegment()
    {
        using namespace G17Dragonriding;

        if (!_landing)
            return;
        if (++_landingSegments > 12)
        {
            AbortLanding("着陆分段超过安全上限：已恢复飞行控制。", false);
            return;
        }

        LandingProfile const profile = GetLandingProfile();
        Movement::PointsArray path;
        bool finalSegment = false;
        if (!BuildLandingPath(profile, path, finalSegment))
        {
            AbortLanding("前向与两侧缓转路径均被地形/障碍阻挡：已恢复控制，不会撞墙直坠。", false);
            return;
        }

        _landingApproach = !finalSegment;
        if (_archetype == ARCHETYPE_MAGIC)
            me->SendPlaySpellVisualKit(VISUAL_KIT_WIND_BURST, 0);
        else if (_archetype == ARCHETYPE_MECHANICAL)
            me->SendPlaySpellVisualKit(VISUAL_KIT_MECHANICAL_THRUST, 0);
        else if (_archetype == ARCHETYPE_BEAST && finalSegment)
            me->SendPlaySpellVisualKit(VISUAL_KIT_BOOST_LAUNCH, 0);

        LaunchLandingPath(path, profile, finalSegment);
    }

    void AbortLanding(char const* reason, bool timedOut)
    {
        if (!_landing)
            return;

        _landing = false;
        _landingApproach = false;
        _landingTimer = 0;
        _landingSegments = 0;
        _momentum = std::max(_momentum, 0.18f);
        _targetSpeedRate = G17Dragonriding::FLIGHT_SPEED_RATES.front();
        _currentSpeedRate = std::max(_currentSpeedRate,
            G17Dragonriding::FLIGHT_SPEED_RATES.front());
        RestoreClientFlightControl(true);
        ApplyMovementRates(_currentSpeedRate, true);
        G17Dragonriding::SendToRider(me, timedOut
            ? "|cffffb040[G17-B2R1] 着陆超时：已清除运动、恢复重力隔离与全部玩家控制。|r"
            : reason);
    }

    void CompleteLanding()
    {
        using namespace G17Dragonriding;

        if (_archetype == ARCHETYPE_MAGIC)
            me->SendPlaySpellVisualKit(VISUAL_KIT_WIND_BURST, 0);
        else
            me->SendPlaySpellVisualKit(VISUAL_KIT_LANDING_DUST, 0);
        Player* player = GetRider(me);
        NormalizeVehicleForExit();
        if (player)
        {
            player->SetFallInformation(0, player->GetPositionZ());
            player->ExitVehicle();
        }
        me->DespawnOrUnsummon(500ms);
    }

    bool _landing = false;
    bool _landingApproach = false;
    bool _climbing = false;
    bool _stalling = false;
    bool _windStance = false; // B3-R12 御风姿态
    bool _boostTopSpeedAnnounced = false;
    bool _travelHeadingReady = false;
    bool _safetyCleanupStarted = false;
    uint32 _safetyCheckTimer = G17Dragonriding::SAFETY_CHECK_INTERVAL_MS;
    uint32 _motionSampleAccumulator = 0;
    uint32 _speedPacketTimer = 0;
    uint32 _climbControlTimer = 0;
    uint32 _boostTimer = 0;
    uint32 _boostTrailTimer = 0;
    uint32 _boostGustTimer = 0;
    uint32 _boostGustPulseCount = 0;
    uint32 _landingTimer = 0;
    uint32 _landingSegments = 0;
    uint32 _orphanGraceTimer = 3000;
    float _momentum = 0.10f;
    float _currentSpeedRate = G17Dragonriding::FLIGHT_SPEED_RATES.front();
    float _targetSpeedRate = G17Dragonriding::FLIGHT_SPEED_RATES.front();
    float _currentTurnRate = 1.0f;
    float _lastAppliedSpeedRate = 0.0f;
    float _lastZ = 0.0f;
    float _lastSampleX = 0.0f;
    float _lastSampleY = 0.0f;
    float _smoothedTravelHeading = 0.0f;
    float _climbExitHeading = 0.0f;
    size_t _speedTier = 0;
    size_t _notifiedSpeedTier = 0;
    uint32 _sourceSpell = 0;
    uint32 _sourceCreature = 0;
    uint32 _sourceDisplay = 0;
    uint32 _archetype = G17Dragonriding::ARCHETYPE_DRAGON;
    // B3-R2: current skill page (false = movement, true = archetype combat).
    bool _combatPage = false;
    uint32 _energyRegenTimer = 0;
    // B3-R4: mount assisted-strike timer.
    uint32 _autoCombatTimer = 0;
};

// B3-R2: the 25 combat carriers are cast BY THE VEHICLE from the combat page
// of the action bar.  CheckCast validates the G17 session, the rider context,
// battlegrounds, energy and the server-side cooldown; AfterCast executes the
// real effect with the RIDER as the attacking/healing unit.
class spell_g17_combat_skill : public SpellScript
{
    PrepareSpellScript(spell_g17_combat_skill);

    SpellCastResult CheckCast()
    {
        // B3-R3: dual caster — the vehicle-bar button (dragon caster) OR the
        // rider's own skill panel (player caster; allowed by Attributes 0x100).
        Creature* dragon = G17Dragonriding::ResolveDragonFromCaster(GetCaster());
        if (!dragon || !dragon->IsAIEnabled())
            return SPELL_FAILED_ERROR;

        Player* player = G17Dragonriding::GetRider(dragon);
        if (!player)
            return SPELL_FAILED_NOT_READY;

        // Default: BGs/arenas get NO G17 combat (design-doc balance rule).
        if (player->InBattleground() || player->InArena())
            return SPELL_FAILED_NOT_READY;

        // The combat page only carries the rider's own archetype set.
        uint32 const block = G17Dragonriding::CombatArchetype(GetSpellInfo()->Id);
        uint32 const arch = G17Dragonriding::CombatArchetypeToMount(block);
        if (dragon->AI()->GetData(G17Dragonriding::DATA_ARCHETYPE) != arch)
            return SPELL_FAILED_NOT_READY;

        uint32 const slot = G17Dragonriding::CombatSlot(GetSpellInfo()->Id);

        // Slot 0 is the generator: free and RESTORES energy.
        if (slot != 0)
        {
            uint32 const cost = (slot == 4) ? G17Dragonriding::COMBAT_BURST_ENERGY
                                            : G17Dragonriding::COMBAT_ENERGY_COST;
            if (dragon->GetPower(POWER_ENERGY) < cost)
                return SPELL_FAILED_NO_POWER;
        }

        // B3-R12: pre-validate the target so a targetless press FAILS the
        // cast cleanly (SPELL_FAILED_BAD_TARGETS, nothing consumed) instead
        // of executing, printing the chat hint and silently eating the core
        // GCD - the user-reported hidden lockout ("not ready" with no UI
        // swirl; the core applies its GCD to any COMPLETED cast, even one
        // whose G17 effects early-returned).  The B3-R6b removal was about a
        // naive GetUnitTarget()-only check; this mirrors ExecuteCombatSkill's
        // FULL resolution chain: explicit target -> rider selection -> dragon
        // victim.  Triggered casts (the auto-combat generator, which carries
        // the dragon's victim) are exempt.
        if (!GetSpell()->IsTriggered() && slot != 2)
        {
            Unit* target = GetSpell()->m_targets.GetUnitTarget();
            if (!target || target == dragon)
                target = player->GetSelectedUnit();
            if (!target)
                target = dragon->GetVictim();
            if (target == player || (target && target->IsFriendlyTo(player)))
                target = nullptr;
            if (!target)
                return SPELL_FAILED_BAD_TARGETS;
        }

        // Server-side cooldown gate (DBC records carry no cooldown).  The
        // cooldown is kept on BOTH histories so the vehicle bar AND the
        // rider's skill panel grey out together and neither path can bypass
        // the other.
        if (!dragon->GetSpellHistory()->IsReady(GetSpellInfo()) ||
            !player->GetSpellHistory()->IsReady(GetSpellInfo()))
            return SPELL_FAILED_NOT_READY;

        return SPELL_CAST_OK;
    }

    void HandleDummy(SpellEffIndex effectIndex)
    {
        // The DBC carrier is a visual-only dummy; all real behavior is ours.
        if (G17Dragonriding::IsDragon(GetCaster()))
            PreventHitDefaultEffect(effectIndex);
    }

    void Execute()
    {
        Creature* dragon = G17Dragonriding::ResolveDragonFromCaster(GetCaster());
        if (dragon)
            G17Dragonriding::ExecuteCombatSkill(dragon, GetSpell());
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_g17_combat_skill::CheckCast);
        OnEffectHit += SpellEffectFn(spell_g17_combat_skill::HandleDummy, EFFECT_0, SPELL_EFFECT_ANY);
        AfterCast += SpellCastFn(spell_g17_combat_skill::Execute);
    }
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
            if (dragon->IsAIEnabled())
            {
                if (dragon->AI()->GetData(G17Dragonriding::DATA_FLIGHT_STATE) ==
                    G17Dragonriding::FLIGHT_STATE_LANDING ||
                    dragon->AI()->GetData(G17Dragonriding::DATA_BOOST_ACTIVE))
                    return SPELL_FAILED_NOT_READY;
            }
        return SPELL_CAST_OK;
    }

    void HandleBoost(SpellEffIndex effectIndex)
    {
        TC_LOG_INFO("scripts.g17.dragonriding",
            "G17-B2R2 SKILL2 BOOST HandleBoost fired: caster={} isDragon={}",
            GetCaster() ? GetCaster()->GetGUID().ToString() : "null",
            G17Dragonriding::IsDragon(GetCaster()) ? 1 : 0);
        // B3-R9: accept BOTH casters - the vehicle bar (dragon) and the
        // player-cast buttons of the custom G17 DragonRide UI.
        Creature* dragon = G17Dragonriding::ResolveDragonFromCaster(GetCaster());
        if (!dragon)
            return;

        // Suppress Aura 210 so DBC speed cannot multiply B2 above 1200%.
        PreventHitDefaultEffect(effectIndex);
        if (dragon->IsAIEnabled())
            dragon->AI()->DoAction(G17Dragonriding::ACTION_ACCELERATE);
    }

    void ConsumeEnergy()
    {
        // B3-R9: dual-caster - energy always drains on the dragon.
        if (Creature* dragon = G17Dragonriding::ResolveDragonFromCaster(GetCaster()))
            dragon->ModifyPower(POWER_ENERGY, -int32(G17Dragonriding::BOOST_ENERGY_COST));
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
        // B3-R9: dual-caster - resolve from either the dragon or the player.
        if (Creature* dragon = G17Dragonriding::ResolveDragonFromCaster(GetCaster()))
            if (dragon->IsAIEnabled() &&
                dragon->AI()->GetData(G17Dragonriding::DATA_FLIGHT_STATE) == G17Dragonriding::FLIGHT_STATE_CLIMBING)
                dragon->ModifyPower(POWER_ENERGY, -int32(G17Dragonriding::CLIMB_ENERGY_COST));
    }

    void HandleJump(SpellEffIndex effectIndex)
    {
        TC_LOG_INFO("scripts.g17.dragonriding",
            "G17-B2R2 SKILL3 DASH HandleJump fired: caster={} isDragon={}",
            GetCaster() ? GetCaster()->GetGUID().ToString() : "null",
            G17Dragonriding::IsDragon(GetCaster()) ? 1 : 0);
        // B3-R9: dual-caster (vehicle bar OR player-cast UI button).
        Creature* dragon = G17Dragonriding::ResolveDragonFromCaster(GetCaster());
        if (!dragon)
            return;

        PreventHitDefaultEffect(effectIndex);
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

    // B2R2: 52226 "飞行器着陆" is the real quest-item/vehicle landing command,
    // not a no-op.  Its DBC dummy effect can carry cast conditions (quest/item
    // requirements) that reject the cast when the caster is the G17 dragon,
    // which is why skill 4 "could not be used on any mount" after B2R1.  We
    // explicitly allow the cast while on a G17 dragon, suppress the default
    // dummy effect so the quest behavior never fires in this context, and
    // trigger landing from both OnEffectHit and AfterCast so it cannot be
    // skipped by a single hook ordering issue.
    SpellCastResult CheckCast()
    {
        Unit* caster = GetCaster();
        if (!caster)
            return SPELL_FAILED_ERROR;

        // Always allow the landing command while seated on a G17 vehicle, even
        // though the dragon is not the original flying machine the quest spell
        // was authored for.  This overrides the quest/item cast conditions
        // that were blocking skill 4 on every mount.
        if (G17Dragonriding::IsDragon(caster))
            return SPELL_CAST_OK;

        // Outside a G17 dragon, do not interfere with the native spell at all.
        return SPELL_CAST_OK;
    }

    void StartLanding(SpellEffIndex effectIndex)
    {
        TC_LOG_INFO("scripts.g17.dragonriding",
            "G17-B2R2 SKILL4 LAND OnEffectHit fired: caster={} isDragon={}",
            GetCaster() ? GetCaster()->GetGUID().ToString() : "null",
            G17Dragonriding::IsDragon(GetCaster()) ? 1 : 0);
        // B3-R9: dual-caster (vehicle bar OR player-cast UI button).
        Creature* dragon = G17Dragonriding::ResolveDragonFromCaster(GetCaster());
        if (!dragon)
            return;

        // Suppress the native dummy effect so only the guarded G17 vehicle
        // landing state machine acts for this cast.
        PreventHitDefaultEffect(effectIndex);
        if (dragon->IsAIEnabled())
            dragon->AI()->DoAction(G17Dragonriding::ACTION_LAND);
    }

    void EnsureLanding()
    {
        // AfterCast fallback: guarantees the landing starts even if the dummy
        // effect is skipped by core effect processing.  ACTION_LAND is
        // idempotent in the AI (guarded by _landing), so a double call is safe.
        if (Creature* dragon = GetCaster()->ToCreature())
        {
            TC_LOG_INFO("scripts.g17.dragonriding",
                "G17-B2R2 SKILL4 LAND AfterCast fired: isDragon={} aiEnabled={}",
                G17Dragonriding::IsDragon(dragon) ? 1 : 0, dragon->IsAIEnabled() ? 1 : 0);
            if (G17Dragonriding::IsDragon(dragon) && dragon->IsAIEnabled())
                dragon->AI()->DoAction(G17Dragonriding::ACTION_LAND);
        }
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_g17_dragon_safe_landing::CheckCast);
        OnEffectHit += SpellEffectFn(spell_g17_dragon_safe_landing::StartLanding,
            EFFECT_0, SPELL_EFFECT_ANY);
        AfterCast += SpellCastFn(spell_g17_dragon_safe_landing::EnsureLanding);
    }
};

// ---- B3-R2 bar-button SpellScripts (cast by the vehicle, like the four
// proven B2 movement skills).  Each is a visual-only DBC dummy; the real
// behavior goes through the vehicle AI via DoAction. ----

// ---- B3-R12 skill variety #1: 突袭·俯冲打击 (990029, combat slot 7) ----
// The first AREA skill: instant damage to every enemy within SWOOP_RADIUS of
// the target (skills 1-5 are single-target).  Prototyped on War Stomp (45)
// client-side (icon 50 / visual 2355).  Target pre-validated in CheckCast so
// a targetless press fails cleanly (no hidden GCD lockout - see the combat
// skill's B3-R12 note).
class spell_g17_swoop_strike : public SpellScript
{
    PrepareSpellScript(spell_g17_swoop_strike);

    SpellCastResult CheckCast()
    {
        Creature* dragon = G17Dragonriding::ResolveDragonFromCaster(GetCaster());
        if (!dragon || !dragon->IsAIEnabled())
            return SPELL_FAILED_ERROR;
        Player* player = G17Dragonriding::GetRider(dragon);
        if (!player)
            return SPELL_FAILED_NOT_READY;
        if (player->InBattleground() || player->InArena())
            return SPELL_FAILED_NOT_READY;

        if (!GetSpell()->IsTriggered())
        {
            Unit* target = GetSpell()->m_targets.GetUnitTarget();
            if (!target || target == dragon)
                target = player->GetSelectedUnit();
            if (!target)
                target = dragon->GetVictim();
            if (target == player || (target && target->IsFriendlyTo(player)))
                target = nullptr;
            if (!target)
                return SPELL_FAILED_BAD_TARGETS;
            if (target->GetMap() != player->GetMap() ||
                !player->IsWithinDist(target, G17Dragonriding::COMBAT_MAX_RANGE) ||
                !player->IsWithinLOS(target->GetPositionX(), target->GetPositionY(), target->GetPositionZ()))
                return SPELL_FAILED_OUT_OF_RANGE;
        }

        if (!dragon->GetSpellHistory()->IsReady(GetSpellInfo()))
            return SPELL_FAILED_NOT_READY;
        if (dragon->GetPower(POWER_ENERGY) < G17Dragonriding::SWOOP_ENERGY)
            return SPELL_FAILED_NO_POWER;
        return SPELL_CAST_OK;
    }

    void HandleStrike(SpellEffIndex effectIndex)
    {
        Creature* dragon = G17Dragonriding::ResolveDragonFromCaster(GetCaster());
        if (!dragon || !dragon->IsAIEnabled())
            return;
        Player* player = G17Dragonriding::GetRider(dragon);
        if (!player)
            return;
        PreventHitDefaultEffect(effectIndex);

        Unit* target = GetSpell()->m_targets.GetUnitTarget();
        if (!target || target == dragon)
            target = player->GetSelectedUnit();
        if (!target)
            target = dragon->GetVictim();
        if (!target || target == player || target->IsFriendlyTo(player))
            return;

        dragon->ModifyPower(POWER_ENERGY, -int32(G17Dragonriding::SWOOP_ENERGY));
        dragon->GetSpellHistory()->AddCooldown(
            G17Dragonriding::SPELL_SWOOP_STRIKE, 0, Milliseconds(G17Dragonriding::SWOOP_CD_MS));
        player->GetSpellHistory()->AddCooldown(
            G17Dragonriding::SPELL_SWOOP_STRIKE, 0, Milliseconds(G17Dragonriding::SWOOP_CD_MS));
        G17Dragonriding::SendVehicleCooldownPackets(
            player, dragon, G17Dragonriding::SPELL_SWOOP_STRIKE, G17Dragonriding::SWOOP_CD_MS);

        // AoE burst around the target; attacker = rider for credit.
        uint32 const amount = (G17Dragonriding::SWOOP_BASE_DAMAGE +
            player->GetLevel() * G17Dragonriding::SWOOP_DMG_PER_LEVEL);
        std::list<Unit*> hits;
        // Fork signature (GridNotifiers.h:962): (WorldObject const* obj,
        // Unit const* funit, float range) - obj = range center, funit = the
        // faction reference.  Center the burst on the TARGET; faction = the
        // rider's.  (The real C2661 was my 2-arg call against this 3-arg ctor.)
        Trinity::AnyUnfriendlyUnitInObjectRangeCheck check(target, player, G17Dragonriding::SWOOP_RADIUS);
        Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(player, hits, check);
        Cell::VisitAllObjects(target, searcher, G17Dragonriding::SWOOP_RADIUS);
        uint32 count = 0;
        for (Unit* hit : hits)
        {
            if (!hit || !hit->IsAlive() || hit == player || hit == dragon)
                continue;
            Unit::DealDamage(player, hit, amount, nullptr, DIRECT_DAMAGE,
                SPELL_SCHOOL_MASK_NORMAL, GetSpellInfo(), true);
            hit->SendPlaySpellVisualKit(G17Dragonriding::VISUAL_KIT_IMPACT_RING, 0);
            ++count;
        }
        dragon->SendMeleeAttackStart(target);
        dragon->SendMeleeAttackStop(target);

        if (WorldSession* session = player->GetSession())
            ChatHandler(session).PSendSysMessage(
                "|cff80dfff[G17] 突袭·俯冲打击：%u 点范围伤害（%u 个目标）。|r", amount, count);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_g17_swoop_strike::CheckCast);
        OnEffectHit += SpellEffectFn(spell_g17_swoop_strike::HandleStrike, EFFECT_0, SPELL_EFFECT_ANY);
    }
};

// ---- B3-R12 skill variety #2: 御风姿态 (990030, movement slot 7) ----
// Stance toggle: +15% turn rate and doubled passive energy regen while
// active; the AI owns the state (ACTION_WIND_STANCE) and prints on/off.
// Prototyped on Aspect of the Cheetah (5118) client-side (icon 1181 /
// visual 3719).  Self-cast: no target required.
class spell_g17_wind_stance : public SpellScript
{
    PrepareSpellScript(spell_g17_wind_stance);

    SpellCastResult CheckCast()
    {
        Creature* dragon = G17Dragonriding::ResolveDragonFromCaster(GetCaster());
        if (!dragon || !dragon->IsAIEnabled())
            return SPELL_FAILED_ERROR;
        Player* player = G17Dragonriding::GetRider(dragon);
        if (!player)
            return SPELL_FAILED_NOT_READY;
        if (player->InBattleground() || player->InArena())
            return SPELL_FAILED_NOT_READY;
        if (!dragon->GetSpellHistory()->IsReady(GetSpellInfo()))
            return SPELL_FAILED_NOT_READY;
        return SPELL_CAST_OK;
    }

    void HandleToggle(SpellEffIndex effectIndex)
    {
        Creature* dragon = G17Dragonriding::ResolveDragonFromCaster(GetCaster());
        if (!dragon || !dragon->IsAIEnabled())
            return;
        PreventHitDefaultEffect(effectIndex);

        dragon->GetSpellHistory()->AddCooldown(
            G17Dragonriding::SPELL_WIND_STANCE, 0, Milliseconds(G17Dragonriding::WIND_STANCE_CD_MS));
        if (Player* player = G17Dragonriding::GetRider(dragon))
        {
            player->GetSpellHistory()->AddCooldown(
                G17Dragonriding::SPELL_WIND_STANCE, 0, Milliseconds(G17Dragonriding::WIND_STANCE_CD_MS));
            G17Dragonriding::SendVehicleCooldownPackets(
                player, dragon, G17Dragonriding::SPELL_WIND_STANCE, G17Dragonriding::WIND_STANCE_CD_MS);
        }
        dragon->AI()->DoAction(G17Dragonriding::ACTION_WIND_STANCE);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_g17_wind_stance::CheckCast);
        OnEffectHit += SpellEffectFn(spell_g17_wind_stance::HandleToggle, EFFECT_0, SPELL_EFFECT_ANY);
    }
};

// 切换技能页: swaps the vehicle action bar between movement and combat pages.
class spell_g17_page_switch : public SpellScript
{
    PrepareSpellScript(spell_g17_page_switch);

    SpellCastResult CheckCast()
    {
        Creature* dragon = G17Dragonriding::ResolveDragonFromCaster(GetCaster());
        if (!dragon || !dragon->IsAIEnabled())
            return SPELL_FAILED_ERROR;
        if (!G17Dragonriding::GetRider(dragon))
            return SPELL_FAILED_NOT_READY;
        // Internal short cooldown so the bar cannot be flicker-switched.
        if (!dragon->GetSpellHistory()->IsReady(GetSpellInfo()))
            return SPELL_FAILED_NOT_READY;
        return SPELL_CAST_OK;
    }

    void HandleDummy(SpellEffIndex effectIndex)
    {
        if (G17Dragonriding::IsDragon(GetCaster()))
            PreventHitDefaultEffect(effectIndex);
    }

    void Switch()
    {
        Creature* dragon = G17Dragonriding::ResolveDragonFromCaster(GetCaster());
        if (!dragon || !dragon->IsAIEnabled())
            return;
        dragon->GetSpellHistory()->AddCooldown(
            G17Dragonriding::SPELL_PAGE_SWITCH, 0, Milliseconds(G17Dragonriding::PAGE_SWITCH_CD_MS));
        if (Player* switchRider = G17Dragonriding::GetRider(dragon))
            G17Dragonriding::SendVehicleCooldownPackets(
                switchRider, dragon, G17Dragonriding::SPELL_PAGE_SWITCH, G17Dragonriding::PAGE_SWITCH_CD_MS);
        dragon->AI()->DoAction(G17Dragonriding::ACTION_PAGE_SWITCH);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_g17_page_switch::CheckCast);
        OnEffectHit += SpellEffectFn(spell_g17_page_switch::HandleDummy, EFFECT_0, SPELL_EFFECT_ANY);
        AfterCast += SpellCastFn(spell_g17_page_switch::Switch);
    }
};

// 拉升: powered steep climb; costs energy (checked here, consumed in AfterCast
// only when the maneuver actually started, mirroring the proven climb script).
class spell_g17_ascend : public SpellScript
{
    PrepareSpellScript(spell_g17_ascend);

    SpellCastResult CheckCast()
    {
        SpellCastResult const result = G17Dragonriding::CheckEnergyCast(
            GetCaster(), G17Dragonriding::ASCEND_ENERGY_COST);
        if (result != SPELL_CAST_OK)
            return result;

        if (Creature* dragon = GetCaster() ? GetCaster()->ToCreature() : nullptr)
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
        if (Creature* dragon = G17Dragonriding::ResolveDragonFromCaster(GetCaster()))
            if (dragon->IsAIEnabled() &&
                dragon->AI()->GetData(G17Dragonriding::DATA_FLIGHT_STATE) == G17Dragonriding::FLIGHT_STATE_CLIMBING)
                dragon->ModifyPower(POWER_ENERGY, -int32(G17Dragonriding::ASCEND_ENERGY_COST));
    }

    void HandleAscend(SpellEffIndex effectIndex)
    {
        Creature* dragon = G17Dragonriding::ResolveDragonFromCaster(GetCaster());
        if (!dragon || !dragon->IsAIEnabled())
            return;
        PreventHitDefaultEffect(effectIndex);
        dragon->AI()->DoAction(G17Dragonriding::ACTION_ASCEND);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_g17_ascend::CheckCast);
        OnEffectHit += SpellEffectFn(spell_g17_ascend::HandleAscend, EFFECT_0, SPELL_EFFECT_ANY);
        AfterCast += SpellCastFn(spell_g17_ascend::ConsumeEnergy);
    }
};

// 俯冲: powered dive; costs NO energy (it is the energy-recovery loop).
class spell_g17_dive : public SpellScript
{
    PrepareSpellScript(spell_g17_dive);

    SpellCastResult CheckCast()
    {
        Creature* dragon = G17Dragonriding::ResolveDragonFromCaster(GetCaster());
        if (!dragon || !dragon->IsAIEnabled())
            return SPELL_FAILED_ERROR;
        if (!G17Dragonriding::GetRider(dragon))
            return SPELL_FAILED_NOT_READY;

        uint32 const state = dragon->AI()->GetData(G17Dragonriding::DATA_FLIGHT_STATE);
        if (state == G17Dragonriding::FLIGHT_STATE_CLIMBING ||
            state == G17Dragonriding::FLIGHT_STATE_LANDING)
            return SPELL_FAILED_NOT_READY;
        return SPELL_CAST_OK;
    }

    void HandleDive(SpellEffIndex effectIndex)
    {
        Creature* dragon = G17Dragonriding::ResolveDragonFromCaster(GetCaster());
        if (!dragon || !dragon->IsAIEnabled())
            return;
        PreventHitDefaultEffect(effectIndex);
        dragon->AI()->DoAction(G17Dragonriding::ACTION_DIVE);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_g17_dive::CheckCast);
        OnEffectHit += SpellEffectFn(spell_g17_dive::HandleDive, EFFECT_0, SPELL_EFFECT_ANY);
    }
};

// 滑翔/制动: instant air brake, free.
class spell_g17_glide_brake : public SpellScript
{
    PrepareSpellScript(spell_g17_glide_brake);

    SpellCastResult CheckCast()
    {
        Creature* dragon = G17Dragonriding::ResolveDragonFromCaster(GetCaster());
        if (!dragon || !dragon->IsAIEnabled())
            return SPELL_FAILED_ERROR;
        if (!G17Dragonriding::GetRider(dragon))
            return SPELL_FAILED_NOT_READY;

        uint32 const state = dragon->AI()->GetData(G17Dragonriding::DATA_FLIGHT_STATE);
        if (state == G17Dragonriding::FLIGHT_STATE_CLIMBING ||
            state == G17Dragonriding::FLIGHT_STATE_LANDING)
            return SPELL_FAILED_NOT_READY;
        return SPELL_CAST_OK;
    }

    void HandleBrake(SpellEffIndex effectIndex)
    {
        Creature* dragon = G17Dragonriding::ResolveDragonFromCaster(GetCaster());
        if (!dragon || !dragon->IsAIEnabled())
            return;
        PreventHitDefaultEffect(effectIndex);
        dragon->AI()->DoAction(G17Dragonriding::ACTION_GLIDE_BRAKE);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_g17_glide_brake::CheckCast);
        OnEffectHit += SpellEffectFn(spell_g17_glide_brake::HandleBrake, EFFECT_0, SPELL_EFFECT_ANY);
    }
};

class g17_dragonriding_playerscript : public PlayerScript
{
public:
    g17_dragonriding_playerscript() : PlayerScript("g17_dragonriding_playerscript") { }

    // B3-R2 migration: B3-R1 leaked the 25 carriers into player spellbooks
    // (they were unusable there while riding).  Any copy that survived a
    // logout mid-flight is cleaned up at login; the skills live ONLY on the
    // vehicle action bar now.
    // B3-R2c fix: fork's PlayerScript::OnLogin is OnLogin(Player*, bool).
    void OnLogin(Player* player, bool /*firstLogin*/) override
    {
        if (player)
            G17Dragonriding::RevokeCombatSkills(player);
    }

    void OnSpellCast(Player* player, Spell* spell, bool /*skipCheck*/) override
    {
        if (!player || !spell)
            return;

        if (!G17Dragonriding::IsAutoEnabled(player) || player->IsMounted() ||
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
                ChatHandler(session).SendSysMessage("|cffff8040[G17-B2R1] 已进入禁飞/城市/室内/副本区域，御龙载具已安全清理。|r");
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
            handler->SendSysMessage("|cffff4040[G17-B2R1] 已在普通坐骑上；请直接重新点击已学坐骑让自动接管工作。|r");
            return true;
        }

        if (!G17Dragonriding::SpawnTypedVehicle(player, 0, 0, 0,
            G17Dragonriding::ARCHETYPE_DRAGON, handler))
            handler->SendSysMessage("|cffff4040[G17-B2R1] 当前状态或地点不允许建立御龙载具。|r");
        return true;
    }

    static bool HandleMount(ChatHandler* handler, uint32 spellId)
    {
        Player* player = handler->GetPlayer();
        SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
        if (!player || !info || !G17Dragonriding::HasMountAuraMetadata(info) || !player->HasSpell(spellId))
        {
            handler->SendSysMessage("|cffff4040[G17-B2R1] 必须提供自己已经学会的坐骑法术ID（支持包装法术）。|r");
            return true;
        }

        G17Dragonriding::SetAutoEnabled(player, true);
        player->CastSpell(player, spellId, false);
        handler->PSendSysMessage("|cff80dfff[G17-B2R1] 已施放拥有的坐骑法术%u；成功上马后会自动接管。|r", spellId);
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
            handler->PSendSysMessage("|cffffff00[G17-B2R1] auto=%s；用法：.dragon auto on|off|r",
                G17Dragonriding::IsAutoEnabled(player) ? "on" : "off");
            return true;
        }

        handler->PSendSysMessage("|cff80dfff[G17-B2R1] 全坐骑自动接管已%s。|r",
            G17Dragonriding::IsAutoEnabled(player) ? "开启" : "关闭");
        return true;
    }

    static bool HandleDismiss(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (!G17Dragonriding::GetDragon(player))
        {
            handler->SendSysMessage("|cffffff00[G17-B2R1] 当前没有御龙载具。|r");
            return true;
        }

        G17Dragonriding::CleanupPlayer(player, true);
        handler->SendSysMessage("|cff80dfff[G17-B2R1] 御龙载具已安全解除。|r");
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
            handler->PSendSysMessage("|cff80dfff[G17-B2R1] ACTIVE sourceSpell=%u sourceCreature=%u display=%u type=%s vehicle=%u seat=%d movementSeat=%d controlled=%s energy=%u/%u momentum=%u%% speed=%u%% state=%u auto=%s map=%u zone=%u area=%u|r",
                sourceSpell, sourceCreature, sourceDisplay, G17Dragonriding::ArchetypeName(archetype),
                vehicle ? vehicle->GetVehicleInfo()->ID : 0,
                int32(G17Dragonriding::GetAuthoritativePassengerSeatId(vehicle, player)), int32(player->GetTransSeat()),
                dragon->GetCharmerGUID() == player->GetGUID() ? "true" : "false",
                dragon->GetPower(POWER_ENERGY), dragon->GetMaxPower(POWER_ENERGY), momentum, speed,
                flightState, G17Dragonriding::IsAutoEnabled(player) ? "on" : "off",
                player->GetMapId(), player->GetZoneId(), player->GetAreaId());
        }
        else
            handler->PSendSysMessage("|cffffff00[G17-B2R1] INACTIVE auto=%s area_allowed=%s|r",
                G17Dragonriding::IsAutoEnabled(player) ? "on" : "off",
                G17Dragonriding::IsBlockedArea(player) ? "false" : "true");
        return true;
    }

    static bool HandleHelp(ChatHandler* handler)
    {
        handler->SendSysMessage("|cff80dfff===== G17-B2 完整御空术 =====|r");
        handler->SendSysMessage("默认auto=on：直接点击已学坐骑；直接法术和包装/内层坐骑都会保留外观并自动接管。");
        handler->SendSysMessage("七档动量速度250%-1200%：前进和俯冲蓄势，拉升、转向、后退制动和停滞消耗动量；速度平滑变化且不能无限叠加。");
        handler->SendSysMessage("低动量会进入可恢复失速；向前、俯冲或技能2恢复。技能3以限角Catmull-Rom曲线向前上方爬升，末端切线平滑交还控制。");
        handler->SendSysMessage("技能4使用无视觉Dummy触发器，绝不添加降落伞；魔法平姿、猛兽无火跃落、机械反推、龙类长斜坡均为多点路径。");
        handler->SendSysMessage(".dragon auto on|off；.dragon mount <spellId>；.dragon summon/dismiss/status");
        handler->SendSysMessage("室内、禁区、死亡、离车、换图和异常中断均归一化Vehicle、重力、飞行姿态与全部临时速度。|r");
        return true;
    }};

void AddSC_dragonriding_commandscript()
{
    // B2R3: clear the DBC cast gates of 52226 before any player can press it.
    G17Dragonriding::EnsureLandingCommandCastable();

    // B3-R9: the custom G17 DragonRide UI casts the three real movement
    // skills as the PLAYER.  52226 already carries CASTABLE_WHILE_MOUNTED
    // (DBC Attributes 0x100); 55215/52197 do not, so the fork's
    // SpellInfo::CheckVehicle() would reject the player cast before any
    // script hook runs.  Set the attribute at runtime with the proven
    // B2R3 const_cast pattern (server DBC stays untouched on disk).
    for (uint32 movementSpell : { G17Dragonriding::SPELL_ACCELERATE, G17Dragonriding::SPELL_CLIMB })
    {
        if (SpellInfo const* raw = sSpellMgr->GetSpellInfo(movementSpell))
        {
            SpellInfo* info = const_cast<SpellInfo*>(raw);
            info->Attributes |= SPELL_ATTR0_CASTABLE_WHILE_MOUNTED;
            TC_LOG_INFO("server.loading",
                ">> G17-B3R9 movement spell %u Attributes |= CASTABLE_WHILE_MOUNTED (player-cast UI path)", movementSpell);
        }
    }

    // B2R3 proof-of-load banner. Logged to the core "server" channel which is
    // always active, so it cannot be hidden by appender filtering. If this
    // block is absent from worldserver.log at startup, the running exe is old.
    TC_LOG_INFO("server.loading", " ");
    TC_LOG_INFO("server.loading", ">> G17-B3R12 dragonriding LOADED  build=20260827-r12a (target pre-validation + swoop 990029 + stance 990030; AoE check ctor fixed)");
    TC_LOG_INFO("server.loading", "   skill2=layered audited visual kits | skill3=facing-locked dash w/o reverse | skill4=52226 sanitized cast");
    TC_LOG_INFO("server.loading", " ");

    new dragonriding_commandscript();
    new g17_dragonriding_playerscript();
    RegisterCreatureAI(npc_g17_dragonriding_vehicle);
    RegisterSpellScript(spell_g17_dragon_breath_energy);
    RegisterSpellScript(spell_g17_dragon_accelerate_energy);
    RegisterSpellScript(spell_g17_dragon_climb);
    RegisterSpellScript(spell_g17_dragon_safe_landing);
    RegisterSpellScript(spell_g17_combat_skill);
    RegisterSpellScript(spell_g17_swoop_strike);   // B3-R12
    RegisterSpellScript(spell_g17_wind_stance);    // B3-R12
    RegisterSpellScript(spell_g17_page_switch);
    RegisterSpellScript(spell_g17_ascend);
    RegisterSpellScript(spell_g17_dive);
    RegisterSpellScript(spell_g17_glide_brake);

    TC_LOG_INFO("server.loading",
        ">> G17-B3R6 performance fix LOADED: no LearnSpell on mount (lag fix), target validation in CheckCast (no cooldown without target), 6-button vehicle bar + page switching + C8 per-slot visuals");
}
