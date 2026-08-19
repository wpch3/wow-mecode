#!/bin/bash
# =====================================================================
# toolcheck.sh -- 装机自检清单（Git Bash 专用，只读）
#
# 用途：按 12-装机路线图.md 的六个批次逐项确认装到哪了，
#       并自动检测能检测的部分（.NET / 目录 / 插件 / 字母占用）
#
# 用法：
#     bash toolcheck.sh                          # 只做能自动检测的部分
#     bash toolcheck.sh "/d/魔兽世界"            # 带客户端路径，检测更全
#     bash toolcheck.sh "/d/魔兽世界" "/d/tools" # 再带工具安装目录
#
# 不修改任何文件。
# =====================================================================

set -u

CLIENT="${1:-}"
TOOLDIR="${2:-}"

PASS=0
FAIL=0
SKIP=0

ok ()   { echo "  [OK]   $1"; PASS=$((PASS+1)); }
no ()   { echo "  [--]   $1"; FAIL=$((FAIL+1)); }
skip () { echo "  [??]   $1"; SKIP=$((SKIP+1)); }
note () { echo "         $1"; }

echo "====================================================================="
echo " 装机自检 -- 对照 工具库/12-装机路线图.md"
echo " 时间: $(date '+%Y-%m-%d %H:%M:%S')"
[ -n "$CLIENT" ]  && echo " 客户端: $CLIENT"
[ -n "$TOOLDIR" ] && echo " 工具目录: $TOOLDIR"
echo "====================================================================="

# 在若干位置找一个文件名，找到返回 0
find_tool () {
    local pattern="$1"
    local dirs="$TOOLDIR /c/tools /d/tools /c/Program\ Files /d/"
    for d in $dirs; do
        [ -d "$d" ] || continue
        if find "$d" -maxdepth 4 -iname "$pattern" -print -quit 2>/dev/null | grep -q .; then
            return 0
        fi
    done
    return 1
}

# ---------------------------------------------------------------------
echo ""
echo "---------------------------------------------------------------------"
echo " 批次 0 -- 十分钟零成本"
echo "---------------------------------------------------------------------"

# idTip
if [ -n "$CLIENT" ]; then
    if [ -d "$CLIENT/Interface/AddOns/idTip" ] || [ -d "$CLIENT/Interface/AddOns/idtip" ]; then
        ok "idTip 已装到 Interface\\AddOns\\"
        # 顺手确认不是 retail 版
        toc=$(find "$CLIENT/Interface/AddOns" -maxdepth 2 -iname "idTip.toc" -print -quit 2>/dev/null)
        if [ -n "$toc" ]; then
            iface=$(grep -a -i "^## Interface" "$toc" 2>/dev/null | head -1)
            note "$iface"
            if echo "$iface" | grep -q "30300"; then
                note "-> Interface 30300 = 3.3.5a 正确版本"
            else
                note "-> [!] 不是 30300，可能是 retail 版，进游戏会被禁用"
            fi
        fi
    else
        no "idTip 未装"
        note "用 WotLK 移植版，不要用 CurseForge 的 retail 版"
        note "https://forum.warmane.com/showthread.php?t=431404"
    fi
    # 顺带看看 AddOns 里还有什么
    n=$(ls -1 "$CLIENT/Interface/AddOns" 2>/dev/null | wc -l)
    [ "$n" -gt 0 ] && note "AddOns 目录下共 $n 项"
else
    skip "idTip -- 没给客户端路径，跳过"
fi

# 体检脚本本身
if [ -f "$(dirname "$0")/client_check.sh" ]; then
    ok "client_check.sh 就位"
    if [ -n "$CLIENT" ]; then
        note "跑一下: bash 工具库/tools/client_check.sh \"$CLIENT\""
    fi
else
    no "client_check.sh 找不到"
fi

# ---------------------------------------------------------------------
echo ""
echo "---------------------------------------------------------------------"
echo " 批次 1 -- WoWDatabaseEditor"
echo "---------------------------------------------------------------------"

# .NET 8 Desktop Runtime
if command -v dotnet >/dev/null 2>&1; then
    rt=$(dotnet --list-runtimes 2>/dev/null)
    if echo "$rt" | grep -q "Microsoft.WindowsDesktop.App 8\."; then
        ok ".NET 8 Desktop Runtime 已装"
        echo "$rt" | grep "WindowsDesktop.App 8\." | head -1 | sed 's/^/         /'
    elif echo "$rt" | grep -q "Microsoft.WindowsDesktop.App"; then
        no ".NET Desktop Runtime 有，但不是 8.x"
        echo "$rt" | grep "WindowsDesktop.App" | head -3 | sed 's/^/         /'
        note "WDE 需要 8.x: https://dotnet.microsoft.com/download/dotnet/8.0"
    else
        no ".NET Desktop Runtime 未装（只有 dotnet 命令）"
    fi
else
    skip "dotnet 命令不在 PATH -- 可能装了但没进 PATH，手动确认"
fi

if find_tool "WoWDatabaseEditor*.exe" || find_tool "LoaderAvalonia.exe"; then
    ok "WoWDatabaseEditor 找到了"
else
    skip "WoWDatabaseEditor 没在常见目录找到（可能装在别处）"
    note "直链: https://ci.appveyor.com/api/projects/BAndysc/wowdatabaseeditor/artifacts/WoWDatabaseEditorWindows.zip?branch=master"
fi

note "装完手动确认三项:"
note "  1. core version 选的是 TrinityCore 3.3.5 (WotLK)"
note "  2. SmartAI 编辑器里看到的是下拉菜单不是裸数字"
note "  3. DBC 路径指向 D:\\TC-Build\\bin\\RelWithDebInfo\\dbc\\"

