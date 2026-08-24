#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
PAYLOAD = ROOT / "payload"
SQL = ROOT / "sql" / "G23P2_daily_reward_atomic.sql"


def require(cond: bool, msg: str) -> None:
    if not cond:
        raise AssertionError(msg)


def text(rel: str) -> str:
    return (PAYLOAD / rel).read_text(encoding="utf-8")


def static_gates() -> None:
    core = text("lua_scripts/extensions/G23Core.ext")
    announce = text("lua_scripts/custom_announce.lua")
    daily = text("lua_scripts/custom_daily_reward.lua")
    tele = text("lua_scripts/custom_teleport.lua")
    big = text("lua_scripts/bignum_selftest.lua")
    diag = text("lua_scripts/custom_diag.lua")
    obj = text("lua_scripts/extensions/ObjectVariables.ext")
    welcome = text("lua_scripts/custom_welcome.lua")
    sql = SQL.read_text(encoding="utf-8")

    require("RegisterPlayerEvent(42, commandDispatcher)" in core, "missing shared command dispatcher")
    require("CharDBQuery is synchronous" in core, "synchronous DML contract not documented")
    require("GetStateMapId() == -1" in announce, "announcement not world-state bound")
    require("timerId ~= nil" in announce, "announcement lacks same-state duplicate guard")
    require("RegisterPlayerEvent" not in announce, "announcement must not start per login/map state")

    for token in ("SELECT LOWER(REPLACE(UUID()", "INSERT IGNORE", "ownerToken ~= token",
                  "status='pending'", "status='granted'", "releaseClaim", "ModifyMoney, player, -gold"):
        require(token in daily, f"daily atomic/compensation gate missing: {token}")
    require("CharDBExecute" not in daily, "daily reward must not use async CharDBExecute")

    for method in ("IsInCombat", "IsPvPFlagged", "InBattleground", "InArena",
                   "InBattlegroundQueue", "IsOnVehicle", "IsMounted", "IsFlying",
                   "GetMovementType", "IsAlive", "IsDead", "IsDungeon", "IsRaid", "GetGMRank"):
        require(method in tele, f"teleport safety method missing: {method}")
    require(tele.count("checkSafety(player") >= 5, "safety must be rechecked on command/gossip/final teleport")
    require("SESSION_TTL" in tele and "sweepSessions" in tele, "session timeout cleanup missing")

    require("or true" not in big, "constant-true assertion remains")
    require("p53 + 1 == p53" in big and "p53 - 1 ~= p53" in big, "2^53 boundary assertion incorrect")
    require("可选标准库（不计入必须PASS）" in big, "required/optional split missing")

    first_query = min((diag.find("G23.CharQuery("), diag.find("G23.WorldQuery(")))
    handler = diag.find("local function runDiag")
    require(first_query > handler >= 0, "diagnostic DB access must be inside on-demand handler")
    require("RegisterServerEvent" not in obj and "GetGUIDLow" not in obj.split("--")[0],
            "ObjectVariables quarantine reintroduced delete callback")
    require("G23.GetHelpEntries()" in welcome, "help2 is not shared-registry driven")

    upper = sql.upper()
    for banned in ("DELIMITER", "CREATE PROCEDURE", "CREATE FUNCTION", "CURSOR"):
        require(banned not in upper, f"banned SQL construct: {banned}")
    require("PRIMARY KEY (guid, claim_date)" in sql, "daily unique claim gate missing")
    require("UNIQUE KEY uq_custom_daily_reward_claim_token" in sql, "claim token uniqueness missing")


def model_gates() -> None:
    # Minimal model of the SQL PRIMARY KEY + token ownership contract.
    claim = None
    grants = 0

    def reserve(token: str) -> bool:
        nonlocal claim
        if claim is None:
            claim = {"token": token, "status": "pending"}
        return claim["token"] == token and claim["status"] == "pending"

    def finalize(token: str) -> None:
        nonlocal grants
        require(claim is not None and claim["token"] == token and claim["status"] == "pending",
                "non-owner finalized claim")
        grants += 1
        claim["status"] = "granted"

    # Two independent Lua states race with different DB UUID tokens.
    winners = [reserve("a" * 32), reserve("b" * 32)]
    require(winners == [True, False], f"atomic gate produced wrong winners: {winners}")
    finalize("a" * 32)
    require(grants == 1 and not reserve("b" * 32), "duplicate grant possible after finalization")

    # Pre-award failure/compensation can release pending and allow one safe retry.
    claim = None
    require(reserve("c" * 32), "first retry fixture failed")
    claim = None  # equivalent to verified DELETE after successful money rollback
    require(reserve("d" * 32), "released claim did not allow retry")
    finalize("d" * 32)
    require(grants == 2, "retry did not finalize exactly once")


if __name__ == "__main__":
    static_gates()
    model_gates()
    print("G23P2_STATIC_AND_MODEL=PASS")
