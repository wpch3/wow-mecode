#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "payload/src/server/scripts/Commands/cs_dragonriding.cpp"
ORIGINAL = ROOT / "original/src/server/scripts/Commands/cs_dragonriding.cpp"
SAFE_ROLLBACK = ROOT / "rollback_safe/src/server/scripts/Commands/cs_dragonriding.cpp"
SQL = ROOT / "sql/G17B2R2_world_landing_binding_guard.sql"
TOOL = ROOT / "tools/apply_g17b2r2_source.py"

PRE_SHA = "ff185d9987b8f4457d8380e1c662cd0313b33a7ae4be6b82974e7702d1fdc4fc"
POST_SHA = "3b92e815dc81ade4aa9927c19716dabddb8e8f93a6d0aff8b32c80dfbcbfc7f1"
INTERMEDIATE_SHA = "03dd649ded01dcd1917b1d0e98689ae1dbfe4289f6fc2548a3a62d616e6a0844"
INTERMEDIATE2_SHA = "adedfc58344a104ccc96ff28155b504727f50e0026d842345721610c6a32a59f"
INTERMEDIATE3_SHA = "3e4590da5d8864f8447cd3b55acf05c249855927a33e0e792dd426f03426237a"
INTERMEDIATE4_SHA = "613420676babe4c71c570c24a0f5d94976623516e0519b4553b3d5962056bafe"
SAFE_ROLLBACK_SHA = "ff185d9987b8f4457d8380e1c662cd0313b33a7ae4be6b82974e7702d1fdc4fc"
LANDING_SCRIPT = "spell_g17_dragon_safe_landing"
SAFE_ACTION = 52226
LEGACY_ACTION = 53208


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def model_world_binding(slot_spells: list[int],
                        bindings: set[tuple[int, str]]):
    """Pure model of the R2 idempotent binding guard (re-affirms 52226).

    The ownership guard only inspects action slot 3 (Index=3), exactly like
    the SQL; the other three slots (breath/accelerate/climb) are unrelated.
    """
    output_spells = list(slot_spells)
    output_bindings = set(bindings)
    slot3 = output_spells[3]
    owned_rows = 1 if slot3 in (LEGACY_ACTION, SAFE_ACTION) else 0
    foreign_rows = 0 if slot3 in (LEGACY_ACTION, SAFE_ACTION) else 1
    can_apply = owned_rows == 1 and foreign_rows == 0
    if can_apply:
        output_spells = [
            SAFE_ACTION if spell in (LEGACY_ACTION, SAFE_ACTION) else spell
            for spell in output_spells
        ]
        output_bindings = {
            row for row in output_bindings
            if row[1] != LANDING_SCRIPT or row[0] == SAFE_ACTION
        }
        output_bindings.add((SAFE_ACTION, LANDING_SCRIPT))
    return can_apply, output_spells, output_bindings


class TestFrozenInputs(unittest.TestCase):
    def test_01_original_is_exact_b2r1_postimage(self):
        self.assertEqual(sha(ORIGINAL), PRE_SHA)

    def test_02_safe_rollback_is_b2r1_floor(self):
        self.assertEqual(sha(SAFE_ROLLBACK), SAFE_ROLLBACK_SHA)

    def test_03_payload_has_frozen_postimage(self):
        self.assertEqual(sha(SOURCE), POST_SHA)

    def test_04_payload_differs_from_preimage(self):
        self.assertNotEqual(sha(SOURCE), PRE_SHA)

    def test_05_tool_embeds_matching_hashes(self):
        text = TOOL.read_text(encoding="utf-8")
        self.assertIn(PRE_SHA, text)
        self.assertIn(POST_SHA, text)
        self.assertIn(INTERMEDIATE_SHA, text)
        self.assertIn(INTERMEDIATE2_SHA, text)
        self.assertIn(SAFE_ROLLBACK_SHA, text)


