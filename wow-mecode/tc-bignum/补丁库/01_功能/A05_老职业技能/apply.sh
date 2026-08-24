#!/bin/bash
# =====================================================================
#  老职业技能强化 —— 一键补丁
#
#  用法（Git Bash）：
#      cd /d/TrinityCore
#      bash step18_老职业技能/apply.sh
#
#  只改 1 个文件：src/server/game/World/World.cpp（+5 行）
#  CRLF 兼容 + 幂等
# =====================================================================

set -e

ROOT="${1:-.}"
WORLD="$ROOT/src/server/game/World/World.cpp"

echo "=============================================="
echo " 老职业技能强化 —— 补丁脚本"
echo "=============================================="
echo ""

if [ ! -f "$WORLD" ]; then
    echo "[×] 找不到 $WORLD"
    echo "    请在 TrinityCore 源码根目录下运行，或用参数指定："
    echo "    bash apply.sh /d/TrinityCore"
    exit 1
fi
echo "[√] 找到 World.cpp"

SRCDIR="$ROOT/src/server/game/Spells"
if [ ! -f "$SRCDIR/CustomSpellTweak.cpp" ] || [ ! -f "$SRCDIR/CustomSpellTweak.h" ]; then
    echo "[×] 找不到源文件，请先把 CustomSpellTweak.h / .cpp 复制到："
    echo "    $SRCDIR/"
    exit 1
fi
echo "[√] 找到 CustomSpellTweak.h / .cpp"
echo ""

# ---------- 检测行尾 ----------
CRLF=0
if grep -q $'\r' "$WORLD" 2>/dev/null; then
    CRLF=1
    echo "[i] World.cpp 行尾：CRLF（写回时会还原）"
else
    echo "[i] World.cpp 行尾：LF"
fi

TMP=$(mktemp)
sed 's/\r$//' "$WORLD" > "$TMP"

# ---------- 幂等检查 ----------
if grep -q "CustomSpellTweak" "$TMP"; then
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

# --- 1) include ---
old_inc = '#include "WorldSession.h"\n'
if old_inc in src:
    src = src.replace(old_inc, old_inc + '#include "CustomSpellTweak.h"\n', 1)
else:
    alt = '#include "World.h"\n'
    assert alt in src, "找不到 include 锚点"
    src = src.replace(alt, alt + '#include "CustomSpellTweak.h"\n', 1)

# --- 2) 挂载点 ---
# 插在 LoadSpellInfoImmunities() 之后，此时所有法术数据已加载完毕
old = '''    TC_LOG_INFO("server.loading", "Loading SpellInfo immunity infos...");
    sSpellMgr->LoadSpellInfoImmunities();
'''
new = '''    TC_LOG_INFO("server.loading", "Loading SpellInfo immunity infos...");
    sSpellMgr->LoadSpellInfoImmunities();

    // ---- 老职业技能强化 ----
    // 必须在所有 SpellInfo 加载完之后，否则改动会被覆盖
    TC_LOG_INFO("server.loading", "Applying custom class spell tweaks...");
    sCustomSpellTweak->ApplyAll();
    // ---- end ----
'''
n = src.count(old)
assert n == 1, "挂载点锚点出现 %d 次，应为 1 次" % n
src = src.replace(old, new, 1)

with open(path, 'w', encoding='utf-8', errors='surrogateescape') as f:
    f.write(src)
PYEOF

echo "[1/1] √ 已插入挂载点"

# ---------- 写回 ----------
if [ "$CRLF" = "1" ]; then
    sed -i 's/$/\r/' "$TMP"
fi
cp "$TMP" "$WORLD"
rm -f "$TMP"

echo ""
echo "=============================================="
echo " 补丁完成"
echo "=============================================="
echo ""
echo "改动确认："
grep -n "CustomSpellTweak" "$WORLD" | sed 's/\r$//'
echo ""
echo "下一步："
echo "  1. 【必须】重跑 CMake —— 新增了源文件"
echo "  2. 编译"
echo "  3. customspell.conf 放进 worldserver.conf.d\\"
echo "  4. 重启 worldserver"
echo ""
echo "回退："
echo "  git checkout -- src/server/game/World/World.cpp"
echo "  rm src/server/game/Spells/CustomSpellTweak.*"
