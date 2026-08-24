#!/usr/bin/env python3
"""F44受控数据、生成物、运行时接线及本地API签名审计。"""
from __future__ import annotations

import json
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[5]
F44 = REPO / "tc-bignum/补丁库/02_修复/F44_combo道标与全职业目标策略"
A06 = REPO / "tc-bignum/补丁库/01_功能/A06_战斗辅助/源文件"
TOOLS = REPO / "tc-bignum/tools"
SRC = F44 / "源文件/cs_combathelper.cpp"
HDR = F44 / "源文件/CombatSpecData.h"
CPP = F44 / "源文件/CombatSpecData.cpp"
BASE = TOOLS / "specdata_v3_base.json"
GEN = TOOLS / "gen_specdata_v3.py"


def must(ok: bool, message: str) -> None:
    if not ok:
        raise AssertionError(message)


def all_base_items(spec: dict):
    for group_index, group in enumerate(spec["groups"]):
        for item in group:
            yield group_index, item


def main() -> None:
    data = json.loads(BASE.read_text(encoding="utf-8"))
    must(len(data) == 31, f"expected 31 specs, got {len(data)}")
    count = sum(len(group) for spec in data for group in spec["groups"][:4])
    must(count == 515, f"expected 515 controlled entries, got {count}")

    forbidden_opener = {1160, 1130, 16857}
    for spec in data:
        must(not any(item[0] in forbidden_opener for item in spec["groups"][3]),
             f"enemy spell in opener: class={spec['cls']} spec={spec['idx']}")
        for _, item in all_base_items(spec):
            flags = set(item[1].split("|"))
            must(not ("SF_HEAL" in flags and "SF_DEBUFF_KEEP" in flags),
                 f"friendly heal marked debuff: {item}")
            must(not ("SF_SELF" in flags and "SF_MAINTAIN_FRIEND" in flags),
                 f"self/stable conflict: {item}")

    # 53563/974从动态基础rotation移除，由生成器稳定维护表注入。
    base_ids = [item[0] for spec in data for _, item in all_base_items(spec)]
    must(53563 not in base_ids and 974 not in base_ids,
         "beacon/earth shield leaked back into dynamic controlled rotation")

    subprocess.run([sys.executable, str(GEN), "--check"], cwd=REPO, check=True)
    must(CPP.read_bytes() == (A06 / "CombatSpecData.cpp").read_bytes(),
         "F44 and A06 generated cpp differ")
    must(HDR.read_bytes() == (A06 / "CombatSpecData.h").read_bytes(),
         "F44 and A06 headers differ")
    must(SRC.read_bytes() == (A06 / "cs_combathelper.cpp").read_bytes(),
         "F44 and A06 runtime source differ")

    cpp = CPP.read_text(encoding="utf-8")
    src = SRC.read_text(encoding="utf-8")
    hdr = HDR.read_text(encoding="utf-8")
    gen = GEN.read_text(encoding="utf-8")

    for spell, name in ((53563, "beacon"), (974, "earth shield"),
                        (50720, "vigilance"), (54646, "focus magic"),
                        (53601, "sacred shield"), (6346, "fear ward"),
                        (34477, "misdirection"), (57934, "tricks")):
        must(re.search(rf"\{{\s*{spell},\s*SF_MAINTAIN_FRIEND\|SF_BUFF_KEEP", cpp) is not None,
             f"{name} missing stable-friendly generated flags")

    for marker in (
        "SF_MAINTAIN_FRIEND", "SF_FRIEND", "ForEachFriendlyGroupUnit",
        "GetFirstBotMember", "FindMaintainedTarget", "CommitMaintainedTarget",
        "PlanCaches().erase", "generation != generation", "CastChecked",
        "CountNearbyTauntCandidates", "HasTauntCandidateForVictim",
        "tune.keepBuffs && !player->IsInCombat()",
    ):
        must(marker in src or marker in hdr, f"runtime/header marker missing: {marker}")

    # 自动路径中只能有CastChecked内部这一处原始CastSpell调用。
    raw_casts = re.findall(r"player->CastSpell\(", src)
    must(len(raw_casts) == 1, f"expected one raw CastSpell transaction, got {len(raw_casts)}")
    must("result != SPELL_CAST_OK" in src and "MarkFailed(player, spellId, result)" in src,
         "CastChecked return-value/backoff transaction incomplete")

    must("d.spell == s.spell && d.flags == s.flags" in cpp,
         "generated AppendUnique reverted to spell-only first-wins")
    must("d.spell == s.spell && d.flags == s.flags" in src,
         "CollectAllSpecSkills reverted to spell-only first-wins")
    must("static std::unordered_map<uint32, PlanCache> s_planCache" not in src,
         "unclearable local PlanCache returned")
    must("/tmp/specs.json" not in gen and "/home/user/tc-bignum/patches" not in gen,
         "generator still depends on old temporary/absolute path")

    # 本仓库快照中的真实API签名门槛。
    unit_h = (REPO / "src/Unit.h").read_text(encoding="utf-8", errors="replace")
    group_h = (REPO / "src/Group.h").read_text(encoding="utf-8", errors="replace")
    object_h = (REPO / "src/Object.h").read_text(encoding="utf-8", errors="replace")
    must("GetAuraApplication(uint32 spellId, ObjectGuid casterGUID" in unit_h,
         "caster-aware aura API unavailable")
    must("HasAura(uint32 spellId, ObjectGuid casterGUID" in unit_h,
         "caster-aware HasAura API unavailable")
    must("GetFirstBotMember()" in group_h and "GetMemberFlags(ObjectGuid guid)" in group_h,
         "NPCBot/main-tank group APIs unavailable")
    must("SpellCastResult CastSpell(" in object_h,
         "CastSpell return type is not SpellCastResult")

    print("[OK] F44_STATIC_TESTS=PASS specs=31 base_entries=515 raw_casts=1")


if __name__ == "__main__":
    main()
