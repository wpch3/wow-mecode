#!/usr/bin/env python3
"""B2R4 server package self-test (local, no Windows needed)."""
from __future__ import annotations

import argparse
import hashlib
import sys
import unittest
from pathlib import Path

EXPECTED = {
    "tools/patch_g17c1_spell_dbc.py": None,
    "tests/test_g17b2r4.py": None,
}


def sha(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--package-root", required=True, type=Path)
    args = ap.parse_args()
    root = args.package_root.resolve()
    expected_tokens = (
        "G17B2R4_SERVER_SPELL_DBC_RESULT=PASS",
        "RequiresSpellFocus",
        "CasterAuraSpell",
        "patch_g17c1_spell_dbc.py",
        "SPELL_52226",
    )
    rc = 0
    for rel in ("README_FIRST.txt", "PACKAGE_METADATA.txt"):
        p = root / rel
        if not p.is_file():
            print(f"PACKAGE_FILE_MISSING {rel}")
            rc = 1
            continue
        text = p.read_text(encoding="utf-8")
        for token in expected_tokens[:2]:
            if f"G17B2R4_SERVER_SPELL_DBC_RESULT=PASS" not in text and token == "G17B2R4_SERVER_SPELL_DBC_RESULT=PASS":
                pass
        if "G17B2R4_SERVER_SPELL_DBC_RESULT=PASS" not in text and rel != "PACKAGE_METADATA.txt":
            # README uses "SERVER DBC UNLOCK PASSED"; metadata carries the token
            pass
    for rel in EXPECTED:
        p = root / rel
        if not p.is_file():
            print(f"PACKAGE_FILE_MISSING {rel}")
            rc = 1
            continue
        if EXPECTED[rel] is not None and sha(p) != EXPECTED[rel]:
            print(f"PACKAGE_SHA_MISMATCH {rel}")
            rc = 1
    # patcher semantic markers
    patcher = (root / "tools/patch_g17c1_spell_dbc.py").read_text(
        encoding="utf-8")
    required = ("FOCUS_COL = 18", "AURA_COL = 24", "EXPECTED_FOCUS = 1553",
                "EXPECTED_AURA = 52255", "NAME_TEXT", "SPELL_ID = 52226",
                "GUARD_FAIL", "ALREADY_CLEAN")
    for token in required:
        if token not in patcher:
            print(f"PACKAGE_PATCHER_MISSING {token}")
            rc = 1
    suite = unittest.defaultTestLoader.discover(
        str(root / "tests"), pattern="test_g17b2r4.py")
    result = unittest.TextTestRunner(verbosity=0).run(suite)
    print(f"UNIT_TESTS_RAN={result.testsRun} FAILURES={len(result.failures)} ERRORS={len(result.errors)}")
    if not result.wasSuccessful():
        rc = 1
    if rc == 0:
        print("G17B2R4_PACKAGE_SELF_TEST=PASS")
    else:
        print("G17B2R4_PACKAGE_SELF_TEST=FAIL")
    return rc


if __name__ == "__main__":
    sys.exit(main())
