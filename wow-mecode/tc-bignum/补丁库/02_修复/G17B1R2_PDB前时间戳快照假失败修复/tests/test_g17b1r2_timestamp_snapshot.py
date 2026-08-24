from pathlib import Path
R=Path(__file__).resolve().parents[1]
old=(R/'original/Install-Build-G17B1R1-Windows.ps1').read_text()
new=(R/'payload/Install-Build-G17B1R2-Windows.ps1').read_text()
assert '$bp=Get-Item $Pdb' in old
assert '$BeforePdbUtc=$bp.LastWriteTimeUtc' not in old
assert '$BeforeExeUtc=$be.LastWriteTimeUtc' in new
assert '$BeforePdbUtc=$bp.LastWriteTimeUtc' in new
assert 'BEFORE_PDB_UTC=$($BeforePdbUtc.ToString' in new
assert '$ap.LastWriteTimeUtc-le $BeforePdbUtc' in new
assert '$ap.LastWriteTimeUtc-le $bp.LastWriteTimeUtc' not in new
assert '[string[]]$NativeArgs' in new
print('G17B1R2_TIMESTAMP_SNAPSHOT_STATIC=PASS')
