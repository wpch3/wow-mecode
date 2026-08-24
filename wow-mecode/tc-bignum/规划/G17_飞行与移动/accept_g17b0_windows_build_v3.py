#!/usr/bin/env python3
from __future__ import annotations

from datetime import datetime
from pathlib import Path
import hashlib
import re
import sys

EXPECTED_SIZE = 6396
EXPECTED_SHA256 = "58c408728f6a8a91a317abd26e10911a346b607e7adaf0cbb3880ec656a6952e"
EXPECTED_LOADER = "5502e5b4e22535957f3db81083530b048ec33f6852f4697fbe55795628cee5cc"
EXPECTED_PAYLOAD = "c9535dca3390ece6735e6ff6b7418ed99ff206628b5e8febd7b78b05cba999bd"
EXPECTED_BEFORE_EXE = "a43fcf10567db83ef8b96ed9b47567860fd786dd40f6aeb0841e6d643583cb80"
EXPECTED_AFTER_EXE = "59491e97426dc059e2f440b6ca17f28ffdc6eb296e3f8d04fc428b59099b5881"


def require(ok: bool, message: str) -> None:
    if not ok:
        raise AssertionError(message)


def unique_value(lines: list[str], key: str) -> str:
    prefix = key + "="
    values = [line[len(prefix):] for line in lines if line.startswith(prefix)]
    require(len(values) == 1, f"expected one {key}, got {len(values)}")
    return values[0]


