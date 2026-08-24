#!/usr/bin/env python3
"""G17-B2R2 package validator (local/agent-side, no Windows needed).

Reproduces the gates that the one-click installer depends on:

  1. Frozen hashes: original == B2R1 preimage, payload == R2 postimage,
     rollback_safe == B2R1 floor.
  2. The installer/rollback PS1 Read-ToolHash pattern: built exactly like
     PowerShell does ('\\s' inside single quotes reaches .NET regex as \\s),
     matches every hash definition in tools/apply_g17b2r2_source.py and
     captures the full 64-hex value.  It also checks the *function-scope*
     re-match (the Where-Object filter scope does not leak $Matches).
  3. The World SQL gate strings equal the -PassMarker strings the installer
     waits for (a mismatch aborts the run after mysql itself succeeded).
  4. The proof-of-load banner in the payload is quoted verbatim in both
     READMEs, the status doc and PACKAGE_METADATA; docs carry the real
     postimage hash and no stale hash from earlier drafts.
  5. Behavior test suite (tests/test_g17b2r2.py) and package self-test.

Usage:  python3 tools/validate_g17b2r2_package.py [--package-root DIR]
Output: G17B2R2_LOCAL_VALIDATION=PASS (or FAIL at the end).
"""

from __future__ import annotations

import argparse
import hashlib
import io
import re
import subprocess
import sys
import unittest
from pathlib import Path