class TestPowerShellInstaller(unittest.TestCase):
    """The PS1 must read hashes from the Python tool, never hard-code them,
    so a payload hash change can never cause the 'not a locked image' FAIL."""

    def setUp(self):
        self.install = (ROOT / "Install-Build-G17B2R2-Windows.ps1").read_text(encoding="utf-8")
        self.rollback = (ROOT / "Rollback-Build-G17B2R2-Windows.ps1").read_text(encoding="utf-8")

    def test_01_reads_hashes_from_tool(self):
        for script in (self.install, self.rollback):
            self.assertIn("Read-ToolHash", script)
            self.assertIn('POST_SHA256', script)
            self.assertRegex(script, r"Read-ToolHash\s+\"POST_SHA256\"")

    def test_02_no_stale_hardcoded_post_hash(self):
        # The old postimage hashes must not be assigned to $Post.
        stale = ("03dd649ded01dcd1917b1d0e98689ae1dbfe4289f6fc2548a3a62d616e6a0844",
                 "3e4590da5d8864f8447cd3b55acf05c249855927a33e0e792dd426f03426237a",
                 "613420676babe4c71c570c24a0f5d94976623516e0519b4553b3d5962056bafe")
        for h in stale:
            self.assertNotIn(f'$Post = "{h}"', self.install)
            self.assertNotIn(f'$Post = "{h}"', self.rollback)

    def test_03_no_broken_ps1_quote_escaping(self):
        # The broken form was: -match "^\s*$Name...\""  inside a DOUBLE-quoted
        # PS string, where \" is a parse error (PS escapes with backtick).
        # The fixed form builds ONE single-quoted pattern string and passes it
        # as a variable: single backslashes survive into .NET regex unchanged.
        # Raw regex: in the PS1 single-quoted pattern string, single
        # backslashes reach .NET regex unchanged, so the source line literally
        # contains ^\s* etc.  (Built via regex, not a Python string, to avoid
        # SyntaxWarning for \s on Python 3.12.)
        ps_pattern = (r"\$pattern = \('\^\\s\*' \+ \[regex\]::Escape\(\$Name\) "
                      r"\+ '\\s\*=\\s\*\"\(\[0-9a-f\]\+\)\"'\)")
        for script in (self.install, self.rollback):
            self.assertNotIn('-match "^', script)
            self.assertIsNotNone(re.search(ps_pattern, script), script)
            self.assertIn("Where-Object { $_ -match $pattern", script)

    def test_04_hash_read_uses_function_scope_rematch(self):
        # Regression: the first R2 installer read $Matches[1] immediately after
        # the Where-Object filter. $Matches is populated inside the filter's
        # child scope and does not survive into the function scope, so the
        # returned hash was $null and the installer died with "dragonriding
        # source is not a locked B2R1/B2R2 image" on the user's machine. The
        # proven two-step pattern (identical to the WorldDatabaseInfo parser
        # that already ran successfully on the user's Windows build) re-runs
        # -match on the selected line in the function scope.
        for script in (self.install, self.rollback):
            idx_filter = script.index("Where-Object { $_ -match $pattern")
            idx_rematch = script.index("if ($line -notmatch $pattern)")
            self.assertLess(idx_filter, idx_rematch)
            self.assertIn("return $Matches[1]", script)

    def test_05_sql_gate_markers_match_installer(self):
        # Regression: the installer waited for "G17B2R2_SPELL_52226_OVERRIDE=PASS"
        # but the castable SQL ALWAYS emits "G17B2R2_SPELL_52226_CASTABLE=PASS".
        # After mysql exited 0 the gate check still threw -> guaranteed FAIL at
        # the second World SQL step, so the one-click run could never complete.
        markers = {
            "G17B2R2_world_landing_binding_guard.sql":
                "G17B2R2_WORLD_BINDING_GUARD=PASS",
            "G17B2R2_world_spell52226_castable_override.sql":
                "G17B2R2_SPELL_52226_CASTABLE=PASS",
        }
        for fname, marker in markers.items():
            sql = (ROOT / "sql" / fname).read_text(encoding="utf-8")
            self.assertIn("'" + marker + "'", sql)
            self.assertIn('-PassMarker "' + marker + '"', self.install)

    def test_06_tool_recognizes_all_intermediates_as_upgradeable(self):
        import importlib.util
        spec = importlib.util.spec_from_file_location("applytool", TOOL)
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        self.assertEqual(mod.INTERMEDIATE_SHA256, INTERMEDIATE_SHA)
        self.assertEqual(mod.INTERMEDIATE2_SHA256, INTERMEDIATE2_SHA)
        self.assertEqual(mod.INTERMEDIATE3_SHA256, INTERMEDIATE3_SHA)
        self.assertEqual(mod.INTERMEDIATE4_SHA256, INTERMEDIATE4_SHA)
        for h in (INTERMEDIATE_SHA, INTERMEDIATE2_SHA,
                  INTERMEDIATE3_SHA, INTERMEDIATE4_SHA):
            self.assertIn(h, mod.UPGRADEABLE_SHAS)

    def test_07_ps1_reads_all_upgrade_sources_from_tool(self):
        # Regression (second user run): D:\TrinityCore held the earlier R2
        # draft 3e4590da (an earlier delivery README called it final), but the
        # PS1/tool had no such hash in the recognized list, so the run died at
        # "dragonriding source is not a locked B2R1/B2R2 image".  Every valid
        # R2-lineage source must be read from the tool, never hard-coded.
        import importlib.util
        spec = importlib.util.spec_from_file_location("applytool", TOOL)
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        for name in ("INTERMEDIATE_SHA256", "INTERMEDIATE2_SHA256",
                     "INTERMEDIATE3_SHA256", "INTERMEDIATE4_SHA256"):
            self.assertIn(f'Read-ToolHash "{name}"', self.install)
        self.assertNotIn("$Upgradeable +=", self.install)
        self.assertNotIn("$Upgradeable +=", self.rollback)
        self.assertEqual(len(mod.UPGRADEABLE_SHAS), 4)

    def test_08_tool_check_accepts_user_source_3e4590da(self):
        # The user's real second run: D:\TrinityCore held the earlier R2
        # draft 3e4590da (an earlier delivery README called it final).  The
        # apply tool must classify it as upgradeable, so the installer's
        # pre-check passes and the source is replaced with the frozen payload
        # (with a forensic backup), instead of dying with "not a locked
        # image".
        #
        # NOTE (R2FIX3): this used to be tested by monkeypatching module sha()
        # with a `p == target` stub.  On Windows the temp path from
        # TemporaryDirectory does not string-equal root.resolve()'s path, so
        # the stub never fired and check() raised "target SHA not recognized"
        # on the user's machine.  The classifier is now a pure function, so
        # this test exercises exactly what the tool does, with no mocks.
        import importlib.util
        spec = importlib.util.spec_from_file_location("applytool3", TOOL)
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)

        self.assertEqual(
            mod.state_for_digest(INTERMEDIATE3_SHA),
            "B2R2_INTERMEDIATE_UPGRADEABLE")
        self.assertEqual(
            mod.state_for_digest(INTERMEDIATE4_SHA),
            "B2R2_INTERMEDIATE_UPGRADEABLE")
        # Same lifecycle for the other previously-shipped R2 lineage.
        self.assertEqual(
            mod.state_for_digest(INTERMEDIATE_SHA),
            "B2R2_INTERMEDIATE_UPGRADEABLE")
        self.assertEqual(
            mod.state_for_digest(INTERMEDIATE2_SHA),
            "B2R2_INTERMEDIATE_UPGRADEABLE")
        # And the normal states still classify correctly.  Note PRE_SHA and
        # SAFE_ROLLBACK_SHA are the SAME byte image (B2R1), and in the state
        # dict the SAFE_ROLLBACK key is built last, so B2R1 reports as
        # B2R2_SAFE_ROLLBACK_B2R1 (apply() accepts both B2R1 states anyway).
        self.assertIn(mod.state_for_digest(PRE_SHA),
                      ("READY_B2R1_PREIMAGE", "B2R2_SAFE_ROLLBACK_B2R1"))
        self.assertEqual(mod.state_for_digest(POST_SHA), "B2R2_APPLIED")
        self.assertEqual(mod.state_for_digest(SAFE_ROLLBACK_SHA),
                         "B2R2_SAFE_ROLLBACK_B2R1")
        # Unknown digests stay rejected (no silent writes).
        self.assertEqual(mod.state_for_digest("40" * 32), "")
        self.assertNotIn("", (mod.state_for_digest(INTERMEDIATE3_SHA),))


