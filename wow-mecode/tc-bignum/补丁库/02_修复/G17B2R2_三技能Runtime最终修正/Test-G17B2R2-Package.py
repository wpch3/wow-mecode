#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import sys
from pathlib import Path

EXPECTED = {
    "payload/src/server/scripts/Commands/cs_dragonriding.cpp":
        "3e4590da5d8864f8447cd3b55acf05c249855927a33e0e792dd426f03426237a",
    "original/src/server/scripts/Commands/cs_dragonriding.cpp":
        "ff185d9987b8f4457d8380e1c662cd0313b33a7ae4be6b82974e7702d1fdc4fc",
    "rollback_safe/src/server/scripts/Commands/cs_dragonriding.cpp":
        "ff185d9987b8f4457d8380e1c662cd0313b33a7ae4be6b82974e7702d1fdc4fc",
    "tools/apply_g17b2r2_source.py": None,
    "sql/G17B2R2_world_landing_binding_guard.sql": None,
    "tests/test_g17b2r2.py": None,
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
    # The tool must embed the postimage hash and accept the prior final/intermediate.
    tool = (root / "tools/apply_g17b2r2_source.py").read_text(encoding="utf-8")
    for token in (
        "3e4590da5d8864f8447cd3b55acf05c249855927a33e0e792dd426f03426237a",
        "03dd649ded01dcd1917b1d0e98689ae1dbfe4289f6fc2548a3a62d616e6a0844",
        "adedfc58344a104ccc96ff28155b504727f50e0026d842345721610c6a32a59f",
    ):
        if token not in tool:
            print(f"PACKAGE_TOOL_MISSING_HASH {token}")
            rc = 1
    if rc == 0:
        print("G17B2R2_PACKAGE_SELF_TEST=PASS")
    else:
        print("G17B2R2_PACKAGE_SELF_TEST=FAIL")
    return rc


if __name__ == "__main__":
    sys.exit(main())
