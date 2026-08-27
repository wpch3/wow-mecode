#!/usr/bin/env python3
"""G17-C3 tests: appender behavior + installer static guards + PS check."""
from __future__ import annotations

import importlib.util
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
APPENDER = ROOT / "tools/append_g17b3r2_spells.py"


def build_synthetic_dbc() -> bytes:
    fields = 234
    recsize = fields * 4
    strings = b"\x00" + "飞行器着陆".encode("utf-8") + b"\x00" + b"decoy\x00"
    strsz = len(strings)

    def row(sid, name_off):
        vals = [0] * fields
        vals[0] = sid
        vals[4] = 0x100
        vals[71] = 3
        vals[140] = name_off
        return struct.pack("<" + "I" * fields, *vals)

    rows = row(52226, 1) + row(52229, 2)
    header = struct.pack("<5I", 0x43424457, 2, fields, recsize, strsz)
    return header + rows + strings


def load_mod():
    spec = importlib.util.spec_from_file_location("app", APPENDER)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


class TestC3Appender(unittest.TestCase):
    def setUp(self):
        self.mod = load_mod()

    def test_01_constants(self):
        self.assertEqual(self.mod.ID_BASE, 990025)
        self.assertEqual(self.mod.COUNT, 4)
        self.assertEqual(self.mod.FIELDS, 234)
        self.assertEqual(len(self.mod.SKILLS), 4)

    def test_02_append_adds_4_and_preserves_all(self):
        with tempfile.TemporaryDirectory() as tmp:
            inp = Path(tmp) / "in.dbc"
            out = Path(tmp) / "out.dbc"
            inp.write_bytes(build_synthetic_dbc())
            args = type("A", (), {"input": str(inp), "output": str(out)})()
            rc = self.mod.do_append(args)
            self.assertEqual(rc, 0)
            data = out.read_bytes()
            magic, count, fields, recsize, strsz = struct.unpack_from("<5I", data, 0)
            self.assertEqual(count, 2 + 4)
            orig = inp.read_bytes()
            # every original record preserved byte-for-byte
            self.assertEqual(data[20:20 + 2 * recsize], orig[20:20 + 2 * recsize])
            # string block prefix preserved
            self.assertEqual(data[20 + count * recsize:20 + count * recsize + len(orig) - 20 - 2 * recsize],
                             orig[20 + 2 * recsize:])
            # appended records verified
            ids = [struct.unpack_from("<I", data, 20 + i * recsize)[0] for i in range(count)]
            self.assertEqual(sorted(ids), [52226, 52229, 990025, 990026, 990027, 990028])
            for i, sid in enumerate((990025, 990026, 990027, 990028)):
                vals = struct.unpack_from("<" + "I" * 234, data, 20 + (2 + i) * recsize)
                self.assertEqual(vals[0], sid)
                self.assertEqual(vals[4], 0x100)   # CASTABLE_WHILE_MOUNTED
                self.assertEqual(vals[71], 3)      # SPELL_EFFECT_DUMMY
                self.assertEqual(vals[28], 1)      # instant
                self.assertEqual(vals[46], 1)      # range
                self.assertIn(vals[133], (279, 2755, 539, 505))  # real icons

    def test_03_check_states(self):
        with tempfile.TemporaryDirectory() as tmp:
            inp = Path(tmp) / "in.dbc"
            out = Path(tmp) / "out.dbc"
            inp.write_bytes(build_synthetic_dbc())
            run = subprocess.run([sys.executable, str(APPENDER), "check", "--input", str(inp)],
                                 capture_output=True, text=True)
            self.assertIn("G17B3R2_SPELL_DBC_STATE=MISSING", run.stdout)
            args = type("A", (), {"input": str(inp), "output": str(out)})()
            self.assertEqual(self.mod.do_append(args), 0)
            run = subprocess.run([sys.executable, str(APPENDER), "check", "--input", str(out)],
                                 capture_output=True, text=True)
            self.assertIn("G17B3R2_SPELL_DBC_STATE=ALREADY_APPENDED", run.stdout)

    def test_04_deterministic(self):
        hashes = []
        for _ in range(2):
            with tempfile.TemporaryDirectory() as tmp:
                inp = Path(tmp) / "in.dbc"
                out = Path(tmp) / "out.dbc"
                inp.write_bytes(build_synthetic_dbc())
                args = type("A", (), {"input": str(inp), "output": str(out)})()
                self.mod.do_append(args)
                import hashlib
                hashes.append(hashlib.sha256(out.read_bytes()).hexdigest())
        self.assertEqual(hashes[0], hashes[1])

    def test_05_version_marker(self):
        run = subprocess.run([sys.executable, str(APPENDER), "--version"],
                             capture_output=True, text=True)
        self.assertIn("G17B3R2_DBC_APPENDER_VERSION=v1_append4", run.stdout)


