#!/bin/bash
# ============================================================
# syntax_check.sh —— 用【真实编译器】验证 TrinityCore 脚本语法
#
# 用法: bash tools/syntax_check.sh 补丁库/xx/源文件/cs_xxx.cpp
#
# 【为什么需要这个】2026-08-05 F17 教训：
#   我只用自己写的括号平衡脚本检查，报"全部通过"，
#   但实际有变量重定义（C2374/C2086），用户白编译一次。
#   括号平衡 != 语法正确。必须用真编译器。
#
# 原理: 剥掉真实 include，换成 tools/tc_stub.h 里的最小桩，
#       用 g++ -fsyntax-only 做纯语法检查（不链接、不需要TC源码）
# ============================================================
set -e
F="$1"
[ -z "$F" ] && { echo "用法: bash tools/syntax_check.sh <文件.cpp>"; exit 1; }
[ -f "$F" ] || { echo "文件不存在: $F"; exit 1; }
D=$(mktemp -d)
cp "$(dirname "$0")/tc_stub.h" "$D/stub.h"
python3 - "$F" "$D/t.cpp" << 'PY'
import re,sys
src=open(sys.argv[1],encoding='utf-8-sig').read()
src=re.sub(r'^#include\s+[<"][^>"]+[>"].*$','',src,flags=re.M)
open(sys.argv[2],'w',encoding='utf-8').write('#include "stub.h"\n'+src)
PY
cd "$D"
OUT=$(g++ -std=c++20 -fsyntax-only -Wall t.cpp 2>&1 || true)
E=$(echo "$OUT" | grep -c 'error' || true)
W=$(echo "$OUT" | grep -c 'warning' || true)
echo "=============================================="
echo " $F"
echo "=============================================="
if [ "$E" != "0" ] || [ "$W" != "0" ]; then echo "$OUT" | head -30; echo "---"; fi
echo " 错误: $E   警告: $W"
if [ "$E" = "0" ] && [ "$W" = "0" ]; then echo " >>> 语法干净，可以交付 <<<"; RC=0
elif [ "$E" = "0" ]; then echo " >>> 无错误，但有警告（建议清理）<<<"; RC=0
else echo " >>> 有编译错误，不能交付 <<<"; RC=1; fi
rm -rf "$D"; exit $RC
