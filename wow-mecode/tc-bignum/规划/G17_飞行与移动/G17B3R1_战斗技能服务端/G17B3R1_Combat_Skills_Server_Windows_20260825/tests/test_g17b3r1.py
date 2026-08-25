#!/usr/bin/env python3
"""G17-B3R1 server package behavior tests (static C++ contract + lifecycle)."""
from __future__ import annotations

import hashlib
import importlib.util
import re
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ORIGINAL = ROOT / "original_src/src/server/scripts/Commands/cs_dragonriding.cpp"
PAYLOAD = ROOT / "payload_src/src/server/scripts/Commands/cs_dragonriding.cpp"
ROLLBACK = ROOT / "rollback_safe_src/src/server/scripts/Commands/cs_dragonriding.cpp"
TOOL = ROOT / "tools/apply_g17b3r1_source.py"

PRE_SHA = "98446106309b45371f138d9c7bc707ee608d9a3db347e13d61cfd68cc97810f9"
POST_SHA = "1a96b72eb28ffa2c0ac0d3e0c07e26c30f25bcd8525babd15efad02a041825d6"


def sha(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


class TestFrozenInputs(unittest.TestCase):
    def test_01_original_is_b2r3_floor(self):
        self.assertEqual(sha(ORIGINAL), PRE_SHA)

    def test_02_payload_is_b3r1_postimage(self):
        self.assertEqual(sha(PAYLOAD), POST_SHA)

    def test_03_rollback_is_b2r3_floor(self):
        self.assertEqual(sha(ROLLBACK), PRE_SHA)

    def test_04_payload_differs(self):
        self.assertNotEqual(sha(PAYLOAD), sha(ORIGINAL))


class TestR0ExitNormalize(unittest.TestCase):
    """Direct 'leave vehicle' must normalize the RIDER (flight/gravity/speed)."""

    def setUp(self):
        self.text = PAYLOAD.read_text(encoding="utf-8")

    def test_01_helper_defined(self):
        self.assertIn("void NormalizeRiderAfterExit(Player* player)", self.text)

    def test_02_helper_clears_flight_and_speed(self):
        self.assertIn("player->SetCanFly(false)", self.text)
        self.assertIn("player->SetDisableGravity(false)", self.text)
        self.assertIn("player->UpdateSpeed(moveType)", self.text)
        self.assertIn("MOVEMENTFLAG_HOVER", self.text)

    def test_03_called_from_exit_paths(self):
        # PassengerBoarded(apply=false), CleanupPlayer, JustDied
        self.assertGreaterEqual(self.text.count("NormalizeRiderAfterExit("), 4)

    def test_04_landing_path_unchanged(self):
        # The normal landing action still exists (skill 4 unaffected).
        self.assertIn("ACTION_LAND", self.text)


class TestB3R1Combat(unittest.TestCase):
    def setUp(self):
        self.text = PAYLOAD.read_text(encoding="utf-8")

    def test_01_constants_present(self):
        for tok in ("COMBAT_SPELL_BASE   = 990000", "COMBAT_SPELL_COUNT  = 25",
                    "COMBAT_STUN_MS", "COMBAT_BASE_DAMAGE[5]",
                    "COMBAT_CD_MS[5]", "COMBAT_HEAL_PCT"):
            self.assertIn(tok, self.text)

    def test_02_helpers(self):
        for tok in ("bool IsCombatSkill", "CombatArchetypeToMount",
                    "GrantCombatSkills", "RevokeCombatSkills",
                    "HandleCombatSkillSpell"):
            self.assertIn(tok, self.text)

    def test_03_real_effects(self):
        for tok in ("Unit::DealDamage(", "Unit::DealHeal(",
                    "target->SetStunned(true)", "AddCooldown(",
                    "RemoveAurasWithMechanic(", "LearnSpell(",
                    "RemoveSpell("):
            self.assertIn(tok, self.text)

    def test_04_script_registered_and_gated(self):
        self.assertIn("class spell_g17_combat_skill", self.text)
        self.assertIn("RegisterSpellScript(spell_g17_combat_skill)", self.text)
        self.assertIn("player->InBattleground() || player->InArena()", self.text)
        self.assertIn("SPELL_FAILED_NO_POWER", self.text)

    def test_05_grant_revoke_wired(self):
        self.assertIn("GrantCombatSkills(player", self.text)
        self.assertIn("RevokeCombatSkills(player", self.text)
        self.assertRegex(self.text, r"GetData\(G17Dragonriding::DATA_ARCHETYPE\)")

    def test_06_native_api_safety(self):
        # No parachute spell usage and no pure-visual-only combat.
        self.assertNotIn("53208", self.text)
        self.assertNotRegex(self.text, r"SendPlaySpellVisualKit\s*\([^)]*\);\s*//.*combat")


class TestToolLifecycle(unittest.TestCase):
    def test_01_pure_state_classifier(self):
        spec = importlib.util.spec_from_file_location("t", TOOL)
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        self.assertIn(mod.state_for_digest(PRE_SHA),
                      ("READY_B3R1_PREIMAGE", "B3R1_SAFE_ROLLBACK"))
        self.assertEqual(mod.state_for_digest(POST_SHA), "B3R1_APPLIED")
        self.assertEqual(mod.state_for_digest("40" * 32), "")

    def test_02_installer_static(self):
        install = (ROOT / "Install-Build-G17B3R1-Windows.ps1").read_text(
            encoding="utf-8")
        self.assertIn("apply_g17b3r1_source.py", install)
        self.assertIn("append_g17b3_spells.py", install)
        self.assertIn("G17B3R1_WORLD_COMBAT_BINDING=PASS", install)
        self.assertIn("G17B3R1_SERVER_DBC_APPEND=PASS", install)
        # Regression: appender `check` prints to STDOUT only; installer must
        # not ReadAllText a report file (crash: 'Could not find file
        # ...G17B3R1_DBC_CHECK_BEFORE.txt' on the user's machine).
        self.assertNotIn("ReadAllText($dbcCheck)", install)
        self.assertIn("$dbcOut = @(& $python $Appender check", install)
        self.assertIn("DBC_CHECK_STATE=", install)
        self.assertIn("G17B3_SPELL_DBC_STATE=MISSING", install)
        # Version fingerprint so the user can instantly tell an old package.
        self.assertIn('$B3R1_BUILD = "f1_dbc_stdout"', install)
        self.assertIn('"B3R1_BUILD=" + $B3R1_BUILD', install)

    def test_03_ps_files_parse(self):
        checker = ROOT / "tools/ps_static_check.py"
        run = subprocess.run(
            ["python3", str(checker),
             str(ROOT / "Install-Build-G17B3R1-Windows.ps1"),
             str(ROOT / "Rollback-Build-G17B3R1-Windows.ps1")],
            capture_output=True, text=True)
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)
