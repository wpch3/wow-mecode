#!/bin/bash
# =====================================================================
# client_check.sh -- 整合客户端体检（Git Bash 专用，只读不改文件）
#
# 用途：拿到别人整合好的 3.3.5a 客户端，先搞清三件事
#   1. wow.exe 是不是原版？改过没有？加壳没有？
#   2. Data 里已被占了哪些 patch 字母？我该用哪个空位？
#   3. 有没有 MPQ 是"看不进去"的（无 listfile / 被保护）？
#
# 用法（Git Bash）：
#     bash client_check.sh "/d/魔兽世界"
#     bash client_check.sh "D:/World of Warcraft"
#
# 依赖：md5sum / sha1sum / find / grep / awk  —— Git Bash 全部自带
#       不依赖 bc（Git Bash 没有 bc）
# =====================================================================

set -u

CLIENT="${1:-}"

if [ -z "$CLIENT" ]; then
    echo "用法: bash client_check.sh <客户端根目录>"
    echo "例:   bash client_check.sh \"/d/魔兽世界\""
    exit 1
fi

if [ ! -d "$CLIENT" ]; then
    echo "[错误] 目录不存在: $CLIENT"
    exit 1
fi

# 数字加千分位 + MB，纯 awk 实现，不依赖 bc
fmtsize () {
    awk -v n="$1" 'BEGIN{
        s=sprintf("%d", n); out=""; c=0;
        for(i=length(s); i>=1; i--){
            out=substr(s,i,1) out; c++;
            if(c%3==0 && i>1) out="," out;
        }
        printf "%s 字节 (%.2f MB)", out, n/1048576;
    }'
}

echo "====================================================================="
echo " 客户端体检报告"
echo " 目标: $CLIENT"
echo " 时间: $(date '+%Y-%m-%d %H:%M:%S')"
echo "====================================================================="

# ---------------------------------------------------------------------
# [1/5] exe 身份
# ---------------------------------------------------------------------
echo ""
echo "---------------------------------------------------------------------"
echo " [1/5] 可执行文件清点"
echo "---------------------------------------------------------------------"

# 官方原版 3.3.5.12340 Wow.exe 指纹（deDE/enGB/enUS/esES/esMX/frFR/ruRU 共用同一个文件）
# 来源: https://github.com/anzz1/wow-client-checksums
# 注意：该表【没有 zhCN/zhTW】，中文代理版 exe 与欧美版本就不是同一个文件，
#       所以中文客户端 hash 对不上是【正常现象】，不代表被改过。
OFFICIAL_MD5="45892bdedd0ad70aed4ccd22d9fb5984"
OFFICIAL_SHA1="178f78380affd260cb775d44397ba6b33ac05fdb"

EXE_COUNT=0
while IFS= read -r -d '' exe; do
    EXE_COUNT=$((EXE_COUNT+1))
    name=$(basename "$exe")
    size=$(stat -c %s "$exe" 2>/dev/null || echo 0)
    md5=$(md5sum "$exe" 2>/dev/null | cut -d' ' -f1)
    sha1=$(sha1sum "$exe" 2>/dev/null | cut -d' ' -f1)

    echo ""
    echo "  文件: $name"
    echo "  大小: $(fmtsize "$size")"
    echo "  MD5 : $md5"
    echo "  SHA1: $sha1"

    if [ "$md5" = "$OFFICIAL_MD5" ]; then
        echo "  判定: [原版] 与暴雪官方 3.3.5.12340 逐字节一致"
        echo "        -> 没打过任何补丁。自定义 MPQ 大概率不加载，先做活体测试。"
    else
        echo "  判定: [非欧美原版] 与官方 12340 不一致"
        echo "        中文客户端本来就对不上这个 hash，别急着下结论"
        echo "        -> 定性只能靠第 5 段的活体测试"
    fi
done < <(find "$CLIENT" -maxdepth 1 -iname "*.exe" -print0 2>/dev/null)

if [ "$EXE_COUNT" -eq 0 ]; then
    echo "  [!!] 根目录下没找到 exe，路径可能给错了"
fi

# ---------------------------------------------------------------------
# [2/5] 加壳 / 保护器检测
# ---------------------------------------------------------------------
echo ""
echo "---------------------------------------------------------------------"
echo " [2/5] 加壳 / 保护器特征扫描"
echo "---------------------------------------------------------------------"

# 只用足够长、随机数据里几乎不可能撞上的特征串，避免误报
PACKER_SIGS="UPX0 UPX1 .aspack ASPack Themida WinLicense VMProtect .vmp0 MoleBox PECompact ASProtect Obsidium .MPRESS1 SafeEngine .enigma1 .petite"

while IFS= read -r -d '' exe; do
    name=$(basename "$exe")
    hits=""
    for sig in $PACKER_SIGS; do
        if grep -a -q -F -- "$sig" "$exe" 2>/dev/null; then
            hits="$hits $sig"
        fi
    done
    if [ -z "$hits" ]; then
        echo "  $name : [干净] 未命中已知壳特征"
    else
        echo "  $name : [注意] 命中 ->$hits"
        echo "           用 Detect It Easy (github.com/horsicq/Detect-It-Easy) 复核"
    fi
done < <(find "$CLIENT" -maxdepth 1 -iname "*.exe" -print0 2>/dev/null)

