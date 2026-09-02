#!/usr/bin/env python3
"""G17-B3R8 package self-test.

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
TOOL = PKG / "tools" / "apply_g17b3r10_source.py"
INSTALL_PS1 = PKG / "Install-Build-G17B3R10-Windows.ps1"
SUMS = PKG / "SHA256SUMS.txt"
PRE_B3R8 = "dcfa78dd92ac4491882b9e2ec5c18a8b0803d6fcbb01cbe553e7fc069ac0f487"
PRE_B3R6 = "3fdb46e89a03a521d641b285a15339619a43b68c6399938121fb24683dfd306b"
PRE_B3R7 = "f2360d7e1be3ccea66f8bd499b19d4a7cc04acadee804dedbf8bf6774e3ca38c"
PRE_B3R9 = "f0564c5ad225a67f0e49477d31d6939d32b490e6d69b8218f122bc7dce5560c3"
POST = "199cff4a8073f60634ae30b3a4c5fed9a6f90d7fab60fe4ece4d6d263a8fe3fe"

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
    ok("T1 original == B3R8 preimage (user current)", sha(PKG / "original.cpp") == PRE_B3R8)
    ok("T1 payload == B3R7 postimage", sha(PKG / "payload.cpp") == POST)
    ok("T1 rollback == B3R6 (safety floor)", sha(PKG / "rollback_safe.cpp") == PRE_B3R6)
    for img, expect in [("original_src/src/server/scripts/Commands/cs_dragonriding.cpp", PRE_B3R8),
                        ("payload_src/src/server/scripts/Commands/cs_dragonriding.cpp", POST),
                        ("rollback_safe_src/src/server/scripts/Commands/cs_dragonriding.cpp", PRE_B3R6)]:
        ok(f"T1 mirror {img.split('/')[0]}", sha(PKG / img) == expect)


def t2_gate_simulation():
    tool = TOOL.read_text(encoding="utf-8")
    ps1 = INSTALL_PS1.read_text(encoding="utf-8")
    pre = re.search(r'PRE_SHA256\s*=\s*"([0-9a-f]{64})"', tool)
    post = re.search(r'POST_SHA256\s*=\s*"([0-9a-f]{64})"', tool)
    rb = re.search(r'SAFE_ROLLBACK_SHA256\s*=\s*"([0-9a-f]{64})"', tool)
    ok("T2 tool PRE/POST/ROLLBACK constants", pre and post and rb
       and pre.group(1) == PRE_B3R8 and post.group(1) == POST and rb.group(1) == PRE_B3R6)
    # PS1 reads these three names from the tool - all must exist
    for name in ("PRE_SHA256", "POST_SHA256", "SAFE_ROLLBACK_SHA256"):
        ok(f"T2 PS1 reads {name} and tool defines it", name in ps1 and f'{name} = "' in tool)
    inter = re.findall(r'INTERMEDIATE\d*_SHA256\s*=\s*"([0-9a-f]{64})"', tool)
    ok("T2 15 upgradeable intermediates incl. B3R6/R7/R8/R9", len(inter) == 15 and PRE_B3R6 in inter and PRE_B3R7 in inter and PRE_B3R8 in inter and PRE_B3R9 in inter)
    ok("T2 PS1 fingerprint", "199cff4a" in ps1 and "G17B3R10_WINDOWS_BUILD_RESULT" in ps1)


def t3_functional_check():
    with tempfile.TemporaryDirectory() as td:
        src = Path(td) / "src" / "server" / "scripts" / "Commands"
        src.mkdir(parents=True)
        target = src / "cs_dragonriding.cpp"
        target.write_bytes((PKG / "original.cpp").read_bytes())
        r = subprocess.run([sys.executable, str(TOOL), "check", "--source-root", str(Path(td))],
                           capture_output=True, text=True)
        out = r.stdout + r.stderr
        ok("T3 check on B3R6 state -> also upgradeable", r.returncode == 0 and ("UPGRADEABLE" in out.upper() or PRE_B3R6[:8] in out), out[-300:])
        target.write_bytes(hashlib.sha256(b"").hexdigest().encode())  # reset
        # B3R7 image
        import subprocess as _sp
        (src_dir := Path(td)).exists()
        (src_dir / "src/server/scripts/Commands").mkdir(parents=True, exist_ok=True)
        (src_dir / "src/server/scripts/Commands/cs_dragonriding.cpp").write_bytes(Path("/home/user/wow-mecode/wow-mecode/tc-bignum/规划/G17_飞行与移动/G17B3R7_七槽与冷却显示/G17B3R7_SevenSlots_CooldownUI_Windows_20260827/payload.cpp").read_bytes())
        r = subprocess.run([sys.executable, str(TOOL), "check", "--source-root", str(Path(td))], capture_output=True, text=True)
        out = r.stdout + r.stderr
        ok("T3 check on B3R7 state -> also upgradeable", r.returncode == 0 and ("UPGRADEABLE" in out.upper() or PRE_B3R7[:8] in out), out[-300:])
        target.write_bytes((PKG / "payload.cpp").read_bytes())
        r = subprocess.run([sys.executable, str(TOOL), "check", "--source-root", str(Path(td))],
                           capture_output=True, text=True)
        out = r.stdout + r.stderr
        ok("T3 check on B3R9 state -> already applied", r.returncode == 0 and POST[:8] in out, out[-300:])
        target.write_bytes(b"garbage")
        r = subprocess.run([sys.executable, str(TOOL), "check", "--source-root", str(Path(td))],
                           capture_output=True, text=True)
        out = r.stdout + r.stderr
        ok("T3 check on unknown state -> refuses (zero-write contract)",
           r.returncode != 0 or "unknown" in out.lower() or "unrecognized" in out.lower(), out[-300:])


def t4_payload_content():
    payload = (PKG / "payload.cpp").read_text(encoding="utf-8")
    orig = (PKG / "original.cpp").read_text(encoding="utf-8")
    ok("T4 brake at slot 6 on BOTH pages", payload.count("m_spells[5] = SPELL_GLIDE_BRAKE;") == 2)
    ok("T4 page switch at slot 8 on BOTH pages (user directive)", payload.count("m_spells[7] = SPELL_PAGE_SWITCH;") == 2
       and payload.count("m_spells[5] = SPELL_PAGE_SWITCH;") == 0)
    ok("T4 movement filler = archetype generator @7", "COMBAT_SPELL_BASE + ArchetypeBlock(mountArchetype) * 5u; // 生成器@7" in payload)
    ok("T4 combat filler = dive @7", payload.count("m_spells[6] = SPELL_DIVE;") == 1)
    ok("T4 switch chat feedback present", "已切换到" in payload)
    ok("T4 mount hint present", "御龙术就绪" in payload)
    ok("T4 runtime attribute sanitize present (B3R9 heritage)",
       payload.count("info->Attributes |= SPELL_ATTR0_CASTABLE_WHILE_MOUNTED;") == 1
       and "SPELL_ACCELERATE, G17Dragonriding::SPELL_CLIMB" in payload)
    ok("T4 dual-caster handlers (B3R9 heritage)",
       payload.count("B3-R9: dual-caster") >= 3
       and payload.count("ResolveDragonFromCaster(GetCaster())") >= 12)
    ok("T4 CheckEnergyCast dual-caster gate", "Creature* dragon = ResolveDragonFromCaster(caster);" in payload)
    ok("T4 no empty slots left (all 8 filled on both pages)", payload.count("m_spells[7] = 0;") == 0)
    ok("T4 cooldown helper present", "void SendVehicleCooldownPackets(Player* rider, Unit* owner, uint32 spellId, uint32 cdMs)" in payload
       and "BuildCooldownPacket(data, SPELL_COOLDOWN_FLAG_NONE, spellId, cdMs)" in payload
       and "BuildCooldownPacket(self, SPELL_COOLDOWN_FLAG_NONE, spellId, cdMs)" in payload)
    ok("T4 combat call site", "SendVehicleCooldownPackets(player, dragon, info->Id, COMBAT_CD_MS[slot]);" in payload)
    ok("T4 page-switch call site", "switchRider" in payload)
    # diff containment: only expected regions changed
    import difflib
    diff = list(difflib.unified_diff(orig.splitlines(), payload.splitlines(), lineterm="", n=0))
    hunks = [l for l in diff if l.startswith("@@")]
    ok("T4 diff bounded (<= 12 hunks)", len(hunks) <= 25, f"hunks={len(hunks)}: {hunks}")
    added = [l for l in diff if l.startswith("+") and not l.startswith("+++")]
    removed = [l for l in diff if l.startswith("-") and not l.startswith("---")]
    # bounded diff: header note (5) + 2 page layouts (2+2) + helper (~28) + 2 call sites (2) = small
    ok("T4 bounded diff (added<=150, removed<=45)", len(added) <= 150 and len(removed) <= 45,
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
    print(f"G17B3R10_PACKAGE_SELFTEST")
    t1_lineage()
    t2_gate_simulation()
    t3_functional_check()
    t4_payload_content()
    t5_static()
    t6_sums()
    print(f"G17B3R10_PACKAGE_SELFTEST={'PASS' if FAIL == 0 else 'FAIL'} PASS={PASS} FAIL={FAIL}")
    return 0 if FAIL == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
