#!/usr/bin/env python3
"""G17-B3R4 (combat experience rework) behavior tests.

Static C++ contract + addon v2 contract + tool + range-patcher lifecycle.
B3R3's dual-cast design is preserved; these pin the design:
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
TOOL = ROOT / "tools/apply_g17b3r4_source.py"
RANGER = ROOT / "tools/patch_g17b3r4_ranges.py"
ADDON_LUA = ROOT / "addon_src/G17DragonBar/G17DragonBar.lua"
ADDON_TOC = ROOT / "addon_src/G17DragonBar/G17DragonBar.toc"

PRE_SHA = "29f3e55470f3ceaab79c8c5a6145ece76a8743c99999adec505a446239c32b3a"
POST_SHA = "7cb417b3cec7c6d93002c35c96a17748583d412308ac019bf2830fd496afa936"


def sha(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


class TestFrozenInputs(unittest.TestCase):
    def test_01_original_is_b3r3_floor(self):
        self.assertEqual(sha(ORIGINAL), PRE_SHA)

    def test_02_payload_is_b3r4_postimage(self):
        self.assertEqual(sha(PAYLOAD), POST_SHA)

    def test_03_rollback_is_b3r3_floor(self):
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
        self.assertIn("G17-B3R4 combat experience LOADED", self.text)

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
        # v2: no IsSpellKnown dependency (3.3.5 lacks it - the v1 bug);
        # detection scans the spellbook by NAME instead
        self.assertIn("RefreshBookCache()", self.lua)
        self.assertIn("GetSpellBookItemName", self.lua)
        self.assertIn("GetSpellName", self.lua)
        self.assertIn("IsKnown(MARKER_ID)", self.lua)
        # combat skills auto-detected from the learned archetype set
        self.assertIn("COMBAT_BASE, COMBAT_COUNT = 990000, 25", self.lua)
        # secure spell buttons (hand-built, no ActionButtonTemplate inheritance)
        self.assertIn('safecall(button.SetAttribute, button, "type", "spell")', self.lua)
        self.assertIn('safecall(button.SetAttribute, button, "spell", name)', self.lua)
        self.assertIn('"SecureActionButtonTemplate")', self.lua)
        self.assertNotIn("ActionButtonTemplate, SecureActionButtonTemplate", self.lua)
        # debug command for on-client diagnosis
        self.assertIn('msg == "debug"', self.lua)

    def test_04_safety(self):
        # never touches default UI frames; guarded APIs; no file-scope varargs
        self.assertNotIn("VehicleMenuBar", self.lua)
        self.assertNotIn("MainMenuBar", self.lua)
        self.assertIn("InCombatLockdown()", self.lua)
        # the v1 bug was a file-scope vararg; safecall(fn, ...) inside a
        # function is valid Lua - forbid the specific v1 pattern instead
        self.assertNotIn('UnitIsUnit("vehicle", ...)', self.lua)

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
        self.assertEqual(mod.state_for_digest(POST_SHA), "B3R4_APPLIED")
        for legacy in ("98446106309b45371f138d9c7bc707ee608d9a3db347e13d61cfd68cc97810f9",
                       "1a96b72eb28ffa2c0ac0d3e0c07e26c30f25bcd8525babd15efad02a041825d6",
                       "ecd307b472cb2c49f68607a8b0afe5dcf5f87a7a8eb6f087a4717f4cd8fa1bbb",
                       "feb3dad467188052c7b189478cea7060b14f8e13eb5bd7082d9f81b4ca3ab9ce",
                       "a65b0ddcd06a66cfbdf04a91cd4114295615f9ee0c014f92bd742cb6c245b24d",
                       "175e5a122765691448738c7db7a25b32535f1fc29d7781e297e10614d4173975",
                       "f49fd955ec27f2336bfcc6ed8e84f995abaf1d98a1136cf1eb0daefecf563a14"):
            self.assertEqual(mod.state_for_digest(legacy), "B3R4_INTERMEDIATE_UPGRADEABLE")

    def test_02_full_lifecycle(self):
        with tempfile.TemporaryDirectory() as tmp:
            target = Path(tmp) / "src/server/scripts/Commands/cs_dragonriding.cpp"
            target.parent.mkdir(parents=True)
            target.write_bytes(ORIGINAL.read_bytes())
            run = subprocess.run([sys.executable, str(TOOL), "apply", "--source-root", tmp],
                                 capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn("G17B3R4_SOURCE_APPLY=PASS", run.stdout)
            self.assertEqual(sha(target), POST_SHA)
            run = subprocess.run([sys.executable, str(TOOL), "apply", "--source-root", tmp],
                                 capture_output=True, text=True)
            self.assertIn("G17B3R4_SOURCE_APPLY=ALREADY_CURRENT", run.stdout)
            run = subprocess.run([sys.executable, str(TOOL), "rollback", "--source-root", tmp],
                                 capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn("G17B3R4_SOURCE_ROLLBACK=PASS_B3R3_FLOOR", run.stdout)
            self.assertEqual(sha(target), PRE_SHA)


class TestInstallerStatic(unittest.TestCase):
    def test_01_installer_contract(self):
        install = (ROOT / "Install-Build-G17B3R4-Windows.ps1").read_text(encoding="utf-8")
        self.assertIn('$B3R4_BUILD = "r1c_lineage_gate"', install)
        self.assertIn('"B3R4_BUILD=" + $B3R4_BUILD', install)
        self.assertIn("apply_g17b3r4_source.py", install)
        self.assertIn("G17B3R4_ADDON_INSTALL=PASS", install)
        self.assertIn("G17B3R4_WINDOWS_BUILD_RESULT=PASS", install)
        self.assertIn("addon_src\\G17DragonBar", install)
        self.assertIn("patch_g17b3r4_ranges.py", install)
        self.assertIn("G17B3R4_RANGE_PATCH=PASS", install)
        # no SQL/DBC steps in this batch
        self.assertNotIn("Invoke-WorldSql", install)
        self.assertNotIn("append_g17b3r3_spells", install)
        self.assertIn("$recognized = @($Pre, $Post, $SafeRollback) + $Upgradeable", install)

    def test_02_ps_files_parse(self):
        checker = ROOT / "tools/ps_static_check.py"
        run = subprocess.run(
            ["python3", str(checker),
             str(ROOT / "Install-Build-G17B3R4-Windows.ps1"),
             str(ROOT / "Rollback-Build-G17B3R4-Windows.ps1")],
            capture_output=True, text=True)
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)



class TestCombatExperience(unittest.TestCase):
    """B3-R4: level-scaled damage, combat visuals, mount assisted strikes."""

    def setUp(self):
        self.text = PAYLOAD.read_text(encoding="utf-8")

    def test_01_level_scaled_damage(self):
        self.assertIn("constexpr uint32 COMBAT_DAMAGE_PER_LEVEL = 15;", self.text)
        self.assertIn("constexpr uint32 COMBAT_BURST_PER_LEVEL  = 45;", self.text)
        self.assertIn("uint32 const levelBonus = player->GetLevel() *", self.text)
        # regression (real user run): the fork's accessor is GetLevel(), not
        # getLevel() (Unit.h:890) - C2039 otherwise
        self.assertNotIn("getLevel()", self.text)
        self.assertIn("uint32 const amount = uint32(float(base + levelBonus) * mult * factor);", self.text)

    def test_02_combat_visuals(self):
        self.assertIn("dragon->SendMeleeAttackStart(target);", self.text)
        self.assertIn("dragon->SendMeleeAttackStop(target);", self.text)
        self.assertIn("target->SendPlaySpellVisualKit(VISUAL_KIT_IMPACT_RING, 0);", self.text)
        self.assertIn("player->SendPlaySpellVisualKit(VISUAL_KIT_TRAIL_PULSE, 0);", self.text)

    def test_03_mount_auto_combat(self):
        self.assertIn("void UpdateAutoCombat(uint32 diff)", self.text)
        self.assertIn("UpdateAutoCombat(diff);", self.text)
        self.assertIn("constexpr uint32 AUTOCOMBAT_INTERVAL_MS = 3500;", self.text)
        self.assertIn("constexpr uint32 AUTOCOMBAT_MIN_ENERGY  = 20;", self.text)
        # fires only while the rider is in combat and not in BG/arena
        self.assertIn("rider->IsInCombat()", self.text)
        self.assertIn("rider->InBattleground() || rider->InArena()", self.text)
        # rider-attributed, LOS + range gated
        self.assertIn("Unit::DealDamage(rider, victim, amount", self.text)
        self.assertIn("rider->IsWithinLOS(victim->GetPositionX()", self.text)
        # never grounds the rider
        self.assertIn("AUTOCOMBAT_MIN_ENERGY", self.text)
        # timer member + reset
        self.assertIn("uint32 _autoCombatTimer = 0;", self.text)


class TestRangePatcher(unittest.TestCase):
    def _synthetic(self):
        import struct
        fields, recsize = 234, 936
        strings = b"\x00"
        rows = bytearray()
        ids = [1, 52226] + list(range(990000, 990026))  # all 25 combat + 990025
        for sid in ids:
            vals = [0] * fields
            vals[0] = sid
            vals[4] = 0x100
            vals[71] = 3
            vals[46] = 1 if 990000 <= sid <= 990024 else 3
            rows += struct.pack("<" + "I" * fields, *vals)
        header = struct.pack("<5I", 0x43424457, len(ids), fields, recsize, len(strings))
        return header + bytes(rows) + strings

    def test_01_check_and_patch(self):
        import tempfile
        with tempfile.TemporaryDirectory() as tmp:
            inp = Path(tmp) / "in.dbc"
            out = Path(tmp) / "out.dbc"
            inp.write_bytes(self._synthetic())
            run = subprocess.run([sys.executable, str(RANGER), "check", "--input", str(inp)],
                                 capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn("G17B3R4_RANGE_STATE=MISSING", run.stdout)
            self.assertIn("G17B3R4_RANGE_SELF=25", run.stdout)
            run = subprocess.run([sys.executable, str(RANGER), "patch", "--input", str(inp), "--output", str(out)],
                                 capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn("G17B3R4_RANGE_PATCH=PATCHED", run.stdout)
            self.assertIn("G17B3R4_RANGE_PATCHED_RECORDS=25", run.stdout)
            # same size (in-place patch)
            self.assertEqual(inp.stat().st_size, out.stat().st_size)
            run = subprocess.run([sys.executable, str(RANGER), "check", "--input", str(out)],
                                 capture_output=True, text=True)
            self.assertIn("G17B3R4_RANGE_STATE=PATCHED", run.stdout)
            # non-combat records untouched (verify by actual id)
            import struct
            data = out.read_bytes()
            count = struct.unpack_from("<I", data, 4)[0]
            recs = data[20:20 + count * 936]
            for i in range(count):
                sid = struct.unpack_from("<I", recs, i * 936)[0]
                val = struct.unpack_from("<I", recs, i * 936 + 46 * 4)[0]
                if not (990000 <= sid <= 990024):
                    self.assertNotEqual(val, 4, f"id {sid} unexpectedly patched")
                else:
                    self.assertEqual(val, 4, f"combat id {sid} not patched")



class TestInstallerLineageGate(unittest.TestCase):
    """Regression (real user run, 2026-08-25): the r1b installer silently
    failed to add INTERMEDIATE6/7 to $Upgradeable (string-replace pattern
    mismatch), so the PS-side gate rejected the user's live source state
    f49fd955 even though the apply TOOL whitelisted it.  This test simulates
    the INSTALLER's exact gate: parse Read-ToolHash calls from the PS1, build
    the recognized set exactly like the PS1 does, and require every tool
    upgradeable digest (and the user's live state) to be recognized."""

    def test_01_installer_gate_simulates_all_states(self):
        import re
        install = (ROOT / "Install-Build-G17B3R4-Windows.ps1").read_text(encoding="utf-8")
        tool = (ROOT / "tools/apply_g17b3r4_source.py").read_text(encoding="utf-8")

        def tool_hash(name):
            m = re.search(r'^\s*' + re.escape(name) + r'\s*=\s*"([0-9a-f]+)"', tool, re.M)
            assert m, f"tool lacks {name}"
            return m.group(1)

        names = re.findall(r'Read-ToolHash\s+"([A-Z0-9_]+)"', install)
        self.assertIn("INTERMEDIATE7_SHA256", names,
                      "installer must read INTERMEDIATE7 (the r1b bug)")
        self.assertIn("INTERMEDIATE6_SHA256", names,
                      "installer must read INTERMEDIATE6 (the r1b bug)")

        pre = tool_hash("PRE_SHA256")
        post = tool_hash("POST_SHA256")
        safe = tool_hash("SAFE_ROLLBACK_SHA256")
        recognized = {pre, post, safe}
        for i in range(1, 8):
            key = "INTERMEDIATE_SHA256" if i == 1 else f"INTERMEDIATE{i}_SHA256"
            if key in names:
                recognized.add(tool_hash(key))

        # every digest the tool treats as upgradeable must be recognized by
        # the INSTALLER gate too (the two layers can never diverge again)
        upgradeable = re.search(r'UPGRADEABLE_SHAS = \(([^)]*)\)', tool).group(1)
        for var in re.findall(r'(INTERMEDIATE[0-9]*_SHA256)', upgradeable):
            digest = tool_hash(var)
            self.assertIn(digest, recognized, f"{var} ({digest}) missing from installer gate")

        # the user's live states
        for state in ("29f3e55470f3ceaab79c8c5a6145ece76a8743c99999adec505a446239c32b3a",  # B3R3 (preimage)
                      "f49fd955ec27f2336bfcc6ed8e84f995abaf1d98a1136cf1eb0daefecf563a14",  # B3R4 r1 (current)
                      "7cb417b3cec7c6d93002c35c96a17748583d412308ac019bf2830fd496afa936"): # B3R4b r1b
            self.assertIn(state, recognized, f"user state {state} rejected by installer gate")


if __name__ == "__main__":
    unittest.main()
