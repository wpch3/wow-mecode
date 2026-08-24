#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path, PurePosixPath
import hashlib
import re
import zipfile

ROOT = Path(__file__).resolve().parents[1]
PLAN = ROOT.parents[2] / "规划/G17_飞行与移动"
INSTALL = ROOT / "sql/G17B0_world_install_v3_collation_safe.sql"
CHECK = ROOT / "sql/G17B0_world_postcheck_v3_readonly_collation_safe.sql"
ROLLBACK = ROOT / "sql/G17B0_world_rollback_v3_owned_only_collation_safe.sql"
PACKAGE = PLAN / "G17B0_World_Install_v3_20260822"
ZIP = PLAN / "G17B0_World_Install_v3_20260822.zip"
ZIP_SIZE = 11819
ZIP_SHA256 = "a0625d4d0c3098ab88d1ff3e9380fbc80bc504771a4a55f7b2776bc80d2fd254"
TOP = "G17B0_World_Install_v3_20260822"


def require(ok, message):
    if not ok:
        raise AssertionError(message)


def statements(sql: str) -> list[str]:
    """Split MySQL statements outside comments/quotes/backticks."""
    result, current = [], []
    state = "code"
    i = 0
    while i < len(sql):
        char = sql[i]
        nxt = sql[i + 1] if i + 1 < len(sql) else ""
        if state == "code":
            if char == "-" and nxt == "-":
                state = "comment"
                i += 2
                continue
            if char == "'":
                state = "single"
                current.append(char)
                i += 1
                continue
            if char == "`":
                state = "backtick"
                current.append(char)
                i += 1
                continue
            if char == ";":
                text = "".join(current).strip()
                if text:
                    result.append(text)
                current = []
                i += 1
                continue
            current.append(char)
        elif state == "comment":
            if char == "\n":
                state = "code"
                current.append("\n")
        elif state == "single":
            current.append(char)
            if char == "'":
                if nxt == "'":
                    current.append(nxt)
                    i += 2
                    continue
                state = "code"
            elif char == "\\" and nxt:
                current.append(nxt)
                i += 2
                continue
        elif state == "backtick":
            current.append(char)
            if char == "`":
                state = "code"
        i += 1
    require(state in {"code", "comment"}, f"unterminated SQL lexical state: {state}")
    require(not "".join(current).strip(), "missing final semicolon")
    return result


install = INSTALL.read_text(encoding="utf-8")
check = CHECK.read_text(encoding="utf-8")
rollback = ROLLBACK.read_text(encoding="utf-8")
install_statements = statements(install)
check_statements = statements(check)
rollback_statements = statements(rollback)

require("INSERT IGNORE" not in install.upper(), "forbidden INSERT IGNORE")
require("REPLACE INTO" not in install.upper(), "forbidden REPLACE")
require(install_statements[0].upper() == "USE `WORLD`", "install explicit world database")
require(install_statements[1].upper() == "SET NAMES UTF8MB4 COLLATE UTF8MB4_UNICODE_CI",
        "install connection collation")
for var in ("SCRIPT", "S1", "S2", "S3", "S4", "NAME", "SUBNAME"):
    require(re.search(rf"SET @G17B0_{var} := _utf8mb4.* COLLATE utf8mb4_unicode_ci", install),
            f"explicit collation variable: {var}")
require(install.count("COLLATE utf8mb4_unicode_ci") >= 25, "install explicit collation coverage")
require("SET NAMES utf8mb4;" not in install, "old implicit 0900 connection form")
require(sum(s.upper() == "START TRANSACTION" for s in install_statements) == 1, "install transaction start")
require(sum(s.upper() == "COMMIT" for s in install_statements) == 1, "install commit")
require(sum(s.lstrip().upper().startswith("SELECT ") for s in install_statements) == 1,
        "install must expose one result table")
require(install_statements[-1].lstrip().upper().startswith("SELECT CASE"), "final install result")
require("G17B0_WORLD_INSTALL_PASS" in install_statements[-1], "install PASS marker")
for marker in (
    "BLOCKED_SOURCE_27756_NOT_EXACT_VEHICLE_70",
    "BLOCKED_TARGET_1000171_FOREIGN_COLLISION",
    "BLOCKED_FOREIGN_SCRIPT_BINDING_COLLISION",
    "FAILED_TARGET_POSTIMAGE", "FAILED_ACTION_BAR_POSTIMAGE",
    "FAILED_MOVEMENT_POSTIMAGE", "FAILED_SCRIPT_BINDING_POSTIMAGE",
):
    require(marker in install, marker)

# Every permanent write is gated. Temporary clone writes are intentionally excluded.
for statement in install_statements:
    upper = statement.lstrip().upper()
    permanent_write = (
        upper.startswith("INSERT INTO `CREATURE_TEMPLATE`") or
        upper.startswith("UPDATE `CREATURE_TEMPLATE`") or
        upper.startswith("DELETE FROM `CREATURE_TEMPLATE_SPELL`") or
        upper.startswith("INSERT INTO `CREATURE_TEMPLATE_SPELL`") or
        upper.startswith("DELETE FROM `CREATURE_TEMPLATE_MOVEMENT`") or
        upper.startswith("INSERT INTO `CREATURE_TEMPLATE_MOVEMENT`") or
        upper.startswith("DELETE FROM `SPELL_SCRIPT_NAMES`") or
        upper.startswith("INSERT INTO `SPELL_SCRIPT_NAMES`")
    )
    if permanent_write:
        require("@G17B0_CAN_APPLY=1" in upper, f"ungated permanent write: {upper[:80]}")

