#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""G11 step 2 source-specific installer.

Targets the exact D:\TrinityCore files reported by probe_g11_step2.py on
2026-08-19. It patches four files, preserves encoding/newlines, creates one
backup per file, and refuses unknown source hashes.

Usage:
  py -3 install_g11_step2.py D:\TrinityCore --check
  py -3 install_g11_step2.py D:\TrinityCore --apply
  py -3 install_g11_step2.py D:\TrinityCore --rollback
  py -3 install_g11_step2.py --self-test
"""

from __future__ import annotations

import argparse
import hashlib
import shutil
import sys
from dataclasses import dataclass
from pathlib import Path


EXPECTED_SHA256 = {
    "src/server/game/AI/NpcBots/bot_ai.h":
        "69e338dd4c1afe8e43cef67ccd6838444b9de993d77281a230cd81f89fe97b2e",
    "src/server/game/AI/NpcBots/bot_ai.cpp":
        "fc006114e7ea20207044c053fb0cd7b87633a1b2452a69058d24bab36d544302",
    "src/server/game/AI/NpcBots/botconfig.h":
        "69c3eea543268ecff33d844e2a1f5408efddeab117cb5369c4e8edff681156fc",
    "src/server/game/AI/NpcBots/botconfig.cpp":
        "49bc1f21bbe1c9fbe43ec19bc7ae27839dcf831c4a74ba82be922615c62f4a71",
}

BACKUP_SUFFIX = ".g11_step2.bak"


@dataclass
class SourceFile:
    path: Path
    raw: bytes
    text: str
    encoding: str
    bom: bool
    newline: str


def digest_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def read_source(path: Path) -> SourceFile:
    raw = path.read_bytes()
    bom = raw.startswith(b"\xef\xbb\xbf")
    payload = raw[3:] if bom else raw
    try:
        text = payload.decode("utf-8")
        encoding = "utf-8"
    except UnicodeDecodeError:
        text = raw.decode("gb18030")
        encoding = "gb18030"
        bom = False
    newline = "\r\n" if "\r\n" in text else "\n"
    return SourceFile(path, raw, text, encoding, bom, newline)


def encode_source(source: SourceFile, text: str) -> bytes:
    data = text.encode(source.encoding)
    if source.bom:
        data = b"\xef\xbb\xbf" + data
    return data


def insert_after_unique_line(text: str, token: str, block: str, newline: str) -> str:
    lines = text.splitlines(keepends=True)
    matches = [i for i, line in enumerate(lines) if token in line]
    if len(matches) != 1:
        raise ValueError(f"anchor must match once: {token!r}; matches={len(matches)}")
    payload = newline.join(block.strip("\n").split("\n")) + newline
    lines.insert(matches[0] + 1, payload)
    return "".join(lines)


def insert_before_unique_line(text: str, token: str, block: str, newline: str) -> str:
    lines = text.splitlines(keepends=True)
    matches = [i for i, line in enumerate(lines) if token in line]
    if len(matches) != 1:
        raise ValueError(f"anchor must match once: {token!r}; matches={len(matches)}")
    payload = newline.join(block.strip("\n").split("\n")) + newline
    lines.insert(matches[0], payload)
    return "".join(lines)


def patch_bot_ai_h(text: str, nl: str) -> str:
    text = insert_after_unique_line(
        text,
        "uint8 GetCurrentScene() const;",
        "    void UpdateAutonomyPerception(uint32 diff); // G11 step 2: read-only nearby-player facts",
        nl,
    )
    text = insert_after_unique_line(
        text,
        "uint32 _careChatTimer;",
        """    uint32 _autonomyPerceptionTimer{};
    uint32 _autonomyPerceptionLogTimer{};
    ObjectGuid _lastPerceivedPlayerGuid;""",
        nl,
    )
    return text


def patch_bot_ai_cpp(text: str, nl: str) -> str:
    text = insert_after_unique_line(text, '#include "World.h"', '#include "WorldSession.h"', nl)
    text = insert_after_unique_line(
        text,
        "_careChatTimer    = urand(60000, 120000);",
        """    _autonomyPerceptionTimer = urand(BotCfg::GetAutonomyPerceptionIntervalMin(),
        BotCfg::GetAutonomyPerceptionIntervalMax());
    _autonomyPerceptionLogTimer = 0;
    _lastPerceivedPlayerGuid = ObjectGuid::Empty;""",
        nl,
    )

    method = r'''// G11 step 2: read-only nearby-player perception.
// This function never moves, speaks, fights, groups, changes bonds, or writes DB data.
void bot_ai::UpdateAutonomyPerception(uint32 diff)
{
    if (!BotCfg::IsAutonomyPerceptionEnabled() || !BotCfg::IsAutonomyPerceptionReadOnly())
        return;

    if (!IsWanderer() || !IAmFree() || !me->IsAlive() || !me->IsInWorld() || IsDuringTeleport())
        return;

    if (_autonomyPerceptionLogTimer > diff)
        _autonomyPerceptionLogTimer -= diff;
    else
        _autonomyPerceptionLogTimer = 0;

    if (_autonomyPerceptionTimer > diff)
    {
        _autonomyPerceptionTimer -= diff;
        return;
    }

    uint32 const intervalMin = BotCfg::GetAutonomyPerceptionIntervalMin();
    uint32 const intervalMax = BotCfg::GetAutonomyPerceptionIntervalMax();
    _autonomyPerceptionTimer = urand(intervalMin, intervalMax);

    std::list<Player*> players;
    float const range = BotCfg::GetAutonomyPerceptionRange();
    me->GetPlayerListInGrid(players, range, true);

    Player* nearest = nullptr;
    float nearestDistance = range + 1.0f;
    uint32 candidateCount = 0;

    for (Player* player : players)
    {
        if (!player || !player->IsInWorld() || !player->IsAlive() || player->IsBeingTeleported())
            continue;
        if (!me->IsInMap(player) || !me->InSamePhase(player))
            continue;

        float const distance = me->GetDistance(player);
        if (distance > range)
            continue;

        ++candidateCount;
        if (!nearest || distance < nearestDistance)
        {
            nearest = player;
            nearestDistance = distance;
        }
    }

    if (!nearest)
    {
        _lastPerceivedPlayerGuid.Clear();
        _autonomyPerceptionLogTimer = 0;
        return;
    }

    ObjectGuid const playerGuid = nearest->GetGUID();
    bool const changedPlayer = playerGuid != _lastPerceivedPlayerGuid;
    if (!changedPlayer && _autonomyPerceptionLogTimer != 0)
        return;

    WorldSession* session = nearest->GetSession();
    bool const isDisconnectedBot = !session || session->IsBotSession() || session->PlayerDisconnected();
    int32 const levelDelta = int32(nearest->GetLevel()) - int32(me->GetLevel());
    float const healthPct = nearest->GetHealthPct();
    bool const hasGroup = nearest->GetGroup() != nullptr;
    bool const helpCandidate = nearest->IsInCombat() && healthPct < 50.0f &&
        levelDelta >= -10 && levelDelta <= 10;

    BOT_LOG_INFO("npcbots",
        "[G11-PERCEPTION] bot={} name={} scene={} map={} zone={} player={} playerName={} "
        "kind={} dist={:.1f} levelDelta={} hp={:.1f} combat={} group={} candidates={} "
        "seen={} helpCandidate={} readonly=1",
        me->GetEntry(), me->GetName(), uint32(GetCurrentScene()), me->GetMapId(), me->GetZoneId(),
        playerGuid.GetCounter(), nearest->GetName(), isDisconnectedBot ? "playerbot" : "human",
        nearestDistance, levelDelta, healthPct, uint32(nearest->IsInCombat()), uint32(hasGroup),
        candidateCount, changedPlayer ? "first" : "again", uint32(helpCandidate));

    _lastPerceivedPlayerGuid = playerGuid;
    _autonomyPerceptionLogTimer = BotCfg::GetAutonomyPerceptionLogCooldown();
}
'''
    text = insert_before_unique_line(
        text,
        "// G19第3步：算出 bot 当前所处的场景类别",
        method,
        nl,
    )
    text = insert_before_unique_line(
        text,
        "    if (doHealth)",
        "    UpdateAutonomyPerception(diff);\n",
        nl,
    )
    return text


def patch_botconfig_h(text: str, nl: str) -> str:
    return insert_after_unique_line(
        text,
        "static uint32 GetCompanionCareMoneyGive();",
        """    static bool IsAutonomyPerceptionEnabled();
    static float GetAutonomyPerceptionRange();
    static uint32 GetAutonomyPerceptionIntervalMin();
    static uint32 GetAutonomyPerceptionIntervalMax();
    static uint32 GetAutonomyPerceptionLogCooldown();
    static bool IsAutonomyPerceptionReadOnly();""",
        nl,
    )


def patch_botconfig_cpp(text: str, nl: str) -> str:
    text = insert_after_unique_line(
        text,
        "static uint32 _companionCareMoneyGive;",
        """static bool _autonomyPerceptionEnable;
