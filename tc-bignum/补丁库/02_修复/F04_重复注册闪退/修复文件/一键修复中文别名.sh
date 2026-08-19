#!/bin/bash
# ============================================================================
#  一键修复：删掉损坏的 "??" 别名，重新插入正确的中文别名
# ============================================================================
#
#  用法（Git Bash）：
#      cd 到本脚本和 alias.txt 所在目录，然后：
#      bash 一键修复中文别名.sh /d/TrinityCore/src
#
#  它会：
#    1. 自动备份 botcommands.cpp
#    2. 删掉所有 { "??", ... } 这类损坏行
#    3. 以 UTF-8-BOM 重新写入正确的 21 行中文别名
#    4. 自动验证结果
#
#  【为什么用脚本而不是手贴】
#    手动在 VS2022 里粘贴，字符可能又被转成 '?'（这次就是这么坏的）。
#    Python 的 encoding='utf-8-sig' 能保证写出带 BOM 的 UTF-8，
#    汉字不会被替换。
# ============================================================================

SRC="${1:-/d/TrinityCore/src}"
BC="$SRC/server/game/AI/NpcBots/botcommands.cpp"
DIR="$(cd "$(dirname "$0")" && pwd)"
ALIAS="$DIR/alias.txt"

echo "==================================================================="
echo " 修复 botcommands.cpp 的中文别名"
echo "==================================================================="
echo "  目标文件: $BC"
echo "  别名来源: $ALIAS"
echo

if [ ! -f "$BC" ];    then echo "[错误] 找不到 $BC"; exit 1; fi
if [ ! -f "$ALIAS" ]; then echo "[错误] 找不到 $ALIAS（请和本脚本放同一目录）"; exit 1; fi

# 备份
BAK="$BC.bak.$(date +%Y%m%d_%H%M%S)"
cp "$BC" "$BAK"
echo "  已备份到: $BAK"
echo

python3 - "$BC" "$ALIAS" <<'PYEOF'
import io, re, sys

bc_path, alias_path = sys.argv[1], sys.argv[2]

# utf-8-sig 读取：有BOM会自动去掉，没有也能读
src = io.open(bc_path, encoding='utf-8-sig', errors='replace').read()
alias = io.open(alias_path, encoding='utf-8-sig').read()

lines = src.split('\n')

# ---- 1) 删掉损坏的别名行（名字里含 ? 的命令条目）和旧的段落标记 ----
removed = 0
out = []
for l in lines:
    m = re.match(r'\s*\{\s*"([^"]*)"', l)
    if m:
        nm = m.group(1)
        # 删损坏的（名字含?）
        if '?' in nm:
            removed += 1
            continue
        # 【幂等】删已存在的中文别名，后面统一重新插入
        # 不做这一步的话，重复运行脚本会插入两份 -> 又变成重复注册
        if any('\u4e00' <= c <= '\u9fff' for c in nm):
            removed += 1
            continue
    if '中文别名' in l or '???' in l:
        removed += 1
        continue
    out.append(l)

print(f"  删除损坏/旧别名行: {removed} 行")
src = '\n'.join(out)

# ---- 2) 找插入点 ----
key = None
for cand in [
    'static ChatCommandTable npcbotCommandTable =\n        {\n',
    'static ChatCommandTable npcbotCommandTable =\r\n        {\r\n',
]:
    if cand in src:
        key = cand
        break

if key is None:
    m = re.search(r'(static ChatCommandTable npcbotCommandTable\s*=\s*\r?\n\s*\{\s*\r?\n)', src)
    if not m:
        print("  [错误] 找不到 npcbotCommandTable 的插入点，已还原（用备份文件）")
        sys.exit(1)
    key = m.group(1)

# ---- 3) 插入 ----
if not alias.endswith('\n'):
    alias += '\n'
src = src.replace(key, key + alias, 1)

# ---- 4) 以 UTF-8 BOM 写回 ----
io.open(bc_path, 'w', encoding='utf-8-sig', newline='').write(src)
print("  已以 UTF-8-BOM 写回")
PYEOF

if [ $? -ne 0 ]; then
    echo
    echo "[失败] 已回滚"
    cp "$BAK" "$BC"
    exit 1
fi

echo
echo "-------------------------------------------------------------------"
echo " 验证"
echo "-------------------------------------------------------------------"

BOM=$(head -c 3 "$BC" | od -An -tx1 | tr -d ' \n')
if [ "$BOM" = "efbbbf" ]; then
    echo "  编码: UTF-8 BOM  [正常]"
else
    echo "  编码: 无BOM      [异常!]"
fi

python3 - "$BC" <<'PYEOF'
import io, re, sys
from collections import Counter
s = io.open(sys.argv[1], encoding='utf-8-sig', errors='replace').read()
lines = s.split('\n')

def has_cn(t):
    return any('\u4e00' <= c <= '\u9fff' for c in t)

cn = []
qm = []
for i, l in enumerate(lines, 1):
    m = re.match(r'\s*\{\s*"([^"]+)"', l)
    if not m:
        continue
    nm = m.group(1)
    if has_cn(nm):
        cn.append(nm)
    if '?' in nm:
        qm.append((i, nm))

print(f"  中文命令名: {len(cn)} 个（应为 21）")
c = Counter(cn)
dup = {k: v for k, v in c.items() if v > 1}
print(f"  重复的中文名: {dup if dup else '无  [正常]'}")
print(f"  残留的 '?' 命令: {len(qm)} 个  {'[正常]' if not qm else '[异常!]'}")

# npcbotCommandTable 总条目
start = next((i for i, l in enumerate(lines) if 'static ChatCommandTable npcbotCommandTable' in l), None)
if start is not None:
    end = next((i for i in range(start, len(lines)) if lines[i].strip() == '};'), None)
    total = sum(1 for i in range(start, end) if re.match(r'\s*\{\s*"', lines[i]))
    print(f"  npcbotCommandTable 共 {total} 条（应为 51 = 30上游 + 21别名）")
PYEOF

echo
echo "==================================================================="
echo " 完成。若上面全部 [正常]，直接重新编译即可（不用重跑 CMake）"
echo " 出问题可用备份还原: cp \"$BAK\" \"$BC\""
echo "==================================================================="
