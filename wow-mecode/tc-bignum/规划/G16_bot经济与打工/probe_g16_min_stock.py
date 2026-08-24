#!/usr/bin/env python3
"""G16 read-only source probe for per-entry minimum stock and legendary filtering.

This script reads source/Git metadata and writes one UTF-8-BOM CRLF report.
It never edits TrinityCore source files.
"""

from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import sys
from typing import Iterable


REQUIRED_FILES = (
    "src/server/game/AuctionHouseBot/AuctionHouseBotSeller.h",
    "src/server/game/AuctionHouseBot/AuctionHouseBotSeller.cpp",
    "src/server/game/AuctionHouseBot/AuctionHouseBot.h",
    "src/server/game/AuctionHouseBot/AuctionHouseBot.cpp",
    "src/server/game/AuctionHouse/AuctionHouseMgr.h",
    "src/server/game/AuctionHouse/AuctionHouseMgr.cpp",
)

PATTERNS = (
    "class TC_GAME_API AuctionBotSeller",
    "bool AuctionBotSeller::Initialize()",
    "void AuctionBotSeller::AddNewAuctions",
    "SelectRandomContainerElement(_itemPool",
    "_itemPool[prototype->GetQuality()][prototype->GetClass()].push_back(itemId)",
    "forceIncludeItems",
    "forceExcludeItems",
    "enum AuctionBotConfigUInt32Values",
    "void AuctionBotConfig::GetConfigFromFile()",
    "bool IsBotChar(uint32 characterID) const",
    "GetAuctionsBegin()",
    "GetAuctionsEnd()",
    "auctionHouse->AddAuction(auctionEntry)",
    "ITEM_SUBCLASS_JUNK_PET",
    "ITEM_SUBCLASS_JUNK_MOUNT",
    "CONFIG_AHBOT_MIN_COPIES_PER_ENTRY",
    "AuctionHouseBot.Items.MinCopiesPerEntry",
    "BuildEntryDeficits",
    "IsAllowedLegendary",
)


def run(command: list[str], cwd: Path) -> tuple[int, str]:
    proc = subprocess.run(
        command,
        cwd=cwd,
        stdin=subprocess.DEVNULL,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
    )
    return proc.returncode, proc.stdout.rstrip("\r\n")


def decode_source(data: bytes) -> tuple[str, str]:
    if data.startswith(b"\xef\xbb\xbf"):
        return data.decode("utf-8-sig"), "UTF-8 BOM"
    for encoding, label in (("utf-8", "UTF-8 no BOM"), ("gb18030", "GB18030/GBK")):
        try:
            return data.decode(encoding), label
        except UnicodeDecodeError:
            pass
    return data.decode("utf-8", errors="replace"), "unknown (UTF-8 replacement used in report)"


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def newline_label(data: bytes) -> str:
    crlf = data.count(b"\r\n")
    lf = data.count(b"\n")
    if crlf and crlf == lf:
        return "CRLF"
    if lf and not crlf:
        return "LF"
    if lf:
        return f"mixed (LF={lf}, CRLF={crlf})"
    return "no newline"


def line_hits(text: str, needle: str) -> list[int]:
    return [number for number, line in enumerate(text.splitlines(), 1) if needle in line]


def add_section(lines: list[str], title: str) -> None:
    lines.extend(("", "=" * 78, title, "=" * 78))


def numbered_lines(text: str) -> Iterable[str]:
    for number, line in enumerate(text.splitlines(), 1):
        yield f"{number:6d}: {line}"


