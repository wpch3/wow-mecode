#!/usr/bin/env python3
r"""G17 read-only source probe v2 for the user's real D:\TrinityCore tree.

Collects exact hashes, source anchors, conflict markers, configuration/parsing
APIs, location-update callers, and existing vehicle/flight capabilities. It
never edits source/config/database; its only non-test write is the requested
UTF-8-BOM report.
"""

from __future__ import annotations

import argparse
import hashlib
import os
import re
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path

REQUIRED = {
    "SpellInfo.cpp": Path("src/server/game/Spells/SpellInfo.cpp"),
    "Player.cpp": Path("src/server/game/Entities/Player/Player.cpp"),
    "DBCStructure.h": Path("src/server/shared/DataStores/DBCStructure.h"),
    "SharedDefines.h": Path("src/server/shared/SharedDefines.h"),
    "DBCEnums.h": Path("src/server/shared/DataStores/DBCEnums.h"),
    "Config.h": Path("src/common/Configuration/Config.h"),
    "StringConvert.h": Path("src/common/Utilities/StringConvert.h"),
    "Util.h": Path("src/common/Utilities/Util.h"),
}

OPTIONAL = {
    "Map.h": Path("src/server/game/Maps/Map.h"),
    "World.cpp": Path("src/server/game/World/World.cpp"),
    "World.h": Path("src/server/game/World/World.h"),
    "worldserver.conf.dist": Path("src/server/worldserver/worldserver.conf.dist"),
    "Vehicle.cpp": Path("src/server/game/Entities/Vehicle/Vehicle.cpp"),
    "Vehicle.h": Path("src/server/game/Entities/Vehicle/Vehicle.h"),
    "Unit.cpp": Path("src/server/game/Entities/Unit/Unit.cpp"),
    "bot_ai.cpp": Path("src/server/game/AI/NpcBots/bot_ai.cpp"),
    "Spell.cpp": Path("src/server/game/Spells/Spell.cpp"),
    "spell_generic.cpp": Path("src/server/scripts/Spells/spell_generic.cpp"),
}

TEXT_SUFFIXES = {".cpp", ".c", ".h", ".hpp", ".conf", ".dist", ".in"}
CONTEXT_REQUESTS = {
    "SpellInfo.cpp": (
        '#include "SpellInfo.h"',
        '#include "Config.h"',
        '#include "StringConvert.h"',
        '#include "Util.h"',
        "uint32 GetTargetFlagMask",
        "SpellInfo::CheckLocation",
        "SPELL_ATTR4_CAST_ONLY_IN_OUTLAND",
        "IsFlyable()",
        "CanFlyInZone",
    ),
    "Player.cpp": (
        "Player::CanFlyInZone",
        "54197",
        "UpdateAreaDependentAuras",
        "CheckLocation(GetMapId(), m_zoneUpdateId, newArea, this, false)",
    ),
    "DBCStructure.h": (
        "bool IsFlyable() const",
        "struct MapEntry",
        "bool IsWorldMap() const",
        "bool IsContinent() const",
        "struct VehicleSeatEntry",
    ),
    "SharedDefines.h": ("SPELL_ATTR4_CAST_ONLY_IN_OUTLAND",),
    "DBCEnums.h": (
        "AREA_FLAG_SLAVE_CAPITAL",
        "AREA_FLAG_CAPITAL",
        "AREA_FLAG_CITY",
        "AREA_FLAG_NO_FLY_ZONE",
    ),
    "Config.h": ("GetStringDefault", "GetBoolDefault"),
    "StringConvert.h": ("StringTo",),
    "Util.h": ("Tokenize",),
    "Map.h": ("IsBattlegroundOrArena", "IsDungeon"),
    "World.cpp": ("LoadConfigSettings", "GetStringDefault", "GetBoolDefault"),
    "worldserver.conf.dist": ("AllowTwoSide", "Rate."),
    "Vehicle.cpp": ("VEHICLE_SEAT_FLAG_CAN_CONTROL",),
    "Unit.cpp": ("SetCanFly", "SetDisableGravity", "MOVE_FLIGHT"),
    "bot_ai.cpp": (
        "OnBotEnterVehicle",
        "VEHICLE_SEAT_FLAG_CAN_CONTROL",
        "SetCanFly",
        "POWER_ENERGY",
        "CheckLocation(me->GetMapId()",
    ),
    "Spell.cpp": ("CheckLocation(m_caster->GetMapId()",),
    "spell_generic.cpp": ("spellInfo->CheckLocation(target->GetMapId()",),
}

