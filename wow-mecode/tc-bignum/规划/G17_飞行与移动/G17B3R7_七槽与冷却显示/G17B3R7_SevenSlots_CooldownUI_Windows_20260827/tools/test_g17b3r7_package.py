#!/usr/bin/env python3
"""G17-B3R7 package self-test.

T1  lineage image contract (original/payload/rollback mirrors + SHA constants)
T2  installer gate simulation: PS1 Read-ToolHash names vs tool constants;
    UPGRADEABLE_SHAS covers every historical image including B3R6 final
T3  functional: `apply check` against a synthetic target at the B3R6 state
    reports the correct lineage state (no writes)
T4  payload content: all 5 changes present, and the diff vs the B3R6 original
    contains ONLY the expected hunks
T5  PS1 static syntax check
T6  SHA256SUMS complete and correct
"""
from __future__ import annotations

import hashlib
import re
import subprocess
import sys
import tempfile
from pathlib import Path

PKG = Path(__file__).resolve().parents[1]
TOOL = PKG / "tools" / "apply_g17b3r7_source.py"
INSTALL_PS1 = PKG / "Install-Build-G17B3R7-Windows.ps1"
SUMS = PKG / "SHA256SUMS.txt"
PRE = "3fdb46e89a03a521d641b285a15339619a43b68c6399938121fb24683dfd306b"
POST = "f2360d7e1be3ccea66f8bd499b19d4a7cc04acadee804dedbf8bf6774e3ca38c"

PASS = 0
FAIL = 0


def ok(name, cond, detail=""):
    global PASS, FAIL
    if cond:
        PASS += 1
        print(f"PASS {name}")
    else:
        FAIL += 1
        print(f"FAIL {name} {detail}")


