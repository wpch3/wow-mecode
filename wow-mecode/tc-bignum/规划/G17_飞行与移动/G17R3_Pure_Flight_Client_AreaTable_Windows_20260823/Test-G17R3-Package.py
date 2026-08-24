#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
EXPECTED = {
    "third_party/mpqcli-windows-amd64.exe": "5dc56b130432098d18e911fdf4b3464adf850432b7fbb35003a2feff7c30a79f",
    "original/DBFilesClient/AreaTable.dbc": "b0356ff41e5777896509ec52bc68af516b67d82a659dbc47757960aef98b62dd",
    "payload/DBFilesClient/AreaTable.dbc": "214c6935d11b784f0bf5e4855fb756126d9d667d622a346c3124ae748812b6a8",
    "original/src/server/game/Spells/SpellInfo.cpp": "73d52ac0feb67a32822fc0bf086a9174ba7ef0bc186223cdc8a690f48fccb9e2",
    "payload/src/server/game/Spells/SpellInfo.cpp": "c3ec2237ed6da8831662a8b7a5d45cf88f8efc7798cdd35c52a07700fa9cbcbf",
}
REQUIRED = [
    "README_FIRST.txt", "PACKAGE_METADATA.txt", "THIRD_PARTY_PROVENANCE.txt",
    "OFFLINE_VALIDATION_20260823.txt", "Run-G17R3-Windows-Fix.cmd",
    "Run-G17R3-Full-Rollback.cmd", "Install-Build-G17R3-Server.ps1",
    "Upgrade-G17R3-Client-MPQ.ps1", "Rollback-G17R3-Client-MPQ.ps1",
    "Rollback-G17R3-Server.ps1", "tools/apply_g17r3_server_source.py",
    "tools/patch_g17r3_client_areatable_dbc.py", "tests/test_g17r3.py",
    "third_party/mpqcli-LICENSE.txt", "SHA256SUMS.txt",
]


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def main() -> int:
    for rel in REQUIRED:
        require((ROOT / rel).is_file(), f"missing required file: {rel}")
    for rel, expected in EXPECTED.items():
        require(sha(ROOT / rel) == expected, f"hash mismatch: {rel}")

    manifest = {}
    for line in (ROOT / "SHA256SUMS.txt").read_text(encoding="utf-8-sig").splitlines():
        digest, rel = line.split("  ", 1)
        require(rel not in manifest, f"duplicate manifest path: {rel}")
        manifest[rel] = digest
    actual = {p.relative_to(ROOT).as_posix() for p in ROOT.rglob("*") if p.is_file() and p.name != "SHA256SUMS.txt"}
    require(set(manifest) == actual, "manifest file set mismatch")
    for rel, digest in manifest.items():
        require(sha(ROOT / rel) == digest, f"manifest hash mismatch: {rel}")

    install = (ROOT / "Run-G17R3-Windows-Fix.cmd").read_text(encoding="utf-8-sig")
    require(install.index("Install-Build-G17R3-Server.ps1") < install.index("Upgrade-G17R3-Client-MPQ.ps1"), "unsafe phase order")
    require("G17R3_WINDOWS_FIX_RESULT=PASS" in install and "G17R3_WINDOWS_FIX_RESULT=FAIL" in install, "master result markers missing")

    upgrade = (ROOT / "Upgrade-G17R3-Client-MPQ.ps1").read_text(encoding="utf-8-sig")
    require("ExpectedAreaPatchedHash" in upgrade and "ExpectedServerAreaHash" not in upgrade, "unexpected client constants")
    require("SERVER_DBC_MODIFIED=False" in upgrade and "R1_STATE_MODIFIED=False" in upgrade, "non-write contracts missing")
    require("if (Test-Path -LiteralPath $TemporaryTarget) -or" not in upgrade, "unparenthesized Test-Path boolean")
    require("(Test-Path -LiteralPath $TemporaryTarget) -or (Test-Path -LiteralPath $SwapOld)" in upgrade, "safe swap-temp check missing")

    server = (ROOT / "Install-Build-G17R3-Server.ps1").read_text(encoding="utf-8-sig")
    require("R3_PREPARED_STATE=REUSED_BEFORE_APPLY" in server, "prepared-state recovery missing")
    require("R3_EXISTING_BUILD_STATUS" in server and "SpellInfo*.obj" in server, "build evidence gates missing")
    require("[System.IO.File]::Replace($BuildStateTemp,$BuildState,$BuildStatePrevious)" in server, "atomic state replacement missing")

    rollback = (ROOT / "Rollback-G17R3-Server.ps1").read_text(encoding="utf-8-sig")
    require("AUTO_RECOVER_R3_BINARIES=PASS" in rollback and "AUTO_RECOVER_R3_SOURCE=PASS" in rollback, "rollback recovery missing")
    require("current EXE changed" in rollback and "backup PDB changed" in rollback, "rollback hash gates missing")

    payload = (ROOT / "payload/src/server/game/Spells/SpellInfo.cpp").read_text(encoding="utf-8-sig")
    require(payload.count("!player->IsOutdoors()") == 1, "live outdoor gate count mismatch")
    require("((areaFlags & AREA_FLAG_INSIDE) || !player->IsOutdoors())" in payload, "live indoor safety expression missing")

    require(not list(ROOT.rglob("*.sql")), "R3 package must not contain SQL")
    tests = subprocess.run([sys.executable, str(ROOT / "tests/test_g17r3.py")], text=True, capture_output=True)
    require(tests.returncode == 0, "G17R3 unit tests failed:\n" + tests.stdout + tests.stderr)
    print("G17R3_PACKAGE_STATIC_TEST=PASS")
    print("G17R3_UNIT_TESTS=PASS_9_OF_9")
    print("REAL_WINDOWS_AND_GAME_ACCEPTANCE=PENDING_USER")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError) as error:
        print(f"G17R3_PACKAGE_STATIC_TEST_ERROR={error}", file=sys.stderr)
        raise SystemExit(1)
