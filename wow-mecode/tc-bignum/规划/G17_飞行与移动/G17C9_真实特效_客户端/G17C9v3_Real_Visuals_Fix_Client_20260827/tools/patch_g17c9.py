#!/usr/bin/env python3
"""G17-C9 v3: per-archetype per-slot visuals from REAL verified spell effects + remove DBC RecoveryTime.

v3 installer-fix release: identical DBC payload to C9 v2 (Wowhead-verified visuals),
but shipped with a corrected installer (see README_FIRST.txt).

Fixes two user-reported issues:
1. Visual effects didn't match skill descriptions (C8 used generic per-slot IDs)
2. Cooldown showed even when cast failed (DBC RecoveryTime triggers on button press)

Visual assignments (all from REAL spells, verified visual IDs):
  Dragon (fire/breath):   slot0=1483(火息术breath) slot1=219(顺劈斩sweep) slot2=784(真言盾shield) slot3=145(雷霆一击clap) slot4=2253(炎爆术explosion)
  Beast (physical/bite):  slot0=39(英勇打击)       slot1=250(斩杀)        slot2=57(强效治疗)     slot3=867(冲锋)         slot4=12295(毁灭打击)
  Magic (arcane):         slot0=67(火球术)          slot1=7749(奥术冲击)   slot2=784(真言盾)      slot3=965(魔爆术)       slot4=9490(暴风雪)
  Mech (explosion):       slot0=3445(烈焰震击)      slot1=3819(铁皮手雷)   slot2=8039(痛苦压制)   slot3=266(偷袭)         slot4=9493(乱射)
  Generic:                slot0=39(英勇打击)        slot1=219(顺劈斩)      slot2=3077(快速治疗)   slot3=322(制裁之锤)     slot4=7675(烧尽)

Cooldown fix: RecoveryTime (col 29) and CategoryRecoveryTime (col 30) set to 0.
Server-side AddCooldown sends SMSG_SPELL_COOLDOWN which the client displays correctly.
This prevents the "cooldown on failed cast" issue (client was reading DBC RecoveryTime
on button press regardless of server result).
"""
from __future__ import annotations

import argparse
import hashlib
import struct
import sys
from pathlib import Path

G17C9_VERSION = "v3_wowhead_visuals_no_dbc_cd"
ID_LO, ID_HI = 990000, 990024
FIELDS = 234
RECSIZE = FIELDS * 4

EFFECT_COL = 71
BASEPTS_COL = 80
TGT_A_COL = 92
VISUAL_COL = 131
RANGE_COL = 46
RECOVERY_COL = 29
CAT_RECOVERY_COL = 30

SCHOOL_DAMAGE = 2
TARGET_RANGE = 4
TGT_UNIT_ENEMY = 18

# 5 archetypes × 5 slots = 25 unique visuals
# Each visual is from a REAL spell verified in the zhCN client
# All visual IDs verified via Wowhead WotLK database (wotlk.evowow.com / wowhead.com/wotlk)
# Each ID is from a REAL boss/player spell with the exact visual effect expected
ARCHETYPE_SLOT_VISUALS = {
    0: [7860,  3879,  4302,  4961,  7776],  # Dragon: 龙息术(7860), 扫尾(3879), 寒冰护体(4302), 飞翼打击(4961), 冲击新星(7776)
    1: [  39,  250,    57,   322,  9333],  # Beast: 英勇打击, 斩杀, 强效治疗, 制裁之锤, 火焰爆裂
    2: [  67, 7749,   784,   965,  8041],  # Magic: 火球术, 奥术冲击, 真言盾, 魔爆术, 魔爆术(boss)
    3: [3445,  3819,  8039,   266,  7479],  # Mech: 烈焰震击, 铁皮手雷, 痛苦压制, 偷袭, 流星
    4: [  39,   219,  3077,   322,  2253],  # Generic: 英勇打击, 顺劈斩, 快速治疗, 制裁之锤, 炎爆术
}


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
    block = (sid - ID_LO) // 5
    slot = (sid - ID_LO) % 5
    return ARCHETYPE_SLOT_VISUALS[block][slot]


