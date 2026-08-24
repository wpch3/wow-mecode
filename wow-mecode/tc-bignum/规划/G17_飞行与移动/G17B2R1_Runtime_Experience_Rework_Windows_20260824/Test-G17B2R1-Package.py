#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path

PRE = "8b47a5b507bc281198363972e10f91ab0ed3784ad920cf810bd20eacfb6ec1d5"
POST = "ff185d9987b8f4457d8380e1c662cd0313b33a7ae4be6b82974e7702d1fdc4fc"
SAFE = "e298a856edcf366b09934c3635ea8493b6d4e529d9fa2dbf2de2bce77b5b0203"
SQL_SHA = "b4526c241e8c8a012f3463038c40d8ca251749f91305939449b1098732e88b66"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--package-root", type=Path, default=Path(__file__).resolve().parent)
    args = parser.parse_args()
    root = args.package_root.resolve()
    required = [
        "01_Install_Build_G17B2R1.cmd",
        "02_Rollback_Build_G17B2R1.cmd",
        "Install-Build-G17B2R1-Windows.ps1",
        "Rollback-Build-G17B2R1-Windows.ps1",
        "README_FIRST.txt",
        "PACKAGE_METADATA.txt",
        "SHA256SUMS.txt",
        "original/src/server/scripts/Commands/cs_dragonriding.cpp",
        "payload/src/server/scripts/Commands/cs_dragonriding.cpp",
        "rollback_safe/src/server/scripts/Commands/cs_dragonriding.cpp",
        "sql/G17B2R1_world_safety_migration.sql",
        "tests/test_g17b2r1.py",
        "tools/apply_g17b2r1_source.py",
        "证据/G17B2R1_52226_project_dbc_audit.txt",
        "证据/G17B2R1_52226_world_preimage_collision_audit.txt",
        "证据/G17B2R1_visual_kit_audit.txt",
        "证据/G17B2R1_local_validation_20260824.txt",
    ]
    for relative in required:
        require((root / relative).is_file(), f"missing package file: {relative}")

    original = root / required[7]
    payload = root / required[8]
    rollback = root / required[9]
    sql = root / required[10]
    require(sha(original) == PRE, "frozen B2 preimage SHA mismatch")
    require(sha(payload) == POST, "B2R1 payload SHA mismatch")
    require(sha(rollback) == SAFE, "B2R1 safe rollback SHA mismatch")
    require(sha(sql) == SQL_SHA, "World SQL SHA mismatch")

    active = payload.read_text(encoding="utf-8")
    safe = rollback.read_text(encoding="utf-8")
    require("53208" not in active and "14475" not in active, "known-bad ID in active source")
    require("53208" not in safe and "14475" not in safe, "known-bad ID in safe rollback")
    require("SPELL_SAFE_LANDING   = 52226" in active, "visual-free command absent")
    require("LaunchMoveSpline" in active and "MoveLand" not in active and "MoveJump" not in active,
            "custom multi-point movement contract missing")

    install = (root / required[2]).read_text(encoding="utf-8")
    rollback_ps = (root / required[3]).read_text(encoding="utf-8")
    for script in (install, rollback_ps):
        lowered = script.lower()
        require("get-command py.exe" not in lowered and "python314\\python.exe" not in lowered,
                "forbidden Python launcher/version dependency")
        require("windowsapps" in lowered, "WindowsApps exclusion missing")
        require("database=world" in lowered and "runs_sql=true" in lowered, "explicit World SQL gate missing")
        require("mysql_pwd" in lowered, "database password environment handling missing")
        require("/t:worldserver" in lowered and "fresh_objects" in lowered, "strict worldserver build gate missing")
    require('"apply"' in install, "install does not invoke source apply")
    require('"rollback"' in rollback_ps, "rollback does not invoke safe source rollback")

    for relative in required[:2]:
        data = (root / relative).read_bytes()
        require(b"%*" in data, f"argument forwarding missing: {relative}")
        require(b"powershell.exe" in data.lower(), f"PowerShell launch missing: {relative}")

    manifest = root / "SHA256SUMS.txt"
    listed: dict[str, str] = {}
    for line in manifest.read_text(encoding="utf-8").splitlines():
        digest, relative = line.split("  ", 1)
        listed[relative] = digest
    actual = sorted(p.relative_to(root).as_posix() for p in root.rglob("*")
                    if p.is_file() and p.name != "SHA256SUMS.txt")
    require(sorted(listed) == actual, "SHA256SUMS file set mismatch")
    for relative, digest in listed.items():
        require(sha(root / relative) == digest, f"SHA256 mismatch: {relative}")

    print("G17B2R1_PACKAGE_SELFTEST=PASS")
    print(f"PACKAGE_FILES={len(actual) + 1}")
    print(f"PRE_SHA256={PRE}")
    print(f"POST_SHA256={POST}")
    print(f"SAFE_ROLLBACK_SHA256={SAFE}")


if __name__ == "__main__":
    main()
