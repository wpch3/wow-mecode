#!/usr/bin/env python3
"""G17-C1 package self-test (local)."""
from __future__ import annotations
import argparse
import hashlib
import sys
import unittest
from pathlib import Path

def sha(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--package-root", required=True, type=Path)
    args = ap.parse_args()
    root = args.package_root.resolve()
    rc = 0
    for rel in ("tools/patch_g17c1_spell_dbc.py", "tests/test_g17c1.py",
                "third_party/mpqcli-windows-amd64.exe"):
        p = root / rel
        if not p.is_file():
            print(f"PACKAGE_FILE_MISSING {rel}")
            rc = 1
    patcher = (root / "tools/patch_g17c1_spell_dbc.py").read_text(encoding="utf-8")
    for token in ("FOCUS_COL = 18", "AURA_COL = 24", "EXPECTED_FOCUS = 1553",
                  "EXPECTED_AURA = 52255", "NAME_TEXT", "SPELL_ID = 52226"):
        if token not in patcher:
            print(f"PACKAGE_PATCHER_MISSING {token}")
            rc = 1
    meta = (root / "PACKAGE_METADATA.txt").read_text(encoding="utf-8")
    if "03bf11fdeff7c296837fc6b0cc335476a9df33965baf8eed8ca671529577ccba" not in meta:
        print("PACKAGE_METADATA_MISSING_NEW_SPELL_HASH")
        rc = 1
    if "G17C1_CLIENT_MPQ_UNLOCK_RESULT=PASS" not in meta:
        print("PACKAGE_METADATA_MISSING_RESULT_TOKEN")
        rc = 1
    suite = unittest.defaultTestLoader.discover(str(root / "tests"), pattern="test_g17c1.py")
    result = unittest.TextTestRunner(verbosity=0).run(suite)
    print(f"UNIT_TESTS_RAN={result.testsRun} FAILURES={len(result.failures)} ERRORS={len(result.errors)}")
    if not result.wasSuccessful():
        rc = 1
    print("G17C1_PACKAGE_SELF_TEST=" + ("PASS" if rc == 0 else "FAIL"))
    return rc

if __name__ == "__main__":
    sys.exit(main())
