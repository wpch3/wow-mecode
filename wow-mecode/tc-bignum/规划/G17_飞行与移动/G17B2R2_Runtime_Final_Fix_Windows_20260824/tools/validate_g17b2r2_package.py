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
  3. Every valid R2-lineage upgrade source is recognized by the apply tool
     AND read from the tool by the PS1 (single source of truth).  The exact
     failure of the user's first real Windows run (source SHA 3e4590da, the
     earlier R2 draft) is asserted as accepted.
  4. The World SQL gate strings equal the -PassMarker strings the installer
     waits for (a mismatch aborts the run after mysql itself succeeded).
  5. The proof-of-load banner in the payload is quoted verbatim in both
     READMEs, the status doc and PACKAGE_METADATA; docs carry the real
     postimage hash and no stale postimage claim.
  6. Behavior test suite (tests/test_g17b2r2.py) and package self-test.

Usage:  python3 tools/validate_g17b2r2_package.py [--package-root DIR]
Output: G17B2R2_LOCAL_VALIDATION=PASS (or FAIL at the end).
"""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
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
# The earlier R2 draft that was on the user's machine when the first one-click
# run was attempted (an earlier delivery README called 3e4590da "final").
INTERMEDIATE3_SHA = "3e4590da5d8864f8447cd3b55acf05c249855927a33e0e792dd426f03426237a"
# The earliest R2 draft shell (archived under 补丁库/规划 in the repo).
INTERMEDIATE4_SHA = "613420676babe4c71c570c24a0f5d94976623516e0519b4553b3d5962056bafe"
BANNER = ">> G17-B2R2 dragonriding LOADED  build=20260824-r2 (skill2/3/4 fixes active)"
STALE_HASHES = ("postimage=61342067", "postimage=03dd649d",
                '$Post = "3e4590da', '$Post = "61342067')
ALL_INTERMEDIATE_NAMES = ("INTERMEDIATE_SHA256", "INTERMEDIATE2_SHA256",
                          "INTERMEDIATE3_SHA256", "INTERMEDIATE4_SHA256")


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--package-root", type=Path,
                    default=Path(__file__).resolve().parents[1])
    args = ap.parse_args()
    root = args.package_root.resolve()
    ok_flags = []

    def check(name: str, ok: bool, detail: str = "") -> None:
        line = f"{name}={'PASS' if ok else 'FAIL'}"
        if detail:
            line += " " + detail
        print(line)
        ok_flags.append(ok)
        return ok

    rel_src = "payload/src/server/scripts/Commands/cs_dragonriding.cpp"
    rel_orig = "original/src/server/scripts/Commands/cs_dragonriding.cpp"
    rel_rollback = "rollback_safe/src/server/scripts/Commands/cs_dragonriding.cpp"
    payload = root / rel_src
    original = root / rel_orig
    rollback_safe = root / rel_rollback
    tool = root / "tools/apply_g17b2r2_source.py"

    check("ORIGINAL_IS_B2R1_PREIMAGE", sha(original) == PRE_SHA)
    check("PAYLOAD_IS_R2_POSTIMAGE", sha(payload) == POST_SHA)
    check("ROLLBACK_IS_B2R1_FLOOR", sha(rollback_safe) == SAFE_SHA)
    check("PAYLOAD_DIFFERS_FROM_PREIMAGE", sha(payload) != sha(original))

    tool_lines = tool.read_text(encoding="utf-8").splitlines()
    names = {
        "PRE_SHA256": PRE_SHA,
        "POST_SHA256": POST_SHA,
        "SAFE_ROLLBACK_SHA256": SAFE_SHA,
        "INTERMEDIATE_SHA256": INTERMEDIATE_SHA,
        "INTERMEDIATE2_SHA256": INTERMEDIATE2_SHA,
        "INTERMEDIATE3_SHA256": INTERMEDIATE3_SHA,
        "INTERMEDIATE4_SHA256": INTERMEDIATE4_SHA,
    }
    ps_matched = True
    for name, expected in names.items():
        pattern = r"^\s*" + re.escape(name) + r'\s*=\s*"([0-9a-f]+)"'
        hits = [line for line in tool_lines if re.match(pattern, line)]
        if len(hits) != 1 or re.match(pattern, hits[0]).group(1) != expected:
            ps_matched = False
    check("PS1_PATTERN_MATCHES_TOOL_HASHES", ps_matched)

    # The PS1 pattern line uses ONE single-quoted pattern string.  In the
    # file it literally contains single backslashes; compare via regex so we
    # never trip over Python escape-sequence warnings.
    ps_pattern = (r"\$pattern = \('\^\\s\*' \+ \[regex\]::Escape\(\$Name\) "
                  r"\+ '\\s\*=\\s\*\"\(\[0-9a-f\]\+\)\"'\)")
    for script_name in ("Install-Build-G17B2R2-Windows.ps1",
                        "Rollback-Build-G17B2R2-Windows.ps1"):
        script = (root / script_name).read_text(encoding="utf-8")
        ok = True
        if not re.search(ps_pattern, script):
            ok = False
        if "Where-Object { $_ -match $pattern" not in script:
            ok = False
        if "if ($line -notmatch $pattern)" not in script:
            ok = False
        if "return $Matches[1]" not in script:
            ok = False
        check(f"PS1_{script_name}_FUNCTION_SCOPE_READ", ok)

    install_ps1 = (root / "Install-Build-G17B2R2-Windows.ps1").read_text(
        encoding="utf-8")
    read_all = all(f'Read-ToolHash "{name}"' in install_ps1
                   for name in ALL_INTERMEDIATE_NAMES)
    no_hardcode = "$Upgradeable +=" not in install_ps1
    check("PS1_UPGRADE_SOURCES_FROM_TOOL_SINGLE_SOURCE",
          read_all and no_hardcode)

    spec = importlib.util.spec_from_file_location("applytool_g17b2r2", tool)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    check("TOOL_ACCEPTS_USER_SOURCE_3E4590DA",
          INTERMEDIATE3_SHA in mod.UPGRADEABLE_SHAS)
    check("TOOL_ACCEPTS_ALL_R2_LINEAGE",
          all(h in mod.UPGRADEABLE_SHAS for h in (
              INTERMEDIATE_SHA, INTERMEDIATE2_SHA,
              INTERMEDIATE3_SHA, INTERMEDIATE4_SHA)))

    pairs = (
        ("G17B2R2_WORLD_BINDING_GUARD=PASS",
         "sql/G17B2R2_world_landing_binding_guard.sql"),
        ("G17B2R2_SPELL_52226_CASTABLE=PASS",
         "sql/G17B2R2_world_spell52226_castable_override.sql"),
    )
    sql_ok = True
    for marker, rel in pairs:
        sql_text = (root / rel).read_text(encoding="utf-8")
        if marker not in sql_text:
            sql_ok = False
        if f'-PassMarker "{marker}"' not in install_ps1:
            sql_ok = False
    check("SQL_GATE_MARKERS_MATCH_INSTALLER", sql_ok)

    docs = [root / name for name in ("README_FIRST.txt", "README_详细步骤.txt",
                                     "00-G17B2R2_实现与验收状态.md",
                                     "PACKAGE_METADATA.txt")]
    banner_ok = all(BANNER in d.read_text(encoding="utf-8") for d in docs)
    banner_ok &= BANNER in payload.read_text(encoding="utf-8")
    check("BANNER_IS_VERBATIM_IN_DOCS_AND_PAYLOAD", banner_ok)

    docs_ok = True
    for d in docs[:-1]:  # PACKAGE_METADATA has the full hash, skip stale check
        text = d.read_text(encoding="utf-8")
        if "3b92e815" not in text:
            docs_ok = False
        for stale in STALE_HASHES:
            if stale in text:
                docs_ok = False
    check("DOCS_CARRY_REAL_POSTIMAGE_NO_STALE", docs_ok)

    suite = unittest.defaultTestLoader.discover(str(root / "tests"),
                                                pattern="test_g17b2r2.py")
    stream = io.StringIO()
    result = unittest.TextTestRunner(stream=stream, verbosity=1).run(suite)
    print("UNIT_TESTS_RAN=%d FAILURES=%d ERRORS=%d" %
          (result.testsRun, len(result.failures), len(result.errors)))
    check("UNIT_TEST_SUITE", result.wasSuccessful())

    package_test = subprocess.run(
        [sys.executable, str(root / "Test-G17B2R2-Package.py"),
         "--package-root", str(root)],
        capture_output=True, text=True)
    out = package_test.stdout + package_test.stderr
    check("PACKAGE_SELF_TEST",
          package_test.returncode == 0
          and "G17B2R2_PACKAGE_SELF_TEST=PASS" in out)

    print("")
    print("G17B2R2_LOCAL_VALIDATION=%s" % ("PASS" if all(ok_flags) else "FAIL"))
    return 0 if all(ok_flags) else 1


if __name__ == "__main__":
    sys.exit(main())
