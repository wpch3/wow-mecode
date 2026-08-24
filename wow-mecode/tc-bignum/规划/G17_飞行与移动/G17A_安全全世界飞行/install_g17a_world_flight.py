#!/usr/bin/env python3
r"""Hash-locked installer for G17-A safe old-world flight.

Only src/server/game/Spells/SpellInfo.cpp is modified. The installer requires
its sibling packaged full source at 源文件/SpellInfo.cpp, creates/validates a
.g17a.bak backup, preserves CRLF, and supports check/apply/rollback/self-test.
It never modifies DBC, MPQ, configuration, or any database.
"""

from __future__ import annotations

import argparse
import hashlib
import os
import sys
import tempfile
from pathlib import Path

SOURCE_RELATIVE = Path("src/server/game/Spells/SpellInfo.cpp")
PACKAGE_SOURCE_RELATIVE = Path("源文件/SpellInfo.cpp")
BACKUP_SUFFIX = ".g17a.bak"
TEMP_SUFFIX = ".g17a.tmp"
PRE_SHA256 = "78dbc810f8afa080275d7fabcb2bc75ff52652295a25677b447cbe5f3ca89753"
POST_SHA256 = "537e5c350baa5f4a90bd0ec38c6b6858360e287aeabd75ab54050b4432e50755"
PRE_SIZE = 180195
POST_SIZE = 182275
MARKER_BEGIN = "// G17-WORLD-FLIGHT-BEGIN"
MARKER_END = "// G17-WORLD-FLIGHT-END"

INCLUDE_OLD = '#include "SpellInfo.h"\n'
INCLUDE_NEW = '''#include "SpellInfo.h"\n#include "Config.h"\n#include "StringConvert.h"\n#include "Util.h"\n'''
HELPER_ANCHOR = "uint32 GetTargetFlagMask(SpellTargetObjectTypes objType)\n"
HELPER_BLOCK = '''// G17-WORLD-FLIGHT-BEGIN
namespace
{
bool G17IsBlockedArea(uint32 zoneId, uint32 areaId)
{
    std::string const blockedAreaIds = sConfigMgr->GetStringDefault("WorldFlight.BlockedAreaIds", "");
    for (std::string_view token : Trinity::Tokenize(blockedAreaIds, ',', false))
    {
        if (Optional<uint32> blockedAreaId = Trinity::StringTo<uint32>(token))
            if (*blockedAreaId == zoneId || *blockedAreaId == areaId)
                return true;
    }

    return false;
}

bool G17IsOldWorldFlightAllowed(uint32 mapId, uint32 zoneId, uint32 areaId, AreaTableEntry const* areaEntry)
{
    if (!sConfigMgr->GetBoolDefault("WorldFlight.Enable", false) ||
        !sConfigMgr->GetBoolDefault("WorldFlight.AllowOldWorld", true) || !areaEntry)
        return false;

    MapEntry const* mapEntry = sMapStore.LookupEntry(mapId);
    if (!mapEntry || !mapEntry->IsWorldMap() || (mapId != 0 && mapId != 1))
        return false;

    AreaTableEntry const* zoneEntry = sAreaTableStore.LookupEntry(zoneId);
    uint32 areaFlags = areaEntry->Flags | (zoneEntry ? zoneEntry->Flags : 0);

    // These are hard safety boundaries and cannot be disabled from configuration.
    if (areaFlags & (AREA_FLAG_NO_FLY_ZONE | AREA_FLAG_ARENA | AREA_FLAG_ARENA_INSTANCE))
        return false;

    if (sConfigMgr->GetBoolDefault("WorldFlight.BlockIndoorAreas", true) &&
        (areaFlags & AREA_FLAG_INSIDE))
        return false;

    if (sConfigMgr->GetBoolDefault("WorldFlight.BlockCapitalAreas", true) &&
        (areaFlags & (AREA_FLAG_SLAVE_CAPITAL | AREA_FLAG_SLAVE_CAPITAL2 | AREA_FLAG_CAPITAL | AREA_FLAG_CITY)))
        return false;

    return !G17IsBlockedArea(zoneId, areaId);
}
}
// G17-WORLD-FLIGHT-END

'''

