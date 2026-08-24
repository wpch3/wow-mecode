#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import sys
from pathlib import Path

EXPECTED = {
    "payload/src/server/scripts/Commands/cs_dragonriding.cpp": "98446106309b45371f138d9c7bc707ee608d9a3db347e13d61cfd68cc97810f9",
    "original/src/server/scripts/Commands/cs_dragonriding.cpp":
        "3b92e815dc81ade4aa9927c19716dabddb8e8f93a6d0aff8b32c80dfbcbfc7f1",
    "rollback_safe/src/server/scripts/Commands/cs_dragonriding.cpp":
        "3b92e815dc81ade4aa9927c19716dabddb8e8f93a6d0aff8b32c80dfbcbfc7f1",
    "tools/apply_g17b2r3_source.py": None,
    "sql/G17B2R3_world_landing_binding_guard.sql": None,
    "tests/test_g17b2r3.py": None,
}


def sha(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--package-root", required=True, type=Path)
    args = ap.parse_args()
    root = args.package_root.resolve()
    rc = 0
    for rel, expected in EXPECTED.items():
        p = root / rel
        if not p.is_file():
            print(f"PACKAGE_FILE_MISSING {rel}")
            rc = 1
            continue
        actual = sha(p)
        if expected is not None and actual != expected:
            print(f"PACKAGE_SHA_MISMATCH {rel} expected={expected} actual={actual}")
            rc = 1
        else:
            print(f"PACKAGE_FILE_OK {rel} sha256={actual}")
    # The tool must embed the postimage hash and every valid R2-lineage
    # upgrade source (earlier drafts included).
    tool = (root / "tools/apply_g17b2r3_source.py").read_text(encoding="utf-8")
    for token in (
        "adedfc58344a104ccc96ff28155b504727f50e0026d842345721610c6a32a59f",
        "3e4590da5d8864f8447cd3b55acf05c249855927a33e0e792dd426f03426237a",
        "613420676babe4c71c570c24a0f5d94976623516e0519b4553b3d5962056bafe",
    ):
        if token not in tool:
            print(f"PACKAGE_TOOL_MISSING_HASH {token}")
            rc = 1
    # Installer gates must match the gate strings the SQL files really emit,
    # and the READMEs must quote the banner the payload really prints.
    install = (root / "Install-Build-G17B2R3-Windows.ps1").read_text(
        encoding="utf-8")
    binding_sql = (root / "sql/G17B2R3_world_landing_binding_guard.sql")
    castable_sql = (root / "sql/G17B2R3_world_spell52226_castable_override.sql")
    pairs = (
        ("G17B2R3_WORLD_BINDING_GUARD=PASS", binding_sql.read_text(
            encoding="utf-8")),
        ("G17B2R3_SPELL_52226_CASTABLE=PASS", castable_sql.read_text(
            encoding="utf-8")),
    )
    for marker, sql_text in pairs:
        if marker not in sql_text:
            print(f"PACKAGE_SQL_MISSING_GATE {marker}")
            rc = 1
        if f'-PassMarker "{marker}"' not in install:
            print(f"PACKAGE_PS1_MISSING_GATE {marker}")
            rc = 1
    payload = (root / "payload/src/server/scripts/Commands/cs_dragonriding.cpp"
               ).read_text(encoding="utf-8")
    banner = (">> G17-B2R3 dragonriding LOADED  build=20260824-r3 "
              "(skill4 castable + skill3 anti-reverse)")
    if banner not in payload:
        print("PACKAGE_PAYLOAD_MISSING_BANNER")
        rc = 1
    if not (root / "PACKAGE_METADATA.txt").is_file():
        print("PACKAGE_FILE_MISSING PACKAGE_METADATA.txt")
        rc = 1
    if rc == 0:
        print("G17B2R3_PACKAGE_SELF_TEST=PASS")
    else:
        print("G17B2R3_PACKAGE_SELF_TEST=FAIL")
    return rc


if __name__ == "__main__":
    sys.exit(main())
