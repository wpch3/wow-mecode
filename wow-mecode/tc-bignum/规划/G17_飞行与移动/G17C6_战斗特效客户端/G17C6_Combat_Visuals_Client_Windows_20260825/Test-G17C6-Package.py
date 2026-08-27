#!/usr/bin/env python3
"""G17-C6 package self-test."""
from __future__ import annotations

import argparse
import hashlib
import subprocess
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

    for rel in ("tools/patch_g17b3r5_visuals.py", "tests/test_g17c6.py",
                "third_party/mpqcli-windows-amd64.exe",
                "Install-G17C6-Combat-Visuals.ps1",
                "Rollback-G17C6-Combat-Visuals.ps1"):
        if not (root / rel).is_file():
            print(f"PACKAGE_FILE_MISSING {rel}")
            rc = 1

    patcher = (root / "tools/patch_g17b3r5_visuals.py").read_text(encoding="utf-8")
    for token in ("ID_LO, ID_HI = 990000, 990024", "VISUAL_COL = 131",
                  "RANGE_COL = 46", "BLOCK_VISUALS = [1483, 6587, 7749, 98, 219]",
                  'G17B3R5_VISUAL_PATCHER_VERSION = "v1_visuals_range"'):
        if token not in patcher:
            print(f"PACKAGE_PATCHER_MISSING {token}")
            rc = 1

    meta = (root / "PACKAGE_METADATA.txt").read_text(encoding="utf-8")
    for token in ("5db5b7a52a4fad0e7c05ed6127fe95a437dce158332ae9b626ec99e2b7855e9b",
                  "G17C6_CLIENT_VISUALS_RESULT=PASS"):
        if token not in meta:
            print(f"PACKAGE_METADATA_MISSING {token}")
            rc = 1

    checker = root / "tools/ps_static_check.py"
    ps1 = sorted(root.glob("*.ps1"))
    run = subprocess.run([sys.executable, str(checker)] + [str(p) for p in ps1],
                         capture_output=True, text=True)
    if run.returncode != 0:
        print(run.stdout + run.stderr)
        print("PACKAGE_PS_STATIC_CHECK=FAIL")
        rc = 1
    else:
        print("PACKAGE_PS_STATIC_CHECK=PASS")

    suite = unittest.defaultTestLoader.discover(str(root / "tests"), pattern="test_g17c6.py")
    result = unittest.TextTestRunner(verbosity=0).run(suite)
    print(f"UNIT_TESTS_RAN={result.testsRun} FAILURES={len(result.failures)} ERRORS={len(result.errors)}")
    if not result.wasSuccessful():
        rc = 1

    print("G17C6_PACKAGE_SELF_TEST=" + ("PASS" if rc == 0 else "FAIL"))
    return rc


if __name__ == "__main__":
    sys.exit(main())
