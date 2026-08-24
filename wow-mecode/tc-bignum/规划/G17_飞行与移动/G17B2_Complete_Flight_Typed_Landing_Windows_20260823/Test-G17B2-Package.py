#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent
REQUIRED = [
    "01_Install_Build_G17B2.cmd",
    "02_Rollback_Build_G17B2.cmd",
    "Install-Build-G17B2-Windows.ps1",
    "Rollback-Build-G17B2-Windows.ps1",
    "README_FIRST.txt",
    "PACKAGE_METADATA.txt",
    "OFFLINE_VALIDATION_20260823.txt",
    "SHA256SUMS.txt",
    "tools/apply_g17b2_source.py",
    "tests/test_g17b2.py",
    "证据/g17b2_local_validation_20260823.txt",
    "original/src/server/scripts/Commands/cs_dragonriding.cpp",
    "payload/src/server/scripts/Commands/cs_dragonriding.cpp",
]
for relative in REQUIRED:
    assert (ROOT / relative).is_file(), relative

sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
pre_hash = "35af002b09b5d8112bbc1aaa1750f4a6245adec8b7c91a7852d69bdd283668b8"
post_hash = "8b47a5b507bc281198363972e10f91ab0ed3784ad920cf810bd20eacfb6ec1d5"
assert sha(ROOT / "original/src/server/scripts/Commands/cs_dragonriding.cpp") == pre_hash
assert sha(ROOT / "payload/src/server/scripts/Commands/cs_dragonriding.cpp") == post_hash

post = (ROOT / "payload/src/server/scripts/Commands/cs_dragonriding.cpp").read_text(encoding="utf-8")
installer = (ROOT / "Install-Build-G17B2-Windows.ps1").read_text(encoding="ascii")
rollback = (ROOT / "Rollback-Build-G17B2-Windows.ps1").read_text(encoding="ascii")
for expected in (
    "FLIGHT_SPEED_RATES = { 2.5f, 4.0f, 6.0f, 8.0f, 10.0f, 11.0f, 12.0f }",
    "MOVEMENTFLAG_FORWARD",
    "MOVEMENTFLAG_PITCH_DOWN",
    "MoveJump(destination, CLIMB_HORIZONTAL_SPEED",
    "CompleteClimb",
    "EnterStall",
    "RecoverFromStall",
    "PreventHitDefaultEffect(effectIndex)",
    "VISUAL_KIT_MAGIC_WIND",
    "VISUAL_KIT_MECHANICAL_ROCKET",
    "StartBeastPounce",
    "case ARCHETYPE_DRAGON",
    "GetAuthoritativePassengerSeatId",
    "SAFETY_CHECK_INTERVAL_MS = 250",
    "FindOwnedMountAura(_player)",
):
    assert expected in post, expected
for forbidden in ("SPELL_FALL_SAFETY", "MoveTakeoff", "TeleportTo(", "NearTeleportTo("):
    assert forbidden not in post, forbidden
assert not re.search(r"CastSpell\([^;]*53208", post)

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
    "G17B2_WINDOWS_BUILD_RESULT=PASS",
    "Python312\\python.exe",
    "Python310\\python.exe",
    "apply_g17b2_source.py",
):
    assert expected in installer, expected
assert "Get-Command py.exe" not in installer
assert pre_hash in rollback and post_hash not in rollback

for relative in (
    "01_Install_Build_G17B2.cmd",
    "02_Rollback_Build_G17B2.cmd",
    "Install-Build-G17B2-Windows.ps1",
    "Rollback-Build-G17B2-Windows.ps1",
):
    data = (ROOT / relative).read_bytes()
    assert data.count(b"\r\n") == data.count(b"\n") and data.count(b"\r\n") > 0
    assert not data.startswith(b"\xef\xbb\xbf")
    assert all(byte < 128 for byte in data)

completed = subprocess.run(
    [sys.executable, str(ROOT / "tests/test_g17b2.py")],
    capture_output=True,
    text=True,
    env={**os.environ, "PYTHONDONTWRITEBYTECODE": "1"},
)
assert completed.returncode == 0, completed.stdout + completed.stderr
assert "Ran 33 tests" in completed.stderr

with tempfile.TemporaryDirectory(prefix="g17b2-package-") as temporary:
    source_root = Path(temporary)
    target = source_root / "src/server/scripts/Commands/cs_dragonriding.cpp"
    target.parent.mkdir(parents=True)
    shutil.copy2(ROOT / "original/src/server/scripts/Commands/cs_dragonriding.cpp", target)
    tool = ROOT / "tools/apply_g17b2_source.py"
    for command in ("check", "apply", "apply", "rollback"):
        result = subprocess.run(
            [sys.executable, str(tool), command, "--source-root", str(source_root)],
            capture_output=True,
            text=True,
            env={**os.environ, "PYTHONDONTWRITEBYTECODE": "1"},
        )
        assert result.returncode == 0, result.stdout + result.stderr
    assert sha(target) == pre_hash

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

print("G17B2_PACKAGE_SELFTEST=PASS")
print("G17B2_STRICT_IMAGES=PASS")
print("G17B2_MOMENTUM_1200_CAP_GATE=PASS")
print("G17B2_SKILL3_FORWARD_CONTROL_GATE=PASS")
print("G17B2_TYPED_NO_PARACHUTE_LANDING_GATE=PASS")
print("G17B2_B1R3_R4_R5_REGRESSION_GATES=PASS")
print("G17B2_TIMESTAMP_SNAPSHOTS=PASS")
print("G17B2_TESTS=PASS_33_OF_33")