def is_inside(child: Path, parent: Path) -> bool:
    try:
        child.relative_to(parent)
        return True
    except ValueError:
        return False


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Read-only G16 AHBot source probe; writes a complete report."
    )
    parser.add_argument("source_root", help=r"TrinityCore root, for example D:\TrinityCore")
    parser.add_argument("report", help=r"Output report, for example D:\probe_g16_min_stock.txt")
    args = parser.parse_args()

    root = Path(args.source_root).expanduser().resolve()
    report = Path(args.report).expanduser().resolve()

    if not root.is_dir():
        print(f"[ERROR] Source root does not exist: {root}", file=sys.stderr)
        return 2
    if is_inside(report, root):
        print("[ERROR] Put the report outside the source tree so Git status remains unchanged.", file=sys.stderr)
        return 2

    file_data: dict[str, bytes] = {}
    file_text: dict[str, str] = {}
    file_encoding: dict[str, str] = {}
    missing: list[str] = []
    for rel in REQUIRED_FILES:
        path = root / rel
        if not path.is_file():
            missing.append(rel)
            continue
        data = path.read_bytes()
        text, encoding = decode_source(data)
        file_data[rel] = data
        file_text[rel] = text
        file_encoding[rel] = encoding

    if missing:
        print("[ERROR] Required files are missing:", file=sys.stderr)
        for rel in missing:
            print(f"  - {rel}", file=sys.stderr)
        return 3

    git_commands = {
        "root": ["git", "rev-parse", "--show-toplevel"],
        "branch": ["git", "branch", "--show-current"],
        "head": ["git", "rev-parse", "HEAD"],
        "status": ["git", "-c", "core.quotepath=false", "status", "--short", "--branch"],
        "porcelain": ["git", "-c", "core.quotepath=false", "status", "--porcelain=v1", "--untracked-files=all"],
    }
    git_results = {name: run(command, root) for name, command in git_commands.items()}
    before_status = git_results["porcelain"]

    lines: list[str] = [
        "G16 AHBot per-entry minimum stock + legendary filtering source probe",
        "READ ONLY: this report did not edit TrinityCore source.",
        f"source_root={root}",
        "confirmed_requirement=MinCopiesPerEntry=10 active independent AHBot auction rows per eligible itemEntry",
        "stack_metric=SUM(item_instance.count) is observation only and does not replace 10 rows",
        "legendary_policy=allow only Misc/Pet and Misc/Mount; reject every other legendary item",
    ]

    add_section(lines, "1. Git facts")
    for name in ("root", "branch", "head", "status"):
        code, output = git_results[name]
        lines.append(f"[{name}] exit={code}")
        lines.extend(output.splitlines() or ["(empty)"])

    add_section(lines, "2. Required file metadata")
    for rel in REQUIRED_FILES:
        data = file_data[rel]
        text = file_text[rel]
        lines.extend(
            (
                f"FILE={rel}",
                f"  bytes={len(data)}",
                f"  sha256={sha256(data)}",
                f"  encoding={file_encoding[rel]}",
                f"  newline={newline_label(data)}",
                f"  raw_braces_open={text.count('{')}",
                f"  raw_braces_close={text.count('}')}",
            )
        )

    add_section(lines, "3. Anchor and conflict-symbol counts")
    combined = "\n".join(file_text.values())
    for pattern in PATTERNS:
        locations: list[str] = []
        total = 0
        for rel in REQUIRED_FILES:
            hits = line_hits(file_text[rel], pattern)
            total += len(hits)
            locations.extend(f"{rel}:{line}" for line in hits)
        lines.append(f"PATTERN={pattern!r} COUNT={total}")
        lines.extend(f"  {location}" for location in locations)
    lines.extend(
        (
            "",
            f"combined_raw_braces_open={combined.count('{')}",
            f"combined_raw_braces_close={combined.count('}')}",
        )
    )

    add_section(lines, "4. Complete numbered source snapshots")
    for rel in REQUIRED_FILES:
        lines.extend(("", "-" * 78, f"BEGIN FILE {rel}", "-" * 78))
        lines.extend(numbered_lines(file_text[rel]))
        lines.extend(("-" * 78, f"END FILE {rel}", "-" * 78))

    after_status = run(git_commands["porcelain"], root)
    add_section(lines, "5. Read-only verification")
    lines.append(f"git_status_command_exit_before={before_status[0]}")
    lines.append(f"git_status_command_exit_after={after_status[0]}")
    lines.append(f"git_status_unchanged={int(before_status == after_status)}")
    if before_status != after_status:
        lines.append("[WARNING] Git status changed while the probe ran. Investigate before patching.")
        lines.append("BEFORE:")
        lines.extend(before_status[1].splitlines() or ["(empty)"])
        lines.append("AFTER:")
        lines.extend(after_status[1].splitlines() or ["(empty)"])
    else:
        lines.append("[OK] Git status is byte-for-byte unchanged.")

    report.parent.mkdir(parents=True, exist_ok=True)
    payload = "\r\n".join(lines) + "\r\n"
    report.write_bytes(b"\xef\xbb\xbf" + payload.encode("utf-8"))
    print(f"[OK] Wrote complete UTF-8-BOM report: {report}")
    print(f"[OK] Source files read: {len(REQUIRED_FILES)}; source edits: 0")
    return 0 if before_status == after_status else 4


if __name__ == "__main__":
    raise SystemExit(main())
