#!/usr/bin/env python3
"""G17-B0 source installer: exact preimage + structural anchors + atomic rollback."""
from __future__ import annotations

import argparse
import hashlib
import os
import shutil
import sys
import tempfile
from pathlib import Path

PACKAGE = Path(__file__).resolve().parent
DEFAULT_SOURCE = Path(r"D:\TrinityCore")
LOADER_REL = Path("src/server/scripts/Commands/cs_script_loader.cpp")
TARGET_REL = Path("src/server/scripts/Commands/cs_dragonriding.cpp")
PREIMAGE_ROOT = PACKAGE / "original" / "windows_preimage_20260822"
PRE_LOADER = PREIMAGE_ROOT / LOADER_REL
POST_LOADER = PACKAGE / "postimage" / LOADER_REL
PAYLOAD = PACKAGE / "payload" / TARGET_REL
ABSENT_MARKER = PACKAGE / "original" / "src/server/scripts/Commands/cs_dragonriding.cpp.absent"
APPLY_APPROVAL = PACKAGE / "G17B0_APPLY_APPROVED.txt"
BACKUP_SUFFIX = ".before_g17b0.bak"

PRE_LOADER_SHA256 = "2a4895a32532f3c6c2c6dc3096fced4bff6d53c39dd3787bd81a76653d42f3f7"
POST_LOADER_SHA256 = "5502e5b4e22535957f3db81083530b048ec33f6852f4697fbe55795628cee5cc"
PAYLOAD_SHA256 = "c9535dca3390ece6735e6ff6b7418ed99ff206628b5e8febd7b78b05cba999bd"

DECL_ANCHOR = b"void AddSC_wp_commandscript();\r\n"
CALL_ANCHOR = b"    AddSC_wp_commandscript();\r\n"
DECL_INSERT = b"void AddSC_dragonriding_commandscript();\r\n"
CALL_INSERT = b"    AddSC_dragonriding_commandscript();\r\n"

# Read-only compatibility files captured by the user's Windows probe. They are
# never edited, but any drift invalidates this locked installer.
CONTEXT_SHA256 = {
    Path("src/server/scripts/ScriptLoader.h"): "6b617f0aafa6218a618e10bbff49b054991406e7ee195c12c744423418f6a0ee",
    Path("src/server/scripts/CMakeLists.txt"): "fdf785cdc99e125ee0dd7cffd4c924c6343f88924cd79e3c8d0f78d6b6bbbfbe",
    Path("src/server/game/AI/CoreAI/CombatAI.h"): "43b5650727ab7978cf7091498237acba54506571e815e947395df3243ba5780f",
    Path("src/server/game/Scripting/ScriptMgr.h"): "60c4dda3bc2753fdbdbc951ce417d14016d56c8b0e3521d388d80dba8276e6d3",
    Path("src/server/game/Entities/Unit/Unit.h"): "aaf9e8348ace53c5944e95b6a0e7ae02fd0f376bf5af709eedd3f864b340cfff",
    Path("src/server/game/Maps/Map.h"): "e5a731410d2b21fb614aed6a79b4bce29449ac1608593e68de102ff8c58bd65b",
    Path("sql/base/dev/world_database.sql"): "f766eac917bcbc1483f6f875b13733b54415ddcf11a45f4ff9e12684f646ea00",
}