PRE_SHA = "ff185d9987b8f4457d8380e1c662cd0313b33a7ae4be6b82974e7702d1fdc4fc"
POST_SHA = "3b92e815dc81ade4aa9927c19716dabddb8e8f93a6d0aff8b32c80dfbcbfc7f1"
SAFE_SHA = "ff185d9987b8f4457d8380e1c662cd0313b33a7ae4be6b82974e7702d1fdc4fc"
INTERMEDIATE_SHA = "03dd649ded01dcd1917b1d0e98689ae1dbfe4289f6fc2548a3a62d616e6a0844"
INTERMEDIATE2_SHA = "adedfc58344a104ccc96ff28155b504727f50e0026d842345721610c6a32a59f"
BANNER = ">> G17-B2R2 dragonriding LOADED  build=20260824-r2 (skill2/3/4 fixes active)"
STALE_HASHES = ("3e4590da", "postimage=61342067", "postimage=03dd649d")


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--package-root", type=Path, default=Path(__file__).resolve().parents[1])
    args = ap.parse_args()
    root = args.package_root.resolve()

    def check(name: str, ok: bool, detail: str = "") -> None:
        line = f"{name}={ 'PASS' if ok else 'FAIL'}"
        if detail:
            line += " " + detail
        print(line)
        return ok

    all_ok = True
    rel_src = "payload/src/server/scripts/Commands/cs_dragonriding.cpp"
    rel_orig = "original/src/server/scripts/Commands/cs_dragonriding.cpp"
    rel_rollback = "rollback_safe/src/server/scripts/Commands/cs_dragonriding.cpp"
    payload = root / rel_src
    original = root / rel_orig
    rollback_safe = root / rel_rollback
    tool = root / "tools/apply_g17b2r2_source.py"

    all_ok &= check("ORIGINAL_IS_B2R1_PREIMAGE", sha(original) == PRE_SHA)
    all_ok &= check("PAYLOAD_IS_R2_POSTIMAGE", sha(payload) == POST_SHA)
    all_ok &= check("ROLLBACK_IS_B2R1_FLOOR", sha(rollback_safe) == SAFE_SHA)
    all_ok &= check("PAYLOAD_DIFFERS_FROM_PREIMAGE", sha(payload) != sha(original))

    tool_lines = tool.read_text(encoding="utf-8").splitlines()
    names = {
        "PRE_SHA256": PRE_SHA,
        "POST_SHA256": POST_SHA,
        "SAFE_ROLLBACK_SHA256": SAFE_SHA,
        "INTERMEDIATE_SHA256": INTERMEDIATE_SHA,
        "INTERMEDIATE2_SHA256": INTERMEDIATE2_SHA,
    }
    ps_matched = True
    for name, expected in names.items():
        pattern = r"^\s*" + re.escape(name) + r'\s*=\s*"([0-9a-f]+)"'
        hits = [line for line in tool_lines if re.match(pattern, line)]
        if len(hits) != 1 or re.match(pattern, hits[0]).group(1) != expected:
            ps_matched = False
    all_ok &= check("PS1_PATTERN_MATCHES_TOOL_HASHES", ps_matched)

    for script_name in ("Install-Build-G17B2R2-Windows.ps1",
                        "Rollback-Build-G17B2R2-Windows.ps1"):
        script = (root / script_name).read_text(encoding="utf-8")
        ok = True
        if "$pattern = ('^\\s*' + [regex]::Escape($Name) + '\\s*=\\s*\"([0-9a-f]+)\"')" not in script:
            ok = False
        if "Where-Object { $_ -match $pattern" not in script:
            ok = False
        if "if ($line -notmatch $pattern)" not in script:
            ok = False
        if "return $Matches[1]" not in script:
            ok = False
        all_ok &= check(f"PS1_{script_name}_FUNCTION_SCOPE_READ", ok)

    pairs = (
        ("G17B2R2_WORLD_BINDING_GUARD=PASS",
         "sql/G17B2R2_world_landing_binding_guard.sql"),
        ("G17B2R2_SPELL_52226_CASTABLE=PASS",
         "sql/G17B2R2_world_spell52226_castable_override.sql"),
    )
    sql_ok = True
    for marker, rel in pairs:
        sql_text = (root / rel).read_text(encoding="utf-8")
        install = (root / "Install-Build-G17B2R2-Windows.ps1").read_text(encoding="utf-8")
        if marker not in sql_text:
            sql_ok = False
        if f'-PassMarker "{marker}"' not in install:
            sql_ok = False
    all_ok &= check("SQL_GATE_MARKERS_MATCH_INSTALLER", sql_ok)

    docs = [root / name for name in ("README_FIRST.txt", "README_详细步骤.txt",
                                     "00-G17B2R2_实现与验收状态.md",
                                     "PACKAGE_METADATA.txt")]
    banner_ok = all(BANNER in d.read_text(encoding="utf-8") for d in docs)
    banner_ok &= BANNER in payload.read_text(encoding="utf-8")
    all_ok &= check("BANNER_IS_VERBATIM_IN_DOCS_AND_PAYLOAD", banner_ok)

    docs_ok = True
    for d in docs[:-1]:  # PACKAGE_METADATA has the full hash, skip stale check
        text = d.read_text(encoding="utf-8")
        if "3b92e815" not in text:
            docs_ok = False
        for stale in STALE_HASHES:
            if stale in text:
                docs_ok = False
    all_ok &= check("DOCS_CARRY_REAL_POSTIMAGE_NO_STALE", docs_ok)

    # Behavior suite.
    suite = unittest.defaultTestLoader.discover(str(root / "tests"),
                                                pattern="test_g17b2r2.py")
    stream = io.StringIO()
    result = unittest.TextTestRunner(stream=stream, verbosity=1).run(suite)
    print("UNIT_TESTS_RAN=%d FAILURES=%d ERRORS=%d" %
          (result.testsRun, len(result.failures), len(result.errors)))
    all_ok &= check("UNIT_TEST_SUITE", result.wasSuccessful())

    package_test = subprocess.run(
        [sys.executable, str(root / "Test-G17B2R2-Package.py"),
         "--package-root", str(root)],
        capture_output=True, text=True)
    out = package_test.stdout + package_test.stderr
    all_ok &= check("PACKAGE_SELF_TEST",
                    package_test.returncode == 0
                    and "G17B2R2_PACKAGE_SELF_TEST=PASS" in out)

    print("")
    print("G17B2R2_LOCAL_VALIDATION=%s" % ("PASS" if all_ok else "FAIL"))
    return 0 if all_ok else 1


if __name__ == "__main__":
    sys.exit(main())
