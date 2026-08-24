#!/usr/bin/env python3
from pathlib import Path
import re

ROOT=Path(__file__).resolve().parents[1]
CPP=(ROOT/'payload/src/server/scripts/Commands/cs_dragonriding.cpp').read_text(encoding='utf-8')
INSTALL=(ROOT/'sql/G17B0_world_install.sql').read_text(encoding='utf-8')
CHECK=(ROOT/'sql/G17B0_world_check.sql').read_text(encoding='utf-8')
ROLLBACK=(ROOT/'sql/G17B0_world_rollback.sql').read_text(encoding='utf-8')

def require(ok,msg):
    if not ok: raise AssertionError(msg)

require(CPP.count('void AddSC_dragonriding_commandscript()')==1,'AddSC implementation')
for needle in (
    'struct npc_g17_dragonriding_vehicle : public VehicleAI',
    'player->EnterVehicle(dragon, 0);',
    'me->SetPowerType(POWER_ENERGY);',
    'me->SetMaxPower(POWER_ENERGY, 100);',
    'BREATH_ENERGY_COST = 20',
    'BOOST_ENERGY_COST  = 30',
    'CLIMB_ENERGY_COST  = 25',
    'SPELL_DRAGON_BREATH = 9573',
    'SPELL_ACCELERATE     = 55215',
    'SPELL_CLIMB          = 52197',
    'SPELL_SAFE_LANDING   = 53208',
    'MoveTakeoff(POINT_CLIMB',
    'MoveLand(POINT_LAND',
    'void OnMapChanged(Player* player) override',
    'void OnUpdateZone(Player* player',
    'void OnLogout(Player* player) override',
    'void OnPlayerRepop(Player* player) override',
    'void OnPVPKill(Player*',
    'void OnPlayerKilledByCreature(Creature*',
    'map->IsDungeon() || map->IsBattlegroundOrArena()',
    'AREA_FLAG_NO_FLY_ZONE',
    'AREA_FLAG_CAPITAL',
    '{ "summon",  HandleSummon',
    '{ "dismiss", HandleDismiss',
    '{ "status",  HandleStatus',
): require(needle in CPP,needle)

require(CPP.count('RegisterSpellScript(')==4,'four spell scripts registered')
require(CPP.count('RegisterCreatureAI(')==1,'one vehicle AI registered')
require(CPP.count('ModifyPower(POWER_ENERGY, -G17Dragonriding::')==3,'breath/boost/climb each consume energy')
require('AfterCast += SpellCastFn(spell_g17_dragon_climb::ConsumeEnergy);' in CPP,'climb energy consumption hook')
require('not the B2 momentum/gliding/client-prediction implementation.' in CPP,'B0/B2 boundary')

for sql in (INSTALL,CHECK,ROLLBACK):
    require('1000171' in sql,'custom entry fixed')
require('@G17B0_SOURCE := 27756' in INSTALL,'clone native Ruby Drake')
require('`VehicleId`<>0' in INSTALL,'source Vehicle gate')
require("COALESCE(`ScriptName`,'')<>@G17B0_SCRIPT" in INSTALL,'NULL-safe collision gate')
require('INSERT INTO `creature_template_spell`' in INSTALL,'action bar rows')
for idx,spell in enumerate((9573,55215,52197,53208)):
    require(re.search(rf'SELECT @G17B0_ENTRY,{idx},{spell},NULL',INSTALL) is not None,f'action {idx}={spell}')
    require(f'(@G17B0_ENTRY,{idx},{spell})' in INSTALL,f'install verifies action {idx}={spell}')
    require(f'(@G17B0_ENTRY,{idx},{spell})' in CHECK,f'check verifies action {idx}={spell}')
require("'FAILED_TARGET_VEHICLE'" in INSTALL,'target VehicleId assertion')
require("'FAILED_FLIGHT_MOVEMENT'" in INSTALL and "'FAILED_FLIGHT_MOVEMENT'" in CHECK,'movement assertions')
require('G17B0_WORLD_INSTALL_PASS' in INSTALL,'install final assertion')
require('G17B0_WORLD_CHECK_PASS' in CHECK,'check final assertion')
require('G17B0_WORLD_ROLLBACK_PASS' in ROLLBACK,'rollback assertion')
require("AND `ScriptName`=@G17B0_SCRIPT" in ROLLBACK,'owned-only creature rollback')
require('REPLACE INTO `creature_template`' not in INSTALL,'no destructive template replace')
require('DELETE FROM `creature_template`' not in INSTALL,'install never deletes template')

print('G17B0_STATIC_TESTS=PASS')
print('G17B0_REGISTERED_SPELL_SCRIPTS=4')
print('G17B0_ACTION_BAR_SLOTS=4')
print('G17B0_B2_CLAIMED=False')
