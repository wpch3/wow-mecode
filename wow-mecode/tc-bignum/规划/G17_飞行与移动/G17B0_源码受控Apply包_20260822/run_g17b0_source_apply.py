#!/usr/bin/env python3
"""One-shot controlled source Apply for the approved G17-B0 preimage."""
from __future__ import annotations

import sys
import traceback
from pathlib import Path

import install_g17b0_source as installer


def main() -> int:
    root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(r"D:\TrinityCore")
    print("G17B0_SOURCE_APPLY_BEGIN")
    print(f"SOURCE_ROOT={root}")
    try:
        installer.validate_package()
        if not installer.APPLY_APPROVAL.is_file():
            raise installer.G17B0Error("approval marker missing")
        approval = installer.APPLY_APPROVAL.read_text(encoding="utf-8").strip()
        if approval != "G17B0_SOURCE_AND_DB_APPROVED":
            raise installer.G17B0Error("approval marker content mismatch")
        print("[OK] G17B0_SOURCE_AND_DB_APPROVED=True")

        loader, target = installer.source_paths(root)
        before = installer.snapshot(root, loader, target)
        state_before = installer.check(root)
        print(f"[OK] G17B0_SOURCE_STATE_BEFORE={state_before.upper()}")

        installer.apply(root, approval_required=True)

        state_after = installer.check(root)
        if state_after != "applied":
            raise installer.G17B0Error(f"final source state is not applied: {state_after}")
        after = installer.snapshot(root, loader, target)
        expected_changes = 0 if state_before == "applied" else 2
        actual_changes = int(before["loader"] != after["loader"]) + int(before["target"] != after["target"])
        if actual_changes != expected_changes:
            raise installer.G17B0Error(
                f"unexpected changed-file count: actual={actual_changes} expected={expected_changes}"
            )
        for rel in installer.CONTEXT_SHA256:
            key = str(rel)
            if before[key] != after[key]:
                raise installer.G17B0Error(f"locked context changed: {rel}")

        backup = Path(str(loader) + installer.BACKUP_SUFFIX)
        if not backup.is_file() or installer.sha_file(backup) != installer.PRE_LOADER_SHA256:
            raise installer.G17B0Error("locked loader backup missing or hash mismatch")
        if installer.sha_file(loader) != installer.POST_LOADER_SHA256:
            raise installer.G17B0Error("loader postimage hash mismatch")
        if not target.is_file() or installer.sha_file(target) != installer.PAYLOAD_SHA256:
            raise installer.G17B0Error("payload hash mismatch")

        print(f"[OK] G17B0_SOURCE_APPLY_CHANGED_FILES={actual_changes}")
        print(f"[OK] G17B0_LOADER_BACKUP={backup}")
        print(f"[OK] G17B0_LOADER_POST_SHA256={installer.POST_LOADER_SHA256}")
        print(f"[OK] G17B0_PAYLOAD_SHA256={installer.PAYLOAD_SHA256}")
        print("[OK] G17B0_FINAL_SOURCE_STATE=ALREADY_APPLIED")
        print("[OK] G17B0_SOURCE_APPLY_PASS=True")
        print("G17B0_SOURCE_APPLY_END")
        return 0
    except Exception as exc:
        print(f"[FAIL] {type(exc).__name__}: {exc}")
        traceback.print_exc()
        print("G17B0_SOURCE_APPLY_PASS=False")
        print("G17B0_SOURCE_APPLY_END")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
