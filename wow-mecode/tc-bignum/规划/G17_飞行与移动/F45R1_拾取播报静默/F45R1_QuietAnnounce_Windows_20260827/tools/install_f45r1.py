#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""F45R1: silence the AoE-loot announcement at the SOURCE (version-tolerant).

Why: the user added `AoELoot.Announce = 0` to worldserver.conf but the message
kept printing - their compiled CustomAoELoot.cpp predates the config gate (or
ignores it).  F45R1 patches the source directly and handles BOTH variants:

  V1 (gated, like the repo payload 3492...):
      sConfigMgr->GetBoolDefault("AoELoot.Announce", true)
      -> flip the default to false.  The stats line can no longer print, with
         or without any conf entry.  Zero structural change.
  V2 (ungated): a ChatHandler(...).PSendSysMessage("群体拾取F45：发现...")
      call that is NOT inside an if (Announce() ...) gate
      -> the whole call statement is replaced by a comment (paren-matched).

The patch is idempotent, keeps a timestamped backup, and never touches
anything else in the file.  No loot logic is modified - only the announce.

Usage:
  python install_f45r1.py check   <TrinityCore-root>
  python install_f45r1.py apply   <TrinityCore-root> <uploads-dir>
  python install_f45r1.py rollback <TrinityCore-root> <uploads-dir>
  python install_f45r1.py selftest
