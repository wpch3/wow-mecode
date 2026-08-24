#!/bin/bash
# =====================================================================
# m2ver.sh -- M2 模型版本快查（Git Bash 专用，不需要 Python）
#
# 用途：patch_scan.py 的免依赖版。没装 Python 就用这个。
#       只读 M2 文件头的魔数和版本号，判断能不能在 3.3.5 用。
#
# 用法：
#     bash m2ver.sh "/d/下载/你的补丁目录"
#
# 依赖：od / find / head  -- Git Bash 全部自带
# 只读，不改任何文件。
#
# 判定表（wowdev.wiki/M2）：
#     MD20 + 264      = WotLK 3.3.5  可直接用
#     MD20 + 其他     = 别的资料片，要转换
#     MD21            = Legion+ 分块格式，3.3.5 完全不认
# =====================================================================

set -u

DIR="${1:-}"
if [ -z "$DIR" ] || [ ! -d "$DIR" ]; then
    echo "用法: bash m2ver.sh <补丁解压目录>"
    exit 1
fi

# 读小端 uint32
le32 () {
    od -An -tu4 -N4 -j"$2" "$1" 2>/dev/null | tr -d ' \n'
}

vername () {
    case "$1" in
        256|257)                 echo "Classic" ;;
        260|261|262|263)         echo "TBC" ;;
        264)                     echo "WotLK 3.3.5" ;;
        265|266|267|268|269|270|271) echo "Cataclysm" ;;
        272)                     echo "MoP/WoD" ;;
        273|274)                 echo "Legion/BfA/SL" ;;
        *)                       echo "未知" ;;
    esac
}

echo "====================================================================="
echo " M2 版本快查"
echo " 目标: $DIR"
echo "====================================================================="

n_total=0; n_264=0; n_other=0; n_md21=0
declare -A vercount
samples=""

while IFS= read -r -d '' f; do
    n_total=$((n_total+1))
    magic=$(head -c 4 "$f" 2>/dev/null | tr -d '\0')
    if [ "$magic" = "MD20" ]; then
        ver=$(le32 "$f" 4)
        [ -z "$ver" ] && ver=0
        key="MD20 v$ver"
        vercount["$key"]=$(( ${vercount["$key"]:-0} + 1 ))
        if [ "$ver" = "264" ]; then
            n_264=$((n_264+1))
        else
            n_other=$((n_other+1))
        fi
        if [ $n_total -le 5 ]; then
            samples="$samples\n    $(basename "$f")   MD20 v$ver  ($(vername "$ver"))"
        fi
    else
        n_md21=$((n_md21+1))
        key="$magic (分块格式)"
        vercount["$key"]=$(( ${vercount["$key"]:-0} + 1 ))
        if [ $n_total -le 5 ]; then
            samples="$samples\n    $(basename "$f")   $magic  <- Legion+分块"
        fi
    fi
done < <(find "$DIR" -type f -iname "*.m2" -print0 2>/dev/null)

# 顺带数一下配套文件
n_skin=$(find "$DIR" -type f -iname "*.skin" 2>/dev/null | wc -l)
n_blp=$(find "$DIR"  -type f -iname "*.blp"  2>/dev/null | wc -l)
n_dbc=$(find "$DIR"  -type f -iname "*.dbc"  2>/dev/null | wc -l)

echo ""
echo "  .m2 $n_total    .skin $n_skin    .blp $n_blp    .dbc $n_dbc"
echo ""

if [ "$n_total" -eq 0 ]; then
    echo "  没找到 .m2 文件 -- 纯贴图补丁，或路径给错了"
    exit 0
fi

echo "  版本分布："
for k in "${!vercount[@]}"; do
    printf "    %-26s %d 个\n" "$k" "${vercount[$k]}"
done

echo ""
echo "  样例："
printf "%b\n" "$samples"

echo ""
echo "---------------------------------------------------------------------"
if [ "$n_md21" -gt 0 ]; then
    echo "  >> 【不能直接用】有 $n_md21 个 Legion+ 分块格式模型"
    echo "     3.3.5 只认 MD20，这些会崩溃或不显示。是原始扒取文件，要转换。"
elif [ "$n_264" -gt 0 ] && [ "$n_other" -eq 0 ]; then
    echo "  >> 【可以直接用】全部 $n_264 个模型都是 MD20 v264 = 原生 WotLK"
    echo "     打包成 MPQ 丢进 Data 就行。"
    if [ "$n_dbc" -eq 0 ]; then
        echo ""
        echo "     [注意] 补丁没带 DBC。如果进游戏人物不变，"
        echo "            可能要改 CharSections.dbc（批次3 WDBX Editor）"
    else
        echo "     补丁自带 $n_dbc 个 DBC，一起放进 MPQ 的 DBFilesClient\\ 目录。"
    fi
elif [ "$n_other" -gt 0 ] && [ "$n_264" -eq 0 ]; then
    echo "  >> 【需要转换】是 MD20 但版本不是 264，还差一步"
else
    echo "  >> 【混合】v264: $n_264 个可用 / 其他版本: $n_other 个要转"
fi
echo "====================================================================="
