#!/usr/bin/env python3
"""Exact-hash installer for G17-R3 live indoor server hardening."""
from __future__ import annotations
import argparse, hashlib, os, shutil, sys, tempfile
from pathlib import Path

REL = Path("src/server/game/Spells/SpellInfo.cpp")
PRE = "73d52ac0feb67a32822fc0bf086a9174ba7ef0bc186223cdc8a690f48fccb9e2"
POST = "c3ec2237ed6da8831662a8b7a5d45cf88f8efc7798cdd35c52a07700fa9cbcbf"
BACKUP_SUFFIX = ".g17r3_before_20260823.bak"

class InstallError(RuntimeError): pass

def sha(data: bytes) -> str: return hashlib.sha256(data).hexdigest()
def root() -> Path: return Path(__file__).resolve().parents[1]
def original() -> Path: return root() / "original" / REL
def payload() -> Path: return root() / "payload" / REL

def verify_package() -> tuple[bytes, bytes]:
    old, new = original().read_bytes(), payload().read_bytes()
    if sha(old) != PRE or sha(new) != POST or old == new: raise InstallError("packaged source hashes mismatch")
    required = {
        b"Player const* player)": 1,
        b"!areaEntry || !player": 2,
        b"!player->IsOutdoors()": 1,
        b"G17-R3: old-world AreaTable rows do not reliably carry AREA_FLAG_INSIDE": 1,
        b"areaEntry, player);": 1,
        b"G17R2 old-world pure-flight location allowed": 1,
    }
    for marker, count in required.items():
        if new.count(marker) != count: raise InstallError(f"payload marker mismatch: {marker!r}")
    return old, new

def paths(source_root: Path) -> tuple[Path, Path]:
    target = source_root.resolve() / REL
    return target, target.with_name(target.name + BACKUP_SUFFIX)

def atomic(path: Path, data: bytes) -> None:
    fd, name = tempfile.mkstemp(prefix=path.name+".", suffix=".tmp", dir=path.parent); temp=Path(name)
    try:
        with os.fdopen(fd,"wb") as f: f.write(data); f.flush(); os.fsync(f.fileno())
        os.replace(temp,path)
    except Exception:
        temp.unlink(missing_ok=True); raise

def check(source_root: Path) -> None:
    verify_package(); target, backup=paths(source_root)
    if not target.is_file(): raise InstallError(f"target missing: {target}")
    h=sha(target.read_bytes()); print(f"TARGET={target}");print(f"TARGET_SHA256={h}");print(f"BACKUP={backup}")
    if h==PRE: print("G17R3_SERVER_SOURCE_STATE=PREIMAGE")
    elif h==POST: print("G17R3_SERVER_SOURCE_STATE=POSTIMAGE")
    else: raise InstallError("target is neither R2 preimage nor R3 postimage")
    print("G17R3_SERVER_SOURCE_CHECK=PASS")

def apply(source_root: Path) -> None:
    old,new=verify_package(); target,backup=paths(source_root)
    if not target.is_file(): raise InstallError(f"target missing: {target}")
    current=target.read_bytes(); h=sha(current); print(f"PRE_APPLY_SHA256={h}")
    if h==POST:
        if backup.exists() and sha(backup.read_bytes())!=PRE: raise InstallError("postimage installed but backup hash is unexpected")
        print("G17R3_SERVER_SOURCE_APPLY=ALREADY_CURRENT");print(f"POST_APPLY_SHA256={POST}");return
    if h!=PRE or current!=old: raise InstallError("target is not locked R2 preimage; refusing overwrite")
    if backup.exists():
        if sha(backup.read_bytes())!=PRE: raise InstallError("refusing to overwrite different backup")
    else:
        shutil.copy2(target,backup)
        if sha(backup.read_bytes())!=PRE: raise InstallError("backup verification failed")
    atomic(target,new)
    if sha(target.read_bytes())!=POST: atomic(target,old); raise InstallError("post-write verification failed; original restored")
    print(f"BACKUP={backup}");print(f"BACKUP_SHA256={PRE}");print(f"POST_APPLY_SHA256={POST}");print("G17R3_SERVER_SOURCE_APPLY=PASS")

def rollback(source_root: Path) -> None:
    old,_=verify_package();target,backup=paths(source_root); h=sha(target.read_bytes())
    if h==PRE: print("G17R3_SERVER_SOURCE_ROLLBACK=ALREADY_PREIMAGE");return
    if h!=POST: raise InstallError("target is not locked R3 postimage")
    if not backup.is_file() or sha(backup.read_bytes())!=PRE: raise InstallError("verified backup missing")
    atomic(target,old)
    if sha(target.read_bytes())!=PRE: raise InstallError("rollback verification failed")
    print("G17R3_SERVER_SOURCE_ROLLBACK=PASS")

def main() -> int:
    p=argparse.ArgumentParser();p.add_argument("mode",choices=("check","apply","rollback"));p.add_argument("--source-root",type=Path,default=Path(r"D:\TrinityCore"));a=p.parse_args()
    try:
        {"check":check,"apply":apply,"rollback":rollback}[a.mode](a.source_root);return 0
    except (InstallError,OSError) as e:
        print(f"G17R3_SERVER_SOURCE_ERROR={e}",file=sys.stderr);return 1
if __name__=="__main__": raise SystemExit(main())
