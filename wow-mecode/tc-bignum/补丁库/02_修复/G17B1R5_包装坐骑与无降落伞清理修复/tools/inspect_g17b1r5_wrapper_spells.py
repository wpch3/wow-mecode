#!/usr/bin/env python3
"""Read-only proof extractor for the two reported 3.3.5a wrapper-mount chains."""
from __future__ import annotations

import argparse
import hashlib
import struct
from pathlib import Path

EXPECTED_SHA256 = "dd25091167f671764735ce88c78b66485c6d661fadf05d322574c261f6e464ea"
SPELL_IDS = (48025, 51621, 48024, 51617, 48023, 71342, 71343, 71344, 71345, 71346, 71347)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--spell-dbc", required=True, type=Path)
    args = parser.parse_args()
    data = args.spell_dbc.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    if digest != EXPECTED_SHA256:
        raise RuntimeError(f"unexpected Spell.dbc SHA-256: {digest}")

    magic, record_count, field_count, record_size, string_size = struct.unpack_from("<4s4I", data)
    if magic != b"WDBC" or field_count != 234 or record_size != 936:
        raise RuntimeError("unsupported Spell.dbc layout")
    records_offset = 20
    strings_offset = records_offset + record_count * record_size
    if strings_offset + string_size != len(data):
        raise RuntimeError("Spell.dbc size/header mismatch")

    rows = {
        struct.unpack_from("<I", data, records_offset + index * record_size)[0]: index
        for index in range(record_count)
    }

    def uint(index: int, field: int) -> int:
        return struct.unpack_from("<I", data, records_offset + index * record_size + field * 4)[0]

    def string(offset: int) -> str:
        if not offset:
            return ""
        start = strings_offset + offset
        end = data.find(b"\0", start, strings_offset + string_size)
        return data[start:end].decode("utf-8", "replace")

    print(f"SPELL_DBC_SHA256={digest}")
    print(f"WDBC_RECORDS={record_count} FIELDS={field_count} RECORD_SIZE={record_size} STRINGS={string_size}")
    print("ID\tNAME\tEFFECT\tAURA\tMISC_VALUE\tTRIGGER_SPELL")
    for spell_id in SPELL_IDS:
        index = rows.get(spell_id)
        if index is None:
            raise RuntimeError(f"spell ID missing: {spell_id}")
        name = next((string(uint(index, field)) for field in range(136, 152) if string(uint(index, field))), "")
        effects = [uint(index, field) for field in range(71, 74)]
        auras = [uint(index, field) for field in range(95, 98)]
        misc = [uint(index, field) for field in range(110, 113)]
        triggers = [uint(index, field) for field in range(116, 119)]
        print(f"{spell_id}\t{name}\t{effects}\t{auras}\t{misc}\t{triggers}")


if __name__ == "__main__":
    main()
