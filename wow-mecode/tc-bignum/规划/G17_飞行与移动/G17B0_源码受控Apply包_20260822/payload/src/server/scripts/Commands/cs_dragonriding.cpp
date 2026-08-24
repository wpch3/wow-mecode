/*
 * G17-B0: native controllable dragon vehicle for TrinityCore 3.3.5a.
 *
 * Scope is intentionally B0: a native vehicle seat/action bar, server-side
 * energy, four discrete abilities, and defensive lifecycle cleanup.  This is
 * not the B2 momentum/gliding/client-prediction implementation.
 */

#include "ScriptMgr.h"
#include "Chat.h"
#include "CombatAI.h"
#include "DBCStores.h"
#include "GridDefines.h"
#include "Map.h"
#include "MotionMaster.h"
#include "Player.h"
#include "RBAC.h"
#include "ScriptedCreature.h"
#include "SpellInfo.h"
#include "SpellScript.h"
#include "TemporarySummon.h"
#include "Vehicle.h"
#include "WorldSession.h"

using namespace Trinity::ChatCommands;

namespace G17Dragonriding
{
constexpr uint32 NPC_DRAGONRIDING_VEHICLE = 1000171;

constexpr uint32 SPELL_DRAGON_BREATH = 9573;  // Flame Breath
constexpr uint32 SPELL_ACCELERATE     = 55215; // Burst of Speed: +100% flight speed for 10 sec
constexpr uint32 SPELL_CLIMB          = 52197; // Jump Jets: intercepted only for this vehicle
constexpr uint32 SPELL_SAFE_LANDING   = 53208; // Parachute: also starts controlled landing
constexpr uint32 SPELL_FALL_SAFETY    = 53208;

constexpr int32 ACTION_CLIMB = 1;
constexpr int32 ACTION_LAND  = 2;
constexpr uint32 POINT_CLIMB = 17001;
constexpr uint32 POINT_LAND  = 17002;

constexpr int32 BREATH_ENERGY_COST = 20;
constexpr int32 BOOST_ENERGY_COST  = 30;
constexpr int32 CLIMB_ENERGY_COST  = 25;
constexpr float CLIMB_HEIGHT       = 12.0f;

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

Player* GetRider(Creature* dragon)
{
    if (!dragon)
        return nullptr;

    if (Vehicle* vehicle = dragon->GetVehicleKit())
        if (Unit* passenger = vehicle->GetPassenger(0))
            return passenger->ToPlayer();

    return nullptr;
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

void CleanupPlayer(Player* player, bool addFallSafety)
{
    Creature* dragon = GetDragon(player);
    if (!dragon)
        return;

    if (addFallSafety && player->IsAlive())
        player->CastSpell(player, SPELL_FALL_SAFETY, true);

    player->ExitVehicle();
    dragon->DespawnOrUnsummon(500ms);
}

SpellCastResult CheckEnergyCast(Unit* caster, int32 cost)
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
        me->SetReactState(REACT_PASSIVE);
        me->SetCanFly(true);
        me->SetDisableGravity(true);
        me->SetSpeedRate(MOVE_FLIGHT, 1.5f);
        me->SetPowerType(POWER_ENERGY);
        me->SetMaxPower(POWER_ENERGY, 100);
        me->SetPower(POWER_ENERGY, 100);
    }

    void PassengerBoarded(Unit* passenger, int8 seatId, bool apply) override
    {
        if (seatId != 0 || !passenger || passenger->GetTypeId() != TYPEID_PLAYER)
            return;

        Player* player = passenger->ToPlayer();
        if (apply)
        {
            _landing = false;
            me->SetCanFly(true);
            me->SetDisableGravity(true);
            me->SetPowerType(POWER_ENERGY);
            me->SetMaxPower(POWER_ENERGY, 100);
            me->SetPower(POWER_ENERGY, 100);
            return;
        }

        // Voluntary exit, death and forced removal all get a fall-safety aura.
        if (player->IsAlive())
            player->CastSpell(player, G17Dragonriding::SPELL_FALL_SAFETY, true);
        me->DespawnOrUnsummon(500ms);
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
            SendToRider(me, "|cffff4040[G17-B0] 当前下方没有可验证的安全地面，已取消着陆。|r");
            return;
        }