def sha(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


def t1_lineage():
    ok("T1 original == B3R6 preimage", sha(PKG / "original.cpp") == PRE)
    ok("T1 payload == B3R7 postimage", sha(PKG / "payload.cpp") == POST)
    ok("T1 rollback == B3R6 (safety floor)", sha(PKG / "rollback_safe.cpp") == PRE)
    for img, expect in [("original_src/src/server/scripts/Commands/cs_dragonriding.cpp", PRE),
                        ("payload_src/src/server/scripts/Commands/cs_dragonriding.cpp", POST),
                        ("rollback_safe_src/src/server/scripts/Commands/cs_dragonriding.cpp", PRE)]:
        ok(f"T1 mirror {img.split('/')[0]}", sha(PKG / img) == expect)


def t2_gate_simulation():
    tool = TOOL.read_text(encoding="utf-8")
    ps1 = INSTALL_PS1.read_text(encoding="utf-8")
    pre = re.search(r'PRE_SHA256\s*=\s*"([0-9a-f]{64})"', tool)
    post = re.search(r'POST_SHA256\s*=\s*"([0-9a-f]{64})"', tool)
    rb = re.search(r'SAFE_ROLLBACK_SHA256\s*=\s*"([0-9a-f]{64})"', tool)
    ok("T2 tool PRE/POST/ROLLBACK constants", pre and post and rb
       and pre.group(1) == PRE and post.group(1) == POST and rb.group(1) == PRE)
    # PS1 reads these three names from the tool - all must exist
    for name in ("PRE_SHA256", "POST_SHA256", "SAFE_ROLLBACK_SHA256"):
        ok(f"T2 PS1 reads {name} and tool defines it", name in ps1 and f'{name} = "' in tool)
    inter = re.findall(r'INTERMEDIATE\d*_SHA256\s*=\s*"([0-9a-f]{64})"', tool)
    ok("T2 12 upgradeable intermediates incl. B3R6 final", len(inter) == 12 and PRE in inter)
    ok("T2 PS1 fingerprint", "f2360d7e" in ps1 and "G17B3R7_WINDOWS_BUILD_RESULT" in ps1)


def t3_functional_check():
    with tempfile.TemporaryDirectory() as td:
        src = Path(td) / "src" / "server" / "scripts" / "Commands"
        src.mkdir(parents=True)
        target = src / "cs_dragonriding.cpp"
        target.write_bytes((PKG / "original.cpp").read_bytes())
        r = subprocess.run([sys.executable, str(TOOL), "check", "--source-root", str(Path(td))],
                           capture_output=True, text=True)
        out = r.stdout + r.stderr
        ok("T3 check on B3R6 state -> upgradeable/ready", r.returncode == 0 and PRE[:8] in out, out[-300:])
        target.write_bytes((PKG / "payload.cpp").read_bytes())
        r = subprocess.run([sys.executable, str(TOOL), "check", "--source-root", str(Path(td))],
                           capture_output=True, text=True)
        out = r.stdout + r.stderr
        ok("T3 check on B3R7 state -> already applied", r.returncode == 0 and POST[:8] in out, out[-300:])
        target.write_bytes(b"garbage")
        r = subprocess.run([sys.executable, str(TOOL), "check", "--source-root", str(Path(td))],
                           capture_output=True, text=True)
        out = r.stdout + r.stderr
        ok("T3 check on unknown state -> refuses (zero-write contract)",
           r.returncode != 0 or "unknown" in out.lower() or "unrecognized" in out.lower(), out[-300:])


def t4_payload_content():
    payload = (PKG / "payload.cpp").read_text(encoding="utf-8")
    orig = (PKG / "original.cpp").read_text(encoding="utf-8")
    ok("T4 movement page 7 slots", "dragon->m_spells[5] = SPELL_GLIDE_BRAKE;" in payload
       and payload.count("SPELL_GLIDE_BRAKE;") == 2)
    ok("T4 page switch last on both pages", payload.count("m_spells[6] = SPELL_PAGE_SWITCH;") == 2)
    ok("T4 slot7 stays empty", payload.count("m_spells[7] = 0;") == 2)
    ok("T4 cooldown helper present", "void SendVehicleCooldownPackets(Player* rider, Unit* owner, uint32 spellId, uint32 cdMs)" in payload
       and "BuildCooldownPacket(data, SPELL_COOLDOWN_FLAG_NONE, spellId, cdMs)" in payload
       and "BuildCooldownPacket(self, SPELL_COOLDOWN_FLAG_NONE, spellId, cdMs)" in payload)
    ok("T4 combat call site", "SendVehicleCooldownPackets(player, dragon, info->Id, COMBAT_CD_MS[slot]);" in payload)
    ok("T4 page-switch call site", "switchRider" in payload)
    # diff containment: only expected regions changed
    import difflib
    diff = list(difflib.unified_diff(orig.splitlines(), payload.splitlines(), lineterm="", n=0))
    hunks = [l for l in diff if l.startswith("@@")]
    ok("T4 diff has exactly 6 hunks (header + 5 changes)", len(hunks) == 6, f"hunks={len(hunks)}: {hunks}")
    added = [l for l in diff if l.startswith("+") and not l.startswith("+++")]
    removed = [l for l in diff if l.startswith("-") and not l.startswith("---")]
    # bounded diff: header note (5) + 2 page layouts (2+2) + helper (~28) + 2 call sites (2) = small
    ok("T4 bounded diff (added<=60, removed<=8)", len(added) <= 60 and len(removed) <= 8,
       f"added={len(added)} removed={len(removed)}")


def t5_static():
    r = subprocess.run([sys.executable, str(PKG / "tools" / "ps_static_check.py"), str(INSTALL_PS1)],
                       capture_output=True, text=True)
    ok("T5 PS1 static syntax", r.returncode == 0 and "PS_STATIC_OK" in r.stdout, r.stdout + r.stderr)


def t6_sums():
    if not SUMS.exists():
        ok("T6 SHA256SUMS exists", False)
        return
    listed = {}
    for line in SUMS.read_text(encoding="utf-8").splitlines():
        if line.strip():
            h, name = line.split(None, 1)
            listed[name.strip()] = h
    actual = {}
    for f in PKG.rglob("*"):
        if not f.is_file():
            continue
        rel = f.relative_to(PKG).as_posix()
        if rel == "SHA256SUMS.txt" or "__pycache__" in rel or rel.endswith(".pyc"):
            continue
        actual[rel] = sha(f)
    missing = [r for r in actual if r not in listed]
    extra = [r for r in listed if r not in actual]
    bad = [r for r in actual if r in listed and listed[r] != actual[r]]
    ok("T6 SHA256SUMS complete and correct", not missing and not extra and not bad,
       f"missing={missing[:3]} extra={extra[:3]} bad={bad[:3]}")


def main() -> int:
    print(f"G17B3R7_PACKAGE_SELFTEST")
    t1_lineage()
    t2_gate_simulation()
    t3_functional_check()
    t4_payload_content()
    t5_static()
    t6_sums()
    print(f"G17B3R7_PACKAGE_SELFTEST={'PASS' if FAIL == 0 else 'FAIL'} PASS={PASS} FAIL={FAIL}")
    return 0 if FAIL == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
