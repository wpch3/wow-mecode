#!/usr/bin/env python3
"""G17-C2 (B3) tests: appender behavior + installer static guards + PS check."""
from __future__ import annotations

import importlib.util
import struct
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
APPENDER = ROOT / "tools/append_g17b3_spells.py"


def build_synthetic_dbc() -> bytes:
    fields = 234
    recsize = fields * 4
    strings = b"\x00" + "飞行器着陆".encode("utf-8") + b"\x00" + b"decoy\x00"
    strsz = len(strings)

    def row(sid, focus, aura, name_off):
        vals = [0] * fields
        vals[0] = sid
        vals[4] = 0x100
        vals[18] = focus
        vals[24] = aura
        vals[71] = 3
        vals[140] = name_off
        return struct.pack("<" + "I" * fields, *vals)

    rows = row(52226, 1553, 52255, 1) + row(52229, 0, 0, 2)
    header = struct.pack("<5I", 0x43424457, 2, fields, recsize, strsz)
    return header + rows + strings


def load_mod():
    spec = importlib.util.spec_from_file_location("app", APPENDER)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


class TestB3Appender(unittest.TestCase):
    def setUp(self):
        self.mod = load_mod()

    def test_01_append_adds_25_and_preserves_all(self):
        with tempfile.TemporaryDirectory() as tmp:
            inp = Path(tmp) / "in.dbc"
            out = Path(tmp) / "out.dbc"
            inp.write_bytes(build_synthetic_dbc())
            args = type("A", (), {"input": str(inp), "output": str(out)})()
            rc = self.mod.do_append(args)
            self.assertEqual(rc, 0)
            data = out.read_bytes()
            magic, count, fields, recsize, strsz = struct.unpack_from("<5I", data, 0)
            self.assertEqual(count, 2 + 25)
            orig_recs = inp.read_bytes()[20:20 + 2 * 234 * 4]
            new_recs = data[20:20 + 2 * 234 * 4]
            self.assertEqual(orig_recs, new_recs)
            ids = {struct.unpack_from("<I", data, 20 + i * recsize)[0]
                   for i in range(count)}
            for k in range(25):
                self.assertIn(990000 + k, ids)

    def test_02_idempotent(self):
        with tempfile.TemporaryDirectory() as tmp:
            inp = Path(tmp) / "in.dbc"
            out = Path(tmp) / "out.dbc"
            out2 = Path(tmp) / "out2.dbc"
            inp.write_bytes(build_synthetic_dbc())
            a1 = type("A", (), {"input": str(inp), "output": str(out)})()
            self.assertEqual(self.mod.do_append(a1), 0)
            a2 = type("A", (), {"input": str(out), "output": str(out2)})()
            self.assertEqual(self.mod.do_append(a2), 0)
            self.assertFalse(out2.exists())

    def test_03_appended_records_are_dummy_carriers(self):
        with tempfile.TemporaryDirectory() as tmp:
            inp = Path(tmp) / "in.dbc"
            out = Path(tmp) / "out.dbc"
            inp.write_bytes(build_synthetic_dbc())
            a = type("A", (), {"input": str(inp), "output": str(out)})()
            self.assertEqual(self.mod.do_append(a), 0)
            data = out.read_bytes()
            _m, count, fields, recsize, strsz = struct.unpack_from("<5I", data, 0)
            for k in range(25):
                off = 20 + (2 + k) * recsize
                vals = struct.unpack_from("<" + "I" * fields, data, off)
                self.assertEqual(vals[0], 990000 + k)
                self.assertEqual(vals[4], 0x100)
                self.assertEqual(vals[71], 3)
                self.assertNotEqual(vals[140], 0)


class TestInstallerStatic(unittest.TestCase):
    def test_01_uses_appender_and_expected_hashes(self):
        install = (ROOT / "Install-G17C2-Combat-Skills.ps1").read_text(
            encoding="utf-8")
        self.assertIn("append_g17b3_spells.py", install)
        self.assertIn('"03bf11fdeff7c296837fc6b0cc335476a9df33965baf8eed8ca671529577ccba"',
                      install)
        self.assertIn('"760d3f274ab63fc780867a7193717eeca73b194632b2f69f43cf399faf65e2fe"',
                      install)
        self.assertIn("C2_PATCHER_VERSION_CHECK=PASS", install)

    def test_02_ps_files_parse_clean(self):
        checker = ROOT / "tools/ps_static_check.py"
        run = subprocess.run(
            ["python3", str(checker),
             str(ROOT / "Install-G17C2-Combat-Skills.ps1"),
             str(ROOT / "Rollback-G17C2-Combat-Skills.ps1")],
            capture_output=True, text=True)
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)
