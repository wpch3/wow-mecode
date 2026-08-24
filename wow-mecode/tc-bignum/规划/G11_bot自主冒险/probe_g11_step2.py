#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""G11 第2步装机前只读探针。

用法：
  python probe_g11_step2.py D:\\TrinityCore
  python probe_g11_step2.py D:\\TrinityCore D:\\probe_g11_step2.txt

只读取源码和 Git 状态，不修改任何源码文件。
"""

from __future__ import annotations

import hashlib
import re
import subprocess
import sys
from pathlib import Path


DEFAULT_ROOT = Path(r"D:\TrinityCore")


def read_source(path: Path) -> tuple[str, str]:
    raw = path.read_bytes()
    if raw.startswith(b"\xef\xbb\xbf"):
        return raw.decode("utf-8-sig"), "utf-8-sig"
    for encoding in ("utf-8", "gb18030"):
        try:
            return raw.decode(encoding), encoding
        except UnicodeDecodeError:
            continue
    return raw.decode("latin1"), "latin1-fallback"


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def run_git(root: Path, *args: str) -> str:
    try:
        result = subprocess.run(
            ["git", *args],
            cwd=root,
            text=True,
            encoding="utf-8",
            errors="replace",
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=20,
            check=False,
        )
        return result.stdout.rstrip()
    except Exception as exc:  # pragma: no cover - diagnostic fallback
        return f"[git command failed] {exc}"


def context(lines: list[str], index: int, before: int = 8, after: int = 16) -> str:
    start = max(0, index - before)
    end = min(len(lines), index + after + 1)
    return "\n".join(f"{n + 1:6d}: {lines[n]}" for n in range(start, end))


def find_sections(
    title: str,
    rel: str,
    text: str,
    patterns: list[tuple[str, str]],
    before: int = 8,
    after: int = 16,
) -> str:
    lines = text.splitlines()
    out = [f"\n===== {title} :: {rel} ====="]
    for label, pattern in patterns:
        rx = re.compile(pattern)
        matches = [i for i, line in enumerate(lines) if rx.search(line)]
        out.append(f"\n--- {label} / matches={len(matches)} ---")
        if not matches:
            out.append("[NOT FOUND]")
            continue
        for pos, index in enumerate(matches[:8], 1):
            out.append(f"[match {pos} at line {index + 1}]")
            out.append(context(lines, index, before, after))
        if len(matches) > 8:
            out.append(f"[only first 8 of {len(matches)} matches shown]")
    return "\n".join(out)


def symbol_counts(rel: str, text: str, symbols: list[str]) -> str:
    out = [f"\n===== SYMBOL COUNTS :: {rel} ====="]
    for symbol in symbols:
        out.append(f"{symbol}\t{text.count(symbol)}")
    return "\n".join(out)


def main() -> int:
    root = Path(sys.argv[1]) if len(sys.argv) >= 2 else DEFAULT_ROOT
    output = Path(sys.argv[2]) if len(sys.argv) >= 3 else root / "probe_g11_step2.txt"

    rel_paths = {
        "bot_ai.h": "src/server/game/AI/NpcBots/bot_ai.h",
        "bot_ai.cpp": "src/server/game/AI/NpcBots/bot_ai.cpp",
        "botconfig.h": "src/server/game/AI/NpcBots/botconfig.h",
        "botconfig.cpp": "src/server/game/AI/NpcBots/botconfig.cpp",
        "Object.h": "src/server/game/Entities/Object/Object.h",
        "Object.cpp": "src/server/game/Entities/Object/Object.cpp",
        "WorldSession.h": "src/server/game/Server/WorldSession.h",
        "Group.h": "src/server/game/Groups/Group.h",
    }

    missing = [rel for rel in rel_paths.values() if not (root / rel).is_file()]
    if missing:
        print("[FAILED] 以下文件不存在；请确认源码根目录：", file=sys.stderr)
        for rel in missing:
            print(f"  {root / rel}", file=sys.stderr)
        return 2

    loaded: dict[str, tuple[str, str]] = {}
    for key, rel in rel_paths.items():
        loaded[key] = read_source(root / rel)

    report: list[str] = []
    report.append("G11 STEP2 PERCEPTION PRE-INSTALL PROBE")
    report.append(f"root={root}")
    report.append("readonly=1")
    report.append("")
    report.append("===== GIT STATE =====")
    report.append("-- rev-parse --")
    report.append(run_git(root, "rev-parse", "--show-toplevel"))
    report.append(run_git(root, "rev-parse", "--abbrev-ref", "HEAD"))
    report.append(run_git(root, "rev-parse", "HEAD"))
    report.append("-- status --")
    report.append(run_git(root, "status", "--short", "--branch"))

    report.append("\n===== FILE META =====")
    for key, rel in rel_paths.items():
        path = root / rel
        _, encoding = loaded[key]
        report.append(
            f"{rel}\tbytes={path.stat().st_size}\tencoding={encoding}\tsha256={sha256(path)}"
        )

    bot_h = loaded["bot_ai.h"][0]
    bot_cpp = loaded["bot_ai.cpp"][0]
    cfg_h = loaded["botconfig.h"][0]
    cfg_cpp = loaded["botconfig.cpp"][0]
    obj_h = loaded["Object.h"][0]
    obj_cpp = loaded["Object.cpp"][0]
    ws_h = loaded["WorldSession.h"][0]
    group_h = loaded["Group.h"][0]

    report.append(
        find_sections(
            "bot_ai.h anchors",
            rel_paths["bot_ai.h"],
            bot_h,
            [
                ("class and constructor", r"class\s+bot_ai|explicit\s+bot_ai|bot_ai\s*\("),
                ("GlobalUpdate declaration", r"\bGlobalUpdate\s*\("),
                ("CommonTimers declaration", r"\bCommonTimers\s*\("),
                ("GetCurrentScene from G19", r"\bGetCurrentScene\s*\("),
                ("SetWanderer anchor", r"\bSetWanderer\s*\("),
                ("timer/member neighborhood", r"_updateTimerLong|_updateTimerMedium|_careChatTimer|_textCooldowns"),
                ("access labels", r"^\s*(public|protected|private)\s*:"),
            ],
            before=10,
            after=20,
        )
    )

    report.append(
        find_sections(
            "bot_ai.cpp anchors",
            rel_paths["bot_ai.cpp"],
            bot_cpp,
            [
                ("includes", r"^#include"),
                ("constructor definitions", r"^bot_ai::bot_ai\s*\("),
                ("GlobalUpdate definition", r"^bool\s+bot_ai::GlobalUpdate\s*\("),
                ("FindMaster call", r"\bFindMaster\s*\(\s*\)"),
                ("teleport guard", r"IsDuringTeleport\s*\(\s*\)"),
                ("alive/world guards", r"if\s*\(\s*!me->IsAlive\s*\(\s*\)\s*\)|if\s*\(\s*!me->IsInWorld\s*\(\s*\)\s*\)"),
                ("CommonTimers definition", r"^void\s+bot_ai::CommonTimers\s*\("),
                ("existing player grid scans", r"PlayerListSearcher|GetPlayerListInGrid|AnyPlayerIn.*RangeCheck"),
                ("G19 scene definition/calls", r"\bGetCurrentScene\s*\("),
                ("existing autonomy/social symbols", r"Perception|perception|Autonomy|autonomy|NearbyPlayer|nearbyPlayer"),
            ],
            before=10,
            after=24,
        )
    )

    report.append(
        find_sections(
            "botconfig.h anchors",
            rel_paths["botconfig.h"],
            cfg_h,
            [
                ("class start", r"class\s+BotCfg"),
                ("companion custom accessors", r"Companion|companion"),
                ("wanderer accessors", r"Wander|wander"),
                ("private static members", r"^\s*static\s+.*_"),
                ("access labels", r"^\s*(public|protected|private)\s*:"),
            ],
            before=8,
            after=18,
        )
    )

    report.append(
        find_sections(
            "botconfig.cpp anchors",
            rel_paths["botconfig.cpp"],
            cfg_cpp,
            [
                ("config load function", r"_loadConfig\s*\("),
                ("companion custom config", r"Companion|companion"),
                ("wandering config", r"NpcBot\.WanderingBots"),
                ("static member definitions", r"^\s*(bool|uint\d+|int\d+|float|std::).*BotCfg::_"),
                ("accessor definitions", r"^\w.*BotCfg::(Is|Get|Enable)"),
            ],
            before=8,
            after=18,
        )
    )

    report.append(
        find_sections(
            "upstream player search API",
            rel_paths["Object.h"],
            obj_h,
            [("GetPlayerListInGrid declaration", r"GetPlayerListInGrid\s*\(")],
            before=8,
            after=12,
        )
    )
    report.append(
        find_sections(
            "upstream player search implementation",
            rel_paths["Object.cpp"],
            obj_cpp,
            [("GetPlayerListInGrid definition", r"WorldObject::GetPlayerListInGrid\s*\(")],
            before=8,
            after=16,
        )
    )
    report.append(
        find_sections(
            "session classification API",
            rel_paths["WorldSession.h"],
            ws_h,
            [
                ("PlayerDisconnected", r"PlayerDisconnected\s*\("),
                ("custom IsBotSession", r"IsBotSession\s*\("),
            ],
            before=8,
            after=12,
        )
    )
    report.append(
        find_sections(
            "group facts API",
            rel_paths["Group.h"],
            group_h,
            [
                ("AddMember Creature", r"AddMember\s*\(\s*Creature\s*\*"),
                ("AddInvite Player", r"AddInvite\s*\(\s*Player\s*\*"),
            ],
            before=8,
            after=12,
        )
    )

    symbols = [
        "GetCurrentScene",
        "GlobalUpdate",
        "CommonTimers",
        "GetPlayerListInGrid",
        "PlayerListSearcher",
        "IsWanderer()",
        "IAmFree()",
        "IsDuringTeleport()",
        "Perception",
        "perception",
        "NpcBot.Autonomy.Perception",
    ]
    report.append(symbol_counts(rel_paths["bot_ai.h"], bot_h, symbols))
    report.append(symbol_counts(rel_paths["bot_ai.cpp"], bot_cpp, symbols))
    report.append(symbol_counts(rel_paths["botconfig.h"], cfg_h, symbols))
    report.append(symbol_counts(rel_paths["botconfig.cpp"], cfg_cpp, symbols))

    report.append("\n===== BRACE COUNTS (diagnostic only, not a syntax proof) =====")
    for key in ("bot_ai.h", "bot_ai.cpp", "botconfig.h", "botconfig.cpp"):
        text = loaded[key][0]
        report.append(
            f"{rel_paths[key]}\topen={text.count('{')}\tclose={text.count('}')}\tdelta={text.count('{') - text.count('}')}"
        )

    report.append("\n===== END =====")
    report.append("请把本文件完整回传；不要只截最后几十行。")

    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w", encoding="utf-8-sig", newline="\r\n") as f:
        f.write("\n".join(report) + "\n")
    print(f"[OK] 只读探针完成：{output}")
    print(f"[OK] 报告字节数：{output.stat().st_size}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
