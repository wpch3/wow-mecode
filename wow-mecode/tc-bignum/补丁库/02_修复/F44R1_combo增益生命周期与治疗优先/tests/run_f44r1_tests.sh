#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../../.." && pwd)"
PATCH="$ROOT/tc-bignum/补丁库/02_修复/F44R1_combo增益生命周期与治疗优先"
MOCK="$ROOT/tc-bignum/补丁库/01_功能/A06_战斗辅助/_编译验证"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

cd "$ROOT"
python3 tc-bignum/tools/gen_specdata_v3.py --check
python3 "$PATCH/tests/test_f44r1_static.py"
python3 "$PATCH/tests/test_f44r1_behavior.py"
python3 -m py_compile \
  tc-bignum/tools/gen_specdata_v3.py \
  "$PATCH/install_f44r1_combo.py" \
  "$PATCH/tests/test_f44r1_static.py" \
  "$PATCH/tests/test_f44r1_behavior.py"
python3 "$PATCH/install_f44r1_combo.py" --self-test > "$TMP/installer-normal.log"
python3 -O "$PATCH/install_f44r1_combo.py" --self-test > "$TMP/installer-optimized.log"
grep -q 'F44R1_INSTALLER_SELF_TEST_PASS=True' "$TMP/installer-normal.log"
grep -q 'F44R1_INSTALLER_SELF_TEST_PASS=True' "$TMP/installer-optimized.log"
echo '[OK] F44R1_INSTALLER_SELF_TEST=normal+optimized'

g++ -std=c++17 -Wall -Wextra -Werror -I "$MOCK" \
  "$MOCK/cs_test.cpp" "$MOCK/specdata_test.cpp" "$MOCK/shim.cpp" "$MOCK/runtest.cpp" \
  -o "$TMP/f44r1_runtime_mock"
"$TMP/f44r1_runtime_mock" > "$TMP/runtime.log"
grep -q '通过 32 / 失败 0' "$TMP/runtime.log"
echo '[OK] F44R1_FULL_RUNTIME_MOCK=32/32'

for test in aoetest bartest bufffixtest bugfixtest logictest persisttest setuptest v33test v34test v3test; do
  g++ -std=c++17 -Wall -Wextra -I "$MOCK" \
    "$MOCK/$test.cpp" "$MOCK/specdata_test.cpp" "$MOCK/shim.cpp" \
    -o "$TMP/$test"
  "$TMP/$test" > "$TMP/$test.log"
  if grep -q '\[FAIL\]' "$TMP/$test.log"; then
    echo "[FAIL] legacy mock: $test" >&2
    grep '\[FAIL\]' "$TMP/$test.log" >&2
    exit 1
  fi
  echo "[OK] LEGACY_MOCK=$test"
done

echo '[OK] F44R1_ALL_TESTS=PASS'
