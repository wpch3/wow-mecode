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
POST_SHA = "3e4590da5d8864f8447cd3b55acf05c249855927a33e0e792dd426f03426237a"
INTERMEDIATE_SHA = "03dd649ded01dcd1917b1d0e98689ae1dbfe4289f6fc2548a3a62d616e6a0844"
INTERMEDIATE2_SHA = "adedfc58344a104ccc96ff28155b504727f50e0026d842345721610c6a32a59f"
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

    def test_06_tool_recognizes_both_intermediates_as_upgradeable(self):
        import importlib.util
        spec = importlib.util.spec_from_file_location("applytool", TOOL)
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        self.assertEqual(mod.INTERMEDIATE_SHA256, INTERMEDIATE_SHA)
        self.assertEqual(mod.INTERMEDIATE2_SHA256, INTERMEDIATE2_SHA)
        self.assertIn(INTERMEDIATE_SHA, mod.UPGRADEABLE_SHAS)
        self.assertIn(INTERMEDIATE2_SHA, mod.UPGRADEABLE_SHAS)


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


if __name__ == "__main__":
    unittest.main(verbosity=2)
