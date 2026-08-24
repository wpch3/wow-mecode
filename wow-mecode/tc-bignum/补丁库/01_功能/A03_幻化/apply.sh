#!/bin/bash
# =====================================================================
#  幻化系统 Transmogrification —— 一键补丁
#
#  用法（Git Bash）：
#      cd /d/TrinityCore
#      bash step16_幻化/apply.sh
#
#  特性：
#    - CRLF 兼容（先转 LF 匹配，写回还原）
#    - 幂等：重复执行不会重复插入
#    - 只改 2 个文件：Player.cpp（4行）+ World.cpp（3行）
#    - 出错立即停止
# =====================================================================

set -e

ROOT="${1:-.}"
PLAYER="$ROOT/src/server/game/Entities/Player/Player.cpp"
WORLD="$ROOT/src/server/game/World/World.cpp"

echo "=============================================="
echo " 幻化系统 Transmogrification —— 补丁脚本"
echo "=============================================="
echo ""

# ---------- 前置检查 ----------
for f in "$PLAYER" "$WORLD"; do
    if [ ! -f "$f" ]; then
        echo "[×] 找不到 $f"
        echo "    请在 TrinityCore 源码根目录下运行，或用参数指定："
        echo "    bash apply.sh /d/TrinityCore"
        exit 1
    fi
done
echo "[√] 找到 Player.cpp / World.cpp"

SRCDIR="$ROOT/src/server/game/Custom"
if [ ! -f "$SRCDIR/CustomTransmog.cpp" ] || [ ! -f "$SRCDIR/CustomTransmog.h" ]; then
    echo "[×] 找不到源文件，请先把 CustomTransmog.h / .cpp 复制到："
    echo "    $SRCDIR/"
    exit 1
fi
echo "[√] 找到 CustomTransmog.h / .cpp"
echo ""

patch_file() {
    local FILE="$1"
    local LABEL="$2"
    local PYCODE="$3"

    local CRLF=0
    if grep -q $'\r' "$FILE" 2>/dev/null; then
        CRLF=1
    fi

    local TMP
    TMP=$(mktemp)
    sed 's/\r$//' "$FILE" > "$TMP"

    if grep -q "CustomTransmog" "$TMP"; then
        echo "[$LABEL] ! 已打过，跳过"
        rm -f "$TMP"
        return 0
    fi

    if ! python3 -c "$PYCODE" "$TMP"; then
        echo "[$LABEL] × 补丁失败"
        rm -f "$TMP"
        exit 1
    fi

    if [ "$CRLF" = "1" ]; then
        sed -i 's/$/\r/' "$TMP"
    fi
    cp "$TMP" "$FILE"
    rm -f "$TMP"
    echo "[$LABEL] √ 完成（行尾 $([ "$CRLF" = "1" ] && echo CRLF || echo LF)）"
}

echo "---------- 开始打补丁 ----------"

# =====================================================================
#  1. Player.cpp —— 核心：SetVisibleItemSlot 里查幻化缓存
#
#  原文（Player.cpp:12170 起）：
#      void Player::SetVisibleItemSlot(uint8 slot, Item* pItem)
#      {
#          if (pItem)
#          {
#              SetUInt32Value(PLAYER_VISIBLE_ITEM_1_ENTRYID + (slot * 2), pItem->GetEntry());
#
#  改为：先查幻化表，有幻化就用假 entry
# =====================================================================
read -r -d '' PY_PLAYER <<'PYEOF' || true
import sys
path = sys.argv[1]
with open(path, 'r', encoding='utf-8', errors='surrogateescape') as f:
    src = f.read()

# --- include ---
old_inc = '#include "CharacterCache.h"\n'
new_inc = '#include "CharacterCache.h"\n#include "CustomTransmog.h"\n'
if old_inc in src:
    src = src.replace(old_inc, new_inc, 1)
else:
    # 退路：插在 #include "Player.h" 之后
    alt = '#include "Player.h"\n'
    assert alt in src, "找不到 include 锚点"
    src = src.replace(alt, alt + '#include "CustomTransmog.h"\n', 1)

