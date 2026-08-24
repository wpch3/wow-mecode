#!/usr/bin/env python3
from pathlib import Path
from datetime import datetime
import hashlib,sys
p=Path(sys.argv[1]) if len(sys.argv)>1 else Path(__file__).parent/'证据/G17B1R1_WINDOWS_BUILD_RESULT_20260823.txt'
b=p.read_bytes(); text=b.decode('utf-8-sig'); lines=text.splitlines()
kv={}
for line in lines:
    if '=' in line and not line.startswith('MSBUILD|'):
        k,v=line.split('=',1);kv.setdefault(k,[]).append(v)
def one(k):
    assert k in kv and len(kv[k])==1,(k,kv.get(k));return kv[k][0]
def dt(k):return datetime.fromisoformat(one(k).replace('Z','+00:00'))
assert one('SOURCE_SHA256_BEFORE')=='10a7002db4c173e441836870da01dd3009a7ba470369ca1a89bdb399ee9b2f45'
assert one('SOURCE_SHA256_AFTER')=='2c7594d0f1428a767570063ac90c5f816991bf1d883fe61e45a1a28902a68199'
for k in ('NATIVE_SELFTEST_EXIT','SOURCE_APPLY_EXIT','MSBUILD_EXIT'): assert one(k)=='0',(k,one(k))
assert one('G17B1_SOURCE_APPLY_GATE')=='PASS'
assert int(one('DRAGONRIDING_VCXPROJ_HITS'))>=1
assert int(one('DRAGONRIDING_FRESH_OBJECTS'))>=1
assert one('BEFORE_EXE_SHA256')!=one('AFTER_EXE_SHA256')
assert int(one('AFTER_EXE_SIZE'))>0
start=dt('BUILD_START_UTC');assert dt('AFTER_EXE_UTC')>start;assert dt('AFTER_PDB_UTC')>start
assert any(line.strip()=='MSBUILD|  cs_dragonriding.cpp' for line in lines)
assert any('worldserver.vcxproj -> D:\\TC-Build\\bin\\RelWithDebInfo\\worldserver.exe' in line for line in lines)
assert one('G17B1R1_WINDOWS_BUILD_ERROR')=='exe/pdb timestamp did not advance'
assert one('G17B1R1_WINDOWS_BUILD_RESULT')=='FAIL'
print('REPORT_SHA256='+hashlib.sha256(b).hexdigest())
print('NATIVE_SELFTEST=PASS')
print('SOURCE_APPLY_POSTIMAGE=PASS')
print('MSBUILD_WORLD_SERVER_LINK=PASS')
print('FRESH_DRAGONRIDING_OBJECT=PASS')
print('EXE_HASH_CHANGED=PASS')
print('EXE_AND_PDB_AFTER_BUILD_START=PASS')
print('FINAL_FAILURE_CLASS=FALSE_NEGATIVE_LAZY_PDB_FILEINFO_TIMESTAMP')
print('G17B1R1_WINDOWS_BUILD_ACCEPTANCE=PASS')
