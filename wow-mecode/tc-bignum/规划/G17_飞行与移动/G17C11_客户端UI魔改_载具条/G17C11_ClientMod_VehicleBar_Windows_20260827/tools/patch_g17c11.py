#!/usr/bin/env python3
"""G17-C11: CLIENT MOD - VehicleMenuBar 8-slot vehicle action bar.

This is real client modification (客户端魔改): the stock 3.3.5 UI hardcodes
VEHICLE_MAX_ACTIONBUTTONS = 6 (VehicleMenuBar.lua line 6) and creates only six
VehicleMenuBarActionButton frames.  The client's own keybind routing
(ActionButton.lua ActionButtonDown/Up) already follows that constant, so two
minimal file edits give the STOCK vehicle bar eight native slots:

  1. VehicleMenuBar.lua : VEHICLE_MAX_ACTIONBUTTONS = 6 -> 8
  2. VehicleMenuBar.xml : add VehicleMenuBarActionButton7/8 (same template,
     same anchor chain LEFT of the previous button, offset x=2)

The patched files ship pre-built in payload/Interface/FrameXML/ (patched from
the user's own client via G17Extract; only these hunks differ).  The installer
adds them to the existing G17 patch-MPQ chain alongside the DBC files
(Spell.dbc / AreaTable.dbc are passed through byte-identically).

Baseline (unmodified) hashes for the two files:
  VehicleMenuBar.lua 2183cb19190a49c5207624c44e756110764c18734c086ad41a8e3eda422a7a34
  VehicleMenuBar.xml fff0aec7e40f564b04114c0794be9152e0c546082d9a38ed6e6853e0903bc4df
"""
from __future__ import annotations

import argparse
import hashlib
import sys
from pathlib import Path

G17C11_VERSION = "v1_vehiclebar_8slots"
PACKAGE_ROOT = Path(__file__).resolve().parents[1]

PAYLOAD_LUA = PACKAGE_ROOT / "payload" / "Interface" / "FrameXML" / "VehicleMenuBar.lua"
PAYLOAD_XML = PACKAGE_ROOT / "payload" / "Interface" / "FrameXML" / "VehicleMenuBar.xml"

PAYLOAD_LUA_SHA256 = "0d572a7fbb7d69005b02f22932fbdc8f64c06f0d1d4ddc52a048300f30bb1a3c"
PAYLOAD_XML_SHA256 = "31563ecf8787054408bab4049f212b2b77aa56c12dd264d60aa03b1494f1e628"
BASELINE_LUA_SHA256 = "2183cb19190a49c5207624c44e756110764c18734c086ad41a8e3eda422a7a34"
BASELINE_XML_SHA256 = "fff0aec7e40f564b04114c0794be9152e0c546082d9a38ed6e6853e0903bc4df"


def sha_file(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify_payload() -> None:
    if sha_file(PAYLOAD_LUA) != PAYLOAD_LUA_SHA256:
        raise RuntimeError("payload VehicleMenuBar.lua hash mismatch")
    if sha_file(PAYLOAD_XML) != PAYLOAD_XML_SHA256:
        raise RuntimeError("payload VehicleMenuBar.xml hash mismatch")
    lua = PAYLOAD_LUA.read_text(encoding="utf-8")
    xml = PAYLOAD_XML.read_text(encoding="utf-8")
    if lua.count("VEHICLE_MAX_ACTIONBUTTONS = 8;") != 1:
        raise RuntimeError("payload lua missing the 8-slot constant")
    if xml.count('name="VehicleMenuBarActionButton7"') != 1:
        raise RuntimeError("payload xml missing button 7")
    if xml.count('name="VehicleMenuBarActionButton8"') != 1:
        raise RuntimeError("payload xml missing button 8")


def check_lua(path: Path) -> int:
    """Report the client-side state of an extracted VehicleMenuBar.lua."""
    text = path.read_text(encoding="utf-8", errors="replace")
    if "VEHICLE_MAX_ACTIONBUTTONS = 8;" in text:
        state = "COMPLETE"
    elif "VEHICLE_MAX_ACTIONBUTTONS = 6;" in text:
        state = "FRESH"
    else:
        state = "PARTIAL"
    print(f"G17C11_STATE={state}")
    print("G17C11_CHECK=PASS" if state != "PARTIAL" else "G17C11_CHECK=FAIL")
    return 0 if state != "PARTIAL" else 2


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--version", action="store_true")
    ap.add_argument("command", nargs="?", choices=("check", "verify"))
    ap.add_argument("--lua", default="")
    args = ap.parse_args()

    if args.version:
        print("G17C11_VERSION=" + G17C11_VERSION)
        return 0
    if args.command == "verify":
        verify_payload()
        print("G17C11_PAYLOAD_VERIFY=PASS")
        return 0
    if args.command == "check":
        if not args.lua:
            print("usage: patch_g17c11.py check --lua <extracted VehicleMenuBar.lua>")
            return 2
        return check_lua(Path(args.lua))
    print("usage: patch_g17c11.py [--version | verify | check --lua PATH]")
    return 2


if __name__ == "__main__":
    sys.exit(main())