class G17B0Error(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise G17B0Error(message)


def sha_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def transform_loader(pre: bytes) -> bytes:
    require(pre.count(DECL_ANCHOR) == 1, "declaration anchor is not unique")
    require(pre.count(CALL_ANCHOR) == 1, "call anchor is not unique")
    require(pre.count(DECL_INSERT) == 0, "G17-B0 declaration already present in preimage")
    require(pre.count(CALL_INSERT) == 0, "G17-B0 call already present in preimage")
    post = pre.replace(DECL_ANCHOR, DECL_ANCHOR + DECL_INSERT, 1)
    post = post.replace(CALL_ANCHOR, CALL_ANCHOR + CALL_INSERT, 1)
    return post


def validate_package() -> None:
    require(ABSENT_MARKER.read_text(encoding="utf-8").strip() == "ABSENT_BEFORE_G17B0",
            "new-file absent marker missing or invalid")
    require(PRE_LOADER.is_file(), f"package preimage missing: {PRE_LOADER}")
    require(POST_LOADER.is_file(), f"package postimage missing: {POST_LOADER}")
    require(PAYLOAD.is_file(), f"package payload missing: {PAYLOAD}")
    require(sha_file(PRE_LOADER) == PRE_LOADER_SHA256, "package loader preimage hash mismatch")
    require(sha_file(POST_LOADER) == POST_LOADER_SHA256, "package loader postimage hash mismatch")
    require(sha_file(PAYLOAD) == PAYLOAD_SHA256, "package C++ payload hash mismatch")
    require(transform_loader(PRE_LOADER.read_bytes()) == POST_LOADER.read_bytes(),
            "package postimage is not the exact structural transform of preimage")
    for rel, expected in CONTEXT_SHA256.items():
        captured = PREIMAGE_ROOT / rel
        require(captured.is_file(), f"captured context missing: {rel}")
        require(sha_file(captured) == expected, f"captured context hash mismatch: {rel}")


def source_paths(root: Path) -> tuple[Path, Path]:
    require((root / ".git").exists(), f"not a TrinityCore git root: {root}")
    loader = root / LOADER_REL
    target = root / TARGET_REL
    require(loader.is_file(), f"loader missing: {loader}")
    require(target.parent.is_dir(), f"Commands directory missing: {target.parent}")
    return loader, target


def validate_context(root: Path) -> dict[str, str]:
    actual: dict[str, str] = {}
    for rel, expected in CONTEXT_SHA256.items():
        path = root / rel
        require(path.is_file(), f"locked compatibility file missing: {rel}")
        digest = sha_file(path)
        actual[str(rel)] = digest
        require(digest == expected,
                f"locked compatibility file drifted; rerun narrow probe: {rel} actual={digest}")
    return actual


def classify(loader: Path, target: Path) -> tuple[str, str, str]:
    loader_sha = sha_file(loader)
    target_sha = sha_file(target) if target.is_file() else "ABSENT"
    loader_data = loader.read_bytes()

    if loader_sha == PRE_LOADER_SHA256 and target_sha == "ABSENT":
        require(loader_data.count(DECL_ANCHOR) == 1 and loader_data.count(CALL_ANCHOR) == 1,
                "preimage wp anchors are not unique")
        require(loader_data.count(DECL_INSERT) == 0 and loader_data.count(CALL_INSERT) == 0,
                "preimage unexpectedly contains G17-B0 anchors")
        return "ready", loader_sha, target_sha

    if loader_sha == POST_LOADER_SHA256 and target_sha == PAYLOAD_SHA256:
        require(loader_data.count(DECL_ANCHOR) == 1 and loader_data.count(CALL_ANCHOR) == 1,
                "postimage wp anchors are not unique")
        require(loader_data.count(DECL_INSERT) == 1 and loader_data.count(CALL_INSERT) == 1,
                "postimage G17-B0 anchors are not unique")
        return "applied", loader_sha, target_sha

    raise G17B0Error(
        "unknown/mixed source state; no edits made: "
        f"loader={loader_sha} target={target_sha}"
    )


def atomic_write(path: Path, data: bytes, tag: str) -> None:
    temp = path.with_name(path.name + f".{tag}.tmp")
    try:
        temp.write_bytes(data)
        if path.exists():
            try:
                os.chmod(temp, path.stat().st_mode)
            except OSError:
                pass
        os.replace(temp, path)
    finally:
        if temp.exists():
            temp.unlink()


def snapshot(root: Path, loader: Path, target: Path) -> dict[str, object]:
    values: dict[str, object] = {
        "loader": sha_file(loader),
        "target_exists": target.exists(),
        "target": sha_file(target) if target.is_file() else "ABSENT",
    }
    for rel in CONTEXT_SHA256:
        path = root / rel
        values[str(rel)] = sha_file(path) if path.is_file() else "MISSING"
    return values


def check(root: Path) -> str:
    validate_package()
    loader, target = source_paths(root)
    before = snapshot(root, loader, target)
    context = validate_context(root)
    state, loader_sha, target_sha = classify(loader, target)
    after = snapshot(root, loader, target)
    require(before == after, "read-only check changed source files")
    print(f"[OK] loader state={state} sha256={loader_sha}")
    print(f"[OK] target state={state} sha256={target_sha}")
    print(f"[OK] locked_context_files={len(context)}")
    print(f"[OK] G17B0_SOURCE_STATE={'READY_TO_APPLY' if state == 'ready' else 'ALREADY_APPLIED'}")
    print("[OK] G17B0_CHECK_SOURCE_EDITS=0")
    return state


def apply(root: Path, approval_required: bool = True) -> int:
    validate_package()
    if approval_required:
        require(APPLY_APPROVAL.is_file() and
                APPLY_APPROVAL.read_text(encoding="utf-8").strip() == "G17B0_SOURCE_AND_DB_APPROVED",
                "Apply is intentionally locked until the database probe is reviewed")
    loader, target = source_paths(root)
    validate_context(root)
    state, _, _ = classify(loader, target)
    if state == "applied":
        print("[OK] G17B0_ALREADY_APPLIED=True")
        return 0

    original_loader = loader.read_bytes()
    backup = Path(str(loader) + BACKUP_SUFFIX)
    if backup.exists():
        require(sha_file(backup) == PRE_LOADER_SHA256,
                f"existing loader backup is not locked preimage: {backup}")
    else:
        shutil.copy2(loader, backup)
        require(sha_file(backup) == PRE_LOADER_SHA256, "loader backup verification failed")

    require(not target.exists(), f"new target unexpectedly exists: {target}")
    try:
        atomic_write(target, PAYLOAD.read_bytes(), "g17b0")
        atomic_write(loader, transform_loader(original_loader), "g17b0")
        state_after, loader_sha, target_sha = classify(loader, target)
        require(state_after == "applied", "source did not classify as applied after write")
        validate_context(root)
    except Exception:
        atomic_write(loader, original_loader, "g17b0_restore")
        if target.exists() and sha_file(target) == PAYLOAD_SHA256:
            target.unlink()
        raise

    print(f"[OK] applied loader sha256={loader_sha}")
    print(f"[OK] created cs_dragonriding.cpp sha256={target_sha}")
    print(f"[OK] loader_backup={backup}")
    print("[OK] G17B0_APPLY_CHANGED_FILES=2")
    print("[OK] G17B0_SOURCE_APPLY_PASS=True")
    return 0


def rollback(root: Path) -> int:
    validate_package()
    loader, target = source_paths(root)
    validate_context(root)
    state, _, _ = classify(loader, target)
    if state == "ready":
        print("[OK] G17B0_ALREADY_ROLLED_BACK=True")
        return 0

    backup = Path(str(loader) + BACKUP_SUFFIX)
    require(backup.is_file(), f"loader rollback backup missing: {backup}")
    require(sha_file(backup) == PRE_LOADER_SHA256, "loader rollback backup hash mismatch")
    require(target.is_file() and sha_file(target) == PAYLOAD_SHA256,
            "refusing to remove modified/foreign cs_dragonriding.cpp")

    current_loader = loader.read_bytes()
    current_target = target.read_bytes()
    try:
        atomic_write(loader, backup.read_bytes(), "g17b0_rollback")
        target.unlink()
        state_after, loader_sha, target_sha = classify(loader, target)
        require(state_after == "ready", "rollback did not restore locked preimage/absence")
        validate_context(root)
    except Exception:
        atomic_write(loader, current_loader, "g17b0_rollback_restore")
        if not target.exists():
            atomic_write(target, current_target, "g17b0_rollback_restore")
        raise

    print(f"[OK] restored loader sha256={loader_sha}")
    print(f"[OK] removed target state={target_sha}")
    print("[OK] G17B0_ROLLBACK_CHANGED_FILES=2")
    print("[OK] G17B0_SOURCE_ROLLBACK_PASS=True")
    return 0


def self_test() -> int:
    validate_package()
    with tempfile.TemporaryDirectory(prefix="g17b0_installer_") as td:
        root = Path(td)
        (root / ".git").mkdir()
        for rel in CONTEXT_SHA256:
            source = PREIMAGE_ROOT / rel
            destination = root / rel
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)
        loader = root / LOADER_REL
        loader.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(PRE_LOADER, loader)

        require(check(root) == "ready", "self-test preimage was not ready")
        apply(root, approval_required=False)
        require(check(root) == "applied", "self-test apply was not detected")
        rollback(root)
        require(check(root) == "ready", "self-test rollback was not detected")

        # Negative fixture 1: loader drift must fail closed.
        drifted = loader.read_bytes().replace(DECL_ANCHOR, DECL_ANCHOR + b"// drift\r\n", 1)
        atomic_write(loader, drifted, "negative")
        failed_closed = False
        try:
            classify(loader, root / TARGET_REL)
        except G17B0Error:
            failed_closed = True
        require(failed_closed, "drifted loader false-passed")
        atomic_write(loader, PRE_LOADER.read_bytes(), "negative_restore")

        # Negative fixture 2: foreign new target must fail closed.
        foreign = root / TARGET_REL
        foreign.write_text("// foreign\n", encoding="utf-8")
        failed_closed = False
        try:
            classify(loader, foreign)
        except G17B0Error:
            failed_closed = True
        require(failed_closed, "foreign target false-passed")

    print("[OK] G17B0_INSTALLER_SELF_TEST_PASS=True")
    print("[OK] G17B0_NEGATIVE_FIXTURES=2")
    return 0


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--apply", action="store_true")
    mode.add_argument("--rollback", action="store_true")
    mode.add_argument("--self-test", action="store_true")
    parser.add_argument("source_root", nargs="?", type=Path, default=DEFAULT_SOURCE)
    return parser.parse_args()


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
    except Exception as exc:
        print(f"[FAIL] G17B0_SOURCE_INSTALLER={type(exc).__name__}: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
