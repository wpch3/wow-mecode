#!/usr/bin/env python3
"""G17-B3R1 server package self-test."""
from __future__ import annotations

import argparse
import hashlib
import subprocess
import sys
import unittest
from pathlib import Path

EXPECTED_HASHES = {
    "original_src/src/server/scripts/Commands/cs_dragonriding.cpp":
        "98446106309b45371f138d9c7bc707ee608d9a3db347e13d61cfd68cc97810f9",
    "payload_src/src/server/scripts/Commands/cs_dragonriding.cpp":
        "2ddf54a66395896244869318e4bcfd619d10afc884033c6aa88e7cb53d0e6963",
    "rollback_safe_src/src/server/scripts/Commands/cs_dragonriding.cpp":
        "98446106309b45371f138d9c7bc707ee608d9a3db347e13d61cfd68cc97810f9",
}


def sha(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--package-root", required=True, type=Path)
    args = ap.parse_args()
    root = args.package_root.resolve()
    rc = 0

    for rel, expected in EXPECTED_HASHES.items():
        p = root / rel
        if not p.is_file():
            print(f"PACKAGE_FILE_MISSING {rel}")
            rc = 1
            continue
        if sha(p) != expected:
            print(f"PACKAGE_SHA_MISMATCH {rel}")
            rc = 1

    for rel in ("tools/apply_g17b3r1_source.py", "tools/append_g17b3_spells.py",
                "tools/ps_static_check.py", "tests/test_g17b3r1.py",
                "sql/G17B3R1_world_combat_binding.sql",
                "Install-Build-G17B3R1-Windows.ps1",
                "Rollback-Build-G17B3R1-Windows.ps1"):
        if not (root / rel).is_file():
            print(f"PACKAGE_FILE_MISSING {rel}")
            rc = 1

    install = (root / "Install-Build-G17B3R1-Windows.ps1").read_text(
        encoding="utf-8")
    for token in ("G17B3R1_WINDOWS_BUILD_RESULT=PASS",
                  "G17B3R1_WORLD_COMBAT_BINDING=PASS",
                  "G17B3R1_SERVER_DBC_APPEND=PASS",
                  "apply_g17b3r1_source.py", "append_g17b3_spells.py"):
        if token not in install:
            print(f"PACKAGE_INSTALLER_MISSING {token}")
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

    suite = unittest.defaultTestLoader.discover(str(root / "tests"),
                                                pattern="test_g17b3r1.py")
    result = unittest.TextTestRunner(verbosity=0).run(suite)
    print(f"UNIT_TESTS_RAN={result.testsRun} FAILURES={len(result.failures)} "
          f"ERRORS={len(result.errors)}")
    if not result.wasSuccessful():
        rc = 1

    print("G17B3R1_PACKAGE_SELF_TEST=" + ("PASS" if rc == 0 else "FAIL"))
    return rc


if __name__ == "__main__":
    sys.exit(main())