class TestInstallerStatic(unittest.TestCase):
    def test_01_hash_gates(self):
        install = (ROOT / "Install-G17C3-Skill-Page-Buttons.ps1").read_text(encoding="utf-8")
        # input gate: the C2 image
        self.assertIn('760d3f274ab63fc780867a7193717eeca73b194632b2f69f43cf399faf65e2fe', install)
        self.assertIn("$ExpectedSpellSize = 48981416", install)
        # output gate: the deterministic C3 image
        self.assertIn('006a892b0b3363caedc7436f907948778fe6d084759fa0fc0ddc7f7603c03997', install)
        self.assertIn("$ExpectedAppendedSpellSize = 48985408", install)
        # area + tool gates unchanged from the proven C2 chain
        self.assertIn('1acef997a27f844a8abee9b477c44f2097f745168c6df85ece6b1a135568c233', install)
        self.assertIn('5dc56b130432098d18e911fdf4b3464adf850432b7fbb35003a2feff7c30a79f', install)

    def test_02_flow_markers(self):
        install = (ROOT / "Install-G17C3-Skill-Page-Buttons.ps1").read_text(encoding="utf-8")
        for token in ("G17C3_CLIENT_BAR_BUTTONS_RESULT=PASS",
                      "G17C3_SPELL_DBC_STATE=ALREADY_APPENDED",
                      "G17C2_CLIENT_MPQ_UNLOCK_STATE.txt",
                      "ENV_MODE=C2_STATE",
                      "CLIENT_CACHE_REMOVED=True",
                      "LOCALE_MIRROR=CREATED_ABSENT_SLOT",
                      'G17B3R2_DBC_APPENDER_VERSION\s*=\s*"v1_append4"',
                      'C3_BUILD=" + $BuildFingerprint',
                      '"patch-zhCN-Y.MPQ"'):
            self.assertIn(token, install)
        # mirror contract: locale hash must equal new archive hash
        self.assertIn("locale mirror hash mismatch", install)

    def test_03_ps_files_parse(self):
        checker = ROOT / "tools/ps_static_check.py"
        run = subprocess.run(
            ["python3", str(checker),
             str(ROOT / "Install-G17C3-Skill-Page-Buttons.ps1"),
             str(ROOT / "Rollback-G17C3-Skill-Page-Buttons.ps1")],
            capture_output=True, text=True)
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_04_rollback_contract(self):
        rollback = (ROOT / "Rollback-G17C3-Skill-Page-Buttons.ps1").read_text(encoding="utf-8")
        self.assertIn("G17C3_CLIENT_BAR_BUTTONS_ROLLBACK_RESULT=PASS", rollback)
        self.assertIn("LOCALE_MIRROR_REMOVED=CREATED_BY_C3", rollback)
        self.assertIn("CLIENT_CACHE_REMOVED=True", rollback)

    def test_05_workroot_empty_guard(self):
        # Regression (real user run 2026-08-25): in C2_STATE mode $WorkRoot
        # stays "" and Test-Path -LiteralPath "" throws "Cannot bind argument
        # to parameter 'LiteralPath' because it is an empty string." before
        # ANY write.  The guard must test emptiness BEFORE Test-Path.
        install = (ROOT / "Install-G17C3-Skill-Page-Buttons.ps1").read_text(encoding="utf-8")
        self.assertIn(
            "if (-not $WorkRoot -or -not (Test-Path -LiteralPath $WorkRoot -PathType Container))",
            install)
        self.assertIn('$BuildFingerprint = "v2_workroot_fix"', install)


if __name__ == "__main__":
    unittest.main()
