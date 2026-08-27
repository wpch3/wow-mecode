#!/usr/bin/env python3
"""G17-B3R2 source apply tool (multi-page vehicle skill bar).

Locked-lineage installer for cs_dragonriding.cpp, identical contract to the
proven B3-R1 tool:
  check   - report the target's lineage state (no writes)
  apply   - forensic-backup + atomic write of the B3R2 postimage
  rollback- atomic write of the safety-rollback image (B3R1 floor)

Lineage:
  PRE      2ddf54a6...  B3R1 FIX6 (the user's current, deployed build)
  POST     224aab59...  B3R2 multi-page skill bar
  ROLLBACK 2ddf54a6...  undo B3R2 -> back to the deployed B3R1 build
All earlier G17 images remain valid upgrade sources so a machine stranded on
any historical state can upgrade directly.
"""
from __future__ import annotations
import argparse, hashlib, os
from pathlib import Path

SOURCE_RELATIVE = Path("src/server/scripts/Commands/cs_dragonriding.cpp")
PRE_SHA256 = "2ddf54a66395896244869318e4bcfd619d10afc884033c6aa88e7cb53d0e6963"   # B3R1 FIX6 (user's current build)
POST_SHA256 = "175e5a122765691448738c7db7a25b32535f1fc29d7781e297e10614d4173975" # B3R2d six-visible slots + fast landing
# all earlier lineage images remain valid upgrade sources
INTERMEDIATE_SHA256 = "98446106309b45371f138d9c7bc707ee608d9a3db347e13d61cfd68cc97810f9"  # B2R3 R3FIX5 floor
INTERMEDIATE2_SHA256 = "1a96b72eb28ffa2c0ac0d3e0c07e26c30f25bcd8525babd15efad02a041825d6" # B3R1 first attempt (MSVC errors)
INTERMEDIATE3_SHA256 = "ecd307b472cb2c49f68607a8b0afe5dcf5f87a7a8eb6f087a4717f4cd8fa1bbb" # B3R1 FIX4 (5 MSVC errors)
# B3R2c: the r1b postimage (feb3dad4) is the user's CURRENT source state
# (r1b run: source apply + DBC + SQL all PASS, MSBuild failed on 5 errors).
INTERMEDIATE4_SHA256 = "feb3dad467188052c7b189478cea7060b14f8e13eb5bd7082d9f81b4ca3ab9ce"
# B3R2d: the r1c postimage (a65b0ddc) is the user's CURRENT source state
# (r1c run: everything PASS; this batch re-layouts pages for the 6-button
# client bar cap and speeds up landings).
INTERMEDIATE5_SHA256 = "a65b0ddcd06a66cfbdf04a91cd4114295615f9ee0c014f92bd742cb6c245b24d"
SAFE_ROLLBACK_SHA256 = "2ddf54a66395896244869318e4bcfd619d10afc884033c6aa88e7cb53d0e6963"
UPGRADEABLE_SHAS = (INTERMEDIATE_SHA256, INTERMEDIATE2_SHA256, INTERMEDIATE3_SHA256,
                    INTERMEDIATE4_SHA256, INTERMEDIATE5_SHA256)
PACKAGE_ROOT = Path(__file__).resolve().parents[1]
PAYLOAD = PACKAGE_ROOT / "payload_src" / SOURCE_RELATIVE
SAFE_ROLLBACK = PACKAGE_ROOT / "rollback_safe_src" / SOURCE_RELATIVE

def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def atomic_write(path: Path, data: bytes) -> None:
    temporary = path.with_name(path.name + ".g17b3r2.tmp")
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
    states = {PRE_SHA256: "READY_B3R2_PREIMAGE", POST_SHA256: "B3R2_APPLIED",
              SAFE_ROLLBACK_SHA256: "B3R2_SAFE_ROLLBACK"}
    for h in UPGRADEABLE_SHAS:
        states[h] = "B3R2_INTERMEDIATE_UPGRADEABLE"
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
    print(f"G17B3R2_SOURCE_STATE={state}")
    print(f"TARGET_SHA256={digest}")
    return state

def apply(root: Path) -> None:
    state = check(root)
    target = root / SOURCE_RELATIVE
    if state == "B3R2_APPLIED":
        print("G17B3R2_SOURCE_APPLY=ALREADY_CURRENT")
        return
    backup = target.with_name(target.name + ".g17b3r2.preimage")
    if state in ("READY_B3R2_PREIMAGE", "B3R2_INTERMEDIATE_UPGRADEABLE") and not backup.exists():
        backup.write_bytes(target.read_bytes())
    atomic_write(target, PAYLOAD.read_bytes())
    if sha(target) != POST_SHA256:
        raise RuntimeError("postimage SHA mismatch")
    print(f"FORENSIC_BACKUP={backup}")
    print("G17B3R2_SOURCE_APPLY=PASS")

def rollback(root: Path) -> None:
    state = check(root)
    target = root / SOURCE_RELATIVE
    if state == "B3R2_SAFE_ROLLBACK":
        print("G17B3R2_SOURCE_ROLLBACK=ALREADY_B3R1_FLOOR")
        return
    atomic_write(target, SAFE_ROLLBACK.read_bytes())
    if sha(target) != SAFE_ROLLBACK_SHA256:
        raise RuntimeError("safety rollback SHA mismatch")
    print("G17B3R2_SOURCE_ROLLBACK=PASS_B3R1_FLOOR")

def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("command", choices=("check", "apply", "rollback"))
    ap.add_argument("--source-root", required=True, type=Path)
    args = ap.parse_args()
    {"check": check, "apply": apply, "rollback": rollback}[args.command](args.source_root.resolve())

if __name__ == "__main__":
    main()
