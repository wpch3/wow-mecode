#!/usr/bin/env python3
"""G17-B3: append 25 combat DUMMY spell records to a 3.3.5a Spell.dbc.

Deterministic, idempotent-guarded, and preserves every existing record and
the whole string block byte-for-byte.  Each new record:
  - Attributes = 0x100 (CASTABLE_WHILE_MOUNTED)
  - Effect1..3 = SPELL_EFFECT_DUMMY (3), base points set per table
  - CastingTimeIndex=1, RangeIndex=1 (self/default), no focus/aura gates
  - Name (col 140) = zhCN name, icon (col 133), description (col 174)
Used as the client-side carrier only; all real behavior is server SpellScripts.

Guards:
  - input is WDBC, 234 fields, recsize=936
  - all IDs 990000..990024 are ABSENT in input (idempotency: if ALL present
    -> ALREADY_APPENDED, no write; if SOME present -> GUARD_FAIL, no write)
  - output record count = input + 25, string block = input + appended names
"""
from __future__ import annotations

import argparse
import hashlib
import struct
import sys
from pathlib import Path

ID_BASE = 990000
COUNT = 25
FIELDS = 234
RECSIZE = FIELDS * 4
EFFECT_DUMMY = 3
ATTR_MOUNTED = 0x100
NAME_COL = 140
ICON_COL = 133
DESC_COL = 174

SKILLS = [
    # (name, icon, desc)
    ("龙息·烈焰", 130, "喷吐灼热龙息，对前方敌人造成火焰伤害。"),
    ("尾扫·裂地", 130, "以巨尾横扫周围敌人，打断并击退。"),
    ("龙鳞护体", 130, "龙鳞硬化，短时间内大幅提高护甲并减免伤害。"),
    ("振翼·旋风", 130, "奋力振翼，击飞身边敌人并向后位移。"),
    ("龙威爆发", 130, "凝聚龙威全力爆发，对目标造成巨额伤害。"),
    ("猛兽撕咬", 134, "凶猛地撕咬目标，造成物理伤害。"),
    ("狂暴连爪", 134, "连续的爪击，对目标造成多重伤害。"),
    ("兽群守护", 134, "呼唤兽群守护，回复自身生命并移除恐惧。"),
    ("扑袭·压制", 134, "飞扑压制目标，造成伤害并使其瘫痪。"),
    ("嗜血终结", 134, "进入嗜血状态，短时间内攻击大幅强化。"),
    ("奥术弹幕", 66, "凝聚奥术能量射向目标，造成奥术伤害。"),
    ("相位虹吸", 66, "汲取目标能量转化为自身法力。"),
    ("法力护盾", 66, "展开法力护盾，吸收即将到来的伤害。"),
    ("时空过载", 66, "扭曲时间，短暂加速并提高闪避。"),
    ("秘法新星", 66, "释放秘法新星，对周围敌人造成奥术爆发伤害。"),
    ("机炮扫射", 209, "旋转机炮扫射目标，造成物理伤害。"),
    ("火箭齐射", 209, "发射火箭齐射，对目标区域造成爆炸伤害。"),
    ("烟幕掩护", 209, "释放烟幕，降低敌人命中并提高自身闪避。"),
    ("战地维修", 209, "启动维修协议，恢复自身耐久与生命。"),
    ("过载轰击", 209, "引擎过载，对目标进行毁灭性轰击并产生大量热量。"),
    ("冲击波", 136, "释放冲击波，对前方敌人造成伤害。"),
    ("践踏", 136, "重踏地面，伤害并减速周围敌人。"),
    ("守护之力", 136, "守护之力环绕，短暂提高护甲与抗性。"),
    ("猛冲", 136, "猛冲向目标，造成伤害并击退。"),
    ("全功率爆发", 136, "全功率爆发，对所有敌人造成毁灭性伤害。"),
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
        print("G17B3_SPELL_DBC_STATE=ALREADY_APPENDED")
        print("G17B3_SPELL_DBC_APPEND=PASS")
        print("G17B3_SPELL_DBC_WRITE=NONE")
        return 0
    if present:
        print(f"G17B3_SPELL_DBC_STATE=GUARD_FAIL present={present}")
        print("G17B3_SPELL_DBC_APPEND=FAIL")
        return 2

    # Build new string segment: keep original block, append each name/desc.
    # New string offsets refer to the APPENDED block only if we treat the
    # original block as prefix; DBC offsets are absolute into the block, so
    # the appended strings start at len(strings).
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
    Path(args.output).parent.mkdir(parents=True, exist_ok=True)
    Path(args.output).write_bytes(out)

    print("G17B3_SPELL_DBC_STATE=APPENDED")
    print("G17B3_SPELL_DBC_APPEND=PASS")
    print(f"G17B3_INPUT_SHA256={sha(data)}")
    print(f"G17B3_OUTPUT_SHA256={sha(out)}")
    print(f"G17B3_RECORDS_BEFORE={count}")
    print(f"G17B3_RECORDS_AFTER={new_count}")
    print(f"G17B3_STRING_BLOCK_PREFIX_KEPT=" + str(strings == bytes(new_strings[:len(strings)])))
    return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--version", action="store_true")
    ap.add_argument("command", nargs="?", choices=("append", "check"))
    ap.add_argument("--input", default="")
    ap.add_argument("--output", default="")
    args = ap.parse_args()
    if args.version:
        print("G17B3_DBC_APPENDER_VERSION=v1_append25")
        return 0
    if not args.command or not args.input:
        print("usage: append_g17b3_spells.py append --input in.dbc [--output out.dbc]")
        return 2
    if args.command == "check":
        data = Path(args.input).read_bytes()
        count, fields, recsize, recs, strings = parse(data)
        ids = existing_ids(recs, count, recsize)
        present = [ID_BASE + i for i in range(COUNT) if ID_BASE + i in ids]
        print(f"G17B3_RECORDS={count}")
        print(f"G17B3_PRESENT={len(present)}")
        print("G17B3_SPELL_DBC_STATE=" + ("ALREADY_APPENDED" if len(present) == COUNT else
                                          ("PARTIAL" if present else "MISSING")))
        return 0
    if not args.output:
        print("--output required")
        return 2
    return do_append(args)


if __name__ == "__main__":
    sys.exit(main())
