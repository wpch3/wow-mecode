#!/bin/bash
# ============================================================================
#  step47  "Duplicate blank sub-command" 闪退 —— 一键诊断
# ============================================================================
#
#  用法（在 Git Bash 里跑）：
#      cd /d/TrinityCore
#      bash 诊断脚本.sh
#
#  或者直接把下面的命令一条条贴进 Git Bash。
# ============================================================================

SRC="${1:-/d/TrinityCore/src}"

echo "==================================================================="
echo " 检查目录: $SRC"
echo "==================================================================="
echo

LOADER="$SRC/server/scripts/Commands/cs_script_loader.cpp"

if [ ! -f "$LOADER" ]; then
    echo "[错误] 找不到 $LOADER"
    echo "       请把源码路径作为参数传入，例如："
    echo "       bash 诊断脚本.sh /d/TrinityCore/src"
    exit 1
fi

echo "-------------------------------------------------------------------"
echo " 1. 每个 AddSC 出现几次（>2 就是重复了）"
echo "-------------------------------------------------------------------"
echo
echo "  正常情况：声明1次 + 调用1次 = 2 次"
echo
for fn in AddSC_botrename_commandscript AddSC_pbot_autoaccept AddSC_playerbot_commandscript; do
    n=$(grep -c "$fn" "$LOADER" 2>/dev/null)
    if [ "$n" = "2" ]; then
        flag="[正常]"
    elif [ "$n" = "0" ]; then
        flag="[未安装]"
    else
        flag="[!! 重复 !!]  <--- 就是这个导致闪退"
    fi
    printf "  %-42s %s 次  %s\n" "$fn" "$n" "$flag"
done

echo
echo "-------------------------------------------------------------------"
echo " 2. 具体在哪几行"
echo "-------------------------------------------------------------------"
echo
grep -n "AddSC_botrename_commandscript\|AddSC_pbot_autoaccept" "$LOADER" 2>/dev/null

echo
echo "-------------------------------------------------------------------"
echo " 3. 检查是否有【两个文件】注册了同名脚本"
echo "-------------------------------------------------------------------"
echo
echo "  botrename_commandscript 出现在这些文件里："
grep -rln "botrename_commandscript" "$SRC" 2>/dev/null | sed 's/^/    /'
echo
echo "  如果出现在【两个以上 .cpp】里，说明文件被复制了两份"

echo
echo "-------------------------------------------------------------------"
echo " 4. 检查命令名冲突（同名命令被两个脚本注册）"
echo "-------------------------------------------------------------------"
echo
for cmd in botname pbot; do
    echo "  命令 \"$cmd\" 的注册处："
    grep -rn "\"$cmd\"" "$SRC/server" --include=*.cpp 2>/dev/null | \
        grep -v "^.*://" | sed 's/^/    /' | head -5
    echo
done

echo
echo "-------------------------------------------------------------------"
echo " 5. 源文件编码检查（顺带查 bug2）"
echo "-------------------------------------------------------------------"
echo
for f in "$SRC/server/game/AI/NpcBots/botcommands.cpp"; do
    if [ -f "$f" ]; then
        bom=$(head -c 3 "$f" | od -An -tx1 | tr -d ' ')
        if [ "$bom" = "efbbbf" ]; then
            echo "  botcommands.cpp : UTF-8 BOM  [正常]"
        else
            echo "  botcommands.cpp : 无BOM      [中文别名会失效]"
        fi
    fi
done

echo
echo "==================================================================="
echo " 诊断完成"
echo "==================================================================="
