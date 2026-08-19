#!/bin/bash
# =====================================================================
#  召唤数量修复补丁（可选）
#
#  问题：.item / CustomSpellTweak 改了 BasePoints，但亡者大军还是 8 只。
#
#  根因（SpellEffects.cpp:2275-2303）：
#      switch (properties->ID) {
#          case 64: case 61: ... case 713:
#              numSummons = (damage > 0) ? damage : 1;   // 只有白名单里的才读 BasePoints
#              break;
#          default:
#              numSummons = 1;                            // 其余一律 1（然后走 DBC 自己的逻辑）
#      }
#
#  亡者大军的 SummonProperties ID 不在白名单里，所以改 BasePoints 无效。
#
#  本补丁：把 default 分支改成「也读 BasePoints」，
#  这样任何召唤类法术的数量都能通过改 BasePoints 控制。
#
#  用法：
#      cd /d/TrinityCore
#      bash step18_老职业技能/apply_summon.sh
# =====================================================================

set -e

ROOT="${1:-.}"
F="$ROOT/src/server/game/Spells/SpellEffects.cpp"

echo "=============================================="
echo " 召唤数量修复补丁"
echo "=============================================="
echo ""

if [ ! -f "$F" ]; then
    echo "[×] 找不到 $F"
    exit 1
fi
echo "[√] 找到 SpellEffects.cpp"

CRLF=0
if grep -q $'\r' "$F" 2>/dev/null; then
    CRLF=1
    echo "[i] 行尾：CRLF"
else
    echo "[i] 行尾：LF"
fi

TMP=$(mktemp)
sed 's/\r$//' "$F" > "$TMP"

if grep -q "CustomSummonCount" "$TMP"; then
    echo ""
    echo "[!] 补丁已经打过了，跳过。"
    rm -f "$TMP"
    exit 0
fi

echo ""
echo "---------- 开始打补丁 ----------"

python3 - "$TMP" <<'PYEOF'
import sys
path = sys.argv[1]
with open(path, 'r', encoding='utf-8', errors='surrogateescape') as f:
    src = f.read()

old = """        default:
            numSummons = 1;
            break;
    }
"""
new = """        default:
            // ---- CustomSummonCount ----
            // 原版这里写死 numSummons = 1，导致「亡者大军」这类法术
            // 无法通过改 BasePoints 调整召唤数量。
            // 改成也读 BasePoints，但只在 >1 时生效，保持原有行为不变。
            numSummons = (damage > 1) ? uint32(damage) : 1;
            // ---- end ----
            break;
    }
"""
n = src.count(old)
assert n == 1, "锚点出现 %d 次，应为 1 次" % n
src = src.replace(old, new, 1)

with open(path, 'w', encoding='utf-8', errors='surrogateescape') as f:
    f.write(src)
PYEOF

echo "[1/1] √ 已修改 default 分支"

if [ "$CRLF" = "1" ]; then
    sed -i 's/$/\r/' "$TMP"
fi
cp "$TMP" "$F"
rm -f "$TMP"

echo ""
echo "=============================================="
echo " 补丁完成"
echo "=============================================="
echo ""
grep -n "CustomSummonCount" "$F" | sed 's/\r$//'
echo ""
echo "改完只需【编译】，不用重跑 CMake（没新增文件）。"
echo ""
echo "回退：git checkout -- src/server/game/Spells/SpellEffects.cpp"
