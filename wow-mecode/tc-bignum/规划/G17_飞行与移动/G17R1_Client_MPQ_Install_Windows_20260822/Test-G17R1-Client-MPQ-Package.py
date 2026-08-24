#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
TOOL_SHA = "5dc56b130432098d18e911fdf4b3464adf850432b7fbb35003a2feff7c30a79f"
PATCHED_SHA = "dd25091167f671764735ce88c78b66485c6d661fadf05d322574c261f6e464ea"
SERVER_SHA = "df44e75ef1730e363dc06f1bc5ae064299b08d2d0047e663c0a1782ed4c8d10f"
EXPECTED_FILES = {
    "G17R1_Runtime_Acceptance_Template.txt",
    "Install-G17R1-Client-MPQ.ps1",
    "original/NO_TARGET_FILE_PREIMAGE.txt",
    "PACKAGE_METADATA.txt",
    "README_FIRST.txt",
    "Rollback-G17R1-Client-MPQ.ps1",
    "Run-G17R1-Client-MPQ-Install.cmd",
    "Run-G17R1-Client-MPQ-Rollback.cmd",
    "SHA256SUMS.txt",
    "Test-G17R1-Client-MPQ-Package.py",
    "THIRD_PARTY_PROVENANCE.txt",
    "third_party/mpqcli-LICENSE.txt",
    "third_party/mpqcli-windows-amd64.exe",
}


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    actual = {
        p.relative_to(ROOT).as_posix()
        for p in ROOT.rglob("*")
        if p.is_file()
    }
    require(actual == EXPECTED_FILES, f"package file set mismatch: missing={sorted(EXPECTED_FILES-actual)} extra={sorted(actual-EXPECTED_FILES)}")

    tool = ROOT / "third_party/mpqcli-windows-amd64.exe"
    data = tool.read_bytes()
    require(len(data) == 970752, "mpqcli size mismatch")
    require(sha256(tool) == TOOL_SHA, "mpqcli SHA-256 mismatch")
    require(data[:2] == b"MZ", "mpqcli does not have an MZ header")
    pe_offset = int.from_bytes(data[0x3C:0x40], "little")
    require(data[pe_offset:pe_offset + 4] == b"PE\0\0", "mpqcli does not have a PE signature")

    script_names = [
        "Install-G17R1-Client-MPQ.ps1",
        "Rollback-G17R1-Client-MPQ.ps1",
        "Run-G17R1-Client-MPQ-Install.cmd",
        "Run-G17R1-Client-MPQ-Rollback.cmd",
    ]
    for name in script_names:
        raw = (ROOT / name).read_bytes()
        require(not raw.startswith(b"\xef\xbb\xbf"), f"{name} has a UTF-8 BOM")
        require(b"\x00" not in raw, f"{name} contains NUL")
        require(all(b < 128 for b in raw), f"{name} is not ASCII-safe")
        require(b"\r\n" in raw and raw.replace(b"\r\n", b"").find(b"\n") < 0, f"{name} is not CRLF-only")

    install = (ROOT / "Install-G17R1-Client-MPQ.ps1").read_text("ascii")
    rollback = (ROOT / "Rollback-G17R1-Client-MPQ.ps1").read_text("ascii")
    readme = (ROOT / "README_FIRST.txt").read_text("ascii")
    metadata = (ROOT / "PACKAGE_METADATA.txt").read_text("ascii")
    provenance = (ROOT / "THIRD_PARTY_PROVENANCE.txt").read_text("ascii")

    for token in [
        TOOL_SHA,
        PATCHED_SHA,
        SERVER_SHA,
        '"create", "--game", "wow-wotlk"',
        '"DBFilesClient\\Spell.dbc"',
        '"Z","Y","X","W"',
        'TARGET_PREIMAGE=ABSENT',
        'G17R1_CLIENT_MPQ_INSTALL_RESULT=PASS',
        'SERVER_DBC_MODIFIED=False',
        'Assert-ArchiveContent',
        'AUTO_ROLLBACK_AFTER_FAILURE=PASS',
        '("CLIENT_ROOT=" + $ClientRoot)',
        '("INSTALLED_MPQ=" + $SelectedTarget)',
        '"File doesn\'t exist"',
        'higher_target_probe_',
    ]:
        require(token in install, f"installer missing token: {token}")
    require("Move-Item -LiteralPath $TemporaryTarget -Destination $SelectedTarget" in install, "atomic target move missing")
    require("-Destination $SelectedTarget -Force" not in install, "installer can force-overwrite final target")
    require("if (-not (Test-Path -LiteralPath $Candidate))" in install, "absent-slot gate missing")
    require("$ServerSpellDbc" in install and "Copy-Item -LiteralPath $ServerSpellDbc" not in install, "server DBC write boundary violated")

    for token in [
        'Read-Host "Type ROLLBACK',
        'TARGET_POSTIMAGE=ABSENT',
        'G17R1_CLIENT_MPQ_ROLLBACK_RESULT=PASS',
        'TARGET_PREIMAGE"] -cne "ABSENT"',
        SERVER_SHA,
        PATCHED_SHA,
    ]:
        require(token in rollback, f"rollback missing token: {token}")

    for text, label in [(readme, "README"), (metadata, "metadata"), (provenance, "provenance")]:
        require(TOOL_SHA in text, f"{label} missing tool hash")
    require(PATCHED_SHA in readme and SERVER_SHA in readme, "README missing DBC hashes")
    require("v0.10.2" in provenance and "970752" in provenance and "MIT" in provenance, "provenance incomplete")

    sums_path = ROOT / "SHA256SUMS.txt"
    lines = sums_path.read_text("ascii").splitlines()
    parsed: dict[str, str] = {}
    for line in lines:
        match = re.fullmatch(r"([0-9a-f]{64})  (.+)", line)
        require(match is not None, f"bad SHA256SUMS line: {line!r}")
        digest, rel = match.groups()
        require(rel not in parsed, f"duplicate checksum path: {rel}")
        parsed[rel] = digest
    expected_sum_paths = EXPECTED_FILES - {"SHA256SUMS.txt"}
    require(set(parsed) == expected_sum_paths, "SHA256SUMS path set mismatch")
    for rel, digest in parsed.items():
        require(sha256(ROOT / rel) == digest, f"checksum mismatch: {rel}")

    require(not any(p.suffix.lower() in {".dbc", ".mpq"} for p in ROOT.rglob("*") if p.is_file()), "package must not embed client DBC or generated MPQ")
    print("G17R1_CLIENT_MPQ_PACKAGE_FILESET=PASS")
    print("G17R1_CLIENT_MPQ_TOOL_PE_SHA256=PASS")
    print("G17R1_CLIENT_MPQ_POWERSHELL_ASCII_CRLF=PASS")
    print("G17R1_CLIENT_MPQ_NONOVERWRITE_ROLLBACK_POLICY=PASS")
    print("G17R1_CLIENT_MPQ_SHA256SUMS=PASS")
    print("G17R1_CLIENT_MPQ_PACKAGE_SELFTEST=PASS")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"G17R1_CLIENT_MPQ_PACKAGE_SELFTEST=FAIL: {exc}", file=sys.stderr)
        raise