static float _autonomyPerceptionRange;
static uint32 _autonomyPerceptionIntervalMin;
static uint32 _autonomyPerceptionIntervalMax;
static uint32 _autonomyPerceptionLogCooldown;
static bool _autonomyPerceptionReadOnly;""",
        nl,
    )

    load_block = r'''        _autonomyPerceptionEnable = sConfigMgr->GetBoolDefault("NpcBot.Autonomy.Perception.Enable", false);

        float rawPerceptionRange = sConfigMgr->GetFloatDefault("NpcBot.Autonomy.Perception.Range", 60.0f);
        _autonomyPerceptionRange = rawPerceptionRange < 10.0f ? 10.0f :
            (rawPerceptionRange > 120.0f ? 120.0f : rawPerceptionRange);

        int32 rawPerceptionIntervalMin = sConfigMgr->GetIntDefault("NpcBot.Autonomy.Perception.IntervalMin", 4000);
        int32 rawPerceptionIntervalMax = sConfigMgr->GetIntDefault("NpcBot.Autonomy.Perception.IntervalMax", 8000);
        int32 rawPerceptionLogCooldown = sConfigMgr->GetIntDefault("NpcBot.Autonomy.Perception.LogCooldown", 15000);

        _autonomyPerceptionIntervalMin = uint32(rawPerceptionIntervalMin < 1000 ? 1000 :
            (rawPerceptionIntervalMin > 60000 ? 60000 : rawPerceptionIntervalMin));
        _autonomyPerceptionIntervalMax = uint32(rawPerceptionIntervalMax < 1000 ? 1000 :
            (rawPerceptionIntervalMax > 60000 ? 60000 : rawPerceptionIntervalMax));
        if (_autonomyPerceptionIntervalMax < _autonomyPerceptionIntervalMin)
            _autonomyPerceptionIntervalMax = _autonomyPerceptionIntervalMin;

        _autonomyPerceptionLogCooldown = uint32(rawPerceptionLogCooldown < 1000 ? 1000 :
            (rawPerceptionLogCooldown > 300000 ? 300000 : rawPerceptionLogCooldown));

        _autonomyPerceptionReadOnly = sConfigMgr->GetBoolDefault("NpcBot.Autonomy.Perception.ReadOnly", true);
        if (!_autonomyPerceptionReadOnly)
        {
            BOT_LOG_WARN("server.loading", "NpcBot.Autonomy.Perception.ReadOnly=0 is ignored in G11 step 2; forcing read-only mode");
            _autonomyPerceptionReadOnly = true;
        }
'''
    text = insert_after_unique_line(
        text,
        '"NpcBot.Companion.MoneyGive", 50000);',
        load_block,
        nl,
    )

    accessors = r'''bool BotCfg::IsAutonomyPerceptionEnabled()       { return _autonomyPerceptionEnable; }
float BotCfg::GetAutonomyPerceptionRange()        { return _autonomyPerceptionRange; }
uint32 BotCfg::GetAutonomyPerceptionIntervalMin() { return _autonomyPerceptionIntervalMin; }
uint32 BotCfg::GetAutonomyPerceptionIntervalMax() { return _autonomyPerceptionIntervalMax; }
uint32 BotCfg::GetAutonomyPerceptionLogCooldown() { return _autonomyPerceptionLogCooldown; }
bool BotCfg::IsAutonomyPerceptionReadOnly()       { return _autonomyPerceptionReadOnly; }
'''
    text = insert_after_unique_line(
        text,
        "uint32 BotCfg::GetCompanionCareMoneyGive()",
        accessors,
        nl,
    )
    return text


PATCHERS = {
    "src/server/game/AI/NpcBots/bot_ai.h": patch_bot_ai_h,
    "src/server/game/AI/NpcBots/bot_ai.cpp": patch_bot_ai_cpp,
    "src/server/game/AI/NpcBots/botconfig.h": patch_botconfig_h,
    "src/server/game/AI/NpcBots/botconfig.cpp": patch_botconfig_cpp,
}


def looks_installed(files: dict[str, SourceFile]) -> bool:
    return (
        files["src/server/game/AI/NpcBots/bot_ai.h"].text.count("UpdateAutonomyPerception") == 1
        and files["src/server/game/AI/NpcBots/bot_ai.cpp"].text.count("UpdateAutonomyPerception") == 2
        and files["src/server/game/AI/NpcBots/botconfig.h"].text.count("IsAutonomyPerceptionEnabled") == 1
        and files["src/server/game/AI/NpcBots/botconfig.cpp"].text.count("NpcBot.Autonomy.Perception.Enable") == 1
    )


def load_targets(root: Path) -> dict[str, SourceFile]:
    result: dict[str, SourceFile] = {}
    missing = []
    for rel in EXPECTED_SHA256:
        path = root / rel
        if not path.is_file():
            missing.append(str(path))
        else:
            result[rel] = read_source(path)
    if missing:
        raise FileNotFoundError("missing target files:\n  " + "\n  ".join(missing))
    return result


def verify_preinstall_hashes(files: dict[str, SourceFile]) -> None:
    failures = []
    for rel, source in files.items():
        actual = digest_bytes(source.raw)
        expected = EXPECTED_SHA256[rel]
        if actual != expected:
            failures.append(f"{rel}\n  expected={expected}\n  actual  ={actual}")
    if failures:
        raise ValueError(
            "source hash mismatch; refusing to guess or overwrite:\n" + "\n".join(failures)
        )


def build_outputs(files: dict[str, SourceFile]) -> dict[str, bytes]:
    outputs: dict[str, bytes] = {}
    for rel, source in files.items():
        patched = PATCHERS[rel](source.text, source.newline)
        outputs[rel] = encode_source(source, patched)

    decoded = {
        rel: data.decode("utf-8-sig") if files[rel].encoding == "utf-8" else data.decode("gb18030")
        for rel, data in outputs.items()
    }
    assert decoded["src/server/game/AI/NpcBots/bot_ai.h"].count("UpdateAutonomyPerception") == 1
    assert decoded["src/server/game/AI/NpcBots/bot_ai.cpp"].count("UpdateAutonomyPerception") == 2
    assert decoded["src/server/game/AI/NpcBots/bot_ai.cpp"].count("[G11-PERCEPTION]") == 1
    assert decoded["src/server/game/AI/NpcBots/botconfig.h"].count("IsAutonomyPerceptionEnabled") == 1
    assert decoded["src/server/game/AI/NpcBots/botconfig.cpp"].count("NpcBot.Autonomy.Perception.Enable") == 1
    for rel, text in decoded.items():
        if text.count("{") != text.count("}"):
            raise ValueError(f"brace count changed to an unbalanced state: {rel}")
    return outputs


def apply(root: Path) -> int:
    files = load_targets(root)
    if looks_installed(files):
        print("[OK] G11 step 2 already appears installed; no files changed.")
        return 0
    verify_preinstall_hashes(files)
    outputs = build_outputs(files)

    backups = {rel: files[rel].path.with_name(files[rel].path.name + BACKUP_SUFFIX) for rel in files}
    existing = [str(path) for path in backups.values() if path.exists()]
    if existing:
        raise FileExistsError("backup already exists; inspect or rollback first:\n  " + "\n  ".join(existing))

    written: list[str] = []
    try:
        for rel, source in files.items():
            shutil.copyfile(source.path, backups[rel])
        for rel, data in outputs.items():
            files[rel].path.write_bytes(data)
            written.append(rel)
    except Exception:
        for rel in written:
            shutil.copyfile(backups[rel], files[rel].path)
        raise

    print("[OK] G11 step 2 installed into four source files.")
    for rel in files:
        print(f"  {rel}\n    new_sha256={digest_bytes(outputs[rel])}\n    backup={backups[rel]}")
    print("[NEXT] Copy g11_perception.conf, compile RelWithDebInfo, then test logs.")
    return 0


def check(root: Path) -> int:
    files = load_targets(root)
    if looks_installed(files):
        print("[OK] G11 step 2 symbols are already present.")
        return 0
    verify_preinstall_hashes(files)
    build_outputs(files)
    print("[OK] Exact hashes and all patch anchors passed. --apply is safe for this source snapshot.")
    return 0


def rollback(root: Path) -> int:
    restored = 0
    for rel in EXPECTED_SHA256:
        path = root / rel
        backup = path.with_name(path.name + BACKUP_SUFFIX)
        if not backup.is_file():
            raise FileNotFoundError(f"missing rollback backup: {backup}")
        data = backup.read_bytes()
        if digest_bytes(data) != EXPECTED_SHA256[rel]:
            raise ValueError(f"backup hash mismatch: {backup}")
    for rel in EXPECTED_SHA256:
        path = root / rel
        backup = path.with_name(path.name + BACKUP_SUFFIX)
        shutil.copyfile(backup, path)
        backup.unlink()
        restored += 1
    print(f"[OK] Rolled back {restored} source files and removed G11 backup files.")
    return 0


def self_test() -> int:
    nl = "\n"
    fixtures = {
        "src/server/game/AI/NpcBots/bot_ai.h": """uint8 GetCurrentScene() const;\nuint32 _careChatTimer;\n""",
        "src/server/game/AI/NpcBots/bot_ai.cpp": """#include \"World.h\"\n_careChatTimer    = urand(60000, 120000);\n// G19第3步：算出 bot 当前所处的场景类别\n    if (doHealth)\n""",
        "src/server/game/AI/NpcBots/botconfig.h": """static uint32 GetCompanionCareMoneyGive();\n""",
        "src/server/game/AI/NpcBots/botconfig.cpp": """static uint32 _companionCareMoneyGive;\n_companionCareMoneyGive = sConfigMgr->GetIntDefault(\"NpcBot.Companion.MoneyGive\", 50000);\nuint32 BotCfg::GetCompanionCareMoneyGive() { return _companionCareMoneyGive; }\n""",
    }
    outputs = {rel: PATCHERS[rel](text, nl) for rel, text in fixtures.items()}
    assert outputs["src/server/game/AI/NpcBots/bot_ai.cpp"].count("UpdateAutonomyPerception") == 2
    assert "WorldSession.h" in outputs["src/server/game/AI/NpcBots/bot_ai.cpp"]
    assert outputs["src/server/game/AI/NpcBots/botconfig.cpp"].count("NpcBot.Autonomy.Perception.Enable") == 1
    print("[OK] Synthetic anchor and transformation self-test passed.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Install G11 step 2 into the exact probed source snapshot")
    parser.add_argument("root", nargs="?", help="TrinityCore source root, e.g. D:\\TrinityCore")
    modes = parser.add_mutually_exclusive_group(required=True)
    modes.add_argument("--check", action="store_true")
    modes.add_argument("--apply", action="store_true")
    modes.add_argument("--rollback", action="store_true")
    modes.add_argument("--self-test", action="store_true")
    args = parser.parse_args()

    if args.self_test:
        return self_test()
    if not args.root:
        parser.error("root is required unless --self-test is used")
    root = Path(args.root)
    if args.check:
        return check(root)
    if args.apply:
        return apply(root)
    return rollback(root)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"[FAILED] {exc}", file=sys.stderr)
        raise SystemExit(2)
