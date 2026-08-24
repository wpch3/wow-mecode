#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PRE = ROOT / "original/src/server/scripts/Commands/cs_dragonriding.cpp"
POST = ROOT / "payload/src/server/scripts/Commands/cs_dragonriding.cpp"
PRE_SHA = "35af002b09b5d8112bbc1aaa1750f4a6245adec8b7c91a7852d69bdd283668b8"
POST_SHA = "8b47a5b507bc281198363972e10f91ab0ed3784ad920cf810bd20eacfb6ec1d5"
TEXT = POST.read_text(encoding="utf-8")


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def body(name: str, next_marker: str) -> str:
    start = TEXT.index(name)
    end = TEXT.index(next_marker, start)
    return TEXT[start:end]


class SourceImageTests(unittest.TestCase):
    def test_01_exact_b1r5_preimage(self):
        self.assertEqual(sha(PRE), PRE_SHA)

    def test_02_exact_b2_postimage(self):
        self.assertEqual(sha(POST), POST_SHA)

    def test_03_images_are_distinct(self):
        self.assertNotEqual(PRE.read_bytes(), POST.read_bytes())

    def test_04_authoritative_seat_chain_preserved(self):
        self.assertIn("GetAuthoritativePassengerSeatId", TEXT)
        self.assertIn("vehicle->GetPassenger(seatPair.first) == passenger", TEXT)
        self.assertNotIn("GetTransSeat() == _seatId", TEXT)

    def test_05_wrapper_mount_authority_preserved(self):
        self.assertIn("FindOwnedMountAura", TEXT)
        self.assertIn("effect.ApplyAuraName == SPELL_AURA_MOUNTED", TEXT)
        self.assertIn("mountEffect->GetSpellInfo()", TEXT)

    def test_06_continuous_indoor_policy_preserved(self):
        self.assertIn("SAFETY_CHECK_INTERVAL_MS = 250", TEXT)
        self.assertIn("G17B2 continuous safety cleanup", TEXT)
        self.assertIn("IsBlockedArea(player)", TEXT)


class MomentumTests(unittest.TestCase):
    def test_07_seven_bounded_speed_stages(self):
        match = re.search(r"FLIGHT_SPEED_RATES = \{([^}]+)\}", TEXT)
        self.assertIsNotNone(match)
        values = [float(x.rstrip("f")) for x in re.findall(r"[0-9]+(?:\.[0-9]+)?f", match.group(1))]
        self.assertEqual(values, [2.5, 4.0, 6.0, 8.0, 10.0, 11.0, 12.0])
        self.assertEqual(max(values), 12.0)

    def test_08_explicit_1200_percent_cap(self):
        self.assertIn("std::clamp(flightRate, 1.0f, G17Dragonriding::FLIGHT_SPEED_RATES.back())", TEXT)
        self.assertIn("最高封顶1200%", TEXT)

    def test_09_all_required_control_inputs(self):
        for flag in ("MOVEMENTFLAG_FORWARD", "MOVEMENTFLAG_BACKWARD", "MOVEMENTFLAG_PITCH_UP",
                     "MOVEMENTFLAG_PITCH_DOWN", "MOVEMENTFLAG_ASCENDING", "MOVEMENTFLAG_DESCENDING",
                     "MOVEMENTFLAG_LEFT", "MOVEMENTFLAG_RIGHT"):
            self.assertIn(flag, TEXT)

    def test_10_forward_and_dive_gain_momentum(self):
        self.assertRegex(TEXT, r"if \(forward\)\s+momentumDelta \+= 0\.20f")
        self.assertRegex(TEXT, r"if \(descending\)\s+momentumDelta \+= 0\.34f")

    def test_11_climb_turn_brake_idle_cost_momentum(self):
        for literal in ("momentumDelta -= 0.38f", "momentumDelta -= 0.75f",
                        "momentumDelta -= 0.10f", "momentumDelta -= 0.18f"):
            self.assertIn(literal, TEXT)

    def test_12_boost_is_timer_not_speed_stacking(self):
        action = body("if (action == ACTION_ACCELERATE)", "if (action == ACTION_CLIMB)")
        self.assertIn("_boostTimer = BOOST_DURATION_MS", action)
        self.assertIn("std::min(MAX_MOMENTUM", action)
        self.assertNotIn("SetSpeedRate", action)
        self.assertNotIn("+= BOOST_DURATION_MS", action)

    def test_13_dbc_boost_aura_is_suppressed(self):
        section = body("class spell_g17_dragon_accelerate_energy", "class spell_g17_dragon_climb")
        self.assertIn("PreventHitDefaultEffect(effectIndex)", section)
        self.assertIn("ACTION_ACCELERATE", section)

    def test_14_speed_changes_are_smoothed(self):
        self.assertIn("float const maxStep = acceleration * dt", TEXT)
        self.assertIn("SPEED_PACKET_INTERVAL_MS = 200", TEXT)
        self.assertIn("std::min(_targetSpeedRate, _currentSpeedRate + maxStep)", TEXT)

    def test_15_stall_enables_gravity_and_is_recoverable(self):
        enter = body("void EnterStall()", "void RecoverFromStall()")
        recover = body("void RecoverFromStall()", "void StartForwardClimb()")
        self.assertIn("SetDisableGravity(false, false)", enter)
        self.assertIn("RestoreClientFlightControl(false)", recover)
        self.assertIn("STALL_RECOVER_MOMENTUM", TEXT)

    def test_16_model_reaches_cap_without_exceeding(self):
        momentum = 0.10
        current = 2.5
        rates = [2.5, 4.0, 6.0, 8.0, 10.0, 11.0, 12.0]
        thresholds = [0.0, 0.18, 0.34, 0.50, 0.66, 0.82, 0.94]
        tier = 0
        for _ in range(300):
            dt = 0.1
            momentum = min(1.0, max(0.0, momentum + 0.20 * dt))
            while tier + 1 < len(rates) and momentum >= thresholds[tier + 1] + 0.01:
                tier += 1
            target = rates[tier]
            current = min(target, current + 2.2 * dt)
        self.assertEqual(momentum, 1.0)
        self.assertAlmostEqual(current, 12.0)
        self.assertLessEqual(current, 12.0)

    def test_17_model_brake_and_idle_reduce_momentum(self):
        momentum = 0.8
        momentum = max(0.0, momentum + (0.20 - 0.75) * 1.0)  # forward + backward brake
        self.assertLess(momentum, 0.3)
        momentum = max(0.0, momentum - 0.18 * 2.0)
        self.assertEqual(momentum, 0.0)


