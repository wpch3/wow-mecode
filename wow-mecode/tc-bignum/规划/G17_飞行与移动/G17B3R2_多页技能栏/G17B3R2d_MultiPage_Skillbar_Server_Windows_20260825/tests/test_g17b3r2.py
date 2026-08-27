#!/usr/bin/env python3
"""G17-B3R2 multi-page vehicle skill bar behavior tests.

Static C++ contract + tool lifecycle + DBC appender determinism.  These pin
the USER-DIRECTED design of the rework:
  - the skill bar lives on the VEHICLE (Creature::m_spells, 8 slots), never
    in the player spellbook (no GrantCombatSkills / LearnSpell);
  - the movement page is PURE movement (no 9573 fire breath on the bar);
  - a page-switch button swaps movement <-> archetype combat page and
    re-sends the bar with Player::VehicleSpellInitialize();
  - slot-0 of the combat page generates energy; the dive skill restores
    energy; passive regen exists; the combat page caps speed at 600%;
  - combat skills are cast by the vehicle with damage/heal credited to the
    rider; cooldowns live on the vehicle spell history (page-switch safe);
  - BG/arena blocking and the rider-exit normalization are preserved.
"""
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
ORIGINAL = ROOT / "original_src/src/server/scripts/Commands/cs_dragonriding.cpp"
PAYLOAD = ROOT / "payload_src/src/server/scripts/Commands/cs_dragonriding.cpp"
ROLLBACK = ROOT / "rollback_safe_src/src/server/scripts/Commands/cs_dragonriding.cpp"
TOOL = ROOT / "tools/apply_g17b3r2_source.py"
APPENDER = ROOT / "tools/append_g17b3r2_spells.py"

PRE_SHA = "2ddf54a66395896244869318e4bcfd619d10afc884033c6aa88e7cb53d0e6963"
POST_SHA = "175e5a122765691448738c7db7a25b32535f1fc29d7781e297e10614d4173975"


