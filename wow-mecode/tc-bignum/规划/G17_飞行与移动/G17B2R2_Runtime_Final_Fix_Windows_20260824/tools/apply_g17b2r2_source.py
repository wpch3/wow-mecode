#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path

SOURCE_RELATIVE = Path("src/server/scripts/Commands/cs_dragonriding.cpp")
# B2R1 postimage is the normal preimage for R2.
PRE_SHA256 = "ff185d9987b8f4457d8380e1c662cd0313b33a7ae4be6b82974e7702d1fdc4fc"
POST_SHA256 = "3b92e815dc81ade4aa9927c19716dabddb8e8f93a6d0aff8b32c80dfbcbfc7f1"
# Earlier R2 builds that are valid to upgrade from (banner-less final and the
# first intermediate with unverified kits).
INTERMEDIATE_SHA256 = "03dd649ded01dcd1917b1d0e98689ae1dbfe4289f6fc2548a3a62d616e6a0844"
INTERMEDIATE2_SHA256 = "adedfc58344a104ccc96ff28155b504727f50e0026d842345721610c6a32a59f"
# Safety rollback returns to the B2R1 byte image (the last user-compiled PASS).
SAFE_ROLLBACK_SHA256 = "ff185d9987b8f4457d8380e1c662cd0313b33a7ae4be6b82974e7702d1fdc4fc"
UPGRADEABLE_SHAS = (INTERMEDIATE_SHA256, INTERMEDIATE2_SHA256)
PACKAGE_ROOT = Path(__file__).resolve().parents[1]
PAYLOAD = PACKAGE_ROOT / "payload" / SOURCE_RELATIVE
SAFE_ROLLBACK = PACKAGE_ROOT / "rollback_safe" / SOURCE_RELATIVE


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def atomic_write(path: Path, data: bytes) -> None:
    temporary = path.with_name(path.name + ".g17b2r2.tmp")
    if temporary.exists():
        raise RuntimeError(f"temporary file exists: {temporary}")
    temporary.write_bytes(data)
    os.replace(temporary, path)


def verify_package() -> None:
    if sha(PAYLOAD) != POST_SHA256:
        raise RuntimeError("package payload SHA mismatch")
    if sha(SAFE_ROLLBACK) != SAFE_ROLLBACK_SHA256:
        raise RuntimeError("package safety-rollback SHA mismatch")


def check(root: Path) -> str:
    verify_package()
    target = root / SOURCE_RELATIVE
    if not target.is_file():
        raise RuntimeError(f"target missing: {target}")
    digest = sha(target)
    states = {
        PRE_SHA256: "READY_B2R1_PREIMAGE",
        POST_SHA256: "B2R2_APPLIED",
        SAFE_ROLLBACK_SHA256: "B2R2_SAFE_ROLLBACK_B2R1",
    }
    for h in UPGRADEABLE_SHAS:
        states[h] = "B2R2_INTERMEDIATE_UPGRADEABLE"
    if digest not in states:
        raise RuntimeError(f"target SHA not recognized: {digest}")
    state = states[digest]
    print(f"G17B2R2_SOURCE_STATE={state}")
    print(f"TARGET_SHA256={digest}")
    return state


def apply(root: Path) -> None:
    state = check(root)
    target = root / SOURCE_RELATIVE
    if state == "B2R2_APPLIED":
        print("G17B2R2_SOURCE_APPLY=ALREADY_CURRENT")
        return

    backup = target.with_name(target.name + ".g17b2r2.b2r1-preimage")
    if state in ("READY_B2R1_PREIMAGE", "B2R2_INTERMEDIATE_UPGRADEABLE"):
        # Keep a forensic backup of whatever is currently there before
        # overwriting it.  If a backup already exists from a previous apply,
        # keep it as-is instead of refusing.
        if not backup.exists():
            backup.write_bytes(target.read_bytes())

    atomic_write(target, PAYLOAD.read_bytes())
    if sha(target) != POST_SHA256:
        raise RuntimeError("postimage SHA mismatch")
    print(f"FORENSIC_BACKUP={backup}")
    print("G17B2R2_SOURCE_APPLY=PASS")


def rollback(root: Path) -> None:
    state = check(root)
    target = root / SOURCE_RELATIVE
    if state == "B2R2_SAFE_ROLLBACK_B2R1":
        print("G17B2R2_SOURCE_ROLLBACK=ALREADY_B2R1")
        return

    # Rollback returns to the B2R1 byte image, which is the last version the
    # user confirmed compiled and launched.  It does NOT restore the older
    # known-bad B2 runtime.
    atomic_write(target, SAFE_ROLLBACK.read_bytes())
    if sha(target) != SAFE_ROLLBACK_SHA256:
        raise RuntimeError("safety rollback SHA mismatch")
    print("G17B2R2_SOURCE_ROLLBACK=PASS_B2R1_FLOOR")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("command", choices=("check", "apply", "rollback"))
    parser.add_argument("--source-root", required=True, type=Path)
    args = parser.parse_args()
    {"check": check, "apply": apply, "rollback": rollback}[args.command](
        args.source_root.resolve())


if __name__ == "__main__":
    main()
