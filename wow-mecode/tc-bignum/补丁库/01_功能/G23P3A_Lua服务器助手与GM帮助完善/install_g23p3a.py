#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""G23-P3A hash-locked runtime Lua installer.
Target: D:\TC-Build\bin\RelWithDebInfo
"""
from __future__ import annotations
import argparse, hashlib, os, shutil, sys, tempfile
from pathlib import Path

HERE=Path(__file__).resolve().parent
ORIGINAL=HERE/'original'; PAYLOAD=HERE/'payload'
MODIFIED={
 'lua_scripts/extensions/G23Core.ext':(
  '89b394b19654bd346c38632d91b6fb9d6e8fb7d5ba640658d21231b2ae91a06c',
  '3311a1744502921f4b1ad85c06883d76ff68c4738e79e84e33a4f04b67eef12f'),
 'lua_scripts/custom_welcome.lua':(
  'c0baa8fa5954a16c1a15ae0de48626a7dac13b80834896ab85e96e1e0d6836c2',
  'f54ee1b08c54000c505199695708de9aeb1ccd286731db057d707ba74d8e905a'),
}
NEW={
 'lua_scripts/custom_server_assistant.lua':'1d80eb699e356a17c7d87048e96515d269175fc4b91521d52b56fd0344f48f39',
 'lua_scripts/custom_gmhelp.lua':'dabb6005423334ddf7fe151d7f47d3b2c9fb8d584921add72445e69c83eb8dca',
}

def sha(path:Path)->str: return hashlib.sha256(path.read_bytes()).hexdigest()
def require(v:bool,msg:str)->None:
 if not v: raise RuntimeError(msg)
def atomic_copy(src:Path,dst:Path)->None:
 dst.parent.mkdir(parents=True,exist_ok=True)
 fd,name=tempfile.mkstemp(prefix=f'.{dst.name}.g23p3a.',dir=dst.parent); os.close(fd); tmp=Path(name)
 try: shutil.copyfile(src,tmp); os.replace(tmp,dst)
 finally:
  if tmp.exists(): tmp.unlink()

def verify_package()->None:
 errors=[]
 for rel,(pre,post) in MODIFIED.items():
  for side,base,expected in (('pre',ORIGINAL,pre),('post',PAYLOAD,post)):
   p=base/rel
   if not p.is_file(): errors.append(f'missing package {side}: {rel}')
   elif sha(p)!=expected: errors.append(f'package {side} hash mismatch: {rel}')
 for rel,post in NEW.items():
  p=PAYLOAD/rel; marker=ORIGINAL/(rel+'.absent')
  if not marker.is_file(): errors.append(f'missing absent marker: {rel}')
  if not p.is_file(): errors.append(f'missing new payload: {rel}')
  elif sha(p)!=post: errors.append(f'new payload hash mismatch: {rel}')
 if errors: raise RuntimeError('\n'.join(errors))

def inspect(root:Path)->dict[str,str]:
 out={}
 for rel,(pre,post) in MODIFIED.items():
  p=root/rel
  if not p.is_file(): out[rel]='MISSING'
  else:
   h=sha(p); out[rel]='PRE' if h==pre else ('POST' if h==post else f'UNKNOWN:{h}')
 for rel,post in NEW.items():
  p=root/rel
  if not p.exists(): out[rel]='ABSENT'
  elif p.is_file() and sha(p)==post: out[rel]='POST'
  else: out[rel]=f"UNKNOWN:{sha(p) if p.is_file() else 'NOT_A_FILE'}"
 return out

def overall(states:dict[str,str])->str:
 if {states[r] for r in MODIFIED}=={'PRE'} and {states[r] for r in NEW}=={'ABSENT'}: return 'READY_TO_APPLY'
 if {states[r] for r in MODIFIED}=={'POST'} and {states[r] for r in NEW}=={'POST'}: return 'ALREADY_APPLIED'
 return 'BLOCKED_MIXED_OR_UNKNOWN'

def print_states(root:Path,states:dict[str,str])->None:
 print(f'G23P3A_ROOT={root}')
 for rel in list(MODIFIED)+list(NEW): print(f'G23P3A_FILE={rel}; state={states[rel]}')
 print(f'G23P3A_STATE={overall(states)}')
 print('G23P3A_SQL_REQUIRED=False')
 print('G23P3A_COMPILE_REQUIRED=False')
 print('G23P3A_RELOAD_ELUNA_ALLOWED=False')

def apply(root:Path)->None:
 verify_package(); before=inspect(root); print_states(root,before); state=overall(before)
 if state=='ALREADY_APPLIED': print('G23P3A_APPLY=NOOP_ALREADY_POST'); return
 require(state=='READY_TO_APPLY','refusing apply: target is mixed, missing, or unknown')
 replaced=[]; created=[]
 try:
  for rel in MODIFIED: atomic_copy(PAYLOAD/rel,root/rel); replaced.append(rel)
  for rel in NEW: atomic_copy(PAYLOAD/rel,root/rel); created.append(rel)
 except Exception:
  for rel in reversed(created):
   p=root/rel
   if p.is_file(): p.unlink()
  for rel in reversed(replaced): atomic_copy(ORIGINAL/rel,root/rel)
  raise
 require(overall(inspect(root))=='ALREADY_APPLIED','post-apply verification failed')
 print('G23P3A_APPLY=PASS'); print('G23P3A_FINAL_STATE=POST')

def rollback(root:Path)->None:
 verify_package(); before=inspect(root); print_states(root,before); state=overall(before)
 if state=='READY_TO_APPLY': print('G23P3A_ROLLBACK=NOOP_ALREADY_PRE'); return
 require(state=='ALREADY_APPLIED','refusing rollback: target is mixed or unknown')
 restored=[]; removed=[]
 try:
  for rel in MODIFIED: atomic_copy(ORIGINAL/rel,root/rel); restored.append(rel)
  for rel in NEW:
   p=root/rel
   if p.is_file(): p.unlink(); removed.append(rel)
 except Exception:
  for rel in restored: atomic_copy(PAYLOAD/rel,root/rel)
  for rel in removed: atomic_copy(PAYLOAD/rel,root/rel)
  raise
 require(overall(inspect(root))=='READY_TO_APPLY','post-rollback verification failed')
 print('G23P3A_ROLLBACK=PASS'); print('G23P3A_FINAL_STATE=PRE')

def selftest()->None:
 verify_package()
 with tempfile.TemporaryDirectory(prefix='g23p3a-installer-') as name:
  root=Path(name)
  for rel in MODIFIED: atomic_copy(ORIGINAL/rel,root/rel)
  require(overall(inspect(root))=='READY_TO_APPLY','fixture not PRE')
  apply(root); apply(root); rollback(root); rollback(root)
  victim=root/next(iter(MODIFIED)); victim.write_bytes(victim.read_bytes()+b'X')
  snapshot={r:(root/r).read_bytes() if (root/r).is_file() else None for r in list(MODIFIED)+list(NEW)}
  try: apply(root)
  except RuntimeError: pass
  else: raise RuntimeError('mixed target was not rejected')
  current={r:(root/r).read_bytes() if (root/r).is_file() else None for r in list(MODIFIED)+list(NEW)}
  require(snapshot==current,'mixed rejection modified target')
 print('G23P3A_INSTALLER_SELFTEST=PASS')

def main()->int:
 ap=argparse.ArgumentParser(); ap.add_argument('action',choices=('check','apply','rollback','selftest')); ap.add_argument('root',nargs='?'); a=ap.parse_args()
 try:
  if a.action=='selftest': selftest(); return 0
  if not a.root: ap.error('root required')
  root=Path(a.root).expanduser().resolve(); verify_package()
  if a.action=='check':
   st=inspect(root); print_states(root,st); return 0 if overall(st)!='BLOCKED_MIXED_OR_UNKNOWN' else 2
  (apply if a.action=='apply' else rollback)(root); return 0
 except Exception as e:
  print(f'G23P3A_ERROR={e}',file=sys.stderr); return 1
if __name__=='__main__': raise SystemExit(main())
