#!/usr/bin/env python3
"""G17-C13 package self-test.

T1  payload FrameXML passthrough (C12 lua + C11 xml unchanged)
T2  patcher visual table: the two user-confirmed IDs kept; all impact-class
T3  patcher functional: synthetic chain DBC -> FRESH -> patch -> COMPLETE
    -> idempotent; appended-record fields verified (target/visual/attr)
T4  installer gate simulation: PS1 version gate + state-file paths
T5  PS1 static syntax
T6  SHA256SUMS complete
"""
from __future__ import annotations

import hashlib
import re
import struct
import subprocess
import sys
from pathlib import Path

PKG = Path(__file__).resolve().parents[1]
PATCHER = PKG / "tools" / "patch_g17c13.py"
INSTALL_PS1 = PKG / "Install-G17C13-ClientDBC.ps1"
SUMS = PKG / "SHA256SUMS.txt"
LUA = PKG / "payload" / "Interface" / "FrameXML" / "VehicleMenuBar.lua"
XML = PKG / "payload" / "Interface" / "FrameXML" / "VehicleMenuBar.xml"
C12 = Path("/home/user/wow-mecode/wow-mecode/tc-bignum/规划/G17_飞行与移动/G17C12_客户端UI框适配/G17C12_ClientMod_BarFits_Windows_20260827")
C11 = Path("/home/user/wow-mecode/wow-mecode/tc-bignum/规划/G17_飞行与移动/G17C11_客户端UI魔改_载具条/G17C11_ClientMod_VehicleBar_Windows_20260827")

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


def t1_passthrough():
    ok("T1 lua == installed C12 image", sha(LUA) == sha(C12 / "payload/Interface/FrameXML/VehicleMenuBar.lua"))
    ok("T1 xml == installed C11 image", sha(XML) == sha(C11 / "payload/Interface/FrameXML/VehicleMenuBar.xml"))


def t2_visual_table():
    src = PATCHER.read_text(encoding="utf-8")
    ok("T2 user-confirmed 7860 kept (dragon slot0)", "7860" in src)
    ok("T2 user-confirmed 4961 kept (dragon slot3)", "4961" in src)
    # every combat visual must be a known impact kit
    impact = {"7860","143","222","4961","2253","3860","36","7732","3057","67","9152","13","173","107","7479","46","64"}
    m = re.search(r"COMBAT_VISUALS = \{(.*?)\}", src, re.S)
    # only the visual lists on value lines (start with whitespace + digits)
    ids = set()
    for line in m.group(1).splitlines():
        vals = re.findall(r"\b(\d+)\b", line)
        if vals and line.strip().startswith("[") is False and ":" in line:
            ids.update(v for v in vals if v not in ("0","1","2","3","4"))
    ok("T2 all combat visuals are impact-class IDs", ids <= impact, str(ids - impact))
    m = re.search(r"FLIGHT_VISUALS = \{(.*?)\}", src)
    fids = dict(re.findall(r"(\d+):\s*(\d+)", m.group(1)))
    ok("T2 flight visuals set for all four carriers",
       set(fids) == {"990025", "990026", "990027", "990028"})


