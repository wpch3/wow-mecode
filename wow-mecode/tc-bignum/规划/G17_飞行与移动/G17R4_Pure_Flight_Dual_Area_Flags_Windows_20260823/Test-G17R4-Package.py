from __future__ import annotations

import hashlib
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent
EXPECTED = {
    "original/DBFilesClient/AreaTable.stock.dbc": "b0356ff41e5777896509ec52bc68af516b67d82a659dbc47757960aef98b62dd",
    "original/DBFilesClient/AreaTable.R3.dbc": "214c6935d11b784f0bf5e4855fb756126d9d667d622a346c3124ae748812b6a8",
    "payload/DBFilesClient/AreaTable.dbc": "1acef997a27f844a8abee9b477c44f2097f745168c6df85ece6b1a135568c233",
    "third_party/mpqcli-windows-amd64.exe": "5dc56b130432098d18e911fdf4b3464adf850432b7fbb35003a2feff7c30a79f",
}


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def static_ps_balance(path: Path) -> None:
    text = path.read_text(encoding="utf-8-sig")
    pairs = {"(": ")", "[": "]", "{": "}"}
    closing = set(pairs.values())
    stack: list[tuple[str, int]] = []
    quote = ""
    escaped = False
    line_comment = False
    block_comment = False
    i = 0
    while i < len(text):
        c = text[i]
        n = text[i + 1] if i + 1 < len(text) else ""
        if line_comment:
            if c in "\r\n":
                line_comment = False
            i += 1
            continue
        if block_comment:
            if c == "#" and n == ">":
                block_comment = False
                i += 2
            else:
                i += 1
            continue
        if quote:
            if quote == '"' and c == "`" and not escaped:
                escaped = True
                i += 1
                continue
            if c == quote and not escaped:
                if quote == "'" and n == "'":
                    i += 2
                    continue
                quote = ""
            escaped = False
            i += 1
            continue
        if c == "<" and n == "#":
            block_comment = True
            i += 2
            continue
        if c == "#":
            line_comment = True
            i += 1
            continue
        if c in "'\"":
            quote = c
        elif c in pairs:
            stack.append((c, i))
        elif c in closing:
            assert stack, f"unmatched {c} in {path} at {i}"
            opening, at = stack.pop()
            assert pairs[opening] == c, f"mismatched {opening} at {at} and {c} at {i} in {path}"
        i += 1
    assert not quote and not block_comment, f"unterminated quote/comment in {path}"
    assert not stack, f"unclosed delimiters in {path}: {stack[-5:]}"


def actual_ps_ast(path: Path) -> None:
    pwsh = shutil.which("pwsh") or shutil.which("powershell")
    if not pwsh:
        return
    command = (
        "$e=$null;$t=$null;"
        "[System.Management.Automation.Language.Parser]::ParseFile($args[0],[ref]$t,[ref]$e)|Out-Null;"
        "if($e.Count){$e|ForEach-Object{$_.ToString()}|Write-Error;exit 1}"
    )
    subprocess.run([pwsh, "-NoLogo", "-NoProfile", "-Command", command, str(path)], check=True)


def main() -> int:
    for rel, expected in EXPECTED.items():
        path = ROOT / rel
        assert path.is_file(), rel
        assert digest(path) == expected, rel

    subprocess.run([sys.executable, str(ROOT / "tests/test_g17r4.py")], check=True)
    subprocess.run(
        [sys.executable, str(ROOT / "tools/patch_g17r4_client_areatable_dbc.py"), "verify",
         "--original", str(ROOT / "original/DBFilesClient/AreaTable.stock.dbc"),
         "--patched", str(ROOT / "payload/DBFilesClient/AreaTable.dbc")],
        check=True,
    )

    ps_files = sorted(ROOT.glob("*.ps1"))
    assert {p.name for p in ps_files} == {"Install-G17R4-Client-MPQ.ps1", "Rollback-G17R4-Client-MPQ.ps1"}
    for path in ps_files:
        static_ps_balance(path)
        actual_ps_ast(path)

    install = (ROOT / "Install-G17R4-Client-MPQ.ps1").read_text(encoding="utf-8-sig")
    required_markers = [
        "G17R3_CLIENT_MPQ_UPGRADE_STATE.txt",
        "G17R4_CLIENT_MPQ_UPGRADE_STATE.txt",
        "ExpectedAreaR3Hash",
        "1acef997a27f844a8abee9b477c44f2097f745168c6df85ece6b1a135568c233",
        "LOCALE_CUSTOM_DBC_COLLISIONS",
        "OTHER_ROOT_CUSTOM_DBC_COLLISIONS",
        "Python312\\python.exe",
        "CLIENT_CACHE_REMOVED",
        "SERVER_DBC_MODIFIED=False",
    ]
    for marker in required_markers:
        assert marker in install, marker
    assert "py.exe" in install and "WindowsApps aliases are not used" in install
    assert "$StateTemp = $R4StateFile" in install
    assert "$BackupR3State" in install

    files = [p.relative_to(ROOT).as_posix() for p in ROOT.rglob("*") if p.is_file()]
    assert not any(name.lower().endswith((".cpp", ".h", ".sql")) for name in files)
    assert not any(name.endswith("Spell.dbc") for name in files)
    assert not any("EffectAura" in p.read_text(encoding="utf-8", errors="ignore") for p in ROOT.glob("tools/*.py"))

    print("G17R4_PACKAGE_SELFTEST=PASS")
    print(f"POWER_SHELL_FILES={len(ps_files)}")
    print("SERVER_SOURCE_FILES=0")
    print("SQL_FILES=0")
    print("CLIENT_SPELL_EFFECT_DISGUISE=False")
    print("AREA_FLAGS=0x00000400,0x00004000")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
