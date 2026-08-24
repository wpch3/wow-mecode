#!/usr/bin/env python3
from __future__ import annotations
import hashlib,importlib.util,sys,tempfile,unittest
from pathlib import Path
sys.dont_write_bytecode=True
R=Path(__file__).resolve().parents[1];PRE=R/'original/src/server/scripts/Commands/cs_dragonriding.cpp';POST=R/'payload/src/server/scripts/Commands/cs_dragonriding.cpp';TOOL=R/'tools/apply_g17b1r4_source.py'
def load():
 s=importlib.util.spec_from_file_location('g17b1r4tool',TOOL);assert s and s.loader;m=importlib.util.module_from_spec(s);sys.modules[s.name]=m;s.loader.exec_module(m);return m
T=load()
class Tests(unittest.TestCase):
 def setUp(self):
  self.pre=PRE.read_text();self.post=POST.read_text();a=self.post.index('struct npc_g17_dragonriding_vehicle');b=self.post.index('class spell_g17_dragon_breath_energy',a);self.ai=self.post[a:b]
 def test_hashes(self):
  self.assertEqual(hashlib.sha256(PRE.read_bytes()).hexdigest(),T.PRE_SHA256);self.assertEqual(hashlib.sha256(POST.read_bytes()).hexdigest(),T.POST_SHA256)
 def test_root_cause_old_source_had_only_zone_hook(self):
  self.assertIn('void OnUpdateZone(Player* player',self.pre);self.assertNotIn('void UpdateAI(uint32 diff) override',self.pre)
 def test_continuous_safety_interval_is_bounded(self):
  self.assertIn('SAFETY_CHECK_INTERVAL_MS = 250',self.post);self.assertIn('void UpdateAI(uint32 diff) override',self.ai);self.assertIn('_safetyCheckTimer -= diff',self.ai)
 def test_periodic_gate_uses_live_rider_and_server_policy(self):
  for x in ('Player* player = G17Dragonriding::GetRider(me)','G17Dragonriding::IsBlockedArea(player)','player->IsOutdoors()'):self.assertIn(x,self.ai)
 def test_cleanup_is_single_shot_and_fail_closed(self):
  p=self.ai.index('_safetyCleanupStarted = true;');c=self.ai.index('G17Dragonriding::CleanupPlayer(player, true);');self.assertLess(p,c);self.assertIn('if (_safetyCleanupStarted)',self.ai)
 def test_safety_state_resets_on_reset_and_boarding(self):
  self.assertGreaterEqual(self.ai.count('_safetyCleanupStarted = false;'),2);self.assertGreaterEqual(self.ai.count('_safetyCheckTimer = G17Dragonriding::SAFETY_CHECK_INTERVAL_MS;'),3)
 def test_existing_zone_hook_remains_defense_in_depth(self):
  self.assertIn('void OnUpdateZone(Player* player',self.post);self.assertIn('G17Dragonriding::IsBlockedArea(player)',self.post)
 def test_r3_authoritative_seat_fix_is_preserved(self):
  for x in ('GetAuthoritativePassengerSeatId','vehicle->GetPassenger(seatPair.first) == passenger','authoritativeSeat == _seatId','seat=%d movementSeat=%d controlled=%s'):self.assertIn(x,self.post)
 def test_deferred_climb_and_landing_code_is_untouched(self):
  def block(s,a,b):return s[s.index(a):s.index(b,s.index(a))]
  self.assertEqual(block(self.pre,'    void DoAction(int32 action) override','    void JustDied'),block(self.post,'    void DoAction(int32 action) override','    void JustDied'))
  self.assertEqual(block(self.pre,'class spell_g17_dragon_climb','class spell_g17_dragon_safe_landing'),block(self.post,'class spell_g17_dragon_climb','class spell_g17_dragon_safe_landing'))
 def test_b2_boundary_preserved(self):
  self.assertNotIn('NearTeleportTo',self.post);self.assertIn('5档动量、1200%极速和独立战斗页属于下一阶段B2/B3',self.post)
 def test_installer_lifecycle(self):
  with tempfile.TemporaryDirectory() as d:
   root=Path(d);t=root/T.SOURCE_RELATIVE;t.parent.mkdir(parents=True);t.write_bytes(PRE.read_bytes());self.assertEqual(T.check(root),'READY_PREIMAGE');T.apply(root);self.assertEqual(T.check(root),'ALREADY_APPLIED');T.apply(root);T.rollback(root);self.assertEqual(hashlib.sha256(t.read_bytes()).hexdigest(),T.PRE_SHA256)
if __name__=='__main__':unittest.main(verbosity=2)
