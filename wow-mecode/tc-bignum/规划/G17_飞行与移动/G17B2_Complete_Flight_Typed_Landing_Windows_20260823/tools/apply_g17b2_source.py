#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path

SOURCE_RELATIVE = Path("src/server/scripts/Commands/cs_dragonriding.cpp")
PRE_SHA256 = "35af002b09b5d8112bbc1aaa1750f4a6245adec8b7c91a7852d69bdd283668b8"
POST_SHA256 = "8b47a5b507bc281198363972e10f91ab0ed3784ad920cf810bd20eacfb6ec1d5"
PAYLOAD = Path(__file__).resolve().parents[1] / "payload" / SOURCE_RELATIVE


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def atomic_write(path: Path, data: bytes) -> None:
    temporary = path.with_name(path.name + ".g17b2.tmp")
    if temporary.exists():
        raise RuntimeError(f"temporary file exists: {temporary}")
    temporary.write_bytes(data)
    os.replace(temporary, path)


def check(root: Path) -> str:
    target = root / SOURCE_RELATIVE
    if not target.is_file():
        raise RuntimeError(f"target missing: {target}")
    digest = sha(target)
    if digest == PRE_SHA256:
        state = "READY_PREIMAGE"
    elif digest == POST_SHA256:
        state = "ALREADY_APPLIED"
    else:
        raise RuntimeError(f"target SHA not recognized: {digest}")
    print(f"G17B2_SOURCE_STATE={state}")
    print(f"TARGET_SHA256={digest}")
    return state


def apply(root: Path) -> None:
    state = check(root)
    target = root / SOURCE_RELATIVE
    if sha(PAYLOAD) != POST_SHA256:
        raise RuntimeError("package payload SHA mismatch")
    if state == "ALREADY_APPLIED":
        print("G17B2_SOURCE_APPLY=ALREADY_CURRENT")
        return
    backup = target.with_name(target.name + ".g17b2.preimage")
    if backup.exists():
        if sha(backup) != PRE_SHA256:
            raise RuntimeError("existing backup SHA mismatch")
    else:
        backup.write_bytes(target.read_bytes())
    atomic_write(target, PAYLOAD.read_bytes())
    if sha(target) != POST_SHA256:
        raise RuntimeError("postimage SHA mismatch")
    print(f"BACKUP={backup}")
    print("G17B2_SOURCE_APPLY=PASS")


def rollback(root: Path) -> None:
    target = root / SOURCE_RELATIVE
    backup = target.with_name(target.name + ".g17b2.preimage")
    if not target.is_file() or sha(target) != POST_SHA256:
        raise RuntimeError("target is not exact G17B2 postimage")
    if not backup.is_file() or sha(backup) != PRE_SHA256:
        raise RuntimeError("exact G17B2 backup missing")
    atomic_write(target, backup.read_bytes())
    if sha(target) != PRE_SHA256:
        raise RuntimeError("rollback SHA mismatch")
    backup.unlink()
    print("G17B2_SOURCE_ROLLBACK=PASS")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("command", choices=("check", "apply", "rollback"))
    parser.add_argument("--source-root", required=True, type=Path)
    args = parser.parse_args()
    {"check": check, "apply": apply, "rollback": rollback}[args.command](args.source_root.resolve())


if __name__ == "__main__":
    main()
