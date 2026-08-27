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
POST_SHA = "2ddf54a66395896244869318e4bcfd619d10afc884033c6aa88e7cb53d0e6963"


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
        self.assertIn('$B3R1_BUILD = "f3_decl_order"', install)
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


# Exact SHA256 the user's real source had when the FIX4-window package was
# rejected by the locked-lineage gate (SOURCE_SHA256_BEFORE in
# G17B3R1_WINDOWS_BUILD_RESULT.txt, run of 2026-08-25).
USER_SOURCE_AFTER_FAILED_RUN = "1a96b72eb28ffa2c0ac0d3e0c07e26c30f25bcd8525babd15efad02a041825d6"


class TestLineageUpgrade(unittest.TestCase):
    """FIX5 regression: the gate must accept the real user state 1a96b72e.

    The pre-FIX4 package applied its payload (user source -> 1a96b72e) and
    then failed MSBuild with 6 real error classes. FIX4 replaced the payload
    (ecd307b4) but did NOT whitelist 1a96b72e, so the rerun died with
    'dragonriding source is not a locked lineage image: 1a96b72e...' before
    touching anything. These tests pin the exact upgrade path.
    """

    def _tool_module(self):
        spec = importlib.util.spec_from_file_location("t", TOOL)
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        return mod

    def test_01_intermediate7_is_user_state(self):
        mod = self._tool_module()
        self.assertEqual(mod.INTERMEDIATE7_SHA256, USER_SOURCE_AFTER_FAILED_RUN)

    def test_02_user_state_is_upgradeable(self):
        mod = self._tool_module()
        self.assertIn(USER_SOURCE_AFTER_FAILED_RUN, mod.UPGRADEABLE_SHAS)
        self.assertEqual(mod.state_for_digest(USER_SOURCE_AFTER_FAILED_RUN),
                         "B3R1_INTERMEDIATE_UPGRADEABLE")

    def test_02b_fix4_postimage_is_upgradeable(self):
        # FIX6: after the FIX5-window run the user's source sits on the FIX4
        # postimage ecd307b4 (apply PASS, MSBuild FAIL with 5 decl-order /
        # reference errors).  It must be INTERMEDIATE8 so the next rerun
        # upgrades to the FIX6 payload instead of being lineage-rejected.
        fix4 = "ecd307b472cb2c49f68607a8b0afe5dcf5f87a7a8eb6f087a4717f4cd8fa1bbb"
        mod = self._tool_module()
        self.assertEqual(mod.INTERMEDIATE8_SHA256, fix4)
        self.assertIn(fix4, mod.UPGRADEABLE_SHAS)
        self.assertEqual(mod.state_for_digest(fix4), "B3R1_INTERMEDIATE_UPGRADEABLE")

    def test_03_installer_gate_whitelists_user_state(self):
        install = (ROOT / "Install-Build-G17B3R1-Windows.ps1").read_text(
            encoding="utf-8")
        self.assertIn('Read-ToolHash "INTERMEDIATE7_SHA256"', install)
        # the recognized set = Pre + Post + SafeRollback + all intermediates
        self.assertRegex(install, r"\$recognized = @\(\$Pre, \$Post, \$SafeRollback\) \+ \$Upgradeable")
        self.assertIn("INTERMEDIATE7_SHA256", install)

    def test_04_full_tool_lifecycle(self):
        # End-to-end: check -> apply (preimage) -> apply again (already) ->
        # rollback, against a temp source root.
        import sys
        import tempfile
        mod = self._tool_module()
        with tempfile.TemporaryDirectory() as tmp:
            target = Path(tmp) / "src/server/scripts/Commands/cs_dragonriding.cpp"
            target.parent.mkdir(parents=True)
            target.write_bytes(ORIGINAL.read_bytes())
            self.assertEqual(sha(target), PRE_SHA)
            out = subprocess.run(
                [sys.executable, str(TOOL), "check", "--source-root", tmp],
                capture_output=True, text=True)
            self.assertEqual(out.returncode, 0, out.stdout + out.stderr)
            # PRE_SHA == SAFE_ROLLBACK_SHA (same B2R3 floor) so the state map
            # reports the rollback alias; both mean "recognized, not applied".
            self.assertIn("G17B3R1_SOURCE_STATE=", out.stdout)
            self.assertIn("TARGET_SHA256=" + PRE_SHA, out.stdout)
            out = subprocess.run(
                [sys.executable, str(TOOL), "apply", "--source-root", tmp],
                capture_output=True, text=True)
            self.assertEqual(out.returncode, 0, out.stdout + out.stderr)
            self.assertIn("G17B3R1_SOURCE_APPLY=PASS", out.stdout)
            self.assertEqual(sha(target), POST_SHA)
            # NOTE: no .preimage backup is written for the floor state (the
            # state map aliases 98446106 to B3R1_SAFE_ROLLBACK); that is fine
            # because rollback_safe_src is byte-identical to the floor image.
            # For a true intermediate (e.g. 1a96b72e) apply() does write a
            # forensic .g17b3r1.preimage backup.
            out = subprocess.run(
                [sys.executable, str(TOOL), "apply", "--source-root", tmp],
                capture_output=True, text=True)
            self.assertEqual(out.returncode, 0, out.stdout + out.stderr)
            self.assertIn("G17B3R1_SOURCE_APPLY=ALREADY_CURRENT", out.stdout)
            out = subprocess.run(
                [sys.executable, str(TOOL), "rollback", "--source-root", tmp],
                capture_output=True, text=True)
            self.assertEqual(out.returncode, 0, out.stdout + out.stderr)
            self.assertIn("G17B3R1_SOURCE_ROLLBACK=PASS_B2R3_FLOOR", out.stdout)
            self.assertEqual(sha(target), PRE_SHA)

    def test_05_payload_file_integrity(self):
        # Whole-file sanity for a payload that must survive MSVC: balanced
        # braces/parens and no truncation.
        text = PAYLOAD.read_text(encoding="utf-8")
        self.assertEqual(text.count("{"), text.count("}"))
        self.assertEqual(text.count("("), text.count(")"))
        self.assertTrue(text.rstrip().endswith("}"))
        self.assertGreater(len(text.splitlines()), 1500)