CONTEXT_RADII = {
    "SpellInfo.cpp::#include \"SpellInfo.h\"": 24,
    "SpellInfo.cpp::uint32 GetTargetFlagMask": 10,
    "SpellInfo.cpp::SpellInfo::CheckLocation": 52,
    "Player.cpp::UpdateAreaDependentAuras": 18,
    "DBCStructure.h::struct MapEntry": 58,
    "DBCStructure.h::bool IsFlyable() const": 14,
    "DBCEnums.h::AREA_FLAG_CAPITAL": 18,
    "Config.h::GetStringDefault": 10,
    "StringConvert.h::StringTo": 12,
    "Util.h::Tokenize": 12,
}


@dataclass(frozen=True)
class SourceFile:
    label: str
    relative: Path
    path: Path
    text: str
    lines: tuple[str, ...]
    sha256: str
    size: int


def decode_bytes(data: bytes) -> str:
    for encoding in ("utf-8-sig", "utf-8", "gb18030"):
        try:
            return data.decode(encoding)
        except UnicodeDecodeError:
            pass
    return data.decode("utf-8", errors="replace")


def load_source(root: Path, label: str, relative: Path) -> SourceFile | None:
    path = root / relative
    if not path.is_file():
        return None
    data = path.read_bytes()
    text = decode_bytes(data)
    return SourceFile(
        label=label,
        relative=relative,
        path=path,
        text=text,
        lines=tuple(text.splitlines()),
        sha256=hashlib.sha256(data).hexdigest(),
        size=len(data),
    )


def matching_lines(source: SourceFile, needle: str) -> list[int]:
    return [i for i, line in enumerate(source.lines, 1) if needle in line]


def context(source: SourceFile, line_number: int, radius: int = 8) -> list[str]:
    start = max(1, line_number - radius)
    end = min(len(source.lines), line_number + radius)
    return [f"{n:06d}: {source.lines[n - 1]}" for n in range(start, end + 1)]


def run_git(root: Path, *args: str) -> tuple[int, str]:
    try:
        result = subprocess.run(
            ["git", "-C", str(root), *args],
            check=False,
            capture_output=True,
            text=True,
            timeout=20,
        )
    except (OSError, subprocess.TimeoutExpired) as exc:
        return 127, str(exc)
    output = (result.stdout + result.stderr).strip()
    return result.returncode, output or "<empty>"


def scan_tree(root: Path, needles: tuple[str, ...]) -> dict[str, list[str]]:
    results = {needle: [] for needle in needles}
    source_root = root / "src"
    if not source_root.is_dir():
        return results
    for path in sorted(source_root.rglob("*"), key=lambda p: str(p).casefold()):
        if not path.is_file() or path.suffix.casefold() not in TEXT_SUFFIXES:
            continue
        try:
            text = decode_bytes(path.read_bytes())
        except OSError:
            continue
        relative = path.relative_to(root)
        for line_number, line in enumerate(text.splitlines(), 1):
            for needle in needles:
                if needle.casefold() in line.casefold() and len(results[needle]) < 100:
                    results[needle].append(f"{relative}:{line_number}: {line.strip()}")
    return results


def parse_attr_mask(text: str) -> str:
    match = re.search(
        r"SPELL_ATTR4_CAST_ONLY_IN_OUTLAND\s*=\s*(0x[0-9A-Fa-f]+|\d+)",
        text,
    )
    return match.group(1) if match else "<not-found>"


