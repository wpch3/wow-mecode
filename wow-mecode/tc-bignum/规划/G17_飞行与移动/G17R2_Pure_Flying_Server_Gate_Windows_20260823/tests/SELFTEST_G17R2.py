#!/usr/bin/env python3
"""Static, policy-matrix, installer and compile smoke tests for G17-R2."""

from __future__ import annotations

import hashlib
import importlib.util
import shutil
import subprocess
import sys
import tempfile
import textwrap
import unittest
from pathlib import Path

BATCH = Path(__file__).resolve().parents[1]
REL = Path("src/server/game/Spells/SpellInfo.cpp")
ORIGINAL = BATCH / "original" / REL
PAYLOAD = BATCH / "payload" / REL
INSTALLER = BATCH / "tools" / "apply_g17r2_source.py"
PRE_SHA = "537e5c350baa5f4a90bd0ec38c6b6858360e287aeabd75ab54050b4432e50755"
POST_SHA = "73d52ac0feb67a32822fc0bf086a9174ba7ef0bc186223cdc8a690f48fccb9e2"

NO_FLY = 1 << 0
ARENA = 1 << 1
ARENA_INSTANCE = 1 << 2
INSIDE = 1 << 3
CAPITAL = 1 << 4
CITY = 1 << 5


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def g17_allowed(*, enabled=True, allow_old=True, map_id=0, world_map=True,
                area_exists=True, flags=0, block_indoor=True, block_city=True,
                blocked=False) -> bool:
    if not enabled or not allow_old or not area_exists:
        return False
    if not world_map or map_id not in (0, 1):
        return False
    if flags & (NO_FLY | ARENA | ARENA_INSTANCE):
        return False
    if block_indoor and flags & INSIDE:
        return False
    if block_city and flags & (CAPITAL | CITY):
        return False
    return not blocked


def strict_result(*, policy: bool, area_exists=True, player_exists=True,
                  dbc_flyable=False, can_fly=False) -> bool:
    if not area_exists or not player_exists:
        return False
    return policy or (dbc_flyable and can_fly)


def non_strict_result(*, policy: bool, original_continent=False) -> bool:
    return policy or original_continent