        _landing = true;
        SendToRider(me, "|cff80dfff[G17-B0] 正在安全着陆；到地面后会自动离车。|r");
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
            GetCaster()->ModifyPower(POWER_ENERGY, -G17Dragonriding::BREATH_ENERGY_COST);
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
            GetCaster()->ModifyPower(POWER_ENERGY, -G17Dragonriding::BOOST_ENERGY_COST);
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
            GetCaster()->ModifyPower(POWER_ENERGY, -G17Dragonriding::CLIMB_ENERGY_COST);
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
                ChatHandler(session).SendSysMessage("|cffff8040[G17-B0] 已进入禁飞/城市/室内/副本区域，御龙载具已安全清理。|r");
            G17Dragonriding::CleanupPlayer(player, true);
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
        using namespace G17Dragonriding;
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        if (!player->IsAlive())
        {
            handler->SendSysMessage("|cffff4040[G17-B0] 死亡状态不能召唤。|r");
            return true;
        }
        if (player->IsInCombat())
        {
            handler->SendSysMessage("|cffff4040[G17-B0] 战斗中不能召唤。|r");
            return true;
        }
        if (GetDragon(player))
        {
            handler->SendSysMessage("|cffffff00[G17-B0] 你已经在御龙载具上。|r");
            return true;
        }
        if (player->GetVehicleBase() || player->IsMounted() || player->IsInFlight())
        {
            handler->SendSysMessage("|cffff4040[G17-B0] 请先离开其它载具、坐骑或出租飞行。|r");
            return true;
        }
        if (IsBlockedArea(player))
        {
            handler->SendSysMessage("|cffff4040[G17-B0] 当前地点是室内、城市、禁飞区、副本或PVP地图，拒绝召唤。|r");
            return true;
        }

        Position spawn = player->GetPosition();
        spawn.m_positionZ += 1.5f;
        TempSummon* dragon = player->SummonCreature(NPC_DRAGONRIDING_VEHICLE, spawn, TEMPSUMMON_MANUAL_DESPAWN);
        if (!dragon)
        {
            handler->SendSysMessage("|cffff4040[G17-B0] 召唤失败：请确认已导入G17B0 world SQL。|r");
            return true;
        }

        dragon->SetCreatorGUID(player->GetGUID());
        dragon->SetFacingTo(player->GetOrientation());
        dragon->SetCanFly(true);
        dragon->SetDisableGravity(true);
        dragon->SetPowerType(POWER_ENERGY);
        dragon->SetMaxPower(POWER_ENERGY, 100);
        dragon->SetPower(POWER_ENERGY, 100);
        player->EnterVehicle(dragon, 0);

        if (player->GetVehicleBase() != dragon)
        {
            dragon->DespawnOrUnsummon();
            handler->SendSysMessage("|cffff4040[G17-B0] 进入可控座位失败；请回传worldserver相关Vehicle日志。|r");
            return true;
        }

        handler->SendSysMessage("|cff80dfff[G17-B0] 御龙载具已召唤：1龙息、2加速、3爬升、4安全着陆。|r");
        return true;
    }

    static bool HandleDismiss(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (!G17Dragonriding::GetDragon(player))
        {
            handler->SendSysMessage("|cffffff00[G17-B0] 当前没有御龙载具。|r");
            return true;
        }

        G17Dragonriding::CleanupPlayer(player, true);
        handler->SendSysMessage("|cff80dfff[G17-B0] 御龙载具已安全解除。|r");
        return true;
    }

    static bool HandleStatus(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (Creature* dragon = G17Dragonriding::GetDragon(player))
        {
            handler->PSendSysMessage("|cff80dfff[G17-B0] ACTIVE entry=%u energy=%u/%u map=%u zone=%u area=%u|r",
                dragon->GetEntry(), dragon->GetPower(POWER_ENERGY), dragon->GetMaxPower(POWER_ENERGY),
                player->GetMapId(), player->GetZoneId(), player->GetAreaId());
        }
        else
            handler->PSendSysMessage("|cffffff00[G17-B0] INACTIVE area_allowed=%s|r",
                G17Dragonriding::IsBlockedArea(player) ? "false" : "true");
        return true;
    }

    static bool HandleHelp(ChatHandler* handler)
    {
        handler->SendSysMessage("|cff80dfff===== G17-B0 原生御龙载具 =====|r");
        handler->SendSysMessage(".dragon summon  召唤并进入可控龙载具");
        handler->SendSysMessage(".dragon dismiss 安全离车并清理");
        handler->SendSysMessage(".dragon status  查看载具与energy状态");
        handler->SendSysMessage("技能条：1龙息(20能量) / 2短时加速(30) / 3爬升(25) / 4安全着陆");
        handler->SendSysMessage("B0是3.3.5原生离散技能版；连续动量/俯冲/滑翔属于后续B2。 ");
        return true;
    }
};

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
