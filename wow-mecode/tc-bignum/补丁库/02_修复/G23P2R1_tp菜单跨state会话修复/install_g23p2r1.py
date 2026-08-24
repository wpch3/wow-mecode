#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""G23-P2R1 hash-locked runtime Lua hotfix installer.

Target: D:\TC-Build\bin\RelWithDebInfo
This batch changes only lua_scripts/custom_teleport.lua.
"""

from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import shutil
import sys
import tempfile

HERE = Path(__file__).resolve().parent
REL = Path("lua_scripts/custom_teleport.lua")
PRE = "9578875f6f9aebb3e50dee1fa9947166360799a4af56c145489d722a29c73b95"
POST = "b84e7c1da66d45c781564917a06cc87b92c693e41964305bdab42068c42a0a23"
ORIGINAL = HERE / "original" / REL
PAYLOAD = HERE / "payload" / REL


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def atomic_copy(source: Path, target: Path) -> None:
    target.parent.mkdir(parents=True, exist_ok=True)
    fd, temp_name = tempfile.mkstemp(prefix=f".{target.name}.g23p2r1.", dir=target.parent)
    os.close(fd)
    temp = Path(temp_name)
    try:
        shutil.copyfile(source, temp)
        os.replace(temp, target)
    finally:
        if temp.exists():
            temp.unlink()


def verify_package() -> None:
    require(ORIGINAL.is_file(), f"missing package original: {ORIGINAL}")
    require(PAYLOAD.is_file(), f"missing package payload: {PAYLOAD}")
    require(sha256(ORIGINAL) == PRE, "package original hash mismatch")
    require(sha256(PAYLOAD) == POST, "package payload hash mismatch")


def inspect(root: Path) -> tuple[str, str]:
    target = root / REL
    if not target.is_file():
        return "MISSING", "MISSING"
    actual = sha256(target)
    if actual == PRE:
        return "PRE_P2", actual
    if actual == POST:
        return "POST_P2R1", actual
    return "UNKNOWN", actual


def overall(state: str) -> str:
    if state == "PRE_P2": return "READY_TO_APPLY"
    if state == "POST_P2R1": return "ALREADY_APPLIED"
    return "BLOCKED_UNKNOWN"


def print_state(root: Path) -> str:
    state, actual = inspect(root)
    print(f"G23P2R1_ROOT={root}")
    print(f"G23P2R1_FILE={REL.as_posix()}; state={state}; sha256={actual}")
    print(f"G23P2R1_STATE={overall(state)}")
    print("G23P2R1_SQL_REQUIRED=False")
    print("G23P2R1_COMPILE_REQUIRED=False")
    print("G23P2R1_RELOAD_ELUNA_ALLOWED=False")
    return state


def install(root: Path, direction: str) -> None:
    verify_package()
    before = print_state(root)
    expected = "PRE_P2" if direction == "apply" else "POST_P2R1"
    final = "POST_P2R1" if direction == "apply" else "PRE_P2"
    source = PAYLOAD if direction == "apply" else ORIGINAL

    if before == final:
        print(f"G23P2R1_{direction.upper()}=NOOP_ALREADY_{final}")
        return
    require(before == expected, f"refusing {direction}: target must be {expected}")

    target = root / REL
    recovery = target.read_bytes()
    try:
        atomic_copy(source, target)
        actual_state, _ = inspect(root)
        require(actual_state == final, f"post-{direction} verification failed: {actual_state}")
    except Exception:
        target.write_bytes(recovery)
        raise

    print(f"G23P2R1_{direction.upper()}=PASS")
    print(f"G23P2R1_FINAL_STATE={final}")


def selftest() -> None:
    verify_package()
    with tempfile.TemporaryDirectory(prefix="g23p2r1-installer-") as temp_name:
        root = Path(temp_name)
        atomic_copy(ORIGINAL, root / REL)
        require(inspect(root)[0] == "PRE_P2", "fixture is not PRE_P2")
        install(root, "apply")
        require(inspect(root)[0] == "POST_P2R1", "apply did not reach POST_P2R1")
        install(root, "apply")
        install(root, "rollback")
        require(inspect(root)[0] == "PRE_P2", "rollback did not reach PRE_P2")
        install(root, "rollback")

        target = root / REL
        target.write_bytes(target.read_bytes() + b"X")
        before = target.read_bytes()
        try:
            install(root, "apply")
        except RuntimeError:
            pass
        else:
            raise RuntimeError("unknown target was not rejected")
        require(target.read_bytes() == before, "unknown-target rejection modified target")
    print("G23P2R1_INSTALLER_SELFTEST=PASS")


def main() -> int:
    parser = argparse.ArgumentParser(description="G23-P2R1 stateless teleport menu hotfix")
    parser.add_argument("action", choices=("check", "apply", "rollback", "selftest"))
    parser.add_argument("root", nargs="?", help=r"runtime root, e.g. D:\TC-Build\bin\RelWithDebInfo")
    args = parser.parse_args()
    try:
        if args.action == "selftest":
            selftest()
            return 0
        if not args.root:
            parser.error("root is required")
        root = Path(args.root).expanduser().resolve()
        verify_package()
        if args.action == "check":
            state = print_state(root)
            return 0 if state in {"PRE_P2", "POST_P2R1"} else 2
        install(root, args.action)
        return 0
    except Exception as error:
        print(f"G23P2R1_ERROR={error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
