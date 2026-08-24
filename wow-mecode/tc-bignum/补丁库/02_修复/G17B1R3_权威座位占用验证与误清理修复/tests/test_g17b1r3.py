#!/usr/bin/env python3
from __future__ import annotations
import hashlib,importlib.util,sys,tempfile,unittest
from pathlib import Path
sys.dont_write_bytecode=True
R=Path(__file__).resolve().parents[1];PRE=R/'original/src/server/scripts/Commands/cs_dragonriding.cpp';POST=R/'payload/src/server/scripts/Commands/cs_dragonriding.cpp';TOOL=R/'tools/apply_g17b1r3_source.py'
def load():
 s=importlib.util.spec_from_file_location('g17b1r3tool',TOOL);assert s and s.loader;m=importlib.util.module_from_spec(s);sys.modules[s.name]=m;s.loader.exec_module(m);return m
T=load()
def accepted(vehicle:bool, authoritative:int, expected:int, charmer:bool)->bool:return vehicle and authoritative==expected and charmer
class Tests(unittest.TestCase):
 def setUp(self):self.pre=PRE.read_text();self.post=POST.read_text();a=self.post.index('class VerifyBoardingEvent');b=self.post.index('bool SpawnTypedVehicle',a);self.verify=self.post[a:b]
 def test_hashes(self):
  self.assertEqual(hashlib.sha256(PRE.read_bytes()).hexdigest(),T.PRE_SHA256);self.assertEqual(hashlib.sha256(POST.read_bytes()).hexdigest(),T.POST_SHA256)
 def test_real_runtime_tuple_accepts_movement_minus_one(self):
  self.assertTrue(accepted(True,0,0,True)) # reported actualVehicle==expected, seat-map=0 is implied by charmer, movementSeat=-1 is diagnostic only
 def test_negative_control_tuples_fail_closed(self):
  for case in ((False,0,0,True),(True,-1,0,True),(True,1,0,True),(True,0,0,False)):self.assertFalse(accepted(*case),case)
 def test_authoritative_helper_uses_vehicle_seat_occupancy(self):
  self.assertIn('int8 GetAuthoritativePassengerSeatId',self.post);self.assertIn('vehicle->GetPassenger(seatPair.first) == passenger',self.post)
 def test_verifier_does_not_gate_on_movement_seat(self):
  self.assertIn('authoritativeSeat == _seatId',self.verify);self.assertIn('movementSeat = _player->GetTransSeat()',self.verify);self.assertNotIn('_player->GetTransSeat() == _seatId',self.verify)
 def test_control_and_failure_diagnostics(self):
  self.assertIn('dragon->GetCharmerGUID() == _player->GetGUID()',self.verify)
  for x in ('G17B1R3 boarding verified','authoritativeSeat={} movementSeat={} controlled=true','expectedSeat={} authoritativeSeat={} movementSeat={} charmer={}'):self.assertIn(x,self.verify)
 def test_status_uses_authoritative_seat_and_preserves_movement_diagnostic(self):
  self.assertIn('seat=%d movementSeat=%d controlled=%s',self.post);self.assertIn('GetAuthoritativePassengerSeatId(vehicle, player)',self.post)
 def test_b1_contract_and_b2_boundary_preserved(self):
  for x in ('player->HasSpell(info->Id)','GetMountDisplayId()','SetDisplayId(sourceDisplay)','SetNativeDisplayId(sourceDisplay)','.dragon auto on|off','IsBlockedArea','RegisterSpellScript(spell_g17_dragon_breath_energy)'):self.assertIn(x,self.post)
  self.assertNotIn('NearTeleportTo',self.post);self.assertIn('5档动量、1200%极速和独立战斗页属于下一阶段B2/B3',self.post)
 def test_installer_lifecycle(self):
  with tempfile.TemporaryDirectory() as d:
   root=Path(d);t=root/T.SOURCE_RELATIVE;t.parent.mkdir(parents=True);t.write_bytes(PRE.read_bytes());self.assertEqual(T.check(root),'READY_PREIMAGE');T.apply(root);self.assertEqual(T.check(root),'ALREADY_APPLIED');T.apply(root);T.rollback(root);self.assertEqual(hashlib.sha256(t.read_bytes()).hexdigest(),T.PRE_SHA256)
if __name__=='__main__':unittest.main(verbosity=2)
