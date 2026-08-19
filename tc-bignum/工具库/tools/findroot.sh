#!/bin/bash
# =====================================================================
# findroot.sh -- 找出打包 MPQ 时该选哪一层目录（只读）
#
# 用途：解压补丁后，自动找到"能直接看到 Character 的那一层"，
#       也就是 MPQ Editor 里 "Build from directory" 该选的目录。
#
# 这是打包唯一真正会出错的地方，层级错一层游戏完全不认，
# 而且不报错，只表现为"装了没变化"。
#
# 用法：
#     bash findroot.sh "/d/下载/你解压出来的补丁目录"
#
# 只读，不改任何文件。
# =====================================================================

set -u

DIR="${1:-}"
if [ -z "$DIR" ] || [ ! -d "$DIR" ]; then
    echo "用法: bash findroot.sh <补丁解压目录>"
    exit 1
fi

echo "====================================================================="
echo " MPQ 打包根目录定位"
echo " 目标: $DIR"
echo "====================================================================="

# WoW 客户端认识的顶层目录名（MPQ 内部第一层只能是这些）
WOW_TOPDIRS="Character Creature Item Interface World Spells Tileset Textures Environments DBFilesClient Sound XTextures particles Cameras"

found=0
echo ""
echo "---------------------------------------------------------------------"
echo " 扫描结果"
echo "---------------------------------------------------------------------"

# 找所有含有 WoW 顶层目录名的父目录
for top in $WOW_TOPDIRS; do
    while IFS= read -r -d '' hit; do
        parent=$(dirname "$hit")
        # 记录这个 parent 下有哪些 WoW 顶层目录
        if [ -z "${SEEN:-}" ] || ! echo "${SEEN:-}" | grep -qF "|$parent|"; then
            SEEN="${SEEN:-}|$parent|"
            echo ""
            echo "  [候选根目录]"
            echo "    $parent"
            echo ""
            echo "    这一层下面有这些 WoW 目录："
            for t in $WOW_TOPDIRS; do
                if [ -d "$parent/$t" ]; then
                    cnt=$(find "$parent/$t" -type f 2>/dev/null | wc -l)
                    printf "      %-16s (%d 个文件)\n" "$t" "$cnt"
                fi
            done
            found=$((found+1))
        fi
    done < <(find "$DIR" -maxdepth 6 -type d -name "$top" -print0 2>/dev/null)
done

echo ""
echo "---------------------------------------------------------------------"
echo " 结论"
echo "---------------------------------------------------------------------"
echo ""

if [ "$found" -eq 0 ]; then
    echo "  [!!] 没找到 Character / Creature / Item 等 WoW 标准目录"
    echo ""
    echo "  可能原因："
    echo "    1. 补丁本身就是 .MPQ 文件，不用打包，直接改名放 Data 即可"
    echo "    2. 目录结构不标准，需要手动看一下"
    echo "    3. 路径给错了"
    echo ""
    echo "  当前目录下的内容："
    ls -1 "$DIR" 2>/dev/null | head -20 | sed 's/^/      /'
    # 有没有现成的 MPQ
    n_mpq=$(find "$DIR" -maxdepth 2 -iname "*.mpq" 2>/dev/null | wc -l)
    if [ "$n_mpq" -gt 0 ]; then
        echo ""
        echo "  [发现] 目录里有 $n_mpq 个现成的 .MPQ 文件："
        find "$DIR" -maxdepth 2 -iname "*.mpq" -printf "      %f\n" 2>/dev/null
        echo ""
        echo "  ==> 这些是打好的包，【不用自己打包】"
        echo "      改成没被占用的字母（如 patch-Y.MPQ）直接放进 Data 即可"
    fi
elif [ "$found" -eq 1 ]; then
    root=$(echo "$SEEN" | tr '|' '\n' | grep -v '^$' | head -1)
    echo "  找到唯一根目录，MPQ Editor 里就选这个："
    echo ""
    echo "    $root"
    echo ""
    # 转成 Windows 路径方便复制
    # /d/xxx -> D:\xxx（盘符转大写，符合 Windows 习惯）
    win=$(echo "$root" | sed 's|^/\([a-zA-Z]\)/|\U\1\E:\\|; s|/|\\|g')
    echo "  Windows 路径格式（可直接粘贴到 MPQ Editor）："
    echo ""
    echo "    $win"
    echo ""
    echo "  操作："
    echo "    MPQs -> New MPQ -> 命名 patch-Y -> Next"
    echo "    -> 勾 \"Build the MPQ archive from a file or directory\""
    echo "    -> 点 ... 选上面这个目录"
    echo "    -> Game Compatibility 点 Change 改成 World of Warcraft"
    echo "    -> Next -> Next -> Finish"
else
    echo "  找到 $found 个候选，说明补丁按种族/类型分了包。"
    echo ""
    echo "  两个选择："
    echo "    A. 分别打成多个 MPQ（patch-Y / patch-X ...）"
    echo "    B. 【推荐】新建一个空目录，把各包的 Character 合并进去，"
    echo "       Windows 会问是否合并文件夹，选是。然后打成一个包。"
fi

echo ""
echo "====================================================================="
echo " 打包完成后务必自检：重新打开 MPQ，第一层必须直接是 Character"
echo "====================================================================="
