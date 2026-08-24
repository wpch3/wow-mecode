#!/usr/bin/env python3
"""F44R1 buff-lifecycle/healing-priority installer: exact check/apply/rollback/self-test."""
from __future__ import annotations

import argparse
import hashlib
import os
import shutil
import sys
import tempfile
from pathlib import Path

FILES = {
    "cs_combathelper.cpp": {
        "pre": "5f31a5b97fa0ffe99d1472b370660dc86164ddf3f402bc16b030ee47418f31da",
        "post": "209cd709d3f649261032a22f54407601dd30953984c23c8464cea6a939c4d4fc",
    },
    "CombatSpecData.h": {
        "pre": "af3e9c2575b725fa50e7bee921d1f7612546e04b967cb2da149e4fe6a2bdd4a7",
        "post": "2f471943f67b4188ddee240114d0b9d6056225f1a81c45e73618098a3b49b74d",
    },
    "CombatSpecData.cpp": {
        "pre": "1cdd6d3a8b07a4915de60746c04848f00bae2608a9c9cd25982d6f6e6a14272e",
        "post": "d7bc0532ba16f2ead2ff5af81ee12329d70e172a0005afac302fa575cf224e56",
    },
}
REL_TARGET = Path("src/server/scripts/Commands")
BACKUP_SUFFIX = ".before_f44r1.bak"
PACKAGE = Path(__file__).resolve().parent
SOURCE = PACKAGE / "源文件"
ORIGINALS = PACKAGE / "原始文件" / "F44_真人缺陷版"


class F44R1Error(RuntimeError):
    pass


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise F44R1Error(message)


def validate_package() -> None:
    for name, hashes in FILES.items():
        path = SOURCE / name
        require(path.is_file(), f"package missing: {path}")
        actual = sha256(path)
        require(actual == hashes["post"],
                f"package postimage hash mismatch: {name} actual={actual}")


def target_paths(root: Path) -> dict[str, Path]:
    directory = root / REL_TARGET
    require(directory.is_dir(), f"TrinityCore Commands directory not found: {directory}")
    result = {name: directory / name for name in FILES}
    for name, path in result.items():
        require(path.is_file(), f"target missing: {name} path={path}")
    return result


def classify(paths: dict[str, Path]) -> tuple[str, dict[str, str]]:
    actual = {name: sha256(path) for name, path in paths.items()}
    pre = all(actual[name] == FILES[name]["pre"] for name in FILES)
    post = all(actual[name] == FILES[name]["post"] for name in FILES)
    if pre:
        return "ready", actual
    if post:
        return "applied", actual
    details = ", ".join(f"{name}={digest}" for name, digest in actual.items())
    raise F44R1Error(f"mixed/unknown source snapshot; no edits made: {details}")


def atomic_write(path: Path, data: bytes) -> None:
    temporary = path.with_name(path.name + ".f44r1.tmp")
    try:
        temporary.write_bytes(data)
        os.replace(temporary, path)
    finally:
        if temporary.exists():
            temporary.unlink()


def check(root: Path) -> str:
    validate_package()
    paths = target_paths(root)
    before = {name: sha256(path) for name, path in paths.items()}
    state, actual = classify(paths)
    after = {name: sha256(path) for name, path in paths.items()}
    require(before == after, "read-only check changed source hashes")
    for name in FILES:
        print(f"[OK] {name} state={state} sha256={actual[name]}")
    print(f"[OK] CHECK_STATE={state}")
    print("[OK] SOURCE_EDITS=0")
    return state


def apply(root: Path) -> int:
    validate_package()
    paths = target_paths(root)
    state, _ = classify(paths)
    if state == "applied":
        print("[OK] F44R1_ALREADY_APPLIED=True")
        return 0

    originals = {name: path.read_bytes() for name, path in paths.items()}
    for name, path in paths.items():
        backup = Path(str(path) + BACKUP_SUFFIX)
        if backup.exists():
            require(sha256(backup) == FILES[name]["pre"],
                    f"existing backup is not locked preimage: {backup}")
        else:
            shutil.copy2(path, backup)
            require(sha256(backup) == FILES[name]["pre"],
                    f"backup verification failed: {backup}")

    try:
        for name, path in paths.items():
            atomic_write(path, (SOURCE / name).read_bytes())
        state_after, actual = classify(paths)
        require(state_after == "applied", "post-write source did not classify as applied")
    except Exception:
        for name, path in paths.items():
            atomic_write(path, originals[name])
        raise

    for name in FILES:
        print(f"[OK] applied {name} sha256={actual[name]}")
    print("[OK] APPLY_CHANGED_FILES=3")
    print("[OK] F44R1_APPLY_PASS=True")
    return 0


def rollback(root: Path) -> int:
    validate_package()
    paths = target_paths(root)
    state, _ = classify(paths)
    if state == "ready":
        print("[OK] F44R1_ALREADY_ROLLED_BACK=True")
        return 0

    backups: dict[str, Path] = {}
    for name, path in paths.items():
        backup = Path(str(path) + BACKUP_SUFFIX)
        require(backup.is_file(), f"rollback backup missing: {backup}")
        require(sha256(backup) == FILES[name]["pre"],
                f"rollback backup hash mismatch: {backup}")
        backups[name] = backup

    current = {name: path.read_bytes() for name, path in paths.items()}
    try:
        for name, path in paths.items():
            atomic_write(path, backups[name].read_bytes())
        state_after, actual = classify(paths)
        require(state_after == "ready", "rollback did not restore locked preimage")
    except Exception:
        for name, path in paths.items():
            atomic_write(path, current[name])
        raise

    for name in FILES:
        print(f"[OK] restored {name} sha256={actual[name]}")
    print("[OK] ROLLBACK_CHANGED_FILES=3")
    print("[OK] F44R1_ROLLBACK_PASS=True")
    return 0


def self_test() -> int:
    validate_package()
    with tempfile.TemporaryDirectory(prefix="f44r1_selftest_") as td:
        root = Path(td)
        target = root / REL_TARGET
        target.mkdir(parents=True)
        for name, hashes in FILES.items():
            original = ORIGINALS / name
            require(original.is_file(), f"self-test original missing: {original}")
            require(sha256(original) == hashes["pre"],
                    f"self-test original hash mismatch: {name}")
            shutil.copy2(original, target / name)

        require(check(root) == "ready", "fixture was not ready")
        apply(root)
        require(check(root) == "applied", "fixture was not applied")
        rollback(root)
        require(check(root) == "ready", "fixture rollback failed")

        # Negative fixture: mixed pre/post must fail closed.
        shutil.copy2(SOURCE / "CombatSpecData.h", target / "CombatSpecData.h")
        failed_closed = False
        try:
            classify(target_paths(root))
        except F44R1Error:
            failed_closed = True
        require(failed_closed, "mixed snapshot negative fixture false-passed")

    print("[OK] F44R1_INSTALLER_SELF_TEST_PASS=True")
    return 0


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--apply", action="store_true")
    mode.add_argument("--rollback", action="store_true")
    mode.add_argument("--self-test", action="store_true")
    parser.add_argument("source_root", nargs="?", type=Path)
    args = parser.parse_args()
    if not args.self_test and args.source_root is None:
        parser.error("source_root is required for --check/--apply/--rollback")
    return args


def main() -> int:
    args = parse_args()
    try:
        if args.self_test:
            return self_test()
        root = args.source_root.resolve()
        if args.check:
            check(root)
            return 0
        if args.apply:
            return apply(root)
        return rollback(root)
    except F44R1Error as exc:
        print(f"[FAIL] {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