def parse_utc(value: str) -> datetime:
    require(value.endswith("Z"), f"not UTC: {value}")
    return datetime.fromisoformat(value[:-1] + "+00:00")


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit(f"usage: {Path(sys.argv[0]).name} RESULT.txt")
    path = Path(sys.argv[1])
    data = path.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    require(len(data) == EXPECTED_SIZE, f"unexpected size: {len(data)}")
    require(digest == EXPECTED_SHA256, f"unexpected SHA-256: {digest}")
    lines = data.decode("utf-8-sig").splitlines()
    require(len(lines) == 122, f"unexpected logical line count: {len(lines)}")

    required_lines = (
        "G17B0_WINDOWS_BUILD_V3_NATIVE_SAFE_START",
        "CONFIGURATION=RelWithDebInfo", "PLATFORM=x64",
        f"LOADER_SHA256={EXPECTED_LOADER}", f"PAYLOAD_SHA256={EXPECTED_PAYLOAD}",
        "G17B0_BUILD_SOURCE_GATE=PASS",
        "NATIVE_SELFTEST|G17B0_NATIVE_STDOUT",
        "NATIVE_RUNNER_SELFTEST_EXIT=0", "G17B0_NATIVE_RUNNER_SELFTEST=PASS",
        "CMAKE_EXIT=0", "DRAGONRIDING_VCXPROJ_HITS=1",
        "G17B0_CMAKE_SOURCE_MEMBERSHIP=PASS",
        "MSBUILD_EXIT=0", "DRAGONRIDING_FRESH_OBJECTS=1",
        "G17B0_WINDOWS_BUILD_PASS=True", "G17B0_WINDOWS_BUILD_RESULT=PASS",
        "G17B0_WINDOWS_BUILD_COMPLETE", "STOP_DO_NOT_START_WORLDSERVER",
    )
    for line in required_lines:
        require(line in lines, f"missing marker: {line}")
    require(any(line.startswith("NATIVE_SELFTEST|G17B0_NATIVE_STDERR") for line in lines),
            "native stderr self-test output")
    require(any(line == "CMAKE|-- Configuring done (5.8s)" for line in lines), "CMake configured")
    require(any(line == "CMAKE|-- Generating done (2.3s)" for line in lines), "CMake generated")
    require(any(line.endswith("D:\\TC-Build\\src\\server\\scripts\\scripts.vcxproj") and
                line.startswith("VCXPROJ_HIT=") for line in lines), "vcxproj hit")
    require("MSBUILD|  cs_dragonriding.cpp" in lines, "dragonriding compile line")
    require("MSBUILD|  cs_script_loader.cpp" in lines, "loader compile line")
    require(any(line.endswith("scripts.vcxproj -> D:\\TC-Build\\src\\server\\scripts\\RelWithDebInfo\\scripts.lib")
                for line in lines), "scripts library link")
    require(any(line.endswith("worldserver.vcxproj -> D:\\TC-Build\\bin\\RelWithDebInfo\\worldserver.exe")
                for line in lines), "worldserver link")

    require(unique_value(lines, "BEFORE_EXE_SHA256") == EXPECTED_BEFORE_EXE, "before exe SHA")
    require(unique_value(lines, "AFTER_EXE_SHA256") == EXPECTED_AFTER_EXE, "after exe SHA")
    require(EXPECTED_BEFORE_EXE != EXPECTED_AFTER_EXE, "exe SHA must change")
    before_exe_size = int(unique_value(lines, "BEFORE_EXE_SIZE"))
    after_exe_size = int(unique_value(lines, "AFTER_EXE_SIZE"))
    before_pdb_size = int(unique_value(lines, "BEFORE_PDB_SIZE"))
    after_pdb_size = int(unique_value(lines, "AFTER_PDB_SIZE"))
    require(before_exe_size > 0 and after_exe_size > 0 and before_exe_size != after_exe_size,
            "exe size evidence")
    require(before_pdb_size > 0 and after_pdb_size > 0 and before_pdb_size != after_pdb_size,
            "pdb size evidence")
    require(parse_utc(unique_value(lines, "AFTER_EXE_UTC")) >
            parse_utc(unique_value(lines, "BEFORE_EXE_UTC")), "exe time advance")
    require(parse_utc(unique_value(lines, "AFTER_PDB_UTC")) >
            parse_utc(unique_value(lines, "BEFORE_PDB_UTC")), "pdb time advance")

    object_lines = [line for line in lines if line.startswith("DRAGONRIDING_OBJECT=")]
    require(len(object_lines) == 1, "one fresh object detail")
    match = re.fullmatch(r"DRAGONRIDING_OBJECT=(.+);size=(\d+);utc=(.+)", object_lines[0])
    require(bool(match), "fresh object format")
    require(match.group(1).endswith("\\cs_dragonriding.obj"), "fresh object path")
    require(int(match.group(2)) > 0, "fresh object size")
    require(parse_utc(match.group(3)) >= parse_utc(unique_value(lines, "G17B0_MSBUILD_START_UTC")),
            "fresh object time")

    warning_lines = [line for line in lines if "warning C4018" in line]
    require(len(warning_lines) == 1, f"expected one tracked C4018 warning, got {len(warning_lines)}")
    require("cs_dragonriding.cpp(119,40)" in warning_lines[0], "tracked warning location")
    require(not any(line.startswith("G17B0_WINDOWS_BUILD_ERROR=") for line in lines), "build error marker")
    require("G17B0_WINDOWS_BUILD_PASS=False" not in lines, "false marker")
    require("G17B0_WINDOWS_BUILD_RESULT=FAIL" not in lines, "fail marker")
    require(not any(re.search(r"(?:^|[| ])error C\d+", line, re.IGNORECASE) for line in lines),
            "compiler error")

    print(f"G17B0_WINDOWS_BUILD_V3_RESULT_SIZE={len(data)}")
    print(f"G17B0_WINDOWS_BUILD_V3_RESULT_SHA256={digest}")
    print("G17B0_WINDOWS_BUILD_V3_CMAKE_EXIT=0")
    print("G17B0_WINDOWS_BUILD_V3_VCXPROJ_HITS=1")
    print("G17B0_WINDOWS_BUILD_V3_MSBUILD_EXIT=0")
    print("G17B0_WINDOWS_BUILD_V3_FRESH_OBJECTS=1")
    print(f"G17B0_WINDOWS_BUILD_V3_AFTER_EXE_SHA256={EXPECTED_AFTER_EXE}")
    print("G17B0_WINDOWS_BUILD_V3_WARNING_C4018=1_TRACKED_NONFATAL")
    print("G17B0_WINDOWS_BUILD_V3_ACCEPTANCE=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
