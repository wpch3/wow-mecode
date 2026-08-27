#!/usr/bin/env python3
"""G17-B3R2: append 4 skill-bar DUMMY spell records to a 3.3.5a Spell.dbc.

Adds the multi-page vehicle skill bar carriers:
  990025 切换技能页 (page switch button, on both pages)
  990026 拉升        (powered steep climb)
  990027 俯冲        (powered dive, restores energy)
  990028 滑翔制动    (air brake)

Deterministic, idempotent-guarded, and preserves every existing record and
the whole string block byte-for-byte (same proven template as the B3-R1
appender).  Each new record:
  - Attributes = 0x100 (CASTABLE_WHILE_MOUNTED)
  - Effect1..3 = SPELL_EFFECT_DUMMY (3), no native effect
  - CastingTimeIndex=1, RangeIndex=1 (self/default), no focus/aura gates
  - Name (col 140) = zhCN name, icon (col 133), description (col 174)
All real behavior is server-side SpellScripts in cs_dragonriding.cpp.

Icon IDs were read from the project's real 3.3.5a zhCN client Spell.dbc:
  279  = 战斗姿态 (Battle Stance)   -> page switch
  2755 = 喷射跳跃 (Jump Jets 52197) -> ascend
  539  = 逃脱/俯冲 (Dive 781)       -> dive
  505  = 缓落术 (Slow Fall 130)     -> glide brake

Guards:
  - input is WDBC, 234 fields, recsize=936
  - all IDs 990025..990028 are ABSENT in input (idempotency: if ALL present
    -> ALREADY_APPENDED, no write; if SOME present -> GUARD_FAIL, no write)
  - output record count = input + 4, string block = input + appended names
"""
from __future__ import annotations

import argparse
import hashlib
import struct
import sys
from pathlib import Path

G17B3R2_DBC_APPENDER_VERSION = "v1_append4"
ID_BASE = 990025
COUNT = 4
FIELDS = 234
RECSIZE = FIELDS * 4
EFFECT_DUMMY = 3
ATTR_MOUNTED = 0x100
NAME_COL = 140
ICON_COL = 133
DESC_COL = 174

SKILLS = [
    # (name, icon, desc)
    ("切换技能页", 279, "在移动技能页与战斗技能页之间切换。"),
    ("拉升", 2755, "消耗龙能量，沿当前朝向急速爬升。"),
    ("俯冲", 539, "收翼急速俯冲，恢复龙能量并提升动量。"),
    ("滑翔制动", 505, "展开双翼制动，迅速降低飞行速度。"),
]


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def parse(data: bytes) -> tuple[int, int, int, bytes, bytes]:
    magic, count, fields, recsize, strsz = struct.unpack_from("<5I", data, 0)
    if data[:4] != b"WDBC":
        raise RuntimeError("not a WDBC file")
    if fields != FIELDS or recsize != RECSIZE:
        raise RuntimeError(f"unexpected layout fields={fields} recsize={recsize}")
    if len(data) != 20 + count * recsize + strsz:
        raise RuntimeError("file size mismatch")
    base = 20
    recs = data[base:base + count * recsize]
    strings = data[base + count * recsize:base + count * recsize + strsz]
    return count, fields, recsize, recs, strings


def existing_ids(recs: bytes, count: int, recsize: int) -> set[int]:
    ids = set()
    for i in range(count):
        ids.add(struct.unpack_from("<I", recs, i * recsize)[0])
    return ids


def build_record(sid: int, name: str, icon: int, desc: str,
                 name_off: int, desc_off: int) -> bytes:
    vals = [0] * FIELDS
    vals[0] = sid
    vals[4] = ATTR_MOUNTED
    vals[28] = 1          # CastingTimeIndex = instant
    vals[46] = 1          # RangeIndex = default
    vals[71] = EFFECT_DUMMY
    vals[80] = 0
    vals[133] = icon
    vals[140] = name_off
    vals[174] = desc_off
    return struct.pack("<" + "I" * FIELDS, *vals)


def do_append(args) -> int:
    data = Path(args.input).read_bytes()
    count, fields, recsize, recs, strings = parse(data)
    ids = existing_ids(recs, count, recsize)
    claimed = [ID_BASE + i for i in range(COUNT)]
    present = [sid for sid in claimed if sid in ids]
    if len(present) == COUNT:
        print("G17B3R2_SPELL_DBC_STATE=ALREADY_APPENDED")
        print("G17B3R2_SPELL_DBC_APPEND=PASS")
        print("G17B3R2_SPELL_DBC_WRITE=NONE")
        return 0
    if present:
        print(f"G17B3R2_SPELL_DBC_STATE=GUARD_FAIL present={present}")
        print("G17B3R2_SPELL_DBC_APPEND=FAIL")
        return 2

    # Keep the original string block as an untouched prefix; new records point
    # at offsets inside the appended tail (DBC offsets are absolute).
    new_strings = bytearray(strings)
    name_offs = []
    desc_offs = []
    for _n, _i, _d in SKILLS:
        name_offs.append(len(new_strings))
        new_strings += _n.encode("utf-8") + b"\x00"
        desc_offs.append(len(new_strings))
        new_strings += _d.encode("utf-8") + b"\x00"

    new_recs = bytearray(recs)
    for i in range(COUNT):
        sid = ID_BASE + i
        name, icon, desc = SKILLS[i]
        new_recs += build_record(sid, name, icon, desc,
                                 name_offs[i], desc_offs[i])

    new_count = count + COUNT
    new_strsz = len(new_strings)
    header = struct.pack("<5I", 0x43424457, new_count, FIELDS, RECSIZE,
                         new_strsz)
    out = header + bytes(new_recs) + bytes(new_strings)
    out_path = Path(args.output)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(out)

    print("G17B3R2_SPELL_DBC_STATE=APPENDED")
    print("G17B3R2_SPELL_DBC_APPEND=PASS")
    print(f"G17B3R2_INPUT_SHA256={sha(data)}")
    print(f"G17B3R2_OUTPUT_SHA256={sha(out)}")
    print(f"G17B3R2_RECORDS_BEFORE={count}")
    print(f"G17B3R2_RECORDS_AFTER={new_count}")
    print(f"G17B3R2_STRING_BLOCK_PREFIX_KEPT=" + str(strings == bytes(new_strings[:len(strings)])))
    return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--version", action="store_true")
    ap.add_argument("command", nargs="?", choices=("append", "check"))
    ap.add_argument("--input", default="")
    ap.add_argument("--output", default="")
    args = ap.parse_args()
    if args.version:
        print("G17B3R2_DBC_APPENDER_VERSION=" + G17B3R2_DBC_APPENDER_VERSION)
        return 0
    if not args.command or not args.input:
        print("usage: append_g17b3r2_spells.py append --input in.dbc [--output out.dbc]")
        return 2
    if args.command == "check":
        data = Path(args.input).read_bytes()
        count, fields, recsize, recs, strings = parse(data)
        ids = existing_ids(recs, count, recsize)
        present = [ID_BASE + i for i in range(COUNT) if ID_BASE + i in ids]
        print(f"G17B3R2_RECORDS={count}")
        print(f"G17B3R2_PRESENT={len(present)}")
        print("G17B3R2_SPELL_DBC_STATE=" + ("ALREADY_APPENDED" if len(present) == COUNT else
                                            ("PARTIAL" if present else "MISSING")))
        return 0
    if not args.output:
        print("--output required")
        return 2
    return do_append(args)


if __name__ == "__main__":
    sys.exit(main())
