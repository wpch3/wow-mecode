#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path, PurePosixPath
import hashlib
import zipfile

ROOT = Path(__file__).resolve().parents[1]
PLAN = ROOT.parents[2] / "规划/G17_飞行与移动"
PACKAGE = PLAN / "G17B0_Windows_Build_20260822"
ZIP = PLAN / "G17B0_Windows_Build_20260822.zip"
TOP = PACKAGE.name
ZIP_SIZE = 7231
ZIP_SHA256 = "c6bdef705e30ce2d395e8fc345eb42e9a86e344485f3a7cb04f408a0bc08fe38"


def require(ok: bool, message: str) -> None:
    if not ok:
        raise AssertionError(message)


ps1_path = PACKAGE / "Build-G17B0-Windows.ps1"
cmd_path = PACKAGE / "Run-G17B0-Windows-Build.cmd"
ps1 = ps1_path.read_text(encoding="utf-8")
cmd = cmd_path.read_bytes()

for token in (
    '$SourceRoot = "D:\\TrinityCore"',
    '$BuildRoot = "D:\\TC-Build"',
    '$ExpectedLoader = "5502e5b4e22535957f3db81083530b048ec33f6852f4697fbe55795628cee5cc"',
    '$ExpectedPayload = "c9535dca3390ece6735e6ff6b7418ed99ff206628b5e8febd7b78b05cba999bd"',
    'Get-Process worldserver',
    'cmake.exe', '-S $SourceRoot -B $BuildRoot',
    'Select-String -SimpleMatch "cs_dragonriding.cpp"',
    '/t:worldserver', '/p:Configuration=RelWithDebInfo', '/p:Platform=x64',
    '*dragonriding*.obj',
    'G17B0_WINDOWS_BUILD_PASS=True',
    'G17B0_WINDOWS_BUILD_RESULT=PASS',
    'G17B0_WINDOWS_BUILD_PASS=False',
    'STOP_DO_NOT_START_WORLDSERVER',
    'G17B0_WINDOWS_BUILD_RESULT.txt',
):
    require(token in ps1, f"missing build contract: {token}")

for forbidden in (
    "Start-Process $Exe", "& $Exe", "mysql.exe", "mysqld.exe",
    "G17B0_WINDOWS_BUILD=PASS",
):
    require(forbidden not in ps1, f"forbidden build behavior/claim: {forbidden}")

# Basic PowerShell delimiter check outside comments and quoted strings.
stack: list[str] = []
state = "code"
i = 0
pairs = {")": "(", "]": "[", "}": "{"}
while i < len(ps1):
    char = ps1[i]
    nxt = ps1[i + 1] if i + 1 < len(ps1) else ""
    if state == "code":
        if char == "#":
            state = "comment"
        elif char == '"':
            state = "double"
        elif char == "'":
            state = "single"
        elif char in "([{":
            stack.append(char)
        elif char in ")]}":
            require(bool(stack) and stack.pop() == pairs[char], f"delimiter at {i}")
    elif state == "comment":
        if char == "\n":
            state = "code"
    elif state == "double":
        if char == "`" and nxt:
            i += 1
        elif char == '"':
            state = "code"
    elif state == "single":
        if char == "'":
            if nxt == "'":
                i += 1
            else:
                state = "code"
    i += 1
require(state == "code" and not stack, "PowerShell quote/delimiter balance")

require(b"\r\n" in cmd, "CMD must use CRLF")
for token in (
    b"Build-G17B0-Windows.ps1", b"powershell.exe", b"G17B0_WINDOWS_BUILD_RESULT.txt",
    b"exit /b %RC%",
):
    require(token in cmd, f"CMD contract: {token!r}")

# Package internal checksums.
checksum_lines = (PACKAGE / "SHA256SUMS.txt").read_text(encoding="utf-8").splitlines()
require(len(checksum_lines) == 6, "internal checksum count")
seen: set[str] = set()
for line in checksum_lines:
    digest, rel = line.split("  ", 1)
    require(rel not in seen, "duplicate checksum path")
    seen.add(rel)
    file = PACKAGE / PurePosixPath(rel)
    require(file.is_file(), f"missing checksummed file: {rel}")
    require(hashlib.sha256(file.read_bytes()).hexdigest() == digest, f"checksum: {rel}")

# ZIP identity, CRC, path safety, and directory byte identity.
require(ZIP.stat().st_size == ZIP_SIZE, "ZIP size")
require(hashlib.sha256(ZIP.read_bytes()).hexdigest() == ZIP_SHA256, "ZIP SHA-256")
with zipfile.ZipFile(ZIP) as archive:
    require(archive.testzip() is None, "ZIP CRC")
    infos = archive.infolist()
    require(len(infos) == 7, "ZIP file count")
    for info in infos:
        pure = PurePosixPath(info.filename)
        require(not pure.is_absolute() and ".." not in pure.parts, "unsafe ZIP path")
        require(pure.parts and pure.parts[0] == TOP, "ZIP top directory")
        rel = PurePosixPath(*pure.parts[1:])
        require((PACKAGE / rel).read_bytes() == archive.read(info), f"ZIP bytes: {rel}")

print("G17B0_WINDOWS_BUILD_SCRIPT_LEXICAL=PASS")
print("G17B0_WINDOWS_BUILD_SOURCE_GATES=PASS")
print("G17B0_WINDOWS_BUILD_CMAKE_MEMBERSHIP_GATE=PASS")
print("G17B0_WINDOWS_BUILD_FRESH_OBJECT_GATE=PASS")
print("G17B0_WINDOWS_BUILD_NO_SERVER_START=PASS")
print("G17B0_WINDOWS_BUILD_INTERNAL_CHECKSUMS=PASS_6")
print("G17B0_WINDOWS_BUILD_ZIP=FINAL_PASS")
