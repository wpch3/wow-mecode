#!/usr/bin/env python3
"""Patch client-only 3.3.5a AreaTable.dbc old-world flyability flags.

The server AreaTable.dbc must remain original. Server G17-A/R2 remains the
authority for map, indoor, city, no-fly, arena, instance and blacklist policy.
"""

from __future__ import annotations

import argparse
import hashlib
import os
import struct
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path

HEADER = struct.Struct("<4s4I")
U32 = struct.Struct("<I")
EXPECTED_RECORD_COUNT = 2307
EXPECTED_FIELD_COUNT = 36
EXPECTED_RECORD_SIZE = 144
EXPECTED_INPUT_SIZE = 362740
EXPECTED_INPUT_SHA256 = "b0356ff41e5777896509ec52bc68af516b67d82a659dbc47757960aef98b62dd"
# Filled from the deterministic algorithm and locked by tests.
EXPECTED_PATCHED_ROWS = 948
EXPECTED_OUTPUT_SHA256 = "214c6935d11b784f0bf5e4855fb756126d9d667d622a346c3124ae748812b6a8"

FIELD_ID = 0
FIELD_MAP_ID = 1
FIELD_PARENT_AREA_ID = 2
FIELD_FLAGS = 4
AREA_FLAG_SLAVE_CAPITAL = 0x00000008
AREA_FLAG_SLAVE_CAPITAL2 = 0x00000020
AREA_FLAG_ARENA = 0x00000080
AREA_FLAG_CAPITAL = 0x00000100
AREA_FLAG_CITY = 0x00000200
AREA_FLAG_OUTLAND = 0x00000400
AREA_FLAG_ARENA_INSTANCE = 0x00010000
AREA_FLAG_INSIDE = 0x02000000
AREA_FLAG_NO_FLY_ZONE = 0x20000000
HARD_BLOCK = AREA_FLAG_NO_FLY_ZONE | AREA_FLAG_ARENA | AREA_FLAG_ARENA_INSTANCE
CITY_BLOCK = AREA_FLAG_SLAVE_CAPITAL | AREA_FLAG_SLAVE_CAPITAL2 | AREA_FLAG_CAPITAL | AREA_FLAG_CITY


class PatchError(RuntimeError):
    pass


@dataclass(frozen=True)
class Dbc:
    data: bytes
    records: int
    fields: int
    record_size: int
    string_size: int
    records_offset: int
    strings_offset: int


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def load(path: Path, require_locked_hash: bool = True) -> Dbc:
    data = path.read_bytes()
    if len(data) < HEADER.size:
        raise PatchError("AreaTable.dbc is too small")
    magic, records, fields, record_size, string_size = HEADER.unpack_from(data)
    if magic != b"WDBC":
        raise PatchError(f"unexpected magic: {magic!r}")
    if (records, fields, record_size) != (EXPECTED_RECORD_COUNT, EXPECTED_FIELD_COUNT, EXPECTED_RECORD_SIZE):
        raise PatchError(
            f"unexpected AreaTable layout: records={records} fields={fields} record_size={record_size}"
        )
    strings_offset = HEADER.size + records * record_size
    if strings_offset + string_size != len(data):
        raise PatchError("header size does not match file size")
    if require_locked_hash and (len(data) != EXPECTED_INPUT_SIZE or digest(data) != EXPECTED_INPUT_SHA256):
        raise PatchError(f"input is not the locked zhCN build-12340 AreaTable.dbc: size={len(data)} sha256={digest(data)}")
    return Dbc(data, records, fields, record_size, string_size, HEADER.size, strings_offset)


def offset(info: Dbc, row: int, field: int) -> int:
    return info.records_offset + row * info.record_size + field * 4


def get_u32(info: Dbc, row: int, field: int) -> int:
    return U32.unpack_from(info.data, offset(info, row, field))[0]


def rows_by_id(info: Dbc) -> dict[int, int]:
    result: dict[int, int] = {}
    for row in range(info.records):
        area_id = get_u32(info, row, FIELD_ID)
        if area_id in result:
            raise PatchError(f"duplicate AreaTable ID {area_id}")
        result[area_id] = row
    return result


def select_rows(info: Dbc) -> list[int]:
    by_id = rows_by_id(info)
    selected: list[int] = []
    for row in range(info.records):
        map_id = get_u32(info, row, FIELD_MAP_ID)
        if map_id not in (0, 1):
            continue
        flags = get_u32(info, row, FIELD_FLAGS)
        parent_id = get_u32(info, row, FIELD_PARENT_AREA_ID)
        parent_flags = 0
        if parent_id:
            parent_row = by_id.get(parent_id)
            if parent_row is None:
                raise PatchError(f"area {get_u32(info, row, FIELD_ID)} has missing parent {parent_id}")
            parent_flags = get_u32(info, parent_row, FIELD_FLAGS)
        combined = flags | parent_flags
        if combined & HARD_BLOCK:
            continue
        if combined & AREA_FLAG_INSIDE:
            continue
        if combined & CITY_BLOCK:
            continue
        if flags & AREA_FLAG_OUTLAND:
            continue
        selected.append(row)
    return selected


