#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""G23-P2 hash-locked runtime Lua installer.

Target root is D:\TC-Build\bin\RelWithDebInfo, not the TrinityCore source tree.
SQL is deliberately separate and must be imported with HeidiSQL before startup.
"""

from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import shutil
import sys
import tempfile

BATCH = "G23P2_LUA_STABILITY"
HERE = Path(__file__).resolve().parent
ORIGINAL = HERE / "original"
PAYLOAD = HERE / "payload"

MODIFIED = {
    "lua_scripts/bignum_selftest.lua": (
        "7b0ba63fa13ac6ae085deb3d611f9dc69e5d9ac8a7162c79da614d80256270e3",
        "9c8e4ec5fc841187c7c263264ef60d5ec3857e295c250dda6f29d5483628cf76",
    ),
    "lua_scripts/custom_announce.lua": (
        "63b756a638232bc26ed785a0824b6729511d0c69fef03a1624e283245807d238",
        "20559972a3ee226f87de7a7f1d493ddc99ae845755d2e7f613d3909959161d28",
    ),
    "lua_scripts/custom_daily_reward.lua": (
        "204fe2428f85a3ae648a8c1d0061205e22284c16b2f9b61a0c6e5c611953dafe",
        "2814c4586c0e5e75f861424d21ae8be4db1638d42338e2a813c3eb0b06933b07",
    ),
    "lua_scripts/custom_teleport.lua": (
        "663759959aada88ce596bcef2f2b5a3e6ef8dc5def1dd51b7068a6d2ec8a4f45",
        "9578875f6f9aebb3e50dee1fa9947166360799a4af56c145489d722a29c73b95",
    ),
    "lua_scripts/custom_welcome.lua": (
        "d9ffe19f46ed749c64b7447e8184009679eb0aa4a5f374580263da41b635cdfa",
        "c0baa8fa5954a16c1a15ae0de48626a7dac13b80834896ab85e96e1e0d6836c2",
    ),
    "lua_scripts/extensions/ObjectVariables.ext": (
        "6a682daa547db2a72620e7e1f906532c52c1dd4c1345837d4d6b24640307a547",
        "f823b6139319ed982b03586845841fa668f949970948b909876fd5f0da642e4b",
    ),
}

NEW_FILES = {
    "lua_scripts/custom_diag.lua": "fecd8da54885f657786f9e6f1953b49614d9fe9d136b3e6631f5c79a531e7f8d",
    "lua_scripts/extensions/G23Core.ext": "89b394b19654bd346c38632d91b6fb9d6e8fb7d5ba640658d21231b2ae91a06c",
    "worldserver.conf.d/zz_g23_p2_eluna_security.conf": "1721879827c48a191adfbe4b5877e7d20b230adf261b2b2009c7e288a401e905",
}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def atomic_copy(source: Path, target: Path) -> None:
    target.parent.mkdir(parents=True, exist_ok=True)
    fd, temp_name = tempfile.mkstemp(prefix=f".{target.name}.g23p2.", dir=target.parent)
    os.close(fd)
    temp = Path(temp_name)
    try:
        shutil.copyfile(source, temp)
        os.replace(temp, target)
    finally:
        if temp.exists():
            temp.unlink()


def verify_package() -> None:
    errors: list[str] = []
    for rel, (pre, post) in MODIFIED.items():
        for side, root, expected in (("pre", ORIGINAL, pre), ("post", PAYLOAD, post)):
            path = root / rel
            if not path.is_file():
                errors.append(f"missing package {side}: {rel}")
            elif sha256(path) != expected:
                errors.append(f"package {side} hash mismatch: {rel}")
    for rel, post in NEW_FILES.items():
        path = PAYLOAD / rel
        if not path.is_file():
            errors.append(f"missing new payload: {rel}")
        elif sha256(path) != post:
            errors.append(f"new payload hash mismatch: {rel}")
    if errors:
        raise RuntimeError("\n".join(errors))


def inspect(root: Path) -> dict[str, str]:
    states: dict[str, str] = {}
    for rel, (pre, post) in MODIFIED.items():
        path = root / rel
        if not path.is_file():
            states[rel] = "MISSING"
            continue
        actual = sha256(path)
        if actual == pre:
            states[rel] = "PRE"
        elif actual == post:
            states[rel] = "POST"
        else:
            states[rel] = f"UNKNOWN:{actual}"
    for rel, post in NEW_FILES.items():
        path = root / rel
        if not path.exists():
            states[rel] = "ABSENT"
        elif path.is_file() and sha256(path) == post:
            states[rel] = "POST"
        else:
            actual = sha256(path) if path.is_file() else "NOT_A_FILE"
            states[rel] = f"UNKNOWN:{actual}"
    return states


def overall_state(states: dict[str, str]) -> str:
    modified = {states[rel] for rel in MODIFIED}
    created = {states[rel] for rel in NEW_FILES}
    if modified == {"PRE"} and created == {"ABSENT"}:
        return "READY_TO_APPLY"
    if modified == {"POST"} and created == {"POST"}:
        return "ALREADY_APPLIED"
    return "BLOCKED_MIXED_OR_UNKNOWN"


def print_states(root: Path, states: dict[str, str]) -> None:
    print(f"G23P2_ROOT={root}")
    for rel in list(MODIFIED) + list(NEW_FILES):
        print(f"G23P2_FILE={rel}; state={states[rel]}")
    print(f"G23P2_STATE={overall_state(states)}")
    print("G23P2_SQL=IMPORT_sql/G23P2_daily_reward_atomic.sql_BEFORE_STARTUP")
    print("G23P2_COMPILE_REQUIRED=False")
    print("G23P2_RELOAD_ELUNA_ALLOWED=False")


def apply(root: Path) -> None:
    verify_package()
    before = inspect(root)
    print_states(root, before)
    state = overall_state(before)
    if state == "ALREADY_APPLIED":
        print("G23P2_APPLY=NOOP_ALREADY_POST")
        return
    require(state == "READY_TO_APPLY", "refusing apply: target is mixed, missing, or unknown")

    replaced: list[str] = []
    created: list[str] = []
    try:
        for rel in MODIFIED:
            atomic_copy(PAYLOAD / rel, root / rel)
            replaced.append(rel)
        for rel in NEW_FILES:
            atomic_copy(PAYLOAD / rel, root / rel)
            created.append(rel)
    except Exception:
        for rel in reversed(created):
            path = root / rel
            if path.is_file(): path.unlink()
        for rel in reversed(replaced):
            atomic_copy(ORIGINAL / rel, root / rel)
        raise

    after = inspect(root)
    require(overall_state(after) == "ALREADY_APPLIED", f"post-apply verification failed: {after}")
    print("G23P2_APPLY=PASS")
    print("G23P2_FINAL_STATE=POST")


def rollback(root: Path) -> None:
    verify_package()
    before = inspect(root)
    print_states(root, before)
    state = overall_state(before)
    if state == "READY_TO_APPLY":
        print("G23P2_ROLLBACK=NOOP_ALREADY_PRE")
        return
    require(state == "ALREADY_APPLIED", "refusing rollback: target is mixed or unknown")

    restored: list[str] = []
    removed: list[str] = []
    try:
        for rel in MODIFIED:
            atomic_copy(ORIGINAL / rel, root / rel)
            restored.append(rel)
        for rel in NEW_FILES:
            path = root / rel
            if path.is_file():
                path.unlink()
                removed.append(rel)
    except Exception:
        for rel in restored:
            atomic_copy(PAYLOAD / rel, root / rel)
        for rel in removed:
            atomic_copy(PAYLOAD / rel, root / rel)
        raise

    after = inspect(root)
    require(overall_state(after) == "READY_TO_APPLY", f"post-rollback verification failed: {after}")
    print("G23P2_ROLLBACK=PASS")
    print("G23P2_FINAL_STATE=PRE")
    print("G23P2_SQL_DATA=PRESERVED_INTENTIONALLY")


def selftest() -> None:
    verify_package()
    with tempfile.TemporaryDirectory(prefix="g23p2-installer-") as temp_name:
        root = Path(temp_name)
        for rel in MODIFIED:
            atomic_copy(ORIGINAL / rel, root / rel)
        require(overall_state(inspect(root)) == "READY_TO_APPLY", "fixture is not PRE")
        apply(root)
        require(overall_state(inspect(root)) == "ALREADY_APPLIED", "apply did not reach POST")
        apply(root)
        rollback(root)
        require(overall_state(inspect(root)) == "READY_TO_APPLY", "rollback did not reach PRE")
        rollback(root)

        victim = root / next(iter(MODIFIED))
        victim.write_bytes(victim.read_bytes() + b"X")
        snapshot = {rel: (root / rel).read_bytes() if (root / rel).is_file() else None
                    for rel in list(MODIFIED) + list(NEW_FILES)}
        try:
            apply(root)
        except RuntimeError:
            pass
        else:
            raise RuntimeError("mixed tree was not rejected")
        current = {rel: (root / rel).read_bytes() if (root / rel).is_file() else None
                   for rel in list(MODIFIED) + list(NEW_FILES)}
        require(snapshot == current, "mixed-tree rejection modified target")
    print("G23P2_INSTALLER_SELFTEST=PASS")


def main() -> int:
    parser = argparse.ArgumentParser(description="G23-P2 hash-locked Lua runtime installer")
    parser.add_argument("action", choices=("check", "apply", "rollback", "selftest"))
    parser.add_argument("root", nargs="?", help="runtime root, e.g. D:\\TC-Build\\bin\\RelWithDebInfo")
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
            states = inspect(root)
            print_states(root, states)
            return 0 if overall_state(states) != "BLOCKED_MIXED_OR_UNKNOWN" else 2
        if args.action == "apply":
            apply(root)
        else:
            rollback(root)
        return 0
    except Exception as error:
        print(f"G23P2_ERROR={error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
