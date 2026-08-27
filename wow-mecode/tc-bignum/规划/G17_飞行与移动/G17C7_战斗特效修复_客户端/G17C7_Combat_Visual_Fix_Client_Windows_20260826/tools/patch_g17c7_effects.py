#!/usr/bin/env python3
"""G17-C7: fix the combat carriers' Effect_1 from DUMMY(3) to SCHOOL_DAMAGE(2).

ROOT CAUSE (confirmed by real DBC analysis):
  3.3.5 client ONLY renders spell visuals for real effect types.
  DUMMY (3) effects are completely inert client-side — no cast visual,
  no impact visual, no animation, even when SpellVisualID is set.

  Real spells with visuals (Fireball 133, Fire Breath 9573) all have
  Effect_1 = 2 (SCHOOL_DAMAGE). Our carriers have Effect_1 = 3 (DUMMY).

  This patcher changes Effect_1 (col 71) from 3 to 2 for records
  990000-990024, keeping BasePoints_1 (col 80) at 0 (server script
  handles actual damage via PreventHitDefaultEffect).

  Also ensures SpellVisualID (col 131) and RangeIndex (col 46) are set
  (idempotent with C6's visual patch and B3R4's range patch).

States:
  FRESH          = Effect_1=3, needs full fix
  EFFECT_FIXED   = Effect_1=2, visual+range already set
  PARTIAL        = unexpected state, refuse
"""
from __future__ import annotations

import argparse
import hashlib
import struct
import sys
from pathlib import Path

G17C7_EFFECT_PATCHER_VERSION = "v1_school_damage"
ID_LO, ID_HI = 990000, 990024
FIELDS = 234
RECSIZE = FIELDS * 4

EFFECT_COL = 71
BASEPTS_COL = 80
VISUAL_COL = 131
RANGE_COL = 46
TGT_A_COL = 92  # EffectImplicitTargetA_1

DUMMY = 3
SCHOOL_DAMAGE = 2
TARGET_RANGE = 4  # 30yd
TGT_UNIT_ENEMY = 18  # TARGET_UNIT_TARGET_ENEMY

BLOCK_VISUALS = [1483, 6587, 7749, 98, 219]


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
    found = effect_ok = visual_ok = range_ok = other = 0
    for i in range(count):
        off = i * RECSIZE
        sid = struct.unpack_from("<I", recs, off)[0]
        if not (ID_LO <= sid <= ID_HI):
            continue
        found += 1
        eff = struct.unpack_from("<I", recs, off + EFFECT_COL * 4)[0]
        vis = struct.unpack_from("<I", recs, off + VISUAL_COL * 4)[0]
        rng = struct.unpack_from("<I", recs, off + RANGE_COL * 4)[0]
        if eff == SCHOOL_DAMAGE:
            effect_ok += 1
        if vis == expected_visual(sid):
            visual_ok += 1
        if rng == TARGET_RANGE:
            range_ok += 1
        if eff not in (DUMMY, SCHOOL_DAMAGE):
            other += 1
    return found, effect_ok, visual_ok, range_ok, other


def state_of(found, effect_ok, visual_ok, range_ok, other):
    total = ID_HI - ID_LO + 1
    if other or found != total:
        return "PARTIAL"
    if effect_ok == total and visual_ok == total and range_ok == total:
        return "COMPLETE"
    return "FRESH"


def patch(recs: bytearray):
    n = 0
    for i in range(len(recs) // RECSIZE):
        off = i * RECSIZE
        sid = struct.unpack_from("<I", recs, off)[0]
        if not (ID_LO <= sid <= ID_HI):
            continue
        # Effect_1: DUMMY -> SCHOOL_DAMAGE (triggers client visual rendering)
        struct.pack_into("<I", recs, off + EFFECT_COL * 4, SCHOOL_DAMAGE)
        # BasePoints_1: 0 (server script handles damage via PreventHitDefaultEffect)
        struct.pack_into("<I", recs, off + BASEPTS_COL * 4, 0)
        # ImplicitTargetA_1: 18 (TARGET_UNIT_TARGET_ENEMY, same as Fire Breath)
        struct.pack_into("<I", recs, off + TGT_A_COL * 4, TGT_UNIT_ENEMY)
        # SpellVisualID: per archetype block (idempotent with C6)
        struct.pack_into("<I", recs, off + VISUAL_COL * 4, expected_visual(sid))
        # RangeIndex: 4 = 30yd (idempotent with B3R4)
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
        print("G17C7_EFFECT_PATCHER_VERSION=" + G17C7_EFFECT_PATCHER_VERSION)
        return 0
    if not args.command or not args.input:
        print("usage: patch_g17c7_effects.py check --input in.dbc | patch --input in.dbc --output out.dbc")
        return 2

    data = Path(args.input).read_bytes()
    count, recs, strings = parse(data)
    found, effect_ok, visual_ok, range_ok, other = scan(recs, count)
    state = state_of(found, effect_ok, visual_ok, range_ok, other)

    print(f"G17C7_RECORDS={count}")
    print(f"G17C7_CARRIERS_FOUND={found}")
    print(f"G17C7_EFFECT_SCHOOL_DAMAGE={effect_ok}")
    print(f"G17C7_EFFECT_DUMMY={found - effect_ok}")
    print(f"G17C7_VISUAL_OK={visual_ok}")
    print(f"G17C7_RANGE_OK={range_ok}")
    print(f"G17C7_UNEXPECTED={other}")
    print(f"G17C7_EFFECT_STATE={state}")

    if args.command == "check":
        print("G17C7_EFFECT_CHECK=PASS" if state != "PARTIAL" else "G17C7_EFFECT_CHECK=FAIL")
        return 0 if state != "PARTIAL" else 2

    if state == "COMPLETE":
        print("G17C7_EFFECT_PATCH=ALREADY_COMPLETE")
        print("G17C7_EFFECT_PATCH_RESULT=PASS")
        print("G17C7_EFFECT_WRITE=NONE")
        return 0
    if state == "PARTIAL":
        print("G17C7_EFFECT_PATCH=REFUSED_PARTIAL")
        print("G17C7_EFFECT_PATCH_RESULT=FAIL")
        return 2

    if not args.output:
        print("--output required")
        return 2

    out_recs = bytearray(recs)
    patched = patch(out_recs)
    out = data[:20] + bytes(out_recs) + strings
    Path(args.output).write_bytes(out)
    print("G17C7_EFFECT_PATCH=PATCHED")
    print("G17C7_EFFECT_PATCH_RESULT=PASS")
    print(f"G17C7_EFFECT_PATCHED_RECORDS={patched}")
    print(f"G17C7_EFFECT_INPUT_SHA256={sha(data)}")
    print(f"G17C7_EFFECT_OUTPUT_SHA256={sha(out)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
