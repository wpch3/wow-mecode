#!/usr/bin/env python3
"""G17-C9 v3 package self-test.

Would have caught EVERY defect of the v1/v2 delivery before it reached the user:

  T1  patcher functional test on synthetic DBCs
      (FRESH -> patch -> COMPLETE, field-by-field verification against the
      Wowhead visual table, layout/string-block preservation, idempotent
      ALREADY_COMPLETE, PARTIAL refusal with no output write)
  T2  INSTALLER GATE SIMULATION: extract every regex literal the PS1 matches
      against the patcher text and evaluate it exactly like PowerShell
      -notmatch would (this is the class of bug that shipped in v1/v2: the
      PS1 grepped G17B3R5_VISUAL_PATCHER_VERSION while the patcher defines
      G17C9_VERSION)
  T3  expected hash constants in the PS1 vs actual package files (mpqcli)
  T4  PS1 static syntax check (ps_static_check.py)
  T5  stale-token scan (no C6/B3R5 leftovers, no .g17c6 tmp suffixes)
  T6  rollback PS1 is a real script with restore logic (v1/v2 shipped 0 bytes)
  T7  SHA256SUMS.txt covers every package file (no missing/extra entries)

Usage: python tools/test_g17c9_package.py   (from the package root)
"""
from __future__ import annotations

import hashlib
import re
import struct
import subprocess
import sys
from pathlib import Path

PKG = Path(__file__).resolve().parents[1]
PATCHER = PKG / "tools" / "patch_g17c9.py"
INSTALL_PS1 = PKG / "Install-G17C9-Real-Visuals.ps1"
ROLLBACK_PS1 = PKG / "Rollback-G17C9-Real-Visuals.ps1"
SUMS = PKG / "SHA256SUMS.txt"

FIELDS = 234
RECSIZE = FIELDS * 4
EFFECT_COL, BASEPTS_COL, TGT_A_COL = 71, 80, 92
VISUAL_COL, RANGE_COL = 131, 46
RECOVERY_COL, CAT_RECOVERY_COL = 29, 30

sys.path.insert(0, str(PKG / "tools"))
import patch_g17c9 as p9  # noqa: E402

PASS = 0
FAIL = 0


