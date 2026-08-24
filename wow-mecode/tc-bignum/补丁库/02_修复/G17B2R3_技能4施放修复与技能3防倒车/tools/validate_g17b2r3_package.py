#!/usr/bin/env python3
"""G17-B2R3 package validator (local/agent-side, no Windows needed).

Reproduces the gates that the one-click installer depends on:

  1. Frozen hashes: original == R2 postimage (3b92e815), payload == R3
     postimage, rollback_safe == R2 floor.
  2. PS1 Read-ToolHash two-step function-scope pattern (the Where-Object
     filter scope does not leak $Matches) and single-source hash reading.
  3. state_for_digest pure classifier: PRE/POST/SAFE + all five R2-lineage
     upgrade sources; unknown digest rejected (zero writes).
  4. World SQL gate strings equal the -PassMarker strings the installer
     waits for.
  5. B2R3 payload specifics: 52226 cast-gate sanitizer
     (RequiresSpellFocus/CasterAuraSpell/Stances/items) present and invoked
     at load + boarding; skill 3 stops old motion before sampling the path.
  6. Banner + sanitizer proof line quoted verbatim in READMEs/status; docs
     carry the real postimage and no stale postimage claims.
  7. Behavior test suite (tests/test_g17b2r3.py) and package self-test.

Usage:  python3 tools/validate_g17b2r3_package.py [--package-root DIR]
Output: G17B2R3_LOCAL_VALIDATION=PASS (or FAIL at the end).
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

PRE_SHA = "3b92e815dc81ade4aa9927c19716dabddb8e8f93a6d0aff8b32c80dfbcbfc7f1"
POST_SHA = "98446106309b45371f138d9c7bc707ee608d9a3db347e13d61cfd68cc97810f9"
SAFE_SHA = "3b92e815dc81ade4aa9927c19716dabddb8e8f93a6d0aff8b32c80dfbcbfc7f1"
INTERMEDIATE_SHA = "ff185d9987b8f4457d8380e1c662cd0313b33a7ae4be6b82974e7702d1fdc4fc"
INTERMEDIATE2_SHA = "3e4590da5d8864f8447cd3b55acf05c249855927a33e0e792dd426f03426237a"
INTERMEDIATE3_SHA = "613420676babe4c71c570c24a0f5d94976623516e0519b4553b3d5962056bafe"
INTERMEDIATE4_SHA = "03dd649ded01dcd1917b1d0e98689ae1dbfe4289f6fc2548a3a62d616e6a0844"
INTERMEDIATE5_SHA = "adedfc58344a104ccc96ff28155b504727f50e0026d842345721610c6a32a59f"
BANNER = ">> G17-B2R3 dragonriding LOADED  build=20260824-r3 (skill4 castable + skill3 anti-reverse)"
SANITIZER_LINE = ">> G17-B2R3 landing command %u cast-gates cleared (focus/aura/item/stance)"
STALE_CLAIMS = ("postimage=61342067", "postimage=03dd649d",
                '$Post = "3e4590da', '$Post = "61342067')
ALL_INTERMEDIATE_NAMES = ("INTERMEDIATE_SHA256", "INTERMEDIATE2_SHA256",
                          "INTERMEDIATE3_SHA256", "INTERMEDIATE4_SHA256",
                          "INTERMEDIATE5_SHA256")


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
    tool = root / "tools/apply_g17b2r3_source.py"

    check("ORIGINAL_IS_R2_POSTIMAGE", sha(original) == PRE_SHA)
    check("PAYLOAD_IS_R3_POSTIMAGE", sha(payload) == POST_SHA)
    check("ROLLBACK_IS_R2_FLOOR", sha(rollback_safe) == SAFE_SHA)
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
        "INTERMEDIATE5_SHA256": INTERMEDIATE5_SHA,
    }
    ps_matched = True
    for name, expected in names.items():
        pattern = r"^\s*" + re.escape(name) + r'\s*=\s*"([0-9a-f]+)"'
        hits = [line for line in tool_lines if re.match(pattern, line)]
        if len(hits) != 1 or re.match(pattern, hits[0]).group(1) != expected:
            ps_matched = False
    check("PS1_PATTERN_MATCHES_TOOL_HASHES", ps_matched)

    ps_pattern = (r"\$pattern = \('\^\\s\*' \+ \[regex\]::Escape\(\$Name\) "
                  r"\+ '\\s\*=\\s\*\"\(\[0-9a-f\]\+\)\"'\)")
    for script_name in ("Install-Build-G17B2R3-Windows.ps1",
                        "Rollback-Build-G17B2R3-Windows.ps1"):
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

    install_ps1 = (root / "Install-Build-G17B2R3-Windows.ps1").read_text(
        encoding="utf-8")
    read_all = all(f'Read-ToolHash "{name}"' in install_ps1
                   for name in ALL_INTERMEDIATE_NAMES)
    no_hardcode = "$Upgradeable +=" not in install_ps1
    check("PS1_UPGRADE_SOURCES_FROM_TOOL_SINGLE_SOURCE",
          read_all and no_hardcode)

    spec = importlib.util.spec_from_file_location("applytool_g17b2r3", tool)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    check("TOOL_ACCEPTS_ALL_R2_LINEAGE",
          all(h in mod.UPGRADEABLE_SHAS for h in (
              INTERMEDIATE_SHA, INTERMEDIATE2_SHA,
              INTERMEDIATE3_SHA, INTERMEDIATE4_SHA, INTERMEDIATE5_SHA)))
    classifier_ok = (hasattr(mod, "state_for_digest")
                     and mod.state_for_digest(PRE_SHA) in
                     ("READY_B2R2_PREIMAGE", "B2R3_SAFE_ROLLBACK_B2R2")
                     and mod.state_for_digest(POST_SHA) == "B2R3_APPLIED"
                     and mod.state_for_digest("40" * 32) == "")
    check("TOOL_STATE_CLASSIFIER_PURE_AND_CORRECT", classifier_ok)

    pairs = (
        ("G17B2R3_WORLD_BINDING_GUARD=PASS",
         "sql/G17B2R3_world_landing_binding_guard.sql"),
        ("G17B2R3_SPELL_52226_CASTABLE=PASS",
         "sql/G17B2R3_world_spell52226_castable_override.sql"),
    )
    sql_ok = True
    for marker, rel in pairs:
        sql_text = (root / rel).read_text(encoding="utf-8")
        if marker not in sql_text:
            sql_ok = False
        if f'-PassMarker "{marker}"' not in install_ps1:
            sql_ok = False
    check("SQL_GATE_MARKERS_MATCH_INSTALLER", sql_ok)

    payload_text = payload.read_text(encoding="utf-8")
    sanitizer_ok = all(token in payload_text for token in (
        "void EnsureLandingCommandCastable()",
        "info->RequiresSpellFocus = 0;",
        "info->CasterAuraSpell = 0;",
        "info->Stances = 0;",
        "info->EquippedItemClass = -1;",
        "info->Reagent", "info->TotemCategory"))
    calls = payload_text.count("G17Dragonriding::EnsureLandingCommandCastable();")
    sanitizer_ok &= calls >= 2
    check("PAYLOAD_52226_CAST_GATE_SANITIZER", sanitizer_ok)

    body = payload_text[payload_text.index("void StartForwardClimb()"):]
    anti_reverse = ("me->StopMoving();" in body
                    and body.index("me->StopMoving();") <
                    body.index("BuildClimbPath(distance, path, exitHeading)"))
    after_momentum = body[body.index("_momentum = std::max(0.0f, _momentum - 0.18f);"):]
    anti_reverse &= "RestoreClientFlightControl(true);" not in \
        after_momentum[:after_momentum.index("LaunchMoveSpline")]
    check("PAYLOAD_SKILL3_STOP_BEFORE_PATH_SAMPLE", anti_reverse)

    docs = [root / name for name in ("README_FIRST.txt", "README_详细步骤.txt",
                                     "00-G17B2R3_实现与验收状态.md",
                                     "PACKAGE_METADATA.txt")]
    banner_ok = all(BANNER in d.read_text(encoding="utf-8") for d in docs)
    banner_ok &= BANNER in payload_text
    check("BANNER_IS_VERBATIM_IN_DOCS_AND_PAYLOAD", banner_ok)
    check("SANITIZER_PROOF_LINE_IN_PAYLOAD", SANITIZER_LINE in payload_text)

    docs_ok = True
    for d in docs[:-1]:
        text = d.read_text(encoding="utf-8")
        if "98446106" not in text:
            docs_ok = False
        for stale in STALE_CLAIMS:
            if stale in text:
                docs_ok = False
    check("DOCS_CARRY_REAL_POSTIMAGE_NO_STALE", docs_ok)

    suite = unittest.defaultTestLoader.discover(str(root / "tests"),
                                                pattern="test_g17b2r3.py")
    stream = io.StringIO()
    result = unittest.TextTestRunner(stream=stream, verbosity=1).run(suite)
    print("UNIT_TESTS_RAN=%d FAILURES=%d ERRORS=%d" %
          (result.testsRun, len(result.failures), len(result.errors)))
    check("UNIT_TEST_SUITE", result.wasSuccessful())

    package_test = subprocess.run(
        [sys.executable, str(root / "Test-G17B2R3-Package.py"),
         "--package-root", str(root)],
        capture_output=True, text=True)
    out = package_test.stdout + package_test.stderr
    check("PACKAGE_SELF_TEST",
          package_test.returncode == 0
          and "G17B2R3_PACKAGE_SELF_TEST=PASS" in out)

    print("")
    print("G17B2R3_LOCAL_VALIDATION=%s" % ("PASS" if all(ok_flags) else "FAIL"))
    return 0 if all(ok_flags) else 1


if __name__ == "__main__":
    sys.exit(main())