class G17R2Tests(unittest.TestCase):
    def test_locked_hashes(self) -> None:
        self.assertEqual(sha(ORIGINAL), PRE_SHA)
        self.assertEqual(sha(PAYLOAD), POST_SHA)

    def test_exact_scope_and_markers(self) -> None:
        old = ORIGINAL.read_bytes()
        new = PAYLOAD.read_bytes()
        self.assertNotEqual(old, new)
        self.assertEqual(new.count(b"G17R2 old-world pure-flight location allowed"), 1)
        self.assertEqual(new.count(b"if (!g17OldWorldAllowed &&"), 1)
        self.assertNotIn(
            b"(!areaEntry->IsFlyable() && !g17OldWorldAllowed) ||\r\n"
            b"                !player->CanFlyInZone(map_id, zone_id, this)",
            new,
        )
        # R2 changes only the strict location block; the G17 safety helper is byte-identical.
        start = old.index(b"// G17-WORLD-FLIGHT-BEGIN")
        end = old.index(b"// G17-WORLD-FLIGHT-END") + len(b"// G17-WORLD-FLIGHT-END")
        self.assertEqual(old[start:end], new[start:end])

    def test_wetlands_and_old_world_safe_outdoor_pass_both_paths(self) -> None:
        for map_id in (0, 1):
            policy = g17_allowed(map_id=map_id)
            self.assertTrue(policy)
            self.assertTrue(strict_result(policy=policy, dbc_flyable=False, can_fly=False))
            self.assertTrue(non_strict_result(policy=policy, original_continent=False))

    def test_feature_disabled_restores_original_rejection(self) -> None:
        policy = g17_allowed(enabled=False)
        self.assertFalse(strict_result(policy=policy, dbc_flyable=False, can_fly=False))
        self.assertFalse(non_strict_result(policy=policy, original_continent=False))

    def test_old_world_safety_boundaries_reject(self) -> None:
        cases = [
            dict(flags=NO_FLY),
            dict(flags=ARENA),
            dict(flags=ARENA_INSTANCE),
            dict(flags=INSIDE),
            dict(flags=CITY),
            dict(flags=CAPITAL),
            dict(blocked=True),
            dict(world_map=False),
            dict(map_id=530),
        ]
        for case in cases:
            with self.subTest(case=case):
                policy = g17_allowed(**case)
                self.assertFalse(policy)
                self.assertFalse(strict_result(policy=policy, dbc_flyable=False, can_fly=False))

    def test_hard_boundaries_cannot_be_disabled(self) -> None:
        for flags in (NO_FLY, ARENA, ARENA_INSTANCE):
            self.assertFalse(g17_allowed(flags=flags, block_indoor=False, block_city=False))
        self.assertTrue(g17_allowed(flags=INSIDE, block_indoor=False))
        self.assertTrue(g17_allowed(flags=CITY, block_city=False))

    def test_outland_northrend_original_rules_do_not_regress(self) -> None:
        for map_id in (530, 571):
            policy = g17_allowed(map_id=map_id)
            self.assertFalse(policy)
            self.assertTrue(strict_result(policy=policy, dbc_flyable=True, can_fly=True))
            self.assertTrue(non_strict_result(policy=policy, original_continent=True))
            self.assertFalse(strict_result(policy=policy, dbc_flyable=True, can_fly=False))

    def test_installer_apply_idempotent_rollback(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            target = root / REL
            target.parent.mkdir(parents=True)
            shutil.copy2(ORIGINAL, target)
            cmd = [sys.executable, str(INSTALLER), "apply", "--source-root", str(root)]
            first = subprocess.run(cmd, check=True, text=True, capture_output=True)
            self.assertIn("G17R2_SOURCE_APPLY=PASS", first.stdout)
            self.assertEqual(sha(target), POST_SHA)
            second = subprocess.run(cmd, check=True, text=True, capture_output=True)
            self.assertIn("G17R2_SOURCE_APPLY=ALREADY_CURRENT", second.stdout)
            rollback = subprocess.run(
                [sys.executable, str(INSTALLER), "rollback", "--source-root", str(root)],
                check=True, text=True, capture_output=True,
            )
            self.assertIn("G17R2_SOURCE_ROLLBACK=PASS", rollback.stdout)
            self.assertEqual(sha(target), PRE_SHA)

    def test_installer_rejects_unknown_target(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            target = root / REL
            target.parent.mkdir(parents=True)
            target.write_bytes(b"unknown\n")
            run = subprocess.run(
                [sys.executable, str(INSTALLER), "apply", "--source-root", str(root)],
                text=True, capture_output=True,
            )
            self.assertNotEqual(run.returncode, 0)
            self.assertIn("refusing overwrite", run.stderr)
            self.assertEqual(target.read_bytes(), b"unknown\n")

    def test_cpp_policy_expression_compiles_and_runs(self) -> None:
        compiler = shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            self.skipTest("no C++ compiler")
        source = textwrap.dedent(r"""
            #include <cassert>
            bool strict_gate(bool area, bool player, bool g17, bool flyable, bool canFly)
            {
                if (!area || !player)
                    return false;
                if (!g17 && (!flyable || !canFly))
                    return false;
                return true;
            }
            int main()
            {
                assert(strict_gate(true, true, true, false, false));
                assert(!strict_gate(true, true, false, false, false));
                assert(strict_gate(true, true, false, true, true));
                assert(!strict_gate(true, true, false, true, false));
                assert(!strict_gate(false, true, true, false, false));
            }
        """)
        with tempfile.TemporaryDirectory() as temp:
            src = Path(temp) / "gate.cpp"
            exe = Path(temp) / "gate"
            src.write_text(source, encoding="utf-8")
            subprocess.run([compiler, "-std=c++17", str(src), "-o", str(exe)], check=True)
            subprocess.run([str(exe)], check=True)


if __name__ == "__main__":
    unittest.main(verbosity=2)
