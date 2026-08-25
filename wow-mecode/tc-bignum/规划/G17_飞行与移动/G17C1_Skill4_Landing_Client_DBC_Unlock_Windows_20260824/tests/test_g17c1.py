#!/usr/bin/env python3
"""Behavior tests for the G17 Spell.dbc unlock (spell 52226)."""
from __future__ import annotations

import struct
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PATCHER = ROOT / "tools/patch_g17c1_spell_dbc.py"


def build_synthetic_dbc(pre_cleaned: bool = False,
                        name_encoding: str = "utf-8",
                        fields: int = 234) -> bytes:
    """Minimal WDBC with the 52226 row plus one decoy row.

    Real column layout (SpellEntry order): `fields` columns; only the columns
    the patcher audits matter; missing columns are zero-filled.
    """
    recsize = fields * 4
    # DBC string block: index 0 MUST be the empty string.
    strings = (b"\x00" + "飞行器着陆".encode(name_encoding) + b"\x00" + b"decoy\x00")
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

    rows = (row(52226, 0 if pre_cleaned else 1553,
                0 if pre_cleaned else 52255, 1)
            + row(52229, 0, 0, 2))
    header = struct.pack("<5I", 0x43424457, 2, fields, recsize, strsz)
    return header + rows + strings


def load_module():
    import importlib.util
    spec = importlib.util.spec_from_file_location("patcher", PATCHER)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


