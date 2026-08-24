#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""F44 .combo 只读源码探针（schema 1）。

默认读取：D:\TrinityCore\src\server\scripts\Commands
也可传 --source-root 或 --commands-dir。不会修改源码、配置、数据库或Git。
"""
from __future__ import annotations

import argparse
import hashlib
import re
import sys
from pathlib import Path

FILES = ("cs_combathelper.cpp", "CombatSpecData.h", "CombatSpecData.cpp")


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def decode_source(data: bytes) -> tuple[str, str]:
    if data.startswith(b"\xef\xbb\xbf"):
        return data.decode("utf-8-sig"), "UTF-8-BOM"
    return data.decode("utf-8"), "UTF-8"


def count(text: str, pattern: str) -> int:
    return len(re.findall(pattern, text, flags=re.MULTILINE))


def matching_lines(text: str, pattern: str, limit: int = 12) -> list[str]:
    rx = re.compile(pattern)
    out: list[str] = []
    for number, line in enumerate(text.splitlines(), 1):
        if rx.search(line):
            out.append(f"{number:06d}: {line.rstrip()}")
            if len(out) >= limit:
                break
    return out


def analyze(cs: str, hdr: str, data: str) -> dict[str, object]:
    beacon_rows = matching_lines(data, r"\{\s*53563\s*,")
    earth_rows = matching_lines(data, r"\{\s*974\s*,")

    result: dict[str, object] = {
        "BEACON_ROWS": len(beacon_rows),
        "BEACON_HEALKIT_AS_HOT": bool(re.search(
            r"\{\s*53563\s*,\s*SF_HEAL\s*\|\s*SF_HOT\s*,", data)),
        "BEACON_ROTATION_AS_SELF_BUFF": bool(re.search(
            r"\{\s*53563\s*,\s*SF_SELF\s*\|\s*SF_BUFF_KEEP\s*,", data)),
        "EARTH_SHIELD_HEALKIT_AS_HOT": bool(re.search(
            r"\{\s*974\s*,\s*SF_HEAL\s*\|\s*SF_HOT\s*,", data)),
        "HEAL_TARGET_IS_DYNAMIC_LOWEST_HP": (
            "PickHealTarget(player, 40.0f, ht);" in cs
            and "weighted < curW" in cs),
        "HOT_CHECK_EXACT_REAL_SPELL": (
            "ht.unit->HasAura(realSpell, player->GetGUID())" in cs),
        "HEAL_CAST_IGNORES_SF_SELF": (
            "player->CastSpell(ht.unit, real, CastSpellExtraArgs(false))" in cs),
        "COMBO_TICK_HAS_GENERATION_TOKEN": bool(re.search(
            r"(?:tickGeneration|comboGeneration|generationToken|scheduleGeneration)", cs)),
        "SCHEDULE_INTERVAL_500MS": "Milliseconds(500)" in cs,
        "TAUNT_FLAG_DECLARED": "SF_TAUNT" in hdr,
        # 当前仓库只有.combo list诊断标签引用；没有“目标未攻击我”等资格判断。
        "TAUNT_SINGLE_RUNTIME_REFERENCE_COUNT": count(
            cs, r"CombatSpec::SF_TAUNT\b"),
        "TAUNT_AOE_RUNTIME_REFERENCE_COUNT": count(
            cs, r"CombatSpec::SF_TAUNT_AOE\b"),
        "TAUNT_NEED_CHECK_PRESENT": bool(re.search(
            r"CombatSpec::SF_TAUNT(?:_AOE)?[\s\S]{0,500}(?:GetVictim|victim|getAttackers)",
            cs)),
        "KEEPBUFFS_RUNTIME_REFERENCE_COUNT": count(cs, r"tune\.keepBuffs"),
        "RAID_BUFF_RUNTIME_REFERENCE_COUNT": count(cs, r"CombatSpec::SF_RAID_BUFF"),
        "DISPEL_CAST_RETURN_IGNORED": (
            "player->CastSpell(dt, real, CastSpellExtraArgs(false));" in cs
            and not re.search(r"SpellCastResult\s+\w+\s*=\s*\n?\s*player->CastSpell\(dt,", cs)),
        "HEAL_SCAN_INCLUDES_NPCBOT_GROUP_MEMBERS": (
            "GetAllGroupMembers" in cs or "GetFirstBotMember" in cs),
        "OPENER_ALWAYS_CASTS_SELF": "if (tryCast(sk, player))" in cs,
        "BUFF_QUEUE_USES_EXACT_AURA_ID": "if (p->HasAura(spellId))" in cs,
        "STATE_ERASED_ON_LOGOUT": bool(re.search(
            r"OnLogout[\s\S]{0,900}States\(\)\.erase", cs)),
        "PLAN_CACHE_ERASE_HOOK": bool(re.search(
            r"s_planCache\.erase|ClearPlanCache", cs)),
        "TARGET_MAINTAIN_FLAG_PRESENT": (
            "SF_TARGET_MAINTAIN" in hdr or "SF_MAINTAIN_TARGET" in hdr),
        "BEACON_LINES": beacon_rows,
        "EARTH_SHIELD_LINES": earth_rows,
    }

    root = (
        result["BEACON_HEALKIT_AS_HOT"]
        and result["HEAL_TARGET_IS_DYNAMIC_LOWEST_HP"]
        and result["HOT_CHECK_EXACT_REAL_SPELL"]
        and not result["TARGET_MAINTAIN_FLAG_PRESENT"]
    )
    result["BEACON_RECAST_ROOT_PATTERN_PRESENT"] = bool(root)
    return result


def run_self_test() -> int:
    cs = """
    PickHealTarget(player, 40.0f, ht);
    if (!out.unit || weighted < curW) {}
    if (ht.unit->HasAura(realSpell, player->GetGUID())) return 2;
    player->CastSpell(ht.unit, real, CastSpellExtraArgs(false));
    player->CastSpell(dt, real, CastSpellExtraArgs(false));
    if (tryCast(sk, player)) return;
    if (p->HasAura(spellId)) return;
    ScheduleTick(p); Milliseconds(500);
    """
    hdr = "SF_TAUNT = 0x2000, SF_RAID_BUFF = 0x20000;"
    data = """
      { 53563, SF_SELF|SF_BUFF_KEEP, "圣光道标" },
      { 53563, SF_HEAL|SF_HOT, "圣光道标" },
      { 974, SF_HEAL|SF_HOT, "大地之盾" },
    """
    a = analyze(cs, hdr, data)
    expected_true = (
        "BEACON_HEALKIT_AS_HOT",
        "HEAL_TARGET_IS_DYNAMIC_LOWEST_HP",
        "HOT_CHECK_EXACT_REAL_SPELL",
        "HEAL_CAST_IGNORES_SF_SELF",
        "DISPEL_CAST_RETURN_IGNORED",
        "BEACON_RECAST_ROOT_PATTERN_PRESENT",
    )
    for key in expected_true:
        if a.get(key) is not True:
            print(f"[FAIL] self-test {key}={a.get(key)!r}")
            return 1
    if a.get("COMBO_TICK_HAS_GENERATION_TOKEN") is not False:
        print("[FAIL] self-test generation detection")
        return 1
    print("[OK] F44 combo probe self-test passed")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="F44 .combo只读源码探针")
    parser.add_argument("--source-root", default=r"D:\TrinityCore",
                        help="TrinityCore源码根目录")
    parser.add_argument("--commands-dir", default="",
                        help="直接指定Commands目录（优先于--source-root）")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()

    if args.self_test:
        return run_self_test()

    commands = (Path(args.commands_dir) if args.commands_dir else
                Path(args.source_root) / "src" / "server" / "scripts" / "Commands")

    print("COMBO_PROBE_SCHEMA=1")
    print("MODE=READ_ONLY")
    print(f"COMMANDS_DIR={commands}")

    texts: dict[str, str] = {}
    all_ok = True
    for name in FILES:
        path = commands / name
        print(f"\n[FILE] {path}")
        if not path.is_file():
            print("EXISTS=False")
            all_ok = False
            continue
        raw = path.read_bytes()
        try:
            text, encoding = decode_source(raw)
        except UnicodeDecodeError as exc:
            print("EXISTS=True")
            print(f"BYTES={len(raw)}")
            print(f"SHA256={sha256(raw)}")
            print(f"UTF8_DECODE=False error={exc}")
            all_ok = False
            continue
        texts[name] = text
        print("EXISTS=True")
        print(f"BYTES={len(raw)}")
        print(f"SHA256={sha256(raw)}")
        print(f"ENCODING={encoding}")
        print(f"LINES={len(text.splitlines())}")

    if not all_ok or len(texts) != len(FILES):
        print("\nREADY_FOR_EXACT_FIX=False")
        print("REASON=missing_or_unreadable_required_file")
        return 2

    audit = analyze(texts["cs_combathelper.cpp"], texts["CombatSpecData.h"],
                    texts["CombatSpecData.cpp"])
    print("\n[STATIC_AUDIT]")
    for key, value in audit.items():
        if key.endswith("_LINES"):
            continue
        print(f"{key}={value}")

    print("\n[BEACON_CONTEXT]")
    for line in audit["BEACON_LINES"]:  # type: ignore[index]
        print(line)
    print("\n[EARTH_SHIELD_CONTEXT]")
    for line in audit["EARTH_SHIELD_LINES"]:  # type: ignore[index]
        print(line)

    anchors_ok = (
        int(audit["BEACON_ROWS"]) >= 1
        and "ComboTick" in texts["cs_combathelper.cpp"]
        and "CheckHealSkill" in texts["cs_combathelper.cpp"]
        and "BuildPlan" in texts["CombatSpecData.cpp"]
    )
    print(f"\nREADY_FOR_EXACT_FIX={anchors_ok}")
    print("SOURCE_OR_CONFIG_EDITS=0")
    print("DATABASE_EDITS=0")
    return 0 if anchors_ok else 3


if __name__ == "__main__":
    sys.exit(main())