def make_patched(info: Dbc, selected: list[int]) -> bytes:
    out = bytearray(info.data)
    for row in selected:
        at = offset(info, row, FIELD_FLAGS)
        flags = U32.unpack_from(out, at)[0]
        U32.pack_into(out, at, flags | AREA_FLAG_OUTLAND)
    return bytes(out)


def verify_diff(original: Dbc, patched: bytes, selected: list[int]) -> None:
    if len(patched) != len(original.data):
        raise PatchError("patched size changed")
    if patched[:HEADER.size] != original.data[:HEADER.size]:
        raise PatchError("header changed")
    expected_bytes = {
        offset(original, row, FIELD_FLAGS) + byte
        for row in selected for byte in range(4)
    }
    actual = {i for i, pair in enumerate(zip(original.data, patched)) if pair[0] != pair[1]}
    if not actual:
        raise PatchError("patch changed no bytes")
    if not actual <= expected_bytes:
        raise PatchError(f"unexpected changed byte offsets: {sorted(actual - expected_bytes)[:10]}")
    selected_set = set(selected)
    for row in range(original.records):
        start = original.records_offset + row * original.record_size
        end = start + original.record_size
        if row not in selected_set and original.data[start:end] != patched[start:end]:
            raise PatchError(f"unselected row {row} changed")
        before = get_u32(original, row, FIELD_FLAGS)
        after = U32.unpack_from(patched, offset(original, row, FIELD_FLAGS))[0]
        expected = before | AREA_FLAG_OUTLAND if row in selected_set else before
        if after != expected:
            raise PatchError(f"flags mismatch at row {row}: {before:#x}->{after:#x}, expected {expected:#x}")
    if patched[original.strings_offset:] != original.data[original.strings_offset:]:
        raise PatchError("string block changed")


def atomic_write(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temp_name = tempfile.mkstemp(prefix=path.name + ".", suffix=".tmp", dir=path.parent)
    temp = Path(temp_name)
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temp, path)
    except Exception:
        temp.unlink(missing_ok=True)
        raise


def patch(input_path: Path, output_path: Path, report_path: Path | None) -> None:
    if input_path.resolve() == output_path.resolve():
        raise PatchError("input and output must differ")
    info = load(input_path)
    selected = select_rows(info)
    out = make_patched(info, selected)
    verify_diff(info, out, selected)
    if EXPECTED_PATCHED_ROWS and len(selected) != EXPECTED_PATCHED_ROWS:
        raise PatchError(f"patched row count changed: {len(selected)}")
    if EXPECTED_OUTPUT_SHA256 and digest(out) != EXPECTED_OUTPUT_SHA256:
        raise PatchError(f"patched output hash changed: {digest(out)}")
    if output_path.exists() and output_path.read_bytes() != out:
        raise PatchError(f"refusing to overwrite different output: {output_path}")
    if not output_path.exists():
        atomic_write(output_path, out)
    lines = [
        "G17R3_CLIENT_AREATABLE_PATCH=PASS",
        f"INPUT_SIZE={len(info.data)}",
        f"INPUT_SHA256={digest(info.data)}",
        f"OUTPUT_SIZE={len(out)}",
        f"OUTPUT_SHA256={digest(out)}",
        f"PATCHED_ROWS={len(selected)}",
        "PATCHED_MAPS=0,1",
        "HARD_NO_FLY_ARENA_INSTANCE_PRESERVED=True",
        "STATIC_CITY_BOUNDARIES_PRESERVED=True",
        "OLDWORLD_COMBINED_INSIDE_ROWS=0",
        "LIVE_SERVER_OUTDOOR_CHECK_REQUIRED=True",
        "SERVER_DBC_MUST_REMAIN_ORIGINAL=True",
    ]
    report = "\n".join(lines) + "\n"
    if report_path:
        atomic_write(report_path, report.encode("utf-8"))
    sys.stdout.write(report)


def verify(original_path: Path, patched_path: Path) -> None:
    info = load(original_path)
    selected = select_rows(info)
    patched = patched_path.read_bytes()
    verify_diff(info, patched, selected)
    expected = make_patched(info, selected)
    if patched != expected:
        raise PatchError("patched file is not deterministic expected output")
    if EXPECTED_PATCHED_ROWS and len(selected) != EXPECTED_PATCHED_ROWS:
        raise PatchError("row count mismatch")
    if EXPECTED_OUTPUT_SHA256 and digest(patched) != EXPECTED_OUTPUT_SHA256:
        raise PatchError("output hash mismatch")
    print("G17R3_CLIENT_AREATABLE_VERIFY=PASS")
    print(f"ORIGINAL_SHA256={digest(info.data)}")
    print(f"PATCHED_SHA256={digest(patched)}")
    print(f"PATCHED_ROWS={len(selected)}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    p = sub.add_parser("patch")
    p.add_argument("--input", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--report", type=Path)
    v = sub.add_parser("verify")
    v.add_argument("--original", type=Path, required=True)
    v.add_argument("--patched", type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.command == "patch":
            patch(args.input, args.output, args.report)
        else:
            verify(args.original, args.patched)
        return 0
    except (PatchError, OSError) as exc:
        print(f"G17R3_CLIENT_AREATABLE_ERROR={exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