LOCATION_OLD = '''    // continent limitation (virtual continent)
    if (HasAttribute(SPELL_ATTR4_CAST_ONLY_IN_OUTLAND))
    {
        if (strict)
        {
            AreaTableEntry const* areaEntry = sAreaTableStore.LookupEntry(area_id);
            if (!areaEntry)
                areaEntry = sAreaTableStore.LookupEntry(zone_id);

            if (!areaEntry || !areaEntry->IsFlyable() || !player->CanFlyInZone(map_id, zone_id, this))
                return SPELL_FAILED_INCORRECT_AREA;
        }
        else
        {
            uint32 const v_map = GetVirtualMapForMapAndZone(map_id, zone_id);
            MapEntry const* mapEntry = sMapStore.LookupEntry(v_map);
            if (!mapEntry || mapEntry->Expansion() < 1 || !mapEntry->IsContinent())
                return SPELL_FAILED_INCORRECT_AREA;
        }
    }
'''

LOCATION_NEW = '''    // continent limitation (virtual continent)
    if (HasAttribute(SPELL_ATTR4_CAST_ONLY_IN_OUTLAND))
    {
        AreaTableEntry const* areaEntry = sAreaTableStore.LookupEntry(area_id);
        if (!areaEntry)
            areaEntry = sAreaTableStore.LookupEntry(zone_id);

        bool const g17OldWorldAllowed = G17IsOldWorldFlightAllowed(map_id, zone_id, area_id, areaEntry);
        if (strict)
        {
            if (!areaEntry || (!areaEntry->IsFlyable() && !g17OldWorldAllowed) ||
                !player->CanFlyInZone(map_id, zone_id, this))
                return SPELL_FAILED_INCORRECT_AREA;
        }
        else
        {
            uint32 const v_map = GetVirtualMapForMapAndZone(map_id, zone_id);
            MapEntry const* mapEntry = sMapStore.LookupEntry(v_map);
            bool const originalContinentAllowed =
                mapEntry && mapEntry->Expansion() >= 1 && mapEntry->IsContinent();
            if (!originalContinentAllowed && !g17OldWorldAllowed)
                return SPELL_FAILED_INCORRECT_AREA;
        }
    }
'''


class InstallError(RuntimeError):
    pass


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def read_utf8_crlf(path: Path) -> tuple[bytes, str]:
    data = path.read_bytes()
    if data.startswith(b"\xef\xbb\xbf"):
        raise InstallError(f"unexpected UTF-8 BOM: {path}")
    if b"\r\n" not in data or b"\n" in data.replace(b"\r\n", b""):
        raise InstallError(f"expected pure CRLF line endings: {path}")
    try:
        text = data.decode("utf-8")
    except UnicodeDecodeError as exc:
        raise InstallError(f"expected UTF-8 source: {path}: {exc}") from exc
    return data, text.replace("\r\n", "\n")


def to_crlf(text: str) -> bytes:
    if "\r" in text:
        raise InstallError("internal text unexpectedly contains CR")
    return text.replace("\n", "\r\n").encode("utf-8")


def require_count(text: str, needle: str, expected: int, label: str) -> None:
    actual = text.count(needle)
    if actual != expected:
        raise InstallError(f"{label}: expected {expected}, found {actual}")


def patch_text(text: str) -> str:
    require_count(text, MARKER_BEGIN, 0, "preimage G17 marker")
    require_count(text, MARKER_END, 0, "preimage G17 end marker")
    require_count(text, INCLUDE_OLD, 1, "SpellInfo include anchor")
    require_count(text, '#include "Config.h"', 0, "pre-existing Config include")
    require_count(text, '#include "StringConvert.h"', 0, "pre-existing StringConvert include")
    require_count(text, '#include "Util.h"', 0, "pre-existing Util include")
    require_count(text, HELPER_ANCHOR, 1, "helper insertion anchor")
    require_count(text, LOCATION_OLD, 1, "original continent limitation block")

    result = text.replace(INCLUDE_OLD, INCLUDE_NEW, 1)
    result = result.replace(HELPER_ANCHOR, HELPER_BLOCK + HELPER_ANCHOR, 1)
    result = result.replace(LOCATION_OLD, LOCATION_NEW, 1)
    validate_post_text(result)
    return result


def unpatch_text(text: str) -> str:
    validate_post_text(text)
    require_count(text, INCLUDE_NEW, 1, "postimage include block")
    require_count(text, HELPER_BLOCK + HELPER_ANCHOR, 1, "postimage helper block")
    require_count(text, LOCATION_NEW, 1, "postimage continent block")
    result = text.replace(LOCATION_NEW, LOCATION_OLD, 1)
    result = result.replace(HELPER_BLOCK + HELPER_ANCHOR, HELPER_ANCHOR, 1)
    result = result.replace(INCLUDE_NEW, INCLUDE_OLD, 1)
    require_count(result, MARKER_BEGIN, 0, "rollback marker")
    require_count(result, LOCATION_OLD, 1, "rollback original block")
    return result


