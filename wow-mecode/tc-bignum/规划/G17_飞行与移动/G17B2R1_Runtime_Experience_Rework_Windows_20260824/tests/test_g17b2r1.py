#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import math
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "payload/src/server/scripts/Commands/cs_dragonriding.cpp"
ORIGINAL = ROOT / "original/src/server/scripts/Commands/cs_dragonriding.cpp"
SAFE_ROLLBACK = ROOT / "rollback_safe/src/server/scripts/Commands/cs_dragonriding.cpp"
SQL = ROOT / "sql/G17B2R1_world_safety_migration.sql"
DBC_EVIDENCE = ROOT / "证据/G17B2R1_52226_project_dbc_audit.txt"
WORLD_EVIDENCE = ROOT / "证据/G17B2R1_52226_world_preimage_collision_audit.txt"
TOOL = ROOT / "tools/apply_g17b2r1_source.py"
PRE_SHA = "8b47a5b507bc281198363972e10f91ab0ed3784ad920cf810bd20eacfb6ec1d5"
POST_SHA = "ff185d9987b8f4457d8380e1c662cd0313b33a7ae4be6b82974e7702d1fdc4fc"
SAFE_ROLLBACK_SHA = "e298a856edcf366b09934c3635ea8493b6d4e529d9fa2dbf2de2bce77b5b0203"
LEGACY_ACTION = 53208
SAFE_ACTION = 52226
LANDING_SCRIPT = "spell_g17_dragon_safe_landing"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def smoothstep(value: float) -> float:
    value = min(1.0, max(0.0, value))
    return value * value * (3.0 - 2.0 * value)


def climb_nodes(start_heading: float, desired_heading: float, distance: float = 20.0,
                height: float = 8.0, count: int = 7):
    delta = (desired_heading - start_heading + math.pi) % (2 * math.pi) - math.pi
    delta = min(0.70, max(-0.70, delta))
    x = y = z = 0.0
    out = [(x, y, z, start_heading)]
    for node in range(1, count + 1):
        t = node / count
        heading = start_heading + delta * smoothstep(t)
        x += math.cos(heading) * distance / count
        y += math.sin(heading) * distance / count
        z = height * smoothstep(t)
        out.append((x, y, z, heading))
    return out


def model_world_migration(slot_spells: list[int],
                          bindings: set[tuple[int, str]]):
    """Pure state model of the SQL ownership guard and three mutations."""
    output_spells = list(slot_spells)
    output_bindings = set(bindings)
    owned_rows = sum(spell in (LEGACY_ACTION, SAFE_ACTION) for spell in output_spells)
    foreign_rows = sum(spell not in (LEGACY_ACTION, SAFE_ACTION) for spell in output_spells)
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
    def test_01_original_is_exact_b2_preimage(self):
        self.assertEqual(sha(ORIGINAL), PRE_SHA)

    def test_02_payload_has_frozen_postimage_hash(self):
        self.assertEqual(sha(SOURCE), POST_SHA)

    def test_03_safe_rollback_has_frozen_hash(self):
        self.assertEqual(sha(SAFE_ROLLBACK), SAFE_ROLLBACK_SHA)

    def test_04_project_dbc_evidence_passes(self):
        text = DBC_EVIDENCE.read_text(encoding="utf-8")
        self.assertIn("G17B2R1_PROJECT_DBC_AUDIT=PASS", text)
        self.assertIn("SHA256=dd25091167f671764735ce88c78b66485c6d661fadf05d322574c261f6e464ea", text)

    def test_05_world_collision_evidence_passes(self):
        text = WORLD_EVIDENCE.read_text(encoding="utf-8")
        self.assertIn("G17B2R1_WORLD_PREIMAGE_COLLISION_AUDIT=PASS", text)
        self.assertIn("LITERAL_MATCHES=0", text)


