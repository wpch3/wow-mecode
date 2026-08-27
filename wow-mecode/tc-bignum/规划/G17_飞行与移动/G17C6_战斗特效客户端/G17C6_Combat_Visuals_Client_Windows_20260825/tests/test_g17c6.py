#!/usr/bin/env python3
"""G17-C6 tests: patcher behavior + installer static guards + PS check."""
from __future__ import annotations

import hashlib
import importlib.util
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PATCHER = ROOT / "tools/patch_g17b3r5_visuals.py"


def build_synthetic_dbc() -> bytes:
    fields, recsize = 234, 936
    strings = b"\x00" + "飞行器着陆".encode("utf-8") + b"\x00" + b"decoy\x00"
    strsz = len(strings)
    def row(sid):
        vals = [0] * fields
        vals[0] = sid
        vals[4] = 0x100
        vals[71] = 3
        vals[46] = 1
        vals[140] = 1
        return struct.pack("<" + "I" * fields, *vals)
    ids = [52226] + list(range(990000, 990026))
    rows = b"".join(row(sid) for sid in ids)
    header = struct.pack("<5I", 0x43424457, len(ids), fields, recsize, strsz)
    return header + rows + strings


def load_mod():
    spec = importlib.util.spec_from_file_location("app", PATCHER)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


class TestC6Patcher(unittest.TestCase):
    def test_01_patch_and_check(self):
        with tempfile.TemporaryDirectory() as tmp:
            inp = Path(tmp) / "in.dbc"
            out = Path(tmp) / "out.dbc"
            inp.write_bytes(build_synthetic_dbc())
            run = subprocess.run([sys.executable, str(PATCHER), "check", "--input", str(inp)],
                                 capture_output=True, text=True)
            self.assertIn("G17B3R5_VISUAL_STATE=FRESH", run.stdout)
            run = subprocess.run([sys.executable, str(PATCHER), "patch", "--input", str(inp), "--output", str(out)],
                                 capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn("G17B3R5_VISUAL_PATCH=PATCHED", run.stdout)
            self.assertIn("G17B3R5_VISUAL_PATCHED_RECORDS=25", run.stdout)
            self.assertEqual(inp.stat().st_size, out.stat().st_size)
            run = subprocess.run([sys.executable, str(PATCHER), "check", "--input", str(out)],
                                 capture_output=True, text=True)
            self.assertIn("G17B3R5_VISUAL_STATE=COMPLETE", run.stdout)

    def test_02_visuals_per_block(self):
        with tempfile.TemporaryDirectory() as tmp:
            inp = Path(tmp) / "in.dbc"
            out = Path(tmp) / "out.dbc"
            inp.write_bytes(build_synthetic_dbc())
            subprocess.run([sys.executable, str(PATCHER), "patch", "--input", str(inp), "--output", str(out)],
                           capture_output=True, text=True)
            data = out.read_bytes()
            count = struct.unpack_from("<I", data, 4)[0]
            recs = data[20:20 + count * 936]
            expected = [1483, 6587, 7749, 98, 219]
            for i in range(count):
                sid = struct.unpack_from("<I", recs, i * 936)[0]
                if 990000 <= sid <= 990024:
                    block = (sid - 990000) // 5
                    vis = struct.unpack_from("<I", recs, i * 936 + 131 * 4)[0]
                    rng = struct.unpack_from("<I", recs, i * 936 + 46 * 4)[0]
                    self.assertEqual(vis, expected[block], f"sid {sid}")
                    self.assertEqual(rng, 4, f"sid {sid}")

    def test_03_version(self):
        run = subprocess.run([sys.executable, str(PATCHER), "--version"], capture_output=True, text=True)
        self.assertIn("G17B3R5_VISUAL_PATCHER_VERSION=v1_visuals_range", run.stdout)


class TestInstallerStatic(unittest.TestCase):
    def test_01_hash_gates(self):
        install = (ROOT / "Install-G17C6-Combat-Visuals.ps1").read_text(encoding="utf-8")
        self.assertIn('006a892b0b3363caedc7436f907948778fe6d084759fa0fc0ddc7f7603c03997', install)
        self.assertIn('5db5b7a52a4fad0e7c05ed6127fe95a437dce158332ae9b626ec99e2b7855e9b', install)
        self.assertIn('$ExpectedSpellSize = 48985408', install)

    def test_02_flow_markers(self):
        install = (ROOT / "Install-G17C6-Combat-Visuals.ps1").read_text(encoding="utf-8")
        for token in ("G17C6_CLIENT_VISUALS_RESULT=PASS",
                      "G17C6_VISUAL_STATE=ALREADY_COMPLETE",
                      "G17C3_CLIENT_BAR_BUTTONS_STATE.txt",
                      "ENV_MODE=C3_STATE",
                      "ENV_MODE=DISCOVERY",
                      "CLIENT_CACHE_REMOVED=True",
                      'G17B3R5_VISUAL_PATCHER_VERSION\s*=\s*"v1_visuals_range"',
                      'C6_BUILD=" + $BuildFingerprint',
                      '"patch-zhCN-" + $s + ".MPQ"'):
            self.assertIn(token, install)
        self.assertIn("locale mirror hash mismatch", install)

    def test_03_ps_files_parse(self):
        checker = ROOT / "tools/ps_static_check.py"
        run = subprocess.run(
            ["python3", str(checker),
             str(ROOT / "Install-G17C6-Combat-Visuals.ps1"),
             str(ROOT / "Rollback-G17C6-Combat-Visuals.ps1")],
            capture_output=True, text=True)
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_04_rollback_contract(self):
        rollback = (ROOT / "Rollback-G17C6-Combat-Visuals.ps1").read_text(encoding="utf-8")
        self.assertIn("G17C6_CLIENT_VISUALS_ROLLBACK_RESULT=PASS", rollback)
        self.assertIn("CLIENT_CACHE_REMOVED=True", rollback)


if __name__ == "__main__":
    unittest.main()
