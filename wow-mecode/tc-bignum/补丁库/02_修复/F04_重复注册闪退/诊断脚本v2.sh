#!/bin/bash
# ============================================================================
#  step47  "Duplicate blank sub-command" 闪退 —— 诊断脚本 v2
# ============================================================================
#
#  v1 的不足：只查了 botname / pbot 两个命令名，
#  但 ASSERT 说的是【任意】命令重名。v2 全量扫描。
#
#  用法（Git Bash）：
#      bash 诊断脚本v2.sh /d/TrinityCore/src
# ============================================================================

SRC="${1:-/d/TrinityCore/src}"
LOADER="$SRC/server/scripts/Commands/cs_script_loader.cpp"

echo "==================================================================="
echo " 诊断 v3  目录: $SRC"
echo "==================================================================="

# ---------------------------------------------------------------------------
#  结论摘要（放最前面，避免被后面几百行淹没）
# ---------------------------------------------------------------------------
echo
echo "###############################################################"
echo "#  结论摘要 —— 先看这里"
echo "###############################################################"
PROBLEMS=0
if [ -f "$LOADER" ]; then
    DUPREG=$(grep -oP 'AddSC_\w+' "$LOADER" | sort | uniq -c | awk '$1!=2 {print "    "$2"  出现 "$1" 次（应为2次）"}')
    if [ -n "$DUPREG" ]; then
        echo
        echo "  [严重] AddSC 注册次数异常 —— 这是启动闪退的头号原因："
        echo "$DUPREG"
        echo
        echo "     修复：打开 cs_script_loader.cpp，让每个 AddSC_xxx"
        echo "           在【声明区】和【函数体】各出现【一次】，删掉多余的。"
        PROBLEMS=$((PROBLEMS+1))
    fi
    FL=$(grep -n "^void AddCommandsScripts" "$LOADER" | cut -d: -f1)
    if [ -n "$FL" ]; then
        INBODY=$(awk -v fl="$FL" 'NR>fl && /^[[:space:]]*void[[:space:]]+AddSC_/ {print "    "NR": "$0}' "$LOADER")
        if [ -n "$INBODY" ]; then
            echo
            echo "  [注意] 有声明被写进了函数体内（不致命，但应清理）："
            echo "$INBODY"
            PROBLEMS=$((PROBLEMS+1))
        fi
    fi
fi
if [ "$PROBLEMS" = "0" ]; then
    echo
    echo "  [未发现致命问题] 注册次数与位置都正常。"
    echo "  若仍闪退，请看下面第 4 节，重点找【同一完整命令路径】重复的情况。"
fi
echo
echo "  提示：第 4 节会列出很多重名，那些【绝大多数是正常的】——"
echo "        不同父命令下的同名子命令（如 .npcbot add 和 .gobject add）互不冲突。"
echo

# ---------------------------------------------------------------------------
echo
echo "### 1. AddSC 注册次数（每个应正好 2 次）"
echo "-------------------------------------------------------------------"
if [ -f "$LOADER" ]; then
    grep -oP 'AddSC_\w+' "$LOADER" | sort | uniq -c | awk '
        $1==2 {printf "  %-45s %s 次  [正常]\n", $2, $1; next}
        {printf "  %-45s %s 次  [!! 异常 !!]\n", $2, $1}
    ' | grep -v "正常" || echo "  全部正常"
else
    echo "  [错误] 找不到 $LOADER"
fi

# ---------------------------------------------------------------------------
echo
echo "### 2.【关键】声明是否被误写进函数体内"
echo "-------------------------------------------------------------------"
echo "  说明：'void AddSC_xxx();' 必须在 AddCommandsScripts() 【之前】。"
echo "        写进函数体里 C++ 也能编译，但那是【局部声明】不是调用，"
echo "        会导致脚本【根本没被注册】。"
echo
if [ -f "$LOADER" ]; then
    FN_LINE=$(grep -n "^void AddCommandsScripts" "$LOADER" | cut -d: -f1)
    if [ -n "$FN_LINE" ]; then
        echo "  AddCommandsScripts() 起始行: $FN_LINE"
        echo
        BAD=$(awk -v fl="$FN_LINE" 'NR>fl && /^[[:space:]]*void[[:space:]]+AddSC_/ {print NR": "$0}' "$LOADER")
        if [ -n "$BAD" ]; then
            echo "  [!! 问题 !!] 下面这些【声明】写在了函数体内部："
            echo "$BAD" | sed 's/^/      /'
            echo
            echo "      修复：把它们【移到】第 $FN_LINE 行【之前】的声明区，"
            echo "            并确认函数体内有对应的【调用】（不带 void、不带分号前缀）。"
        else
            echo "  [正常] 没有声明被写进函数体"
        fi
    fi
fi

