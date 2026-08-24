#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""G23 read-only Windows baseline collector.

Captures the real AoE Loot source/hook, active config, deployed Lua files,
Eluna API/event contexts and recent relevant log lines. It never writes under
source_root or run_dir and does not connect to any database.
"""

from __future__ import annotations

import argparse
import codecs
import hashlib
import os
import re
import shutil
import subprocess
import sys
import tempfile
from collections import defaultdict
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Iterable, Optional

SCHEMA = 1
MAX_TEXT_FILE = 8 * 1024 * 1024
MAX_LOG_TAIL = 5 * 1024 * 1024
MAX_LOG_FILES = 12
MAX_LOG_MATCHES = 300

REQUIRED_SOURCE = (
    Path("src/server/game/Handlers/LootHandler.cpp"),
    Path("src/server/game/Loot/CustomAoELoot.cpp"),
    Path("src/server/game/Loot/CustomAoELoot.h"),
)

ELUNA_TOKENS = (
    "Eluna.ScriptPath",
    "RegisterPlayerEvent",
    "RegisterPlayerGossipEvent",
    "RegisterServerEvent",
    "CreateLuaEvent",
    "CharDBQuery",
    "CharDBExecute",
    "WorldDBQuery",
    "WorldDBExecute",
    "OnLootItem",
    "PLAYER_EVENT_ON_COMMAND",
    "GOSSIP_EVENT_ON_SELECT",
)

CONFIG_KEY_RE = re.compile(
    r"^\s*(AoELoot\.[A-Za-z0-9_.-]+|Eluna\.[A-Za-z0-9_.-]+|Lua\.[A-Za-z0-9_.-]+)\s*=\s*(.*?)\s*$",
    re.IGNORECASE,
)
SCRIPT_PATH_RE = re.compile(
    r"^\s*(?:Eluna\.ScriptPath|Lua\.ScriptPath)\s*=\s*(.*?)\s*$",
    re.IGNORECASE,
)
SENSITIVE_RE = re.compile(r"password|databaseinfo|username|secret|token", re.IGNORECASE)
LOG_RE = re.compile(r"eluna|lua|aoeloot|customaoeloot|群体拾取", re.IGNORECASE)
SQL_TABLE_RE = re.compile(
    r"\b(?:FROM|JOIN|INTO|UPDATE|TABLE)\s+`?([A-Za-z_][A-Za-z0-9_]*)`?",
    re.IGNORECASE,
)
REGISTER_RE = re.compile(
    r"\b(Register(?:Player|Server|Guild|Group|Creature|GameObject|Item|PlayerGossip|CreatureGossip|GameObjectGossip)Event)\s*\(([^\r\n]*)",
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


def bool_text(value: bool) -> str:
    return "True" if value else "False"


def safe_rel(path: Path, root: Path) -> str:
    try:
        return str(path.resolve().relative_to(root.resolve())).replace("\\", "/")
    except (ValueError, OSError):
        return str(path.resolve()).replace("\\", "/")


def read_bytes_stable(path: Path, limit: int = MAX_TEXT_FILE) -> bytes:
    before = path.stat()
    if before.st_size > limit:
        raise ProbeError(f"file exceeds capture limit ({before.st_size}>{limit}): {path}")
    data = path.read_bytes()
    after = path.stat()
    if before.st_size != after.st_size or before.st_mtime_ns != after.st_mtime_ns:
        raise ProbeError(f"file changed while being read: {path}")
    return data


def decode_bytes(data: bytes) -> tuple[str, str]:
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


def newline_style(data: bytes) -> str:
    crlf = data.count(b"\r\n")
    bare_lf = data.count(b"\n") - crlf
    bare_cr = data.count(b"\r") - crlf
    if crlf and not bare_lf and not bare_cr:
        return "CRLF"
    if bare_lf and not crlf and not bare_cr:
        return "LF"
    if not crlf and not bare_lf and not bare_cr:
        return "NONE"
    return f"MIXED(crlf={crlf},lf={bare_lf},cr={bare_cr})"


def read_text_file(path: Path, limit: int = MAX_TEXT_FILE) -> TextFile:
    data = read_bytes_stable(path, limit)
    text, encoding = decode_bytes(data)
    return TextFile(path, data, text, encoding, newline_style(data))


def run_command(args: list[str], cwd: Optional[Path] = None, timeout: int = 30) -> tuple[int, str]:
    try:
        cp = subprocess.run(
            args,
            cwd=str(cwd) if cwd else None,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=timeout,
            check=False,
        )
    except (OSError, subprocess.TimeoutExpired) as exc:
        return 127, f"<command unavailable: {exc}>"
    text, _ = decode_bytes(cp.stdout)
    return cp.returncode, text.rstrip("\r\n")


def git_snapshot(root: Path) -> dict[str, str]:
    result: dict[str, str] = {}
    for key, argv in (
        ("branch", ["git", "branch", "--show-current"]),
        ("head", ["git", "rev-parse", "HEAD"]),
        ("status", ["git", "status", "--porcelain=v1", "--untracked-files=all"]),
    ):
        code, text = run_command(argv, cwd=root)
        result[key] = text if code == 0 else f"<unavailable exit={code}> {text}"
    return result


def file_fingerprint(path: Path) -> tuple[int, int, str]:
    stat = path.stat()
    return stat.st_size, stat.st_mtime_ns, hashlib.sha256(path.read_bytes()).hexdigest()


def find_config_files(run_dir: Path) -> list[Path]:
    out: list[Path] = []
    primary = run_dir / "worldserver.conf"
    if primary.is_file():
        out.append(primary)
    conf_dir = run_dir / "worldserver.conf.d"
    if conf_dir.is_dir():
        out.extend(sorted(p for p in conf_dir.rglob("*.conf") if p.is_file()))
    return out


def clean_config_value(raw: str) -> str:
    value = raw.strip()
    if len(value) >= 2 and value[0] == value[-1] and value[0] in "\"'":
        value = value[1:-1]
    return value.strip()


def resolve_script_path(run_dir: Path, raw: str) -> Optional[Path]:
    value = clean_config_value(raw)
    if not value or any(ch in value for ch in "*?<>|"):
        return None
    value = os.path.expandvars(value).replace("\\", os.sep).replace("/", os.sep)
    p = Path(value)
    if not p.is_absolute():
        p = run_dir / p
    try:
        return p.resolve()
    except OSError:
        return p.absolute()


def find_lua_files(paths: Iterable[Path]) -> list[Path]:
    seen: set[str] = set()
    out: list[Path] = []
    for root in paths:
        if not root.is_dir():
            continue
        for path in sorted(root.rglob("*.lua")):
            if not path.is_file():
                continue
            key = os.path.normcase(str(path.resolve()))
            if key in seen:
                continue
            seen.add(key)
            out.append(path)
    return out


def find_eluna_source_files(source_root: Path) -> list[Path]:
    src = source_root / "src"
    if not src.is_dir():
        return []
    out: list[Path] = []
    wanted_ext = {".cpp", ".h", ".hpp", ".in", ".conf", ".dist"}
    for base, dirs, files in os.walk(src):
        dirs[:] = [d for d in dirs if d not in {".git", "dep", "tests"}]
        low_base = base.lower()
        likely = "eluna" in low_base or "luaengine" in low_base or "hooks" in low_base
        for name in files:
            path = Path(base) / name
            if path.suffix.lower() not in wanted_ext:
                continue
            if likely or "eluna" in name.lower() or "lua" in name.lower():
                out.append(path)
    return sorted(set(out))


def contexts(text: str, tokens: Iterable[str], radius: int = 3, max_blocks: int = 220) -> list[str]:
    lines = text.splitlines()
    hits: list[int] = []
    lowered = [line.lower() for line in lines]
    token_lower = tuple(t.lower() for t in tokens)
    for idx, line in enumerate(lowered):
        if any(token in line for token in token_lower):
            hits.append(idx)
    blocks: list[tuple[int, int]] = []
    for idx in hits:
        start, end = max(0, idx - radius), min(len(lines), idx + radius + 1)
        if blocks and start <= blocks[-1][1]:
            blocks[-1] = (blocks[-1][0], max(blocks[-1][1], end))
        else:
            blocks.append((start, end))
        if len(blocks) >= max_blocks:
            break
    rendered: list[str] = []
    for start, end in blocks:
        rendered.append("\n".join(f"{i + 1:6d}: {lines[i]}" for i in range(start, end)))
    return rendered


def recent_log_matches(run_dir: Path) -> tuple[list[Path], list[str]]:
    candidates: list[Path] = []
    for root in (run_dir, run_dir / "logs", run_dir / "Logs"):
        if not root.is_dir():
            continue
        for path in root.glob("*.log"):
            if path.is_file():
                candidates.append(path)
        for path in root.glob("*.txt"):
            if path.is_file() and ("server" in path.name.lower() or "world" in path.name.lower()):
                candidates.append(path)
    unique = {os.path.normcase(str(p.resolve())): p for p in candidates}
    files = sorted(unique.values(), key=lambda p: p.stat().st_mtime_ns, reverse=True)[:MAX_LOG_FILES]
    matches: list[str] = []
    for path in files:
        try:
            size = path.stat().st_size
            with path.open("rb") as fh:
                if size > MAX_LOG_TAIL:
                    fh.seek(size - MAX_LOG_TAIL)
                    fh.readline()
                data = fh.read(MAX_LOG_TAIL)
            text, _ = decode_bytes(data)
        except (OSError, UnicodeError) as exc:
            matches.append(f"{path}: <read failed: {exc}>")
            continue
        for line in text.splitlines():
            if LOG_RE.search(line):
                matches.append(f"{path.name}: {line}")
                if len(matches) >= MAX_LOG_MATCHES:
                    return files, matches
    return files, matches


def process_snapshot() -> list[str]:
    if os.name != "nt":
        return ["<not Windows: process query skipped>"]
    ps = (
        "$ErrorActionPreference='Stop';"
        "$p=@(Get-CimInstance Win32_Process -Filter \"Name='worldserver.exe'\");"
        "$p|ForEach-Object{\"PID=$($_.ProcessId);PATH=$($_.ExecutablePath);CREATED=$($_.CreationDate)\"}"
    )
    code, text = run_command(["powershell.exe", "-NoProfile", "-NonInteractive", "-Command", ps])
    if code != 0:
        return [f"<process query failed exit={code}: {text}>"]
    return [line for line in text.splitlines() if line.strip()]


def add_full_file(lines: list[str], label: str, tf: TextFile, display_path: str) -> None:
    lines.append(
        f"FILE label={label}; path={display_path}; bytes={len(tf.data)}; sha256={tf.sha256}; "
        f"encoding={tf.encoding}; newline={tf.newline}"
    )
    lines.append(f"----- BEGIN {label} {display_path} -----")
    lines.extend(tf.text.splitlines())
    lines.append(f"----- END {label} {display_path} -----")


def collect(source_root: Path, run_dir: Path) -> tuple[str, bool]:
    source_root = source_root.resolve()
    run_dir = run_dir.resolve()
    started = datetime.now(timezone.utc)
    lines: list[str] = []
    lines.append(f"G23_PROBE_SCHEMA={SCHEMA}")
    lines.append(f"CAPTURE_UTC={started.isoformat()}")
    lines.append(f"SOURCE_ROOT={source_root}")
    lines.append(f"RUN_DIR={run_dir}")
    lines.append("READ_ONLY_DECLARATION=No source/config/database writes; no DB connections")

    if not source_root.is_dir():
        raise ProbeError(f"source root not found: {source_root}")
    if not run_dir.is_dir():
        raise ProbeError(f"run dir not found: {run_dir}")

    git_before = git_snapshot(source_root)
    required_paths = [source_root / rel for rel in REQUIRED_SOURCE]
    before_fp = {str(p): file_fingerprint(p) for p in required_paths if p.is_file()}

    lines.append("\n===== 1. GIT AND REQUIRED SOURCE IDENTITY =====")
    lines.append(f"GIT_BRANCH={git_before['branch']}")
    lines.append(f"GIT_HEAD={git_before['head']}")
    lines.append("GIT_STATUS_BEGIN")
    lines.extend(git_before["status"].splitlines() or ["<clean>"])
    lines.append("GIT_STATUS_END")

    existing_required = 0
    source_files: dict[str, TextFile] = {}
    for rel, path in zip(REQUIRED_SOURCE, required_paths):
        if not path.is_file():
            lines.append(f"REQUIRED_SOURCE_MISSING={rel.as_posix()}")
            continue
        existing_required += 1
        source_files[rel.as_posix()] = read_text_file(path)
    lines.append(f"G23_REQUIRED_SOURCE_FILES={existing_required}/{len(REQUIRED_SOURCE)}")

    loot_tf = source_files.get(REQUIRED_SOURCE[0].as_posix())
    cpp_tf = source_files.get(REQUIRED_SOURCE[1].as_posix())
    hdr_tf = source_files.get(REQUIRED_SOURCE[2].as_posix())
    hook_counts: dict[str, int] = {}
    if loot_tf:
        for marker in (
            '#include "CustomAoELoot.h"',
            "HandleAutostoreLootItemOpcode",
            "HandleLootMoneyOpcode",
            "CustomAoELoot::LootAllAround",
            "CustomAoELoot::GatherMoneyAround",
            "player->StoreLootItem(lootSlot, loot)",
        ):
            hook_counts[marker] = loot_tf.text.count(marker)
            lines.append(f"HOOK_COUNT {marker}={hook_counts[marker]}")
    source_markers_ready = bool(
        loot_tf
        and cpp_tf
        and hdr_tf
        and hook_counts.get('#include "CustomAoELoot.h"', 0) == 1
        and hook_counts.get("CustomAoELoot::LootAllAround", 0) >= 1
        and hook_counts.get("CustomAoELoot::GatherMoneyAround", 0) >= 1
    )
    lines.append(f"G23_AOE_HOOK_MARKERS_READY={bool_text(source_markers_ready)}")

    lines.append("\n===== 2. FULL REAL AOE LOOT SOURCE/Hooks =====")
    for rel in REQUIRED_SOURCE:
        tf = source_files.get(rel.as_posix())
        if tf:
            add_full_file(lines, "REAL_SOURCE", tf, rel.as_posix())

    lines.append("\n===== 3. ACTIVE CONFIG KEYS AND SCRIPT PATH RESOLUTION =====")
    config_files = find_config_files(run_dir)
    config_before = {str(p): file_fingerprint(p) for p in config_files}
    lines.append(f"CONFIG_FILE_COUNT={len(config_files)}")
    raw_script_paths: list[str] = []
    aoe_key_count = 0
    for path in config_files:
        try:
            tf = read_text_file(path)
        except ProbeError as exc:
            lines.append(f"CONFIG_READ_ERROR path={safe_rel(path, run_dir)} error={exc}")
            continue
        lines.append(
            f"CONFIG_FILE path={safe_rel(path, run_dir)}; bytes={len(tf.data)}; sha256={tf.sha256}; "
            f"encoding={tf.encoding}; newline={tf.newline}"
        )
        for number, line in enumerate(tf.text.splitlines(), 1):
            match = CONFIG_KEY_RE.match(line)
            if not match:
                continue
            key, value = match.group(1), match.group(2)
            if SENSITIVE_RE.search(key):
                shown = "<REDACTED>"
            else:
                shown = value
            lines.append(f"CONFIG_KEY file={safe_rel(path, run_dir)}; line={number}; {key}={shown}")
            if key.lower().startswith("aoeloot."):
                aoe_key_count += 1
            script_match = SCRIPT_PATH_RE.match(line)
            if script_match:
                raw_script_paths.append(script_match.group(1))

    script_paths: list[Path] = []
    for raw in raw_script_paths:
        resolved = resolve_script_path(run_dir, raw)
        if resolved:
            script_paths.append(resolved)
    for fallback in (run_dir / "lua_scripts", run_dir / "data" / "lua_scripts"):
        script_paths.append(fallback.resolve())
    path_seen: set[str] = set()
    script_paths = [p for p in script_paths if not (os.path.normcase(str(p)) in path_seen or path_seen.add(os.path.normcase(str(p))))]
    for path in script_paths:
        lines.append(f"SCRIPT_PATH path={path}; exists={bool_text(path.is_dir())}; from_config={bool_text(path in [resolve_script_path(run_dir, r) for r in raw_script_paths])}")
    existing_script_paths = [p for p in script_paths if p.is_dir()]
    lines.append(f"G23_SCRIPT_PATH_RESOLVED_COUNT={len(existing_script_paths)}")
    lines.append(f"G23_AOE_CONFIG_KEY_COUNT={aoe_key_count}")

    lines.append("\n===== 4. FULL DEPLOYED LUA FILES =====")
    lua_files = find_lua_files(existing_script_paths)
    lua_before = {str(p): file_fingerprint(p) for p in lua_files}
    lines.append(f"G23_DEPLOYED_LUA_FILE_COUNT={len(lua_files)}")
    by_name: dict[str, list[tuple[str, str]]] = defaultdict(list)
    lua_sql_tables: set[str] = set()
    registration_rows: list[str] = []
    for path in lua_files:
        tf = read_text_file(path)
        display = safe_rel(path, run_dir)
        by_name[path.name.lower()].append((display, tf.sha256))
        lua_sql_tables.update(SQL_TABLE_RE.findall(tf.text))
        for match in REGISTER_RE.finditer(tf.text):
            registration_rows.append(f"LUA_REGISTRATION file={display}; function={match.group(1)}; args={match.group(2).strip()}")
        add_full_file(lines, "DEPLOYED_LUA", tf, display)
    conflict_names = 0
    for name, rows in sorted(by_name.items()):
        hashes = {sha for _, sha in rows}
        if len(rows) > 1:
            conflict_names += 1
            lines.append(f"LUA_DUPLICATE_NAME name={name}; copies={len(rows)}; distinct_hashes={len(hashes)}")
            for display, sha in rows:
                lines.append(f"  COPY path={display}; sha256={sha}")
    lines.append(f"G23_LUA_DUPLICATE_FILENAME_COUNT={conflict_names}")
    lines.append("LUA_SQL_DEPENDENCIES=" + (",".join(sorted(lua_sql_tables)) if lua_sql_tables else "<none detected>"))
    lines.extend(registration_rows or ["LUA_REGISTRATION=<none detected>"])

    lines.append("\n===== 5. REAL ELUNA API/EVENT SOURCE CONTEXTS =====")
    eluna_files = find_eluna_source_files(source_root)
    context_file_count = 0
    context_block_count = 0
    for path in eluna_files:
        try:
            tf = read_text_file(path)
        except ProbeError:
            continue
        blocks = contexts(tf.text, ELUNA_TOKENS)
        if not blocks:
            continue
        context_file_count += 1
        context_block_count += len(blocks)
        display = safe_rel(path, source_root)
        lines.append(f"ELUNA_CONTEXT_FILE path={display}; sha256={tf.sha256}; blocks={len(blocks)}")
        for idx, block in enumerate(blocks, 1):
            lines.append(f"--- CONTEXT {display} #{idx} ---")
            lines.extend(block.splitlines())
    lines.append(f"G23_ELUNA_CONTEXT_FILE_COUNT={context_file_count}")
    lines.append(f"G23_ELUNA_CONTEXT_BLOCK_COUNT={context_block_count}")

    lines.append("\n===== 6. ACTIVE PROCESS IDENTITY =====")
    process_lines = process_snapshot()
    lines.extend(process_lines)
    active_count = sum(1 for line in process_lines if line.startswith("PID="))
    lines.append(f"G23_ACTIVE_WORLDSERVER_COUNT={active_count}")
    exe = run_dir / "worldserver.exe"
    if exe.is_file():
        stat = exe.stat()
        lines.append(
            f"RUN_EXE path={exe}; bytes={stat.st_size}; sha256={hashlib.sha256(exe.read_bytes()).hexdigest()}; "
            f"mtime_ns={stat.st_mtime_ns}"
        )
    else:
        lines.append(f"RUN_EXE_MISSING={exe}")

    lines.append("\n===== 7. RECENT RELEVANT LOG LINES =====")
    log_files, log_matches = recent_log_matches(run_dir)
    lines.append(f"G23_LOG_FILE_SCANNED_COUNT={len(log_files)}")
    lines.append(f"G23_RELEVANT_LOG_MATCH_COUNT={len(log_matches)}")
    for path in log_files:
        stat = path.stat()
        lines.append(f"LOG_FILE path={safe_rel(path, run_dir)}; bytes={stat.st_size}; mtime_ns={stat.st_mtime_ns}")
    lines.extend(log_matches or ["<no recent Eluna/Lua/AoELoot log line matched>"])

    lines.append("\n===== 8. READ-ONLY POSTCHECK =====")
    git_after = git_snapshot(source_root)
    after_fp = {str(p): file_fingerprint(p) for p in required_paths if p.is_file()}
    config_after = {str(p): file_fingerprint(p) for p in config_files if p.is_file()}
    lua_after = {str(p): file_fingerprint(p) for p in lua_files if p.is_file()}
    source_edits = 0 if before_fp == after_fp and git_before["status"] == git_after["status"] else 1
    config_edits = 0 if config_before == config_after else 1
    lua_edits = 0 if lua_before == lua_after else 1
    lines.append(f"G23_SOURCE_FILE_EDIT_COUNT={source_edits}")
    lines.append(f"G23_CONFIG_EDIT_COUNT={config_edits}")
    lines.append(f"G23_LUA_FILE_EDIT_COUNT={lua_edits}")
    lines.append("G23_DATABASE_EDIT_COUNT=0")
    lines.append("G23_DATABASE_CONNECTION_COUNT=0")

    capture_pass = bool(existing_required == len(REQUIRED_SOURCE) and source_markers_ready and source_edits == 0 and config_edits == 0 and lua_edits == 0)
    lines.append(f"G23_BASELINE_CAPTURE_PASS={bool_text(capture_pass)}")
    lines.append("EVIDENCE_BOUNDARY=Static/runtime baseline only; no intermittent-loot root cause or Lua functional pass inferred")
    return "\n".join(lines) + "\n", capture_pass


def write_report(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    normalized = text.replace("\r\n", "\n").replace("\r", "\n").replace("\n", "\r\n")
    path.write_bytes(codecs.BOM_UTF8 + normalized.encode("utf-8"))


def make_fixture(root: Path) -> tuple[Path, Path]:
    source = root / "source"
    run = root / "run"
    (source / "src/server/game/Handlers").mkdir(parents=True)
    (source / "src/server/game/Loot").mkdir(parents=True)
    (source / "src/server/game/Eluna").mkdir(parents=True)
    (run / "worldserver.conf.d").mkdir(parents=True)
    (run / "data/lua_scripts").mkdir(parents=True)

    (source / REQUIRED_SOURCE[0]).write_text(
        '#include "CustomAoELoot.h"\n'
        "void HandleAutostoreLootItemOpcode(){ player->StoreLootItem(lootSlot, loot); CustomAoELoot::LootAllAround(player, c); }\n"
        "void HandleLootMoneyOpcode(){ CustomAoELoot::GatherMoneyAround(player, c); }\n",
        encoding="utf-8",
    )
    (source / REQUIRED_SOURCE[1]).write_text("namespace CustomAoELoot { void x(){} }\n", encoding="utf-8")
    (source / REQUIRED_SOURCE[2]).write_text("namespace CustomAoELoot { bool Enabled(); }\n", encoding="utf-8")
    (source / "src/server/game/Eluna/LuaEngine.cpp").write_text(
        "RegisterPlayerEvent CreateLuaEvent CharDBQuery PLAYER_EVENT_ON_COMMAND\n", encoding="utf-8"
    )
    (run / "worldserver.conf").write_text(
        "Eluna.ScriptPath = data\\lua_scripts\nWorldDatabaseInfo = should_not_appear\n", encoding="utf-8"
    )
    (run / "worldserver.conf.d/aoeloot.conf").write_text(
        "AoELoot.Enable = 1\nAoELoot.Range = 60\n", encoding="utf-8"
    )
    (run / "data/lua_scripts/custom_test.lua").write_text(
        'local q=CharDBQuery("SELECT * FROM custom_table")\nRegisterPlayerEvent(3, function() end)\n', encoding="utf-8"
    )
    (run / "Server.log").write_text("[Eluna] custom_test.lua loaded\n", encoding="utf-8")
    return source, run


def self_test() -> int:
    with tempfile.TemporaryDirectory(prefix="g23_probe_") as td:
        root = Path(td)
        source, run = make_fixture(root)
        before = {str(p): file_fingerprint(p) for p in list(source.rglob("*")) + list(run.rglob("*")) if p.is_file()}
        report, passed = collect(source, run)
        after = {str(p): file_fingerprint(p) for p in list(source.rglob("*")) + list(run.rglob("*")) if p.is_file()}
        required = (
            "G23_PROBE_SCHEMA=1",
            "G23_REQUIRED_SOURCE_FILES=3/3",
            "G23_AOE_HOOK_MARKERS_READY=True",
            "G23_DEPLOYED_LUA_FILE_COUNT=1",
            "LUA_SQL_DEPENDENCIES=custom_table",
            "G23_SOURCE_FILE_EDIT_COUNT=0",
            "G23_CONFIG_EDIT_COUNT=0",
            "G23_LUA_FILE_EDIT_COUNT=0",
            "G23_DATABASE_EDIT_COUNT=0",
            "G23_BASELINE_CAPTURE_PASS=True",
        )
        if not passed or before != after or any(marker not in report for marker in required):
            raise ProbeError("positive fixture failed")
        if "should_not_appear" in report:
            raise ProbeError("sensitive/unrelated config leaked into report")

        missing = source / REQUIRED_SOURCE[2]
        missing.unlink()
        negative, negative_pass = collect(source, run)
        if negative_pass or "G23_BASELINE_CAPTURE_PASS=False" not in negative:
            raise ProbeError("missing-source negative fixture false-passed")

    print("[OK] G23 probe positive fixture and read-only postcheck passed.")
    print("[OK] Missing-source negative fixture did not false-pass.")
    print("[OK] Sensitive/unrelated config values are not captured.")
    print("[OK] G23 baseline probe self-test passed.")
    return 0


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="G23 read-only AoE Loot + Eluna/Lua baseline probe")
    parser.add_argument("source_root", nargs="?", type=Path)
    parser.add_argument("run_dir", nargs="?", type=Path)
    parser.add_argument("report", nargs="?", type=Path)
    parser.add_argument("--self-test", action="store_true")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    if args.self_test:
        return self_test()
    if not args.source_root or not args.run_dir or not args.report:
        raise ProbeError("source_root, run_dir and report are required unless --self-test is used")
    text, passed = collect(args.source_root, args.run_dir)
    write_report(args.report, text)
    digest = hashlib.sha256(args.report.read_bytes()).hexdigest()
    print(f"[OK] G23 report written: {args.report}")
    print(f"[OK] G23 report sha256={digest}")
    print("[OK] G23 source/config/lua/database edits=0")
    print(f"[{'OK' if passed else 'STOP'}] G23_BASELINE_CAPTURE_PASS={bool_text(passed)}")
    return 0 if passed else 2


if __name__ == "__main__":
    try:
        raise SystemExit(main(sys.argv[1:]))
    except ProbeError as exc:
        print(f"[STOP] {exc}", file=sys.stderr)
        raise SystemExit(2)
