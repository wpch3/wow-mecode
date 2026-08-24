#!/usr/bin/env python3
"""F44R1受控数据、生成物、运行时接线和安装前后像静态门槛。"""
from __future__ import annotations

import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[5]
PATCH = REPO / "tc-bignum/补丁库/02_修复/F44R1_combo增益生命周期与治疗优先"
A06 = REPO / "tc-bignum/补丁库/01_功能/A06_战斗辅助/源文件"
TOOLS = REPO / "tc-bignum/tools"
SRC = PATCH / "源文件/cs_combathelper.cpp"
HDR = PATCH / "源文件/CombatSpecData.h"
CPP = PATCH / "源文件/CombatSpecData.cpp"
BASE = TOOLS / "specdata_v3_base.json"
GEN = TOOLS / "gen_specdata_v3.py"
INSTALLER = PATCH / "install_f44r1_combo.py"
ORIGINAL = PATCH / "原始文件/F44_真人缺陷版"


def must(ok: bool, message: str) -> None:
    if not ok:
        raise AssertionError(message)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def items(data: list[dict]):
    for spec in data:
        for group_index, group in enumerate(spec["groups"]):
            for item in group:
                yield spec, group_index, item


def main() -> None:
    data = json.loads(BASE.read_text(encoding="utf-8"))
    must(len(data) == 31, f"expected 31 specs, got {len(data)}")
    count = sum(len(g) for s in data for g in s["groups"][:4])
    must(count == 493, f"expected 493 controlled entries after audit, got {count}")

    by_spell: dict[int, list[tuple[dict, int, list]]] = {}
    for spec, group, item in items(data):
        by_spell.setdefault(item[0], []).append((spec, group, item))
        flags = set(item[1].split("|"))
        must(not ({"SF_MANUAL", "SF_DEPLOYABLE"} <= flags), f"manual/deploy conflict: {item}")
        if "SF_MANUAL" in flags:
            must("SF_BUFF_KEEP" not in flags, f"manual item still aura-maintained: {item}")
        if "SF_DEPLOYABLE" in flags:
            must("SF_SELF" in flags and "SF_EXCLUSIVE_BUFF" in flags,
                 f"deployable missing self/family classification: {item}")

    # 无Unit Aura或需要物品目标的技能不得自动维护；部分由生成器增强表注入。
    controlled_text = BASE.read_text(encoding="utf-8") + GEN.read_text(encoding="utf-8")
    for spell in (2823, 8679, 13219, 8024, 8232, 51730, 21169):
        must(re.search(rf"[\[(]\s*{spell}\s*,\s*['\"]SF_SELF\|SF_MANUAL", controlled_text) is not None,
             f"manual skill missing or leaked to auto path: {spell}")

    # 图腾只能按每场战斗部署；元素萨满不再同时规划灼热+愤怒火图腾。
    for spell in (30706, 8075, 8512, 5675):
        must(spell in by_spell and all("SF_DEPLOYABLE" in x[2][1] for x in by_spell[spell]),
             f"totem not deployable: {spell}")
    elemental = next(s for s in data if s["cls"] == 7 and s["idx"] == 0)
    must(3599 not in [x[0] for x in elemental["groups"][3]],
         "elemental fire-totem conflict returned")

    # 已定位的错误分组必须彻底退出受控基础数据。
    all_ids = [item[0] for _, _, item in items(data)]
    must(54424 not in all_ids, "pet-only Fel Intelligence remained in player auto data")
    holy_pal = next(s for s in data if s["cls"] == 2 and s["idx"] == 0)
    must(54428 not in [x[0] for x in holy_pal["groups"][3]], "Divine Plea remained opener buff")
    must(4987 not in [x[0] for x in holy_pal["groups"][0]], "Cleanse remained enemy rotation")
    for spec in (s for s in data if s["cls"] == 9):
        must(1454 not in [x[0] for x in spec["groups"][1]], "Life Tap remained unconditional burst")

    subprocess.run([sys.executable, str(GEN), "--check"], cwd=REPO, check=True)
    must(CPP.read_bytes() == (A06 / "CombatSpecData.cpp").read_bytes(), "patch/A06 cpp differ")
    must(HDR.read_bytes() == (A06 / "CombatSpecData.h").read_bytes(), "patch/A06 header differ")
    must(SRC.read_bytes() == (A06 / "cs_combathelper.cpp").read_bytes(), "patch/A06 runtime differ")
    must((PATCH / "工具/gen_specdata_v3.py").read_bytes() == GEN.read_bytes(), "delivered generator differs")
    must((PATCH / "工具/specdata_v3_base.json").read_bytes() == BASE.read_bytes(), "delivered JSON differs")

    src = SRC.read_text(encoding="utf-8")
    cpp = CPP.read_text(encoding="utf-8")
    hdr = HDR.read_text(encoding="utf-8")
    gen = GEN.read_text(encoding="utf-8")

    for marker in (
        "SF_EXCLUSIVE_BUFF", "SF_MANUAL", "SF_DEPLOYABLE", "SF_COMBAT_UTILITY",
        "ExclusiveBuffFamily", "AppendOpenerUnique", "PreferredPaladinBlessing",
        "IsPreferredAutoBuffTarget", "AuraRemainMs(target, cast.rank1, auraCaster) != 0",
        "deployedThisCombat", "healerWaitingForHeal", "healingMustPreempt",
        "待执行增益队列已取消", "nowMs - bs.lastBuffTry < 2000",
    ):
        must(marker in src or marker in cpp or marker in hdr, f"required marker missing: {marker}")

    must("refreshWindow" not in src, "early refresh window still present in runtime")
    must("remain > 0 && remain <=" not in src, "early refresh comparison still present")
    must("if (autoBuff)" in src and "tune.keepBuffs && !player->IsInCombat()" not in src,
         "normal buffs are still restricted to out-of-combat/high-scene maintenance")
    must("SF_MAINTAIN_FRIEND|SF_COMBAT_UTILITY" in cpp,
         "misdirection/tricks did not move to combat utility")
    must(re.search(r"\{\s*53563,\s*SF_MAINTAIN_FRIEND\|SF_BUFF_KEEP", cpp) is not None,
         "Beacon stable target semantics regressed")
    must(re.search(r"\{\s*974,\s*SF_MAINTAIN_FRIEND\|SF_BUFF_KEEP", cpp) is not None,
         "Earth Shield stable target semantics regressed")
    must("(25894,'SF_RAID_BUFF|SF_BUFF_KEEP|SF_EXCLUSIVE_BUFF'" in gen,
         "paladin blessing family classification missing")

    raw_casts = re.findall(r"player->CastSpell\(", src)
    must(len(raw_casts) == 1, f"expected one checked raw CastSpell site, got {len(raw_casts)}")
    must("result != SPELL_CAST_OK" in src and "MarkFailed(player, spellId, result)" in src,
         "checked cast/backoff transaction incomplete")

    expected_pre = {
        "cs_combathelper.cpp": "5f31a5b97fa0ffe99d1472b370660dc86164ddf3f402bc16b030ee47418f31da",
        "CombatSpecData.h": "af3e9c2575b725fa50e7bee921d1f7612546e04b967cb2da149e4fe6a2bdd4a7",
        "CombatSpecData.cpp": "1cdd6d3a8b07a4915de60746c04848f00bae2608a9c9cd25982d6f6e6a14272e",
    }
    for name, digest in expected_pre.items():
        must(sha(ORIGINAL / name) == digest, f"locked F44 preimage mismatch: {name}")
    installer = INSTALLER.read_text(encoding="utf-8")
    for name in expected_pre:
        must(sha(PATCH / "源文件" / name) in installer, f"postimage hash absent: {name}")

    print(f"[OK] F44R1_STATIC_TESTS=PASS specs=31 controlled_entries={count} raw_casts=1")


if __name__ == "__main__":
    main()
