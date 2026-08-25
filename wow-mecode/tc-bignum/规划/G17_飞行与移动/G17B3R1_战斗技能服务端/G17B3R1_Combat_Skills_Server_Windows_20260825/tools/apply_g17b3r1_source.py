#!/usr/bin/env python3
from __future__ import annotations
import argparse, hashlib, os
from pathlib import Path

SOURCE_RELATIVE = Path("src/server/scripts/Commands/cs_dragonriding.cpp")
PRE_SHA256 = "98446106309b45371f138d9c7bc707ee608d9a3db347e13d61cfd68cc97810f9"   # B2R3 R3FIX5 (user's current build)
POST_SHA256 = "1a96b72eb28ffa2c0ac0d3e0c07e26c30f25bcd8525babd15efad02a041825d6" # B3R1
# all earlier lineage images remain valid upgrade sources
INTERMEDIATE_SHA256 = "3b92e815dc81ade4aa9927c19716dabddb8e8f93a6d0aff8b32c80dfbcbfc7f1"
INTERMEDIATE2_SHA256 = "ff185d9987b8f4457d8380e1c662cd0313b33a7ae4be6b82974e7702d1fdc4fc"
INTERMEDIATE3_SHA256 = "3e4590da5d8864f8447cd3b55acf05c249855927a33e0e792dd426f03426237a"
INTERMEDIATE4_SHA256 = "613420676babe4c71c570c24a0f5d94976623516e0519b4553b3d5962056bafe"
INTERMEDIATE5_SHA256 = "03dd649ded01dcd1917b1d0e98689ae1dbfe4289f6fc2548a3a62d616e6a0844"
INTERMEDIATE6_SHA256 = "adedfc58344a104ccc96ff28155b504727f50e0026d842345721610c6a32a59f"
SAFE_ROLLBACK_SHA256 = "98446106309b45371f138d9c7bc707ee608d9a3db347e13d61cfd68cc97810f9"
UPGRADEABLE_SHAS = (INTERMEDIATE_SHA256, INTERMEDIATE2_SHA256, INTERMEDIATE3_SHA256,
                    INTERMEDIATE4_SHA256, INTERMEDIATE5_SHA256, INTERMEDIATE6_SHA256)
PACKAGE_ROOT = Path(__file__).resolve().parents[1]
PAYLOAD = PACKAGE_ROOT / "payload_src" / SOURCE_RELATIVE
SAFE_ROLLBACK = PACKAGE_ROOT / "rollback_safe_src" / SOURCE_RELATIVE

def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def atomic_write(path: Path, data: bytes) -> None:
    temporary = path.with_name(path.name + ".g17b3r1.tmp")
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
    states = {PRE_SHA256: "READY_B3R1_PREIMAGE", POST_SHA256: "B3R1_APPLIED",
              SAFE_ROLLBACK_SHA256: "B3R1_SAFE_ROLLBACK"}
    for h in UPGRADEABLE_SHAS:
        states[h] = "B3R1_INTERMEDIATE_UPGRADEABLE"
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
    print(f"G17B3R1_SOURCE_STATE={state}")
    print(f"TARGET_SHA256={digest}")
    return state

def apply(root: Path) -> None:
    state = check(root)
    target = root / SOURCE_RELATIVE
    if state == "B3R1_APPLIED":
        print("G17B3R1_SOURCE_APPLY=ALREADY_CURRENT")
        return
    backup = target.with_name(target.name + ".g17b3r1.preimage")
    if state in ("READY_B3R1_PREIMAGE", "B3R1_INTERMEDIATE_UPGRADEABLE") and not backup.exists():
        backup.write_bytes(target.read_bytes())
    atomic_write(target, PAYLOAD.read_bytes())
    if sha(target) != POST_SHA256:
        raise RuntimeError("postimage SHA mismatch")
    print(f"FORENSIC_BACKUP={backup}")
    print("G17B3R1_SOURCE_APPLY=PASS")

def rollback(root: Path) -> None:
    state = check(root)
    target = root / SOURCE_RELATIVE
    if state == "B3R1_SAFE_ROLLBACK":
        print("G17B3R1_SOURCE_ROLLBACK=ALREADY_B3R1_PREIMAGE")
        return
    atomic_write(target, SAFE_ROLLBACK.read_bytes())
    if sha(target) != SAFE_ROLLBACK_SHA256:
        raise RuntimeError("safety rollback SHA mismatch")
    print("G17B3R1_SOURCE_ROLLBACK=PASS_B2R3_FLOOR")

def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("command", choices=("check", "apply", "rollback"))
    ap.add_argument("--source-root", required=True, type=Path)
    args = ap.parse_args()
    {"check": check, "apply": apply, "rollback": rollback}[args.command](args.source_root.resolve())

if __name__ == "__main__":
    main()
