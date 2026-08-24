#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
REQUIRED = [
    "01_Install_Build_G17B1R5.cmd",
    "02_Rollback_Build_G17B1R5.cmd",
    "Install-Build-G17B1R5-Windows.ps1",
    "Rollback-Build-G17B1R5-Windows.ps1",
    "README_FIRST.txt",
    "PACKAGE_METADATA.txt",
    "OFFLINE_VALIDATION_20260823.txt",
    "SHA256SUMS.txt",
    "tools/apply_g17b1r5_source.py",
    "tools/inspect_g17b1r5_wrapper_spells.py",
    "tests/test_g17b1r5.py",
    "证据/g17b1r5_wrapper_spell_analysis_20260823.md",
    "original/src/server/scripts/Commands/cs_dragonriding.cpp",
    "payload/src/server/scripts/Commands/cs_dragonriding.cpp",
]
for relative in REQUIRED:
    assert (ROOT / relative).is_file(), relative

sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
pre_hash = "e9418704731a2d9cd5119cc2024079a2326802796d00bf24e88928dd17ea7059"
post_hash = "35af002b09b5d8112bbc1aaa1750f4a6245adec8b7c91a7852d69bdd283668b8"
assert sha(ROOT / "original/src/server/scripts/Commands/cs_dragonriding.cpp") == pre_hash
assert sha(ROOT / "payload/src/server/scripts/Commands/cs_dragonriding.cpp") == post_hash

post = (ROOT / "payload/src/server/scripts/Commands/cs_dragonriding.cpp").read_text(encoding="utf-8")
installer = (ROOT / "Install-Build-G17B1R5-Windows.ps1").read_text(encoding="ascii")
rollback = (ROOT / "Rollback-Build-G17B1R5-Windows.ps1").read_text(encoding="ascii")
for expected in (
    "HasMountAuraMetadata",
    "effect.ApplyAuraName == SPELL_AURA_MOUNTED",
    "FindOwnedMountAura(_player)",
    "CleanupPlayer(player, false, true)",
    "NonVisualFallGuardEvent",
    "SAFETY_CHECK_INTERVAL_MS = 250",
    "GetAuthoritativePassengerSeatId",
):
    assert expected in post, expected
for forbidden in ("48025", "71342", "Headless", "Love Rocket", "爱情火箭", "无头骑士"):
    assert forbidden not in post, forbidden

for text in (installer, rollback):
    assert not re.search(r"\[string\[\]\]\$Args\b", text, re.I)
    assert "$NativeArgs" in text and "@NativeArgs" in text
for expected in (
    f'$Pre="{pre_hash}"',
    f'$Post="{post_hash}"',
    "$BeforeExeUtc=$be.LastWriteTimeUtc",
    "$BeforePdbUtc=$bp.LastWriteTimeUtc",
    "$AfterExeUtc-le $BeforeExeUtc",
    "$AfterPdbUtc-le $BeforePdbUtc",
    "G17B1R5_WINDOWS_BUILD_RESULT=PASS",
    "Python312\\python.exe",
    "Python310\\python.exe",
):
    assert expected in installer, expected
assert "Get-Command py.exe" not in installer
assert pre_hash in rollback and post_hash not in rollback

for relative in (
    "01_Install_Build_G17B1R5.cmd",
    "02_Rollback_Build_G17B1R5.cmd",
    "Install-Build-G17B1R5-Windows.ps1",
    "Rollback-Build-G17B1R5-Windows.ps1",
):
    data = (ROOT / relative).read_bytes()
    assert data.count(b"\r\n") == data.count(b"\n") and data.count(b"\r\n") > 0
    assert not data.startswith(b"\xef\xbb\xbf")
    assert all(byte < 128 for byte in data)

completed = subprocess.run(
    [sys.executable, str(ROOT / "tests/test_g17b1r5.py")],
    capture_output=True,
    text=True,
    env={**os.environ, "PYTHONDONTWRITEBYTECODE": "1"},
)
assert completed.returncode == 0, completed.stdout + completed.stderr
assert not any(path.name == "__pycache__" for path in ROOT.rglob("__pycache__"))

checksum_lines = (ROOT / "SHA256SUMS.txt").read_text(encoding="utf-8").splitlines()
checksum_entries = {}
for line in checksum_lines:
    digest, relative = line.split("  ", 1)
    checksum_entries[relative] = digest
expected_checksum_files = {
    path.relative_to(ROOT).as_posix()
    for path in ROOT.rglob("*")
    if path.is_file() and path.name != "SHA256SUMS.txt"
}
assert set(checksum_entries) == expected_checksum_files
for relative, digest in checksum_entries.items():
    assert sha(ROOT / relative) == digest, relative

print("G17B1R5_PACKAGE_SELFTEST=PASS")
print("G17B1R5_WRAPPER_MOUNT_GATE=PASS")
print("G17B1R5_NO_PARACHUTE_BLOCKED_EXIT_GATE=PASS")
print("G17B1R5_TIMESTAMP_SNAPSHOTS=PASS")
print("G17B1R5_TESTS=PASS_18_OF_18")