def validate_post_text(text: str) -> None:
    require_count(text, MARKER_BEGIN, 1, "G17 begin marker")
    require_count(text, MARKER_END, 1, "G17 end marker")
    require_count(text, '#include "Config.h"', 1, "Config include")
    require_count(text, '#include "StringConvert.h"', 1, "StringConvert include")
    require_count(text, '#include "Util.h"', 1, "Util include")
    require_count(text, "G17IsOldWorldFlightAllowed", 2, "G17 helper declaration/use")
    require_count(text, 'GetBoolDefault("WorldFlight.Enable", false)', 1, "default-off switch")
    require_count(text, "AREA_FLAG_NO_FLY_ZONE", 1, "hard NO_FLY boundary")
    require_count(text, "AREA_FLAG_ARENA_INSTANCE", 1, "hard arena boundary")
    require_count(text, "player->CanFlyInZone(map_id, zone_id, this)", 1, "original cold-weather path")
    require_count(text, LOCATION_NEW, 1, "new continent limitation block")
    if LOCATION_OLD in text:
        raise InstallError("old continent limitation block remained after patch")


def package_source_path() -> Path:
    return Path(__file__).resolve().parent / PACKAGE_SOURCE_RELATIVE


def verify_package_source() -> bytes:
    path = package_source_path()
    if not path.is_file():
        raise InstallError(f"missing packaged full source: {path}")
    data, text = read_utf8_crlf(path)
    if len(data) != POST_SIZE or sha256_bytes(data) != POST_SHA256:
        raise InstallError(
            f"packaged source hash/size mismatch: size={len(data)} sha256={sha256_bytes(data)}"
        )
    validate_post_text(text)
    return data


def target_paths(root: Path) -> tuple[Path, Path, Path]:
    source = root.resolve() / SOURCE_RELATIVE
    backup = source.with_name(source.name + BACKUP_SUFFIX)
    temp = source.with_name(source.name + TEMP_SUFFIX)
    return source, backup, temp


def inspect_source(source: Path) -> tuple[str, bytes, str]:
    if not source.is_file():
        raise InstallError(f"missing target source: {source}")
    data, text = read_utf8_crlf(source)
    digest = sha256_bytes(data)
    if digest == PRE_SHA256 and len(data) == PRE_SIZE:
        patch_text(text)
        return "ready", data, text
    if digest == POST_SHA256 and len(data) == POST_SIZE:
        validate_post_text(text)
        return "applied", data, text
    raise InstallError(
        f"target hash/size is not locked pre/post image: size={len(data)} sha256={digest} path={source}"
    )


def command_check(root: Path) -> None:
    package = verify_package_source()
    source, backup, _ = target_paths(root)
    state, data, _ = inspect_source(source)
    print(f"[OK] Package SpellInfo.cpp: size={len(package)} sha256={sha256_bytes(package)}")
    print(f"[OK] Target SpellInfo.cpp state={state}: size={len(data)} sha256={sha256_bytes(data)}")
    if backup.exists():
        backup_data = backup.read_bytes()
        print(f"[INFO] Backup exists: size={len(backup_data)} sha256={sha256_bytes(backup_data)} path={backup}")
    if state == "ready":
        print("[OK] CHECK_READY_TO_APPLY=True")
    else:
        print("[OK] CHECK_ALREADY_APPLIED=True")
    print("[OK] Source/config/database edits: 0")


def atomic_write(temp: Path, destination: Path, data: bytes) -> None:
    if temp.exists():
        raise InstallError(f"refusing to overwrite stale temp file: {temp}")
    try:
        with temp.open("xb") as handle:
            handle.write(data)
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temp, destination)
    finally:
        if temp.exists():
            temp.unlink()


def command_apply(root: Path) -> None:
    package = verify_package_source()
    source, backup, temp = target_paths(root)
    state, pre_data, pre_text = inspect_source(source)
    if state == "applied":
        print("[OK] G17-A is already applied exactly; no files changed.")
        print("[OK] APPLY_ALREADY_APPLIED=True")
        return

    generated = to_crlf(patch_text(pre_text))
    if generated != package:
        raise InstallError("generated postimage does not equal packaged full SpellInfo.cpp")

    if backup.exists():
        backup_data = backup.read_bytes()
        if len(backup_data) != PRE_SIZE or sha256_bytes(backup_data) != PRE_SHA256:
            raise InstallError(f"existing backup is not the locked preimage: {backup}")
        print(f"[OK] Reusing verified backup: {backup}")
    else:
        with backup.open("xb") as handle:
            handle.write(pre_data)
            handle.flush()
            os.fsync(handle.fileno())
        print(f"[OK] Created backup: {backup}")

    atomic_write(temp, source, generated)
    state_after, post_data, _ = inspect_source(source)
    if state_after != "applied":
        raise InstallError("post-write verification did not reach applied state")
    print(f"[OK] Applied G17-A SpellInfo.cpp: size={len(post_data)} sha256={sha256_bytes(post_data)}")
    print("[OK] APPLY_CHANGED_FILES=1")
    print("[INFO] DBC/MPQ/config/database edits=0")


