#!/usr/bin/env python3
"""Offline acceptance for the immutable G17-B0 live-world v2 probe result."""
from __future__ import annotations

from collections import Counter
from hashlib import sha256
from pathlib import Path

HERE = Path(__file__).resolve().parent
RESULT = HERE / "证据/G17B0_DB_PROBE_RESULT_20260822_1340.txt"
EXPECTED_SIZE = 17472
EXPECTED_SHA256 = "af4a6031f4895767c34faf1211bb5ba8d026039beefa0d8cd79b52fe5a8a470e"


def require(ok: bool, message: str) -> None:
    if not ok:
        raise AssertionError(message)


def parse_rows(text: str) -> list[tuple[str, str, str, str]]:
    rows: list[tuple[str, str, str, str]] = []
    for line in text.splitlines():
        if not line.startswith("|") or line.startswith("|---"):
            continue
        parts = [part.strip() for part in line.split("|")[1:-1]]
        if parts and parts[0] == "seq":
            continue
        require(len(parts) == 4, f"malformed result row: {line!r}")
        rows.append(tuple(parts))
    return rows


def one(rows: list[tuple[str, str, str, str]], seq: str, section: str, key: str) -> str:
    values = [row[3] for row in rows if row[:3] == (seq, section, key)]
    require(len(values) == 1, f"expected one row for {(seq, section, key)}, got {len(values)}")
    return values[0]


data = RESULT.read_bytes()
require(len(data) == EXPECTED_SIZE, "result size drift")
require(sha256(data).hexdigest() == EXPECTED_SHA256, "result hash drift")
require(b"\r\n" in data and b"\n" not in data.replace(b"\r\n", b""), "expected pure CRLF")
text = data.decode("ascii")
rows = parse_rows(text)
require(len(rows) == 50, f"expected 50 data rows, got {len(rows)}")
require(Counter(row[0] for row in rows) == {
    "10": 1, "11": 1, "12": 1, "13": 1, "20": 7, "30": 30,
    "40": 1, "50": 1, "60": 4, "70": 1, "120": 1, "130": 1,
}, "unexpected sequence distribution")

require(one(rows, "10", "META", "marker") == "G17B0_DB_PROBE_START", "start marker")
require(one(rows, "11", "META", "database") == "world", "wrong database")
require(one(rows, "12", "META", "database_version") == "8.0.46", "database version")
require(one(rows, "13", "META", "probe_time") == "2026-08-22 13:40:46", "probe time")
require(one(rows, "40", "PREIMAGE", "state") == "G17B0_DB_PREIMAGE_READY", "preimage state")
require(one(rows, "130", "SUMMARY", "marker") == "G17B0_DB_PROBE_COMPLETE", "complete marker")

expected_tables = {
    "creature_template", "creature_template_spell", "creature_template_movement",
    "spell_script_names", "spell_dbc", "npc_spellclick_spells",
    "vehicle_template_accessory",
}
table_rows = [row for row in rows if row[0] == "20"]
require({row[2] for row in table_rows} == expected_tables, "table set")
require(all(row[3] == "engine=InnoDB;collation=utf8mb4_unicode_ci" for row in table_rows),
        "table engine/collation")

required_columns = {
    "creature_template.entry", "creature_template.difficulty_entry_1",
    "creature_template.difficulty_entry_2", "creature_template.difficulty_entry_3",
    "creature_template.name", "creature_template.subname", "creature_template.IconName",
    "creature_template.modelid1", "creature_template.modelid2",
    "creature_template.modelid3", "creature_template.modelid4",
    "creature_template.VehicleId", "creature_template.ScriptName",
    "creature_template.AIName", "creature_template.MovementType",
    "creature_template.VerifiedBuild", "creature_template_spell.CreatureID",
    "creature_template_spell.Index", "creature_template_spell.Spell",
    "creature_template_spell.VerifiedBuild", "creature_template_movement.CreatureId",
    "creature_template_movement.Ground", "creature_template_movement.Swim",
    "creature_template_movement.Flight", "creature_template_movement.Rooted",
    "creature_template_movement.Chase", "creature_template_movement.Random",
    "creature_template_movement.InteractionPauseTimer",
    "spell_script_names.spell_id", "spell_script_names.ScriptName",
}
column_rows = [row for row in rows if row[0] == "30"]
require(len(column_rows) == 30 and {row[2] for row in column_rows} == required_columns,
        "required column set")

creature = one(rows, "50", "CREATURE", "27756")
for token in (
    "name=Ruby Drake", "models=25854,0,0,0", "level=80-80", "faction=35",
    "VehicleId=70", "MovementType=0", "HoverHeight=1", "flags_extra=64",
    "ScriptName=npc_ruby_emerald_amber_drake", "VerifiedBuild=12340",
):
    require(token in creature, f"source creature token: {token}")

actions = {(row[2], row[3]) for row in rows if row[0] == "60"}
require(actions == {
    ("27756:0", "Spell=50232;VerifiedBuild=12340"),
    ("27756:1", "Spell=50240;VerifiedBuild=12340"),
    ("27756:2", "Spell=50253;VerifiedBuild=12340"),
    ("27756:5", "Spell=53389;VerifiedBuild=12340"),
}, "source action bar")
require(one(rows, "70", "MOVEMENT", "27756") ==
        "Ground=0;Swim=1;Flight=1;Rooted=0;Chase=0;Random=0;Pause=NULL",
        "source movement")
require(one(rows, "120", "SUMMARY", "counts") ==
        "source_rows=1;source_vehicle_rows=1;target_rows=0;source_actions=4;"
        "target_actions=0;candidate_script_bindings=0", "summary counts")

# No result rows here means no existing world-table collision for these categories.
require(not any(row[0] in {"80", "90", "100", "110"} for row in rows),
        "unexpected spellclick/accessory/script/custom-spell rows")
require("BLOCKED_" not in text and "ERROR" not in text.upper(), "blocked/error marker")

print("G17B0_DB_RESULT_SIZE=17472")
print(f"G17B0_DB_RESULT_SHA256={EXPECTED_SHA256}")
print("G17B0_DB_RESULT_ROWS=50")
print("G17B0_DB_DATABASE=world")
print("G17B0_DB_VERSION=8.0.46")
print("G17B0_DB_TABLES=7")
print("G17B0_DB_COLUMNS=30")
print("G17B0_DB_SOURCE_ENTRY=27756")
print("G17B0_DB_SOURCE_VEHICLE_ID=70")
print("G17B0_DB_TARGET_1000171_ROWS=0")
print("G17B0_DB_CANDIDATE_SCRIPT_BINDINGS=0")
print("G17B0_DB_PREIMAGE_READY=True")
print("G17B0_DB_PROBE_ACCEPTANCE=PASS")
