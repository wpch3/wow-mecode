#!/usr/bin/env python3
"""G17-C11 package self-test (client mod - 8-slot vehicle bar).

T1  payload hash contract (patched files + recorded baselines)
T2  payload content: constant=8 once, buttons 7/8 once each, anchor chain correct
T3  diff contract vs the G17_extracted originals: ONLY the two intended hunks;
    LF line endings preserved (no CR introduced)
T4  XML well-formed
T5  installer gate simulation: the PS1's version regex and expected payload
    hashes match the real files (the C9-v1 OBSOLETE_PACKAGE lesson)
T6  PS1 static syntax check
T7  SHA256SUMS complete and correct
"""
from __future__ import annotations

import difflib
import hashlib
import re
import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

PKG = Path(__file__).resolve().parents[1]
PATCHER = PKG / "tools" / "patch_g17c11.py"
INSTALL_PS1 = PKG / "Install-G17C11-ClientMod.ps1"
SUMS = PKG / "SHA256SUMS.txt"
ORIG = Path("/home/user/wow-mecode/G17_extracted/Interface/FrameXML")

PAYLOAD_LUA = PKG / "payload" / "Interface" / "FrameXML" / "VehicleMenuBar.lua"
PAYLOAD_XML = PKG / "payload" / "Interface" / "FrameXML" / "VehicleMenuBar.xml"
LUA_SHA = "0d572a7fbb7d69005b02f22932fbdc8f64c06f0d1d4ddc52a048300f30bb1a3c"
XML_SHA = "31563ecf8787054408bab4049f212b2b77aa56c12dd264d60aa03b1494f1e628"
BASE_LUA_SHA = "2183cb19190a49c5207624c44e756110764c18734c086ad41a8e3eda422a7a34"
BASE_XML_SHA = "fff0aec7e40f564b04114c0794be9152e0c546082d9a38ed6e6853e0903bc4df"

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


def t1_hashes():
    ok("T1 payload lua hash", sha(PAYLOAD_LUA) == LUA_SHA)
    ok("T1 payload xml hash", sha(PAYLOAD_XML) == XML_SHA)
    ok("T1 baseline originals unchanged (extracted source of truth)",
       sha(ORIG / "VehicleMenuBar.lua") == BASE_LUA_SHA
       and sha(ORIG / "VehicleMenuBar.xml") == BASE_XML_SHA)


def t2_content():
    lua = PAYLOAD_LUA.read_text(encoding="utf-8")
    xml = PAYLOAD_XML.read_text(encoding="utf-8")
    ok("T2 constant = 8 exactly once", lua.count("VEHICLE_MAX_ACTIONBUTTONS = 8;") == 1
       and lua.count("VEHICLE_MAX_ACTIONBUTTONS = 6;") == 0)
    ok("T2 button 7 defined once", xml.count('name="VehicleMenuBarActionButton7"') == 1)
    ok("T2 button 8 defined once", xml.count('name="VehicleMenuBarActionButton8"') == 1)
    ok("T2 button 7 anchored to button 6",
       'relativeTo="VehicleMenuBarActionButton6" relativePoint="RIGHT"' in xml
       and 'id="7"' in xml)
    ok("T2 button 8 anchored to button 7", 'relativeTo="VehicleMenuBarActionButton7"' in xml)
    ok("T2 both inherit the vehicle template",
       xml.count('inherits="VehicleActionButtonTemplate" id="7"') == 1
       and xml.count('inherits="VehicleActionButtonTemplate" id="8"') == 1)
    ok("T2 no stray button 9/10", 'name="VehicleMenuBarActionButton9"' not in xml
       and 'name="VehicleMenuBarActionButton10"' not in xml)


