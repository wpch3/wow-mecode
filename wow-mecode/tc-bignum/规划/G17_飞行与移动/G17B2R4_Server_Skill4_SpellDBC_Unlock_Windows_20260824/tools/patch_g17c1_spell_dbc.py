#!/usr/bin/env python3
"""G17C1: zero the DBC cast gates of spell 52226 ("飞行器着陆").

Why: the 3.3.5a client reads Spell.dbc locally when the player presses the
vehicle action-bar button.  52226 carries RequiresSpellFocus=1553 (flying
machine focus object) and CasterAuraSpell=52255 (flying machine aura), both
of which only exist in the original quest flow.  The client therefore
rejects the cast locally and never sends the cast packet to the server, so
the server-side SpellInfo sanitizer (G17-B2R3 EnsureLandingCommandCastable)
can never run - which is exactly why skill 4 "did not change" after R3.

This patcher clears those two fields IN THE DBC FILE ITSELF (the same file
the client loads from a custom MPQ and the server loads from dbc/).  The
button keeps its name/icon; only the hidden cast gates are removed.

Semantic guards (refuse to touch a foreign layout):
  - record 52226 exists
  - Attributes (col 4) == 0x100 (SPELL_ATTR0_CASTABLE_WHILE_MOUNTED)
  - RequiresSpellFocus (col 18) == 1553
  - CasterAuraSpell (col 24) == 52255
  - Effect1 (col 71) == 3 (SPELL_EFFECT_DUMMY)
  - the spell's localized Name (one of cols 136..151) == "飞行器着陆"
If any guard fails: exit 1, write nothing, and dump a diagnostic report.

Idempotent: if 18/24 are already 0 the file is left untouched and the
report says ALREADY_CLEAN.

Usage:
  python patch_g17c1_spell_dbc.py patch --input in.dbc --output out.dbc --report r.txt
  python patch_g17c1_spell_dbc.py check --input in.dbc --report r.txt
"""

from __future__ import annotations

import argparse
import hashlib
import struct
import sys
from pathlib import Path

SPELL_ID = 52226
FOCUS_COL = 18
AURA_COL = 24
EXPECTED_FOCUS = 1553
EXPECTED_AURA = 52255
NAME_TEXT = "飞行器着陆"


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def parse_dbc(data: bytes) -> tuple[int, int, int, bytes, bytes]:
    """Return (count, fields, recsize, records, string_block)."""
    magic, count, fields, recsize, strsz = struct.unpack_from("<5I", data, 0)
    if data[:4] != b"WDBC":
        raise RuntimeError("not a WDBC file")
    if recsize != fields * 4:
        raise RuntimeError(f"recsize {recsize} != fields*4 {fields * 4}")
    need = 20 + count * recsize + strsz
    if len(data) != need:
        raise RuntimeError(f"file size mismatch: {len(data)} != {need}")
    base = 20
    records = data[base:base + count * recsize]
    strings = data[base + count * recsize:base + count * recsize + strsz]
    return count, fields, recsize, records, strings


def cstr(strings: bytes, idx: int) -> str:
    if idx == 0:
        return ""
    try:
        end = strings.index(b"\0", idx)
        return strings[idx:end].decode("utf-8", "replace")
    except ValueError:
        return "?"


def find_record(records: bytes, count: int, recsize: int, spell_id: int,
                fields: int) -> tuple[int, tuple[int, ...]]:
    for i in range(count):
        off = i * recsize
        sid = struct.unpack_from("<I", records, off)[0]
        if sid == spell_id:
            vals = struct.unpack_from("<" + "I" * fields, records, off)
            return i, vals
    raise RuntimeError(f"spell {spell_id} not found")


def detect_name_col(vals: tuple[int, ...], strings: bytes) -> int | None:
    # Localized Name array (cols 136..151).  Any of them may hold zhCN.
    for col in range(136, min(152, len(vals))):
        if cstr(strings, vals[col]) == NAME_TEXT:
            return col
    return None