def analyze(root: Path) -> tuple[str, dict[str, object]]:
    root = root.resolve()
    loaded: dict[str, SourceFile] = {}
    missing_required: list[str] = []
    missing_optional: list[str] = []

    for label, relative in REQUIRED.items():
        source = load_source(root, label, relative)
        if source is None:
            missing_required.append(str(relative))
        else:
            loaded[label] = source
    for label, relative in OPTIONAL.items():
        source = load_source(root, label, relative)
        if source is None:
            missing_optional.append(str(relative))
        else:
            loaded[label] = source

    lines: list[str] = []
    add = lines.append
    add("G17 read-only source probe v2")
    add("PROBE_SCHEMA=2")
    add(f"Root={root}")
    add("Source/config/database edits=0")

    add("")
    add("=== 1. GIT IDENTITY ===")
    for title, args in (
        ("BRANCH", ("branch", "--show-current")),
        ("HEAD", ("rev-parse", "HEAD")),
        ("STATUS", ("status", "--short")),
    ):
        rc, output = run_git(root, *args)
        add(f"{title}_EXIT={rc}")
        for row in output.splitlines() or ["<empty>"]:
            add(f"{title} {row}")

    add("")
    add("=== 2. REQUIRED AND OPTIONAL FILES ===")
    for label, relative in {**REQUIRED, **OPTIONAL}.items():
        source = loaded.get(label)
        required = label in REQUIRED
        if source is None:
            add(f"EXISTS=False REQUIRED={required} LABEL={label} PATH={root / relative}")
        else:
            add(
                f"EXISTS=True REQUIRED={required} LABEL={label} PATH={source.path} "
                f"SIZE={source.size} LINES={len(source.lines)} SHA256={source.sha256}"
            )

    add("")
    add("=== 3. EXACT ANCHOR COUNTS ===")
    anchor_counts: dict[str, int] = {}
    for label, requests in CONTEXT_REQUESTS.items():
        source = loaded.get(label)
        for needle in requests:
            key = f"{label}::{needle}"
            count = source.text.count(needle) if source else 0
            anchor_counts[key] = count
            add(f"COUNT={count} FILE={label} NEEDLE={needle}")

    shared_source = loaded.get("SharedDefines.h")
    attr_mask = parse_attr_mask(shared_source.text if shared_source else "")
    add(f"SPELL_ATTR4_CAST_ONLY_IN_OUTLAND_MASK={attr_mask}")

    add("")
    add("=== 4. EXACT SOURCE CONTEXTS ===")
    source_context_block_count = 0
    for label, requests in CONTEXT_REQUESTS.items():
        source = loaded.get(label)
        if source is None:
            continue
        for needle in requests:
            hit_lines = matching_lines(source, needle)
            if not hit_lines:
                continue
            source_context_block_count += 1
            add(f"--- FILE={source.relative} NEEDLE={needle} FIRST_LINE={hit_lines[0]} ---")
            radius = CONTEXT_RADII.get(f"{label}::{needle}", 8)
            lines.extend(context(source, hit_lines[0], radius))

    tree_hits = scan_tree(
        root,
        (
            "G17",
            "WorldFlight.",
            "SPELL_ATTR4_CAST_ONLY_IN_OUTLAND",
            "VEHICLE_SEAT_FLAG_CAN_CONTROL",
            "SetCanFly(",
            "SetDisableGravity(",
            "POWER_ENERGY",
        ),
    )
    g17_conflicts = tree_hits["G17"] + tree_hits["WorldFlight."]

    add("")
    add("=== 5. TREE-WIDE CONFLICT AND CAPABILITY SEARCH ===")
    for needle, hits in tree_hits.items():
        add(f"TREE_HITS={len(hits)} NEEDLE={needle}")
        for hit in hits[:40]:
            add(f"TREE {hit}")

    spell = loaded.get("SpellInfo.cpp")
    player = loaded.get("Player.cpp")
    dbc = loaded.get("DBCStructure.h")
    shared = loaded.get("SharedDefines.h")
    dbc_enums = loaded.get("DBCEnums.h")
    config_h = loaded.get("Config.h")
    string_convert = loaded.get("StringConvert.h")
    util_h = loaded.get("Util.h")

    g17_a_ready = bool(
        not missing_required
        and spell
        and player
        and dbc
        and shared
        and dbc_enums
        and config_h
        and string_convert
        and util_h
        and spell.text.count("SpellInfo::CheckLocation") == 1
        and spell.text.count("SPELL_ATTR4_CAST_ONLY_IN_OUTLAND") == 1
        and spell.text.count("IsFlyable()") == 1
        and spell.text.count("CanFlyInZone") == 1
        and player.text.count("Player::CanFlyInZone") == 1
        and player.text.count("CheckLocation(GetMapId(), m_zoneUpdateId, newArea, this, false)") == 1
        and dbc.text.count("bool IsFlyable() const") == 1
        and dbc.text.count("struct MapEntry") == 1
        and "bool IsWorldMap() const" in dbc.text
        and "bool IsContinent() const" in dbc.text
        and "AREA_FLAG_NO_FLY_ZONE" in dbc_enums.text
        and "AREA_FLAG_CAPITAL" in dbc_enums.text
        and "GetBoolDefault" in config_h.text
        and "GetStringDefault" in config_h.text
        and "StringTo" in string_convert.text
        and "Tokenize" in util_h.text
        and attr_mask != "<not-found>"
        and source_context_block_count > 0
        and not g17_conflicts
    )

    vehicle_evidence = sum(
        len(tree_hits[key])
        for key in (
            "VEHICLE_SEAT_FLAG_CAN_CONTROL",
            "SetCanFly(",
            "SetDisableGravity(",
            "POWER_ENERGY",
        )
    )
    g17_b_probe_ready = bool(
        loaded.get("DBCStructure.h")
        and "struct VehicleSeatEntry" in loaded["DBCStructure.h"].text
        and vehicle_evidence >= 4
    )

    add("")
    add("=== 6. VERDICT ===")
    add("PROBE_SCHEMA=2")
    add(f"SOURCE_CONTEXT_BLOCK_COUNT={source_context_block_count}")
    add(f"MISSING_REQUIRED_COUNT={len(missing_required)}")
    for item in missing_required:
        add(f"MISSING_REQUIRED={item}")
    add(f"MISSING_OPTIONAL_COUNT={len(missing_optional)}")
    for item in missing_optional:
        add(f"MISSING_OPTIONAL={item}")
    add(f"G17_EXISTING_CONFLICT_COUNT={len(g17_conflicts)}")
    add(f"G17_A_WORLD_FLIGHT_PROBE_READY={g17_a_ready}")
    add(f"G17_B_VEHICLE_PROBE_READY={g17_b_probe_ready}")
    if g17_a_ready:
        add("NEXT=Generate a hash-locked, config-gated G17-A installer from these exact contexts.")
    else:
        add("NEXT=Do not patch; resolve missing anchors/files or existing G17 conflicts first.")
    add("[OK] Read-only source probe complete; only the report file will be written.")

    metrics: dict[str, object] = {
        "schema": 2,
        "source_context_blocks": source_context_block_count,
        "missing_required": len(missing_required),
        "attr_mask": attr_mask,
        "g17_conflicts": len(g17_conflicts),
        "g17_a_ready": g17_a_ready,
        "g17_b_probe_ready": g17_b_probe_ready,
        "anchor_counts": anchor_counts,
    }
    return "\n".join(lines) + "\n", metrics


