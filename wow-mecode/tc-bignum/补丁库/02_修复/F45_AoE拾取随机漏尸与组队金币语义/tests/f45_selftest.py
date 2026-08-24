#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""F45 source gates plus deterministic behavioral model."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import hashlib
import random
import subprocess
import sys

BASE = Path(__file__).resolve().parents[1]
ORIGINAL = BASE / "original"
PAYLOAD = BASE / "payload"

EXPECTED = {
    "src/server/game/Handlers/LootHandler.cpp": (
        "35ef90116a1eaf8f3a847fc4e3b4b5c9815afe6f973433d42a56ccd755d436c5",
        "8003bc7e3e7343c7383c9039f9ed8602d7d83e0561a45de1109522f7130ac124",
    ),
    "src/server/game/Custom/CustomAoELoot.cpp": (
        "9a88008a895eb3b70eddbf8fe81d7cbefcd0cc285eb18bf460bc06c086af95c5",
        "3492330facc3ea250be60e9496a60eab1df4d6a1a05e31179bd527713c8b1cda",
    ),
    "src/server/game/Custom/CustomAoELoot.h": (
        "4cb423d0f854406f6aadbb7421af3782a90c1a8262d892ee3935e535ec5af6c6",
        "11428566fc7c4a79ab20a65f639b1ec0d287a212a5fc639c27b6d040d8339e31",
    ),
}


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def function_body(source: str, name: str) -> str:
    start = source.index(name)
    brace = source.index("{", start)
    depth = 0
    for index in range(brace, len(source)):
        char = source[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[start : index + 1]
    raise AssertionError(f"unterminated function {name}")


def source_gates() -> None:
    for rel, (pre, post) in EXPECTED.items():
        assert digest(ORIGINAL / rel) == pre, rel
        assert digest(PAYLOAD / rel) == post, rel

    handler = (PAYLOAD / "src/server/game/Handlers/LootHandler.cpp").read_text(
        encoding="utf-8"
    )
    custom = (PAYLOAD / "src/server/game/Custom/CustomAoELoot.cpp").read_text(
        encoding="utf-8"
    )

    autostore = function_body(handler, "HandleAutostoreLootItemOpcode")
    money = function_body(handler, "HandleLootMoneyOpcode")
    open_loot = function_body(handler, "HandleLootOpcode")

    # Unified trigger: opening a legal creature loot window, not a successful item slot.
    assert "LootAllAround" not in autostore
    assert "GatherMoneyAround" not in money
    assert open_loot.count("GatherMoneyAround") == 1
    assert open_loot.count("LootAllAround") == 1
    assert open_loot.index("GatherMoneyAround") < open_loot.index("SendLoot(")
    assert open_loot.index("SendLoot(") < open_loot.index("LootAllAround")
    assert "player->GetLootGUID() == packet.Unit" in open_loot

    # Gold moves to origin, then the original distribution/event chain runs once.
    assert "aoeOrigin->loot.gold += gatheredGold" in open_loot
    assert "ModifyMoney" not in custom
    assert money.count("OnLootMoney") == 1
    assert money.count("ModifyMoney") >= 3  # NPCBot, human group, solo branches retained.

    # Per-slot roll/master protection and pure-NPCBot distinction.
    assert "ShouldSkipForRoll" not in custom
    assert "GetMembersCount() > 1" in custom
    assert "is_underthreshold" in custom
    assert "rollWinnerGUID" in custom
    assert "is_blocked" in custom
    assert "GetMaxSlotInLootFor" in custom
    assert "LootItemInSlot" in custom

    # Mail is capacity-only. Other inventory errors remain on the corpse.
    mail_branch = "else if (result == EQUIP_ERR_INV_FULL && MailEnabled())"
    assert custom.count(mail_branch) == 1
    assert "EQUIP_ERR_ITEM_MAX_COUNT" not in custom
    assert "else\n                    ++stats.inventoryErrors;" in custom

    # Required event and cleanup semantics are explicit.
    for token in (
        "ACHIEVEMENT_CRITERIA_TYPE_LOOT_ITEM",
        "ACHIEVEMENT_CRITERIA_TYPE_LOOT_TYPE",
        "ACHIEVEMENT_CRITERIA_TYPE_LOOT_EPIC_ITEM",
        "OnLootItem",
        "NotifyItemRemoved",
        "NotifyQuestItemRemoved",
        "RemoveStoredLootItemForContainer",
        "AllLootRemovedFromCorpse",
    ):
        assert token in custom, token

    # Diagnostics expose the causes that previously looked like random missing corpses.
    for token in (
        "discovered",
        "noDynamicFlag",
        "permissionDenied",
        "skinning",
        "rollSlots",
        "inventoryErrors",
        "limitHit",
        "itemsStored",
        "itemsMailed",
        "gatheredGold",
    ):
        assert token in custom, token


@dataclass
class ItemModel:
    quality: str = "low"       # low/high
    result: str = "ok"         # ok/full/max_count
    roll_blocked: bool = False
    looted: bool = False
    mailed: bool = False


@dataclass
class CorpseModel:
    items: list[ItemModel]
    gold: int = 0
    rights: bool = True
    lootable: bool = True
    skinning: bool = False


def model_open(corpses: list[CorpseModel], real_group: bool) -> tuple[int, int, int]:
    """Return stored items, mailed items, one-shot aggregated gold."""
    gathered = 0
    stored = 0
    mailed = 0

    for corpse in corpses:
        if not corpse.lootable or not corpse.rights or corpse.skinning:
            continue
        gathered += corpse.gold
        corpse.gold = 0

    for corpse in corpses:
        if not corpse.lootable or not corpse.rights or corpse.skinning:
            continue
        for item in corpse.items:
            if item.roll_blocked or (real_group and item.quality == "high"):
                continue
            if item.result == "ok":
                item.looted = True
                stored += 1
            elif item.result == "full":
                item.looted = True
                item.mailed = True
                mailed += 1
            elif item.result == "max_count":
                pass
            else:
                raise AssertionError(item.result)

    return stored, mailed, gathered


def behavior_gates() -> None:
    # Different creature/drop types and arbitrary discovery order cannot cause one low
    # corpse to disappear. A high slot protects only itself, not the low slot beside it.
    base = [CorpseModel([ItemModel()]) for _ in range(7)]
    base += [CorpseModel([ItemModel("high"), ItemModel("low")], gold=13)]
    base += [CorpseModel([], gold=17)]
    base += [CorpseModel([ItemModel()], skinning=True)]
    random.Random(45).shuffle(base)
    stored, mailed, gold = model_open(base, real_group=True)
    assert (stored, mailed, gold) == (8, 0, 30)
    high = [item for corpse in base for item in corpse.items if item.quality == "high"]
    assert len(high) == 1 and not high[0].looted

    # Pure NPCBot group has no real roll competitor; the same high slot is collected.
    pure_bot = [CorpseModel([ItemModel("high"), ItemModel("low")], gold=9)]
    assert model_open(pure_bot, real_group=False) == (2, 0, 9)

    # Capacity-only mail: full is mailed; unique/max-count remains on corpse.
    inventory = [
        CorpseModel([ItemModel(result="full"), ItemModel(result="max_count")])
    ]
    assert model_open(inventory, real_group=False) == (0, 1, 0)
    assert inventory[0].items[0].mailed and inventory[0].items[0].looted
    assert not inventory[0].items[1].mailed and not inventory[0].items[1].looted

    # Ten low corpses are complete regardless of mixed ordering; no item-slot event is
    # needed to start the model_open operation.
    ten = [CorpseModel([ItemModel()], gold=i) for i in range(10)]
    random.Random(20260822).shuffle(ten)
    assert model_open(ten, real_group=False) == (10, 0, sum(range(10)))


def installer_gate() -> None:
    for optimized in (False, True):
        command = [sys.executable]
        if optimized:
            command.append("-O")
        command.extend([str(BASE / "install_f45.py"), "selftest"])
        result = subprocess.run(command, text=True, capture_output=True)
        if result.returncode:
            print(result.stdout)
            print(result.stderr, file=sys.stderr)
        assert result.returncode == 0
        assert "F45_INSTALLER_SELFTEST=PASS" in result.stdout


def main() -> int:
    source_gates()
    behavior_gates()
    installer_gate()
    print("F45_SOURCE_GATES=PASS")
    print("F45_BEHAVIOR_MODEL=PASS")
    print("F45_SELFTEST=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
