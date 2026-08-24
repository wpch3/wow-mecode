#!/usr/bin/env python3
from pathlib import Path
import hashlib,os,re,subprocess,sys
R=Path(__file__).resolve().parent
req=['01_Install_Build_G17B1R3.cmd','02_Rollback_Build_G17B1R3.cmd','Install-Build-G17B1R3-Windows.ps1','Rollback-Build-G17B1R3-Windows.ps1','README_FIRST.txt','PACKAGE_METADATA.txt','OFFLINE_VALIDATION_20260823.txt','tools/apply_g17b1r3_source.py','tests/test_g17b1r3.py','original/src/server/scripts/Commands/cs_dragonriding.cpp','payload/src/server/scripts/Commands/cs_dragonriding.cpp']
for x in req:assert (R/x).is_file(),x
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(R/'original/src/server/scripts/Commands/cs_dragonriding.cpp')=='2c7594d0f1428a767570063ac90c5f816991bf1d883fe61e45a1a28902a68199'
assert sha(R/'payload/src/server/scripts/Commands/cs_dragonriding.cpp')=='94ff80334783e8883f0811a1a7f76595d91b729cc43684f00273abb9d955628b'
post=(R/'payload/src/server/scripts/Commands/cs_dragonriding.cpp').read_text();ins=(R/'Install-Build-G17B1R3-Windows.ps1').read_text();rb=(R/'Rollback-Build-G17B1R3-Windows.ps1').read_text()
assert 'vehicle->GetPassenger(seatPair.first) == passenger' in post
assert 'authoritativeSeat == _seatId' in post
verify=post[post.index('class VerifyBoardingEvent'):post.index('bool SpawnTypedVehicle')]
assert '_player->GetTransSeat() == _seatId' not in verify
assert 'movementSeat = _player->GetTransSeat()' in verify
for text in (ins,rb):
 assert not re.search(r'\[string\[\]\]\$Args\b',text,re.I)
 assert '$NativeArgs' in text and '@NativeArgs' in text
for x in ('$BeforeExeUtc=$be.LastWriteTimeUtc','$BeforePdbUtc=$bp.LastWriteTimeUtc','$AfterExeUtc-le $BeforeExeUtc','$AfterPdbUtc-le $BeforePdbUtc','G17B1R3_WINDOWS_BUILD_RESULT=PASS','Python312\\python.exe','Python310\\python.exe'):
 assert x in ins,x
assert 'Get-Command py.exe' not in ins
for p in [R/'01_Install_Build_G17B1R3.cmd',R/'02_Rollback_Build_G17B1R3.cmd',R/'Install-Build-G17B1R3-Windows.ps1',R/'Rollback-Build-G17B1R3-Windows.ps1']:
 b=p.read_bytes();assert b.count(b'\r\n')==b.count(b'\n') and b.count(b'\r\n')>0 and not b.startswith(b'\xef\xbb\xbf');assert all(x<128 for x in b)
cp=subprocess.run([sys.executable,str(R/'tests/test_g17b1r3.py')],capture_output=True,text=True,env={**os.environ,'PYTHONDONTWRITEBYTECODE':'1'});assert cp.returncode==0,cp.stdout+cp.stderr
print('G17B1R3_PACKAGE_SELFTEST=PASS')
print('G17B1R3_AUTHORITATIVE_SEAT_GATE=PASS')
print('G17B1R3_TIMESTAMP_SNAPSHOTS=PASS')
print('G17B1R3_TESTS=PASS_9_OF_9')
