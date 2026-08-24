#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LUA_BIN="${LUA_BIN:-lua}"
LUAC_BIN="${LUAC_BIN:-luac}"

python3 "$ROOT/tests/test_g23p2r1_static.py"
python3 - <<'PY' "$ROOT/install_g23p2r1.py"
from pathlib import Path
import sys
p=Path(sys.argv[1])
compile(p.read_text(encoding='utf-8'), str(p), 'exec')
print('G23P2R1_PYTHON_COMPILE=PASS')
PY
python3 -W error "$ROOT/install_g23p2r1.py" selftest >/dev/null
python3 -O "$ROOT/install_g23p2r1.py" selftest >/dev/null
"$LUAC_BIN" -p "$ROOT/original/lua_scripts/custom_teleport.lua"
"$LUAC_BIN" -p "$ROOT/payload/lua_scripts/custom_teleport.lua"
"$LUAC_BIN" -p "$ROOT/tests/teleport_stateless_mock.lua"
"$LUAC_BIN" -p "$ROOT/tests/cross_state_regression.lua"
"$LUA_BIN" "$ROOT/tests/teleport_stateless_mock.lua" "$ROOT/payload/lua_scripts/custom_teleport.lua" >/dev/null
"$LUA_BIN" "$ROOT/tests/cross_state_regression.lua" \
  "$ROOT/original/lua_scripts/custom_teleport.lua" \
  "$ROOT/payload/lua_scripts/custom_teleport.lua" >/dev/null
python3 - <<'PY' "$ROOT"
from pathlib import Path
import sys
root=Path(sys.argv[1])
for cmd in root.glob('*.cmd'):
    data=cmd.read_bytes()
    assert b'\r\n' in data and b'\n' not in data.replace(b'\r\n',b''), cmd
print('G23P2R1_CMD_CRLF=PASS')
PY
echo "G23P2R1_ALL_LOCAL_TESTS=PASS"
