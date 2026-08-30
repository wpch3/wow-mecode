#!/usr/bin/env python3
"""G17-C8: per-slot visual effects + cooldown display for the 25 combat carriers.

Changes vs C7 (which fixed DUMMY->SCHOOL_DAMAGE but had one visual per archetype):
  1. SpellVisualID: per-SLOT (not per-archetype) — each skill looks different
     regardless of mount type
  2. RecoveryTime (col 29): set to match server-side cooldowns — the client
     shows the cooldown sweep on action bar buttons
  3. CategoryRecoveryTime (col 30): same value for redundancy

Slot visuals (verified from real spells in the zhCN client Spell.dbc):
  slot 0 (generator): 143   火焰冲击 Fire Blast      — quick hit
  slot 1 (damage):    67    火球术 Fireball           — projectile
  slot 2 (heal):      3077  快速治疗 Flash Heal        — healing
  slot 3 (control):   322   制裁之锤 Hammer of Justice — stun
  slot 4 (finisher):  2253  炎爆术 Pyroblast           — big explosion

Slot cooldowns (matching COMBAT_CD_MS in cs_dragonriding.cpp):
  slot 0: 4000ms    slot 1: 6000ms    slot 2: 20000ms
  slot 3: 10000ms   slot 4: 60000ms

Also ensures (idempotent with C7):
  Effect_1 = 2 (SCHOOL_DAMAGE), ImplicitTargetA_1 = 18, RangeIndex = 4
"""
from __future__ import annotations

import argparse
import hashlib
import struct
import sys
from pathlib import Path

G17C8_PATCHER_VERSION = "v1_perslot_cd"
ID_LO, ID_HI = 990000, 990024
FIELDS = 234
RECSIZE = FIELDS * 4

# Columns
EFFECT_COL = 71
BASEPTS_COL = 80
TGT_A_COL = 92
VISUAL_COL = 131
RANGE_COL = 46
RECOVERY_COL = 29       # RecoveryTime (ms) — drives client cooldown display
CAT_RECOVERY_COL = 30   # CategoryRecoveryTime (ms)

# Values
SCHOOL_DAMAGE = 2
TARGET_RANGE = 4
TGT_UNIT_ENEMY = 18

# Per-SLOT visuals (same across all archetypes)
SLOT_VISUALS = [143, 67, 3077, 322, 2253]

# Per-SLOT cooldowns in ms (matching COMBAT_CD_MS in the C++ code)
SLOT_COOLDOWNS = [4000, 6000, 20000, 10000, 60000]


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


def slot_of(sid: int) -> int:
    return (sid - ID_LO) % 5


def scan(recs: bytes, count: int):
    found = visual_ok = cd_ok = effect_ok = range_ok = other = 0
    for i in range(count):
        off = i * RECSIZE
        sid = struct.unpack_from("<I", recs, off)[0]
        if not (ID_LO <= sid <= ID_HI):
            continue
        found += 1
        slot = slot_of(sid)
        eff = struct.unpack_from("<I", recs, off + EFFECT_COL * 4)[0]
        vis = struct.unpack_from("<I", recs, off + VISUAL_COL * 4)[0]
        rec = struct.unpack_from("<I", recs, off + RECOVERY_COL * 4)[0]
        rng = struct.unpack_from("<I", recs, off + RANGE_COL * 4)[0]
        if eff == SCHOOL_DAMAGE: effect_ok += 1
        if vis == SLOT_VISUALS[slot]: visual_ok += 1
        if rec == SLOT_COOLDOWNS[slot]: cd_ok += 1
        if rng == TARGET_RANGE: range_ok += 1
        if eff not in (2, 3): other += 1
    return found, effect_ok, visual_ok, cd_ok, range_ok, other


def state_of(found, effect_ok, visual_ok, cd_ok, range_ok, other):
    total = ID_HI - ID_LO + 1
    if other or found != total:
        return "PARTIAL"
    if all(v == total for v in (effect_ok, visual_ok, cd_ok, range_ok)):
        return "COMPLETE"
    return "FRESH"


def patch(recs: bytearray):
    n = 0
    for i in range(len(recs) // RECSIZE):
        off = i * RECSIZE
        sid = struct.unpack_from("<I", recs, off)[0]
        if not (ID_LO <= sid <= ID_HI):
            continue
        slot = slot_of(sid)
        # Effect: SCHOOL_DAMAGE (triggers client visual rendering)
        struct.pack_into("<I", recs, off + EFFECT_COL * 4, SCHOOL_DAMAGE)
        struct.pack_into("<I", recs, off + BASEPTS_COL * 4, 0)
        # Target: current enemy (same as Fire Breath)
        struct.pack_into("<I", recs, off + TGT_A_COL * 4, TGT_UNIT_ENEMY)
        # Visual: per-slot (each skill looks different)
        struct.pack_into("<I", recs, off + VISUAL_COL * 4, SLOT_VISUALS[slot])
        # Range: 30yd
        struct.pack_into("<I", recs, off + RANGE_COL * 4, TARGET_RANGE)
        # Cooldown: per-slot (client shows cooldown sweep)
        struct.pack_into("<I", recs, off + RECOVERY_COL * 4, SLOT_COOLDOWNS[slot])
        struct.pack_into("<I", recs, off + CAT_RECOVERY_COL * 4, SLOT_COOLDOWNS[slot])
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
        print("G17C8_PATCHER_VERSION=" + G17C8_PATCHER_VERSION)
        return 0
    if not args.command or not args.input:
        print("usage: patch_g17c8_full.py check --input in.dbc | patch --input in.dbc --output out.dbc")
        return 2

    data = Path(args.input).read_bytes()
    count, recs, strings = parse(data)
    found, effect_ok, visual_ok, cd_ok, range_ok, other = scan(recs, count)
    state = state_of(found, effect_ok, visual_ok, cd_ok, range_ok, other)

    print(f"G17C8_RECORDS={count}")
    print(f"G17C8_CARRIERS_FOUND={found}")
    print(f"G17C8_EFFECT_OK={effect_ok}")
    print(f"G17C8_VISUAL_PERSLOT_OK={visual_ok}")
    print(f"G17C8_COOLDOWN_OK={cd_ok}")
    print(f"G17C8_RANGE_OK={range_ok}")
    print(f"G17C8_UNEXPECTED={other}")
    print(f"G17C8_STATE={state}")

    if args.command == "check":
        print("G17C8_CHECK=PASS" if state != "PARTIAL" else "G17C8_CHECK=FAIL")
        return 0 if state != "PARTIAL" else 2

    if state == "COMPLETE":
        print("G17C8_PATCH=ALREADY_COMPLETE")
        print("G17C8_PATCH_RESULT=PASS")
        print("G17C8_WRITE=NONE")
        return 0
    if state == "PARTIAL":
        print("G17C8_PATCH=REFUSED_PARTIAL")
        print("G17C8_PATCH_RESULT=FAIL")
        return 2

    if not args.output:
        print("--output required")
        return 2

    out_recs = bytearray(recs)
    patched = patch(out_recs)
    out = data[:20] + bytes(out_recs) + strings
    Path(args.output).write_bytes(out)
    print("G17C8_PATCH=PATCHED")
    print("G17C8_PATCH_RESULT=PASS")
    print(f"G17C8_PATCHED_RECORDS={patched}")
    print(f"G17C8_INPUT_SHA256={sha(data)}")
    print(f"G17C8_OUTPUT_SHA256={sha(out)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