class TestSkill2RichVisuals(unittest.TestCase):
    def setUp(self):
        self.text = SOURCE.read_text(encoding="utf-8")

    def test_01_only_audited_kit_ids_used(self):
        # B2R2 must NOT introduce unverified SpellVisualKit IDs.  Only the
        # already-audited B2R1 kits (44/696/13709/13481/1066) may be used.
        audited = {"44", "696", "13709", "13481", "1066"}
        consts = dict(re.findall(
            r"constexpr uint32 (VISUAL_KIT_\w+)\s*=\s*(\d+);", self.text))
        self.assertTrue(consts)
        for name, value in consts.items():
            self.assertIn(value, audited, f"{name}={value} unverified")
        used = set(re.findall(
            r"SendPlaySpellVisualKit\(\s*(VISUAL_KIT_\w+)\s*,", self.text))
        self.assertTrue(used, "expected at least one kit send")
        for name in used:
            self.assertIn(name, consts, f"undefined kit constant {name}")
        raw = set(re.findall(
            r"SendPlaySpellVisualKit\(\s*(\d+)\s*,", self.text))
        self.assertEqual(raw, set(), f"raw numeric kit IDs used: {raw}")

    def test_02_layered_launch_fires_multiple_kits(self):
        # Launch must be richer than B2R1's single kit: locate the
        # ACTION_ACCELERATE handler and assert several SendPlay calls in it.
        start = self.text.index("if (action == ACTION_ACCELERATE)")
        end = self.text.index("if (action == ACTION_CLIMB)")
        block = self.text[start:end]
        self.assertGreaterEqual(
            block.count("SendPlaySpellVisualKit"), 3)
        self.assertIn("VISUAL_KIT_BOOST_LAUNCH", block)
        self.assertIn("VISUAL_KIT_MECHANICAL_THRUST", block)

    def test_03_midboost_gust_timer_exists_and_alternates(self):
        self.assertIn("_boostGustTimer", self.text)
        self.assertIn("_boostGustPulseCount", self.text)
        self.assertIn("BOOST_GUST_INTERVAL_MS", self.text)
        # The gust alternates between wind burst and ribbon pulse.
        self.assertIn("VISUAL_KIT_WIND_BURST", self.text)
        self.assertIn("VISUAL_KIT_TRAIL_PULSE", self.text)

    def test_04_top_speed_and_shutdown_have_double_impact(self):
        # Both top-speed and shutdown fire the impact ring (wind burst) twice
        # each so the moments read strongly on screen.
        self.assertEqual(
            self.text.count("me->SendPlaySpellVisualKit(VISUAL_KIT_IMPACT_RING, 0);"),
            2)

    def test_05_mechanical_flame_still_type_gated(self):
        self.assertIn("ARCHETYPE_MECHANICAL", self.text)
        self.assertIn("VISUAL_KIT_MECHANICAL_THRUST", self.text)
        # The rejected fire kit must not be reintroduced.
        self.assertNotIn("14475", self.text)

    def test_06_no_aura_or_damage_added(self):
        self.assertNotIn("AddAura", self.text)
        self.assertNotIn("DealDamage", self.text)


