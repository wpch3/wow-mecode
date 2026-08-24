#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
import hashlib
import sys

EXPECTED_SIZE = 492
EXPECTED_SHA256 = "182a1878251ea7d4cb6af76a4721028a57750f7de47acb1eff6793a31eaef32a"
EXPECTED_HEADER = [
    "G17B0_RESULT", "database_name", "source_rows", "source_vehicle70_rows",
    "target_rows", "target_exact", "action_rows", "action_exact",
    "movement_exact", "script_rows", "script_exact",
]
EXPECTED_ROW = [
    "G17B0_WORLD_CHECK_PASS", "world", "1", "1", "1", "1", "4", "4",
    "1", "4", "4",
]


def parse_pipe_row(line: str) -> list[str]:
    if not line.startswith("|") or not line.endswith("|"):
        raise AssertionError("row is not a complete pipe-delimited table row")
    return [cell.strip() for cell in line[1:-1].split("|")]


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit(f"usage: {Path(sys.argv[0]).name} RESULT.txt")
    path = Path(sys.argv[1])
    data = path.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    assert len(data) == EXPECTED_SIZE, f"unexpected size: {len(data)}"
    assert digest == EXPECTED_SHA256, f"unexpected SHA-256: {digest}"

    text = data.decode("utf-8-sig")
    lines = text.splitlines()
    assert len(lines) == 3, f"expected exactly 3 logical lines, got {len(lines)}"
    header = parse_pipe_row(lines[0])
    divider = parse_pipe_row(lines[1])
    row = parse_pipe_row(lines[2])
    assert header == EXPECTED_HEADER, f"unexpected header: {header}"
    assert len(divider) == len(header), "divider width mismatch"
    assert all(cell and set(cell) == {"-"} for cell in divider), "invalid divider"
    assert row == EXPECTED_ROW, f"unexpected result row: {row}"
    upper = text.upper()
    assert "BLOCKED_" not in upper and "FAILED_" not in upper
    assert "SQL ERROR" not in upper and "ERROR 1267" not in upper

    print(f"G17B0_WORLD_POSTCHECK_RESULT_SIZE={len(data)}")
    print(f"G17B0_WORLD_POSTCHECK_RESULT_SHA256={digest}")
    print("G17B0_WORLD_POSTCHECK_ROWS=1")
    print("G17B0_WORLD_POSTCHECK_DATABASE=world")
    print("G17B0_WORLD_POSTCHECK_SOURCE=1/1")
    print("G17B0_WORLD_POSTCHECK_TARGET=1/1")
    print("G17B0_WORLD_POSTCHECK_ACTIONS=4/4")
    print("G17B0_WORLD_POSTCHECK_MOVEMENT=1")
    print("G17B0_WORLD_POSTCHECK_SCRIPTS=4/4")
    print("G17B0_WORLD_POSTCHECK_V3_ACCEPTANCE=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
