#!/usr/bin/env python3
"""Behavior tests for the B2R4 server Spell.dbc unlock (spell 52226)."""
from __future__ import annotations

import hashlib
import struct
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PATCHER = ROOT / "tools/patch_g17c1_spell_dbc.py"

# Project real 3.3.5a zhCN Spell.dbc (R3 create image, as shipped to client)
# - SHA256 dd25091167f671764735ce88c78b66485c6d661fadf05d322574c261f6e464ea
# (embedded as bytes below would be 48MB; tests use the small synthetic DBC.)
def build_synthetic_dbc() -> bytes:
    """Build a minimal WDBC with the 52226 row plus one decoy row.

    Real column layout (SpellEntry order): 234 fields; only the columns our
    patcher audits matter; missing columns are zero-filled.
    """
    fields = 234
    recsize = fields * 4
    # DBC string block: index 0 MUST be the empty string.
    strings = (b"\x00" + "飞行器着陆".encode("utf-8") + b"\x00" + b"decoy\x00")
    strsz = len(strings)
    # offset 1 -> landing, then -> decoy
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


class TestPatcher(unittest.TestCase):
    def setUp(self):
        self.dbc = build_synthetic_dbc()

    def test_01_guards_detect_real_shape(self):
        import importlib.util
        spec = importlib.util.spec_from_file_location("p", PATCHER)
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        audit = mod.audit(self.dbc)
        self.assertTrue(audit["ok"])
        self.assertEqual(audit["name"], "飞行器着陆")

    def test_02_patch_clears_only_the_two_gates(self):
        import importlib.util
        spec = importlib.util.spec_from_file_location("p", PATCHER)
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        with tempfile.TemporaryDirectory() as tmp:
            inp = Path(tmp) / "Spell.in.dbc"
            out = Path(tmp) / "Spell.out.dbc"
            rep = Path(tmp) / "report.txt"
            inp.write_bytes(self.dbc)
            rc = mod.do_patch(type("A", (), {
                "input": str(inp), "output": str(out), "report": str(rep)})())
            self.assertEqual(rc, 0)
            # same size, string block untouched
            self.assertEqual(out.read_bytes().__len__(), len(self.dbc))
            magic, count, fields, recsize, strsz = struct.unpack_from("<5I", out.read_bytes(), 0)
            self.assertEqual(count, 2)
            # 52226 gates cleared, 52229 untouched
            data = out.read_bytes()
            recs = data[20:20 + count * recsize]
            for i in range(count):
                vals = struct.unpack_from("<" + "I" * fields, recs, i * recsize)
                if vals[0] == 52226:
                    self.assertEqual((vals[18], vals[24]), (0, 0))
                elif vals[0] == 52229:
                    self.assertEqual((vals[18], vals[24]), (0, 0))
                    self.assertEqual(vals[140], 2)

    def test_03_idempotent(self):
        import importlib.util
        spec = importlib.util.spec_from_file_location("p", PATCHER)
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        with tempfile.TemporaryDirectory() as tmp:
            inp = Path(tmp) / "Spell.in.dbc"
            out = Path(tmp) / "Spell.out.dbc"
            rep = Path(tmp) / "report.txt"
            inp.write_bytes(self.dbc)
            mod.do_patch(type("A", (), {"input": str(inp), "output": str(out), "report": str(rep)})())
            first = out.read_bytes()
            rep2 = Path(tmp) / "report2.txt"
            rc = mod.do_patch(type("A", (), {"input": str(out), "output": str(Path(tmp) / "x.dbc"), "report": str(rep2)})())
            self.assertEqual(rc, 0)
            self.assertFalse((Path(tmp) / "x.dbc").exists(), "idempotent run must not rewrite")

    def test_04_refuses_foreign_layout(self):
        import importlib.util
        spec = importlib.util.spec_from_file_location("p", PATCHER)
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        foreign = bytearray(self.dbc)
        # break the name so guard fails
        struct.pack_into("<I", foreign, 20 + 0 * 234 * 4 + 140 * 4, 0)
        with tempfile.TemporaryDirectory() as tmp:
            inp = Path(tmp) / "bad.dbc"
            out = Path(tmp) / "bad.out.dbc"
            rep = Path(tmp) / "rep.txt"
            inp.write_bytes(bytes(foreign))
            rc = mod.do_patch(type("A", (), {"input": str(inp), "output": str(out), "report": str(rep)})())
            self.assertEqual(rc, 2)
            self.assertFalse(out.exists(), "must not write on guard failure")


if __name__ == "__main__":
    unittest.main(verbosity=2)
