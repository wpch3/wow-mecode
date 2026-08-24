#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import importlib.util
import sys
import tempfile
import unittest
from pathlib import Path

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
PRE = ROOT / "original/src/server/scripts/Commands/cs_dragonriding.cpp"
POST = ROOT / "payload/src/server/scripts/Commands/cs_dragonriding.cpp"
TOOL = ROOT / "tools/apply_g17b1r5_source.py"
EVIDENCE = ROOT / "证据/g17b1r5_wrapper_spell_analysis_20260823.md"


def load_tool():
    spec = importlib.util.spec_from_file_location("g17b1r5tool", TOOL)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


PATCH = load_tool()


class G17B1R5Tests(unittest.TestCase):
    def setUp(self):
        self.pre = PRE.read_text(encoding="utf-8")
        self.post = POST.read_text(encoding="utf-8")
        ai_start = self.post.index("struct npc_g17_dragonriding_vehicle")
        ai_end = self.post.index("class spell_g17_dragon_breath_energy", ai_start)
        self.ai = self.post[ai_start:ai_end]
        unboard_start = self.ai.index("        // Do not add an implicit parachute here.")
        unboard_end = self.ai.index("    void UpdateAI", unboard_start)
        self.unboard = self.ai[unboard_start:unboard_end]
        cleanup_start = self.post.index("void CleanupPlayer(")
        cleanup_end = self.post.index("class VerifyBoardingEvent", cleanup_start)
        self.cleanup = self.post[cleanup_start:cleanup_end]

    def test_01_exact_pre_and_post_hashes(self):
        self.assertEqual(hashlib.sha256(PRE.read_bytes()).hexdigest(), PATCH.PRE_SHA256)
        self.assertEqual(hashlib.sha256(POST.read_bytes()).hexdigest(), PATCH.POST_SHA256)

    def test_02_old_same_spell_gate_explains_wrapper_failure(self):
        self.assertIn("info->HasAura(SPELL_AURA_MOUNTED)", self.pre)
        self.assertIn("FindOwnedMountAura(_player, _spellId)", self.pre)

    def test_03_mount_candidate_uses_retained_aura_metadata(self):
        block = self.post[self.post.index("bool HasMountAuraMetadata"):self.post.index("AuraEffect const* FindOwnedMountAura")]
        self.assertIn("spellInfo->GetEffects()", block)
        self.assertIn("effect.ApplyAuraName == SPELL_AURA_MOUNTED", block)
        self.assertNotIn("info->HasAura(SPELL_AURA_MOUNTED)", self.post)

    def test_04_runtime_fix_has_no_mount_name_or_id_hardcode(self):
        for forbidden in ("48025", "71342", "Headless", "Love Rocket", "爱情火箭", "无头骑士"):
            self.assertNotIn(forbidden, self.post)

    def test_05_active_inner_mount_aura_is_runtime_authority(self):
        find_start = self.post.index("AuraEffect const* FindOwnedMountAura")
        find_end = self.post.index("constexpr uint32 SPELL_DRAGON_BREATH", find_start)
        find_block = self.post[find_start:find_end]
        self.assertIn("GetAuraEffectsByType(SPELL_AURA_MOUNTED)", find_block)
        self.assertNotIn("GetId() ==", find_block)
        self.assertIn("AuraEffect const* mountEffect = FindOwnedMountAura(_player);", self.post)
        self.assertIn("mountEffect->GetMiscValue()", self.post)

    def test_06_learned_outer_spell_remains_ownership_anchor(self):
        self.assertIn("_player->HasSpell(_spellId)", self.post)
        self.assertIn("SpawnTypedVehicle(_player, _spellId, creatureEntry", self.post)
        self.assertIn("player->RemoveAurasDueToSpell(sourceSpell)", self.post)
        self.assertIn("player->RemoveAurasByType(SPELL_AURA_MOUNTED)", self.post)

    def test_07_non_mount_spells_remain_fail_closed(self):
        self.assertIn("HasMountAuraMetadata(info)", self.post)
        self.assertIn("player->IsMounted() ||", self.post)
        self.assertIn("!_player->IsMounted()", self.post)
        self.assertIn("if (!mountEffect)", self.post)

    def test_08_wrapper_dbc_and_server_chain_evidence_is_exact(self):
        report = EVIDENCE.read_text(encoding="utf-8")
        for expected in (
            "48025", "51621", "48024", "51617", "48023",
            "71342", "71343", "71344", "71345", "71346", "71347",
            "Effect=[6,6,77]", "Aura=[78,4,0]", "SPELL_EFFECT_NONE",
            "ApplyAuraName` metadata remains", "TriggerSpell=[0,0,0]",
        ):
            self.assertIn(expected, report)

    def test_09_implicit_unboard_path_adds_no_parachute(self):
        self.assertNotIn("CastSpell", self.unboard)
        self.assertNotIn("SPELL_FALL_SAFETY", self.unboard)
        self.assertIn("SetCanFly(false)", self.unboard)
        self.assertIn("SetDisableGravity(false)", self.unboard)

    def test_10_continuous_blocked_cleanup_adds_no_parachute(self):
        self.assertIn("CleanupPlayer(player, false, true);", self.ai)
        update_start = self.ai.index("    void UpdateAI")
        update_end = self.ai.index("    void DoAction", update_start)
        update = self.ai[update_start:update_end]
        self.assertNotIn("CastSpell", update)
        self.assertNotIn("SPELL_FALL_SAFETY", update)

    def test_11_zone_defense_cleanup_adds_no_parachute(self):
        zone_start = self.post.index("    void OnUpdateZone(Player* player")
        zone_end = self.post.index("    }\n};", zone_start)
        zone = self.post[zone_start:zone_end]
        self.assertIn("CleanupPlayer(player, false, true);", zone)
        self.assertNotIn("SPELL_FALL_SAFETY", zone)

    def test_12_blocked_cleanup_normalizes_vehicle_and_rider_state(self):
        for expected in (
            "dragon->SetSpeedRate(MOVE_FLIGHT, 1.0f)",
            "dragon->SetCanFly(false)",
            "dragon->SetDisableGravity(false)",
            "player->ExitVehicle()",
            "NormalizeBlockedExitMovement(player)",
        ):
            self.assertIn(expected, self.cleanup)
        self.assertIn("player->UpdateSpeed(moveType)", self.post)

    def test_13_high_boundary_guard_is_non_visual_and_bounded(self):
        guard_start = self.post.index("class NonVisualFallGuardEvent")
        guard_end = self.post.index("void CleanupPlayer", guard_start)
        guard = self.post[guard_start:guard_end]
        self.assertIn("FALL_GUARD_INTERVAL_MS = 100", self.post)
        self.assertIn("FALL_GUARD_MAX_CHECKS = 200", self.post)
        self.assertIn("SetFallInformation", guard)
        self.assertIn("_player->IsFalling()", guard)
        self.assertNotIn("CastSpell", guard)
        self.assertNotIn("SetFeatherFall", guard)
        self.assertNotIn("SetDisableGravity(true)", guard)
        self.assertNotIn("NearTeleportTo", guard)

    def test_14_b1r3_authoritative_seat_fix_is_preserved(self):
        for expected in (
            "GetAuthoritativePassengerSeatId",
            "vehicle->GetPassenger(seatPair.first) == passenger",
            "authoritativeSeat == _seatId",
            "seat=%d movementSeat=%d controlled=%s",
        ):
            self.assertIn(expected, self.post)

    def test_15_b1r4_live_250ms_policy_is_preserved(self):
        self.assertIn("SAFETY_CHECK_INTERVAL_MS = 250", self.post)
        for expected in (
            "void UpdateAI(uint32 diff) override",
            "_safetyCheckTimer -= diff",
            "_safetyCleanupStarted = true",
            "G17Dragonriding::IsBlockedArea(player)",
        ):
            self.assertIn(expected, self.ai)

    def test_16_deferred_b2_climb_and_landing_are_untouched(self):
        def block(text: str, start: str, end: str) -> str:
            begin = text.index(start)
            return text[begin:text.index(end, begin)]
        self.assertEqual(
            block(self.pre, "    void DoAction(int32 action) override", "    void JustDied"),
            block(self.post, "    void DoAction(int32 action) override", "    void JustDied"),
        )
        self.assertEqual(
            block(self.pre, "class spell_g17_dragon_climb", "class spell_g17_dragon_safe_landing"),
            block(self.post, "class spell_g17_dragon_climb", "class spell_g17_dragon_safe_landing"),
        )

    def test_17_manual_mount_command_accepts_wrapper_metadata(self):
        command_start = self.post.index("    static bool HandleMount")
        command_end = self.post.index("    static bool HandleAuto", command_start)
        command = self.post[command_start:command_end]
        self.assertIn("HasMountAuraMetadata(info)", command)
        self.assertIn("player->HasSpell(spellId)", command)
        self.assertIn("支持包装法术", command)

    def test_18_strict_installer_lifecycle(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            target = root / PATCH.SOURCE_RELATIVE
            target.parent.mkdir(parents=True)
            target.write_bytes(PRE.read_bytes())
            self.assertEqual(PATCH.check(root), "READY_PREIMAGE")
            PATCH.apply(root)
            self.assertEqual(PATCH.check(root), "ALREADY_APPLIED")
            PATCH.apply(root)
            PATCH.rollback(root)
            self.assertEqual(hashlib.sha256(target.read_bytes()).hexdigest(), PATCH.PRE_SHA256)


if __name__ == "__main__":
    unittest.main(verbosity=2)
