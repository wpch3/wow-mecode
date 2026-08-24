#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
PRE = "c9535dca3390ece6735e6ff6b7418ed99ff206628b5e8febd7b78b05cba999bd"
POST = "10a7002db4c173e441836870da01dd3009a7ba470369ca1a89bdb399ee9b2f45"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)
    print("[OK] " + message)


def main() -> int:
    original = ROOT / "original/src/server/scripts/Commands/cs_dragonriding.cpp"
    payload = ROOT / "payload/src/server/scripts/Commands/cs_dragonriding.cpp"
    build = ROOT / "Install-Build-G17R1-Windows.ps1"
    client = ROOT / "Prepare-G17R1-Client-Patch.ps1"
    source_installer = ROOT / "tools/apply_g17r1_source.py"
    dbc_patcher = ROOT / "tools/patch_g17r1_client_spell_dbc.py"
    tests = ROOT / "tests/test_g17r1_runtime_fix.py"

    for path in (original, payload, build, client, source_installer, dbc_patcher, tests):
        require(path.is_file(), f"required file exists: {path.relative_to(ROOT)}")
    require(sha(original) == PRE, "locked G17-B0 preimage hash")
    require(sha(payload) == POST, "locked G17-R1 postimage hash")

    for path in (
        build,
        client,
        ROOT / "Run-G17R1-Windows-Fix.cmd",
        ROOT / "Run-G17R1-Prepare-Client-Patch.cmd",
    ):
        data = path.read_bytes()
        require(all(byte < 128 for byte in data), f"ASCII Windows script: {path.name}")
        require(b"\r\n" in data and b"\n" not in data.replace(b"\r\n", b""),
                f"CRLF Windows script: {path.name}")

    build_text = build.read_text(encoding="ascii")
    for marker in (
        "Invoke-NativeLogged",
        "apply_g17r1_source.py",
        POST,
        "DRAGONRIDING_FRESH_OBJECTS",
        "DRAGONRIDING_C4018_HITS",
        "G17R1_C4018_FIX_GATE=PASS",
        "STOP_DO_NOT_START_WORLDSERVER",
        "Python312\\python.exe",
        "WindowsApps aliases are rejected",
    ):
        require(marker in build_text, f"build marker: {marker}")
    require("Build-G17B0-Windows.ps1" not in build_text, "old build package is not invoked")
    require("Get-Command py.exe" not in build_text, "stale py launcher path is not used")

    client_text = client.read_text(encoding="ascii")
    require("SERVER_DBC_MODIFIED=False" in client_text, "client script records server DBC unchanged")
    require('"--output", $OutputDbc' in client_text, "client patch writes staging/client output")
    require("Copy-Item -LiteralPath $OutputDbc" in client_text, "client install copies from staging")
    require("Copy-Item -LiteralPath $OutputDbc -Destination $InputSpellDbc" not in client_text,
            "client script cannot overwrite input/server DBC")

    sums = ROOT / "SHA256SUMS.txt"
    require(sums.is_file(), "SHA256SUMS exists")
    for line in sums.read_text(encoding="ascii").splitlines():
        expected, relative = line.split("  ", 1)
        require(sha(ROOT / relative) == expected, f"package hash: {relative}")

    completed = subprocess.run([sys.executable, str(tests)], cwd=ROOT, check=False,
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    print(completed.stdout)
    require(completed.returncode == 0, "embedded 8-test suite")
    print("G17R1_WINDOWS_PACKAGE_SELFTEST=PASS")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"G17R1_WINDOWS_PACKAGE_SELFTEST_ERROR={exc}", file=sys.stderr)
        raise SystemExit(1)
