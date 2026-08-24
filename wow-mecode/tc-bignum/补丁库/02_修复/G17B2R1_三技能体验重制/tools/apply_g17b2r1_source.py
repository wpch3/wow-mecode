#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path

SOURCE_RELATIVE = Path("src/server/scripts/Commands/cs_dragonriding.cpp")
PRE_SHA256 = "8b47a5b507bc281198363972e10f91ab0ed3784ad920cf810bd20eacfb6ec1d5"
POST_SHA256 = "ff185d9987b8f4457d8380e1c662cd0313b33a7ae4be6b82974e7702d1fdc4fc"
SAFE_ROLLBACK_SHA256 = "e298a856edcf366b09934c3635ea8493b6d4e529d9fa2dbf2de2bce77b5b0203"
PACKAGE_ROOT = Path(__file__).resolve().parents[1]
PAYLOAD = PACKAGE_ROOT / "payload" / SOURCE_RELATIVE
SAFE_ROLLBACK = PACKAGE_ROOT / "rollback_safe" / SOURCE_RELATIVE


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def atomic_write(path: Path, data: bytes) -> None:
    temporary = path.with_name(path.name + ".g17b2r1.tmp")
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
        PRE_SHA256: "READY_B2_PREIMAGE",
        POST_SHA256: "B2R1_APPLIED",
        SAFE_ROLLBACK_SHA256: "B2R1_SAFE_ROLLBACK",
    }
    if digest not in states:
        raise RuntimeError(f"target SHA not recognized: {digest}")
    state = states[digest]
    print(f"G17B2R1_SOURCE_STATE={state}")
    print(f"TARGET_SHA256={digest}")
    return state


def apply(root: Path) -> None:
    state = check(root)
    target = root / SOURCE_RELATIVE
    if state == "B2R1_APPLIED":
        print("G17B2R1_SOURCE_APPLY=ALREADY_CURRENT")
        return

    backup = target.with_name(target.name + ".g17b2r1.b2-preimage")
    if state == "READY_B2_PREIMAGE":
        if backup.exists() and sha(backup) != PRE_SHA256:
            raise RuntimeError("existing B2 forensic backup SHA mismatch")
        if not backup.exists():
            backup.write_bytes(target.read_bytes())

    atomic_write(target, PAYLOAD.read_bytes())
    if sha(target) != POST_SHA256:
        raise RuntimeError("postimage SHA mismatch")
    print(f"FORENSIC_BACKUP={backup}")
    print("G17B2R1_SOURCE_APPLY=PASS")


def rollback(root: Path) -> None:
    state = check(root)
    target = root / SOURCE_RELATIVE
    if state == "B2R1_SAFE_ROLLBACK":
        print("G17B2R1_SOURCE_ROLLBACK=ALREADY_SAFE")
        return

    # Deliberately do not restore the exact known-bad B2 runtime. The rollback
    # source removes R1's complex paths but keeps the visual-free action and
    # fire-free beast visual. The frozen B2 file remains in the package only as
    # forensic preimage evidence.
    atomic_write(target, SAFE_ROLLBACK.read_bytes())
    if sha(target) != SAFE_ROLLBACK_SHA256:
        raise RuntimeError("safety rollback SHA mismatch")
    print("G17B2R1_SOURCE_ROLLBACK=PASS_SAFE_FLOOR")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("command", choices=("check", "apply", "rollback"))
    parser.add_argument("--source-root", required=True, type=Path)
    args = parser.parse_args()
    {"check": check, "apply": apply, "rollback": rollback}[args.command](
        args.source_root.resolve())


if __name__ == "__main__":
    main()