def t3_diff_contract():
    lua_old = (ORIG / "VehicleMenuBar.lua").read_text(encoding="utf-8")
    lua_new = PAYLOAD_LUA.read_text(encoding="utf-8")
    ld = [l for l in difflib.unified_diff(lua_old.splitlines(), lua_new.splitlines(), lineterm="", n=0)
          if (l.startswith("+") and not l.startswith("+++")) or (l.startswith("-") and not l.startswith("---"))]
    ok("T3 lua diff is exactly 3 lines (-6 / +8 / +comment)", len(ld) == 3
       and ld[0].strip() == "-VEHICLE_MAX_ACTIONBUTTONS = 6;" and "8;" in ld[1], str(ld))

    xml_old = (ORIG / "VehicleMenuBar.xml").read_text(encoding="utf-8")
    xml_new = PAYLOAD_XML.read_text(encoding="utf-8")
    xd = [l for l in difflib.unified_diff(xml_old.splitlines(), xml_new.splitlines(), lineterm="", n=0)
          if (l.startswith("+") and not l.startswith("+++")) or (l.startswith("-") and not l.startswith("---"))]
    ok("T3 xml diff is pure addition of 18 lines (two button defs)", len(xd) == 18
       and all(l.startswith("+") for l in xd), f"len={len(xd)}")

    ok("T3 LF line endings preserved (no CR bytes)",
       b"\r" not in PAYLOAD_LUA.read_bytes() and b"\r" not in PAYLOAD_XML.read_bytes()
       and b"\r" not in (ORIG / "VehicleMenuBar.lua").read_bytes())


def t4_xml():
    try:
        ET.parse(PAYLOAD_XML)
        ok("T4 patched XML well-formed", True)
    except Exception as e:
        ok("T4 patched XML well-formed", False, str(e)[:200])


def t5_gate_simulation():
    ps1 = INSTALL_PS1.read_text(encoding="utf-8")
    patcher = PATCHER.read_text(encoding="utf-8")
    gates = re.findall(r"\$patcherText\s+-(?:not)?match\s+'([^']+)'", ps1)
    ok("T5 found the patcher version gate", len(gates) == 1, str(gates))
    if gates:
        ok("T5 version gate matches the real patcher", re.search(gates[0], patcher) is not None)
    ok("T5 PS1 expected lua hash == payload", f'"{LUA_SHA}"' in ps1)
    ok("T5 PS1 expected xml hash == payload", f'"{XML_SHA}"' in ps1)
    ok("T5 PS1 passthrough gates: no DBC hash pinning on Spell",
       "006a892b" not in ps1 and "5db5b7a5" not in ps1)
    ok("T5 PS1 packs all four internal paths",
       all(t in ps1 for t in ("DBFilesClient\\Spell.dbc", "DBFilesClient\\AreaTable.dbc",
                              "Interface\\FrameXML\\VehicleMenuBar.lua", "Interface\\FrameXML\\VehicleMenuBar.xml")))
    r = subprocess.run([sys.executable, str(PATCHER), "verify"], capture_output=True, text=True)
    ok("T5 patcher verify passes", r.returncode == 0 and "G17C11_PAYLOAD_VERIFY=PASS" in r.stdout, r.stdout)


def t6_static():
    r = subprocess.run([sys.executable, str(PKG / "tools" / "ps_static_check.py"),
                        str(INSTALL_PS1), str(PKG / "Rollback-G17C11-ClientMod.ps1")],
                       capture_output=True, text=True)
    ok("T6 PS1 static syntax", r.returncode == 0 and r.stdout.count("PS_STATIC_OK") == 2, r.stdout + r.stderr)


def t7_sums():
    if not SUMS.exists():
        ok("T7 SHA256SUMS exists", False)
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
    ok("T7 SHA256SUMS complete and correct", not missing and not extra and not bad,
       f"missing={missing[:3]} extra={extra[:3]} bad={bad[:3]}")


def main() -> int:
    print(f"G17C11_PACKAGE_SELFTEST package={PKG.name}")
    t1_hashes()
    t2_content()
    t3_diff_contract()
    t4_xml()
    t5_gate_simulation()
    t6_static()
    t7_sums()
    print(f"G17C11_PACKAGE_SELFTEST={'PASS' if FAIL == 0 else 'FAIL'} PASS={PASS} FAIL={FAIL}")
    return 0 if FAIL == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