"""
from __future__ import annotations

import hashlib
import re
import shutil
import sys
import tempfile
from pathlib import Path

TARGET_REL = Path("src/server/game/Custom/CustomAoELoot.cpp")
MSG_KEY = "群体拾取F45：发现"
ANNOUNCE_TRUE = 'sConfigMgr->GetBoolDefault("AoELoot.Announce", true)'
ANNOUNCE_FALSE = 'sConfigMgr->GetBoolDefault("AoELoot.Announce", false)'


def sha(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def neutralize_call(text: str, key_index: int) -> tuple[str, bool]:
    """Replace the ChatHandler(...).PSendSysMessage(...); statement that
    contains text[key_index] with a comment.  The statement end is the first
    ';' at paren-depth 0 (string-literal aware), which correctly spans the
    whole chained call ChatHandler(x).PSendSysMessage(y)."""
    window_start = max(0, key_index - 800)
    window = text[window_start:key_index]
    start = window.rfind("ChatHandler(")
    if start < 0:
        return text, False
    abs_start = window_start + start
    depth = 0
    in_str = False
    escape = False
    i = abs_start
    n = len(text)
    end = -1
    while i < n:
        c = text[i]
        if in_str:
            if escape:
                escape = False
            elif c == "\\":
                escape = True
            elif c == '"':
                in_str = False
        else:
            if c == '"':
                in_str = True
            elif c == "(":
                depth += 1
            elif c == ")":
                depth -= 1
            elif c == ";" and depth <= 0:
                end = i + 1
                break
        i += 1
    if end < 0:
        return text, False
    replacement = "/* F45R1: loot announcement removed */"
    return text[:abs_start] + replacement + text[end:], True


def patch_text(text: str) -> tuple[str, list[str]]:
    changes: list[str] = []

    if ANNOUNCE_TRUE in text:
        text = text.replace(ANNOUNCE_TRUE, ANNOUNCE_FALSE)
        changes.append("V1:announce-default-off")

    # remove ungated message calls (loop-guarded: at most 8 statements)
    removed = 0
    while removed < 8:
        idx = text.find(MSG_KEY)
        if idx < 0:
            break
        window = text[max(0, idx - 800):idx]
        if "Announce()" in window:
            break  # gated call - already silenced by the default flip
        if "ChatHandler(" not in window:
            break  # unknown shape - do not guess (iron rule)
        text, ok = neutralize_call(text, idx)
        if not ok:
            break
        removed += 1
        changes.append("V2:ungated-message-removed")

    return text, changes


def analyze(text: str) -> dict:
    return {
        "has_gated_default_true": ANNOUNCE_TRUE in text,
        "has_gated_default_false": ANNOUNCE_FALSE in text,
        "message_occurrences": text.count(MSG_KEY),
        "announce_gate_near_message": ("Announce()" in text[max(0, text.find(MSG_KEY) - 800):text.find(MSG_KEY)]
                                       if MSG_KEY in text else False),
    }


def main() -> int:
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        return 2
    cmd = args[0]

    if cmd == "selftest":
        gated = (
            'bool Announce()\n{\n    return ' + ANNOUNCE_TRUE + ';\n}\n'
            'void f() { if (Announce() && s) { ChatHandler(p).PSendSysMessage("' + MSG_KEY + ' %u", x); } }\n'
        )
        ungated = 'void f() { ChatHandler(p).PSendSysMessage("' + MSG_KEY + ' %u", x); other(); }\n'
        quiet = 'bool Announce() { return ' + ANNOUNCE_FALSE + '; }\n'
        t1, c1 = patch_text(gated)
        assert c1 == ["V1:announce-default-off"] and ANNOUNCE_FALSE in t1 and MSG_KEY in t1
        t2, c2 = patch_text(ungated)
        assert "V2:ungated-message-removed" in c2 and MSG_KEY not in t2 and "other();" in t2
        t3, c3 = patch_text(quiet)
        assert not c3
        t4, c4 = patch_text(t1)
        assert not c4  # idempotent
        print("F45R1_SELFTEST=PASS")
        return 0

    if cmd not in ("check", "apply", "rollback") or len(args) < 2:
        print(__doc__)
        return 2

    root = Path(args[1])
    target = root / TARGET_REL
    if not target.is_file():
        print(f"F45R1_ERROR=target missing: {target}")
        return 2
    text = target.read_text(encoding="utf-8", errors="surrogateescape")
    print(f"F45R1_TARGET={target}")
    print(f"F45R1_SHA256_BEFORE={sha(text)}")
    info = analyze(text)
    for k, v in info.items():
        print(f"F45R1_{k}={v}")

    if cmd == "check":
        patchable = info["has_gated_default_true"] or info["message_occurrences"] > 0
        state = "ALREADY_QUIET" if (not patchable or info["has_gated_default_false"] and not info["has_gated_default_true"] and info["message_occurrences"] == 0) else ("PATCHABLE" if patchable else "UNKNOWN")
        if info["has_gated_default_false"] and not info["has_gated_default_true"]:
            state = "ALREADY_QUIET"
        print(f"F45R1_CHECK={state}")
        return 0

    uploads = Path(args[2]) if len(args) > 2 else (root.parent / "uploads")
    uploads.mkdir(parents=True, exist_ok=True)

    if cmd == "rollback":
        backups = sorted(uploads.glob("F45R1_Backup_*/CustomAoELoot.cpp"))
        if not backups:
            print("F45R1_ERROR=no backup found; nothing to roll back")
            return 2
        backup = backups[-1]
        data = backup.read_bytes()
        fd, tmp = tempfile.mkstemp(dir=str(target.parent), prefix=".f45r1.")
        Path(tmp).write_bytes(data)
        import os
        os.close(fd)
        os.replace(tmp, target)
        print(f"F45R1_ROLLBACK=PASS from {backup}")
        print(f"F45R1_SHA256_AFTER={sha(target.read_text(encoding='utf-8', errors='surrogateescape'))}")
        return 0

    # apply
    new_text, changes = patch_text(text)
    if not changes:
        print("F45R1_APPLY=NOOP_ALREADY_QUIET")
        return 0
    stamp = Path(tempfile.mkdtemp(prefix="F45R1_Backup_", dir=str(uploads)))
    shutil.copyfile(target, stamp / "CustomAoELoot.cpp")
    (stamp / "SHA256.txt").write_text(sha(text) + "\n", encoding="utf-8")
    fd, tmp = tempfile.mkstemp(dir=str(target.parent), prefix=".f45r1.")
    Path(tmp).write_text(new_text, encoding="utf-8", errors="surrogateescape", newline="")
    import os
    os.close(fd)
    os.replace(tmp, target)
    print(f"F45R1_BACKUP={stamp / 'CustomAoELoot.cpp'}")
    for c in changes:
        print(f"F45R1_CHANGE={c}")
    print(f"F45R1_SHA256_AFTER={sha(new_text)}")
    print("F45R1_APPLY=PASS")
    print("F45R1_NOTE=rebuild worldserver (or install G17-B3R11 which builds) for the quiet to take effect")
    return 0


if __name__ == "__main__":
    sys.exit(main())
