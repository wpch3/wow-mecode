#!/usr/bin/env python3
from __future__ import annotations
import argparse, hashlib, os
from pathlib import Path
SOURCE_RELATIVE=Path('src/server/scripts/Commands/cs_dragonriding.cpp')
PRE_SHA256='10a7002db4c173e441836870da01dd3009a7ba470369ca1a89bdb399ee9b2f45'
POST_SHA256='2c7594d0f1428a767570063ac90c5f816991bf1d883fe61e45a1a28902a68199'
PAYLOAD=Path(__file__).resolve().parents[1]/'payload'/SOURCE_RELATIVE

def sha(path:Path)->str:return hashlib.sha256(path.read_bytes()).hexdigest()
def atomic_write(path:Path,data:bytes)->None:
    tmp=path.with_name(path.name+'.g17b1.tmp')
    if tmp.exists():raise RuntimeError(f'temporary file exists: {tmp}')
    tmp.write_bytes(data);os.replace(tmp,path)
def check(root:Path)->str:
    target=root/SOURCE_RELATIVE
    if not target.is_file():raise RuntimeError(f'target missing: {target}')
    value=sha(target)
    if value==PRE_SHA256:state='READY_PREIMAGE'
    elif value==POST_SHA256:state='ALREADY_APPLIED'
    else:raise RuntimeError(f'target SHA not recognized: {value}')
    print(f'G17B1_SOURCE_STATE={state}');print(f'TARGET_SHA256={value}');return state
def apply(root:Path)->None:
    state=check(root);target=root/SOURCE_RELATIVE
    if sha(PAYLOAD)!=POST_SHA256:raise RuntimeError('package payload SHA mismatch')
    if state=='ALREADY_APPLIED':print('G17B1_SOURCE_APPLY=ALREADY_CURRENT');return
    backup=target.with_name(target.name+'.g17b1.preimage')
    if backup.exists():
        if sha(backup)!=PRE_SHA256:raise RuntimeError('existing backup SHA mismatch')
    else:backup.write_bytes(target.read_bytes())
    atomic_write(target,PAYLOAD.read_bytes())
    if sha(target)!=POST_SHA256:raise RuntimeError('postimage SHA mismatch')
    print(f'BACKUP={backup}');print('G17B1_SOURCE_APPLY=PASS')
def rollback(root:Path)->None:
    target=root/SOURCE_RELATIVE;backup=target.with_name(target.name+'.g17b1.preimage')
    if not target.is_file() or sha(target)!=POST_SHA256:raise RuntimeError('target is not exact G17B1 postimage')
    if not backup.is_file() or sha(backup)!=PRE_SHA256:raise RuntimeError('exact G17B1 backup missing')
    atomic_write(target,backup.read_bytes())
    if sha(target)!=PRE_SHA256:raise RuntimeError('rollback SHA mismatch')
    backup.unlink();print('G17B1_SOURCE_ROLLBACK=PASS')
def main():
    ap=argparse.ArgumentParser();ap.add_argument('command',choices=['check','apply','rollback']);ap.add_argument('--source-root',required=True,type=Path);a=ap.parse_args()
    {'check':check,'apply':apply,'rollback':rollback}[a.command](a.source_root.resolve())
if __name__=='__main__':main()
