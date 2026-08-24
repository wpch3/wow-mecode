#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import importlib.util
import struct
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ORIGINAL_CPP = ROOT / "original/src/server/scripts/Commands/cs_dragonriding.cpp"
PAYLOAD_CPP = ROOT / "payload/src/server/scripts/Commands/cs_dragonriding.cpp"
PATCHER_PATH = ROOT / "tools/patch_g17r1_client_spell_dbc.py"
INSTALLER_PATH = ROOT / "tools/apply_g17r1_source.py"


def load_module(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


patcher = load_module("g17patcher", PATCHER_PATH)
installer = load_module("g17installer", INSTALLER_PATH)


class SourceFixTests(unittest.TestCase):
    def setUp(self) -> None:
        self.original = ORIGINAL_CPP.read_text(encoding="utf-8")
        self.payload = PAYLOAD_CPP.read_text(encoding="utf-8")

    def test_locked_preimage_and_postimage_hashes(self) -> None:
        self.assertEqual(
            hashlib.sha256(ORIGINAL_CPP.read_bytes()).hexdigest(),
            "c9535dca3390ece6735e6ff6b7418ed99ff206628b5e8febd7b78b05cba999bd",
        )
        self.assertEqual(
            hashlib.sha256(PAYLOAD_CPP.read_bytes()).hexdigest(),
            "10a7002db4c173e441836870da01dd3009a7ba470369ca1a89bdb399ee9b2f45",
        )

    def test_immediate_asynchronous_false_failure_is_removed(self) -> None:
        self.assertIn("player->EnterVehicle(dragon, 0);", self.original)
        self.assertIn("if (player->GetVehicleBase() != dragon)", self.original)
        self.assertNotIn("if (player->GetVehicleBase() != dragon)", self.payload)
        self.assertIn("class VerifyBoardingEvent : public BasicEvent", self.payload)
        self.assertIn("CalculateTime(250ms)", self.payload)
        self.assertLess(
            self.payload.index("player->EnterVehicle(dragon, controlSeat);"),
            self.payload.index("new VerifyBoardingEvent"),
        )

    def test_controllable_seat_is_discovered_and_verified(self) -> None:
        self.assertIn("GetControllableSeatId", self.payload)
        self.assertGreaterEqual(self.payload.count("VEHICLE_SEAT_FLAG_CAN_CONTROL"), 4)
        self.assertIn("dragon->GetCharmerGUID() == _player->GetGUID()", self.payload)
        self.assertIn("player->GetTransSeat()", self.payload)

    def test_visible_diagnostics_exist(self) -> None:
        for marker in (
            "G17R1 summon request",
            "G17R1 vehicle layout",
            "G17R1 vehicle seat",
            "G17R1 boarding scheduled",
            "G17R1 boarding verified",
            "G17R1 boarding verification failed",
            "G17R1 PassengerBoarded",
        ):
            self.assertIn(marker, self.payload)

    def test_c4018_signed_unsigned_warning_is_fixed(self) -> None:
        self.assertNotIn("constexpr int32 BREATH_ENERGY_COST", self.payload)
        self.assertIn("constexpr uint32 BREATH_ENERGY_COST", self.payload)
        self.assertIn("CheckEnergyCast(Unit* caster, uint32 cost)", self.payload)
        self.assertEqual(self.payload.count("-int32(G17Dragonriding::"), 3)

    def test_exact_hash_installer_apply_check_and_rollback(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            source_root = Path(temporary)
            target = source_root / installer.SOURCE_RELATIVE
            target.parent.mkdir(parents=True)
            target.write_bytes(ORIGINAL_CPP.read_bytes())

            installer.check(source_root)
            installer.apply(source_root)
            self.assertEqual(
                hashlib.sha256(target.read_bytes()).hexdigest(), installer.POST_SHA256
            )
            installer.check(source_root)
            installer.apply(source_root)
            installer.rollback(source_root)
            self.assertEqual(
                hashlib.sha256(target.read_bytes()).hexdigest(), installer.PRE_SHA256
            )


class ClientDbcPatcherTests(unittest.TestCase):
    @staticmethod
    def make_dbc(path: Path) -> bytes:
        record_count = 1000
        field_count = 234
        record_size = field_count * 4
        records = [bytearray(record_size) for _ in range(record_count)]
        strings = bytearray(b"\0")

        def add_string(value: str) -> int:
            offset = len(strings)
            strings.extend(value.encode("utf-8") + b"\0")
            return offset

        for row, record in enumerate(records):
            struct.pack_into("<I", record, 0, 100000 + row)

        fixtures = (
            (0, 59961, 78, "Red Proto-Drake", True),
            (1, 61294, 207, "Green Proto-Drake", True),
            (2, 70000, 3, "Outland non-mount", True),
            (3, 70001, 78, "Unrestricted mount", False),
        )
        for row, spell_id, aura, name, restricted in fixtures:
            record = records[row]
            struct.pack_into("<I", record, 0, spell_id)
            if restricted:
                struct.pack_into("<I", record, 8 * 4, 0x84000001)
            struct.pack_into("<I", record, 95 * 4, aura)
            struct.pack_into("<I", record, 136 * 4, add_string(name))

        header = struct.pack(
            "<4s4I", b"WDBC", record_count, field_count, record_size, len(strings)
        )
        data = header + b"".join(records) + bytes(strings)
        path.write_bytes(data)
        return data

    def test_client_patch_is_narrow_deterministic_and_verifiable(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            original_path = root / "Spell.dbc"
            patched_path = root / "client/DBFilesClient/Spell.dbc"
            report_path = root / "report.txt"
            original = self.make_dbc(original_path)

            patcher.patch(original_path, patched_path, report_path)
            patched = patched_path.read_bytes()
            info = patcher.load_dbc(original_path)
            selected = patcher.select_rows(info)
            patcher.verify_diff(info, patched, selected)

            self.assertEqual([patcher.get_u32(info, row, 0) for row in selected], [59961, 61294])
            self.assertEqual(len(original), len(patched))
            self.assertEqual(original[:20], patched[:20])
            self.assertEqual(original[info.strings_offset:], patched[info.strings_offset:])
            report = report_path.read_text(encoding="utf-8")
            self.assertIn("G17R1_CLIENT_SPELL_DBC_PATCH=PASS", report)
            self.assertIn("PROTO_DRAKE_HITS=59961,61294", report)
            self.assertIn("SERVER_DBC_MUST_REMAIN_ORIGINAL=True", report)

            # Idempotent only when the existing output is byte-identical.
            patcher.patch(original_path, patched_path, report_path)
            self.assertEqual(patched, patched_path.read_bytes())

    def test_input_and_output_cannot_be_same(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "Spell.dbc"
            self.make_dbc(path)
            with self.assertRaises(patcher.PatchError):
                patcher.patch(path, path, None)


if __name__ == "__main__":
    unittest.main(verbosity=2)
