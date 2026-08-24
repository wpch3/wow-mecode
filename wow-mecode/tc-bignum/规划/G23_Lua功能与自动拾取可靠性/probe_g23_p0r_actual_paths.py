#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""G23-P0R read-only collector for actual AoE Loot paths and Eluna extensions.

This is a focused replacement for probe_g23_baseline.py schema 1 after the
first Windows report proved that CustomAoELoot.* lives under game/Custom,
not game/Loot. It captures discovered source files, .lua + .ext runtime
scripts, Eluna loader contexts and the newest relevant log lines.
"""

from __future__ import annotations

import argparse
import codecs
import hashlib
import os
import re
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Iterable, Optional

SCHEMA = 2
MAX_FILE = 8 * 1024 * 1024
MAX_LOG_TAIL = 8 * 1024 * 1024
MAX_LOG_MATCHES_PER_FILE = 240

LOOT_HANDLER_REL = Path("src/server/game/Handlers/LootHandler.cpp")
CONFIG_RE = re.compile(
    r"^\s*(AoELoot\.[A-Za-z0-9_.-]+|Eluna\.[A-Za-z0-9_.-]+|Lua\.[A-Za-z0-9_.-]+)\s*=\s*(.*?)\s*$",
    re.IGNORECASE,
)
SCRIPT_PATH_RE = re.compile(r"^\s*(?:Eluna\.ScriptPath|Lua\.ScriptPath)\s*=\s*(.*?)\s*$", re.IGNORECASE)
SENSITIVE_RE = re.compile(r"password|databaseinfo|username|secret|token", re.IGNORECASE)
LOG_RE = re.compile(r"eluna|lua|aoeloot|customaoeloot|群体拾取|objectvariables|error", re.IGNORECASE)
LOADER_TOKENS = (
    "Eluna.ScriptPath", "LoadScripts", "ElunaLoader", "extensions", ".ext",
    "from_chars", "RegisterPlayerEvent", "RegisterPlayerGossipEvent",
    "CreateLuaEvent", "OnLootItem", "OnLootMoney", "Reload", "ElunaState",
)


class ProbeError(RuntimeError):
    pass


@dataclass
class TextFile:
    path: Path
    data: bytes
    text: str
    encoding: str
    newline: str

    @property
    def sha256(self) -> str:
        return hashlib.sha256(self.data).hexdigest()


def bt(v: bool) -> str:
    return "True" if v else "False"


def decode(data: bytes) -> tuple[str, str]:
    if data.startswith(codecs.BOM_UTF8):
        return data.decode("utf-8-sig"), "UTF-8-BOM"
    if data.startswith(codecs.BOM_UTF16_LE):
        return data.decode("utf-16-le"), "UTF-16LE-BOM"
    if data.startswith(codecs.BOM_UTF16_BE):
        return data.decode("utf-16-be"), "UTF-16BE-BOM"
    try:
        return data.decode("utf-8"), "UTF-8"
    except UnicodeDecodeError:
        try:
            return data.decode("gb18030"), "GB18030"
        except UnicodeDecodeError:
            return data.decode("latin-1"), "LATIN-1-FALLBACK"


def nl(data: bytes) -> str:
    crlf = data.count(b"\r\n")
    lf = data.count(b"\n") - crlf
    cr = data.count(b"\r") - crlf
    if crlf and not lf and not cr:
        return "CRLF"
    if lf and not crlf and not cr:
        return "LF"
    if not crlf and not lf and not cr:
        return "NONE"
    return f"MIXED(crlf={crlf},lf={lf},cr={cr})"


def read_tf(path: Path, limit: int = MAX_FILE) -> TextFile:
    before = path.stat()
    if before.st_size > limit:
        raise ProbeError(f"file too large: {path} ({before.st_size}>{limit})")
    data = path.read_bytes()
    after = path.stat()
    if before.st_size != after.st_size or before.st_mtime_ns != after.st_mtime_ns:
        raise ProbeError(f"file changed while reading: {path}")
    text, enc = decode(data)
    return TextFile(path, data, text, enc, nl(data))


def fingerprint(path: Path) -> tuple[int, int, str]:
    st = path.stat()
    return st.st_size, st.st_mtime_ns, hashlib.sha256(path.read_bytes()).hexdigest()


def rel(path: Path, root: Path) -> str:
    try:
        return path.resolve().relative_to(root.resolve()).as_posix()
    except (ValueError, OSError):
        return str(path.resolve()).replace("\\", "/")


def command(args: list[str], cwd: Optional[Path] = None) -> tuple[int, str]:
    try:
        cp = subprocess.run(
            args, cwd=str(cwd) if cwd else None, stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=45, check=False,
        )
    except (OSError, subprocess.TimeoutExpired) as exc:
        return 127, f"<unavailable: {exc}>"
    text, _ = decode(cp.stdout)
    return cp.returncode, text.rstrip("\r\n")


def git_state(root: Path) -> tuple[str, str, str]:
    values = []
    for args in (
        ["git", "branch", "--show-current"],
        ["git", "rev-parse", "HEAD"],
        ["git", "status", "--porcelain=v1", "--untracked-files=all"],
    ):
        code, out = command(args, root)
        values.append(out if code == 0 else f"<git exit={code}> {out}")
    return values[0], values[1], values[2]


def full(lines: list[str], label: str, tf: TextFile, display: str) -> None:
    lines.append(
        f"FILE label={label}; path={display}; bytes={len(tf.data)}; sha256={tf.sha256}; "
        f"encoding={tf.encoding}; newline={tf.newline}"
    )
    lines.append(f"----- BEGIN {label} {display} -----")
    lines.extend(tf.text.splitlines())
    lines.append(f"----- END {label} {display} -----")


def config_files(run_dir: Path) -> list[Path]:
    out = []
    primary = run_dir / "worldserver.conf"
    if primary.is_file():
        out.append(primary)
    extra = run_dir / "worldserver.conf.d"
    if extra.is_dir():
        out.extend(sorted(p for p in extra.rglob("*.conf") if p.is_file()))
    return out


def clean_value(raw: str) -> str:
    value = raw.strip()
    if len(value) >= 2 and value[0] == value[-1] and value[0] in "\"'":
        value = value[1:-1]
    return value.strip()


def resolve_path(run_dir: Path, raw: str) -> Optional[Path]:
    value = clean_value(raw)
    if not value or any(ch in value for ch in "*?<>|"):
        return None
    value = os.path.expandvars(value).replace("\\", os.sep).replace("/", os.sep)
    p = Path(value)
    if not p.is_absolute():
        p = run_dir / p
    return p.resolve()


def contexts(text: str, tokens: Iterable[str], radius: int = 4, max_blocks: int = 300) -> list[str]:
    src = text.splitlines()
    low_tokens = tuple(t.lower() for t in tokens)
    hits = [i for i, line in enumerate(src) if any(t in line.lower() for t in low_tokens)]
    blocks: list[tuple[int, int]] = []
    for i in hits:
        a, b = max(0, i - radius), min(len(src), i + radius + 1)
        if blocks and a <= blocks[-1][1]:
            blocks[-1] = (blocks[-1][0], max(blocks[-1][1], b))
        else:
            blocks.append((a, b))
        if len(blocks) >= max_blocks:
            break
    return ["\n".join(f"{i + 1:6d}: {src[i]}" for i in range(a, b)) for a, b in blocks]


def process_lines() -> list[str]:
    if os.name != "nt":
        return ["<not Windows>"]
    ps = (
        "$ErrorActionPreference='Stop';"
        "$p=@(Get-CimInstance Win32_Process -Filter \"Name='worldserver.exe'\");"
        "$p|ForEach-Object{\"PID=$($_.ProcessId);PATH=$($_.ExecutablePath);CREATED=$($_.CreationDate)\"}"
    )
    code, out = command(["powershell.exe", "-NoProfile", "-NonInteractive", "-Command", ps])
    if code:
        return [f"<process query failed exit={code}: {out}>"]
    return [x for x in out.splitlines() if x.strip()]


def newest_log_matches(run_dir: Path) -> tuple[list[Path], list[str]]:
    found: dict[str, Path] = {}
    for root in (run_dir, run_dir / "logs", run_dir / "Logs"):
        if not root.is_dir():
            continue
        for pattern in ("*.log", "*.txt"):
            for p in root.glob(pattern):
                if p.is_file():
                    found[os.path.normcase(str(p.resolve()))] = p
    files = sorted(found.values(), key=lambda p: p.stat().st_mtime_ns, reverse=True)[:12]
    rendered: list[str] = []
    for p in files:
        try:
            size = p.stat().st_size
            with p.open("rb") as fh:
                if size > MAX_LOG_TAIL:
                    fh.seek(size - MAX_LOG_TAIL)
                    fh.readline()
                data = fh.read(MAX_LOG_TAIL)
            text, _ = decode(data)
            matches = [line for line in text.splitlines() if LOG_RE.search(line)]
            for line in matches[-MAX_LOG_MATCHES_PER_FILE:]:
                rendered.append(f"{p.name}: {line}")
        except OSError as exc:
            rendered.append(f"{p.name}: <read failed: {exc}>")
    return files, rendered


def collect(source_root: Path, run_dir: Path) -> tuple[str, bool]:
    source_root = source_root.resolve()
    run_dir = run_dir.resolve()
    if not source_root.is_dir() or not run_dir.is_dir():
        raise ProbeError("source_root or run_dir not found")

    lines = [
        f"G23_P0R_SCHEMA={SCHEMA}",
        f"CAPTURE_UTC={datetime.now(timezone.utc).isoformat()}",
        f"SOURCE_ROOT={source_root}",
        f"RUN_DIR={run_dir}",
        "READ_ONLY_DECLARATION=No source/config/runtime-script/database writes; no DB connections",
    ]
    branch0, head0, status0 = git_state(source_root)

    loot = source_root / LOOT_HANDLER_REL
    cpp = sorted((source_root / "src/server/game").rglob("CustomAoELoot.cpp"))
    hdr = sorted((source_root / "src/server/game").rglob("CustomAoELoot.h"))
    cpp = [p for p in cpp if p.is_file()]
    hdr = [p for p in hdr if p.is_file()]
    targets = ([loot] if loot.is_file() else []) + cpp + hdr
    source_before = {str(p): fingerprint(p) for p in targets}

    lines.append("\n===== 1. ACTUAL SOURCE DISCOVERY =====")
    lines.append(f"GIT_BRANCH={branch0}")
    lines.append(f"GIT_HEAD={head0}")
    lines.append("GIT_STATUS_BEGIN")
    lines.extend(status0.splitlines() or ["<clean>"])
    lines.append("GIT_STATUS_END")
    lines.append(f"G23_P0R_LOOT_HANDLER_FOUND={bt(loot.is_file())}")
    lines.append(f"G23_P0R_AOE_CPP_CANDIDATE_COUNT={len(cpp)}")
    lines.append(f"G23_P0R_AOE_H_CANDIDATE_COUNT={len(hdr)}")
    for p in cpp:
        lines.append(f"AOE_CPP_CANDIDATE={rel(p, source_root)}")
    for p in hdr:
        lines.append(f"AOE_H_CANDIDATE={rel(p, source_root)}")
    pair_ready = len(cpp) == 1 and len(hdr) == 1 and cpp[0].parent == hdr[0].parent
    lines.append(f"G23_P0R_AOE_SOURCE_PAIR_READY={bt(pair_ready)}")

    source_tfs: list[tuple[str, TextFile]] = []
    if loot.is_file():
        source_tfs.append((rel(loot, source_root), read_tf(loot)))
    for p in cpp + hdr:
        source_tfs.append((rel(p, source_root), read_tf(p)))

    hook_ready = False
    if loot.is_file():
        lt = source_tfs[0][1].text
        markers = (
            '#include "CustomAoELoot.h"',
            "CustomAoELoot::LootAllAround",
            "CustomAoELoot::GatherMoneyAround",
            "HandleAutostoreLootItemOpcode",
            "HandleLootMoneyOpcode",
        )
        counts = {m: lt.count(m) for m in markers}
        for m, n in counts.items():
            lines.append(f"HOOK_COUNT {m}={n}")
        hook_ready = counts[markers[0]] == 1 and all(counts[m] >= 1 for m in markers[1:])
    lines.append(f"G23_P0R_AOE_HOOK_READY={bt(hook_ready)}")

    lines.append("\n===== 2. FULL ACTUAL AOE LOOT SOURCE =====")
    for display, tf in source_tfs:
        full(lines, "ACTUAL_SOURCE", tf, display)

    lines.append("\n===== 3. CONFIG AND RUNTIME SCRIPT ROOT =====")
    configs = config_files(run_dir)
    config_before = {str(p): fingerprint(p) for p in configs}
    raw_paths: list[str] = []
    aoe_keys = 0
    for p in configs:
        tf = read_tf(p)
        lines.append(f"CONFIG_FILE path={rel(p, run_dir)}; sha256={tf.sha256}; bytes={len(tf.data)}")
        for n, line in enumerate(tf.text.splitlines(), 1):
            m = CONFIG_RE.match(line)
            if not m:
                continue
            key, value = m.group(1), m.group(2)
            shown = "<REDACTED>" if SENSITIVE_RE.search(key) else value
            lines.append(f"CONFIG_KEY file={rel(p, run_dir)}; line={n}; {key}={shown}")
            if key.lower().startswith("aoeloot."):
                aoe_keys += 1
            sm = SCRIPT_PATH_RE.match(line)
            if sm:
                raw_paths.append(sm.group(1))
    roots: list[Path] = []
    for raw in raw_paths:
        p = resolve_path(run_dir, raw)
        if p and p not in roots:
            roots.append(p)
    lines.append(f"G23_P0R_AOE_CONFIG_KEY_COUNT={aoe_keys}")
    lines.append(f"G23_P0R_CONFIGURED_SCRIPT_ROOT_COUNT={len(roots)}")
    for p in roots:
        lines.append(f"SCRIPT_ROOT path={p}; exists={bt(p.is_dir())}")
    script_root_ready = len(roots) == 1 and roots[0].is_dir()
    lines.append(f"G23_P0R_SCRIPT_ROOT_READY={bt(script_root_ready)}")

    lines.append("\n===== 4. FULL DEPLOYED LUA AND ELUNA EXTENSION FILES =====")
    deployed: list[Path] = []
    for root in roots:
        if root.is_dir():
            deployed.extend(p for p in root.rglob("*") if p.is_file() and p.suffix.lower() in {".lua", ".ext"})
    unique = {os.path.normcase(str(p.resolve())): p for p in deployed}
    deployed = sorted(unique.values())
    script_before = {str(p): fingerprint(p) for p in deployed}
    lua_count = sum(p.suffix.lower() == ".lua" for p in deployed)
    ext_count = sum(p.suffix.lower() == ".ext" for p in deployed)
    lines.append(f"G23_P0R_DEPLOYED_LUA_COUNT={lua_count}")
    lines.append(f"G23_P0R_DEPLOYED_EXT_COUNT={ext_count}")
    for p in deployed:
        root = next((r for r in roots if p.is_relative_to(r)), run_dir)
        full(lines, "DEPLOYED_SCRIPT", read_tf(p), rel(p, root))

    lines.append("\n===== 5. ELUNA LOADER/API CONTEXTS =====")
    lua_engine = source_root / "src/server/game/LuaEngine"
    loader_candidates: list[Path] = []
    if lua_engine.is_dir():
        wanted = {"ElunaLoader.cpp", "LuaEngine.cpp", "Eluna.cpp", "ElunaConfig.cpp", "PlayerHooks.cpp", "GossipHooks.cpp", "Hooks.h"}
        loader_candidates = sorted(p for p in lua_engine.rglob("*") if p.is_file() and p.name in wanted)
    loader_names = [p for p in loader_candidates if p.name == "ElunaLoader.cpp"]
    lines.append(f"G23_P0R_ELUNA_LOADER_FILE_COUNT={len(loader_names)}")
    block_count = 0
    for p in loader_candidates:
        tf = read_tf(p)
        blocks = contexts(tf.text, LOADER_TOKENS)
        if not blocks:
            continue
        lines.append(f"ELUNA_FILE path={rel(p, source_root)}; sha256={tf.sha256}; blocks={len(blocks)}")
        block_count += len(blocks)
        for i, block in enumerate(blocks, 1):
            lines.append(f"--- ELUNA_CONTEXT {rel(p, source_root)} #{i} ---")
            lines.extend(block.splitlines())
    lines.append(f"G23_P0R_ELUNA_CONTEXT_BLOCK_COUNT={block_count}")

    lines.append("\n===== 6. PROCESS AND NEWEST RELEVANT LOG LINES =====")
    proc = process_lines()
    lines.extend(proc)
    lines.append(f"G23_P0R_ACTIVE_WORLDSERVER_COUNT={sum(x.startswith('PID=') for x in proc)}")
    exe = run_dir / "worldserver.exe"
    if exe.is_file():
        lines.append(f"RUN_EXE bytes={exe.stat().st_size}; sha256={hashlib.sha256(exe.read_bytes()).hexdigest()}; path={exe}")
    log_files, log_matches = newest_log_matches(run_dir)
    lines.append(f"G23_P0R_LOG_FILE_COUNT={len(log_files)}")
    lines.append(f"G23_P0R_NEWEST_RELEVANT_LOG_LINE_COUNT={len(log_matches)}")
    for p in log_files:
        st = p.stat()
        lines.append(f"LOG_FILE path={rel(p, run_dir)}; bytes={st.st_size}; mtime_ns={st.st_mtime_ns}")
    lines.extend(log_matches or ["<no relevant newest log lines>"])

    lines.append("\n===== 7. READ-ONLY POSTCHECK =====")
    branch1, head1, status1 = git_state(source_root)
    source_after = {str(p): fingerprint(p) for p in targets if p.is_file()}
    config_after = {str(p): fingerprint(p) for p in configs if p.is_file()}
    script_after = {str(p): fingerprint(p) for p in deployed if p.is_file()}
    source_edits = 0 if source_before == source_after and (branch0, head0, status0) == (branch1, head1, status1) else 1
    config_edits = 0 if config_before == config_after else 1
    script_edits = 0 if script_before == script_after else 1
    lines.append(f"G23_P0R_SOURCE_EDIT_COUNT={source_edits}")
    lines.append(f"G23_P0R_CONFIG_EDIT_COUNT={config_edits}")
    lines.append(f"G23_P0R_SCRIPT_EDIT_COUNT={script_edits}")
    lines.append("G23_P0R_DATABASE_CONNECTION_COUNT=0")
    lines.append("G23_P0R_DATABASE_EDIT_COUNT=0")

    passed = bool(pair_ready and hook_ready and script_root_ready and len(loader_names) == 1 and source_edits == 0 and config_edits == 0 and script_edits == 0)
    lines.append(f"G23_P0R_CAPTURE_PASS={bt(passed)}")
    lines.append("EVIDENCE_BOUNDARY=Corrected read-only baseline only; runtime root cause still requires source analysis and controlled reproduction")
    return "\n".join(lines) + "\n", passed


def write_report(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    normalized = text.replace("\r\n", "\n").replace("\r", "\n").replace("\n", "\r\n")
    path.write_bytes(codecs.BOM_UTF8 + normalized.encode("utf-8"))


def fixture(root: Path) -> tuple[Path, Path]:
    src, run = root / "source", root / "run"
    (src / "src/server/game/Handlers").mkdir(parents=True)
    (src / "src/server/game/Custom").mkdir(parents=True)
    (src / "src/server/game/LuaEngine/hooks").mkdir(parents=True)
    (run / "worldserver.conf.d").mkdir(parents=True)
    (run / "lua_scripts/extensions").mkdir(parents=True)
    (src / LOOT_HANDLER_REL).write_text(
        '#include "CustomAoELoot.h"\nvoid HandleAutostoreLootItemOpcode(){CustomAoELoot::LootAllAround(p,c);}\n'
        'void HandleLootMoneyOpcode(){CustomAoELoot::GatherMoneyAround(p,c);}\n', encoding="utf-8")
    (src / "src/server/game/Custom/CustomAoELoot.cpp").write_text("namespace CustomAoELoot{}\n", encoding="utf-8")
    (src / "src/server/game/Custom/CustomAoELoot.h").write_text("namespace CustomAoELoot{}\n", encoding="utf-8")
    (src / "src/server/game/LuaEngine/ElunaLoader.cpp").write_text("LoadScripts extensions .ext Eluna.ScriptPath\n", encoding="utf-8")
    (src / "src/server/game/LuaEngine/LuaEngine.cpp").write_text("RegisterPlayerEvent CreateLuaEvent\n", encoding="utf-8")
    (run / "worldserver.conf").write_text('Eluna.ScriptPath="lua_scripts"\n', encoding="utf-8")
    (run / "worldserver.conf.d/aoeloot.conf").write_text("AoELoot.Enable=1\n", encoding="utf-8")
    (run / "lua_scripts/a.lua").write_text("RegisterPlayerEvent(3, f)\n", encoding="utf-8")
    (run / "lua_scripts/extensions/ObjectVariables.ext").write_text("extension=true\n", encoding="utf-8")
    log = [f"old {i} Lua ERROR" for i in range(400)] + ["LATEST_SENTINEL Eluna ERROR"]
    (run / "Eluna.log").write_text("\n".join(log) + "\n", encoding="utf-8")
    return src, run


def self_test() -> int:
    with tempfile.TemporaryDirectory(prefix="g23_p0r_") as td:
        src, run = fixture(Path(td))
        before = {str(p): fingerprint(p) for p in list(src.rglob("*")) + list(run.rglob("*")) if p.is_file()}
        report, passed = collect(src, run)
        after = {str(p): fingerprint(p) for p in list(src.rglob("*")) + list(run.rglob("*")) if p.is_file()}
        required = (
            "G23_P0R_SCHEMA=2", "G23_P0R_AOE_CPP_CANDIDATE_COUNT=1",
            "G23_P0R_AOE_H_CANDIDATE_COUNT=1", "G23_P0R_AOE_SOURCE_PAIR_READY=True",
            "AOE_CPP_CANDIDATE=src/server/game/Custom/CustomAoELoot.cpp",
            "G23_P0R_DEPLOYED_LUA_COUNT=1", "G23_P0R_DEPLOYED_EXT_COUNT=1",
            "LATEST_SENTINEL", "G23_P0R_SOURCE_EDIT_COUNT=0",
            "G23_P0R_CONFIG_EDIT_COUNT=0", "G23_P0R_SCRIPT_EDIT_COUNT=0",
            "G23_P0R_CAPTURE_PASS=True",
        )
        if not passed or before != after or any(x not in report for x in required):
            raise ProbeError("positive fixture failed")
        # Duplicate pair must fail closed.
        (src / "src/server/game/Loot").mkdir()
        (src / "src/server/game/Loot/CustomAoELoot.cpp").write_text("duplicate\n", encoding="utf-8")
        (src / "src/server/game/Loot/CustomAoELoot.h").write_text("duplicate\n", encoding="utf-8")
        negative, neg_pass = collect(src, run)
        if neg_pass or "G23_P0R_AOE_CPP_CANDIDATE_COUNT=2" not in negative or "G23_P0R_CAPTURE_PASS=False" not in negative:
            raise ProbeError("duplicate-source negative fixture false-passed")
    print("[OK] G23-P0R actual Custom path + .lua/.ext positive fixture passed.")
    print("[OK] Newest-log-tail fixture retained the latest sentinel.")
    print("[OK] Duplicate-source negative fixture did not false-pass.")
    print("[OK] G23-P0R read-only self-test passed.")
    return 0


def args(argv: list[str]) -> argparse.Namespace:
    p = argparse.ArgumentParser(description="G23-P0R actual-path read-only collector")
    p.add_argument("source_root", nargs="?", type=Path)
    p.add_argument("run_dir", nargs="?", type=Path)
    p.add_argument("report", nargs="?", type=Path)
    p.add_argument("--self-test", action="store_true")
    return p.parse_args(argv)


def main(argv: list[str]) -> int:
    a = args(argv)
    if a.self_test:
        return self_test()
    if not a.source_root or not a.run_dir or not a.report:
        raise ProbeError("source_root, run_dir and report are required")
    report, passed = collect(a.source_root, a.run_dir)
    write_report(a.report, report)
    digest = hashlib.sha256(a.report.read_bytes()).hexdigest()
    print(f"[OK] G23-P0R report written: {a.report}")
    print(f"[OK] G23-P0R report sha256={digest}")
    print("[OK] G23-P0R source/config/script/database edits=0")
    print(f"[{'OK' if passed else 'STOP'}] G23_P0R_CAPTURE_PASS={bt(passed)}")
    return 0 if passed else 2


if __name__ == "__main__":
    try:
        raise SystemExit(main(sys.argv[1:]))
    except ProbeError as exc:
        print(f"[STOP] {exc}", file=sys.stderr)
        raise SystemExit(2)
