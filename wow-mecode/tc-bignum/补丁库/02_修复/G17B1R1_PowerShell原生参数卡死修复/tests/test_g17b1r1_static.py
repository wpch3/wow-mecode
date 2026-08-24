#!/usr/bin/env python3
from pathlib import Path
import re
R=Path(__file__).resolve().parents[1]
old=(R/'original/Install-Build-G17B1-Windows.ps1').read_text()
new=(R/'payload/Install-Build-G17B1R1-Windows.ps1').read_text()
rb=(R/'payload/Rollback-Build-G17B1R1-Windows.ps1').read_text()
assert re.search(r'\[string\[\]\]\$Args\b',old,re.I)
for text in (new,rb):
 assert not re.search(r'\[string\[\]\]\$Args\b',text,re.I)
 assert '[string[]]$NativeArgs' in text
 assert '@NativeArgs' in text
assert new.count('Invoke-NativeLogged -FilePath')==3
assert rb.count('Invoke-NativeLogged -FilePath')==2
for s in ('-FilePath $com -NativeArgs $NativeSelfTestArgs -Prefix','-FilePath $python -NativeArgs $SourceApplyArgs -Prefix','-FilePath $msbuild -NativeArgs $MSBuildArgs -Prefix'):
 assert s in new
assert 'G17B1R1_WINDOWS_BUILD_RESULT.txt' in new
assert 'Get-Command py.exe' not in new
print('G17B1R1_ROOT_CAUSE_STATIC_TEST=PASS')