class TestSkill3FacingDash(unittest.TestCase):
    def setUp(self):
        self.text = SOURCE.read_text(encoding="utf-8")

    def test_01_climb_uses_rider_facing(self):
        self.assertIn("ResolveFacingHeading", self.text)
        # The climb path must query the rider orientation.
        self.assertRegex(self.text, r"rider->GetOrientation\(\)")

    def test_02_no_stale_smoothed_heading_as_climb_baseline(self):
        # The old buggy line that seeded startHeading from _smoothedTravelHeading
        # must no longer exist.
        self.assertNotIn(
            "startHeading = _travelHeadingReady ? _smoothedTravelHeading",
            self.text)

    def test_03_climb_nodes_advance_along_single_facing_heading(self):
        # Every node must advance along the same facingHeading (no per-node
        # yaw blend). Locate BuildClimbPath and inspect its loop.
        m = re.search(r"bool BuildClimbPath.*?^\s{4}\}", self.text,
                      re.S | re.M)
        self.assertIsNotNone(m)
        body = m.group(0)
        self.assertIn("facingHeading", body)
        # The horizontal step must use cos/sin(facingHeading) directly.
        self.assertRegex(body, r"cos\(facingHeading\)")
        self.assertRegex(body, r"sin\(facingHeading\)")
        # No turnBlend/yaw interpolation remains in the climb path.
        self.assertNotIn("yawDelta", body)
        self.assertNotIn("turnBlend", body)

    def test_04_los_fallback_retained(self):
        # Safety: the 20/12/7 yd LOS fallback must still exist.
        for dist in ("CLIMB_FORWARD_DISTANCE", "12.0f", "7.0f"):
            self.assertIn(dist, self.text)


