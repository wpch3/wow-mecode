#!/usr/bin/env python3
from __future__ import annotations
import hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parent
required=[
 "Install-G17R5-Locale-Mirror.ps1","Rollback-G17R5-Locale-Mirror.ps1",
 "01_Install_G17R5_Locale_Mirror.cmd","02_Rollback_G17R5_Locale_Mirror.cmd","README_FIRST.txt",
 "PACKAGE_METADATA.txt","THIRD_PARTY_PROVENANCE.txt","OFFLINE_VALIDATION_20260823.txt",
 "original/NO_LOCALE_TARGET_PREIMAGE.txt","third_party/mpqcli-windows-amd64.exe","third_party/mpqcli-LICENSE.txt"]
for rel in required:
 p=ROOT/rel
 assert p.is_file(),f"missing: {rel}"
def sha(p:Path)->str:return hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(ROOT/'third_party/mpqcli-windows-amd64.exe')=='5dc56b130432098d18e911fdf4b3464adf850432b7fbb35003a2feff7c30a79f'
install=(ROOT/'Install-G17R5-Locale-Mirror.ps1').read_text('utf-8')
rollback=(ROOT/'Rollback-G17R5-Locale-Mirror.ps1').read_text('utf-8')
for token in ['patch-zhCN-"+$LocaleSlot+".MPQ','TARGET_PREIMAGE=ABSENT','OTHER_LOCALE_DBC_COLLISIONS',
              'ROOT_R4_MPQ_MODIFIED=False','SERVER_MODIFIED=False','ALREADY_CURRENT',
              'dd25091167f671764735ce88c78b66485c6d661fadf05d322574c261f6e464ea',
              '1acef997a27f844a8abee9b477c44f2097f745168c6df85ece6b1a135568c233']:
 assert token in install,token
for forbidden in ['Remove-Item -LiteralPath $Source','Move-Item -LiteralPath $Source']:
 assert forbidden not in install
for token in ['TARGET_PREIMAGE','TARGET_MPQ_SHA256','ROOT_R4_MPQ_MODIFIED=False','SERVER_MODIFIED=False','ROLLBACK_TRANSACTION_RECOVERY']:
 assert token in rollback,token
assert 'Remove-Item -LiteralPath $Target' not in rollback
print('G17R5_PACKAGE_SELFTEST=PASS')
print(f'G17R5_PACKAGE_REQUIRED_FILES={len(required)}')
print('G17R5_WINDOWS_TOOL_SHA256=PASS')
print('G17R5_INSTALL_SAFETY_TOKENS=PASS')
print('G17R5_ROLLBACK_SAFETY_TOKENS=PASS')
