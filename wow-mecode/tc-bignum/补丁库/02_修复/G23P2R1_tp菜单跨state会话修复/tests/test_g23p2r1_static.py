#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OLD = (ROOT / "original/lua_scripts/custom_teleport.lua").read_text(encoding="utf-8")
NEW = (ROOT / "payload/lua_scripts/custom_teleport.lua").read_text(encoding="utf-8")


def need(value: bool, message: str) -> None:
    if not value:
        raise AssertionError(message)


need("local sessions = {}" in OLD, "locked P2 original does not contain reproduced state-local session")
need("sessionOf(player, false)" in OLD, "locked P2 original lacks failing lookup")
need("传送会话已超时" in OLD, "locked P2 original lacks reported message")
for forbidden in ("local sessions", "SESSION_TTL", "sessionOf(", "传送会话已超时"):
    need(forbidden not in NEW, f"stateful-session dependency remains: {forbidden}")
for required in (
    "SENDER_PAGE", "encodePage", "decodePage", "queryTarget", "WHERE t.id=",
    "showCategoryPage", "checkSafety(player", "RegisterPlayerGossipEvent(MENU_ID, 2, onGossip)",
):
    need(required in NEW, f"stateless/safety element missing: {required}")
for method in (
    "IsInCombat", "IsPvPFlagged", "InBattleground", "InArena", "InBattlegroundQueue",
    "IsOnVehicle", "IsMounted", "IsFlying", "GetMovementType", "IsAlive", "IsDead",
    "IsDungeon", "IsRaid", "GetGMRank",
):
    need(method in NEW, f"safety method regressed: {method}")
need(NEW.count("checkSafety(player") >= 5, "safety is not rechecked on all entry/final paths")
need("CharDB" not in NEW, "hotfix unexpectedly added character DB dependency")
need("RegisterPlayerEvent(4" not in NEW, "obsolete logout session cleanup remains")
print("G23P2R1_STATIC=PASS")
