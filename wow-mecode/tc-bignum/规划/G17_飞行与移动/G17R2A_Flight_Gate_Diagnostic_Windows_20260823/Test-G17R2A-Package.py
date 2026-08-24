#!/usr/bin/env python3
from __future__ import annotations

import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent
PS = ROOT / "Probe-G17R2A-Flight-Gates.ps1"
TOOL = ROOT / "third_party" / "mpqcli-windows-amd64.exe"
LICENSE = ROOT / "third_party" / "mpqcli-LICENSE.txt"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    required = [
        PS,
        ROOT / "Run-G17R2A-Diagnostic.cmd",
        ROOT / "README_FIRST_G17R2A.txt",
        ROOT / "THIRD_PARTY_PROVENANCE.txt",
        TOOL,
        LICENSE,
    ]
    assert all(path.is_file() for path in required)
    assert sha(TOOL) == "5dc56b130432098d18e911fdf4b3464adf850432b7fbb35003a2feff7c30a79f"
    assert sha(LICENSE) == "177eb5d7c52683d8f8857243ee1940ce5df8bf4a2abbd8d0c0b4d6dbee970080"
    data = PS.read_bytes()
    assert all(byte < 128 for byte in data)
    text = data.decode("ascii")
    for marker in (
        "G17R2_59961_SERVER_MARKER_HITS",
        "G17R1_CLIENT_EFFECTIVE_DBC_GATE",
        "G17R2A_CLASSIFICATION",
        "Probe-ArchiveTarget",
        "File doesn't exist",
        "ACTIVE_EXE_MATCHES_R2_BUILD",
        "SPELLINFO_R2_POSTIMAGE",
        "PASS_EXPECTED_PATCHED_DBC",
        "CLIENT_DBC_GATE_NOT_INSTALLED_OR_OVERRIDDEN",
        "SERVER_LOCATION_GATE_PASSED_CHECK_LATER_SERVER_GATE",
        "G17R2A_DIAGNOSTIC_RESULT=PASS",
    ):
        assert marker in text, marker
    assert text.count("$ClientRoot = \"D:\\WOW\"") == 1
    assert text.count("$SourceRoot = \"D:\\TrinityCore\"") == 1
    assert "Start-Process" not in text
    assert "Invoke-Sqlcmd" not in text
    assert "Set-Content -LiteralPath $Spell" not in text
    assert "Copy-Item -LiteralPath $StateTarget" not in text
    assert "Move-Item -LiteralPath $StateTarget" not in text
    print("G17R2A_STATIC_TESTS=PASS")
    print("G17R2A_READ_ONLY_CONTRACT=PASS")
    print("G17R2A_TOOL_HASH=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
