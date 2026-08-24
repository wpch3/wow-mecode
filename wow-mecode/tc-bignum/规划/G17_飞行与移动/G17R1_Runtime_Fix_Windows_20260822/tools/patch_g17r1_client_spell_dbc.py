#!/usr/bin/env python3
"""Create a client-only WotLK 3.3.5a Spell.dbc flight-mount patch.

The server must keep its original Spell.dbc.  G17-A's server-side
SpellInfo::CheckLocation remains the authority for allowed and blocked areas.
This tool only clears the client's local "Only in Outland/Northrend" bit on
mount/flying-mount rows so pure flying mounts can send their cast request.
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
from typing import Optional

WDBC_HEADER = struct.Struct("<4s4I")
UINT32 = struct.Struct("<I")
EXPECTED_FIELD_COUNT = 234
EXPECTED_RECORD_SIZE = EXPECTED_FIELD_COUNT * 4
ATTRIBUTES_EX_D_FIELD = 8
CAST_ONLY_IN_OUTLAND = 0x04000000
EFFECT_AURA_FIRST_FIELD = 95
EFFECT_AURA_LAST_FIELD = 98
SPELL_AURA_MOUNTED = 78
SPELL_AURA_FLY = 201
SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED = 207
CLIENT_UNLOCK_AURAS = {
    SPELL_AURA_MOUNTED,
    SPELL_AURA_FLY,
    SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED,
}
NAME_FIRST_FIELD = 136
NAME_LAST_FIELD = 152
PROTO_DRAKE_SPELL_IDS = {
    59569, 59961, 59976, 59996, 60002, 60021, 60024, 61294, 63956, 63963
}


class PatchError(RuntimeError):
    pass


@dataclass(frozen=True)
class DbcInfo:
    data: bytes
    record_count: int
    field_count: int
    record_size: int
    string_size: int
    records_offset: int
    strings_offset: int


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def load_dbc(path: Path) -> DbcInfo:
    try:
        data = path.read_bytes()
    except OSError as exc:
        raise PatchError(f"cannot read {path}: {exc}") from exc

    if len(data) < WDBC_HEADER.size:
        raise PatchError(f"file is too small for a WDBC header: {path}")

    magic, record_count, field_count, record_size, string_size = WDBC_HEADER.unpack_from(data)
    if magic != b"WDBC":
        raise PatchError(f"expected WDBC magic, found {magic!r}: {path}")
    if field_count != EXPECTED_FIELD_COUNT:
        raise PatchError(
            f"expected WotLK build-12340 Spell.dbc field_count={EXPECTED_FIELD_COUNT}, "
            f"found {field_count}"
        )
    if record_size != EXPECTED_RECORD_SIZE:
        raise PatchError(
            f"expected Spell.dbc record_size={EXPECTED_RECORD_SIZE}, found {record_size}"
        )
    if record_count < 1000:
        raise PatchError(f"implausible Spell.dbc record_count={record_count}")

    records_offset = WDBC_HEADER.size
    strings_offset = records_offset + record_count * record_size
    expected_size = strings_offset + string_size
    if expected_size != len(data):
        raise PatchError(
            f"WDBC size mismatch: header implies {expected_size} bytes, file has {len(data)}"
        )
    if string_size < 1 or data[strings_offset] != 0:
        raise PatchError("invalid WDBC string block: offset zero is not an empty string")

    return DbcInfo(
        data=data,
        record_count=record_count,
        field_count=field_count,
        record_size=record_size,
        string_size=string_size,
        records_offset=records_offset,
        strings_offset=strings_offset,
    )


def field_offset(info: DbcInfo, row: int, field: int) -> int:
    return info.records_offset + row * info.record_size + field * 4


def get_u32(info: DbcInfo, row: int, field: int) -> int:
    return UINT32.unpack_from(info.data, field_offset(info, row, field))[0]


def get_string(info: DbcInfo, string_offset: int) -> str:
    if string_offset <= 0:
        return ""
    if string_offset >= info.string_size:
        raise PatchError(f"string offset outside string block: {string_offset}")
    start = info.strings_offset + string_offset
    end = info.data.find(b"\0", start, info.strings_offset + info.string_size)
    if end < 0:
        raise PatchError(f"unterminated string at offset {string_offset}")
    return info.data[start:end].decode("utf-8", errors="replace")


def get_name(info: DbcInfo, row: int) -> str:
    for field in range(NAME_FIRST_FIELD, NAME_LAST_FIELD):
        value = get_string(info, get_u32(info, row, field))
        if value:
            return value
    return "<unnamed>"


def select_rows(info: DbcInfo) -> list[int]:
    selected: list[int] = []
    seen_ids: set[int] = set()
    for row in range(info.record_count):
        spell_id = get_u32(info, row, 0)
        if spell_id in seen_ids:
            raise PatchError(f"duplicate spell ID {spell_id}")
        seen_ids.add(spell_id)

        attributes_ex_d = get_u32(info, row, ATTRIBUTES_EX_D_FIELD)
        if not attributes_ex_d & CAST_ONLY_IN_OUTLAND:
            continue
        auras = {
            get_u32(info, row, field)
            for field in range(EFFECT_AURA_FIRST_FIELD, EFFECT_AURA_LAST_FIELD)
        }
        if auras & CLIENT_UNLOCK_AURAS:
            selected.append(row)

    if not selected:
        raise PatchError(
            "no flight-mount rows matched; input is not the expected unpatched 3.3.5a Spell.dbc"
        )

    selected_ids = {get_u32(info, row, 0) for row in selected}
    proto_hits = selected_ids & PROTO_DRAKE_SPELL_IDS
    if not proto_hits:
        raise PatchError(
            "no canonical proto-drake mount spell matched; refusing to patch an unexpected DBC"
        )
    return selected


def make_patched(info: DbcInfo, selected_rows: list[int]) -> bytes:
    result = bytearray(info.data)
    for row in selected_rows:
        offset = field_offset(info, row, ATTRIBUTES_EX_D_FIELD)
        old_value = UINT32.unpack_from(result, offset)[0]
        UINT32.pack_into(result, offset, old_value & ~CAST_ONLY_IN_OUTLAND)
    return bytes(result)


def verify_diff(original: DbcInfo, patched_data: bytes, selected_rows: list[int]) -> None:
    if len(patched_data) != len(original.data):
        raise PatchError("patched DBC size changed")
    if patched_data[:WDBC_HEADER.size] != original.data[:WDBC_HEADER.size]:
        raise PatchError("patched DBC header changed")

    expected_offsets = {
        field_offset(original, row, ATTRIBUTES_EX_D_FIELD) + byte_index
        for row in selected_rows
        for byte_index in range(4)
    }
    actual_offsets = {
        index
        for index, (before, after) in enumerate(zip(original.data, patched_data))
        if before != after
    }
    if not actual_offsets:
        raise PatchError("patch produced no byte changes")
    if not actual_offsets <= expected_offsets:
        unexpected = sorted(actual_offsets - expected_offsets)[:10]
        raise PatchError(f"bytes outside AttributesExD changed: {unexpected}")

    for row in selected_rows:
        offset = field_offset(original, row, ATTRIBUTES_EX_D_FIELD)
        before = UINT32.unpack_from(original.data, offset)[0]
        after = UINT32.unpack_from(patched_data, offset)[0]
        if before & CAST_ONLY_IN_OUTLAND == 0:
            raise PatchError(f"selected row {row} lacked the restriction bit")
        if after != before & ~CAST_ONLY_IN_OUTLAND:
            raise PatchError(f"row {row} was not changed by exactly the expected mask")

    selected_row_set = set(selected_rows)
    for row in range(original.record_count):
        if row in selected_row_set:
            continue
        start = original.records_offset + row * original.record_size
        end = start + original.record_size
        if patched_data[start:end] != original.data[start:end]:
            raise PatchError(f"unselected row {row} changed")
    if patched_data[original.strings_offset:] != original.data[original.strings_offset:]:
        raise PatchError("string block changed")


def atomic_write(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary_name = tempfile.mkstemp(prefix=path.name + ".", suffix=".tmp", dir=path.parent)
    temporary = Path(temporary_name)
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    except Exception:
        temporary.unlink(missing_ok=True)
        raise


def report_text(input_path: Path, output_path: Path, info: DbcInfo,
                selected_rows: list[int], patched_data: bytes) -> str:
    selected = [
        (get_u32(info, row, 0), get_name(info, row))
        for row in selected_rows
    ]
    proto_hits = sorted(spell_id for spell_id, _ in selected if spell_id in PROTO_DRAKE_SPELL_IDS)
    lines = [
        "G17R1_CLIENT_SPELL_DBC_PATCH=PASS",
        f"INPUT={input_path}",
        f"OUTPUT={output_path}",
        f"INPUT_SIZE={len(info.data)}",
        f"INPUT_SHA256={sha256(info.data)}",
        f"OUTPUT_SIZE={len(patched_data)}",
        f"OUTPUT_SHA256={sha256(patched_data)}",
        f"RECORD_COUNT={info.record_count}",
        f"FIELD_COUNT={info.field_count}",
        f"RECORD_SIZE={info.record_size}",
        f"PATCHED_ROWS={len(selected_rows)}",
        "PROTO_DRAKE_HITS=" + ",".join(str(value) for value in proto_hits),
        "SERVER_DBC_MUST_REMAIN_ORIGINAL=True",
        "CLIENT_ONLY_CAST_GATE_REMOVED=True",
        "SERVER_G17A_LOCATION_POLICY_REMAINS_AUTHORITATIVE=True",
        "PATCHED_SPELLS_BEGIN",
    ]
    lines.extend(f"{spell_id}\t{name}" for spell_id, name in selected)
    lines.append("PATCHED_SPELLS_END")
    return "\n".join(lines) + "\n"


def patch(input_path: Path, output_path: Path, report_path: Optional[Path]) -> None:
    if input_path.resolve() == output_path.resolve():
        raise PatchError("input and output must be different; server DBC must not be modified")
    info = load_dbc(input_path)
    selected_rows = select_rows(info)
    patched_data = make_patched(info, selected_rows)
    verify_diff(info, patched_data, selected_rows)

    if output_path.exists():
        existing = output_path.read_bytes()
        if existing != patched_data:
            raise PatchError(f"refusing to overwrite different existing output: {output_path}")
    else:
        atomic_write(output_path, patched_data)

    report = report_text(input_path, output_path, info, selected_rows, patched_data)
    if report_path:
        atomic_write(report_path, report.encode("utf-8"))
    sys.stdout.write(report)


def verify(original_path: Path, patched_path: Path) -> None:
    info = load_dbc(original_path)
    selected_rows = select_rows(info)
    try:
        patched_data = patched_path.read_bytes()
    except OSError as exc:
        raise PatchError(f"cannot read {patched_path}: {exc}") from exc
    verify_diff(info, patched_data, selected_rows)
    expected = make_patched(info, selected_rows)
    if patched_data != expected:
        raise PatchError("patched file is valid-looking but is not the deterministic expected output")
    print("G17R1_CLIENT_SPELL_DBC_VERIFY=PASS")
    print(f"ORIGINAL_SHA256={sha256(info.data)}")
    print(f"PATCHED_SHA256={sha256(patched_data)}")
    print(f"PATCHED_ROWS={len(selected_rows)}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    patch_parser = subparsers.add_parser("patch", help="create the client-only patched DBC")
    patch_parser.add_argument("--input", required=True, type=Path)
    patch_parser.add_argument("--output", required=True, type=Path)
    patch_parser.add_argument("--report", type=Path)

    verify_parser = subparsers.add_parser("verify", help="verify an existing patched DBC")
    verify_parser.add_argument("--original", required=True, type=Path)
    verify_parser.add_argument("--patched", required=True, type=Path)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        if args.command == "patch":
            patch(args.input, args.output, args.report)
        else:
            verify(args.original, args.patched)
        return 0
    except PatchError as exc:
        print(f"G17R1_CLIENT_SPELL_DBC_ERROR={exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
