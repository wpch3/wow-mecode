#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import importlib.util
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.dont_write_bytecode = True
BATCH = Path(__file__).resolve().parents[1]
ORIGINAL = BATCH / "original/DBFilesClient/AreaTable.dbc"
PAYLOAD = BATCH / "payload/DBFilesClient/AreaTable.dbc"
PATCHER = BATCH / "tools/patch_g17r3_client_areatable_dbc.py"
SERVER_REL = Path("src/server/game/Spells/SpellInfo.cpp")
SERVER_ORIGINAL = BATCH / "original" / SERVER_REL
SERVER_PAYLOAD = BATCH / "payload" / SERVER_REL
SERVER_INSTALLER = BATCH / "tools/apply_g17r3_server_source.py"
SERVER_PRE_SHA256 = "73d52ac0feb67a32822fc0bf086a9174ba7ef0bc186223cdc8a690f48fccb9e2"
SERVER_POST_SHA256 = "c3ec2237ed6da8831662a8b7a5d45cf88f8efc7798cdd35c52a07700fa9cbcbf"

spec = importlib.util.spec_from_file_location("g17r3_patcher", PATCHER)
assert spec and spec.loader
mod = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = mod
spec.loader.exec_module(mod)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


class G17R3Tests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.original = mod.load(ORIGINAL)
        cls.selected = mod.select_rows(cls.original)
        cls.patched = PAYLOAD.read_bytes()
        cls.by_id = mod.rows_by_id(cls.original)

    def test_locked_hashes_and_count(self) -> None:
        self.assertEqual(sha(ORIGINAL), mod.EXPECTED_INPUT_SHA256)
        self.assertEqual(sha(PAYLOAD), mod.EXPECTED_OUTPUT_SHA256)
        self.assertEqual(sha(SERVER_ORIGINAL), SERVER_PRE_SHA256)
        self.assertEqual(sha(SERVER_PAYLOAD), SERVER_POST_SHA256)
        self.assertEqual(len(self.selected), 948)
        self.assertEqual(len(self.patched), len(self.original.data))

    def test_exact_deterministic_diff(self) -> None:
        mod.verify_diff(self.original, self.patched, self.selected)
        self.assertEqual(self.patched, mod.make_patched(self.original, self.selected))
        self.assertEqual(self.original.data[self.original.strings_offset:], self.patched[self.original.strings_offset:])

    def test_every_selected_row_matches_static_client_policy(self) -> None:
        chosen = set(self.selected)
        for row in range(self.original.records):
            map_id = mod.get_u32(self.original, row, mod.FIELD_MAP_ID)
            flags = mod.get_u32(self.original, row, mod.FIELD_FLAGS)
            parent_id = mod.get_u32(self.original, row, mod.FIELD_PARENT_AREA_ID)
            parent_flags = mod.get_u32(self.original, self.by_id[parent_id], mod.FIELD_FLAGS) if parent_id else 0
            combined = flags | parent_flags
            safe = (
                map_id in (0, 1)
                and not combined & mod.HARD_BLOCK
                and not combined & mod.AREA_FLAG_INSIDE
                and not combined & mod.CITY_BLOCK
                and not flags & mod.AREA_FLAG_OUTLAND
            )
            self.assertEqual(row in chosen, safe, f"row={row} area={mod.get_u32(self.original,row,0)}")

    def test_wetlands_zone_becomes_client_flyable(self) -> None:
        for area_id in (11, 118, 205, 298, 1018, 1024):
            row = self.by_id[area_id]
            self.assertIn(row, self.selected)
            after = struct.unpack_from("<I", self.patched, mod.offset(self.original, row, mod.FIELD_FLAGS))[0]
            self.assertTrue(after & mod.AREA_FLAG_OUTLAND)
            self.assertFalse(after & mod.AREA_FLAG_NO_FLY_ZONE)

    def test_static_boundaries_and_absent_inside_metadata(self) -> None:
        chosen = set(self.selected)
        seen = {"hard": 0, "inside": 0, "city": 0, "other_map": 0}
        for row in range(self.original.records):
            flags = mod.get_u32(self.original, row, mod.FIELD_FLAGS)
            parent_id = mod.get_u32(self.original, row, mod.FIELD_PARENT_AREA_ID)
            parent_flags = mod.get_u32(self.original, self.by_id[parent_id], mod.FIELD_FLAGS) if parent_id else 0
            combined = flags | parent_flags
            map_id = mod.get_u32(self.original, row, mod.FIELD_MAP_ID)
            if map_id in (0, 1) and combined & mod.HARD_BLOCK:
                seen["hard"] += 1
                self.assertNotIn(row, chosen)
            if map_id in (0, 1) and combined & mod.AREA_FLAG_INSIDE:
                seen["inside"] += 1
                self.assertNotIn(row, chosen)
            if map_id in (0, 1) and combined & mod.CITY_BLOCK:
                seen["city"] += 1
                self.assertNotIn(row, chosen)
            if map_id not in (0, 1):
                seen["other_map"] += 1
                self.assertNotIn(row, chosen)
        self.assertGreater(seen["hard"], 0)
        self.assertGreater(seen["city"], 0)
        self.assertGreater(seen["other_map"], 0)
        # Locked zhCN 3.3.5a map 0/1 data has no row/parent INSIDE flags.
        # This is a data fact, not a reason to weaken indoor safety.
        self.assertEqual(seen["inside"], 0)

    def test_only_outland_flag_is_added(self) -> None:
        for row in self.selected:
            before = mod.get_u32(self.original, row, mod.FIELD_FLAGS)
            after = struct.unpack_from("<I", self.patched, mod.offset(self.original, row, mod.FIELD_FLAGS))[0]
            self.assertEqual(after ^ before, mod.AREA_FLAG_OUTLAND)

    def test_server_live_outdoor_safety_contract(self) -> None:
        original = SERVER_ORIGINAL.read_text(encoding="utf-8-sig")
        payload = SERVER_PAYLOAD.read_text(encoding="utf-8-sig")
        self.assertNotIn("!player->IsOutdoors()", original)
        self.assertEqual(payload.count("!player->IsOutdoors()"), 1)
        self.assertIn("Player const* player)", payload)
        self.assertIn("!areaEntry || !player", payload)
        self.assertIn("((areaFlags & AREA_FLAG_INSIDE) || !player->IsOutdoors())", payload)
        self.assertIn("area_id, areaEntry, player);", payload)
        self.assertIn("VMap-derived outdoor state", payload)
        indoor_gate = payload.index("!player->IsOutdoors()")
        policy_allow = payload.index("G17R2 old-world pure-flight location allowed")
        self.assertLess(indoor_gate, policy_allow)

    def test_server_source_installer_lifecycle_and_tamper_rejection(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            source_root = Path(temp)
            target = source_root / SERVER_REL
            target.parent.mkdir(parents=True)
            shutil.copy2(SERVER_ORIGINAL, target)
            command = [sys.executable, str(SERVER_INSTALLER)]

            check = subprocess.run(command + ["check", "--source-root", str(source_root)], check=True, text=True, capture_output=True)
            self.assertIn("G17R3_SERVER_SOURCE_STATE=PREIMAGE", check.stdout)
            apply1 = subprocess.run(command + ["apply", "--source-root", str(source_root)], check=True, text=True, capture_output=True)
            self.assertIn("G17R3_SERVER_SOURCE_APPLY=PASS", apply1.stdout)
            self.assertEqual(sha(target), SERVER_POST_SHA256)
            backup = target.with_name(target.name + ".g17r3_before_20260823.bak")
            self.assertEqual(sha(backup), SERVER_PRE_SHA256)

            apply2 = subprocess.run(command + ["apply", "--source-root", str(source_root)], check=True, text=True, capture_output=True)
            self.assertIn("G17R3_SERVER_SOURCE_APPLY=ALREADY_CURRENT", apply2.stdout)
            target.write_bytes(target.read_bytes() + b"tamper")
            rejected = subprocess.run(command + ["rollback", "--source-root", str(source_root)], text=True, capture_output=True)
            self.assertNotEqual(rejected.returncode, 0)
            self.assertIn("not locked R3 postimage", rejected.stderr)

            shutil.copy2(SERVER_PAYLOAD, target)
            rollback1 = subprocess.run(command + ["rollback", "--source-root", str(source_root)], check=True, text=True, capture_output=True)
            self.assertIn("G17R3_SERVER_SOURCE_ROLLBACK=PASS", rollback1.stdout)
            self.assertEqual(sha(target), SERVER_PRE_SHA256)
            rollback2 = subprocess.run(command + ["rollback", "--source-root", str(source_root)], check=True, text=True, capture_output=True)
            self.assertIn("G17R3_SERVER_SOURCE_ROLLBACK=ALREADY_PREIMAGE", rollback2.stdout)

    def test_cli_patch_verify_and_unknown_input_rejection(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            out = root / "AreaTable.dbc"
            report = root / "report.txt"
            run = subprocess.run(
                [sys.executable, str(PATCHER), "patch", "--input", str(ORIGINAL), "--output", str(out), "--report", str(report)],
                check=True, text=True, capture_output=True,
            )
            self.assertIn("G17R3_CLIENT_AREATABLE_PATCH=PASS", run.stdout)
            self.assertIn("LIVE_SERVER_OUTDOOR_CHECK_REQUIRED=True", run.stdout)
            self.assertEqual(out.read_bytes(), self.patched)
            verify = subprocess.run(
                [sys.executable, str(PATCHER), "verify", "--original", str(ORIGINAL), "--patched", str(out)],
                check=True, text=True, capture_output=True,
            )
            self.assertIn("G17R3_CLIENT_AREATABLE_VERIFY=PASS", verify.stdout)
            bad = root / "bad.dbc"
            bad.write_bytes(ORIGINAL.read_bytes()[:-1] + b"X")
            reject = subprocess.run(
                [sys.executable, str(PATCHER), "patch", "--input", str(bad), "--output", str(root / "bad-out.dbc")],
                text=True, capture_output=True,
            )
            self.assertNotEqual(reject.returncode, 0)
            self.assertIn("not the locked", reject.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)
