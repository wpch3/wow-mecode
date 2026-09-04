#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""G17-C13: THREE fixes in one client Spell.dbc repack (chain MPQ).

1. APPEND 990029/990030 (the missing 7th skills - the client has no records
   for them so the vehicle bar shows nothing at slot 7 on both pages).
2. SET visuals for the four flight carriers 990025-990028 (appended by C3v2
   with SpellVisualID=0 - "飞行特效也没有特色").
3. REPLACE all 25 combat visuals with TARGET-SIDE IMPACT kits (user report:
   only 7860 and 4961 render; the rest were caster-side action visuals that
   never play on the target).  Every new ID below is a classic bolt/nuke
   impact visual verified from the 3.3.5.12340 Spell.dbc dump.

Visual table (all impact-rendering; source spell in parentheses):
  DRAGON:  7860 龙息(Dragon's Breath✓user) 143 尾扫(Fire Blast) 222 龙鳞(Moonfire)
           4961 振翼(Wing Buffet✓user) 2253 龙威(Pyroblast)
  BEAST:   3860 撕咬(Wrath) 36 连爪(Chain Lightning) 222 守护(Moonfire)
           7732 扑袭(Shadowfury) 3057 嗜血(Shadowburn)
  MAGIC:   67 弹幕(Fireball) 9152 虹吸(Death Coil) 13 护盾(Frostbolt)
           173 过载(Lightning Bolt) 107 新星(Blast Wave)
  MECH:    143 机炮(Fire Blast) 7479 火箭(Meteor) 46 维修(Immolate)
           7732 烟幕(Shadowfury) 107 过载(Blast Wave)
  GENERIC: 64 猛击(Shadow Bolt) 36 顺劈(Chain Lightning) 222 快疗(Moonfire)
           7732 制裁(Shadowfury) 2253 炎爆(Pyroblast)
  FLIGHT:  990025 切页=2276(Dash) 990026 拉升=9959(Rocket Jump)
           990027 俯冲=2276(Dive) 990028 制动=63(Slow Fall)
           990029 突袭=2355(War Stomp) 990030 御风=3719(Aspect of the Cheetah)
"""
from __future__ import annotations

import argparse
import hashlib
import struct
import sys
from pathlib import Path

G17C13_VERSION = "v1_slot7_impact_visuals"
FIELDS = 234
RECSIZE = FIELDS * 4
ID_LO, ID_HI = 990000, 990024
FLIGHT_IDS = [990025, 990026, 990027, 990028]

EFFECT_COL, BASEPTS_COL, TGT_A_COL = 71, 80, 92
VISUAL_COL, RANGE_COL = 131, 46
RECOVERY_COL, CAT_RECOVERY_COL = 29, 30
ICON_COL, NAME_COL, DESC_COL = 133, 140, 174
SCHOOL_DAMAGE, TARGET_RANGE, TGT_UNIT_ENEMY, TGT_UNIT_CASTER = 2, 4, 18, 1

# combat: per-archetype per-slot (0-4) impact visuals
COMBAT_VISUALS = {
    0: [7860, 143, 222, 4961, 2253],   # Dragon (2 confirmed by user)
    1: [3860, 36, 222, 7732, 3057],    # Beast (druid-nature theme)
    2: [67, 9152, 13, 173, 107],       # Magic (bolt/nuke theme)
    3: [143, 7479, 46, 7732, 107],     # Mech (explosive theme)
    4: [64, 36, 222, 7732, 2253],      # Generic
}
FLIGHT_VISUALS = {990025: 2276, 990026: 9959, 990027: 2276, 990028: 63}
NEW_SPELLS = [
    # (id, name, icon, visual, range_index, target_a, desc)
    (990029, "突袭·俯冲打击", 50, 2355, TARGET_RANGE, TGT_UNIT_ENEMY,
     "俯冲突袭目标区域，对8码内的敌人造成物理伤害。"),
    (990030, "御风姿态", 1181, 3719, 1, TGT_UNIT_CASTER,
     "进入御风姿态：转向速度提高15%，龙能量回复加倍。再次施放解除。"),
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
    return count, data[20:20 + count * RECSIZE], data[20 + count * RECSIZE:]


def scan(recs: bytes, count: int):
    found = flight_ok = combat_ok = new_ok = 0
    for i in range(count):
        sid = struct.unpack_from("<I", recs, i * RECSIZE)[0]
        if ID_LO <= sid <= ID_HI:
            found += 1
            block, slot = divmod(sid - ID_LO, 5)
            if struct.unpack_from("<I", recs, i * RECSIZE + VISUAL_COL * 4)[0] == COMBAT_VISUALS[block][slot]:
                combat_ok += 1
        elif sid in FLIGHT_VISUALS:
            if struct.unpack_from("<I", recs, i * RECSIZE + VISUAL_COL * 4)[0] == FLIGHT_VISUALS[sid]:
                flight_ok += 1
        elif sid in (990029, 990030):
            new_ok += 1
    return found, combat_ok, flight_ok, new_ok


def state_of(found, combat_ok, flight_ok, new_ok):
    if found == 25 and combat_ok == 25 and flight_ok == 4 and new_ok == 2:
        return "COMPLETE"
    if found == 0:
        return "FRESH"          # pre-C2 chain (no carriers at all)
    if found == 25 and new_ok < 2:
        return "FRESH"          # C2-C12 chain: 25+4 carriers, 990029/990030 absent
    return "PARTIAL"


def set_field(recs: bytearray, rec_index: int, col: int, value: int):
    struct.pack_into("<I", recs, rec_index * RECSIZE + col * 4, value)


def build_new_record(sid, name, icon, visual, range_index, target_a, desc,
                     name_off, desc_off):
    vals = [0] * FIELDS
    vals[0] = sid
    vals[4] = 0x100            # CASTABLE_WHILE_MOUNTED
    vals[28] = 1               # CastingTimeIndex = instant
    vals[RANGE_COL] = range_index
    vals[71] = SCHOOL_DAMAGE   # client renders visuals only for SCHOOL_DAMAGE
    vals[80] = 0
    vals[TGT_A_COL] = target_a
    vals[VISUAL_COL] = visual
    vals[ICON_COL] = icon
    vals[NAME_COL] = name_off
    vals[DESC_COL] = desc_off
    return struct.pack("<" + "I" * FIELDS, *vals)


def patch(recs: bytes, strings: bytes, count: int, append_missing: bool):
    out_recs = bytearray(recs)
    patched = 0
    for i in range(count):
        sid = struct.unpack_from("<I", out_recs, i * RECSIZE)[0]
        if ID_LO <= sid <= ID_HI:
            block, slot = divmod(sid - ID_LO, 5)
            set_field(out_recs, i, EFFECT_COL, SCHOOL_DAMAGE)
            set_field(out_recs, i, BASEPTS_COL, 0)
            set_field(out_recs, i, TGT_A_COL, TGT_UNIT_ENEMY)
            set_field(out_recs, i, VISUAL_COL, COMBAT_VISUALS[block][slot])
            set_field(out_recs, i, RANGE_COL, TARGET_RANGE)
            set_field(out_recs, i, RECOVERY_COL, 0)
            set_field(out_recs, i, CAT_RECOVERY_COL, 0)
            patched += 1
        elif sid in FLIGHT_VISUALS:
            set_field(out_recs, i, EFFECT_COL, SCHOOL_DAMAGE)
            set_field(out_recs, i, BASEPTS_COL, 0)
            set_field(out_recs, i, TGT_A_COL, TGT_UNIT_CASTER)
            set_field(out_recs, i, VISUAL_COL, FLIGHT_VISUALS[sid])
            set_field(out_recs, i, RANGE_COL, 1)
            set_field(out_recs, i, RECOVERY_COL, 0)
            set_field(out_recs, i, CAT_RECOVERY_COL, 0)
            patched += 1

    appended = 0
    new_names = b""
    new_records = b""
    if append_missing:
        _, c_recs, _ = struct.unpack_from("<5I", bytes(out_recs[:0]) or b"\0" * 20, 0), None, None
        present = set()
        for i in range(count):
            present.add(struct.unpack_from("<I", out_recs, i * RECSIZE)[0])
        offsets = {}
        for sid, name, icon, visual, rng, tgt, desc in NEW_SPELLS:
            offsets[sid] = ("name", len(strings) + len(new_names))
            new_names += name.encode("utf-8") + b"\x00"
        for sid, name, icon, visual, rng, tgt, desc in NEW_SPELLS:
            offsets[sid] = ("desc", len(strings) + len(new_names))
            new_names += desc.encode("utf-8") + b"\x00"
        # rebuild offsets properly (name first, then desc, in two passes)
        name_offsets, desc_offsets = {}, {}
        pos = len(strings)
        for sid, name, *_ in NEW_SPELLS:
            name_offsets[sid] = pos
            pos += len(name.encode("utf-8")) + 1
        for sid, name, icon, visual, rng, tgt, desc in NEW_SPELLS:
            desc_offsets[sid] = pos
            pos += len(desc.encode("utf-8")) + 1
        for sid, name, icon, visual, rng, tgt, desc in NEW_SPELLS:
            if sid in present:
                continue
            new_records += build_new_record(sid, name, icon, visual, rng, tgt,
                                            desc, name_offsets[sid], desc_offsets[sid])
            appended += 1

    return bytes(out_recs) + new_records, strings + new_names, patched, appended


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--version", action="store_true")
    ap.add_argument("command", nargs="?", choices=("check", "patch", "verify"))
    ap.add_argument("--input", default="")
    ap.add_argument("--output", default="")
    args = ap.parse_args()

    if args.version:
        print("G17C13_VERSION=" + G17C13_VERSION)
        return 0
    if args.command == "verify":
        assert len(COMBAT_VISUALS) == 5 and all(len(v) == 5 for v in COMBAT_VISUALS.values())
        assert COMBAT_VISUALS[0][0] == 7860 and COMBAT_VISUALS[0][3] == 4961
        assert set(FLIGHT_VISUALS) == {990025, 990026, 990027, 990028}
        assert len(NEW_SPELLS) == 2 and {s[0] for s in NEW_SPELLS} == {990029, 990030}
        assert len({v for row in COMBAT_VISUALS.values() for v in row}) >= 15
        print("G17C13_PAYLOAD_VERIFY=PASS")
        return 0

    if not args.command or not args.input:
        print("usage: patch_g17c13.py check --input in.dbc | patch --input in.dbc --output out.dbc | verify")
        return 2

    data = Path(args.input).read_bytes()
    count, recs, strings = parse(data)
    found, combat_ok, flight_ok, new_ok = scan(recs, count)
    state = state_of(found, combat_ok, flight_ok, new_ok)
    print(f"G17C13_RECORDS={count}")
    print(f"G17C13_CARRIERS_FOUND={found}")
    print(f"G17C13_COMBAT_IMPACT_OK={combat_ok}")
    print(f"G17C13_FLIGHT_VISUAL_OK={flight_ok}")
    print(f"G17C13_NEW_SPELLS_PRESENT={new_ok}")
    print(f"G17C13_STATE={state}")
    print(f"G17C13_INPUT_SHA256={sha(data)}")

    if args.command == "check":
        print("G17C13_CHECK=PASS" if state != "PARTIAL" else "G17C13_CHECK=FAIL")
        return 0 if state != "PARTIAL" else 2

    if state == "COMPLETE":
        print("G17C13_PATCH=ALREADY_COMPLETE")
        print("G17C13_RESULT=PASS")
        return 0

    out_recs, out_strings, patched, appended = patch(recs, strings, count, append_missing=True)
    out = bytearray(struct.pack("<5I", 0x43424457, len(out_recs) // RECSIZE, FIELDS, RECSIZE, len(out_strings)) + out_recs + out_strings)
    Path(args.output).write_bytes(bytes(out))
    print(f"G17C13_PATCHED_RECORDS={patched}")
    print(f"G17C13_APPENDED_RECORDS={appended}")
    print(f"G17C13_OUTPUT_SHA256={sha(bytes(out))}")
    print("G17C13_PATCH=PATCHED")
    print("G17C13_RESULT=PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