def audit(data: bytes) -> dict:
    count, fields, recsize, records, strings = parse_dbc(data)
    rec_idx, vals = find_record(records, count, recsize, SPELL_ID, fields)
    name_col = detect_name_col(vals, strings)
    guards = {
        "record_index": rec_idx,
        "spell_id": vals[0],
        "attributes_col4": vals[4],
        "attributes_expected": 0x100,
        "focus_col18": vals[FOCUS_COL],
        "focus_expected": EXPECTED_FOCUS,
        "aura_col24": vals[AURA_COL],
        "aura_expected": EXPECTED_AURA,
        "effect1_col71": vals[71] if fields > 71 else None,
        "effect1_expected": 3,
        "name_col": name_col,
        "name": NAME_TEXT if name_col is not None else None,
    }
    guards["ok"] = bool(
        vals[0] == SPELL_ID and vals[4] == 0x100
        and vals[FOCUS_COL] == EXPECTED_FOCUS and vals[AURA_COL] == EXPECTED_AURA
        and fields > 71 and vals[71] == 3 and name_col is not None)
    guards["already_clean"] = bool(
        vals[0] == SPELL_ID and vals[4] == 0x100
        and vals[FOCUS_COL] == 0 and vals[AURA_COL] == 0 and name_col is not None)
    return guards


def write_report(report: Path, lines: list[str]) -> None:
    report.write_text("\n".join(lines) + "\n", encoding="utf-8")


def do_patch(args) -> int:
    data = Path(args.input).read_bytes()
    count, fields, recsize, records, strings = parse_dbc(data)
    a = audit(data)
    report_lines = []
    for k, v in a.items():
        report_lines.append(f"{k}={v}")
    if a["already_clean"]:
        report_lines.append("G17C1_SPELL_DBC_STATE=ALREADY_CLEAN")
        report_lines.append("G17C1_SPELL_DBC_PATCH=PASS")
        write_report(Path(args.report), report_lines)
        print("G17C1_SPELL_DBC_STATE=ALREADY_CLEAN")
        print("G17C1_SPELL_DBC_PATCH=PASS")
        return 0
    if not a["ok"]:
        report_lines.append("G17C1_SPELL_DBC_STATE=GUARD_FAIL")
        report_lines.append("G17C1_SPELL_DBC_PATCH=FAIL")
        write_report(Path(args.report), report_lines)
        print("G17C1_SPELL_DBC_STATE=GUARD_FAIL")
        print("G17C1_SPELL_DBC_PATCH=FAIL")
        return 2

    # Rewrite just the two 32-bit fields of the target record.
    out = bytearray(data)
    rec_off = 20 + a["record_index"] * recsize
    struct.pack_into("<I", out, rec_off + FOCUS_COL * 4, 0)
    struct.pack_into("<I", out, rec_off + AURA_COL * 4, 0)

    before = sha(data)
    after = sha(bytes(out))
    report_lines.append(f"input_sha256={before}")
    report_lines.append(f"output_sha256={after}")
    report_lines.append("G17C1_SPELL_DBC_STATE=PATCHED")
    report_lines.append("G17C1_SPELL_DBC_PATCH=PASS")
    write_report(Path(args.report), report_lines)
    Path(args.output).write_bytes(out)
    print(f"G17C1_SPELL_DBC_STATE=PATCHED")
    print(f"G17C1_SPELL_DBC_PATCH=PASS")
    print(f"INPUT_SHA256={before}")
    print(f"OUTPUT_SHA256={after}")
    return 0


def do_check(args) -> int:
    data = Path(args.input).read_bytes()
    a = audit(data)
    report_lines = [f"{k}={v}" for k, v in a.items()]
    state = "PATCHED" if a["ok"] else ("ALREADY_CLEAN" if a["already_clean"] else "GUARD_FAIL")
    report_lines.append(f"G17C1_SPELL_DBC_STATE={state}")
    write_report(Path(args.report), report_lines)
    for line in report_lines:
        print(line)
    return 0 if (a["ok"] or a["already_clean"]) else 2


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("command", choices=("patch", "check"))
    ap.add_argument("--input", required=True)
    ap.add_argument("--output", default="")
    ap.add_argument("--report", required=True)
    args = ap.parse_args()
    if args.command == "patch":
        if not args.output:
            print("--output required for patch")
            return 2
        return do_patch(args)
    return do_check(args)


if __name__ == "__main__":
    sys.exit(main())
