#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LUA_BIN="${LUA_BIN:-lua}"
LUAC_BIN="${LUAC_BIN:-luac}"

python3 "$ROOT/tests/test_g23p2_static_and_model.py"
python3 "$ROOT/install_g23p2.py" selftest >/dev/null
python3 -O "$ROOT/install_g23p2.py" selftest >/dev/null

while IFS= read -r -d '' script; do
  "$LUAC_BIN" -p "$script"
done < <(find "$ROOT/payload/lua_scripts" -type f \( -name '*.lua' -o -name '*.ext' \) -print0 | sort -z)

"$LUA_BIN" "$ROOT/tests/g23p2_mock.lua" "$ROOT/payload" -1 >/dev/null
"$LUA_BIN" "$ROOT/tests/g23p2_mock.lua" "$ROOT/payload" 0 >/dev/null
"$LUA_BIN" "$ROOT/tests/daily_atomic_mock.lua" "$ROOT/payload" >/dev/null
"$LUA_BIN" "$ROOT/tests/teleport_safety_mock.lua" "$ROOT/payload" >/dev/null

python3 - <<'PY' "$ROOT"
from pathlib import Path
import sys
root=Path(sys.argv[1])
for cmd in root.glob('*.cmd'):
    data=cmd.read_bytes()
    assert b'\r\n' in data and b'\n' not in data.replace(b'\r\n',b''), cmd
print('G23P2_CMD_CRLF=PASS')
PY

echo "G23P2_ALL_LOCAL_TESTS=PASS"