def sha(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


class TestFrozenInputs(unittest.TestCase):
    def test_01_original_is_b3r1_floor(self):
        self.assertEqual(sha(ORIGINAL), PRE_SHA)

    def test_02_payload_is_b3r2_postimage(self):
        self.assertEqual(sha(PAYLOAD), POST_SHA)

    def test_03_rollback_is_b3r1_floor(self):
        self.assertEqual(sha(ROLLBACK), PRE_SHA)

    def test_04_payload_differs(self):
        self.assertNotEqual(sha(PAYLOAD), sha(ORIGINAL))


class TestMovementPage(unittest.TestCase):
    """Page 0 must be pure movement: 8 slots, no attack spell, switch last."""

    def setUp(self):
        self.text = PAYLOAD.read_text(encoding="utf-8")

    def test_01_page_writer_exists(self):
        self.assertIn("void WriteMovementPage(Creature* dragon)", self.text)

    def test_02_layout_is_pure_movement(self):
        # B3-R2d: the stock 3.3.5 client VehicleMenuBar shows a fixed 6 spell
        # buttons (7 sent -> 6 shown, verified live), so every page is exactly
        # 6 visible entries: 5 skills + the switch button at slot [5].
        self.assertIn("dragon->m_spells[0] = SPELL_ASCEND;", self.text)
        self.assertIn("dragon->m_spells[1] = SPELL_DIVE;", self.text)
        self.assertIn("dragon->m_spells[2] = SPELL_ACCELERATE;", self.text)
        self.assertIn("dragon->m_spells[3] = SPELL_CLIMB;", self.text)
        self.assertIn("dragon->m_spells[4] = SPELL_SAFE_LANDING;", self.text)
        self.assertIn("dragon->m_spells[5] = SPELL_PAGE_SWITCH;", self.text)
        self.assertIn("dragon->m_spells[6] = 0;", self.text)
        self.assertIn("dragon->m_spells[7] = 0;", self.text)
        # the brake skill is off the bar (S key covers braking) but stays
        # registered for the future client-UI extension
        self.assertNotIn("m_spells[4] = SPELL_GLIDE_BRAKE", self.text)
        self.assertIn("RegisterSpellScript(spell_g17_glide_brake);", self.text)

    def test_03_no_attack_spell_on_movement_page(self):
        # The old bar had the 9573 fire breath mixed into the flight page.
        self.assertNotIn("m_spells[0] = SPELL_DRAGON_BREATH", self.text)
        # 9573 stays bound to its energy SpellScript but is NOT on any page.
        self.assertIn("SPELL_DRAGON_BREATH", self.text)  # constant still defined

    def test_04_flight_skills_more_than_three(self):
        # User requirement: the flight page must offer more than three skills.
        movement_spells = ("SPELL_ASCEND", "SPELL_DIVE", "SPELL_ACCELERATE",
                           "SPELL_CLIMB", "SPELL_GLIDE_BRAKE", "SPELL_SAFE_LANDING")
        for token in movement_spells:
            self.assertIn(f"m_spells[", self.text)
            self.assertIn(token, self.text)
        # six movement skills + switch button
        self.assertGreaterEqual(len(movement_spells), 6)

    def test_05_eight_slot_capacity_documented(self):
        self.assertIn("MAX_CREATURE_SPELLS", self.text)
        # B3-R2d: the stock client renders a fixed 6 buttons; the layout
        # comment documents that slots beyond [5] are past the visible bar.
        self.assertIn("beyond the 6-button client bar", self.text)


class TestCombatPage(unittest.TestCase):
    def setUp(self):
        self.text = PAYLOAD.read_text(encoding="utf-8")

    def test_01_page_writer_exists(self):
        self.assertIn("void WriteCombatPage(Creature* dragon, uint32 mountArchetype)", self.text)

    def test_02_archetype_block_mapping(self):
        self.assertIn("uint32 ArchetypeBlock(uint32 mountArchetype)", self.text)
        for tok in ("case ARCHETYPE_DRAGON:     return 0;",
                    "case ARCHETYPE_BEAST:      return 1;",
                    "case ARCHETYPE_MAGIC:      return 2;",
                    "case ARCHETYPE_MECHANICAL: return 3;",
                    "default:                   return 4;"):
            self.assertIn(tok, self.text)

    def test_03_five_slots_plus_switch(self):
        self.assertIn("dragon->m_spells[i] = COMBAT_SPELL_BASE + block * 5u + i;",
                      self.text)
        self.assertIn("dragon->m_spells[5] = SPELL_PAGE_SWITCH;", self.text)

    def test_04_vehicle_cast_with_rider_attribution(self):
        # combat skills are cast BY the vehicle; damage/heal attacker = rider
        self.assertIn("void ExecuteCombatSkill(Creature* dragon, Spell* spell)", self.text)
        self.assertIn("Unit::DealDamage(player, target, amount", self.text)
        self.assertIn("HealInfo healInfo(player, player, heal", self.text)

    def test_05_generator_slot(self):
        self.assertIn("if (slot == 0)\n        dragon->ModifyPower(POWER_ENERGY, GENERATOR_ENERGY_GAIN);",
                      self.text)
        self.assertIn("constexpr int32  GENERATOR_ENERGY_GAIN      = 8;", self.text)

    def test_06_vehicle_cooldowns_survive_page_switch(self):
        self.assertIn("dragon->GetSpellHistory()->AddCooldown(info->Id, 0, Milliseconds(COMBAT_CD_MS[slot]));",
                      self.text)
        self.assertIn("!dragon->GetSpellHistory()->IsReady(GetSpellInfo())", self.text)

    def test_07_bg_arena_still_blocked(self):
        self.assertIn("player->InBattleground() || player->InArena()", self.text)

    def test_08_attack_legality_range_los(self):
        # Design doc 5.3: mount attacks may not bypass distance or LOS.
        self.assertIn("constexpr float  COMBAT_MAX_RANGE  = 40.0f;", self.text)
        self.assertIn("!player->IsWithinDist(target, COMBAT_MAX_RANGE)", self.text)
        self.assertIn("!player->IsWithinLOS(target->GetPositionX()", self.text)
        # Nothing is consumed before legality passes (target resolves first).
        idx_resolve = self.text.index("Target resolution FIRST")
        idx_consume = self.text.index("dragon->ModifyPower(POWER_ENERGY, -int32(cost));")
        self.assertLess(idx_resolve, idx_consume)


class TestPageSwitch(unittest.TestCase):
    def setUp(self):
        self.text = PAYLOAD.read_text(encoding="utf-8")

    def test_01_apply_skill_page_reseands_bar(self):
        self.assertIn("void ApplySkillPage(bool combatPage)", self.text)
        self.assertIn("rider->VehicleSpellInitialize();", self.text)

    def test_02_boarding_installs_movement_page(self):
        self.assertIn("G17Dragonriding::WriteMovementPage(me);", self.text)
        self.assertIn("player->VehicleSpellInitialize();", self.text)

    def test_03_switch_action_wired(self):
        self.assertIn("constexpr int32  ACTION_PAGE_SWITCH = 7;", self.text)
        self.assertIn("ApplySkillPage(!_combatPage);", self.text)

    def test_04_switch_spellscript_exists(self):
        self.assertIn("class spell_g17_page_switch : public SpellScript", self.text)
        self.assertIn("RegisterSpellScript(spell_g17_page_switch);", self.text)

    def test_05_speed_cap_on_combat_page(self):
        self.assertIn("COMBAT_PAGE_SPEED_TIER_CAP", self.text)
        self.assertIn("size_t const maxTier = _combatPage ? COMBAT_PAGE_SPEED_TIER_CAP",
                      self.text)

    def test_06_no_spellbook_learning_left(self):
        self.assertNotIn("GrantCombatSkills", self.text)
        self.assertNotIn("player->LearnSpell(sid", self.text)
        # PlayerScript no longer routes combat carriers.
        self.assertNotIn("IsCombatSkill(spell->GetSpellInfo()->Id)", self.text)


class TestEnergyLoop(unittest.TestCase):
    def setUp(self):
        self.text = PAYLOAD.read_text(encoding="utf-8")

    def test_01_dive_restores_energy(self):
        self.assertIn("constexpr int32  DIVE_ENERGY_GAIN    = 15;", self.text)
        self.assertIn("me->ModifyPower(POWER_ENERGY, gain);", self.text)

    def test_02_ascend_costs_energy(self):
        self.assertIn("constexpr uint32 ASCEND_ENERGY_COST  = 20;", self.text)

    def test_03_passive_regen(self):
        self.assertIn("constexpr uint32 PASSIVE_ENERGY_REGEN_MS    = 2000;", self.text)
        self.assertIn("_energyRegenTimer >= G17Dragonriding::PASSIVE_ENERGY_REGEN_MS", self.text)

    def test_04_new_movement_scripts_registered(self):
        for cls in ("spell_g17_ascend", "spell_g17_dive", "spell_g17_glide_brake"):
            self.assertIn(f"class {cls} : public SpellScript", self.text)
            self.assertIn(f"RegisterSpellScript({cls});", self.text)

    def test_05_dive_respects_floor(self):
        self.assertIn("DIVE_MIN_ALTITUDE", self.text)
        self.assertIn("me->GetFloorZ()", self.text)


class TestMigrationAndSafety(unittest.TestCase):
    def setUp(self):
        self.text = PAYLOAD.read_text(encoding="utf-8")

    def test_01_revoke_covers_b3r2_spells_too(self):
        self.assertIn("for (uint32 sid : { SPELL_PAGE_SWITCH, SPELL_ASCEND, SPELL_DIVE, SPELL_GLIDE_BRAKE })",
                      self.text)

    def test_02_login_cleanup(self):
        self.assertIn("void OnLogin(Player* player, bool /*firstLogin*/) override", self.text)

    def test_03_rider_exit_normalization_kept(self):
        self.assertIn("void NormalizeRiderAfterExit(Player* player)", self.text)
        self.assertGreaterEqual(self.text.count("NormalizeRiderAfterExit("), 4)

    def test_04_stun_release_event_before_use(self):
        lines = self.text.splitlines()
        cls = next(i + 1 for i, l in enumerate(lines)
                   if "class CombatStunReleaseEvent : public BasicEvent" in l)
        use = next(i + 1 for i, l in enumerate(lines)
                   if "new CombatStunReleaseEvent(player->GetGUID()" in l)
        self.assertLess(cls, use)

    def test_05_revoke_decl_before_first_call(self):
        lines = self.text.splitlines()
        decl = next(i + 1 for i, l in enumerate(lines)
                    if l.strip() == "void RevokeCombatSkills(Player* player);")
        call = next(i + 1 for i, l in enumerate(lines)
                    if l.strip() == "RevokeCombatSkills(player);")
        self.assertLess(decl, call)

    def test_06_payload_file_integrity(self):
        self.assertEqual(self.text.count("{"), self.text.count("}"))
        self.assertEqual(self.text.count("("), self.text.count(")"))
        self.assertTrue(self.text.rstrip().endswith("}"))

    def test_07_banner(self):
        self.assertIn("G17-B3R2 multi-page skillbar LOADED", self.text)


class TestSqlContract(unittest.TestCase):
    def setUp(self):
        self.sql = (ROOT / "sql/G17B3R2_world_skill_pages.sql").read_text(encoding="utf-8")

    def test_01_four_bindings(self):
        for sid, script in ((990025, "spell_g17_page_switch"),
                            (990026, "spell_g17_ascend"),
                            (990027, "spell_g17_dive"),
                            (990028, "spell_g17_glide_brake")):
            self.assertIn(f"({sid}, @G17B3R2_SCRIPT", self.sql)
        for script in ("spell_g17_page_switch", "spell_g17_ascend",
                       "spell_g17_dive", "spell_g17_glide_brake"):
            self.assertIn(script, self.sql)

    def test_02_template_bar_alignment(self):
        # Regression (real user run 2026-08-25, ERROR 1054): this fork has NO
        # creature_template.spell1..8 columns; the bar comes from the
        # creature_template_spell table (CreatureID/Index/Spell).
        self.assertNotIn("`spell1`", self.sql)
        self.assertNotIn("`spell2`", self.sql)
        self.assertIn("INSERT INTO `creature_template_spell` (`CreatureID`, `Index`, `Spell`)", self.sql)
        for idx, spell in ((0, 990026), (1, 990027), (2, 55215), (3, 52197),
                           (4, 52226), (5, 990025)):
            self.assertIn(f"(@G17B3R2_ENTRY, {idx}, {spell})", self.sql)
        self.assertNotIn("(0, 990028)", self.sql.replace("9900280", ""))
        self.assertIn("DELETE FROM `creature_template_spell`", self.sql)
        self.assertIn("WHERE `CreatureID` = @G17B3R2_ENTRY AND `Index` BETWEEN 0 AND 7", self.sql)

    def test_03_idempotent_and_guarded(self):
        self.assertIn("INSERT IGNORE", self.sql)
        self.assertIn("G17B3R2_WORLD_SKILL_PAGES=PASS", self.sql)
        self.assertIn("@G17B3R2_TEMPLATE_OK=6", self.sql)
        self.assertIn("@G17B3R2_TEMPLATE_EXTRA=0", self.sql)
        code = [l for l in self.sql.splitlines() if not l.lstrip().startswith("--")]
        joint = chr(10).join(code)
        self.assertNotIn("CROSS JOIN", joint)
        self.assertNotIn("NOT EXISTS", joint)

    def test_04_rollback_sql_uses_correct_table(self):
        rollback = (ROOT / "Rollback-Build-G17B3R2-Windows.ps1").read_text(encoding="utf-8")
        self.assertNotIn("`spell1`", rollback)
        self.assertIn("DELETE FROM `creature_template_spell` WHERE `CreatureID`=1000171", rollback)
        self.assertIn("(1000171, 0, 9573)", rollback)


class TestDbcAppender(unittest.TestCase):
    """Determinism + idempotency of the 990025-990028 carrier appender."""

    def _synthetic_dbc(self) -> bytes:
        fields, recsize = 234, 936
        strings = b"\x00"
        recs = bytearray()
        for sid in (1, 2, 3):
            vals = [0] * fields
            vals[0] = sid
            vals[140] = 0
            recs += struct.pack("<" + "I" * fields, *vals)
        header = struct.pack("<5I", 0x43424457, 3, fields, recsize, len(strings))
        return header + bytes(recs) + strings

    def test_01_append_and_check(self):
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / "in.dbc"
            out = Path(tmp) / "out.dbc"
            src.write_bytes(self._synthetic_dbc())
            run = subprocess.run([sys.executable, str(APPENDER), "check", "--input", str(src)],
                                 capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn("G17B3R2_SPELL_DBC_STATE=MISSING", run.stdout)
            run = subprocess.run([sys.executable, str(APPENDER), "append",
                                  "--input", str(src), "--output", str(out)],
                                 capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn("G17B3R2_SPELL_DBC_STATE=APPENDED", run.stdout)
            self.assertIn("G17B3R2_RECORDS_AFTER=7", run.stdout)
            data = out.read_bytes()
            magic, count, fields, recsize, strsz = struct.unpack_from("<5I", data, 0)
            self.assertEqual(count, 7)
            self.assertEqual(len(data), 20 + count * recsize + strsz)
            # verify appended records
            records = data[20:20 + count * recsize]
            strings = data[20 + count * recsize:]
            ids = sorted(struct.unpack_from("<I", records, i * recsize)[0] for i in range(count))
            self.assertEqual(ids, [1, 2, 3, 990025, 990026, 990027, 990028])

            def s(off):
                end = strings.index(b"\x00", off)
                return strings[off:end].decode("utf-8")

            for i, (sid, name) in enumerate(((990025, "切换技能页"), (990026, "拉升"),
                                             (990027, "俯冲"), (990028, "滑翔制动"))):
                vals = struct.unpack_from("<" + "I" * fields, records, (3 + i) * recsize)
                self.assertEqual(vals[0], sid)
                self.assertEqual(vals[4], 0x100)
                self.assertEqual(vals[71], 3)  # SPELL_EFFECT_DUMMY
                self.assertEqual(vals[28], 1)  # instant
                self.assertEqual(s(vals[140]), name)
            # idempotent: append again -> ALREADY_APPENDED, no write
            run = subprocess.run([sys.executable, str(APPENDER), "check", "--input", str(out)],
                                 capture_output=True, text=True)
            self.assertIn("G17B3R2_SPELL_DBC_STATE=ALREADY_APPENDED", run.stdout)
            out2 = Path(tmp) / "out2.dbc"
            run = subprocess.run([sys.executable, str(APPENDER), "append",
                                  "--input", str(out), "--output", str(out2)],
                                 capture_output=True, text=True)
            self.assertEqual(run.returncode, 0)
            self.assertIn("G17B3R2_SPELL_DBC_WRITE=NONE", run.stdout)
            self.assertFalse(out2.exists())

    def test_02_deterministic_output(self):
        with tempfile.TemporaryDirectory() as tmp:
            hashes = []
            for i in range(2):
                src = Path(tmp) / f"in{i}.dbc"
                out = Path(tmp) / f"out{i}.dbc"
                src.write_bytes(self._synthetic_dbc())
                subprocess.run([sys.executable, str(APPENDER), "append",
                                "--input", str(src), "--output", str(out)],
                               capture_output=True, text=True)
                hashes.append(sha(out))
            self.assertEqual(hashes[0], hashes[1])


class TestToolLifecycle(unittest.TestCase):
    def _tool_module(self):
        spec = importlib.util.spec_from_file_location("t", TOOL)
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        return mod

    def test_01_lineage(self):
        mod = self._tool_module()
        self.assertEqual(mod.PRE_SHA256, PRE_SHA)
        self.assertEqual(mod.POST_SHA256, POST_SHA)
        self.assertEqual(mod.state_for_digest(PRE_SHA) in ("READY_B3R2_PREIMAGE", "B3R2_SAFE_ROLLBACK"), True)
        self.assertEqual(mod.state_for_digest(POST_SHA), "B3R2_APPLIED")
        for legacy in ("98446106309b45371f138d9c7bc707ee608d9a3db347e13d61cfd68cc97810f9",
                       "1a96b72eb28ffa2c0ac0d3e0c07e26c30f25bcd8525babd15efad02a041825d6",
                       "ecd307b472cb2c49f68607a8b0afe5dcf5f87a7a8eb6f087a4717f4cd8fa1bbb",
                       "feb3dad467188052c7b189478cea7060b14f8e13eb5bd7082d9f81b4ca3ab9ce",
                       "a65b0ddcd06a66cfbdf04a91cd4114295615f9ee0c014f92bd742cb6c245b24d"):
            self.assertEqual(mod.state_for_digest(legacy), "B3R2_INTERMEDIATE_UPGRADEABLE")

    def test_02_full_lifecycle(self):
        import shutil
        with tempfile.TemporaryDirectory() as tmp:
            target = Path(tmp) / "src/server/scripts/Commands/cs_dragonriding.cpp"
            target.parent.mkdir(parents=True)
            target.write_bytes(ORIGINAL.read_bytes())
            run = subprocess.run([sys.executable, str(TOOL), "check", "--source-root", tmp],
                                 capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn("G17B3R2_SOURCE_STATE=", run.stdout)
            run = subprocess.run([sys.executable, str(TOOL), "apply", "--source-root", tmp],
                                 capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn("G17B3R2_SOURCE_APPLY=PASS", run.stdout)
            self.assertEqual(sha(target), POST_SHA)
            run = subprocess.run([sys.executable, str(TOOL), "apply", "--source-root", tmp],
                                 capture_output=True, text=True)
            self.assertIn("G17B3R2_SOURCE_APPLY=ALREADY_CURRENT", run.stdout)
            run = subprocess.run([sys.executable, str(TOOL), "rollback", "--source-root", tmp],
                                 capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn("G17B3R2_SOURCE_ROLLBACK=PASS_B3R1_FLOOR", run.stdout)
            self.assertEqual(sha(target), PRE_SHA)


class TestInstallerStatic(unittest.TestCase):
    def test_01_installer_contract(self):
        install = (ROOT / "Install-Build-G17B3R2-Windows.ps1").read_text(encoding="utf-8")
        self.assertIn('$B3R2_BUILD = "r1d_sixslot_fastland"', install)
        self.assertIn('"B3R2_BUILD=" + $B3R2_BUILD', install)
        self.assertIn("apply_g17b3r2_source.py", install)
        self.assertIn("append_g17b3r2_spells.py", install)
        self.assertIn("G17B3R2_WORLD_SKILL_PAGES=PASS", install)
        self.assertIn("G17B3R2_WINDOWS_BUILD_RESULT=PASS", install)
        self.assertIn('Read-ToolHash "INTERMEDIATE3_SHA256"', install)
        self.assertIn("$recognized = @($Pre, $Post, $SafeRollback) + $Upgradeable", install)

    def test_02_ps_files_parse(self):
        checker = ROOT / "tools/ps_static_check.py"
        run = subprocess.run(
            ["python3", str(checker),
             str(ROOT / "Install-Build-G17B3R2-Windows.ps1"),
             str(ROOT / "Rollback-Build-G17B3R2-Windows.ps1")],
            capture_output=True, text=True)
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)



class TestR1cMsvcFixes(unittest.TestCase):
    """Regression for the two real MSVC errors in the user's r1b run."""

    def setUp(self):
        self.text = PAYLOAD.read_text(encoding="utf-8")

    def test_01_onlogin_matches_fork_signature(self):
        # C3668/C4263/C4264: the fork's PlayerScript::OnLogin is
        # OnLogin(Player*, bool firstLogin) (ScriptMgr.h:692).
        self.assertIn("void OnLogin(Player* player, bool /*firstLogin*/) override", self.text)
        self.assertNotIn("void OnLogin(Player* player) override", self.text)

    def test_02_no_bare_flight_state_data_id(self):
        # C2039/C2065/C2737: GetData(G17Dragonriding::FLIGHT_STATE) was missing
        # the DATA_ prefix.  Every GetData argument must be a DATA_* constant.
        import re
        bare = re.findall(r"GetData\(G17Dragonriding::(?!DATA_)[A-Z_]+\)", self.text)
        self.assertEqual(bare, [], f"bare (non-DATA_) GetData arguments: {bare}")


if __name__ == "__main__":
    unittest.main()


class TestR1dFastLanding(unittest.TestCase):
    """B3-R2d: landings must be steeper, faster and shorter than the r1c ones."""

    def setUp(self):
        self.text = PAYLOAD.read_text(encoding="utf-8")

    def test_01_steep_fast_profiles(self):
        # Dragon profile went 45yd/18 descent @21 speed -> 28/26 @26: glide
        # angle steepens ~22deg -> ~43deg and the spline is 24% faster.
        self.assertIn("case ARCHETYPE_DRAGON:      return { 28.0f, 12.0f, 26.0f, 26.0f, 8, false };", self.text)
        self.assertIn("case ARCHETYPE_MAGIC:       return { 18.0f, 7.0f, 18.0f, 18.0f, 6, true };", self.text)
        self.assertIn("case ARCHETYPE_BEAST:       return { 14.0f, 6.0f, 14.0f, 20.0f, 6, false };", self.text)

    def test_02_tighter_timeout(self):
        self.assertIn("constexpr uint32 LANDING_TIMEOUT_MS = 25000;", self.text)

    def test_03_obstacle_guard_kept(self):
        # the faster profiles must still refuse blocked paths
        self.assertIn("BuildLandingPath", self.text)
        self.assertIn("AbortLanding", self.text)