# ---------------------------------------------------------------------
echo ""
echo "---------------------------------------------------------------------"
echo " 批次 2 -- MPQ Editor + listfile"
echo "---------------------------------------------------------------------"

if find_tool "MPQEditor.exe"; then
    ok "MPQ Editor 找到了"
else
    skip "MPQ Editor 没在常见目录找到"
    note "中文版: http://www.zezula.net/download/mpqeditor_cn.zip  (4.0.0.937)"
fi

if find_tool "listfile*.txt" || find_tool "listfiles"; then
    ok "listfile 找到了"
else
    skip "listfile 没找到"
    note "官方包: http://www.zezula.net/download/listfiles.zip  (9.83 MB)"
    note "这是解别人整合包 unknown 文件名的关键"
fi

# 活体测试的痕迹
if [ -n "$CLIENT" ] && [ -d "$CLIENT/Data" ]; then
    echo ""
    note "Data 目录 patch 字母占用:"
    occupied=""
    freelast=""
    for L in 4 5 6 7 8 9 A B C D E F G H I J K L M N O P Q R S T U V W X Y Z; do
        if [ -e "$CLIENT/Data/patch-$L.MPQ" ] || [ -e "$CLIENT/Data/patch-$L.mpq" ]; then
            occupied="$occupied $L"
        else
            freelast="$L"
        fi
    done
    if [ -n "$occupied" ]; then
        note "  已占用:$occupied"
    else
        note "  已占用: (无自定义补丁)"
    fi
    note "  最高空位: patch-$freelast.MPQ"

    # 文件夹型补丁 = exe 打过补丁的铁证
    dirpatch=0
    for d in "$CLIENT/Data"/patch-*.MPQ "$CLIENT/Data"/patch-*.mpq; do
        [ -d "$d" ] && dirpatch=1
    done
    if [ "$dirpatch" -eq 1 ]; then
        ok "发现文件夹型补丁 -> exe 已打过文件夹加载补丁（批次 4 可跳过）"
    fi

    # Cache 提醒
    if [ -d "$CLIENT/Cache" ]; then
        note "  [!] Cache 目录存在 -- 测试补丁前记得删掉"
    fi
fi

# ---------------------------------------------------------------------
echo ""
echo "---------------------------------------------------------------------"
echo " 批次 3 -- WDBX Editor"
echo "---------------------------------------------------------------------"

if find_tool "WDBXEditor.exe"; then
    ok "WDBX Editor 找到了"
else
    skip "WDBX Editor 没找到"
    note "直链: https://ci.appveyor.com/api/projects/majorcyto/wdbxeditor/artifacts/WDBXEditor.zip"
    note "前置: .NET Framework 4.6.1"
fi
note "装完记得看 Tools -> WotLK Item Import（解自定义物品红问号）"

# ---------------------------------------------------------------------
echo ""
echo "---------------------------------------------------------------------"
echo " 批次 4 -- exe 补丁（可选，不是必需）"
echo "---------------------------------------------------------------------"

if [ -n "$CLIENT" ]; then
    n=$(find "$CLIENT" -maxdepth 1 -iname "*.exe" 2>/dev/null | wc -l)
    note "客户端根目录有 $n 个 exe"
    find "$CLIENT" -maxdepth 1 -iname "*.exe" -printf "         %f\n" 2>/dev/null
    if [ "$n" -gt 1 ]; then
        note "-> 多个 exe，可能已有打过补丁的版本"
    fi
else
    skip "没给客户端路径"
fi
note "记住: 原版 exe 本来就能加载 patch-X.MPQ 真文件"
note "      这批只是效率工具，不是入场券（见 00 篇勘误）"

# ---------------------------------------------------------------------
echo ""
echo "---------------------------------------------------------------------"
echo " 批次 5 -- 素材工具（要做新模型新贴图才装）"
echo "---------------------------------------------------------------------"

find_tool "WoWModelViewer.exe" && ok "WoW Model Viewer 找到了" \
    || skip "WMV 未装 -- https://github.com/wowmodelviewer/wowmodelviewer/releases (V.0.11.0)"

find_tool "wow.export.exe" && ok "wow.export 找到了" \
    || skip "wow.export 未装 -- https://www.kruithne.net/wow.export/ (0.2.19)"

find_tool "BLPConverter.exe" && ok "BLPConverter 找到了" \
    || skip "BLP 转换工具未装（社区工具，无官方仓库）"

# ---------------------------------------------------------------------
echo ""
echo "---------------------------------------------------------------------"
echo " 批次 6 -- 按需"
echo "---------------------------------------------------------------------"

find_tool "die.exe"   && ok "Detect It Easy 找到了" || skip "DIE 未装（想查加壳时才要）"
find_tool "HxD.exe"   && ok "HxD 找到了"            || skip "HxD 未装（临时看字节）"
find_tool "blender.exe" && ok "Blender 找到了"      || skip "Blender 未装（做模型才要）"
find_tool "noggit.exe"  && ok "Noggit 找到了"       || skip "Noggit 未装（做地图才要，建议缓）"

# ---------------------------------------------------------------------
echo ""
echo "====================================================================="
printf " 自动检测: 通过 %d / 未装 %d / 待人工确认 %d\n" "$PASS" "$FAIL" "$SKIP"
echo "====================================================================="
cat <<'TAIL'

 说明:
   [OK] 检测到了
   [--] 确认没装
   [??] 检测不到，不代表没装（可能装在非常见目录）

 判定标准以 12-装机路线图.md 每批的【验收】小节为准，
 脚本只能检测文件是否存在，检测不了配置对不对。

 关键提醒:
   - 批次 0 和 1 装完就可以回主线做 .emote + .say 了
   - 批次 2-5 属于客户端改造线，不影响主线
   - Keira3 不要装（AzerothCore 专属）

TAIL