# ---------------------------------------------------------------------------
echo
echo "### 3.【关键】函数体内是否真的有调用"
echo "-------------------------------------------------------------------"
if [ -f "$LOADER" ]; then
    FN_LINE=$(grep -n "^void AddCommandsScripts" "$LOADER" | cut -d: -f1)
    for fn in AddSC_botrename_commandscript AddSC_pbot_autoaccept AddSC_playerbot_commandscript; do
        CALL=$(awk -v fl="$FN_LINE" -v f="$fn" \
            'NR>fl && $0 ~ ("^[[:space:]]*"f"\\(\\);") {print NR}' "$LOADER" | head -1)
        if [ -n "$CALL" ]; then
            printf "  %-45s 调用在 %s 行  [正常]\n" "$fn" "$CALL"
        else
            printf "  %-45s [!! 没有调用 !!] 脚本不会生效\n" "$fn"
        fi
    done
fi

# ---------------------------------------------------------------------------
echo
echo "### 4.【核心】全量扫描：顶层命令名是否重复"
echo "-------------------------------------------------------------------"
echo "  ASSERT 'Duplicate blank sub-command' = 同一【完整路径】注册了两次 invoker"
echo "  【重要】下面列出的重名绝大多数是正常的！"
echo "  不同父命令下的同名子命令不冲突（.npcbot add / .gobject add 都合法）。"
echo "  只有当【同一个 AddSC 被调用两次】时才会崩 —— 那个在上面的结论摘要里。"
echo

TMP=$(mktemp)
# 抓所有形如  { "xxx", rbac::...  或  { "xxx", Handle...  的命令注册
grep -rhoP '\{\s*"\K[A-Za-z_][A-Za-z0-9_]*(?="\s*,\s*(rbac::|Handle|&Handle))' \
    "$SRC/server/scripts/Commands/"*.cpp \
    "$SRC/server/scripts/Custom/"*.cpp \
    "$SRC/server/game/AI/NpcBots/"*.cpp 2>/dev/null | sort > "$TMP"

DUP=$(uniq -d "$TMP")
if [ -n "$DUP" ]; then
    echo "  以下命令名出现多次（子命令重名是正常的，重点看【顶层】）："
    for d in $DUP; do
        echo
        echo "    命令 \"$d\":"
        grep -rn "\"$d\"\s*," "$SRC/server/scripts/Commands/"*.cpp \
            "$SRC/server/scripts/Custom/"*.cpp \
            "$SRC/server/game/AI/NpcBots/"*.cpp 2>/dev/null \
            | grep -P '\{\s*"'"$d"'"' | sed 's/^/      /' | head -4
    done
else
    echo "  [正常] 无重复命令名"
fi
rm -f "$TMP"

# ---------------------------------------------------------------------------
echo
echo "### 5. 同一个脚本类是否被定义/new 了两次"
echo "-------------------------------------------------------------------"
for cls in botrename_commandscript playerbot_commandscript pbot_autoaccept_worldscript; do
    N=$(grep -rl "new $cls()" "$SRC/server" --include=*.cpp 2>/dev/null | wc -l)
    printf "  new %-38s 出现在 %s 个文件  " "$cls()" "$N"
    if [ "$N" = "1" ]; then echo "[正常]"
    elif [ "$N" = "0" ]; then echo "[未安装]"
    else echo "[!! 重复 !!]"; fi
done

# ---------------------------------------------------------------------------
echo
echo "### 6. 源文件是否被复制成多份"
echo "-------------------------------------------------------------------"
for f in cs_botrename.cpp cs_playerbot.cpp pbot_autoaccept.cpp pbot_autoaccept.h; do
    N=$(find "$SRC" -name "$f" 2>/dev/null | wc -l)
    printf "  %-26s %s 个  " "$f" "$N"
    if [ "$N" = "1" ]; then echo "[正常]"
    elif [ "$N" = "0" ]; then echo "[未安装]"
    else
        echo "[!! 重复 !!]"
        find "$SRC" -name "$f" 2>/dev/null | sed 's/^/        /'
    fi
done

# ---------------------------------------------------------------------------
echo
echo "### 7. 编码检查（中文别名相关）"
echo "-------------------------------------------------------------------"
for f in "$SRC/server/game/AI/NpcBots/botcommands.cpp" \
         "$SRC/server/scripts/Commands/cs_botrename.cpp" \
         "$SRC/server/scripts/Commands/cs_playerbot.cpp"; do
    [ -f "$f" ] || continue
    NAME=$(basename "$f")
    BOM=$(head -c 3 "$f" | od -An -tx1 | tr -d ' \n')
    HASCN=$(grep -cP '[\x{4e00}-\x{9fff}]' "$f" 2>/dev/null || echo 0)
    printf "  %-22s 中文行数=%-5s " "$NAME" "$HASCN"
    if [ "$BOM" = "efbbbf" ]; then
        echo "UTF-8 BOM  [正常]"
    elif [ "$HASCN" = "0" ]; then
        echo "无BOM(纯ASCII) [无所谓]"
    else
        echo "无BOM      [!! 中文可能乱码/别名失效 !!]"
    fi
done

echo
echo "==================================================================="
echo " 诊断完成"
echo "==================================================================="
