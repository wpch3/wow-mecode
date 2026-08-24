#!/usr/bin/env python3
from pathlib import Path, PurePosixPath
import hashlib
import importlib.util
import zipfile

ROOT = Path(__file__).resolve().parents[1]
TC_ROOT = ROOT.parents[2]
PLAN = TC_ROOT / '规划/G17_飞行与移动'
INSTALLER = ROOT / 'install_g17b0_source.py'
DB_PROBE = ROOT / 'sql/G17B0_world_probe_readonly.sql'
DB_PROBE_V2 = ROOT / 'sql/G17B0_world_probe_readonly_v2_collation_safe.sql'
DB_PROBE_V2_PLAN = PLAN / 'G17B0_world_probe_readonly_v2_collation_safe.sql'
USER_ZIP = PLAN / '证据/G17B0_LOCK_RESULT_20260822_120243.zip'
PREFLIGHT_ZIP = PLAN / 'G17B0_Preflight_20260822.zip'


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(value, message):
    if not value:
        raise AssertionError(message)


require(USER_ZIP.stat().st_size == 73265, 'user ZIP size')
require(sha(USER_ZIP) == 'ece3d768d3d46f7e573a76ac0527d439ab932c0b831ed923f3de6c600ddaee0c', 'user ZIP sha')
require(PREFLIGHT_ZIP.stat().st_size == 95950, 'preflight ZIP size')
require(sha(PREFLIGHT_ZIP) == '4affab7339c4ef636baae04175fdfc17723da98943a049bdd942fb016528132c', 'preflight ZIP sha')

spec = importlib.util.spec_from_file_location('g17b0_installer', INSTALLER)
module = importlib.util.module_from_spec(spec)
assert spec.loader
spec.loader.exec_module(module)
module.validate_package()
require(len(module.CONTEXT_SHA256) == 7, 'seven locked context files')
require(module.sha_file(module.PRE_LOADER) == module.PRE_LOADER_SHA256, 'loader pre')
require(module.sha_file(module.POST_LOADER) == module.POST_LOADER_SHA256, 'loader post')
require(module.sha_file(module.PAYLOAD) == module.PAYLOAD_SHA256, 'cpp payload')
require(module.transform_loader(module.PRE_LOADER.read_bytes()) == module.POST_LOADER.read_bytes(), 'exact transform')
require(not module.APPLY_APPROVAL.exists(), 'apply approval must be absent during preflight')

for probe in (DB_PROBE, DB_PROBE_V2):
    sql = probe.read_text(encoding='utf-8')
    executable = '\n'.join(line for line in sql.splitlines() if not line.lstrip().startswith('--'))
    for token in ('INSERT ', 'UPDATE ', 'DELETE ', 'REPLACE ', 'CREATE ', 'DROP ',
                  'ALTER ', 'TRUNCATE ', 'CALL ', 'SET ', 'USE '):
        require(token not in executable.upper(), f'{probe.name} read-only SQL token: {token}')
    require(executable.lstrip().upper().startswith('SELECT '), f'{probe.name} SELECT-only')
    for marker in ('G17B0_DB_PROBE_START', 'G17B0_DB_PREIMAGE_READY',
                   'BLOCKED_TARGET_1000171_COLLISION', 'G17B0_DB_PROBE_COMPLETE'):
        require(marker in sql, f'{probe.name}: {marker}')

require(DB_PROBE_V2.read_bytes() == DB_PROBE_V2_PLAN.read_bytes(), 'v2 delivery/formal byte match')
require(sha(DB_PROBE_V2) == '9d767115ecdc5983941e99ba900e7de43370d302ad8b6b837230f32ba9884310', 'v2 SQL sha')
v2_sql = DB_PROBE_V2.read_text(encoding='utf-8')
v2_executable = '\n'.join(line for line in v2_sql.splitlines() if not line.lstrip().startswith('--'))
require(v2_executable.upper().count('UNION ALL') == 15, 'v2 UNION branch count')
require(v2_executable.count('COLLATE utf8mb4_unicode_ci') == 48, 'v2 explicit result collations')
require(v2_executable.count('USING utf8mb4') == 48, 'v2 explicit result charset conversions')

with zipfile.ZipFile(USER_ZIP) as archive:
    require(archive.testzip() is None, 'user ZIP CRC')
    names = [i.filename for i in archive.infolist()]
    require(len(names) == 10, 'user ZIP entries')
    require(all(not PurePosixPath(n).is_absolute() and '..' not in PurePosixPath(n).parts for n in names), 'user ZIP paths')
with zipfile.ZipFile(PREFLIGHT_ZIP) as archive:
    require(archive.testzip() is None, 'preflight ZIP CRC')
    names = [i.filename for i in archive.infolist()]
    require(all(not PurePosixPath(n).is_absolute() and '..' not in PurePosixPath(n).parts for n in names), 'preflight ZIP paths')
    require(any(n.endswith('Run-G17B0-Source-Preflight.cmd') for n in names), 'automatic source preflight wrapper')
    require(not any(n.endswith('G17B0_Apply.cmd') for n in names), 'no Apply wrapper')
    require(not any(n.endswith('G17B0_APPLY_APPROVED.txt') for n in names), 'no approval marker')

print('G17B0_PREFLIGHT_STATIC=PASS')
print('G17B0_USER_RESULT_ZIP=PASS')
print('G17B0_APPLY_LOCKED=True')
print('G17B0_DB_PROBE_READ_ONLY=True')
print('G17B0_DB_PROBE_V2_COLLATION_SAFE=PASS')