# FIX6: the 5 REAL MSVC errors from the user's 2026-08-25 build log:
#   C3861 RevokeCombatSkills (fwd decl placed AFTER the first call)
#   C2061/C2660/C2143/C2059 CombatStunReleaseEvent (class defined AFTER `new`)
#   C2664 GetUnit(Player*, ...) -> wants const WorldObject& (needs *caster)
class TestFix6DeclOrder(unittest.TestCase):
    def _lines(self):
        return PAYLOAD.read_text(encoding="utf-8").splitlines()

    def _first_line(self, needle, start=0):
        lines = self._lines()
        for i in range(start, len(lines)):
            if needle in lines[i]:
                return i + 1
        return -1

    def test_01_revoke_fwd_decl_before_first_call(self):
        lines = self._lines()
        decl = self._first_line("void RevokeCombatSkills(Player* player);")
        call = self._first_line("    RevokeCombatSkills(player);")
        self.assertGreater(decl, 0, "forward declaration missing")
        self.assertGreater(call, 0, "call site missing")
        self.assertLess(decl, call,
                        "C3861 regression: decl must precede first call")

    def test_02_stun_event_class_before_use(self):
        cls = self._first_line("class CombatStunReleaseEvent : public BasicEvent")
        # match the real call site (with actual ctor args), not the comment
        use = self._first_line("new CombatStunReleaseEvent(player->GetGUID()")
        self.assertGreater(cls, 0, "class definition missing")
        self.assertGreater(use, 0, "use site missing")
        self.assertLess(cls, use,
                        "C2061 regression: class must be complete before `new`")

    def test_03_single_definitions(self):
        text = PAYLOAD.read_text(encoding="utf-8")
        self.assertEqual(text.count("class CombatStunReleaseEvent"), 1)
        self.assertEqual(text.count("void RevokeCombatSkills(Player* player);"), 1)

    def test_04_getunit_takes_reference(self):
        # C2664 regression: ObjectAccessor::GetUnit(const WorldObject&, ...)
        text = PAYLOAD.read_text(encoding="utf-8")
        self.assertIn("ObjectAccessor::GetUnit(*caster, _targetGuid)", text)
        self.assertNotIn("ObjectAccessor::GetUnit(caster, _targetGuid)", text)

    def test_05_addevent_two_arg_pattern(self):
        # The proven pattern from NonVisualFallGuardEvent (compiled on the
        # user's machine): AddEvent(new X(...), CalculateTime(Milliseconds(...)))
        text = PAYLOAD.read_text(encoding="utf-8")
        self.assertIn(
            "new CombatStunReleaseEvent(player->GetGUID(), target->GetGUID()),\n"
            "                player->m_Events.CalculateTime(Milliseconds(G17Dragonriding::COMBAT_STUN_MS))",
            text)
        self.assertIn("new NonVisualFallGuardEvent(player, FALL_GUARD_MAX_CHECKS),",
                      text)