def command_rollback(root: Path) -> None:
    source, backup, temp = target_paths(root)
    state, _, _ = inspect_source(source)
    if state == "ready":
        print("[OK] G17-A source is already rolled back exactly; no files changed.")
        print("[OK] ROLLBACK_ALREADY_DONE=True")
        return
    if not backup.is_file():
        raise InstallError(f"missing rollback backup: {backup}")
    backup_data, backup_text = read_utf8_crlf(backup)
    if len(backup_data) != PRE_SIZE or sha256_bytes(backup_data) != PRE_SHA256:
        raise InstallError(f"rollback backup hash/size mismatch: {backup}")
    patch_text(backup_text)  # verifies all exact preimage anchors before restore
    atomic_write(temp, source, backup_data)
    state_after, restored, _ = inspect_source(source)
    if state_after != "ready":
        raise InstallError("rollback verification did not reach ready state")
    print(f"[OK] Rolled back G17-A SpellInfo.cpp: size={len(restored)} sha256={sha256_bytes(restored)}")
    print("[OK] ROLLBACK_CHANGED_FILES=1")
    print(f"[INFO] Verified backup retained: {backup}")


def command_self_test() -> None:
    package = verify_package_source()
    _, package_text = read_utf8_crlf(package_source_path())
    reconstructed_pre = unpatch_text(package_text)
    pre_bytes = to_crlf(reconstructed_pre)
    if len(pre_bytes) != PRE_SIZE or sha256_bytes(pre_bytes) != PRE_SHA256:
        raise InstallError("reverse-generated preimage does not match locked user source")
    regenerated_post = to_crlf(patch_text(reconstructed_pre))
    if regenerated_post != package:
        raise InstallError("patch(unpatch(package)) did not reproduce package bytes")

    try:
        patch_text(reconstructed_pre.replace(INCLUDE_OLD, INCLUDE_OLD + INCLUDE_OLD, 1))
    except InstallError:
        pass
    else:
        raise InstallError("duplicate include anchor negative fixture false-passed")

    changed_location = reconstructed_pre.replace(
        "MapEntry const* mapEntry = sMapStore.LookupEntry(v_map);",
        "MapEntry const* mapEntry = nullptr; // synthetic conflict",
        1,
    )
    try:
        patch_text(changed_location)
    except InstallError:
        pass
    else:
        raise InstallError("changed location block negative fixture false-passed")

    with tempfile.TemporaryDirectory(prefix="g17a_installer_") as temp_dir:
        root = Path(temp_dir)
        source = root / SOURCE_RELATIVE
        source.parent.mkdir(parents=True, exist_ok=True)
        source.write_bytes(pre_bytes)
        command_apply(root)
        state, applied, _ = inspect_source(source)
        if state != "applied" or applied != package:
            raise InstallError("temporary apply fixture failed")
        command_apply(root)
        command_rollback(root)
        state, restored, _ = inspect_source(source)
        if state != "ready" or restored != pre_bytes:
            raise InstallError("temporary rollback fixture failed")

    print("[OK] Package full-source/preimage/postimage hashes and CRLF passed.")
    print("[OK] Exact apply/already-applied/rollback fixture passed.")
    print("[OK] Duplicate/changed-anchor negative fixtures did not false-pass.")
    print("[OK] G17-A installer self-test passed.")


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--self-test", action="store_true")
    group.add_argument("--check", metavar="ROOT", type=Path)
    group.add_argument("--apply", metavar="ROOT", type=Path)
    group.add_argument("--rollback", metavar="ROOT", type=Path)
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    if args.self_test:
        command_self_test()
    elif args.check is not None:
        command_check(args.check)
    elif args.apply is not None:
        command_apply(args.apply)
    elif args.rollback is not None:
        command_rollback(args.rollback)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"[FAILED] G17-A installer: {exc}", file=sys.stderr)
        raise SystemExit(2)
