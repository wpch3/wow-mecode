#!/bin/bash
# =====================================================================
#  AoE Loot 群体拾取 —— 一键补丁
#
#  用法（Git Bash）：
#      cd /d/TrinityCore
#      bash /path/to/apply.sh
#
#  特性：
#    - CRLF 兼容（.gitattributes 有 * text=auto，Windows 检出是 CRLF）
#      先统一成 LF 匹配，写回时还原 CRLF
#    - 幂等：重复执行不会重复插入
#    - 只改 1 个文件：src/server/game/Handlers/LootHandler.cpp
#    - 出错立即停止，不留半吊子状态
# =====================================================================

set -e

ROOT="${1:-.}"
LH="$ROOT/src/server/game/Handlers/LootHandler.cpp"

echo "=============================================="
echo " AoE Loot 群体拾取 —— 补丁脚本"
echo "=============================================="
echo ""

# ---------- 前置检查 ----------
if [ ! -f "$LH" ]; then
    echo "[×] 找不到 $LH"
    echo "    请在 TrinityCore 源码根目录下运行，或用参数指定根目录："
    echo "    bash apply.sh /d/TrinityCore"
    exit 1
fi
echo "[√] 找到 LootHandler.cpp"

SRCDIR="$ROOT/src/server/game/Loot"
if [ ! -f "$SRCDIR/CustomAoELoot.cpp" ] || [ ! -f "$SRCDIR/CustomAoELoot.h" ]; then
    echo "[×] 找不到源文件，请先把 CustomAoELoot.h / .cpp 复制到："
    echo "    $SRCDIR/"
    exit 1
fi
echo "[√] 找到 CustomAoELoot.h / .cpp"
echo ""

# ---------- 检测行尾 ----------
if grep -q $'\r' "$LH" 2>/dev/null; then
    CRLF=1
    echo "[i] LootHandler.cpp 行尾：CRLF（写回时会还原）"
else
    CRLF=0
    echo "[i] LootHandler.cpp 行尾：LF"
fi

TMP=$(mktemp)
# 统一成 LF 再处理
sed 's/\r$//' "$LH" > "$TMP"

# ---------- 幂等检查 ----------
if grep -q "CustomAoELoot" "$TMP"; then
    echo ""
    echo "[!] 补丁已经打过了（检测到 CustomAoELoot 引用），跳过。"
    rm -f "$TMP"
    exit 0
fi

echo ""
echo "---------- 开始打补丁 ----------"

# =====================================================================
#  改动 1/3：加 include
# =====================================================================
if grep -q '^#include "LootPackets.h"' "$TMP"; then
    sed -i 's|^#include "LootPackets.h"|#include "LootPackets.h"\n#include "CustomAoELoot.h"|' "$TMP"
    echo "[1/3] √ 已加 #include \"CustomAoELoot.h\""
else
    echo "[1/3] × 找不到锚点 #include \"LootPackets.h\""
    rm -f "$TMP"
    exit 1
fi

# =====================================================================
#  改动 2/3：物品群体拾取
#
#  原文（HandleAutostoreLootItemOpcode 结尾处）：
#      player->StoreLootItem(lootSlot, loot);
#
#  改为：先拾取圆心那具（原版行为），再处理周围尸体
# =====================================================================
ANCHOR2='    player->StoreLootItem(lootSlot, loot);'
if grep -qF "$ANCHOR2" "$TMP"; then
    python3 - "$TMP" <<'PYEOF'
import sys
path = sys.argv[1]
with open(path, 'r', encoding='utf-8', errors='surrogateescape') as f:
    src = f.read()

old = "    player->StoreLootItem(lootSlot, loot);\n"
new = """    player->StoreLootItem(lootSlot, loot);

    // ---- AoE Loot: 群体拾取周围尸体 ----
    if (CustomAoELoot::Enabled() && lguid.IsCreatureOrVehicle())
        if (Creature* aoeOrigin = player->GetMap()->GetCreature(lguid))
            CustomAoELoot::LootAllAround(player, aoeOrigin);
    // ---- end AoE Loot ----
"""
assert src.count(old) == 1, "锚点2 出现 %d 次，应为 1 次" % src.count(old)
src = src.replace(old, new, 1)

with open(path, 'w', encoding='utf-8', errors='surrogateescape') as f:
    f.write(src)
PYEOF
    echo "[2/3] √ 已插入物品群体拾取"
else
    echo "[2/3] × 找不到锚点 player->StoreLootItem(lootSlot, loot);"
    rm -f "$TMP"
    exit 1
fi

# =====================================================================
#  改动 3/3：金币群体拾取
#
#  插在 HandleLootMoneyOpcode 的单人分支（else 里），
#  刻意避开上面 //npcbot 的组队分金逻辑，不碰它一行。
# =====================================================================
python3 - "$TMP" <<'PYEOF'
import sys
path = sys.argv[1]
with open(path, 'r', encoding='utf-8', errors='surrogateescape') as f:
    src = f.read()

old = """        else
        {
            player->ModifyMoney(loot->gold);
            player->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LOOT_MONEY, loot->gold);
"""
new = """        else
        {
            // ---- AoE Loot: 汇总周围尸体的金币 ----
            if (CustomAoELoot::Enabled() && guid.IsCreatureOrVehicle())
                if (Creature* aoeOrigin = player->GetMap()->GetCreature(guid))
                    loot->gold += CustomAoELoot::GatherMoneyAround(player, aoeOrigin);
            // ---- end AoE Loot ----

            player->ModifyMoney(loot->gold);
            player->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LOOT_MONEY, loot->gold);
"""
n = src.count(old)
assert n == 1, "锚点3 出现 %d 次，应为 1 次" % n
src = src.replace(old, new, 1)

with open(path, 'w', encoding='utf-8', errors='surrogateescape') as f:
    f.write(src)
PYEOF
echo "[3/3] √ 已插入金币群体拾取"

# ---------- 写回（还原 CRLF）----------
if [ "$CRLF" = "1" ]; then
    sed -i 's/$/\r/' "$TMP"
fi
cp "$TMP" "$LH"
rm -f "$TMP"

echo ""
echo "=============================================="
echo " 补丁完成"
echo "=============================================="
echo ""
echo "改动确认："
grep -n "CustomAoELoot" "$LH" | sed 's/\r$//'
echo ""
echo "下一步："
echo "  1. 【必须】重跑 CMake —— 新增了源文件 CustomAoELoot.cpp"
echo "     （file(GLOB) 的文件列表在生成时固定，不重跑扫不到新文件）"
echo "  2. 编译"
echo "  3. 把 aoeloot.conf 放进 worldserver.conf.d\\"
echo "  4. 重启 worldserver"
echo ""
echo "回退：git checkout -- src/server/game/Handlers/LootHandler.cpp"
echo "      并删除 src/server/game/Loot/CustomAoELoot.*"
