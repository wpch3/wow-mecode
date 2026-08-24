#!/bin/bash
# ============================================================================
#  定位 name='??' —— 重复的是一个【中文命令名】
# ============================================================================
#
#  用法（Git Bash）：
#      bash 查中文命令重复.sh /d/TrinityCore/src
#
#  原理：'??' 是 Windows 控制台显示不了的字符，2个问号 = 2个中文字。
#        全项目只有 botcommands.cpp 里的中文别名是【命令名】，
#        所以凶手一定在那里。
# ============================================================================

SRC="${1:-/d/TrinityCore/src}"
BC="$SRC/server/game/AI/NpcBots/botcommands.cpp"

echo "==================================================================="
echo " 查找重复的中文命令名"
echo "==================================================================="

if [ ! -f "$BC" ]; then
    echo "[错误] 找不到 $BC"
    exit 1
fi

echo
echo "### 1. botcommands.cpp 里所有【中文命令名】及出现次数"
echo "-------------------------------------------------------------------"
echo "  （正常应该每个只出现 1 次）"
echo

python3 - "$BC" <<'PYEOF'
import re, sys
from collections import Counter

path = sys.argv[1]
try:
    s = open(path, encoding='utf-8-sig').read()
except Exception:
    s = open(path, encoding='utf-8', errors='replace').read()

def has_cn(t):
    return any('\u4e00' <= c <= '\u9fff' for c in t)

lines = s.split('\n')
found = []
for i, l in enumerate(lines, 1):
    m = re.match(r'\s*\{\s*"([^"]+)"', l)
    if m and has_cn(m.group(1)):
        found.append((i, m.group(1)))

if not found:
    print("  [!] 一个中文命令名都没找到")
    print("      -> 说明中文别名段【还没插进去】，")
    print("         那 name='??' 的凶手就不在这个文件里。")
else:
    cnt = Counter(n for _, n in found)
    dup = {k: v for k, v in cnt.items() if v > 1}
    print(f"  共找到 {len(found)} 个中文命令名，去重后 {len(cnt)} 个")
    print()
    if dup:
        print("  [!! 找到凶手 !!] 以下中文命令名【重复】了：")
        print()
        for name, n in dup.items():
            print(f"    \"{name}\"  出现 {n} 次：")
            for ln, nm in found:
                if nm == name:
                    print(f"        第 {ln} 行: {lines[ln-1].strip()[:80]}")
            print()
        print("  修复：删掉重复的那几行，每个中文别名只保留【一行】。")
    else:
        print("  [正常] 没有重复的中文命令名")
        print()
        print("  列表：")
        for ln, nm in found:
            print(f"    {ln:>5} 行  {nm}")
PYEOF

echo
echo "### 2. 中文别名段是否被整段贴了两次"
echo "-------------------------------------------------------------------"
N=$(grep -c "中文别名（step41）\|中文别名结束" "$BC" 2>/dev/null | head -1)
N=${N:-0}
echo "  段落标记出现 $N 次（正常应为 2：开始+结束各一次）"
if [ "$N" -gt 2 ]; then
    echo "  [!! 可疑 !!] 段落标记出现多次，可能整段贴了两遍"
    grep -n "中文别名" "$BC" | sed 's/^/      /'
else
    echo "  [正常]"
fi

echo
echo "### 3. npcbotCommandTable 里所有命令名的重复检查（含英文）"
echo "-------------------------------------------------------------------"

python3 - "$BC" <<'PYEOF'
import re, sys
from collections import Counter

path = sys.argv[1]
try:
    s = open(path, encoding='utf-8-sig').read()
except Exception:
    s = open(path, encoding='utf-8', errors='replace').read()

lines = s.split('\n')
start = None
for i, l in enumerate(lines):
    if 'static ChatCommandTable npcbotCommandTable' in l:
        start = i
        break

if start is None:
    print("  找不到 npcbotCommandTable")
else:
    end = None
    for i in range(start, len(lines)):
        if lines[i].strip() == '};':
            end = i
            break
    names = []
    for i in range(start, end):
        m = re.match(r'\s*\{\s*"([^"]+)"', lines[i])
        if m:
            names.append((i + 1, m.group(1)))
    cnt = Counter(n for _, n in names)
    dup = {k: v for k, v in cnt.items() if v > 1}
    print(f"  该表共 {len(names)} 条命令（上游原本 30 条）")
    if dup:
        print()
        print("  [!! 重复 !!]")
        for name, n in dup.items():
            print(f"    \"{name}\" x{n}")
            for ln, nm in names:
                if nm == name:
                    print(f"        第 {ln} 行")
    else:
        print("  [正常] 无重复")
PYEOF

echo
echo "==================================================================="
echo " 完成"
echo "==================================================================="
