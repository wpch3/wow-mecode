#!/usr/bin/env python3
"""G17-B3R5: give the 25 combat carriers REAL spell visuals + 30yd ranges.

The B3-R1 appender wrote SpellVisualID = 0 (no cast visual at all) and
RangeIndex = 1 (self) for records 990000-990024.  Result: no visible
spellcasting feedback and explicit-target casts failing with 无法命中.

This patcher sets, per archetype block (5 records each):
  - col 131 (SpellVisualID): a real visual from the project's zhCN client
      dragon 990000-990004 -> 1483  (火息术 9573 Flame Breath)
      beast  990005-990009 -> 6587  (凶猛撕咬 22568 Ferocious Bite)
      magic  990010-990014 -> 7749  (奥术冲击 30451 Arcane Blast)
      mech   990015-990019 -> 98    (投掷炸弹 3823 Throw Bomb)
      generic 990020-990024 -> 219  (顺劈斩 Cleave)
  - col 46 (RangeIndex): 4 (30 yards, same as 惩击/Smite) - idempotent with
    the B3R4 range patch (already applied on the user's server DBC).

The tool works on ANY WDBC Spell.dbc (server DBC for parity, client DBC for
the actual rendering).  In-place record patch, no appends, string block and
every other record byte-identical.

States (over the 25 records):
  visual 0 + range 1  -> FRESH       (both patches needed)
  visual set + range4 -> COMPLETE    (nothing to do)
  anything else       -> PARTIAL     (refuse)
"""
from __future__ import annotations

import argparse
import hashlib
import struct
import sys
from pathlib import Path

G17B3R5_VISUAL_PATCHER_VERSION = "v1_visuals_range"
ID_LO, ID_HI = 990000, 990024
FIELDS = 234
RECSIZE = FIELDS * 4
RANGE_COL = 46
VISUAL_COL = 131
SELF_RANGE = 1
TARGET_RANGE = 4

BLOCK_VISUALS = [1483, 6587, 7749, 98, 219]  # dragon/beast/magic/mech/generic


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


def expected_visual(sid: int) -> int:
    return BLOCK_VISUALS[(sid - ID_LO) // 5]


def scan(recs: bytes, count: int):
    found = visual_ok = range_ok = other = 0
    for i in range(count):
        off = i * RECSIZE
        sid = struct.unpack_from("<I", recs, off)[0]
        if not (ID_LO <= sid <= ID_HI):
            continue
        found += 1
        vis = struct.unpack_from("<I", recs, off + VISUAL_COL * 4)[0]
        rng = struct.unpack_from("<I", recs, off + RANGE_COL * 4)[0]
        if vis == expected_visual(sid):
            visual_ok += 1
        if rng == TARGET_RANGE:
            range_ok += 1
        if vis not in (0, expected_visual(sid)) or rng not in (SELF_RANGE, TARGET_RANGE):
            other += 1
    return found, visual_ok, range_ok, other


def state_of(found, visual_ok, range_ok, other):
    total = ID_HI - ID_LO + 1
    if other or found != total:
        return "PARTIAL"
    if visual_ok == total and range_ok == total:
        return "COMPLETE"
    if visual_ok == 0 and range_ok == total:
        return "VISUAL_MISSING"      # B3R4 range patch already applied
    if visual_ok == 0 and range_ok == 0:
        return "FRESH"
    return "PARTIAL"


def patch(recs: bytearray):
    n = 0
    for i in range(len(recs) // RECSIZE):
        off = i * RECSIZE
        sid = struct.unpack_from("<I", recs, off)[0]
        if not (ID_LO <= sid <= ID_HI):
            continue
        struct.pack_into("<I", recs, off + VISUAL_COL * 4, expected_visual(sid))
        struct.pack_into("<I", recs, off + RANGE_COL * 4, TARGET_RANGE)
        n += 1
    return n


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--version", action="store_true")
    ap.add_argument("command", nargs="?", choices=("check", "patch"))
    ap.add_argument("--input", default="")
    ap.add_argument("--output", default="")
    args = ap.parse_args()

    if args.version:
        print("G17B3R5_VISUAL_PATCHER_VERSION=" + G17B3R5_VISUAL_PATCHER_VERSION)
        return 0
    if not args.command or not args.input:
        print("usage: patch_g17b3r5_visuals.py check --input in.dbc | patch --input in.dbc --output out.dbc")
        return 2

    data = Path(args.input).read_bytes()
    count, recs, strings = parse(data)
    found, visual_ok, range_ok, other = scan(recs, count)
    state = state_of(found, visual_ok, range_ok, other)

    print(f"G17B3R5_RECORDS={count}")
    print(f"G17B3R5_CARRIERS_FOUND={found}")
    print(f"G17B3R5_VISUAL_OK={visual_ok}")
    print(f"G17B3R5_RANGE_OK={range_ok}")
    print(f"G17B3R5_UNEXPECTED={other}")
    print(f"G17B3R5_VISUAL_STATE={state}")

    if args.command == "check":
        print("G17B3R5_VISUAL_CHECK=PASS" if state != "PARTIAL" else "G17B3R5_VISUAL_CHECK=FAIL")
        return 0 if state != "PARTIAL" else 2

    if state == "COMPLETE":
        print("G17B3R5_VISUAL_PATCH=ALREADY_COMPLETE")
        print("G17B3R5_VISUAL_PATCH_RESULT=PASS")
        print("G17B3R5_VISUAL_WRITE=NONE")
        return 0
    if state == "PARTIAL":
        print("G17B3R5_VISUAL_PATCH=REFUSED_PARTIAL")
        print("G17B3R5_VISUAL_PATCH_RESULT=FAIL")
        return 2

    if not args.output:
        print("--output required")
        return 2

    out_recs = bytearray(recs)
    patched = patch(out_recs)
    out = data[:20] + bytes(out_recs) + strings
    Path(args.output).write_bytes(out)
    print("G17B3R5_VISUAL_PATCH=PATCHED")
    print("G17B3R5_VISUAL_PATCH_RESULT=PASS")
    print(f"G17B3R5_VISUAL_PATCHED_RECORDS={patched}")
    print(f"G17B3R5_VISUAL_INPUT_SHA256={sha(data)}")
    print(f"G17B3R5_VISUAL_OUTPUT_SHA256={sha(out)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