def ok(name: str, cond: bool, detail: str = "") -> None:
    global PASS, FAIL
    if cond:
        PASS += 1
        print(f"PASS {name}")
    else:
        FAIL += 1
        print(f"FAIL {name} {detail}")


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def make_dbc(carrier_ids, *, visual=0, effect=3, recovery=5000, category=5000,
             base=17, tgt=0, rng=1, extra_id=12345):
    """Synthetic Spell.dbc-shaped file with the given carrier records."""
    recs = bytearray()
    for sid in carrier_ids:
        rec = bytearray(RECSIZE)
        struct.pack_into("<I", rec, 0, sid)
        struct.pack_into("<I", rec, EFFECT_COL * 4, effect)
        struct.pack_into("<I", rec, BASEPTS_COL * 4, base)
        struct.pack_into("<I", rec, TGT_A_COL * 4, tgt)
        struct.pack_into("<I", rec, VISUAL_COL * 4, visual)
        struct.pack_into("<I", rec, RANGE_COL * 4, rng)
        struct.pack_into("<I", rec, RECOVERY_COL * 4, recovery)
        struct.pack_into("<I", rec, CAT_RECOVERY_COL * 4, category)
        recs += rec
    other = bytearray(RECSIZE)
    struct.pack_into("<I", other, 0, extra_id)
    struct.pack_into("<I", other, VISUAL_COL * 4, 999)
    recs += other
    strings = b"\x00synthetic\x00"
    header = struct.pack("<5I", 0x43424457, len(recs) // RECSIZE, FIELDS, RECSIZE, len(strings))
    return header + bytes(recs) + strings


def field(data: bytes, index: int, col: int) -> int:
    off = 20 + index * RECSIZE + col * 4
    return struct.unpack_from("<I", data, off)[0]


def run_patcher(args, cwd=PKG):
    r = subprocess.run([sys.executable, str(PATCHER), *args],
                       capture_output=True, text=True, cwd=cwd)
    return r.returncode, r.stdout + r.stderr


def t1_patcher_functional():
    ids = list(range(990000, 990025))
    src = PKG / "tools" / "_t1_src.dbc"
    out = PKG / "tools" / "_t1_out.dbc"
    src.write_bytes(make_dbc(ids))
    rc, so = run_patcher(["check", "--input", str(src)])
    ok("T1 check FRESH exit0", rc == 0 and "G17C9_STATE=FRESH" in so, so)
    rc, so = run_patcher(["patch", "--input", str(src), "--output", str(out)])
    ok("T1 patch exit0", rc == 0 and "G17C9_PATCH=PATCHED" in so and "G17C9_PATCHED_RECORDS=25" in so, so)
    data = out.read_bytes()
    srcdata = src.read_bytes()
    ok("T1 layout preserved", len(data) == len(srcdata)
       and struct.unpack_from("<5I", data, 0)[1] == 26
       and data[20 + 26 * RECSIZE:] == srcdata[20 + 26 * RECSIZE:])
    bad = []
    for i, sid in enumerate(ids):
        block, slot = divmod(sid - 990000, 5)
        expect = p9.ARCHETYPE_SLOT_VISUALS[block][slot]
        if field(data, i, VISUAL_COL) != expect: bad.append(f"{sid}:visual={field(data,i,VISUAL_COL)}!={expect}")
        if field(data, i, EFFECT_COL) != 2: bad.append(f"{sid}:effect")
        if field(data, i, BASEPTS_COL) != 0: bad.append(f"{sid}:base")
        if field(data, i, TGT_A_COL) != 18: bad.append(f"{sid}:tgt")
        if field(data, i, RANGE_COL) != 4: bad.append(f"{sid}:range")
        if field(data, i, RECOVERY_COL) != 0: bad.append(f"{sid}:recovery")
        if field(data, i, CAT_RECOVERY_COL) != 0: bad.append(f"{sid}:category")
    ok("T1 field-by-field C9 contract (25x7)", not bad, "; ".join(bad[:5]))
    ok("T1 unrelated record untouched", field(data, 25, 0) == 12345 and field(data, 25, VISUAL_COL) == 999)
    rc, so = run_patcher(["check", "--input", str(out)])
    ok("T1 patched verifies COMPLETE", rc == 0 and "G17C9_STATE=COMPLETE" in so, so)
    rc, so = run_patcher(["patch", "--input", str(out), "--output", str(PKG / "tools" / "_t1_out2.dbc")])
    ok("T1 idempotent ALREADY_COMPLETE", rc == 0 and "G17C9_PATCH=ALREADY_COMPLETE" in so, so)
    ok("T1 idempotent wrote nothing", not (PKG / "tools" / "_t1_out2.dbc").exists())
    # C8-like input (wrong visuals + nonzero cooldowns) must still patch to COMPLETE
    c8 = PKG / "tools" / "_t1_c8.dbc"
    c8.write_bytes(make_dbc(ids, visual=4321, effect=2, recovery=8000, category=8000, base=25, tgt=18, rng=4))
    c8out = PKG / "tools" / "_t1_c8out.dbc"
    rc, so = run_patcher(["patch", "--input", str(c8), "--output", str(c8out)])
    ok("T1 patches from C8-like state", rc == 0 and "G17C9_PATCH=PATCHED" in so, so)
    rc, so = run_patcher(["check", "--input", str(c8out)])
    ok("T1 C8-like result COMPLETE", rc == 0 and "G17C9_STATE=COMPLETE" in so, so)
    # PARTIAL refusal
    part = PKG / "tools" / "_t1_part.dbc"
    part.write_bytes(make_dbc(ids[:12]))
    partout = PKG / "tools" / "_t1_partout.dbc"
    rc, so = run_patcher(["check", "--input", str(part)])
    ok("T1 partial check FAIL exit2", rc == 2 and "G17C9_STATE=PARTIAL" in so, so)
    rc, so = run_patcher(["patch", "--input", str(part), "--output", str(partout)])
    ok("T1 partial patch REFUSED exit2", rc == 2 and "G17C9_PATCH=REFUSED" in so, so)
    ok("T1 partial wrote nothing", not partout.exists())
    for f in src.parent.glob("_t1_*"):
        f.unlink()


def t2_installer_gate_simulation():
    ps1 = INSTALL_PS1.read_text(encoding="utf-8")
    patcher_text = PATCHER.read_text(encoding="utf-8")
    # every  -notmatch '<regex>'  /  -match '<regex>'  literal applied to patcher text
    gate_patterns = re.findall(r"\$patcherText\s+-(?:not)?match\s+'([^']+)'", ps1)
    ok("T2 found >=1 patcher gate", len(gate_patterns) >= 1, str(gate_patterns))
    all_match = True
    detail = []
    for pat in gate_patterns:
        # PowerShell regex ~ .NET; our patterns are plain enough for python re
        if not re.search(pat, patcher_text):
            all_match = False
            detail.append(f"gate regex does NOT match patcher: {pat}")
    ok("T2 every patcher gate regex matches the patcher (v1 bug class)", all_match, "; ".join(detail))
    # the fingerprint in the PS1 must equal the patcher version
    fp = re.search(r'\$BuildFingerprint\s*=\s*"([^"]+)"', ps1)
    ver = re.search(r'G17C9_VERSION\s*=\s*"([^"]+)"', patcher_text)
    ok("T2 PS1 fingerprint == patcher version", fp and ver and fp.group(1) == ver.group(1),
       f"{fp and fp.group(1)} vs {ver and ver.group(1)}")
    # simulate the exact PS1 throw condition
    m = re.search(r'if \(\$patcherText -notmatch \'([^\']+)\'\) \{\s*throw \("OBSOLETE_PACKAGE: ([^"]+)"\)', ps1)
    ok("T2 OBSOLETE gate present", m is not None)
    if m:
        ok("T2 OBSOLETE gate passes on this patcher", re.search(m.group(1), patcher_text) is not None)


def t3_expected_hashes():
    ps1 = INSTALL_PS1.read_text(encoding="utf-8")
    tool = PKG / "third_party" / "mpqcli-windows-amd64.exe"
    exp = re.search(r'\$ExpectedToolHash\s*=\s*"([0-9a-f]{64})"', ps1)
    ok("T3 ExpectedToolHash matches shipped mpqcli", exp and tool.exists()
       and sha(tool.read_bytes()) == exp.group(1))
    exparea = re.search(r'\$ExpectedAreaHash\s*=\s*"([0-9a-f]{64})"', ps1)
    ok("T3 ExpectedAreaHash wellformed", exparea is not None)
    # the old broken gates must be gone
    ok("T3 no hardcoded C3-image input gate",
       "006a892b" not in ps1, "input still pinned to the C3 image")
    ok("T3 no hardcoded C6-image output gate",
       "5db5b7a5" not in ps1, "output still pinned to the C6 image")
    ok("T3 no root-MPQ C3-state hash pinning",
       "no longer matches the C3 state" not in ps1)


def t4_static_syntax():
    r = subprocess.run([sys.executable, str(PKG / "tools" / "ps_static_check.py"),
                        str(INSTALL_PS1), str(ROLLBACK_PS1)],
                       capture_output=True, text=True)
    ok("T4 PS1 static syntax", r.returncode == 0 and "PS_STATIC_OK" in r.stdout, r.stdout + r.stderr)


def t5_stale_tokens():
    for f in (INSTALL_PS1, ROLLBACK_PS1):
        t = f.read_text(encoding="utf-8")
        ok(f"T5 no stale tokens in {f.name}",
           "G17B3R5_VISUAL_PATCHER_VERSION" not in t and ".g17c6" not in t
           and "C6_BUILD=" not in t and "C6_PATCHER_VERSION_CHECK" not in t
           and "must be the C3 image" not in t)


def t6_rollback_real():
    t = ROLLBACK_PS1.read_text(encoding="utf-8")
    ok("T6 rollback non-empty and restores", len(t) > 2000
       and "before_G17C9_root.MPQ" in t and "before_G17C9_locale.MPQ" in t
       and "G17C9_Client_Backup_" in t and "CLIENT_CACHE_REMOVED" in t
       and "G17C9_ROLLBACK_RESULT=PASS" in t)


def t7_sums_complete():
    if not SUMS.exists():
        ok("T7 SHA256SUMS exists", False)
        return
    listed = {}
    for line in SUMS.read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        h, name = line.split(None, 1)
        listed[name.strip()] = h
    actual = {}
    for f in PKG.rglob("*"):
        if not f.is_file():
            continue
        rel = f.relative_to(PKG).as_posix()
        if rel == "SHA256SUMS.txt" or "__pycache__" in rel or rel.endswith(".pyc"):
            continue
        actual[rel] = sha(f.read_bytes())
    missing = [r for r in actual if r not in listed]
    extra = [r for r in listed if r not in actual]
    bad = [r for r in actual if r in listed and listed[r] != actual[r]]
    ok("T7 SHA256SUMS complete and correct", not missing and not extra and not bad,
       f"missing={missing[:3]} extra={extra[:3]} bad={bad[:3]}")


def main() -> int:
    print(f"G17C9_PACKAGE_SELFTEST package={PKG.name}")
    t1_patcher_functional()
    t2_installer_gate_simulation()
    t3_expected_hashes()
    t4_static_syntax()
    t5_stale_tokens()
    t6_rollback_real()
    t7_sums_complete()
    print(f"G17C9_PACKAGE_SELFTEST={'PASS' if FAIL == 0 else 'FAIL'} PASS={PASS} FAIL={FAIL}")
    return 0 if FAIL == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