# --- SetVisibleItemSlot 主体 ---
old = """void Player::SetVisibleItemSlot(uint8 slot, Item* pItem)
{
    if (pItem)
    {
        SetUInt32Value(PLAYER_VISIBLE_ITEM_1_ENTRYID + (slot * 2), pItem->GetEntry());"""

new = """void Player::SetVisibleItemSlot(uint8 slot, Item* pItem)
{
    if (pItem)
    {
        // ---- Transmog: 有幻化就显示假外观，没有则用真实装备 ----
        uint32 visibleEntry = pItem->GetEntry();
        if (uint32 fakeEntry = sCustomTransmog->GetFakeEntry(GetGUID().GetCounter(), slot))
            visibleEntry = fakeEntry;
        // ---- end Transmog ----

        SetUInt32Value(PLAYER_VISIBLE_ITEM_1_ENTRYID + (slot * 2), visibleEntry);"""

n = src.count(old)
assert n == 1, "SetVisibleItemSlot 锚点出现 %d 次，应为 1 次" % n
src = src.replace(old, new, 1)

with open(path, 'w', encoding='utf-8', errors='surrogateescape') as f:
    f.write(src)
PYEOF

patch_file "$PLAYER" "1/2 Player.cpp" "$PY_PLAYER"

# =====================================================================
#  2. World.cpp —— 启动时载入幻化数据
#
#  插在 "Loading Item loot..." 之后（World.cpp:2173 附近）
# =====================================================================
read -r -d '' PY_WORLD <<'PYEOF' || true
import sys
path = sys.argv[1]
with open(path, 'r', encoding='utf-8', errors='surrogateescape') as f:
    src = f.read()

# --- include ---
old_inc = '#include "WorldSession.h"\n'
new_inc = '#include "WorldSession.h"\n#include "CustomTransmog.h"\n'
if old_inc in src:
    src = src.replace(old_inc, new_inc, 1)
else:
    alt = '#include "World.h"\n'
    assert alt in src, "找不到 World.cpp 的 include 锚点"
    src = src.replace(alt, alt + '#include "CustomTransmog.h"\n', 1)

# --- 载入调用 ---
old = """    TC_LOG_INFO("server.loading", "Loading Item loot...");
    sLootItemStorage->LoadStorageFromDB();"""

new = """    TC_LOG_INFO("server.loading", "Loading Item loot...");
    sLootItemStorage->LoadStorageFromDB();

    // ---- Transmog: 载入幻化数据 ----
    TC_LOG_INFO("server.loading", "Loading Transmogrification...");
    sCustomTransmog->LoadFromDB();
    // ---- end Transmog ----"""

n = src.count(old)
assert n == 1, "World.cpp 锚点出现 %d 次，应为 1 次" % n
src = src.replace(old, new, 1)

with open(path, 'w', encoding='utf-8', errors='surrogateescape') as f:
    f.write(src)
PYEOF

patch_file "$WORLD" "2/2 World.cpp" "$PY_WORLD"

echo ""
echo "=============================================="
echo " 补丁完成"
echo "=============================================="
echo ""
echo "改动确认："
grep -n "CustomTransmog" "$PLAYER" | sed 's/\r$//'
grep -n "CustomTransmog" "$WORLD" | sed 's/\r$//'
echo ""
echo "下一步："
echo "  1. 【必须】重跑 CMake —— 新增了源文件 CustomTransmog.cpp"
echo "  2. 编译"
echo "  3. 执行 SQL 23、24（建两张表）"
echo "  4. transmog.conf 放进 worldserver.conf.d\\"
echo "  5. 重启 worldserver"
echo ""
echo "回退："
echo "  git checkout -- src/server/game/Entities/Player/Player.cpp"
echo "  git checkout -- src/server/game/World/World.cpp"
echo "  rm src/server/game/Custom/CustomTransmog.*"
