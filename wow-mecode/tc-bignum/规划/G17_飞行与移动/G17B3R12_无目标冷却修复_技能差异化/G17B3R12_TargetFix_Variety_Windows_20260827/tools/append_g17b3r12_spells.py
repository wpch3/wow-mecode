#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""G17-B3R12: append 2 skill-variety DUMMY carriers to the SERVER Spell.dbc.

Deterministic, idempotent-guarded, preserves every existing record and the
whole string block byte-for-byte.  Records (both are DUMMY carriers; all real
behavior lives in the server SpellScripts):
  990029 突袭·俯冲打击  icon 50  RangeIndex 4 (30yd)  [War Stomp 45 prototype]
  990030 御风姿态        icon 1181 RangeIndex 1 (self) [Aspect of the Cheetah 5118]
Each: Attributes=0x100 (CASTABLE_WHILE_MOUNTED), Effect1..3=DUMMY(3),
CastingTimeIndex=1.

Guards:
  - input is WDBC, 234 fields, recsize=936
  - both ids ABSENT -> append; both PRESENT (and matching) -> ALREADY_APPENDED;
    some present -> GUARD_FAIL (no write)
"""
from __future__ import annotations

import argparse
import hashlib
import struct
import sys
from pathlib import Path

G17B3R12_APPENDER_VERSION = "v1_append2_variety"
FIELDS = 234
RECSIZE = FIELDS * 4
EFFECT_DUMMY = 3
ATTR_MOUNTED = 0x100
NAME_COL = 140
ICON_COL = 133
DESC_COL = 174
RANGE_COL = 46

SKILLS = [
    # (id, name, icon, range_index, desc)
    (990029, "突袭·俯冲打击", 50, 4, "俯冲突袭目标区域，对8码内的敌人造成物理伤害。"),
    (990030, "御风姿态", 1181, 1, "进入御风姿态：转向速度提高15%，龙能量回复加倍。再次施放解除。"),
]


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def parse(data: bytes):
    if data[:4] != b"WDBC":
        raise RuntimeError("not a WDBC file")
    magic, count, fields, recsize, strsz = struct.unpack_from("<5I", data, 0)
    if fields != FIELDS or recsize != RECSIZE:
        raise RuntimeError(f"unexpected layout fields={fields} recsize={recsize}")
    if len(data) != 20 + count * recsize + strsz:
        raise RuntimeError("file size mismatch")
    return count, data[20:20 + count * recsize], data[20 + count * recsize:]


def build_record(sid: int, name: str, icon: int, range_index: int, desc: str,
                 name_off: int, desc_off: int) -> bytes:
    vals = [0] * FIELDS
    vals[0] = sid
    vals[4] = ATTR_MOUNTED
    vals[28] = 1          # CastingTimeIndex = instant
    vals[RANGE_COL] = range_index
    vals[71] = EFFECT_DUMMY
    vals[80] = 0
    vals[ICON_COL] = icon
    vals[NAME_COL] = name_off
    vals[DESC_COL] = desc_off
    return struct.pack("<" + "I" * FIELDS, *vals)


def state_of(recs: bytes, count: int) -> str:
    ids = set()
    for i in range(count):
        sid = struct.unpack_from("<I", recs, i * RECSIZE)[0]
        if sid in (990029, 990030):
            ids.add(sid)
    if len(ids) == 0:
        return "MISSING"
    if len(ids) == 2:
        return "ALREADY_APPENDED"
    return "PARTIAL"


def do_append(args) -> int:
    data = Path(args.input).read_bytes()
    count, recs, strings = parse(data)
    state = state_of(recs, count)
    print(f"G17B3R12_SPELL_DBC_STATE={state}")
    print(f"G17B3R12_SPELL_DBC_INPUT_SHA256={sha(data)}")
    print(f"G17B3R12_SPELL_DBC_INPUT_RECORDS={count}")
    if state == "ALREADY_APPENDED":
        print("G17B3R12_SPELL_DBC_APPEND=ALREADY_APPENDED")
        print("G17B3R12_SPELL_DBC_WRITE=NONE")
        return 0
    if state == "PARTIAL":
        print("G17B3R12_SPELL_DBC_APPEND=FAIL")
        return 2

    new_names = b""
    name_offsets = {}
    for sid, name, icon, rng, desc in SKILLS:
        name_offsets[sid] = len(strings) + len(new_names)
        new_names += name.encode("utf-8") + b"\x00"
    desc_offsets = {}
    for sid, name, icon, rng, desc in SKILLS:
        desc_offsets[sid] = len(strings) + len(new_names)
        new_names += desc.encode("utf-8") + b"\x00"

    new_recs = b""
    for sid, name, icon, rng, desc in SKILLS:
        new_recs += build_record(sid, name, icon, rng, desc, name_offsets[sid], desc_offsets[sid])

    # bytearray: struct.pack_into needs a mutable buffer (real TypeError fix)
    out = bytearray(data[:20] + recs + new_recs + strings + new_names)
    new_count = count + len(SKILLS)
    new_strsz = len(strings) + len(new_names)
    struct.pack_into("<I", out, 4, new_count)
    struct.pack_into("<I", out, 16, new_strsz)

    Path(args.output).write_bytes(bytes(out))
    print(f"G17B3R12_SPELL_DBC_OUTPUT_SHA256={sha(out)}")
    print(f"G17B3R12_SPELL_DBC_OUTPUT_RECORDS={new_count}")
    print("G17B3R12_SPELL_DBC_APPEND=PASS")
    return 0


def do_check(args) -> int:
    data = Path(args.input).read_bytes()
    count, recs, strings = parse(data)
    state = state_of(recs, count)
    print(f"G17B3R12_SPELL_DBC_STATE={state}")
    print(f"G17B3R12_SPELL_DBC_INPUT_SHA256={sha(data)}")
    print(f"G17B3R12_SPELL_DBC_INPUT_RECORDS={count}")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--version", action="store_true")
    ap.add_argument("command", nargs="?", choices=("check", "append"))
    ap.add_argument("--input", default="")
    ap.add_argument("--output", default="")
    args = ap.parse_args()
    if args.version:
        print("G17B3R12_APPENDER_VERSION=" + G17B3R12_APPENDER_VERSION)
        return 0
    if args.command == "check":
        return do_check(args)
    if args.command == "append":
        if not args.output:
            print("--output required for append")
            return 2
        return do_append(args)
    return 2


if __name__ == "__main__":
    sys.exit(main())
