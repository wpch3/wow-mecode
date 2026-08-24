#!/usr/bin/env python3
"""G11 runtime read-only diagnostic v2.

Reads the deployed config, running process metadata, worldserver binary, logger
configuration, and runtime log files. It never edits source, config, or databases;
its only write is the requested UTF-8-BOM report.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

EXPECTED_G11 = {
    "NpcBot.Autonomy.Perception.Enable": "1",
    "NpcBot.Autonomy.Perception.Range": "60",
    "NpcBot.Autonomy.Perception.IntervalMin": "4000",
    "NpcBot.Autonomy.Perception.IntervalMax": "8000",
    "NpcBot.Autonomy.Perception.LogCooldown": "15000",
    "NpcBot.Autonomy.Perception.ReadOnly": "1",
}
MARKER = "[G11-PERCEPTION]"
REQUIRED_LOG_KEYS = (
    "bot",
    "name",
    "scene",
    "map",
    "zone",
    "player",
    "playerName",
    "kind",
    "dist",
    "levelDelta",
    "hp",
    "combat",
    "group",
    "candidates",
    "seen",
    "helpCandidate",
    "readonly",
)
ASSIGNMENT_RE = re.compile(r"^\s*([^#;][^=]*?)\s*=\s*(.*?)\s*$")
LOG_FIELD_RE = re.compile(r"(?:^|\s)([A-Za-z][A-Za-z0-9]*)=([^\s]+)")


@dataclass(frozen=True)
class ConfigHit:
    path: Path
    line_number: int
    key: str
    value: str


@dataclass(frozen=True)
class LogHit:
    path: Path
    line_number: int
    line: str


def decode_bytes(data: bytes) -> str:
    for encoding in ("utf-8-sig", "utf-8", "gb18030"):
        try:
            return data.decode(encoding)
        except UnicodeDecodeError:
            pass
    return data.decode("utf-8", errors="replace")


def decode_text(path: Path) -> str:
    return decode_bytes(path.read_bytes())


def hash_and_find_marker(path: Path) -> tuple[str, bool]:
    """Hash a potentially large exe and find MARKER without loading it all."""
    digest = hashlib.sha256()
    marker = MARKER.encode("ascii")
    overlap = b""
    found = False
    with path.open("rb") as handle:
        while chunk := handle.read(1024 * 1024):
            digest.update(chunk)
            if marker in overlap + chunk:
                found = True
            overlap = chunk[-(len(marker) - 1):]
    return digest.hexdigest(), found


def config_files(run_dir: Path) -> list[Path]:
    files: list[Path] = []
    main = run_dir / "worldserver.conf"
    if main.is_file():
        files.append(main)
    conf_dir = run_dir / "worldserver.conf.d"
    if conf_dir.is_dir():
        files.extend(sorted(conf_dir.glob("*.conf"), key=lambda p: p.name.casefold()))
    return files


def parse_config_hits(paths: Iterable[Path]) -> list[ConfigHit]:
    hits: list[ConfigHit] = []
    for path in paths:
        for line_number, line in enumerate(decode_text(path).splitlines(), 1):
            match = ASSIGNMENT_RE.match(line)
            if not match:
                continue
            hits.append(
                ConfigHit(
                    path=path,
                    line_number=line_number,
                    key=match.group(1).strip(),
                    value=match.group(2).strip(),
                )
            )
    return hits


def query_worldserver_processes() -> tuple[list[dict[str, object]], str | None]:
    if os.name != "nt":
        return [], "process query skipped: not running on Windows"
    command = (
        "$p=@(Get-CimInstance Win32_Process -Filter \"Name='worldserver.exe'\" | "
        "Select-Object ProcessId,ExecutablePath,CreationDate);"
        "$p | ConvertTo-Json -Compress"
    )
    try:
        completed = subprocess.run(
            ["powershell.exe", "-NoProfile", "-Command", command],
            check=False,
            capture_output=True,
            text=True,
            timeout=20,
        )
    except (OSError, subprocess.TimeoutExpired) as exc:
        return [], f"process query failed: {exc}"
    if completed.returncode != 0:
        detail = completed.stderr.strip() or completed.stdout.strip()
        return [], f"process query failed (exit {completed.returncode}): {detail}"
    raw = completed.stdout.strip()
    if not raw:
        return [], None
    try:
        parsed = json.loads(raw)
    except json.JSONDecodeError as exc:
        return [], f"process query returned invalid JSON: {exc}: {raw[:300]}"
    if isinstance(parsed, dict):
        parsed = [parsed]
    if not isinstance(parsed, list):
        return [], f"process query returned unexpected JSON type: {type(parsed).__name__}"
    return [item for item in parsed if isinstance(item, dict)], None


def normalized_windows_path(value: str) -> str:
    return value.replace("/", "\\").rstrip("\\").casefold()


def scan_logs(run_dir: Path) -> tuple[list[Path], list[LogHit]]:
    """Stream log lines so large historical logs do not have to fit in memory."""
    log_paths = sorted(run_dir.rglob("*.log"), key=lambda p: str(p).casefold())
    hits: list[LogHit] = []
    for path in log_paths:
        try:
            with path.open("rb") as handle:
                for line_number, raw_line in enumerate(handle, 1):
                    line = decode_bytes(raw_line).strip()
                    if MARKER in line:
                        hits.append(
                            LogHit(path=path, line_number=line_number, line=line)
                        )
        except OSError:
            continue
    return log_paths, hits


def parse_log_fields(line: str) -> tuple[dict[str, str], list[str]]:
    """Parse exact whitespace-delimited key=value tokens and flag duplicates."""
    fields: dict[str, str] = {}
    duplicates: list[str] = []
    for key, value in LOG_FIELD_RE.findall(line):
        if key in fields:
            duplicates.append(key)
        else:
            fields[key] = value
    return fields, duplicates


def analyze(
    run_dir: Path,
    *,
    process_records: list[dict[str, object]] | None = None,
    process_error: str | None = None,
) -> tuple[str, dict[str, object]]:
    run_dir = run_dir.resolve()
    conf = run_dir / "worldserver.conf.d" / "g11_perception.conf"
    exe = run_dir / "worldserver.exe"
    main_conf = run_dir / "worldserver.conf"
    lines: list[str] = []

    def add(text: str = "") -> None:
        lines.append(text)

    add("G11 runtime read-only diagnostic v2")
    add(f"RunDir={run_dir}")
    add("Source/config/database edits=0")

    add()
    add("=== 1. REQUIRED PATHS ===")
    for path in (conf, exe, main_conf):
        add(f"EXISTS={path.is_file()} PATH={path}")
    if not conf.is_file():
        raise FileNotFoundError(f"G11 config not found: {conf}")
    if not exe.is_file():
        raise FileNotFoundError(f"worldserver.exe not found: {exe}")

    cfg_files = config_files(run_dir)
    cfg_hits = parse_config_hits(cfg_files)

    add()
    add("=== 2. G11 CONFIG VALUES AND DUPLICATE COUNTS ===")
    config_exact = True
    for key, expected in EXPECTED_G11.items():
        matching = [hit for hit in cfg_hits if hit.key.casefold() == key.casefold()]
        actual = matching[0].value if len(matching) == 1 else (
            "<MISSING>" if not matching else "<DUPLICATE>"
        )
        passed = len(matching) == 1 and actual == expected
        config_exact = config_exact and passed
        locations = ";".join(
            f"{hit.path}:{hit.line_number}" for hit in matching
        ) or "<none>"
        add(
            f"PASS={passed} COUNT={len(matching)} EXPECTED={expected} "
            f"ACTUAL={actual} KEY={key} LOCATIONS={locations}"
        )
    add(f"CONFIG_EXACT={config_exact}")

    if process_records is None:
        process_records, process_error = query_worldserver_processes()
    add()
    add("=== 3. ACTIVE WORLDSERVER PROCESS ===")
    if process_error:
        add(f"PROCESS_QUERY_WARNING={process_error}")
    if not process_records:
        add("ACTIVE_PROCESS_COUNT=0")
    else:
        add(f"ACTIVE_PROCESS_COUNT={len(process_records)}")
        for record in process_records:
            add(
                "PROCESS "
                f"PID={record.get('ProcessId', '<unknown>')} "
                f"PATH={record.get('ExecutablePath', '<unknown>')} "
                f"CREATED={record.get('CreationDate', '<unknown>')}"
            )
    expected_process_path = normalized_windows_path(str(exe))
    process_path_match_count = sum(
        normalized_windows_path(str(record.get("ExecutablePath", "")))
        == expected_process_path
        for record in process_records
    )
    # Multiple active worldservers are ambiguous even if one path is correct.
    process_path_match = len(process_records) == 1 and process_path_match_count == 1
    add(f"PROCESS_EXPECTED_PATH_MATCH_COUNT={process_path_match_count}")
    add(f"PROCESS_PATH_MATCH={process_path_match}")

    exe_hash, exe_marker = hash_and_find_marker(exe)
    stat = exe.stat()
    add()
    add("=== 4. EXE METADATA AND G11 MARKER ===")
    add(f"EXE_PATH={exe}")
    add(f"EXE_SIZE={stat.st_size}")
    add(f"EXE_MTIME_NS={stat.st_mtime_ns}")
    add(f"EXE_SHA256={exe_hash}")
    add(f"EXE_G11_MARKER={exe_marker}")

    logger_hits = [hit for hit in cfg_hits if hit.key.casefold() == "logger.npcbots"]
    logger_levels: list[int] = []
    add()
    add("=== 5. NPCBOTS LOGGER CONFIG ===")
    if not logger_hits:
        add("LOGGER_NPCBOTS_COUNT=0")
    for hit in logger_hits:
        first = hit.value.split(",", 1)[0].strip()
        try:
            level = int(first)
            logger_levels.append(level)
        except ValueError:
            level = -1
        add(
            f"LOGGER PATH={hit.path}:{hit.line_number} VALUE={hit.value} LEVEL={level}"
        )
    logger_info_allowed = (
        len(logger_hits) == 1
        and len(logger_levels) == 1
        and 1 <= logger_levels[0] <= 3
    )
    add(f"LOGGER_INFO_ALLOWED={logger_info_allowed}")

    log_paths, log_hits = scan_logs(run_dir)
    parsed_log_hits: list[tuple[LogHit, dict[str, str]]] = []
    bad_field_hits: list[tuple[LogHit, list[str]]] = []
    for hit in log_hits:
        fields, duplicates = parse_log_fields(hit.line)
        parsed_log_hits.append((hit, fields))
        issues = [f"{key}=<missing>" for key in REQUIRED_LOG_KEYS if key not in fields]
        issues.extend(f"{key}=<duplicate>" for key in duplicates)
        if fields.get("readonly") != "1":
            issues.append(f"readonly=<expected-1,actual-{fields.get('readonly', 'missing')}>")
        if fields.get("kind") not in {"human", "playerbot"}:
            issues.append(f"kind=<unexpected-{fields.get('kind', 'missing')}>")
        if issues:
            bad_field_hits.append((hit, issues))
    human_hits = [hit for hit, fields in parsed_log_hits if fields.get("kind") == "human"]
    playerbot_hits = [
        hit for hit, fields in parsed_log_hits if fields.get("kind") == "playerbot"
    ]

    add()
    add("=== 6. G11 RUNTIME LOG MATCHES ===")
    add(f"LOG_FILES_SCANNED={len(log_paths)}")
    add(f"G11_LOG_MATCHES={len(log_hits)}")
    add(f"G11_HUMAN_MATCHES={len(human_hits)}")
    add(f"G11_PLAYERBOT_MATCHES={len(playerbot_hits)}")
    add(f"G11_LINES_WITH_MISSING_FIELDS={len(bad_field_hits)}")
    for hit in log_hits[-50:]:
        add(f"LOG {hit.path}:{hit.line_number}: {hit.line}")
    for hit, missing in bad_field_hits[-20:]:
        add(
            f"MISSING_FIELDS {hit.path}:{hit.line_number}: {','.join(missing)}"
        )

    human_minimum_pass = (
        config_exact
        and exe_marker
        and logger_info_allowed
        and process_path_match
        and bool(human_hits)
        and not bad_field_hits
    )
    playerbot_optional_pass = bool(playerbot_hits) and not any(
        item[0] in playerbot_hits for item in bad_field_hits
    )
    add()
    add("=== 7. VERDICT ===")
    add(f"G11_HUMAN_MINIMUM_PASS={human_minimum_pass}")
    add(f"G11_PLAYERBOT_OPTIONAL_T7_PASS={playerbot_optional_pass}")
    if human_minimum_pass:
        add("NEXT=Human minimum is proven; PlayerBot T7 is optional, then continue T1-T10.")
    elif log_hits:
        add("NEXT=G11 logs exist; inspect exact lines and the failed prerequisite above before changing code.")
    else:
        add("NEXT=No G11 log exists; verify a free wandering unrecruited NPCBot observer before changing code.")
    add("[OK] Read-only diagnostic complete; only the report file will be written.")

    metrics: dict[str, object] = {
        "config_exact": config_exact,
        "process_path_match": process_path_match,
        "exe_marker": exe_marker,
        "logger_info_allowed": logger_info_allowed,
        "log_hits": len(log_hits),
        "human_hits": len(human_hits),
        "playerbot_hits": len(playerbot_hits),
        "bad_field_hits": len(bad_field_hits),
        "human_minimum_pass": human_minimum_pass,
        "playerbot_optional_pass": playerbot_optional_pass,
    }
    return "\n".join(lines) + "\n", metrics


def sample_log(kind: str, player: int, player_name: str) -> str:
    return (
        f"[G11-PERCEPTION] bot=129500 name=Observer scene=5 map=0 zone=12 "
        f"player={player} playerName={player_name} kind={kind} dist=10.0 "
        "levelDelta=0 hp=100.0 combat=0 group=0 candidates=1 seen=first "
        "helpCandidate=0 readonly=1"
    )


def write_report(output: Path, report: str) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(report, encoding="utf-8-sig", newline="\r\n")


def self_test_require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(f"self-test assertion failed: {message}")


def run_self_test() -> None:
    with tempfile.TemporaryDirectory(prefix="g11_runtime_probe_") as temp:
        run_dir = Path(temp) / "RelWithDebInfo"
        conf_dir = run_dir / "worldserver.conf.d"
        conf_dir.mkdir(parents=True)
        (conf_dir / "g11_perception.conf").write_text(
            "[worldserver]\n"
            + "\n".join(f"{key} = {value}" for key, value in EXPECTED_G11.items())
            + "\n",
            encoding="utf-8",
        )
        (run_dir / "worldserver.conf").write_text(
            "[worldserver]\nLogger.npcbots = 2,NpcBots Server\n",
            encoding="utf-8",
        )
        exe = run_dir / "worldserver.exe"
        exe.write_bytes(b"fixture\x00[G11-PERCEPTION]\x00binary")
        (run_dir / "Server.log").write_text(
            sample_log("human", 1, "Human")
            + "\n"
            + sample_log("playerbot", 2, "PBot")
            + "\n",
            encoding="utf-8",
        )
        # analyze() canonicalizes run_dir with resolve(). The fixture process
        # path must use the same canonical form, especially on Windows where
        # TEMP may be exposed through an 8.3 path or a junction.
        process = [{
            "ProcessId": 123,
            "ExecutablePath": str(exe.resolve()),
            "CreationDate": "fixture",
        }]
        report, metrics = analyze(run_dir, process_records=process)
        expected_metrics = {
            "config_exact": True,
            "process_path_match": True,
            "exe_marker": True,
            "logger_info_allowed": True,
            "log_hits": 2,
            "human_hits": 1,
            "playerbot_hits": 1,
            "bad_field_hits": 0,
            "human_minimum_pass": True,
            "playerbot_optional_pass": True,
        }
        mismatches = {
            key: {"expected": expected_metrics[key], "actual": metrics.get(key)}
            for key in expected_metrics
            if metrics.get(key) != expected_metrics[key]
        }
        self_test_require(
            not mismatches,
            f"positive fixture metric mismatch: {mismatches}",
        )
        self_test_require("G11_LOG_MATCHES=2" in report, "positive log count missing")
        self_test_require(
            "kind=human" in report and "kind=playerbot" in report,
            "positive human/playerbot lines missing",
        )

        # Exercise the final renderer, not only the in-memory analysis. This is
        # the regression gate for v1's hidden-column/report-output defect.
        rendered = Path(temp) / "rendered" / "g11_runtime_diag_v2.txt"
        write_report(rendered, report)
        rendered_bytes = rendered.read_bytes()
        self_test_require(
            rendered_bytes.startswith(b"\xef\xbb\xbf"),
            "rendered report has no UTF-8 BOM",
        )
        self_test_require(b"\r\n" in rendered_bytes, "rendered report has no CRLF")
        rendered_text = rendered_bytes.decode("utf-8-sig")
        for required in (
            "CONFIG_EXACT=True",
            "PROCESS_PATH_MATCH=True",
            "EXE_G11_MARKER=True",
            "LOGGER_INFO_ALLOWED=True",
            "G11_HUMAN_MATCHES=1",
            "G11_LINES_WITH_MISSING_FIELDS=0",
            "kind=human",
            "G11_HUMAN_MINIMUM_PASS=True",
        ):
            self_test_require(
                required in rendered_text,
                f"rendered report missing: {required}",
            )

        # Negative fixture verifies that the probe does not report a false pass.
        (conf_dir / "g11_perception.conf").write_text(
            "[worldserver]\n"
            + "\n".join(
                f"{key} = {'0' if key.endswith('.Enable') else value}"
                for key, value in EXPECTED_G11.items()
            )
            + "\n",
            encoding="utf-8",
        )
        (run_dir / "worldserver.conf").write_text(
            "[worldserver]\nLogger.npcbots = 4,NpcBots Server\n",
            encoding="utf-8",
        )
        exe.write_bytes(b"fixture without marker")
        (run_dir / "Server.log").write_text("no g11 line\n", encoding="utf-8")
        _, negative = analyze(run_dir, process_records=process)
        for key, expected in {
            "config_exact": False,
            "exe_marker": False,
            "logger_info_allowed": False,
            "log_hits": 0,
            "human_minimum_pass": False,
        }.items():
            self_test_require(
                negative.get(key) == expected,
                f"negative fixture {key}: expected {expected}, actual {negative.get(key)}",
            )

        # A partial human line must not pass merely because kind=human exists.
        (conf_dir / "g11_perception.conf").write_text(
            "[worldserver]\n"
            + "\n".join(f"{key} = {value}" for key, value in EXPECTED_G11.items())
            + "\n",
            encoding="utf-8",
        )
        (run_dir / "worldserver.conf").write_text(
            "[worldserver]\nLogger.npcbots = 2,NpcBots Server\n",
            encoding="utf-8",
        )
        exe.write_bytes(b"fixture\x00[G11-PERCEPTION]\x00binary")
        (run_dir / "Server.log").write_text(
            "[G11-PERCEPTION] kind=human readonly=10\n"
            "[G11-PERCEPTION] kind=humanity readonly=1\n",
            encoding="utf-8",
        )
        malformed_report, malformed = analyze(run_dir, process_records=process)
        for key, expected in {
            "log_hits": 2,
            "human_hits": 1,  # humanity must not substring-match
            "bad_field_hits": 2,
            "human_minimum_pass": False,
        }.items():
            self_test_require(
                malformed.get(key) == expected,
                f"malformed fixture {key}: expected {expected}, actual {malformed.get(key)}",
            )
        for required in (
            "readonly=<expected-1,actual-10>",
            "kind=<unexpected-humanity>",
            "MISSING_FIELDS ",
        ):
            self_test_require(
                required in malformed_report,
                f"malformed report missing: {required}",
            )

    print("[OK] Positive fixture + final report rendering passed.")
    print("[OK] Negative fixtures: bad prerequisites/no-log and malformed human line did not false-pass.")
    print("[OK] G11 runtime probe v2 self-test passed.")


def default_output() -> Path:
    home = Path.home()
    desktop = home / "Desktop"
    return desktop / "g11_runtime_diag_v2.txt"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "run_dir",
        nargs="?",
        default=r"D:\TC-Build\bin\RelWithDebInfo",
        help="RelWithDebInfo runtime directory",
    )
    parser.add_argument(
        "output",
        nargs="?",
        default=str(default_output()),
        help="UTF-8-BOM report path",
    )
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args(argv)

    if args.self_test:
        run_self_test()
        return 0

    report, _ = analyze(Path(args.run_dir))
    output = Path(args.output)
    write_report(output, report)
    sys.stdout.write(report)
    print(f"[OK] Wrote UTF-8-BOM report: {output}")
    print("[OK] Source/config/database edits: 0")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"[FAILED] G11 runtime probe v2: {exc}", file=sys.stderr)
        raise SystemExit(2)
