#!/usr/bin/env python3
from __future__ import annotations
import hashlib, importlib.util, sys, tempfile, unittest
from pathlib import Path
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1]
PRE=ROOT/'original/src/server/scripts/Commands/cs_dragonriding.cpp'
POST=ROOT/'payload/src/server/scripts/Commands/cs_dragonriding.cpp'
TOOL=ROOT/'tools/apply_g17b1_source.py'
def load():
 spec=importlib.util.spec_from_file_location('g17b1tool',TOOL);assert spec and spec.loader
 m=importlib.util.module_from_spec(spec);sys.modules[spec.name]=m;spec.loader.exec_module(m);return m
T=load()
class G17B1Tests(unittest.TestCase):
 def setUp(self):self.pre=PRE.read_text();self.post=POST.read_text()
 def test_hashes(self):
  self.assertEqual(hashlib.sha256(PRE.read_bytes()).hexdigest(),T.PRE_SHA256)
  self.assertEqual(hashlib.sha256(POST.read_bytes()).hexdigest(),T.POST_SHA256)
 def test_generic_owned_mount_hook(self):
  for token in ['OnSpellCast(Player* player, Spell* spell','info->HasAura(SPELL_AURA_MOUNTED)','player->HasSpell(info->Id)','AutoConvertMountEvent','FindOwnedMountAura','GetMountDisplayId()']:
   self.assertIn(token,self.post)
  self.assertNotIn('GetName()',self.post[self.post.index('uint32 InferArchetype'):self.post.index('AuraEffect const* FindOwnedMountAura')])
 def test_exact_model_and_typed_session(self):
  for token in ['SetDisplayId(sourceDisplay)','SetNativeDisplayId(sourceDisplay)','CREATURE_TYPE_DRAGONKIN','CREATURE_TYPE_MECHANICAL','CREATURE_TYPE_BEAST','CREATURE_TYPE_ELEMENTAL','ARCHETYPE_GENERIC','DATA_SOURCE_SPELL','DATA_SOURCE_CREATURE','DATA_SOURCE_DISPLAY','DATA_ARCHETYPE']:
   self.assertIn(token,self.post)
 def test_safe_conversion_order(self):
  start=self.post.index('bool SpawnTypedVehicle')
  end=self.post.index('class AutoConvertMountEvent',start)
  body=self.post[start:end]
  self.assertLess(body.index('GetVehicleKit()'),body.index('RemoveAurasDueToSpell(sourceSpell)'))
  self.assertLess(body.index('GetControllableSeatId(vehicle)'),body.index('RemoveAurasDueToSpell(sourceSpell)'))
  self.assertLess(body.index('SetData(DATA_SOURCE_DISPLAY'),body.index('RemoveAurasDueToSpell(sourceSpell)'))
 def test_optout_and_owned_fallback_command(self):
  for token in ['.dragon auto on|off','HandleAuto','IsAutoEnabled','SetAutoEnabled','HandleMount','必须提供自己已经学会']:
   self.assertIn(token,self.post)
 def test_r1_vehicle_and_safety_chain_preserved(self):
  for token in ['VerifyBoardingEvent','VEHICLE_SEAT_FLAG_CAN_CONTROL','CalculateTime(250ms)','IsBlockedArea','player->IsOutdoors()','OnMapChanged','OnUpdateZone','SPELL_FALL_SAFETY','RegisterSpellScript(spell_g17_dragon_breath_energy)']:
   self.assertIn(token,self.post)
 def test_b2_not_falsely_implemented(self):
  self.assertIn('5档动量、1200%极速和独立战斗页属于下一阶段B2/B3',self.post)
  self.assertNotIn('NearTeleportTo',self.post)
 def test_installer_lifecycle(self):
  with tempfile.TemporaryDirectory() as td:
   root=Path(td);target=root/T.SOURCE_RELATIVE;target.parent.mkdir(parents=True);target.write_bytes(PRE.read_bytes())
   self.assertEqual(T.check(root),'READY_PREIMAGE');T.apply(root);self.assertEqual(T.check(root),'ALREADY_APPLIED');T.apply(root);T.rollback(root);self.assertEqual(hashlib.sha256(target.read_bytes()).hexdigest(),T.PRE_SHA256)
if __name__=='__main__':unittest.main(verbosity=2)
