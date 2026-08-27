#!/usr/bin/env python3
"""G17-B3R3 (DF-style rider skill panel) behavior tests.

Static C++ contract + addon contract + tool lifecycle.  These pin the design:
  - every dragonriding skill accepts BOTH casters (vehicle-bar button, cast by
    the dragon, and the rider's own skill-panel cast, allowed server-side by
    Attributes 0x100 - see SpellInfo::CheckVehicle in the fork);
  - boarding grants 9 skill-panel spells (4 movement + 5 archetype combat),
    unlearned on exit/login;
  - cooldowns live on BOTH the vehicle and the rider (shared gates); energy
    stays on the vehicle;
  - the vehicle bar keeps the B3R2d six-button layout as a no-addon fallback;
  - the G17DragonBar addon renders an 11-button secure bar while riding and
    touches no client files.
"""
from __future__ import annotations

import hashlib
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ORIGINAL = ROOT / "original_src/src/server/scripts/Commands/cs_dragonriding.cpp"
PAYLOAD = ROOT / "payload_src/src/server/scripts/Commands/cs_dragonriding.cpp"
ROLLBACK = ROOT / "rollback_safe_src/src/server/scripts/Commands/cs_dragonriding.cpp"
TOOL = ROOT / "tools/apply_g17b3r3_source.py"
ADDON_LUA = ROOT / "addon_src/G17DragonBar/G17DragonBar.lua"
ADDON_TOC = ROOT / "addon_src/G17DragonBar/G17DragonBar.toc"

PRE_SHA = "175e5a122765691448738c7db7a25b32535f1fc29d7781e297e10614d4173975"
POST_SHA = "29f3e55470f3ceaab79c8c5a6145ece76a8743c99999adec505a446239c32b3a"


