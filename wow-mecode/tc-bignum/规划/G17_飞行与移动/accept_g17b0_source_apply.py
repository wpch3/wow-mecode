#!/usr/bin/env python3
"""Offline acceptance for the immutable Windows G17-B0 source Apply result."""
from __future__ import annotations

from hashlib import sha256
from pathlib import Path

HERE = Path(__file__).resolve().parent
RESULT = HERE / "证据/G17B0_SOURCE_APPLY_RESULT_20260822.txt"
EXPECTED_SIZE = 1668
EXPECTED_SHA256 = "772f4007cd2199ec6829bbb2bdef3e11a976c072c2c1ad7ae6b31210d3bc5ca4"
LOADER_PRE = "2a4895a32532f3c6c2c6dc3096fced4bff6d53c39dd3787bd81a76653d42f3f7"
LOADER_POST = "5502e5b4e22535957f3db81083530b048ec33f6852f4697fbe55795628cee5cc"
PAYLOAD = "c9535dca3390ece6735e6ff6b7418ed99ff206628b5e8febd7b78b05cba999bd"


def require(ok: bool, message: str) -> None:
    if not ok:
        raise AssertionError(message)


data = RESULT.read_bytes()
require(len(data) == EXPECTED_SIZE, "result size")
require(sha256(data).hexdigest() == EXPECTED_SHA256, "result hash")
require(b"\r\n" in data and b"\n" not in data.replace(b"\r\n", b""), "pure CRLF")
text = data.decode("ascii")
lines = text.splitlines()
require(len(lines) == 30, f"line count: {len(lines)}")
require(lines[0] == "G17B0_SOURCE_APPLY_WRAPPER_BEGIN", "wrapper begin")
require(lines[-1] == "G17B0_SOURCE_APPLY_WRAPPER_END", "wrapper end")
require("[FAIL]" not in text and "=False" not in text, "failure marker")

expected_once = (
    "SOURCE_ROOT=D:\\TrinityCore",
    "[OK] G17B0_SOURCE_AND_DB_APPROVED=True",
    f"[OK] loader state=ready sha256={LOADER_PRE}",
    "[OK] target state=ready sha256=ABSENT",
    "[OK] G17B0_SOURCE_STATE=READY_TO_APPLY",
    "[OK] G17B0_SOURCE_STATE_BEFORE=READY",
    f"[OK] applied loader sha256={LOADER_POST}",
    f"[OK] created cs_dragonriding.cpp sha256={PAYLOAD}",
    f"[OK] loader state=applied sha256={LOADER_POST}",
    f"[OK] target state=applied sha256={PAYLOAD}",
    "[OK] G17B0_SOURCE_STATE=ALREADY_APPLIED",
    f"[OK] G17B0_LOADER_POST_SHA256={LOADER_POST}",
    f"[OK] G17B0_PAYLOAD_SHA256={PAYLOAD}",
    "[OK] G17B0_FINAL_SOURCE_STATE=ALREADY_APPLIED",
    "G17B0_SOURCE_APPLY_BEGIN",
    "G17B0_SOURCE_APPLY_END",
    "G17B0_SOURCE_APPLY_WRAPPER_PASS=True",
)
for marker in expected_once:
    require(lines.count(marker) == 1, f"expected once: {marker}")

require(lines.count("[OK] locked_context_files=7") == 2, "context check count")
require(lines.count("[OK] G17B0_CHECK_SOURCE_EDITS=0") == 2, "read-only postcheck count")
require(lines.count("[OK] G17B0_APPLY_CHANGED_FILES=2") == 1, "installer changed files")
require(lines.count("[OK] G17B0_SOURCE_APPLY_CHANGED_FILES=2") == 1, "wrapper changed files")
require(lines.count("[OK] G17B0_SOURCE_APPLY_PASS=True") == 2, "Apply pass count")
backup = "D:\\TrinityCore\\src\\server\\scripts\\Commands\\cs_script_loader.cpp.before_g17b0.bak"
require(lines.count(f"[OK] loader_backup={backup}") == 1, "installer backup")
require(lines.count(f"[OK] G17B0_LOADER_BACKUP={backup}") == 1, "wrapper backup")
require(lines[1].endswith("Python312\\python.exe"), "Python 3.12 path")

print("G17B0_SOURCE_APPLY_RESULT_SIZE=1668")
print(f"G17B0_SOURCE_APPLY_RESULT_SHA256={EXPECTED_SHA256}")
print("G17B0_SOURCE_STATE_BEFORE=READY")
print("G17B0_SOURCE_APPLY_CHANGED_FILES=2")
print("G17B0_LOCKED_CONTEXT_FILES=7")
print(f"G17B0_LOADER_POST_SHA256={LOADER_POST}")
print(f"G17B0_PAYLOAD_SHA256={PAYLOAD}")
print("G17B0_FINAL_SOURCE_STATE=ALREADY_APPLIED")
print("G17B0_SOURCE_APPLY_WRAPPER_PASS=True")
print("G17B0_WINDOWS_SOURCE_APPLY_ACCEPTANCE=PASS")