for token in (
    "(@G17B0_ENTRY,0,9573)", "(@G17B0_ENTRY,1,55215)",
    "(@G17B0_ENTRY,2,52197)", "(@G17B0_ENTRY,3,53208)",
    "`GROUND`=1", "`SWIM`=0", "`FLIGHT`=1", "`ROOTED`=0",
    "`VEHICLEID`=@G17B0_VEHICLE_ID", "`MODELID1`=25854",
):
    require(token in install.upper(), f"install exact mapping: {token}")

require(len(check_statements) == 1 and check_statements[0].lstrip().upper().startswith("SELECT CASE"),
        "postcheck single SELECT")
for token in ("INSERT ", "UPDATE ", "DELETE ", "REPLACE ", "CREATE ", "DROP ", "SET "):
    require(token not in check_statements[0].upper(), f"postcheck write token: {token}")
require("G17B0_WORLD_CHECK_PASS" in check, "postcheck PASS marker")
require(not re.search(r"FROM\s+`(?!world`\.)", check_statements[0], re.IGNORECASE),
        "postcheck must fully qualify every world table")
require(check.count("FROM `world`.") == 16, "postcheck explicit world table count")
require(check.count("COLLATE utf8mb4_unicode_ci") >= 20, "postcheck explicit collation coverage")

require(rollback_statements[0].upper() == "USE `WORLD`", "rollback explicit world database")
require(rollback_statements[1].upper() == "SET NAMES UTF8MB4 COLLATE UTF8MB4_UNICODE_CI",
        "rollback connection collation")
require(sum(s.upper() == "START TRANSACTION" for s in rollback_statements) == 1, "rollback transaction")
require(sum(s.upper() == "COMMIT" for s in rollback_statements) == 1, "rollback commit")
require(sum(s.lstrip().upper().startswith("SELECT ") for s in rollback_statements) == 1,
        "rollback one result table")
for statement in rollback_statements:
    if statement.lstrip().upper().startswith("DELETE FROM"):
        upper = statement.upper()
        require("@G17B0_OWNED=1" in upper and "@G17B0_FOREIGN=0" in upper,
                "rollback owned-only gate")
require("G17B0_WORLD_ROLLBACK_PASS" in rollback, "rollback PASS marker")
require("ROLLBACK_BLOCKED_FOREIGN_TARGET" in rollback, "rollback foreign block")

# Policy truth table for the preimage gate mirrored by SQL.
def can_apply(source_rows, source_vehicle70, target_foreign, foreign_scripts):
    return int(source_rows == 1 and source_vehicle70 == 1 and
               target_foreign == 0 and foreign_scripts == 0)

require(can_apply(1, 1, 0, 0) == 1, "fresh preimage")
require(can_apply(0, 0, 0, 0) == 0, "missing source")
require(can_apply(1, 1, 1, 0) == 0, "foreign target")
require(can_apply(1, 1, 0, 1) == 0, "foreign script")

# Delivery package and ZIP are byte-locked to the audited formal SQL/evidence.
require((PACKAGE / "01_G17B0_world_install_v3_collation_safe.sql").read_bytes() == INSTALL.read_bytes(),
        "delivery install SQL")
require((PACKAGE / "02_G17B0_world_postcheck_v3_readonly_collation_safe.sql").read_bytes() == CHECK.read_bytes(),
        "delivery postcheck SQL")
require((PACKAGE / "99_G17B0_world_rollback_v3_owned_only_collation_safe.sql").read_bytes() == ROLLBACK.read_bytes(),
        "delivery rollback SQL")
checksum_lines = (PACKAGE / "SHA256SUMS.txt").read_text(encoding="utf-8").splitlines()
require(len(checksum_lines) == 9, "delivery checksum count")
seen = set()
for line in checksum_lines:
    digest, rel = line.split("  ", 1)
    require(rel not in seen, "duplicate checksum")
    seen.add(rel)
    file = PACKAGE / PurePosixPath(rel)
    require(file.is_file() and hashlib.sha256(file.read_bytes()).hexdigest() == digest,
            f"delivery checksum: {rel}")
require(ZIP.stat().st_size == ZIP_SIZE and hashlib.sha256(ZIP.read_bytes()).hexdigest() == ZIP_SHA256,
        "delivery ZIP identity")
with zipfile.ZipFile(ZIP) as archive:
    require(archive.testzip() is None, "delivery ZIP CRC")
    infos = archive.infolist()
    require(len(infos) == 10, "delivery ZIP file count")
    for info in infos:
        pure = PurePosixPath(info.filename)
        require(not pure.is_absolute() and ".." not in pure.parts and pure.parts[0] == TOP,
                "delivery ZIP path")
        rel = PurePosixPath(*pure.parts[1:])
        require((PACKAGE / rel).read_bytes() == archive.read(info), f"delivery ZIP bytes: {rel}")

print("G17B0_WORLD_INSTALL_V3_LEXICAL=PASS")
print("G17B0_WORLD_INSTALL_V3_EXPLICIT_DATABASE=PASS")
print("G17B0_WORLD_INSTALL_V3_COLLATION_SAFE=PASS")
print("G17B0_WORLD_INSTALL_V3_ONE_RESULT_TABLE=True")
print("G17B0_WORLD_INSTALL_V3_TRANSACTION=PASS")
print("G17B0_WORLD_INSTALL_V3_EXACT_GATES=PASS")
print("G17B0_WORLD_POSTCHECK_V3_READ_ONLY=PASS")
print("G17B0_WORLD_ROLLBACK_V3_OWNED_ONLY=PASS")
print("G17B0_WORLD_INSTALL_V3_INTERNAL_CHECKSUMS=PASS_9")
print("G17B0_WORLD_INSTALL_V3_ZIP=FINAL_PASS")