def t3_functional():
    FIELDS, RECSIZE = 234, 234 * 4
    recs = b""
    for sid in list(range(1, 11)) + list(range(990000, 990025)) + [990025, 990026, 990027, 990028]:
        rec = bytearray(RECSIZE)
        struct.pack_into("<I", rec, 0, sid)
        struct.pack_into("<I", rec, 71 * 4, 3)
        recs += bytes(rec)
    strings = b"filler\x00"
    data = struct.pack("<5I", 0x43424457, 39, FIELDS, RECSIZE, len(strings)) + recs + strings
    inp = Path("/tmp/t3_in.dbc"); inp.write_bytes(data)
    r = subprocess.run([sys.executable, str(PATCHER), "check", "--input", str(inp)], capture_output=True, text=True)
    ok("T3 chain state -> FRESH", "G17C13_STATE=FRESH" in r.stdout, r.stdout)
    outp = Path("/tmp/t3_out.dbc")
    r = subprocess.run([sys.executable, str(PATCHER), "patch", "--input", str(inp), "--output", str(outp)], capture_output=True, text=True)
    ok("T3 patch appends 2", "G17C13_APPENDED_RECORDS=2" in r.stdout and "G17C13_PATCH=PATCHED" in r.stdout, r.stdout)
    blob = outp.read_bytes()
    magic, count, fields, recsize, strsz = struct.unpack_from("<5I", blob, 0)
    ok("T3 output layout (41 records)", count == 41 and fields == FIELDS and len(blob) == 20 + count * RECSIZE + strsz)
    r = subprocess.run([sys.executable, str(PATCHER), "check", "--input", str(outp)], capture_output=True, text=True)
    ok("T3 patched verifies COMPLETE", "G17C13_STATE=COMPLETE" in r.stdout, r.stdout)
    r = subprocess.run([sys.executable, str(PATCHER), "patch", "--input", str(outp), "--output", "/tmp/t3_out2.dbc"], capture_output=True, text=True)
    ok("T3 idempotent", "ALREADY_COMPLETE" in r.stdout)
    # field spot-checks
    orecs = blob[20:20 + count * RECSIZE]
    found = {}
    for i in range(count):
        sid = struct.unpack_from("<I", orecs, i * RECSIZE)[0]
        found[sid] = i
    def f(sid, col):
        return struct.unpack_from("<I", orecs, found[sid] * RECSIZE + col * 4)[0]
    ok("T3 990029 enemy-target + War Stomp visual", f(990029, 92) == 18 and f(990029, 131) == 2355 and f(990029, 71) == 2)
    ok("T3 990030 self-target + Cheetah visual", f(990030, 92) == 1 and f(990030, 131) == 3719 and f(990030, 4) & 0x100)
    ok("T3 dragon slot visuals (7860/4961 kept)", f(990000, 131) == 7860 and f(990003, 131) == 4961)
    ok("T3 flight carrier visuals set", f(990026, 131) == 9959 and f(990028, 131) == 63)
    ok("T3 recovery time zeroed", f(990000, 29) == 0 and f(990000, 30) == 0)


def t4_gate_simulation():
    ps1 = INSTALL_PS1.read_text(encoding="utf-8")
    patcher = PATCHER.read_text(encoding="utf-8")
    gates = re.findall(r"\$patcherText\s+-(?:not)?match\s+'([^']+)'", ps1)
    ok("T4 found the patcher version gate", len(gates) == 1, str(gates))
    if gates:
        ok("T4 version gate matches the real patcher", re.search(gates[0], patcher) is not None)
    ok("T4 PS1 packs patched Spell + passthrough lua/xml/area",
       all(k in ps1 for k in ("$GeneratedSpell", "PackSpell", "PackLua", "PackXml", "PackArea")))
    ok("T4 round-trip verifies patched hash", "$PatchedSpellHash" in ps1 and "built archive Spell.dbc mismatch" in ps1)


def t5_static():
    r = subprocess.run([sys.executable, str(PKG / "tools" / "ps_static_check.py"),
                        str(INSTALL_PS1), str(PKG / "Rollback-G17C13-ClientDBC.ps1")],
                       capture_output=True, text=True)
    ok("T5 PS1 static syntax", r.returncode == 0 and r.stdout.count("PS_STATIC_OK") == 2, r.stdout + r.stderr)


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
    print(f"G17C13_PACKAGE_SELFTEST")
    t1_passthrough()
    t2_visual_table()
    t3_functional()
    t4_gate_simulation()
    t5_static()
    t6_sums()
    print(f"G17C13_PACKAGE_SELFTEST={'PASS' if FAIL == 0 else 'FAIL'} PASS={PASS} FAIL={FAIL}")
    return 0 if FAIL == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
