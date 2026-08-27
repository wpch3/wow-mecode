
#!/usr/bin/env python3
"""G17-B3R5 (combat visuals) behavior tests."""
from __future__ import annotations

import hashlib
import importlib.util
import re
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ORIGINAL = ROOT / "original_src/src/server/scripts/Commands/cs_dragonriding.cpp"
PAYLOAD = ROOT / "payload_src/src/server/scripts/Commands/cs_dragonriding.cpp"
ROLLBACK = ROOT / "rollback_safe_src/src/server/scripts/Commands/cs_dragonriding.cpp"
TOOL = ROOT / "tools/apply_g17b3r5_source.py"
VISUALS = ROOT / "tools/patch_g17b3r5_visuals.py"
ADDON_LUA = ROOT / "addon_src/G17DragonBar/G17DragonBar.lua"

PRE_SHA = "7cb417b3cec7c6d93002c35c96a17748583d412308ac019bf2830fd496afa936"
POST_SHA = "1febdecb17d0dbcb17aa831c7a2a4e589f4f3fb8f6855ab41faf8a13ae7bdcc4"


def sha(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


class TestFrozenInputs(unittest.TestCase):
    def test_01_hashes(self):
        self.assertEqual(sha(ORIGINAL), PRE_SHA)
        self.assertEqual(sha(PAYLOAD), POST_SHA)
        self.assertEqual(sha(ROLLBACK), PRE_SHA)


class TestRealAutoCasts(unittest.TestCase):
    def setUp(self):
        self.text = PAYLOAD.read_text(encoding="utf-8")

    def test_01_triggered_generator_cast(self):
        # the mount fights with a REAL cast (client renders it)
        self.assertIn("me->CastSpell(victim, generator, true);", self.text)
        self.assertIn("uint32 const generator = G17Dragonriding::COMBAT_SPELL_BASE +", self.text)
        self.assertIn("constexpr uint32 AUTOCOMBAT_INTERVAL_MS = 4500;", self.text)
        # old script-only damage path removed
        self.assertNotIn("Unit::DealDamage(rider, victim", self.text)
        self.assertNotIn("AUTOCOMBAT_ENERGY_COST", self.text)

    def test_02_no_chat_spam_for_auto_casts(self):
        self.assertIn("session && !spell->IsTriggered()", self.text)

    def test_03_gates_kept(self):
        self.assertIn("rider->IsInCombat()", self.text)
        self.assertIn("rider->InBattleground() || rider->InArena()", self.text)
        self.assertIn("rider->IsWithinLOS(victim->GetPositionX()", self.text)

    def test_04_banner(self):
        self.assertIn("G17-B3R5 combat visuals LOADED", self.text)

    def test_05_file_integrity(self):
        self.assertEqual(self.text.count("{"), self.text.count("}"))
        self.assertEqual(self.text.count("("), self.text.count(")"))
        self.assertNotIn("getLevel()", self.text)


class TestVisualPatcher(unittest.TestCase):
    def _synthetic(self):
        fields, recsize = 234, 936
        strings = b"\x00"
        rows = bytearray()
        ids = [1, 52226] + list(range(990000, 990026))
        for sid in ids:
            vals = [0] * fields
            vals[0] = sid
            vals[4] = 0x100
            vals[71] = 3
            vals[46] = 1 if 990000 <= sid <= 990024 else 3
            rows += struct.pack("<" + "I" * fields, *vals)
        header = struct.pack("<5I", 0x43424457, len(ids), fields, recsize, len(strings))
        return header + bytes(rows) + strings

    def test_01_patch_and_idempotency(self):
        with tempfile.TemporaryDirectory() as tmp:
            inp = Path(tmp) / "in.dbc"
            out = Path(tmp) / "out.dbc"
            inp.write_bytes(self._synthetic())
            run = subprocess.run([sys.executable, str(VISUALS), "check", "--input", str(inp)],
                                 capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn("G17B3R5_VISUAL_STATE=FRESH", run.stdout)
            run = subprocess.run([sys.executable, str(VISUALS), "patch", "--input", str(inp), "--output", str(out)],
                                 capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn("G17B3R5_VISUAL_PATCH=PATCHED", run.stdout)
            self.assertIn("G17B3R5_VISUAL_PATCHED_RECORDS=25", run.stdout)
            self.assertEqual(inp.stat().st_size, out.stat().st_size)
            run = subprocess.run([sys.executable, str(VISUALS), "check", "--input", str(out)],
                                 capture_output=True, text=True)
            self.assertIn("G17B3R5_VISUAL_STATE=COMPLETE", run.stdout)
            # verify per-block visuals + ranges
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
                    self.assertEqual(vis, expected[block])
                    self.assertEqual(rng, 4)

    def test_02_range_only_image_also_converges(self):
        # the B3R4 range-patched image (VISUAL_MISSING) patches to the SAME output
        with tempfile.TemporaryDirectory() as tmp:
            a = Path(tmp) / "a.dbc"
            b = Path(tmp) / "b.dbc"
            a.write_bytes(self._synthetic())
            # simulate range patch already applied
            data = bytearray(a.read_bytes())
            count = struct.unpack_from("<I", data, 4)[0]
            for i in range(count):
                sid = struct.unpack_from("<I", data, 20 + i * 936)[0]
                if 990000 <= sid <= 990024:
                    struct.pack_into("<I", data, 20 + i * 936 + 46 * 4, 4)
            r_only = Path(tmp) / "r30.dbc"
            r_only.write_bytes(bytes(data))
            out1 = Path(tmp) / "o1.dbc"
            out2 = Path(tmp) / "o2.dbc"
            subprocess.run([sys.executable, str(VISUALS), "patch", "--input", str(a), "--output", str(out1)], capture_output=True)
            subprocess.run([sys.executable, str(VISUALS), "patch", "--input", str(r_only), "--output", str(out2)], capture_output=True)
            self.assertEqual(sha(out1), sha(out2))


class TestAddonV3(unittest.TestCase):
    def setUp(self):
        self.lua = ADDON_LUA.read_text(encoding="utf-8")

    def test_01_load_banner(self):
        # the investigation build must PROVE it loaded
        self.assertIn("G17|r DragonBar v3", self.lua)
        self.assertIn("PLAYER_LOGIN", self.lua)

    def test_02_diagnostics(self):
        self.assertIn("Diagnose", self.lua)
        self.assertIn('msg == "debug"', self.lua)
        self.assertIn("自动诊断", self.lua)
        self.assertIn("GetSpellBookItemName ~= nil", self.lua)

    def test_03_robust_detection(self):
        # any one signal counts: book scan, IsSpellKnown, IsUsableSpell
        self.assertIn("safecall(IsUsableSpell, name)", self.lua)

    def test_04_safety(self):
        self.assertNotIn('UnitIsUnit("vehicle", ...)', self.lua)
        self.assertIn("InCombatLockdown()", self.lua)


class TestToolLifecycle(unittest.TestCase):
    def test_01_lineage(self):
        spec = importlib.util.spec_from_file_location("t", TOOL)
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        self.assertEqual(mod.PRE_SHA256, PRE_SHA)
        self.assertEqual(mod.POST_SHA256, POST_SHA)
        self.assertEqual(mod.state_for_digest(POST_SHA), "B3R5_APPLIED")
        for legacy in ("98446106309b45371f138d9c7bc707ee608d9a3db347e13d61cfd68cc97810f9",
                       "1a96b72eb28ffa2c0ac0d3e0c07e26c30f25bcd8525babd15efad02a041825d6",
                       "ecd307b472cb2c49f68607a8b0afe5dcf5f87a7a8eb6f087a4717f4cd8fa1bbb",
                       "feb3dad467188052c7b189478cea7060b14f8e13eb5bd7082d9f81b4ca3ab9ce",
                       "a65b0ddcd06a66cfbdf04a91cd4114295615f9ee0c014f92bd742cb6c245b24d",
                       "175e5a122765691448738c7db7a25b32535f1fc29d7781e297e10614d4173975",
                       "29f3e55470f3ceaab79c8c5a6145ece76a8743c99999adec505a446239c32b3a",
                       "f49fd955ec27f2336bfcc6ed8e84f995abaf1d98a1136cf1eb0daefecf563a14"):
            self.assertEqual(mod.state_for_digest(legacy), "B3R5_INTERMEDIATE_UPGRADEABLE")

    def test_02_full_lifecycle(self):
        with tempfile.TemporaryDirectory() as tmp:
            target = Path(tmp) / "src/server/scripts/Commands/cs_dragonriding.cpp"
            target.parent.mkdir(parents=True)
            target.write_bytes(ORIGINAL.read_bytes())
            run = subprocess.run([sys.executable, str(TOOL), "apply", "--source-root", tmp],
                                 capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn("G17B3R5_SOURCE_APPLY=PASS", run.stdout)
            self.assertEqual(sha(target), POST_SHA)
            run = subprocess.run([sys.executable, str(TOOL), "rollback", "--source-root", tmp],
                                 capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn("G17B3R5_SOURCE_ROLLBACK=PASS_B3R4C_FLOOR", run.stdout)
            self.assertEqual(sha(target), PRE_SHA)


class TestInstallerGate(unittest.TestCase):
    def test_01_gate_simulates_all_states(self):
        install = (ROOT / "Install-Build-G17B3R5-Windows.ps1").read_text(encoding="utf-8")
        tool = (ROOT / "tools/apply_g17b3r5_source.py").read_text(encoding="utf-8")

        def tool_hash(name):
            m = re.search(r'^\s*' + re.escape(name) + r'\s*=\s*"([0-9a-f]+)"', tool, re.M)
            assert m, f"tool lacks {name}"
            return m.group(1)

        names = re.findall(r'Read-ToolHash\s+"([A-Z0-9_]+)"', install)
        self.assertIn("INTERMEDIATE8_SHA256", names)
        recognized = {tool_hash("PRE_SHA256"), tool_hash("POST_SHA256"), tool_hash("SAFE_ROLLBACK_SHA256")}
        for n in names:
            if n.startswith("INTERMEDIATE"):
                recognized.add(tool_hash(n))
        upgradeable = re.search(r'UPGRADEABLE_SHAS = \(([^)]*)\)', tool).group(1)
        for var in re.findall(r'(INTERMEDIATE[0-9]*_SHA256)', upgradeable):
            self.assertIn(tool_hash(var), recognized, f"{var} missing from installer gate")
        for state in ("7cb417b3cec7c6d93002c35c96a17748583d412308ac019bf2830fd496afa936",  # B3R4c (current)
                      "f49fd955ec27f2336bfcc6ed8e84f995abaf1d98a1136cf1eb0daefecf563a14"): # B3R4 r1
            self.assertIn(state, recognized)

    def test_02_installer_contract(self):
        install = (ROOT / "Install-Build-G17B3R5-Windows.ps1").read_text(encoding="utf-8")
        self.assertIn('$B3R5_BUILD = "r1_visual_casts"', install)
        self.assertIn("patch_g17b3r5_visuals.py", install)
        self.assertIn("G17B3R5_VISUAL_PATCH=PASS", install)
        self.assertIn("G17B3R5_WINDOWS_BUILD_RESULT=PASS", install)
        self.assertIn("G17B3R5_ADDON_INSTALL=PASS", install)

    def test_03_ps_files_parse(self):
        checker = ROOT / "tools/ps_static_check.py"
        run = subprocess.run(
            ["python3", str(checker),
             str(ROOT / "Install-Build-G17B3R5-Windows.ps1"),
             str(ROOT / "Rollback-Build-G17B3R5-Windows.ps1")],
            capture_output=True, text=True)
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == "__main__":
    unittest.main()
