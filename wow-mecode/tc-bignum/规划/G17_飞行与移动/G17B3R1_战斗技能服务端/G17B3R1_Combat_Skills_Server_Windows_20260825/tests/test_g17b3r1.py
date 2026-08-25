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
POST_SHA = "ecd307b472cb2c49f68607a8b0afe5dcf5f87a7a8eb6f087a4717f4cd8fa1bbb"


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

    def test_05_compile_fix_regressions(self):
        # Real MSVC build errors fixed (FIX4): these symbols must not appear.
        for bad in ("target->SetStunned", "ObjectAccessor::FindUnit"):
            self.assertNotIn(bad, self.text)
        # HandleCombatSkillSpell is defined INSIDE namespace so the
        # PlayerScript can call G17Dragonriding::HandleCombatSkillSpell.
        idx_ns = self.text.index("namespace G17Dragonriding")
        idx_struct = self.text.index("struct npc_g17_dragonriding_vehicle")
        idx_fn = self.text.index("void HandleCombatSkillSpell")
        self.assertLess(idx_ns, idx_fn)
        self.assertLess(idx_fn, idx_struct)
        # Unit::SetStunned is protected -> use flags+state; and include the
        # full SpellHistory type.
        self.assertIn("SetUnitFlag(UNIT_FLAG_STUNNED)", self.text)
        self.assertIn("AddUnitState(UNIT_STATE_STUNNED)", self.text)
        self.assertIn('#include "SpellHistory.h"', self.text)
        # stun release uses FindPlayer/GetUnit (this fork's API)
        self.assertIn("ObjectAccessor::FindPlayer", self.text)
        self.assertIn("ObjectAccessor::GetUnit", self.text)


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
                    "SetUnitFlag(UNIT_FLAG_STUNNED)", "AddCooldown(",
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

    def test_05_binding_sql_is_explicit_and_idempotent(self):
        # Regression: the previous CROSS JOIN/NOT EXISTS generator produced
        # only 5 bound rows (post-count stayed 5, gate FAILed) on the user's
        # MySQL 8.  The fix uses 25 explicit rows + INSERT IGNORE + recount.
        import re
        sql = (ROOT / "sql/G17B3R1_world_combat_binding.sql").read_text(
            encoding="utf-8")
        rows = re.findall(r"\(9900\d\d,\s*@G17B3R1_SCRIPT\)", sql)
        self.assertEqual(len(rows), 25)
        self.assertEqual(len(set(rows)), 25)
        self.assertIn("INSERT IGNORE INTO `spell_script_names`", sql)
        # ignore comment lines (the NOTE describes the old bad construct)
        code = [l for l in sql.splitlines() if not l.lstrip().startswith("--")]
        joint = chr(10).join(code)
        self.assertNotIn("CROSS JOIN", joint)
        self.assertNotIn("NOT EXISTS", joint)