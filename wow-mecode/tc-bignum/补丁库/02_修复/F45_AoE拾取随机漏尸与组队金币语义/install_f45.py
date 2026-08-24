#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""F45 AoE Loot hash-locked installer.

Usage:
  python install_f45.py check <wow-mecode-root>
  python install_f45.py apply <wow-mecode-root>
  python install_f45.py rollback <wow-mecode-root>
  python install_f45.py selftest
"""

from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import shutil
import sys
import tempfile

BATCH = "F45_AOE_LOOT_RELIABILITY"
FILES = {
    "src/server/game/Handlers/LootHandler.cpp": {
        "pre": "35ef90116a1eaf8f3a847fc4e3b4b5c9815afe6f973433d42a56ccd755d436c5",
        "post": "8003bc7e3e7343c7383c9039f9ed8602d7d83e0561a45de1109522f7130ac124",
    },
    "src/server/game/Custom/CustomAoELoot.cpp": {
        "pre": "9a88008a895eb3b70eddbf8fe81d7cbefcd0cc285eb18bf460bc06c086af95c5",
        "post": "3492330facc3ea250be60e9496a60eab1df4d6a1a05e31179bd527713c8b1cda",
    },
    "src/server/game/Custom/CustomAoELoot.h": {
        "pre": "4cb423d0f854406f6aadbb7421af3782a90c1a8262d892ee3935e535ec5af6c6",
        "post": "11428566fc7c4a79ab20a65f639b1ec0d287a212a5fc639c27b6d040d8339e31",
    },
}

HERE = Path(__file__).resolve().parent
ORIGINAL = HERE / "original"
PAYLOAD = HERE / "payload"


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def verify_package() -> None:
    errors: list[str] = []
    for rel, expected in FILES.items():
        for side, root in (("pre", ORIGINAL), ("post", PAYLOAD)):
            path = root / rel
            if not path.is_file():
                errors.append(f"missing package file: {path}")
                continue
            actual = sha256(path)
            if actual != expected[side]:
                errors.append(
                    f"package {side} hash mismatch: {rel}\n"
                    f"  expected={expected[side]}\n  actual={actual}"
                )
    if errors:
        raise RuntimeError("\n".join(errors))


def inspect(root: Path) -> dict[str, str]:
    states: dict[str, str] = {}
    for rel, expected in FILES.items():
        path = root / rel
        if not path.is_file():
            states[rel] = "MISSING"
            continue
        actual = sha256(path)
        if actual == expected["pre"]:
            states[rel] = "PRE"
        elif actual == expected["post"]:
            states[rel] = "POST"
        else:
            states[rel] = f"UNKNOWN:{actual}"
    return states


def print_states(root: Path, states: dict[str, str]) -> None:
    print(f"F45_ROOT={root}")
    for rel in FILES:
        print(f"F45_FILE={rel}; state={states[rel]}")
    values = set(states.values())
    if values == {"PRE"}:
        overall = "READY_TO_APPLY"
    elif values == {"POST"}:
        overall = "ALREADY_APPLIED"
    else:
        overall = "BLOCKED_MIXED_OR_UNKNOWN"
    print(f"F45_STATE={overall}")


def atomic_copy(source: Path, target: Path) -> None:
    target.parent.mkdir(parents=True, exist_ok=True)
    fd, temp_name = tempfile.mkstemp(prefix=f".{target.name}.f45.", dir=target.parent)
    os.close(fd)
    temp = Path(temp_name)
    try:
        shutil.copyfile(source, temp)
        os.replace(temp, target)
    finally:
        if temp.exists():
            temp.unlink()


def install(root: Path, direction: str) -> None:
    verify_package()
    states = inspect(root)
    print_states(root, states)

    expected_state = "PRE" if direction == "apply" else "POST"
    final_state = "POST" if direction == "apply" else "PRE"
    source_root = PAYLOAD if direction == "apply" else ORIGINAL

    if set(states.values()) == {final_state}:
        print(f"F45_{direction.upper()}=NOOP_ALREADY_{final_state}")
        return
    if set(states.values()) != {expected_state}:
        raise RuntimeError(
            f"refusing {direction}: all targets must be {expected_state}; "
            "no partial overwrite is allowed"
        )

    # Every package contains byte-exact originals. Copy all three as one locked unit;
    # if an unexpected I/O failure occurs, restore files already replaced.
    replaced: list[str] = []
    try:
        for rel in FILES:
            atomic_copy(source_root / rel, root / rel)
            replaced.append(rel)
    except Exception:
        recovery_root = ORIGINAL if direction == "apply" else PAYLOAD
        for rel in reversed(replaced):
            atomic_copy(recovery_root / rel, root / rel)
        raise

    final = inspect(root)
    if set(final.values()) != {final_state}:
        raise RuntimeError(f"post-{direction} verification failed: {final}")

    print(f"F45_{direction.upper()}=PASS")
    print(f"F45_FINAL_STATE={final_state}")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(f"selftest failed: {message}")


def run_selftest() -> None:
    verify_package()
    with tempfile.TemporaryDirectory(prefix="f45-installer-") as tmp:
        root = Path(tmp)
        for rel in FILES:
            atomic_copy(ORIGINAL / rel, root / rel)
        require(set(inspect(root).values()) == {"PRE"}, "fixture is not PRE")
        install(root, "apply")
        require(set(inspect(root).values()) == {"POST"}, "apply did not reach POST")
        install(root, "apply")
        install(root, "rollback")
        require(set(inspect(root).values()) == {"PRE"}, "rollback did not reach PRE")
        install(root, "rollback")

        # Unknown/mixed trees must be rejected without touching any known file.
        mixed = root / next(iter(FILES))
        mixed.write_bytes(mixed.read_bytes() + b"X")
        before = {rel: sha256(root / rel) for rel in FILES}
        try:
            install(root, "apply")
        except RuntimeError:
            pass
        else:
            raise AssertionError("mixed tree was not rejected")
        after = {rel: sha256(root / rel) for rel in FILES}
        require(before == after, "mixed-tree rejection modified files")

    print("F45_INSTALLER_SELFTEST=PASS")


def main() -> int:
    parser = argparse.ArgumentParser(description="F45 hash-locked installer")
    parser.add_argument("action", choices=("check", "apply", "rollback", "selftest"))
    parser.add_argument("root", nargs="?", help="wow-mecode/TrinityCore source root")
    args = parser.parse_args()

    try:
        if args.action == "selftest":
            run_selftest()
            return 0
        if not args.root:
            parser.error("root is required for check/apply/rollback")
        root = Path(args.root).expanduser().resolve()
        verify_package()
        if args.action == "check":
            states = inspect(root)
            print_states(root, states)
            return 0 if set(states.values()) in ({"PRE"}, {"POST"}) else 2
        install(root, args.action)
        return 0
    except Exception as error:
        print(f"F45_ERROR={error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