class TestSourceLifecycle(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        target = self.root / "src/server/scripts/Commands/cs_dragonriding.cpp"
        target.parent.mkdir(parents=True)
        shutil.copy2(ORIGINAL, target)
        self.target = target

    def tearDown(self):
        self.temporary.cleanup()

    def run_tool(self, command: str) -> str:
        result = subprocess.run(
            ["python3", str(TOOL), command, "--source-root", str(self.root)],
            check=True, text=True, capture_output=True)
        return result.stdout

    def test_45_apply_is_strict_and_idempotent(self):
        self.assertIn("READY_B2_PREIMAGE", self.run_tool("check"))
        self.assertIn("G17B2R1_SOURCE_APPLY=PASS", self.run_tool("apply"))
        self.assertEqual(sha(self.target), POST_SHA)
        self.assertIn("ALREADY_CURRENT", self.run_tool("apply"))

    def test_46_apply_preserves_forensic_preimage(self):
        self.run_tool("apply")
        backup = self.target.with_name(self.target.name + ".g17b2r1.b2-preimage")
        self.assertTrue(backup.is_file())
        self.assertEqual(sha(backup), PRE_SHA)

    def test_47_rollback_uses_safe_floor_and_is_idempotent(self):
        self.run_tool("apply")
        self.assertIn("PASS_SAFE_FLOOR", self.run_tool("rollback"))
        self.assertEqual(sha(self.target), SAFE_ROLLBACK_SHA)
        self.assertIn("ALREADY_SAFE", self.run_tool("rollback"))

    def test_48_reapply_from_safe_floor_succeeds(self):
        self.run_tool("rollback")
        self.assertEqual(sha(self.target), SAFE_ROLLBACK_SHA)
        self.assertIn("G17B2R1_SOURCE_APPLY=PASS", self.run_tool("apply"))
        self.assertEqual(sha(self.target), POST_SHA)

    def test_49_unknown_sha_is_rejected_without_writes(self):
        unknown = b"G17B2R1 unknown source fixture\n"
        self.target.write_bytes(unknown)
        unknown_sha = sha(self.target)
        for command in ("check", "apply", "rollback"):
            with self.subTest(command=command):
                result = subprocess.run(
                    ["python3", str(TOOL), command, "--source-root", str(self.root)],
                    check=False, text=True, capture_output=True)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("target SHA not recognized", result.stderr)
                self.assertEqual(sha(self.target), unknown_sha)
                self.assertFalse(self.target.with_name(
                    self.target.name + ".g17b2r1.tmp").exists())
                self.assertFalse(self.target.with_name(
                    self.target.name + ".g17b2r1.b2-preimage").exists())


class TestWorldMigrationModel(unittest.TestCase):
    def test_50_legacy_state_migrates_to_single_safe_state(self):
        before_bindings = {
            (LEGACY_ACTION, LANDING_SCRIPT),
            (12345, "unrelated_script"),
        }
        can_apply, spells, bindings = model_world_migration(
            [LEGACY_ACTION], before_bindings)
        self.assertTrue(can_apply)
        self.assertEqual(spells, [SAFE_ACTION])
        self.assertIn((SAFE_ACTION, LANDING_SCRIPT), bindings)
        self.assertNotIn((LEGACY_ACTION, LANDING_SCRIPT), bindings)
        self.assertIn((12345, "unrelated_script"), bindings)

    def test_51_repeat_execution_is_idempotent(self):
        before_spells = [SAFE_ACTION]
        before_bindings = {
            (SAFE_ACTION, LANDING_SCRIPT),
            (12345, "unrelated_script"),
        }
        first = model_world_migration(before_spells, before_bindings)
        second = model_world_migration(first[1], first[2])
        self.assertTrue(first[0])
        self.assertTrue(second[0])
        self.assertEqual(second[1:], first[1:])

    def test_52_foreign_slot_conflict_is_protected_without_writes(self):
        before_spells = [99999]
        before_bindings = {
            (LEGACY_ACTION, LANDING_SCRIPT),
            (12345, "unrelated_script"),
        }
        can_apply, spells, bindings = model_world_migration(
            before_spells, before_bindings)
        self.assertFalse(can_apply)
        self.assertEqual(spells, before_spells)
        self.assertEqual(bindings, before_bindings)


class TestLandingCommand(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = SOURCE.read_text(encoding="utf-8")
        cls.rollback = SAFE_ROLLBACK.read_text(encoding="utf-8")
        cls.sql = SQL.read_text(encoding="utf-8")
        cls.dbc = DBC_EVIDENCE.read_text(encoding="utf-8")

    def test_06_safe_dummy_is_active_constant(self):
        self.assertRegex(self.source, r"SPELL_SAFE_LANDING\s*=\s*52226")

    def test_07_dummy_has_no_spell_visual(self):
        self.assertIn("SPELL_VISUAL_ID_1=0", self.dbc)
        self.assertIn("SPELL_VISUAL_ID_2=0", self.dbc)

    def test_08_dummy_has_only_dummy_effect(self):
        self.assertIn("EFFECTS=3,0,0", self.dbc)
        self.assertIn("AURAS=0,0,0", self.dbc)

    def test_09_dummy_has_zhcn_landing_name(self):
        self.assertIn("SPELL_NAME_ZHCN=飞行器着陆", self.dbc)
        self.assertIn("SPELL_ICON_ID=2116", self.dbc)

    def test_10_known_bad_parachute_id_absent_from_active_source(self):
        self.assertNotIn("53208", self.source)

    def test_11_known_bad_parachute_id_absent_from_safe_rollback(self):
        self.assertNotIn("53208", self.rollback)

    def test_12_script_guard_precedes_default_suppression(self):
        block = self.source.split("class spell_g17_dragon_safe_landing", 1)[1]
        self.assertLess(block.index("IsDragon(GetCaster())"), block.index("PreventHitDefaultEffect"))

    def test_13_world_action_bar_migrates_to_dummy(self):
        self.assertIn("SET `Spell`=@G17B2R1_SAFE_ACTION", self.sql)
        self.assertIn("@G17B2R1_SAFE_ACTION := 52226", self.sql)

    def test_14_world_binding_migrates_to_dummy(self):
        self.assertIn("INSERT INTO `spell_script_names`", self.sql)
        self.assertIn("@G17B2R1_SAFE_ACTION,@G17B2R1_SCRIPT", self.sql)

    def test_15_world_migration_is_idempotent_and_guarded(self):
        self.assertIn("@G17B2R1_CAN_APPLY", self.sql)
        self.assertIn("NOT EXISTS", self.sql)
        self.assertIn("G17B2R1_WORLD_MIGRATION=PASS", self.sql)

    def test_16_rollback_keeps_safety_floor(self):
        self.assertRegex(self.rollback, r"SPELL_SAFE_LANDING\s*=\s*52226")
        self.assertNotIn("14475", self.rollback)


class TestBoostExperience(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = SOURCE.read_text(encoding="utf-8")

    def test_17_launch_feedback_exists(self):
        self.assertIn("VISUAL_KIT_BOOST_LAUNCH", self.source)
        self.assertIn("高速推进启动", self.source)

    def test_18_sustained_feedback_is_periodic(self):
        self.assertIn("BOOST_TRAIL_INTERVAL_MS = 700", self.source)
        self.assertIn("VISUAL_KIT_SPEED_TRAIL", self.source)
        self.assertIn("_boostTrailTimer = BOOST_TRAIL_INTERVAL_MS", self.source)

    def test_19_top_speed_feedback_exists(self):
        self.assertIn("_currentSpeedRate >= 11.5f", self.source)
        self.assertIn("已进入极速段", self.source)
        self.assertIn("VISUAL_KIT_WIND_BURST", self.source)

    def test_20_shutdown_feedback_exists(self):
        self.assertIn("高速推进结束", self.source)
        self.assertIn("结束风爆已触发", self.source)

    def test_21_recast_is_blocked_while_active(self):
        self.assertIn("DATA_BOOST_ACTIVE", self.source)
        self.assertRegex(self.source, r"GetData\(G17Dragonriding::DATA_BOOST_ACTIVE\)")

    def test_22_hard_cap_remains_1200_percent(self):
        self.assertIn("12.0f", self.source)
        self.assertIn("std::clamp(_currentSpeedRate, 1.0f, FLIGHT_SPEED_RATES.back())", self.source)

    def test_23_boost_can_reach_top_phase_before_four_seconds(self):
        speed = 2.5
        for _ in range(39):  # 3.9 seconds, boost still active
            speed = min(12.0, speed + 2.8 * 0.1)
        self.assertGreaterEqual(speed, 11.5)
        self.assertLessEqual(speed, 12.0)

    def test_24_no_boost_aura_multiplier(self):
        self.assertIn("PreventHitDefaultEffect(effectIndex)", self.source)
        self.assertIn("RemoveAurasDueToSpell(SPELL_ACCELERATE)", self.source)


class TestCurvedClimbAndSteering(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = SOURCE.read_text(encoding="utf-8")

    def test_25_climb_uses_multi_point_spline(self):
        block = self.source.split("bool BuildClimbPath", 1)[1].split("struct LandingProfile", 1)[0]
        self.assertIn("nodeCount = 7", block)
        self.assertIn("LaunchMoveSpline", block)
        self.assertIn("init.SetFly()", block)

    def test_26_climb_no_longer_uses_jump(self):
        self.assertNotIn("MoveJump", self.source)

    def test_27_first_path_point_is_reserved(self):
        self.assertGreaterEqual(self.source.count("path.emplace_back(startX, startY, startZ)"), 2)
        self.assertIn("first real curve node being lost", self.source)

    def test_28_yaw_delta_is_bounded(self):
        nodes = climb_nodes(0.0, math.pi)
        self.assertLessEqual(abs(nodes[-1][3] - nodes[0][3]), 0.700001)

    def test_29_heading_transition_has_no_corner(self):
        headings = [row[3] for row in climb_nodes(0.0, 0.65)]
        steps = [headings[i + 1] - headings[i] for i in range(len(headings) - 1)]
        self.assertTrue(all(step >= 0 for step in steps))
        self.assertLess(max(steps), 0.16)

    def test_30_climb_preserves_forward_and_up(self):
        nodes = climb_nodes(0.0, 0.5)
        self.assertGreater(nodes[-1][0], 15.0)
        self.assertAlmostEqual(nodes[-1][2], 8.0, places=5)
        self.assertTrue(all(nodes[i + 1][2] >= nodes[i][2] for i in range(len(nodes) - 1)))

    def test_31_climb_collision_fallbacks_exist(self):
        self.assertIn("CLIMB_FORWARD_DISTANCE, 12.0f, 7.0f", self.source)
        self.assertIn("me->IsWithinLOS(x, y, z)", self.source)

    def test_32_normal_handoff_does_not_clear_finished_spline(self):
        self.assertIn("RestoreClientFlightControl(timedOut)", self.source)
        self.assertIn("_smoothedTravelHeading = _climbExitHeading", self.source)

    def test_33_high_speed_turn_rate_is_not_amplified(self):
        self.assertNotIn("1.25f", self.source)
        self.assertIn("1.0f - 0.18f * speedBlend", self.source)
        self.assertIn("turnStep = force ? 0.08f : 0.04f", self.source)


class TestTypedLanding(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = SOURCE.read_text(encoding="utf-8")

    def test_34_all_landings_use_custom_spline(self):
        self.assertNotIn("MoveLand", self.source)
        self.assertIn("LaunchLandingPath", self.source)
        self.assertIn("BuildLandingPath", self.source)

    def test_35_magic_is_orientation_fixed(self):
        self.assertIn("ARCHETYPE_MAGIC: return { 26.0f, 7.0f, 12.0f, 14.0f, 8, true }", self.source)

    def test_36_dragon_has_longest_slope(self):
        self.assertIn("ARCHETYPE_DRAGON: return { 45.0f, 12.0f, 18.0f, 21.0f, 10, false }", self.source)

    def test_37_mechanical_has_typed_thrust(self):
        self.assertIn("VISUAL_KIT_MECHANICAL_THRUST = 13481", self.source)
        self.assertIn("分段反推并平滑拉平", self.source)

    def test_38_beast_bad_fire_kit_is_absent(self):
        self.assertNotIn("14475", self.source)
        self.assertIn("无火焰跃落/扑落", self.source)

    def test_39_beast_pounce_has_arc(self):
        self.assertIn("std::sin(PI * t) * 1.6f", self.source)

    def test_40_landing_checks_forward_and_side_options(self):
        self.assertIn("0.0f, -0.30f, 0.30f, -0.60f, 0.60f", self.source)
        self.assertIn("GetFirstCollisionPosition", self.source)
        self.assertIn("IsWithinLOS", self.source)

    def test_41_high_altitude_landing_is_segmented(self):
        self.assertIn("POINT_LAND_APPROACH", self.source)
        self.assertIn("StartLandingSegment();", self.source)
        self.assertIn("_landingSegments > 12", self.source)

    def test_42_landing_timeout_restores_control(self):
        self.assertIn("LANDING_TIMEOUT_MS = 45000", self.source)
        self.assertIn("AbortLanding", self.source)
        self.assertIn("RestoreClientFlightControl(true)", self.source)

    def test_43_contact_normalizes_before_exit(self):
        block = self.source.split("void CompleteLanding()", 1)[1].split("bool _landing", 1)[0]
        self.assertLess(block.index("NormalizeVehicleForExit()"), block.index("player->ExitVehicle()"))
        self.assertIn("SetFallInformation", block)

    def test_44_visuals_are_one_shot_kits_not_auras(self):
        constants = re.findall(r"VISUAL_KIT_[A-Z_]+\s*=\s*(\d+)", self.source)
        self.assertEqual(set(constants), {"44", "696", "13709", "13481", "1066"})
        self.assertNotIn("VISUAL_KIT_MAGIC_WIND = 11818", self.source)


if __name__ == "__main__":
    unittest.main(verbosity=2)
