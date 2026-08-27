#!/usr/bin/env python3
"""G17-B3R3 source apply tool (DF-style rider skill panel).

Locked-lineage installer for cs_dragonriding.cpp, same contract as B3-R2:
  check   - report the target's lineage state (no writes)
  apply   - forensic-backup + atomic write of the B3R3 postimage
  rollback- atomic write of the safety-rollback image (B3R2d floor)

Lineage:
  PRE      175e5a12...  B3R2d (six-visible slots + fast landing)
  POST     29f3e554...  B3R3 dual-cast skill panel
  ROLLBACK 175e5a12...  undo B3R3 -> back to the deployed B3R2d build
All earlier G17 images remain valid upgrade sources (including a65b0ddc,
the r1c image, in case r1d was not installed yet).
"""
from __future__ import annotations
import argparse, hashlib, os
from pathlib import Path

SOURCE_RELATIVE = Path("src/server/scripts/Commands/cs_dragonriding.cpp")
PRE_SHA256 = "7cb417b3cec7c6d93002c35c96a17748583d412308ac019bf2830fd496afa936"   # B3R4c (user current)
POST_SHA256 = "1febdecb17d0dbcb17aa831c7a2a4e589f4f3fb8f6855ab41faf8a13ae7bdcc4" # B3R5 combat visuals
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
SAFE_ROLLBACK_SHA256 = "7cb417b3cec7c6d93002c35c96a17748583d412308ac019bf2830fd496afa936"
UPGRADEABLE_SHAS = (INTERMEDIATE_SHA256, INTERMEDIATE2_SHA256, INTERMEDIATE3_SHA256,
                    INTERMEDIATE4_SHA256, INTERMEDIATE5_SHA256, INTERMEDIATE6_SHA256,
                    INTERMEDIATE7_SHA256, INTERMEDIATE8_SHA256)
PACKAGE_ROOT = Path(__file__).resolve().parents[1]
PAYLOAD = PACKAGE_ROOT / "payload_src" / SOURCE_RELATIVE
SAFE_ROLLBACK = PACKAGE_ROOT / "rollback_safe_src" / SOURCE_RELATIVE

def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def atomic_write(path: Path, data: bytes) -> None:
    temporary = path.with_name(path.name + ".g17b3r5.tmp")
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
    states = {PRE_SHA256: "READY_B3R5_PREIMAGE", POST_SHA256: "B3R5_APPLIED",
              SAFE_ROLLBACK_SHA256: "B3R5_SAFE_ROLLBACK"}
    for h in UPGRADEABLE_SHAS:
        states[h] = "B3R5_INTERMEDIATE_UPGRADEABLE"
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
    print(f"G17B3R5_SOURCE_STATE={state}")
    print(f"TARGET_SHA256={digest}")
    return state

def apply(root: Path) -> None:
    state = check(root)
    target = root / SOURCE_RELATIVE
    if state == "B3R5_APPLIED":
        print("G17B3R5_SOURCE_APPLY=ALREADY_CURRENT")
        return
    backup = target.with_name(target.name + ".g17b3r5.preimage")
    if state in ("READY_B3R5_PREIMAGE", "B3R5_INTERMEDIATE_UPGRADEABLE") and not backup.exists():
        backup.write_bytes(target.read_bytes())
    atomic_write(target, PAYLOAD.read_bytes())
    if sha(target) != POST_SHA256:
        raise RuntimeError("postimage SHA mismatch")
    print(f"FORENSIC_BACKUP={backup}")
    print("G17B3R5_SOURCE_APPLY=PASS")

def rollback(root: Path) -> None:
    state = check(root)
    target = root / SOURCE_RELATIVE
    if state == "B3R5_SAFE_ROLLBACK":
        print("G17B3R5_SOURCE_ROLLBACK=ALREADY_B3R4C_FLOOR")
        return
    atomic_write(target, SAFE_ROLLBACK.read_bytes())
    if sha(target) != SAFE_ROLLBACK_SHA256:
        raise RuntimeError("safety rollback SHA mismatch")
    print("G17B3R5_SOURCE_ROLLBACK=PASS_B3R4C_FLOOR")

def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("command", choices=("check", "apply", "rollback"))
    ap.add_argument("--source-root", required=True, type=Path)
    args = ap.parse_args()
    {"check": check, "apply": apply, "rollback": rollback}[args.command](args.source_root.resolve())

if __name__ == "__main__":
    main()
