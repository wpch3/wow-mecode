#!/usr/bin/env python3
from __future__ import annotations
import argparse,hashlib,os
from pathlib import Path
SOURCE_RELATIVE=Path('src/server/scripts/Commands/cs_dragonriding.cpp')
PRE_SHA256='2c7594d0f1428a767570063ac90c5f816991bf1d883fe61e45a1a28902a68199'
POST_SHA256='94ff80334783e8883f0811a1a7f76595d91b729cc43684f00273abb9d955628b'
PAYLOAD=Path(__file__).resolve().parents[1]/'payload'/SOURCE_RELATIVE
def sha(p:Path)->str:return hashlib.sha256(p.read_bytes()).hexdigest()
def atomic_write(p:Path,b:bytes)->None:
 t=p.with_name(p.name+'.g17b1r3.tmp')
 if t.exists():raise RuntimeError(f'temporary file exists: {t}')
 t.write_bytes(b);os.replace(t,p)
def check(root:Path)->str:
 t=root/SOURCE_RELATIVE
 if not t.is_file():raise RuntimeError(f'target missing: {t}')
 h=sha(t)
 if h==PRE_SHA256:s='READY_PREIMAGE'
 elif h==POST_SHA256:s='ALREADY_APPLIED'
 else:raise RuntimeError(f'target SHA not recognized: {h}')
 print(f'G17B1R3_SOURCE_STATE={s}');print(f'TARGET_SHA256={h}');return s
def apply(root:Path)->None:
 state=check(root);t=root/SOURCE_RELATIVE
 if sha(PAYLOAD)!=POST_SHA256:raise RuntimeError('package payload SHA mismatch')
 if state=='ALREADY_APPLIED':print('G17B1R3_SOURCE_APPLY=ALREADY_CURRENT');return
 bak=t.with_name(t.name+'.g17b1r3.preimage')
 if bak.exists():
  if sha(bak)!=PRE_SHA256:raise RuntimeError('existing backup SHA mismatch')
 else:bak.write_bytes(t.read_bytes())
 atomic_write(t,PAYLOAD.read_bytes())
 if sha(t)!=POST_SHA256:raise RuntimeError('postimage SHA mismatch')
 print(f'BACKUP={bak}');print('G17B1R3_SOURCE_APPLY=PASS')
def rollback(root:Path)->None:
 t=root/SOURCE_RELATIVE;bak=t.with_name(t.name+'.g17b1r3.preimage')
 if not t.is_file() or sha(t)!=POST_SHA256:raise RuntimeError('target is not exact G17B1R3 postimage')
 if not bak.is_file() or sha(bak)!=PRE_SHA256:raise RuntimeError('exact G17B1R3 backup missing')
 atomic_write(t,bak.read_bytes())
 if sha(t)!=PRE_SHA256:raise RuntimeError('rollback SHA mismatch')
 bak.unlink();print('G17B1R3_SOURCE_ROLLBACK=PASS')
def main():
 a=argparse.ArgumentParser();a.add_argument('command',choices=('check','apply','rollback'));a.add_argument('--source-root',required=True,type=Path);n=a.parse_args();{'check':check,'apply':apply,'rollback':rollback}[n.command](n.source_root.resolve())
if __name__=='__main__':main()
