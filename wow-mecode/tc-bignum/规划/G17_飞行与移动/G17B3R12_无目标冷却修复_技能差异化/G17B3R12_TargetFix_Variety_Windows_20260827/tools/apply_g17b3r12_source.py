#!/usr/bin/env python3
"""G17-B3R12 source apply tool: no-target hidden-cooldown fix + skill variety #1/#2.

Locked-lineage installer for cs_dragonriding.cpp, same contract as B3R6-R11:
  check    - report the target's lineage state (no writes)
  apply    - forensic-backup + atomic write of the B3R12 postimage
  rollback - atomic write of the safety-rollback image (B3R6 floor)

Lineage:
  PRE      c5c4c332...  B3R11 r1a (user current)
  POST     3d501d9b...  B3R12 target pre-validation + swoop strike + wind stance
  ROLLBACK 3fdb46e8...  undo everything -> back to the deployed B3R6 build
"""
from __future__ import annotations
import argparse, hashlib, os
from pathlib import Path

SOURCE_RELATIVE = Path("src/server/scripts/Commands/cs_dragonriding.cpp")
PRE_SHA256 = "c5c4c332ad8d06b9841a29e546ec6581a253f96bbee6d2a8b4892a4d6cbf92a9"   # B3R11 r1a (user current)
POST_SHA256 = "3d501d9bb4b0ca2cb7f553877d6d32f85217e85723caf4602e5ad2863c74bcca" # B3R12 target fix + variety
# all earlier lineage images remain valid upgrade sources
INTERMEDIATE_SHA256 = "98446106309b45371f138d9c7bc707ee608d9a3db347e13d61cfd68cc97810f9"  # B2R3 floor
INTERMEDIATE2_SHA256 = "1a96b72eb28ffa2c0ac0d3e0c07e26c30f25bcd8525babd15efad02a041825d6" # B3R1 first attempt
INTERMEDIATE3_SHA256 = "ecd307b472cb2c49f68607a8b0afe5dcf5f87a7a8eb6f087a4717f4cd8fa1bbb" # B3R1 FIX4
INTERMEDIATE4_SHA256 = "feb3dad467188052c7b189478cea7060b14f8e13eb5bd7082d9f81b4ca3ab9ce" # B3R2 r1b
INTERMEDIATE5_SHA256 = "a65b0ddcd06a66cfbdf04a91cd4114295615f9ee0c014f92bd742cb6c245b24d" # B3R2 r1c
INTERMEDIATE6_SHA256 = "175e5a122765691448738c7db7a25b32535f1fc29d7781e297e10614d4173975" # B3R2 r1d (if B3R3 not yet installed)
# B3R4b: the r1 postimage (f49fd955) is the user's CURRENT source state
# (r1 run: source+DBC+addon all PASS, MSBuild failed on getLevel).
INTERMEDIATE7_SHA256 = "f49fd955ec27f2336bfcc6ed8e84f995abaf1d98a1136cf1eb0daefecf563a14"
INTERMEDIATE8_SHA256 = "29f3e55470f3ceaab79c8c5a6145ece76a8743c99999adec505a446239c32b3a" # B3R3
INTERMEDIATE9_SHA256 = "cd05b8369b42d1176ff674c5eeb1fe49c2f57ebc0e7229034d660b00eabe7d1f" # B3R6 r1 (failed compile, user's current)
INTERMEDIATE10_SHA256 = "ddcaa119650510e4b4699ff3a96a6369601e6deb44bd8f6d2ab3683164455c42"
INTERMEDIATE11_SHA256 = "726d403254b6328b05ebdbaf594e8946b6d21e920c5597d568a42f8d7bc339df" # B3R6 r1b
INTERMEDIATE12_SHA256 = "3fdb46e89a03a521d641b285a15339619a43b68c6399938121fb24683dfd306b" # B3R6 final
INTERMEDIATE13_SHA256 = "f2360d7e1be3ccea66f8bd499b19d4a7cc04acadee804dedbf8bf6774e3ca38c" # B3R7
INTERMEDIATE14_SHA256 = "dcfa78dd92ac4491882b9e2ec5c18a8b0803d6fcbb01cbe553e7fc069ac0f487" # B3R8
INTERMEDIATE15_SHA256 = "f0564c5ad225a67f0e49477d31d6939d32b490e6d69b8218f122bc7dce5560c3" # B3R9
INTERMEDIATE16_SHA256 = "199cff4a8073f60634ae30b3a4c5fed9a6f90d7fab60fe4ece4d6d263a8fe3fe" # B3R10
INTERMEDIATE17_SHA256 = "520696eedc555108b2afbba6d232d5a8f67c46c7306461b7d4aeca83342a3029" # B3R11 r1
INTERMEDIATE18_SHA256 = "c5c4c332ad8d06b9841a29e546ec6581a253f96bbee6d2a8b4892a4d6cbf92a9" # B3R11 r1a (user current)
SAFE_ROLLBACK_SHA256 = "3fdb46e89a03a521d641b285a15339619a43b68c6399938121fb24683dfd306b"  # B3R6 floor
UPGRADEABLE_SHAS = (INTERMEDIATE_SHA256, INTERMEDIATE2_SHA256, INTERMEDIATE3_SHA256,
                    INTERMEDIATE4_SHA256, INTERMEDIATE5_SHA256, INTERMEDIATE6_SHA256,
                    INTERMEDIATE7_SHA256, INTERMEDIATE8_SHA256, INTERMEDIATE9_SHA256,
                    INTERMEDIATE10_SHA256, INTERMEDIATE11_SHA256, INTERMEDIATE12_SHA256,
                    INTERMEDIATE13_SHA256, INTERMEDIATE14_SHA256, INTERMEDIATE15_SHA256,
                    INTERMEDIATE16_SHA256, INTERMEDIATE17_SHA256, INTERMEDIATE18_SHA256)