class ClimbTests(unittest.TestCase):
    def test_18_climb_destination_is_forward_and_up(self):
        section = body("void StartForwardClimb()", "void CompleteClimb()")
        self.assertIn("std::cos(orientation) * CLIMB_FORWARD_DISTANCE", section)
        self.assertIn("std::sin(orientation) * CLIMB_FORWARD_DISTANCE", section)
        self.assertIn("me->GetPositionZ() + CLIMB_HEIGHT", section)
        self.assertIn("forwardDot <= 1.0f", section)

    def test_19_climb_uses_short_jump_not_takeoff(self):
        section = body("void StartForwardClimb()", "void CompleteClimb()")
        self.assertIn("MoveJump(destination", section)
        self.assertNotIn("MoveTakeoff", TEXT)
        self.assertIn("CLIMB_CONTROL_TIMEOUT_MS = 1500", TEXT)

    def test_20_climb_explicitly_restores_control(self):
        section = body("void CompleteClimb()", "void StartTypedLanding()")
        self.assertIn("RestoreClientFlightControl(true)", section)
        self.assertIn("ApplyMovementRates(_currentSpeedRate, true)", section)
        restore = body("void RestoreClientFlightControl", "void NormalizeVehicleForExit")
        self.assertIn("SetCanFly(true)", restore)
        self.assertIn("SetDisableGravity(true)", restore)
        self.assertIn("SetAnimTier(AnimTier::Fly)", restore)

    def test_21_blocked_climb_does_not_consume_energy(self):
        climb_spell = body("class spell_g17_dragon_climb", "class spell_g17_dragon_safe_landing")
        self.assertIn("DATA_FLIGHT_STATE) == G17Dragonriding::FLIGHT_STATE_CLIMBING", climb_spell)