class TestSkill4Landing(unittest.TestCase):
    def setUp(self):
        self.text = SOURCE.read_text(encoding="utf-8")

    def test_01_landing_script_has_checkcast(self):
        self.assertIn("spell_g17_dragon_safe_landing", self.text)
        # Slice from the landing class to its Register() and verify CheckCast
        # is wired up there (DOTALL so .* crosses newlines).
        start = self.text.index("class spell_g17_dragon_safe_landing")
        end = self.text.index("class g17_dragonriding_playerscript")
        body = self.text[start:end]
        self.assertRegex(body, r"OnCheckCast\s*\+=\s*SpellCheckCastFn\("
                         r"spell_g17_dragon_safe_landing::CheckCast\)")

    def test_02_checkcast_allows_dragon(self):
        # CheckCast must return SPELL_CAST_OK when the caster is a G17 dragon.
        m = re.search(
            r"class spell_g17_dragon_safe_landing.*?SpellCastResult CheckCast\(\)"
            r".*?^\s{4}\}", self.text, re.S | re.M)
        self.assertIsNotNone(m)
        body = m.group(0)
        self.assertIn("IsDragon", body)
        self.assertIn("SPELL_CAST_OK", body)

    def test_03_default_effect_suppressed(self):
        self.assertIn("PreventHitDefaultEffect", self.text)

    def test_04_aftercast_fallback_starts_landing(self):
        self.assertRegex(self.text, r"AfterCast.*EnsureLanding", re.S)
        self.assertIn("ACTION_LAND", self.text)

    def test_05_keeps_real_spell_52226(self):
        self.assertIn("SPELL_SAFE_LANDING   = 52226", self.text)
        # The legacy parachute spell must not be reintroduced as the action.
        self.assertNotIn("= 53208;  //", self.text)


class TestWorldBindingGuard(unittest.TestCase):
    def test_01_sql_targets_world_and_52226(self):
        sql = SQL.read_text(encoding="utf-8")
        self.assertIn("USE `world`", sql)
        self.assertIn("52226", sql)
        self.assertIn(LANDING_SCRIPT, sql)
        self.assertRegex(sql, r"G17B2R2_WORLD_BINDING_GUARD")

    def test_02_model_passes_when_already_on_52226(self):
        can, spells, binds = model_world_binding(
            [9573, 55215, 52197, 52226],
            {(9573, "spell_g17_dragon_breath_energy"),
             (55215, "spell_g17_dragon_accelerate_energy"),
             (52197, "spell_g17_dragon_climb"),
             (52226, LANDING_SCRIPT)})
        self.assertTrue(can)
        self.assertEqual(spells[3], 52226)
        self.assertIn((52226, LANDING_SCRIPT), binds)

    def test_03_model_migrates_legacy_53208(self):
        can, spells, binds = model_world_binding(
            [9573, 55215, 52197, 53208],
            {(9573, "spell_g17_dragon_breath_energy"),
             (55215, "spell_g17_dragon_accelerate_energy"),
             (52197, "spell_g17_dragon_climb"),
             (53208, LANDING_SCRIPT)})
        self.assertTrue(can)
        self.assertEqual(spells[3], 52226)
        self.assertIn((52226, LANDING_SCRIPT), binds)
        self.assertNotIn((53208, LANDING_SCRIPT), binds)

    def test_04_model_refuses_foreign_slot(self):
        can, spells, _ = model_world_binding(
            [9573, 55215, 52197, 12345], set())
        self.assertFalse(can)
        self.assertEqual(spells[3], 12345)



class TestDocsConsistency(unittest.TestCase):
    """README / status docs must describe the real banner and real postimage."""

    def setUp(self):
        self.docs = [ROOT / name for name in (
            "README_FIRST.txt", "README_详细步骤.txt",
            "00-G17B2R2_实现与验收状态.md")]

    def test_01_readmes_use_exact_payload_banner(self):
        banner = (">> G17-B2R2 dragonriding LOADED  build=20260824-r2 "
                  "(skill2/3/4 fixes active)")
        self.assertIn(banner, SOURCE.read_text(encoding="utf-8"))
        for doc in self.docs:
            self.assertIn(banner, doc.read_text(encoding="utf-8"),
                          f"{doc.name} does not quote the real banner")

    def test_02_readmes_use_postimage_not_stale_hash(self):
        # The docs NOW legitimately mention 3e4590da/61342067 as *upgrade
        # sources* (that was the user's real machine state), so only stale
        # postimage claims are forbidden.
        stale_claims = ("postimage=61342067", "postimage=03dd649d",
                        '$Post = "3e4590da', '$Post = "61342067',
                        "应为 3e4590da", "应为 61342067")
        for doc in self.docs:
            text = doc.read_text(encoding="utf-8")
            self.assertIn("3b92e815", text, doc.name)
            for stale in stale_claims:
                self.assertNotIn(stale, text, doc.name)


if __name__ == "__main__":
    unittest.main(verbosity=2)