def sha(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


class TestFrozenInputs(unittest.TestCase):
    def test_01_original_is_b3r2d_floor(self):
        self.assertEqual(sha(ORIGINAL), PRE_SHA)

    def test_02_payload_is_b3r3_postimage(self):
        self.assertEqual(sha(PAYLOAD), POST_SHA)

    def test_03_rollback_is_b3r2d_floor(self):
        self.assertEqual(sha(ROLLBACK), PRE_SHA)


class TestDualCast(unittest.TestCase):
    def setUp(self):
        self.text = PAYLOAD.read_text(encoding="utf-8")

    def test_01_resolver_defined_and_used(self):
        self.assertIn("Creature* ResolveDragonFromCaster(Unit* caster)", self.text)
        self.assertGreaterEqual(self.text.count("ResolveDragonFromCaster(GetCaster())"), 9)

    def test_02_resolver_handles_both_casters(self):
        self.assertIn("if (Creature* dragon = caster->ToCreature())", self.text)
        self.assertIn("if (Player* player = caster->ToPlayer())", self.text)
        self.assertIn("return GetDragon(player);", self.text)

    def test_03_evidence_comment(self):
        # the fork's SpellInfo::CheckVehicle allows 0x100 carrier casts
        self.assertIn("SpellInfo::CheckVehicle", self.text)
        self.assertIn("SPELL_ATTR0_CASTABLE_WHILE_MOUNTED", self.text)

    def test_04_dual_cooldown_gates(self):
        self.assertIn("!dragon->GetSpellHistory()->IsReady(GetSpellInfo()) ||", self.text)
        self.assertIn("!player->GetSpellHistory()->IsReady(GetSpellInfo())", self.text)
        # cooldown is ADDED to both histories
        self.assertEqual(self.text.count(
            "GetSpellHistory()->AddCooldown(info->Id, 0, Milliseconds(COMBAT_CD_MS[slot]));"), 2)

    def test_05_energy_stays_on_vehicle(self):
        self.assertIn("dragon->ModifyPower(POWER_ENERGY, GENERATOR_ENERGY_GAIN);", self.text)
        self.assertIn("dragon->ModifyPower(POWER_ENERGY, -int32(cost));", self.text)


class TestSkillPanelGrant(unittest.TestCase):
    def setUp(self):
        self.text = PAYLOAD.read_text(encoding="utf-8")

    def test_01_grant_function(self):
        self.assertIn("void GrantSkillPanel(Player* player, uint32 mountArchetype)", self.text)
        self.assertIn("player->LearnSpell(sid, false);", self.text)
        for sid in ("SPELL_PAGE_SWITCH", "SPELL_ASCEND", "SPELL_DIVE", "SPELL_GLIDE_BRAKE"):
            self.assertIn(sid, self.text)

    def test_02_granted_on_board(self):
        self.assertIn(
            "G17Dragonriding::GrantSkillPanel(player, GetData(G17Dragonriding::DATA_ARCHETYPE));",
            self.text)

    def test_03_revoke_covers_everything(self):
        # all 29 carriers (25 combat + 4 bar spells) are unlearned on exit/login
        self.assertIn("for (uint32 i = 0; i < COMBAT_SPELL_COUNT; ++i)", self.text)
        self.assertIn(
            "for (uint32 sid : { SPELL_PAGE_SWITCH, SPELL_ASCEND, SPELL_DIVE, SPELL_GLIDE_BRAKE })",
            self.text)
        self.assertIn("void OnLogin(Player* player, bool /*firstLogin*/) override", self.text)

    def test_04_vehicle_bar_fallback_kept(self):
        # the six-button B3R2d layout still works without the addon
        self.assertIn("dragon->m_spells[5] = SPELL_PAGE_SWITCH;", self.text)
        self.assertIn("dragon->m_spells[4] = SPELL_SAFE_LANDING;", self.text)
        self.assertIn("rider->VehicleSpellInitialize();", self.text)

    def test_05_banner(self):
        self.assertIn("G17-B3R3 skill panel LOADED", self.text)

    def test_06_file_integrity(self):
        self.assertEqual(self.text.count("{"), self.text.count("}"))
        self.assertEqual(self.text.count("("), self.text.count(")"))
        lines = self.text.splitlines()
        resolver = next(i for i, l in enumerate(lines) if "Creature* ResolveDragonFromCaster(Unit* caster)" in l)
        use = next(i for i, l in enumerate(lines) if "ResolveDragonFromCaster(GetCaster())" in l)
        self.assertLess(resolver, use)


class TestAddon(unittest.TestCase):
    def setUp(self):
        self.lua = ADDON_LUA.read_text(encoding="utf-8")
        self.toc = ADDON_TOC.read_text(encoding="utf-8")

    def test_01_toc_contract(self):
        self.assertIn("## Interface: 30300", self.toc)
        self.assertIn("G17DragonBar.lua", self.toc)
        self.assertIn("SavedVariablesPerCharacter", self.toc)

    def test_02_movement_ids(self):
        for sid in (990026, 990027, 55215, 52197, 990028, 52226):
            self.assertIn(str(sid), self.lua)

    def test_03_gating_and_population(self):
        # shows only while the G17 skill panel is granted
        self.assertIn("IsKnown(990026)", self.lua)
        # combat skills auto-detected from the learned archetype set
        self.assertIn("COMBAT_BASE, COMBAT_COUNT = 990000, 25", self.lua)
        # secure spell buttons
        self.assertIn('SetAttribute("type", "spell")', self.lua)
        self.assertIn('SetAttribute("spell", name)', self.lua)
        self.assertIn("ActionButtonTemplate, SecureActionButtonTemplate", self.lua)

    def test_04_safety(self):
        # never touches default UI frames; guarded APIs; no file-scope varargs
        self.assertNotIn("VehicleMenuBar", self.lua)
        self.assertNotIn("MainMenuBar", self.lua)
        self.assertIn("InCombatLockdown()", self.lua)
        self.assertNotIn(", ...)", self.lua)

    def test_05_balance(self):
        # crude Lua sanity: balanced parens/braces/brackets outside strings
        stripped = []
        in_str = None
        i = 0
        text = self.lua
        while i < len(text):
            c = text[i]
            if in_str:
                if c == "\\":
                    i += 2
                    continue
                if c == in_str:
                    in_str = None
                if c == "\n":  # unterminated one-line string
                    in_str = None
                i += 1
                continue
            if c in ("\"", "'"):
                in_str = c
                i += 1
                continue
            if c == "-" and i + 1 < len(text) and text[i + 1] == "-":
                # line comment
                nl = text.find("\n", i)
                i = nl if nl != -1 else len(text)
                continue
            if c == "[" and text[i:i + 4] == "[[":
                end = text.find("]]", i)
                i = end + 2 if end != -1 else len(text)
                continue
            stripped.append(c)
            i += 1
        s = "".join(stripped)
        self.assertEqual(s.count("("), s.count(")"))
        self.assertEqual(s.count("{"), s.count("}"))
        self.assertEqual(s.count("["), s.count("]"))

    def test_06_slash_command(self):
        self.assertIn('SLASH_G17DRAGONBAR1 = "/g17bar"', self.lua)
        self.assertIn("reset", self.lua)


class TestToolLifecycle(unittest.TestCase):
    def test_01_lineage(self):
        import importlib.util
        spec = importlib.util.spec_from_file_location("t", TOOL)
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        self.assertEqual(mod.PRE_SHA256, PRE_SHA)
        self.assertEqual(mod.POST_SHA256, POST_SHA)
        self.assertEqual(mod.state_for_digest(POST_SHA), "B3R3_APPLIED")
        for legacy in ("98446106309b45371f138d9c7bc707ee608d9a3db347e13d61cfd68cc97810f9",
                       "1a96b72eb28ffa2c0ac0d3e0c07e26c30f25bcd8525babd15efad02a041825d6",
                       "ecd307b472cb2c49f68607a8b0afe5dcf5f87a7a8eb6f087a4717f4cd8fa1bbb",
                       "feb3dad467188052c7b189478cea7060b14f8e13eb5bd7082d9f81b4ca3ab9ce",
                       "a65b0ddcd06a66cfbdf04a91cd4114295615f9ee0c014f92bd742cb6c245b24d"):
            self.assertEqual(mod.state_for_digest(legacy), "B3R3_INTERMEDIATE_UPGRADEABLE")

    def test_02_full_lifecycle(self):
        with tempfile.TemporaryDirectory() as tmp:
            target = Path(tmp) / "src/server/scripts/Commands/cs_dragonriding.cpp"
            target.parent.mkdir(parents=True)
            target.write_bytes(ORIGINAL.read_bytes())
            run = subprocess.run([sys.executable, str(TOOL), "apply", "--source-root", tmp],
                                 capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn("G17B3R3_SOURCE_APPLY=PASS", run.stdout)
            self.assertEqual(sha(target), POST_SHA)
            run = subprocess.run([sys.executable, str(TOOL), "apply", "--source-root", tmp],
                                 capture_output=True, text=True)
            self.assertIn("G17B3R3_SOURCE_APPLY=ALREADY_CURRENT", run.stdout)
            run = subprocess.run([sys.executable, str(TOOL), "rollback", "--source-root", tmp],
                                 capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn("G17B3R3_SOURCE_ROLLBACK=PASS_B3R2D_FLOOR", run.stdout)
            self.assertEqual(sha(target), PRE_SHA)


class TestInstallerStatic(unittest.TestCase):
    def test_01_installer_contract(self):
        install = (ROOT / "Install-Build-G17B3R3-Windows.ps1").read_text(encoding="utf-8")
        self.assertIn('$B3R3_BUILD = "r1_skillpanel"', install)
        self.assertIn('"B3R3_BUILD=" + $B3R3_BUILD', install)
        self.assertIn("apply_g17b3r3_source.py", install)
        self.assertIn("G17B3R3_ADDON_INSTALL=PASS", install)
        self.assertIn("G17B3R3_WINDOWS_BUILD_RESULT=PASS", install)
        self.assertIn("addon_src\\G17DragonBar", install)
        # no SQL/DBC steps in this batch
        self.assertNotIn("Invoke-WorldSql", install)
        self.assertNotIn("append_g17b3r3_spells", install)
        self.assertIn("$recognized = @($Pre, $Post, $SafeRollback) + $Upgradeable", install)

    def test_02_ps_files_parse(self):
        checker = ROOT / "tools/ps_static_check.py"
        run = subprocess.run(
            ["python3", str(checker),
             str(ROOT / "Install-Build-G17B3R3-Windows.ps1"),
             str(ROOT / "Rollback-Build-G17B3R3-Windows.ps1")],
            capture_output=True, text=True)
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == "__main__":
    unittest.main()
