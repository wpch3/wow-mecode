#!/usr/bin/env python3
"""G17-C10: ALL-archetype visual refinement (dragon row unchanged from user-verified C9 v3).

Every visual ID below is verified against a full 3.3.5.12340 Spell.dbc dump
(Kaev/AzerothcoreDBCToSQL, 49839 rows) by looking up the SOURCE SPELL whose
SpellVisualID_1 equals it. Per-slot source spells (Wowhead WotLK + DBC dump):
  Dragon (UNCHANGED, user-confirmed PASS):
    slot0=7860 Dragon's Breath(42950) slot1=3879 Tail Sweep(68867) slot2=4302 Ice Barrier(11426)
    slot3=4961 Wing Buffet(31475) slot4=7776 Blast Nova(30616)
  Beast:  slot0=6587 Ferocious Bite(48576) slot1=8634 Mangle Cat(33876) slot2=57 Greater Heal(2060)
          slot3=3942 Pounce(9005) slot4=372 Bloodthirst(23881)
  Magic:  slot0=262 Arcane Missiles(5143) slot1=12655 Drain Life(689) slot2=784 Power Word Shield(17)
          slot3=12303 Arcane Overload(56432) slot4=8041 Arcane Explosion boss(29919)
  Mech:   slot0=1904 Machine Gun(10346) slot1=6399 Rocket Blast(1940) slot2=8697 Welding Beam(35919)
          slot3=1164 Smoke Bomb(8817) slot4=7479 Meteor(24340)
  Generic: slot0=1165 Slam(1464) slot1=219 Cleave(845) slot2=3077 Flash Heal(2061)
           slot3=322 Hammer of Justice(853) slot4=2253 Pyroblast(11366)
Also keeps RecoveryTime/CategoryRecoveryTime = 0 (no phantom cooldown; display is
handled by the server cooldown packets + G17DragonBar spellcast tracking).

v3 installer-fix heritage: this patcher ships with the corrected installer flow
(state files give paths only, patcher detects input state, content verification
+ round-trip archive check, real rollback script).

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

G17C10_VERSION = "v1_all_archetypes_visuals"
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
    0: [7860,  3879,  4302,  4961,  7776],  # Dragon (user-verified, DO NOT CHANGE): 龙息术/扫尾/寒冰护体/飞翼打击/冲击新星
    1: [6587,  8634,    57,  3942,   372],  # Beast: 凶猛撕咬(48576)/斜掠猫(33876)/强效治疗(2060)/突袭(9005)/嗜血(23881)
    2: [ 262, 12655,   784, 12303,  8041],  # Magic: 奥术飞弹(5143)/吸取生命(689)/真言盾(17)/奥术超载(56432)/奥爆boss(29919)
    3: [1904,  6399,  8697,  1164,  7479],  # Mech: 机枪(10346)/火箭冲击(1940)/焊接光束(35919)/烟雾弹(8817)/流星(24340)
    4: [1165,   219,  3077,   322,  2253],  # Generic: 猛击(1464)/顺劈斩(845)/快速治疗(2061)/制裁之锤(853)/炎爆术(11366)
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
        print("G17C10_VERSION=" + G17C10_VERSION)
        return 0
    if not args.command or not args.input:
        print("usage: patch_g17c9.py check --input in.dbc | patch --input in.dbc --output out.dbc")
        return 2

    data = Path(args.input).read_bytes()
    count, recs, strings = parse(data)
    found, effect_ok, visual_ok, cd_removed, other = scan(recs, count)
    state = state_of(found, effect_ok, visual_ok, cd_removed, other)

    print(f"G17C10_RECORDS={count}")
    print(f"G17C10_CARRIERS_FOUND={found}")
    print(f"G17C10_EFFECT_OK={effect_ok}")
    print(f"G17C10_VISUAL_ARCHSLOT_OK={visual_ok}")
    print(f"G17C10_COOLDOWN_REMOVED={cd_removed}")
    print(f"G17C10_UNEXPECTED={other}")
    print(f"G17C10_STATE={state}")

    if args.command == "check":
        print("G17C10_CHECK=PASS" if state != "PARTIAL" else "G17C10_CHECK=FAIL")
        return 0 if state != "PARTIAL" else 2

    if state == "COMPLETE":
        print("G17C10_PATCH=ALREADY_COMPLETE")
        print("G17C10_RESULT=PASS")
        return 0
    if state == "PARTIAL":
        print("G17C10_PATCH=REFUSED")
        print("G17C10_RESULT=FAIL")
        return 2

    if not args.output:
        print("--output required")
        return 2

    out_recs = bytearray(recs)
    patched = patch(out_recs)
    out = data[:20] + bytes(out_recs) + strings
    Path(args.output).write_bytes(out)
    print("G17C10_PATCH=PATCHED")
    print("G17C10_RESULT=PASS")
    print(f"G17C10_PATCHED_RECORDS={patched}")
    print(f"G17C10_INPUT_SHA256={sha(data)}")
    print(f"G17C10_OUTPUT_SHA256={sha(out)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