PACKAGE_ROOT = Path(__file__).resolve().parents[1]
PAYLOAD = PACKAGE_ROOT / "payload_src" / SOURCE_RELATIVE
SAFE_ROLLBACK = PACKAGE_ROOT / "rollback_safe_src" / SOURCE_RELATIVE

def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def atomic_write(path: Path, data: bytes) -> None:
    temporary = path.with_name(path.name + ".g17b3r6.tmp")
    if temporary.exists():
        raise RuntimeError(f"temporary file exists: {temporary}")
    temporary.write_bytes(data)
    os.replace(temporary, path)

def verify_package() -> None:
    if sha(PAYLOAD) != POST_SHA256:
        raise RuntimeError("package payload SHA mismatch")
    if sha(SAFE_ROLLBACK) != SAFE_ROLLBACK_SHA256:
        raise RuntimeError("package safety-rollback SHA mismatch")

def state_for_digest(digest: str) -> str:
    states = {PRE_SHA256: "READY_B3R6_PREIMAGE", POST_SHA256: "B3R6_APPLIED",
              SAFE_ROLLBACK_SHA256: "B3R6_SAFE_ROLLBACK"}
    for h in UPGRADEABLE_SHAS:
        states[h] = "B3R6_INTERMEDIATE_UPGRADEABLE"
    return states.get(digest, "")

def check(root: Path) -> str:
    verify_package()
    target = root / SOURCE_RELATIVE
    if not target.is_file():
        raise RuntimeError(f"target missing: {target}")
    digest = sha(target)
    state = state_for_digest(digest)
    if not state:
        raise RuntimeError(f"target SHA not recognized: {digest}")
    print(f"G17B3R6_SOURCE_STATE={state}")
    print(f"TARGET_SHA256={digest}")
    return state

def apply(root: Path) -> None:
    state = check(root)
    target = root / SOURCE_RELATIVE
    if state == "B3R6_APPLIED":
        print("G17B3R6_SOURCE_APPLY=ALREADY_CURRENT")
        return
    backup = target.with_name(target.name + ".g17b3r6.preimage")
    if state in ("READY_B3R6_PREIMAGE", "B3R6_INTERMEDIATE_UPGRADEABLE") and not backup.exists():
        backup.write_bytes(target.read_bytes())
    atomic_write(target, PAYLOAD.read_bytes())
    if sha(target) != POST_SHA256:
        raise RuntimeError("postimage SHA mismatch")
    print(f"FORENSIC_BACKUP={backup}")
    print("G17B3R6_SOURCE_APPLY=PASS")

def rollback(root: Path) -> None:
    state = check(root)
    target = root / SOURCE_RELATIVE
    if state == "B3R6_SAFE_ROLLBACK":
        print("G17B3R6_SOURCE_ROLLBACK=ALREADY_B3R5_FLOOR")
        return
    atomic_write(target, SAFE_ROLLBACK.read_bytes())
    if sha(target) != SAFE_ROLLBACK_SHA256:
        raise RuntimeError("safety rollback SHA mismatch")
    print("G17B3R6_SOURCE_ROLLBACK=PASS_B3R5_FLOOR")

def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("command", choices=("check", "apply", "rollback"))
    ap.add_argument("--source-root", required=True, type=Path)
    args = ap.parse_args()
    {"check": check, "apply": apply, "rollback": rollback}[args.command](args.source_root.resolve())

if __name__ == "__main__":
    main()
