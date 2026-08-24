#!/usr/bin/env python3
"""Exact-hash source installer for the independent G17-R1 runtime fix."""

from __future__ import annotations

import argparse
import hashlib
import os
import shutil
import sys
import tempfile
from pathlib import Path

SOURCE_RELATIVE = Path("src/server/scripts/Commands/cs_dragonriding.cpp")
PRE_SHA256 = "c9535dca3390ece6735e6ff6b7418ed99ff206628b5e8febd7b78b05cba999bd"
POST_SHA256 = "10a7002db4c173e441836870da01dd3009a7ba470369ca1a89bdb399ee9b2f45"
BACKUP_SUFFIX = ".g17r1_before_20260822.bak"


class InstallError(RuntimeError):
    pass


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def batch_root() -> Path:
    return Path(__file__).resolve().parents[1]


def original_path() -> Path:
    return batch_root() / "original" / SOURCE_RELATIVE


def payload_path() -> Path:
    return batch_root() / "payload" / SOURCE_RELATIVE


def verify_package() -> tuple[bytes, bytes]:
    original = original_path().read_bytes()
    payload = payload_path().read_bytes()
    if digest(original) != PRE_SHA256:
        raise InstallError("packaged original hash mismatch")
    if digest(payload) != POST_SHA256:
        raise InstallError("packaged payload hash mismatch")
    if original == payload:
        raise InstallError("packaged original and payload are identical")
    required_counts = {
        b"class VerifyBoardingEvent : public BasicEvent": 1,
        b"GetControllableSeatId": 2,
        b"CalculateTime(250ms)": 1,
        b"G17R1 boarding verified": 1,
        b"G17R1 boarding verification failed": 1,
        b"constexpr uint32 BREATH_ENERGY_COST": 1,
    }
    for marker, expected_count in required_counts.items():
        if payload.count(marker) != expected_count:
            raise InstallError(f"payload marker count mismatch: {marker!r}")
    if b"if (player->GetVehicleBase() != dragon)" in payload:
        raise InstallError("immediate asynchronous false-failure check remains in payload")
    return original, payload


def target_paths(source_root: Path) -> tuple[Path, Path]:
    target = source_root.resolve() / SOURCE_RELATIVE
    return target, target.with_name(target.name + BACKUP_SUFFIX)


def atomic_write(path: Path, data: bytes) -> None:
    fd, temporary_name = tempfile.mkstemp(prefix=path.name + ".", suffix=".tmp", dir=path.parent)
    temporary = Path(temporary_name)
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    except Exception:
        temporary.unlink(missing_ok=True)
        raise


def check(source_root: Path) -> None:
    verify_package()
    target, backup = target_paths(source_root)
    if not target.is_file():
        raise InstallError(f"target source missing: {target}")
    target_hash = digest(target.read_bytes())
    print(f"TARGET={target}")
    print(f"TARGET_SHA256={target_hash}")
    print(f"BACKUP={backup}")
    if backup.exists():
        print(f"BACKUP_SHA256={digest(backup.read_bytes())}")
    if target_hash == POST_SHA256:
        print("G17R1_SOURCE_STATE=POSTIMAGE")
    elif target_hash == PRE_SHA256:
        print("G17R1_SOURCE_STATE=PREIMAGE")
    else:
        raise InstallError("target is neither the locked G17-B0 preimage nor G17-R1 postimage")
    print("G17R1_SOURCE_CHECK=PASS")


def apply(source_root: Path) -> None:
    original, payload = verify_package()
    target, backup = target_paths(source_root)
    if not target.is_file():
        raise InstallError(f"target source missing: {target}")
    current = target.read_bytes()
    current_hash = digest(current)
    print(f"TARGET={target}")
    print(f"PRE_APPLY_SHA256={current_hash}")

    if current_hash == POST_SHA256:
        if backup.exists() and digest(backup.read_bytes()) != PRE_SHA256:
            raise InstallError("postimage is installed but rollback backup has an unexpected hash")
        print("G17R1_SOURCE_APPLY=ALREADY_CURRENT")
        print("POST_APPLY_SHA256=" + POST_SHA256)
        return
    if current_hash != PRE_SHA256 or current != original:
        raise InstallError("target does not match the locked G17-B0 preimage; refusing to overwrite")

    if backup.exists():
        if digest(backup.read_bytes()) != PRE_SHA256:
            raise InstallError(f"refusing to overwrite different backup: {backup}")
    else:
        shutil.copy2(target, backup)
        if digest(backup.read_bytes()) != PRE_SHA256:
            raise InstallError("backup verification failed")

    atomic_write(target, payload)
    if digest(target.read_bytes()) != POST_SHA256:
        atomic_write(target, original)
        raise InstallError("post-write verification failed; original source restored")

    print(f"BACKUP={backup}")
    print("BACKUP_SHA256=" + PRE_SHA256)
    print("POST_APPLY_SHA256=" + POST_SHA256)
    print("G17R1_SOURCE_APPLY=PASS")


def rollback(source_root: Path) -> None:
    original, _ = verify_package()
    target, backup = target_paths(source_root)
    if not target.is_file():
        raise InstallError(f"target source missing: {target}")
    current_hash = digest(target.read_bytes())
    if current_hash == PRE_SHA256:
        print("G17R1_SOURCE_ROLLBACK=ALREADY_PREIMAGE")
        return
    if current_hash != POST_SHA256:
        raise InstallError("target is not the locked G17-R1 postimage; refusing rollback")
    if not backup.is_file() or digest(backup.read_bytes()) != PRE_SHA256:
        raise InstallError("verified G17-R1 rollback backup is missing")
    atomic_write(target, original)
    if digest(target.read_bytes()) != PRE_SHA256:
        raise InstallError("rollback verification failed")
    print("G17R1_SOURCE_ROLLBACK=PASS")
    print("ROLLED_BACK_SHA256=" + PRE_SHA256)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("check", "apply", "rollback"))
    parser.add_argument("--source-root", type=Path, default=Path(r"D:\TrinityCore"))
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        if args.mode == "check":
            check(args.source_root)
        elif args.mode == "apply":
            apply(args.source_root)
        else:
            rollback(args.source_root)
        return 0
    except (InstallError, OSError) as exc:
        print(f"G17R1_SOURCE_ERROR={exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
