#!/usr/bin/env python3
"""G17-C3 package self-test."""
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

    for rel in ("tools/append_g17b3r2_spells.py", "tests/test_g17c3.py",
                "third_party/mpqcli-windows-amd64.exe",
                "Install-G17C3-Skill-Page-Buttons.ps1",
                "Rollback-G17C3-Skill-Page-Buttons.ps1"):
        p = root / rel
        if not p.is_file():
            print(f"PACKAGE_FILE_MISSING {rel}")
            rc = 1

    appender = (root / "tools/append_g17b3r2_spells.py").read_text(
        encoding="utf-8")
    for token in ("ID_BASE = 990025", "COUNT = 4", "FIELDS = 234",
                  "NAME_COL = 140", "EFFECT_DUMMY = 3",
                  'G17B3R2_DBC_APPENDER_VERSION = "v1_append4"'):
        if token not in appender:
            print(f"PACKAGE_APPENDER_MISSING {token}")
            rc = 1

    meta = (root / "PACKAGE_METADATA.txt").read_text(encoding="utf-8")
    if "006a892b0b3363caedc7436f907948778fe6d084759fa0fc0ddc7f7603c03997" not in meta:
        print("PACKAGE_METADATA_MISSING_NEW_SPELL_HASH")
        rc = 1
    if "G17C3_CLIENT_BAR_BUTTONS_RESULT=PASS" not in meta:
        print("PACKAGE_METADATA_MISSING_RESULT_TOKEN")
        rc = 1

    # PowerShell static parse gate
    checker = root / "tools/ps_static_check.py"
    ps1 = [p for p in root.glob("*.ps1")]
    if not checker.is_file() or not ps1:
        print("PACKAGE_PS_STATIC_CHECK_MISSING")
        rc = 1
    else:
        run = subprocess.run([sys.executable, str(checker)] + [str(p) for p in ps1],
                             capture_output=True, text=True)
        if run.returncode != 0:
            print(run.stdout + run.stderr)
            print("PACKAGE_PS_STATIC_CHECK=FAIL")
            rc = 1
        else:
            print("PACKAGE_PS_STATIC_CHECK=PASS")

    suite = unittest.defaultTestLoader.discover(str(root / "tests"),
                                                pattern="test_g17c3.py")
    result = unittest.TextTestRunner(verbosity=0).run(suite)
    print(f"UNIT_TESTS_RAN={result.testsRun} FAILURES={len(result.failures)} "
          f"ERRORS={len(result.errors)}")
    if not result.wasSuccessful():
        rc = 1

    print("G17C3_PACKAGE_SELF_TEST=" + ("PASS" if rc == 0 else "FAIL"))
    return rc


if __name__ == "__main__":
    sys.exit(main())
