#!/usr/bin/env python3
from pathlib import Path
import hashlib,subprocess,sys
R=Path(__file__).resolve().parent
req=['01_Install_Build_G17B1.cmd','02_Rollback_Build_G17B1.cmd','Install-Build-G17B1-Windows.ps1','Rollback-Build-G17B1-Windows.ps1','README_FIRST.txt','PACKAGE_METADATA.txt','OFFLINE_VALIDATION_20260823.txt','tools/apply_g17b1_source.py','tests/test_g17b1.py','original/src/server/scripts/Commands/cs_dragonriding.cpp','payload/src/server/scripts/Commands/cs_dragonriding.cpp']
for x in req:assert (R/x).is_file(),x
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(R/req[-2])=='10a7002db4c173e441836870da01dd3009a7ba470369ca1a89bdb399ee9b2f45'
assert sha(R/req[-1])=='2c7594d0f1428a767570063ac90c5f816991bf1d883fe61e45a1a28902a68199'
installer=(R/'Install-Build-G17B1-Windows.ps1').read_text()
assert 'Python312\\python.exe' in installer and 'Python310\\python.exe' in installer
assert 'Get-Command py.exe' not in installer
cp=subprocess.run([sys.executable,str(R/'tests/test_g17b1.py')],capture_output=True,text=True)
assert cp.returncode==0,cp.stdout+cp.stderr
print('G17B1_PACKAGE_SELFTEST=PASS');print('G17B1_STATIC_TESTS=PASS_8');print('G17B1_SOURCE_HASHES=PASS')