class LandingTests(unittest.TestCase):
    def test_22_landing_parachute_aura_is_suppressed(self):
        section = body("class spell_g17_dragon_safe_landing", "class g17_dragonriding_playerscript")
        self.assertIn("PreventHitDefaultEffect(effectIndex)", section)
        self.assertNotIn("AfterCast", section)

    def test_23_no_g17_parachute_cast_path(self):
        self.assertNotIn("SPELL_FALL_SAFETY", TEXT)
        self.assertNotRegex(TEXT, r"CastSpell\([^;]*53208")
        self.assertIn("NonVisualFallGuardEvent", TEXT)

    def test_24_magic_wind_landing(self):
        self.assertIn("case ARCHETYPE_MAGIC", TEXT)
        self.assertIn("VISUAL_KIT_MAGIC_WIND", TEXT)
        self.assertIn("MoveLand(POINT_LAND, destination, 10.0f)", TEXT)

    def test_25_mechanical_rocket_landing(self):
        self.assertIn("case ARCHETYPE_MECHANICAL", TEXT)
        self.assertIn("VISUAL_KIT_MECHANICAL_ROCKET", TEXT)
        self.assertIn("MoveLand(POINT_LAND, destination, 28.0f)", TEXT)

    def test_26_beast_staged_pounce_landing(self):
        self.assertIn("POINT_LAND_APPROACH", TEXT)
        self.assertIn("StartBeastPounce", TEXT)
        self.assertIn("VISUAL_KIT_BEAST_POUNCE", TEXT)
        pounce = body("void StartBeastPounce()", "void CompleteLanding()")
        self.assertIn("MoveJump(destination, 14.0f, 8.0f, POINT_LAND", pounce)

    def test_27_dragon_and_generic_landings(self):
        self.assertIn("case ARCHETYPE_DRAGON", TEXT)
        self.assertIn("MoveLand(POINT_LAND, destination, 18.0f)", TEXT)
        self.assertIn("MoveLand(POINT_LAND, destination, 15.0f)", TEXT)

    def test_28_multilingual_type_inference(self):
        for token in ('"rocket"', '"火箭"', '"dragon"', '"龙"', '"carpet"', '"飞毯"',
                      '"headless"', '"无头骑士"', '"horse"', '"马"'):
            self.assertIn(token, TEXT)


class LifecycleTests(unittest.TestCase):
    def test_29_all_temporary_rates_are_normalized(self):
        cleanup = body("void CleanupPlayer", "class VerifyBoardingEvent")
        for move_type in ("MOVE_RUN", "MOVE_RUN_BACK", "MOVE_SWIM", "MOVE_SWIM_BACK",
                          "MOVE_TURN_RATE", "MOVE_FLIGHT", "MOVE_FLIGHT_BACK", "MOVE_PITCH_RATE"):
            self.assertIn(move_type, cleanup)
        self.assertIn("SetSpeedRate(moveType, 1.0f)", cleanup)

    def test_30_vehicle_exit_normalizes_motion_gravity_pose(self):
        section = body("void NormalizeVehicleForExit()", "void UpdateContinuousFlight")
        self.assertIn("Clear(MOTION_SLOT_ACTIVE)", section)
        self.assertIn("ApplyMovementRates(1.0f, true)", section)
        self.assertIn("SetAnimTier(AnimTier::Ground)", section)
        self.assertIn("SetCanFly(false)", section)
        self.assertIn("SetDisableGravity(false)", section)

    def test_31_death_logout_map_repop_cleanup(self):
        for hook in ("OnPVPKill", "OnPlayerKilledByCreature", "OnLogout", "OnPlayerRepop", "OnMapChanged"):
            self.assertIn(hook, TEXT)
        self.assertIn("!player->IsAlive()", TEXT)
        self.assertIn("player->GetMap() != me->GetMap()", TEXT)

    def test_32_no_teleport_or_unbounded_long_motion(self):
        self.assertNotIn("TeleportTo(", TEXT)
        self.assertNotIn("NearTeleportTo(", TEXT)
        update = body("void UpdateAI(uint32 diff)", "void DoAction(int32 action)")
        self.assertNotIn("MoveLand", update)
        self.assertNotIn("MoveJump", update)
        self.assertNotIn("MoveTakeoff", update)

    def test_33_status_exposes_live_momentum_speed_state(self):
        self.assertIn("DATA_MOMENTUM_PERCENT", TEXT)
        self.assertIn("DATA_SPEED_PERCENT", TEXT)
        self.assertIn("momentum=%u%% speed=%u%% state=%u", TEXT)


if __name__ == "__main__":
    unittest.main(verbosity=2)
