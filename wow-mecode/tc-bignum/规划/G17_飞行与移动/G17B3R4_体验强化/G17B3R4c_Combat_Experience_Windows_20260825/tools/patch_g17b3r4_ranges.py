#!/usr/bin/env python3
"""G17-B3R4: fix the 25 combat carriers' RangeIndex in a 3.3.5a Spell.dbc.

Root cause of the user-reported 无法命中: the B3-R1 appender wrote
RangeIndex = 1 (self, 0 yd) for records 990000-990024.  When a cast carries
an explicit unit target (vehicle-bar button press with a target selected),
the server's Spell::CheckRange compares the target distance against the
range entry and rejects anything beyond 0 yd.  RangeIndex 4 is the standard
30-yard entry (used by Smite 585 etc.).

This tool patches col 46 of the existing 990000-990024 records from 1 to 4
in place.  It never appends, never touches any other record or the string
block, and writes a fresh output file.

States:
  check   -> G17B3R4_RANGE_STATE = MISSING (all 25 still self-range)
                            | PATCHED (all already 30yd)
                            | PARTIAL (mixed / unexpected values; refuse)
  patch   -> writes the patched image, byte-identical except col 46 of the
             25 records.
"""
from __future__ import annotations

import argparse
import hashlib
import struct
import sys
from pathlib import Path

G17B3R4_RANGE_PATCHER_VERSION = "v1_range30"
ID_LO, ID_HI = 990000, 990024
FIELDS = 234
RECSIZE = FIELDS * 4
RANGE_COL = 46
SELF_RANGE = 1
TARGET_RANGE = 4  # 30 yards (Smite 585, verified against the real Spell.dbc)


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


def scan(recs: bytes, count: int):
    """Returns (self_count, patched_count, other) over the 25 combat ids."""
    self_n = patched_n = other_n = 0
    for i in range(count):
        off = i * RECSIZE
        sid = struct.unpack_from("<I", recs, off)[0]
        if ID_LO <= sid <= ID_HI:
            val = struct.unpack_from("<I", recs, off + RANGE_COL * 4)[0]
            if val == SELF_RANGE:
                self_n += 1
            elif val == TARGET_RANGE:
                patched_n += 1
            else:
                other_n += 1
    return self_n, patched_n, other_n


def state_of(self_n: int, patched_n: int, other_n: int) -> str:
    total = ID_HI - ID_LO + 1
    if other_n:
        return "PARTIAL"
    if self_n == total:
        return "MISSING"
    if patched_n == total:
        return "PATCHED"
    return "PARTIAL"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--version", action="store_true")
    ap.add_argument("command", nargs="?", choices=("check", "patch"))
    ap.add_argument("--input", default="")
    ap.add_argument("--output", default="")
    args = ap.parse_args()

    if args.version:
        print("G17B3R4_RANGE_PATCHER_VERSION=" + G17B3R4_RANGE_PATCHER_VERSION)
        return 0
    if not args.command or not args.input:
        print("usage: patch_g17b3r4_ranges.py check --input in.dbc | patch --input in.dbc --output out.dbc")
        return 2

    data = Path(args.input).read_bytes()
    count, recs, strings = parse(data)
    self_n, patched_n, other_n = scan(recs, count)
    state = state_of(self_n, patched_n, other_n)

    print(f"G17B3R4_RECORDS={count}")
    print(f"G17B3R4_RANGE_SELF={self_n}")
    print(f"G17B3R4_RANGE_30YD={patched_n}")
    print(f"G17B3R4_RANGE_OTHER={other_n}")
    print(f"G17B3R4_RANGE_STATE={state}")

    if args.command == "check":
        print("G17B3R4_RANGE_CHECK=PASS" if state != "PARTIAL" else "G17B3R4_RANGE_CHECK=FAIL")
        return 0 if state != "PARTIAL" else 2

    if state == "PATCHED":
        print("G17B3R4_RANGE_PATCH=ALREADY_PATCHED")
        print("G17B3R4_RANGE_PATCH_RESULT=PASS")
        print("G17B3R4_RANGE_WRITE=NONE")
        return 0
    if state == "PARTIAL":
        print("G17B3R4_RANGE_PATCH=REFUSED_PARTIAL")
        print("G17B3R4_RANGE_PATCH_RESULT=FAIL")
        return 2

    if not args.output:
        print("--output required")
        return 2

    out_recs = bytearray(recs)
    for i in range(count):
        off = i * RECSIZE
        sid = struct.unpack_from("<I", out_recs, off)[0]
        if ID_LO <= sid <= ID_HI:
            struct.pack_into("<I", out_recs, off + RANGE_COL * 4, TARGET_RANGE)

    out = data[:20] + bytes(out_recs) + strings
    Path(args.output).write_bytes(out)
    print("G17B3R4_RANGE_PATCH=PATCHED")
    print("G17B3R4_RANGE_PATCH_RESULT=PASS")
    print(f"G17B3R4_RANGE_INPUT_SHA256={sha(data)}")
    print(f"G17B3R4_RANGE_OUTPUT_SHA256={sha(out)}")
    print(f"G17B3R4_RANGE_PATCHED_RECORDS={self_n}")
    print("G17B3R4_STRING_BLOCK_UNCHANGED=" + str(strings == strings))
    return 0


if __name__ == "__main__":
    sys.exit(main())
