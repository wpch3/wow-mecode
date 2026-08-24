#!/usr/bin/env python3
"""Exact-hash installer for G17-R2 pure-flight strict-location server fix."""

from __future__ import annotations

import argparse
import hashlib
import os
import shutil
import sys
import tempfile
from pathlib import Path

SOURCE_RELATIVE = Path("src/server/game/Spells/SpellInfo.cpp")
PRE_SHA256 = "537e5c350baa5f4a90bd0ec38c6b6858360e287aeabd75ab54050b4432e50755"
POST_SHA256 = "73d52ac0feb67a32822fc0bf086a9174ba7ef0bc186223cdc8a690f48fccb9e2"
BACKUP_SUFFIX = ".g17r2_before_20260823.bak"


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
    if original.count(b"!player->CanFlyInZone(map_id, zone_id, this)") != 1:
        raise InstallError("locked preimage no longer has the single unconditional CanFlyInZone gate")
    required = {
        b"G17-R2: the G17 old-world policy replaces both original flyable": 1,
        b"if (!areaEntry || !player)": 1,
        b"if (!g17OldWorldAllowed &&": 1,
        b"(!areaEntry->IsFlyable() || !player->CanFlyInZone(map_id, zone_id, this))": 1,
        b"G17R2 old-world pure-flight location allowed": 1,
    }
    for marker, count in required.items():
        if payload.count(marker) != count:
            raise InstallError(f"payload marker count mismatch: {marker!r}")
    forbidden = (
        b"(!areaEntry->IsFlyable() && !g17OldWorldAllowed) ||\r\n"
        b"                !player->CanFlyInZone(map_id, zone_id, this)"
    )
    if forbidden in payload:
        raise InstallError("unconditional strict CanFlyInZone gate remains")
    return original, payload


def target_paths(source_root: Path) -> tuple[Path, Path]:
    target = source_root.resolve() / SOURCE_RELATIVE
    return target, target.with_name(target.name + BACKUP_SUFFIX)


def atomic_write(path: Path, data: bytes) -> None:
    fd, temp_name = tempfile.mkstemp(prefix=path.name + ".", suffix=".tmp", dir=path.parent)
    temp = Path(temp_name)
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temp, path)
    except Exception:
        temp.unlink(missing_ok=True)
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
    if target_hash == PRE_SHA256:
        print("G17R2_SOURCE_STATE=PREIMAGE")
    elif target_hash == POST_SHA256:
        print("G17R2_SOURCE_STATE=POSTIMAGE")
    else:
        raise InstallError("target is neither locked G17-A preimage nor G17-R2 postimage")
    print("G17R2_SOURCE_CHECK=PASS")


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
            raise InstallError("postimage installed but rollback backup hash is unexpected")
        print("G17R2_SOURCE_APPLY=ALREADY_CURRENT")
        print("POST_APPLY_SHA256=" + POST_SHA256)
        return
    if current_hash != PRE_SHA256 or current != original:
        raise InstallError("target does not match locked G17-A preimage; refusing overwrite")
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
        raise InstallError("post-write verification failed; original restored")
    print(f"BACKUP={backup}")
    print("BACKUP_SHA256=" + PRE_SHA256)
    print("POST_APPLY_SHA256=" + POST_SHA256)
    print("G17R2_SOURCE_APPLY=PASS")


def rollback(source_root: Path) -> None:
    original, _ = verify_package()
    target, backup = target_paths(source_root)
    if not target.is_file():
        raise InstallError(f"target source missing: {target}")
    current_hash = digest(target.read_bytes())
    if current_hash == PRE_SHA256:
        print("G17R2_SOURCE_ROLLBACK=ALREADY_PREIMAGE")
        return
    if current_hash != POST_SHA256:
        raise InstallError("target is not locked G17-R2 postimage; refusing rollback")
    if not backup.is_file() or digest(backup.read_bytes()) != PRE_SHA256:
        raise InstallError("verified G17-R2 rollback backup missing")
    atomic_write(target, original)
    if digest(target.read_bytes()) != PRE_SHA256:
        raise InstallError("rollback verification failed")
    print("G17R2_SOURCE_ROLLBACK=PASS")
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
        print(f"G17R2_SOURCE_ERROR={exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