echo ""
echo "  明文串可见度（原版 exe 必有这些串；加壳压缩后会全部消失）："
while IFS= read -r -d '' exe; do
    name=$(basename "$exe")
    n=0
    for s in "World of Warcraft" "WoW.mfil" "DBFilesClient" "GlueXML"; do
        grep -a -q -F -- "$s" "$exe" 2>/dev/null && n=$((n+1))
    done
    if [ "$n" -ge 3 ]; then
        echo "  $name : $n/4 可见 -> [未加壳]，能直接 hex 编辑"
    elif [ "$n" -ge 1 ]; then
        echo "  $name : $n/4 可见 -> 可能改过但没加壳"
    else
        echo "  $name : 0/4 -> [疑似加壳]，hex 编辑会失败，需先脱壳"
    fi
done < <(find "$CLIENT" -maxdepth 1 -iname "*.exe" -print0 2>/dev/null)

# ---------------------------------------------------------------------
# [3/5] Data 目录 MPQ 清点
# ---------------------------------------------------------------------
echo ""
echo "---------------------------------------------------------------------"
echo " [3/5] Data 目录 MPQ 清点"
echo "---------------------------------------------------------------------"

DATA="$CLIENT/Data"
if [ ! -d "$DATA" ]; then
    echo "  [!!] 没有 Data 目录，路径给错了"
else
    echo ""
    echo "  官方基础包（不要动）："
    for f in common.MPQ common-2.MPQ expansion.MPQ lichking.MPQ patch.MPQ patch-2.MPQ patch-3.MPQ; do
        p="$DATA/$f"
        [ -f "$p" ] && printf "    %-18s %s\n" "$f" "$(fmtsize "$(stat -c %s "$p")")"
    done

    echo ""
    echo "  自定义补丁（整合包作者加的，这些就是"很多MPQ"的来源）："
    found_custom=0
    for p in "$DATA"/patch-*.MPQ "$DATA"/patch-*.mpq; do
        [ -f "$p" ] || continue
        f=$(basename "$p")
        case "$f" in
            patch.MPQ|patch-2.MPQ|patch-3.MPQ|patch.mpq|patch-2.mpq|patch-3.mpq) continue ;;
        esac
        printf "    %-18s %s\n" "$f" "$(fmtsize "$(stat -c %s "$p")")"
        found_custom=1
    done
    [ "$found_custom" -eq 0 ] && echo "    （无）"

    echo ""
    echo "  伪装成 MPQ 的【文件夹】："
    found_dir=0
    for d in "$DATA"/patch-*.MPQ "$DATA"/patch-*.mpq; do
        [ -d "$d" ] || continue
        echo "    $(basename "$d")/   <- 是目录不是文件"
        found_dir=1
    done
    if [ "$found_dir" -eq 1 ]; then
        echo "    ==> 有文件夹补丁 = exe 【已打过】文件夹加载补丁，这是好消息"
    else
        echo "    （无）—— 不代表没打补丁，只说明作者没用这个特性"
    fi
fi

# ---------------------------------------------------------------------
# [4/5] 字母占用图
# ---------------------------------------------------------------------
echo ""
echo "---------------------------------------------------------------------"
echo " [4/5] patch 字母占用图 / 你该用哪个空位"
echo "---------------------------------------------------------------------"

LETTERS="4 5 6 7 8 9 A B C D E F G H I J K L M N O P Q R S T U V W X Y Z"

occupy_report () {
    local dir="$1" prefix="$2" label="$3"
    [ -d "$dir" ] || return
    echo ""
    echo "  $label"
    echo "  （优先级从左到右递增，越靠右越能覆盖别人）"
    echo -n "    "
    local free_list=""
    for L in $LETTERS; do
        if [ -e "$dir/${prefix}${L}.MPQ" ] || [ -e "$dir/${prefix}${L}.mpq" ]; then
            echo -n "[$L]"
        else
            echo -n " $L "
            free_list="$free_list $L"
        fi
    done
    echo ""
    echo "    [X]=已占用   X=空闲"
    local pick=""
    for L in $free_list; do pick="$L"; done
    if [ -n "$pick" ]; then
        echo "    ==> 推荐你用: ${prefix}${pick}.MPQ"
    else
        echo "    ==> [!!] 字母全满，需覆盖或让作者的包腾位"
    fi
}

occupy_report "$DATA" "patch-" "Data 目录"

for loc in zhCN zhTW enUS enGB koKR ruRU deDE frFR esES esMX; do
    LD="$DATA/$loc"
    [ -d "$LD" ] || continue
    echo ""
    echo "  >>> 本地化目录 Data/$loc （整体优先级高于 Data）"
    ls "$LD" 2>/dev/null | grep -i '\.mpq$' | sed 's/^/      现有: /'
    occupy_report "$LD" "patch-${loc}-" "Data/$loc 目录"
done

# ---------------------------------------------------------------------
# [5/5] 结论
# ---------------------------------------------------------------------
echo ""
echo "---------------------------------------------------------------------"
echo " [5/5] 下一步"
echo "---------------------------------------------------------------------"
cat <<'TAIL'

  hash 和壳特征只说明"exe 长什么样"，
  说明不了"游戏认不认我的补丁"。后者只能实测。

  照 工具库/11-整合客户端体检与加密应对.md 的【三分钟活体测试】做一遍。

  核心结论提前说：
    - 别人的 MPQ 加没加密，跟你能不能做自己的补丁【无关】
    - 你只需在上面第 4 段推荐的空位新建自己的包
    - 只有当你要"看别人包里有什么"时，加密才成为问题

TAIL

echo "====================================================================="
echo " 体检结束"
echo "====================================================================="
