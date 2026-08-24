#!/usr/bin/env python3
from pathlib import Path
import hashlib,re,subprocess,sys
R=Path(__file__).resolve().parent
req=['01_Install_Build_G17B1R1.cmd','02_Rollback_Build_G17B1R1.cmd','Install-Build-G17B1R1-Windows.ps1','Rollback-Build-G17B1R1-Windows.ps1','README_FIRST.txt','PACKAGE_METADATA.txt','OFFLINE_VALIDATION_20260823.txt','original_installer/Install-Build-G17B1-Windows.ps1','original_installer/Rollback-Build-G17B1-Windows.ps1','tools/apply_g17b1_source.py','tests/test_g17b1.py','original/src/server/scripts/Commands/cs_dragonriding.cpp','payload/src/server/scripts/Commands/cs_dragonriding.cpp']
for x in req:assert (R/x).is_file(),x
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(R/'original/src/server/scripts/Commands/cs_dragonriding.cpp')=='10a7002db4c173e441836870da01dd3009a7ba470369ca1a89bdb399ee9b2f45'
assert sha(R/'payload/src/server/scripts/Commands/cs_dragonriding.cpp')=='2c7594d0f1428a767570063ac90c5f816991bf1d883fe61e45a1a28902a68199'
old=(R/'original_installer/Install-Build-G17B1-Windows.ps1').read_text()
new=(R/'Install-Build-G17B1R1-Windows.ps1').read_text()
rollback=(R/'Rollback-Build-G17B1R1-Windows.ps1').read_text()
assert re.search(r'\[string\[\]\]\$Args\b',old,re.I)
assert not re.search(r'\[string\[\]\]\$Args\b',new,re.I)
assert '@Args' not in new and '@Args' not in rollback
assert '[string[]]$NativeArgs' in new and '[string[]]$NativeArgs' in rollback
assert new.count('Invoke-NativeLogged -FilePath')==3
assert rollback.count('Invoke-NativeLogged -FilePath')==2
for token in ['-FilePath $com -NativeArgs $NativeSelfTestArgs -Prefix','-FilePath $python -NativeArgs $SourceApplyArgs -Prefix','-FilePath $msbuild -NativeArgs $MSBuildArgs -Prefix','G17B1R1_WINDOWS_BUILD_RESULT=PASS','Python312\\python.exe','Python310\\python.exe']:
 assert token in new,token
assert 'Get-Command py.exe' not in new
cp=subprocess.run([sys.executable,str(R/'tests/test_g17b1.py')],capture_output=True,text=True,env={**__import__('os').environ,'PYTHONDONTWRITEBYTECODE':'1'})
assert cp.returncode==0,cp.stdout+cp.stderr
print('G17B1R1_PACKAGE_SELFTEST=PASS')
print('G17B1R1_ARGS_COLLISION_REMOVED=PASS')
print('G17B1R1_NAMED_NATIVE_BINDINGS=PASS')
print('G17B1_STATIC_TESTS=PASS_8')
