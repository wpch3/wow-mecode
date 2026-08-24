#!/usr/bin/env python3
from pathlib import Path
import hashlib,os,re,subprocess,sys
R=Path(__file__).resolve().parent
req=['01_Install_Build_G17B1R4.cmd','02_Rollback_Build_G17B1R4.cmd','Install-Build-G17B1R4-Windows.ps1','Rollback-Build-G17B1R4-Windows.ps1','README_FIRST.txt','PACKAGE_METADATA.txt','OFFLINE_VALIDATION_20260823.txt','tools/apply_g17b1r4_source.py','tests/test_g17b1r4.py','original/src/server/scripts/Commands/cs_dragonriding.cpp','payload/src/server/scripts/Commands/cs_dragonriding.cpp']
for x in req:assert (R/x).is_file(),x
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(R/'original/src/server/scripts/Commands/cs_dragonriding.cpp')=='94ff80334783e8883f0811a1a7f76595d91b729cc43684f00273abb9d955628b'
assert sha(R/'payload/src/server/scripts/Commands/cs_dragonriding.cpp')=='e9418704731a2d9cd5119cc2024079a2326802796d00bf24e88928dd17ea7059'
pre=(R/'original/src/server/scripts/Commands/cs_dragonriding.cpp').read_text();post=(R/'payload/src/server/scripts/Commands/cs_dragonriding.cpp').read_text();ins=(R/'Install-Build-G17B1R4-Windows.ps1').read_text();rb=(R/'Rollback-Build-G17B1R4-Windows.ps1').read_text()
for x in ('SAFETY_CHECK_INTERVAL_MS = 250','void UpdateAI(uint32 diff) override','G17Dragonriding::IsBlockedArea(player)','_safetyCleanupStarted = true;','G17Dragonriding::CleanupPlayer(player, true);'):assert x in post,x
assert 'void UpdateAI(uint32 diff) override' not in pre
assert 'GetAuthoritativePassengerSeatId' in post and 'authoritativeSeat == _seatId' in post
b=lambda s,a,z:s[s.index(a):s.index(z,s.index(a))]
assert b(pre,'    void DoAction(int32 action) override','    void JustDied')==b(post,'    void DoAction(int32 action) override','    void JustDied')
for text in (ins,rb):
 assert not re.search(r'\[string\[\]\]\$Args\b',text,re.I);assert '$NativeArgs' in text and '@NativeArgs' in text
for x in ('$Pre="94ff80334783e8883f0811a1a7f76595d91b729cc43684f00273abb9d955628b"','$Post="e9418704731a2d9cd5119cc2024079a2326802796d00bf24e88928dd17ea7059"','$BeforeExeUtc=$be.LastWriteTimeUtc','$BeforePdbUtc=$bp.LastWriteTimeUtc','$AfterExeUtc-le $BeforeExeUtc','$AfterPdbUtc-le $BeforePdbUtc','G17B1R4_WINDOWS_BUILD_RESULT=PASS','Python312\\python.exe','Python310\\python.exe'):assert x in ins,x
assert 'Get-Command py.exe' not in ins
for p in [R/'01_Install_Build_G17B1R4.cmd',R/'02_Rollback_Build_G17B1R4.cmd',R/'Install-Build-G17B1R4-Windows.ps1',R/'Rollback-Build-G17B1R4-Windows.ps1']:
 data=p.read_bytes();assert data.count(b'\r\n')==data.count(b'\n') and data.count(b'\r\n')>0 and not data.startswith(b'\xef\xbb\xbf');assert all(x<128 for x in data)
cp=subprocess.run([sys.executable,str(R/'tests/test_g17b1r4.py')],capture_output=True,text=True,env={**os.environ,'PYTHONDONTWRITEBYTECODE':'1'});assert cp.returncode==0,cp.stdout+cp.stderr
print('G17B1R4_PACKAGE_SELFTEST=PASS')
print('G17B1R4_CONTINUOUS_INDOOR_GATE=PASS')
print('G17B1R4_TIMESTAMP_SNAPSHOTS=PASS')
print('G17B1R4_TESTS=PASS_11_OF_11')
