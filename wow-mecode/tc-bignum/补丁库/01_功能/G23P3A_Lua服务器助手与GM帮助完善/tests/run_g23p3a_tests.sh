#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LUA_BIN="${LUA_BIN:-lua}"
LUAC_BIN="${LUAC_BIN:-luac}"
python3 "$ROOT/tests/test_p3a_static.py"
python3 - <<'PY' "$ROOT/install_g23p3a.py"
from pathlib import Path
import sys
p=Path(sys.argv[1]); compile(p.read_text(encoding='utf-8'),str(p),'exec')
print('G23P3A_PYTHON_COMPILE=PASS')
PY
python3 -W error "$ROOT/install_g23p3a.py" selftest >/dev/null
python3 -O "$ROOT/install_g23p3a.py" selftest >/dev/null
while IFS= read -r -d '' f; do "$LUAC_BIN" -p "$f"; done < <(find "$ROOT/payload/lua_scripts" "$ROOT/tests" -type f \( -name '*.lua' -o -name '*.ext' \) -print0)
"$LUA_BIN" "$ROOT/tests/p3a_command_and_gmhelp_mock.lua" "$ROOT/payload" >/dev/null
python3 - <<'PY' "$ROOT"
from pathlib import Path
import sys
root=Path(sys.argv[1])
for p in root.glob('*.cmd'):
 d=p.read_bytes(); assert b'\r\n' in d and b'\n' not in d.replace(b'\r\n',b''),p
print('G23P3A_CMD_CRLF=PASS')
PY
echo G23P3A_ALL_LOCAL_TESTS=PASS