def write_report(path: Path, report: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(report, encoding="utf-8-sig", newline="\r\n")


def self_test_require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(f"self-test assertion failed: {message}")


def fixture_files(root: Path, *, conflict: bool = False) -> None:
    content = {
        REQUIRED["SpellInfo.cpp"]: """#include \"SpellInfo.h\"\nuint32 GetTargetFlagMask() { return 0; }\nSpellCastResult SpellInfo::CheckLocation() const\n{\n if (HasAttribute(SPELL_ATTR4_CAST_ONLY_IN_OUTLAND))\n  if (!areaEntry->IsFlyable() || !player->CanFlyInZone(map_id, zone_id, this)) return FAIL;\n}\n""",
        REQUIRED["Player.cpp"]: """bool Player::CanFlyInZone() const\n{\n if (!HasSpell(54197)) return false;\n return true;\n}\nvoid UpdateAreaDependentAuras() { CheckLocation(GetMapId(), m_zoneUpdateId, newArea, this, false); }\n""",
        REQUIRED["DBCStructure.h"]: """bool IsFlyable() const { return true; }\nstruct MapEntry { bool IsWorldMap() const; bool IsContinent() const; };\nstruct VehicleSeatEntry {};\n""",
        REQUIRED["SharedDefines.h"]: """SPELL_ATTR4_CAST_ONLY_IN_OUTLAND = 0x04000000,\n""",
        REQUIRED["DBCEnums.h"]: "AREA_FLAG_SLAVE_CAPITAL\nAREA_FLAG_CAPITAL\nAREA_FLAG_CITY\nAREA_FLAG_NO_FLY_ZONE\n",
        REQUIRED["Config.h"]: "std::string GetStringDefault();\nbool GetBoolDefault();\n",
        REQUIRED["StringConvert.h"]: "StringTo\n",
        REQUIRED["Util.h"]: "Tokenize\n",
        OPTIONAL["Map.h"]: "IsBattlegroundOrArena\nIsDungeon\n",
        OPTIONAL["World.cpp"]: "LoadConfigSettings\nGetOption<\n",
        OPTIONAL["World.h"]: "class World {};\n",
        OPTIONAL["worldserver.conf.dist"]: "AllowTwoSide\nRate.\n",
        OPTIONAL["Vehicle.cpp"]: "VEHICLE_SEAT_FLAG_CAN_CONTROL\n",
        OPTIONAL["Vehicle.h"]: "class Vehicle {};\n",
        OPTIONAL["Unit.cpp"]: "SetCanFly(\nSetDisableGravity(\nMOVE_FLIGHT\nPOWER_ENERGY\n",
        OPTIONAL["bot_ai.cpp"]: "OnBotEnterVehicle\nVEHICLE_SEAT_FLAG_CAN_CONTROL\nSetCanFly(\nPOWER_ENERGY\n",
    }
    if conflict:
        content[OPTIONAL["World.cpp"]] += "WorldFlight.Enable G17\n"
    for relative, text in content.items():
        path = root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")


def run_self_test() -> None:
    with tempfile.TemporaryDirectory(prefix="g17_source_probe_") as temp:
        positive_root = Path(temp) / "positive"
        fixture_files(positive_root)
        report, metrics = analyze(positive_root)
        self_test_require(metrics["missing_required"] == 0, "positive required files")
        self_test_require(metrics["attr_mask"] == "0x04000000", "positive attr mask")
        self_test_require(metrics["g17_conflicts"] == 0, "positive conflict count")
        self_test_require(metrics["g17_a_ready"] is True, "positive G17-A readiness")
        self_test_require(metrics["g17_b_probe_ready"] is True, "positive G17-B readiness")
        self_test_require("000002:" in report, "numbered source context")

        rendered = Path(temp) / "rendered" / "g17_source_probe.txt"
        write_report(rendered, report)
        data = rendered.read_bytes()
        self_test_require(data.startswith(b"\xef\xbb\xbf"), "UTF-8 BOM")
        self_test_require(b"\r\n" in data, "CRLF")
        rendered_text = data.decode("utf-8-sig")
        for required in (
            "PROBE_SCHEMA=2",
            "SOURCE_CONTEXT_BLOCK_COUNT=",
            "SPELL_ATTR4_CAST_ONLY_IN_OUTLAND_MASK=0x04000000",
            "G17_A_WORLD_FLIGHT_PROBE_READY=True",
            "G17_B_VEHICLE_PROBE_READY=True",
        ):
            self_test_require(required in rendered_text, f"rendered field {required}")

        negative_root = Path(temp) / "negative"
        fixture_files(negative_root, conflict=True)
        (negative_root / REQUIRED["SpellInfo.cpp"]).unlink()
        _, negative = analyze(negative_root)
        self_test_require(negative["missing_required"] == 1, "negative missing file")
        self_test_require(negative["g17_conflicts"] >= 1, "negative conflict")
        self_test_require(negative["g17_a_ready"] is False, "negative no false-pass")

    print("[OK] Positive fixture: corrected SharedDefines path/config/parser/location anchors passed.")
    print("[OK] Negative fixture: missing source + existing G17 conflict did not false-pass.")
    print("[OK] Final UTF-8-BOM/CRLF report rendering and schema-2 fields passed.")
    print("[OK] G17 source probe v2 self-test passed.")


def default_output() -> Path:
    return Path.home() / "Desktop" / "g17_source_probe.txt"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", nargs="?", default=r"D:\TrinityCore")
    parser.add_argument("output", nargs="?", default=str(default_output()))
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args(argv)

    if args.self_test:
        run_self_test()
        return 0

    report, _ = analyze(Path(args.root))
    output = Path(args.output)
    write_report(output, report)
    sys.stdout.write(report)
    print(f"[OK] Wrote UTF-8-BOM report: {output}")
    print("[OK] Source/config/database edits: 0")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"[FAILED] G17 source probe: {exc}", file=sys.stderr)
        raise SystemExit(2)