class TestPatcher(unittest.TestCase):
    def setUp(self):
        self.mod = load_module()
        self.dbc = build_synthetic_dbc()

    def test_01_guards_detect_real_shape(self):
        audit = self.mod.audit(self.dbc)
        self.assertTrue(audit["ok"])
        self.assertEqual(audit["name"], "飞行器着陆")
        self.assertTrue(audit["layout_ok"])

    def test_02_patch_clears_only_the_two_gates(self):
        with tempfile.TemporaryDirectory() as tmp:
            inp = Path(tmp) / "Spell.in.dbc"
            out = Path(tmp) / "Spell.out.dbc"
            rep = Path(tmp) / "report.txt"
            inp.write_bytes(self.dbc)
            args = type("A", (), {"input": str(inp), "output": str(out),
                                  "report": str(rep)})()
            rc = self.mod.do_patch(args)
            self.assertEqual(rc, 0)
            magic, count, fields, recsize, strsz = struct.unpack_from(
                "<5I", out.read_bytes(), 0)
            self.assertEqual(count, 2)
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
        with tempfile.TemporaryDirectory() as tmp:
            inp = Path(tmp) / "Spell.in.dbc"
            out = Path(tmp) / "Spell.out.dbc"
            rep = Path(tmp) / "report.txt"
            inp.write_bytes(self.dbc)
            args = type("A", (), {"input": str(inp), "output": str(out),
                                  "report": str(rep)})()
            self.assertEqual(self.mod.do_patch(args), 0)
            first = out.read_bytes()
            rep2 = Path(tmp) / "report2.txt"
            out2 = Path(tmp) / "x.dbc"
            args2 = type("A", (), {"input": str(out), "output": str(out2),
                                   "report": str(rep2)})()
            self.assertEqual(self.mod.do_patch(args2), 0)
            self.assertFalse(out2.exists(), "idempotent run must not rewrite")

    def test_04_refuses_foreign_record(self):
        foreign = bytearray(self.dbc)
        struct.pack_into("<I", foreign, 20 + 0 * 234 * 4 + 140 * 4, 0)
        with tempfile.TemporaryDirectory() as tmp:
            inp = Path(tmp) / "bad.dbc"
            out = Path(tmp) / "bad.out.dbc"
            rep = Path(tmp) / "rep.txt"
            inp.write_bytes(bytes(foreign))
            args = type("A", (), {"input": str(inp), "output": str(out),
                                  "report": str(rep)})()
            self.assertEqual(self.mod.do_patch(args), 2)
            self.assertFalse(out.exists())

    def test_05_already_clean_is_pass_and_writes_nothing(self):
        # Regression (user's real server DBC): 52226 focus/aura already 0/0.
        # Must return success without rewriting and report ALREADY_CLEAN.
        with tempfile.TemporaryDirectory() as tmp:
            inp = Path(tmp) / "clean.dbc"
            out = Path(tmp) / "should_not_exist.dbc"
            rep = Path(tmp) / "rep.txt"
            inp.write_bytes(build_synthetic_dbc(pre_cleaned=True))
            args = type("A", (), {"input": str(inp), "output": str(out),
                                  "report": str(rep)})()
            self.assertEqual(self.mod.do_patch(args), 0)
            self.assertFalse(out.exists())
            self.assertIn("G17C1_SPELL_DBC_STATE=ALREADY_CLEAN",
                          rep.read_text(encoding="utf-8"))
            self.assertIn("G17C1_SPELL_DBC_WRITE=NONE",
                          rep.read_text(encoding="utf-8"))

    def test_06_gbk_name_is_recognized(self):
        # zhCN server DBCs from some toolchains store GBK strings.
        a = self.mod.audit(build_synthetic_dbc(name_encoding="gbk"))
        self.assertTrue(a["name_ok"], a)
        self.assertEqual(a["name_encoding"], "gbk")

    def test_07_foreign_layout_is_refused(self):
        # A DBC with a different field count must never be interpreted.
        with tempfile.TemporaryDirectory() as tmp:
            inp = Path(tmp) / "foreign.dbc"
            out = Path(tmp) / "foreign.out.dbc"
            rep = Path(tmp) / "rep.txt"
            inp.write_bytes(build_synthetic_dbc(fields=218))
            args = type("A", (), {"input": str(inp), "output": str(out),
                                  "report": str(rep)})()
            self.assertEqual(self.mod.do_patch(args), 3)
            self.assertFalse(out.exists())
            self.assertIn("G17C1_SPELL_DBC_STATE=LAYOUT_UNKNOWN",
                          rep.read_text(encoding="utf-8"))


    def test_08_installer_state_write_is_idempotent(self):
        # Regression (user's 3rd B2R4 run): "Cannot create a file when that
        # file already exists" - the ALREADY_CLEAN branch wrote state to a
        # .tmp then Move-Item onto an EXISTING state file without -Force
        # (the second run fails).  All installer state writes must be
        # overwrite-safe and all stale/swap temps must be cleaned up instead
        # of throwing, so re-running any step is always safe.
        install = (ROOT / "Install-G17C1-Client-MPQ.ps1").read_text(encoding="utf-8")
        rollback = (ROOT / "Rollback-G17C1-Client-MPQ.ps1").read_text(encoding="utf-8")
        self.assertIn("-Force", install)
        self.assertIn("Move-Item -LiteralPath $StateTemp -Destination $StateFile -Force",
                      install)
        for script in (install, rollback):
            self.assertNotIn("throw \"state temp exists\"", script)
            self.assertNotIn("throw \"swap temp path exists\"", script)
            self.assertNotIn("throw \"temp target exists\"", script)
            self.assertNotIn("throw \"locale swap temp exists\"", script)
            self.assertNotIn("throw \"swap temp exists\"", script)
            self.assertNotIn("throw \"state temp exists\"", script)


    def test_09_installer_falls_back_when_state_files_missing(self):
        # Regression (user's C1 run): required file missing
        # G17R4_CLIENT_MPQ_UPGRADE_STATE.txt.  The installer must not hard-fail
        # when the R4/R5 state files are absent; it must fall back to content
        # discovery of the R4 chain (root MPQ whose Spell.dbc == dd250911... /
        # 03bf11fd... with AreaTable 1acef997...) plus byte-identical zhCN
        # mirror.  Static assertions on the PS1.
        install = (ROOT / "Install-G17C1-Client-MPQ.ps1").read_text(
            encoding="utf-8")
        self.assertIn("function Discover-ClientEnvironment", install)
        self.assertIn("ENV_MODE=DISCOVERY", install)
        self.assertIn("ENV_REASON=state_files_missing", install)
        self.assertIn("expected exactly one R4 chain owner", install)
        self.assertIn("BYTE_IDENTICAL", install)
        # The old hard failure must be gone from the runtime path.  The ONLY
        # 'required file missing' loop must list the tool and patcher; the
        # R4/R5 state files must NOT be in that list (this was the real bug:
        # v4 added the fallback but left them in the hard-prerequisite loop,
        # so the user's run still died BEFORE reaching ENV_MODE=DISCOVERY).
        import re
        # The installer's hard-prerequisite loop must exist and must contain
        # ONLY the tool and patcher (no state files).  Match the literal text
        # instead of a fragile regex.
        # The installer's hard-prerequisite loop must list ONLY the tool and
        # patcher - never the state files.  Check the literal foreach line.
        self.assertIn("foreach ($Required in @($Tool, $Patcher))", install)
        # ensure the state-file names are never in a Required foreach array
        import re as _re
        for m in _re.finditer(r"foreach\s+\$Required\s+in\s+@\(([^)]*)\)",
                              install):
            self.assertNotIn("R4StateFile", m.group(1))
            self.assertNotIn("R5StateFile", m.group(1))
            self.assertIn("$Tool", m.group(1))
            self.assertIn("$Patcher", m.group(1))
        # The fallback must come BEFORE any possible state-based resolve.
        self.assertLess(install.index("Discover-ClientEnvironment"),
                        install.index("$R4 = Read-KeyValueFile"))
        # Already-unlocked root must yield ALREADY_CURRENT without writing.
        self.assertIn("G17C1_SPELL_DBC_STATE=ALREADY_CLEAN", install)
        self.assertIn("G17C1_CLIENT_MPQ_UNLOCK=ALREADY_CURRENT", install)
        # Discovery-created locale must not try to swap an absent file.
        self.assertIn("LOCALE_MIRROR=CREATED_ABSENT_SLOT", install)
        self.assertIn("BACKUP_LOCALE=NONE_ABSENT", install)


    def test_10_patch_writes_into_missing_nested_parent(self):
        # Regression (user's C1 run): Spell.dbc patch failed with
        # FileNotFoundError writing ...workroot\generated\DBFilesClient\Spell.dbc
        # because the installer never created 'generated\DBFilesClient' and
        # Path.write_bytes does not create parents.  The patcher must create
        # output/report parent directories itself.
        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp)
            inp = base / "Spell.in.dbc"
            inp.write_bytes(build_synthetic_dbc())
            deep_out = base / "nested" / "a" / "b" / "DBFilesClient" / "Spell.out.dbc"
            deep_rep = base / "nested" / "a" / "report.txt"
            args = type("A", (), {"input": str(inp), "output": str(deep_out),
                                  "report": str(deep_rep)})()
            rc = self.mod.do_patch(args)
            self.assertEqual(rc, 0)
            self.assertTrue(deep_out.exists())
            self.assertTrue(deep_rep.exists())
            # the two gates are actually cleared
            data = deep_out.read_bytes()
            count, fields, recsize = struct.unpack_from("<5I", data, 0)[1],                 struct.unpack_from("<5I", data, 0)[2], struct.unpack_from("<5I", data, 0)[3]
            recs = data[20:20 + count * recsize]
            vals = struct.unpack_from("<" + "I" * fields, recs, 33132 * recsize) if False else None
            # verify 52226 row (record 0 in synthetic, but re-scan)
            for i in range(count):
                v = struct.unpack_from("<" + "I" * fields, recs, i * recsize)
                if v[0] == 52226:
                    self.assertEqual((v[18], v[24]), (0, 0))
                    break
            else:
                self.fail("52226 not found")

    def test_11_installer_creates_generated_parent_before_patch(self):
        install = (ROOT / "Install-G17C1-Client-MPQ.ps1").read_text(
            encoding="utf-8")
        self.assertIn("Split-Path -Parent $GeneratedSpell", install)
        idx_mkdir = install.index("Split-Path -Parent $GeneratedSpell")
        idx_patch = install.index("SPELL_DBC_PATCH_EXIT")
        self.assertLess(idx_mkdir, idx_patch)


if __name__ == "__main__":
    unittest.main(verbosity=2)