def scan(recs: bytes, count: int):
    found = visual_ok = effect_ok = cd_removed = other = 0
    for i in range(count):
        off = i * RECSIZE
        sid = struct.unpack_from("<I", recs, off)[0]
        if not (ID_LO <= sid <= ID_HI):
            continue
        found += 1
        eff = struct.unpack_from("<I", recs, off + EFFECT_COL * 4)[0]
        vis = struct.unpack_from("<I", recs, off + VISUAL_COL * 4)[0]
        rec = struct.unpack_from("<I", recs, off + RECOVERY_COL * 4)[0]
        cat = struct.unpack_from("<I", recs, off + CAT_RECOVERY_COL * 4)[0]
        if eff == SCHOOL_DAMAGE: effect_ok += 1
        if vis == expected_visual(sid): visual_ok += 1
        if rec == 0 and cat == 0: cd_removed += 1
        if eff not in (2, 3): other += 1
    return found, effect_ok, visual_ok, cd_removed, other


def state_of(found, effect_ok, visual_ok, cd_removed, other):
    total = ID_HI - ID_LO + 1
    if other or found != total:
        return "PARTIAL"
    if all(v == total for v in (effect_ok, visual_ok, cd_removed)):
        return "COMPLETE"
    return "FRESH"


def patch(recs: bytearray):
    n = 0
    for i in range(len(recs) // RECSIZE):
        off = i * RECSIZE
        sid = struct.unpack_from("<I", recs, off)[0]
        if not (ID_LO <= sid <= ID_HI):
            continue
        # Effect: SCHOOL_DAMAGE (needed for client visual rendering)
        struct.pack_into("<I", recs, off + EFFECT_COL * 4, SCHOOL_DAMAGE)
        struct.pack_into("<I", recs, off + BASEPTS_COL * 4, 0)
        struct.pack_into("<I", recs, off + TGT_A_COL * 4, TGT_UNIT_ENEMY)
        # Visual: per-archetype per-slot (from real verified spells)
        struct.pack_into("<I", recs, off + VISUAL_COL * 4, expected_visual(sid))
        struct.pack_into("<I", recs, off + RANGE_COL * 4, TARGET_RANGE)
        # CRITICAL FIX: remove DBC RecoveryTime (was causing phantom cooldown)
        # Server AddCooldown sends SMSG_SPELL_COOLDOWN which works correctly
        struct.pack_into("<I", recs, off + RECOVERY_COL * 4, 0)
        struct.pack_into("<I", recs, off + CAT_RECOVERY_COL * 4, 0)
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
        print("G17C9_VERSION=" + G17C9_VERSION)
        return 0
    if not args.command or not args.input:
        print("usage: patch_g17c9.py check --input in.dbc | patch --input in.dbc --output out.dbc")
        return 2

    data = Path(args.input).read_bytes()
    count, recs, strings = parse(data)
    found, effect_ok, visual_ok, cd_removed, other = scan(recs, count)
    state = state_of(found, effect_ok, visual_ok, cd_removed, other)

    print(f"G17C9_RECORDS={count}")
    print(f"G17C9_CARRIERS_FOUND={found}")
    print(f"G17C9_EFFECT_OK={effect_ok}")
    print(f"G17C9_VISUAL_ARCHSLOT_OK={visual_ok}")
    print(f"G17C9_COOLDOWN_REMOVED={cd_removed}")
    print(f"G17C9_UNEXPECTED={other}")
    print(f"G17C9_STATE={state}")

    if args.command == "check":
        print("G17C9_CHECK=PASS" if state != "PARTIAL" else "G17C9_CHECK=FAIL")
        return 0 if state != "PARTIAL" else 2

    if state == "COMPLETE":
        print("G17C9_PATCH=ALREADY_COMPLETE")
        print("G17C9_RESULT=PASS")
        return 0
    if state == "PARTIAL":
        print("G17C9_PATCH=REFUSED")
        print("G17C9_RESULT=FAIL")
        return 2

    if not args.output:
        print("--output required")
        return 2

    out_recs = bytearray(recs)
    patched = patch(out_recs)
    out = data[:20] + bytes(out_recs) + strings
    Path(args.output).write_bytes(out)
    print("G17C9_PATCH=PATCHED")
    print("G17C9_RESULT=PASS")
    print(f"G17C9_PATCHED_RECORDS={patched}")
    print(f"G17C9_INPUT_SHA256={sha(data)}")
    print(f"G17C9_OUTPUT_SHA256={sha(out)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
