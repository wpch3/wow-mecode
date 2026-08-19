#!/bin/bash
# =====================================================================
# step22 字段扩容 —— 一键应用（Git Bash 专用）
#
# 用法：在 D:\TrinityCore 根目录开 Git Bash，执行：
#     bash apply_step22.sh
#
# 特点：
#   - 直接原地改代码，不用 .patch 文件
#   - 不受 CRLF/LF 影响（python 二进制替换，保留原行尾）
#   - 可重复执行（已改过的自动跳过）
#   - 每处唯一性校验，改完自动验证
#
# !! 执行顺序 !!
#   必须【先跑本脚本 + 重新编译】，再执行 sql/39_widen_columns.sql
#   反过来会崩服（实测：库int + 代码GetUInt16 + 大值 -> ASSERT）
# =====================================================================

set -u

O="src/server/game/Globals/ObjectMgr.cpp"

RED=$'\033[0;31m'; GRN=$'\033[0;32m'; YLW=$'\033[1;33m'; NC=$'\033[0m'
ok(){   echo "${GRN}  [OK]${NC} $1"; }
skip(){ echo "${YLW}  [跳过]${NC} $1"; }
err(){  echo "${RED}  [错误]${NC} $1"; }

echo "================================================"
echo " step22 字段扩容 - 一键应用"
echo "================================================"
echo

# ---------- 0. 环境检查 ----------
echo "[0/3] 检查环境..."
if [ ! -f "$O" ]; then
    err "找不到 $O"
    err "请在 D:\\TrinityCore 根目录下运行。当前：$(pwd)"
    exit 1
fi
ok "目标文件存在：$O"
echo

# ---------- 通用替换函数（固定字符串，不用正则）----------
# $1=文件 $2=原文 $3=新文 $4=名称
repl(){
    local file="$1" from="$2" to="$3" name="$4"
    local n
    # 已经改过？
    if grep -q -F "$to" "$file" 2>/dev/null; then
        skip "$name（已是目标状态）"
        return 0
    fi
    n=$(grep -o -F "$from" "$file" | wc -l | tr -d ' ')
    if [ "$n" = "1" ]; then
        FROM="$from" TO="$to" python -c "
import os,sys
p=sys.argv[1]
d=open(p,'rb').read()
f=os.environ['FROM'].encode(); t=os.environ['TO'].encode()
assert d.count(f)==1, 'count!=1'
open(p,'wb').write(d.replace(f,t))
" "$file" && ok "$name"
    elif [ "$n" = "0" ]; then
        err "$name —— 找不到原文，可能版本不同或已被改过"
        echo "        原文：$from"
        return 1
    else
        err "$name —— 匹配到 $n 处，不唯一，已跳过（需人工处理）"
        return 1
    fi
}

FAIL=0

# ---------- 1. gossip 三张表的读取器 ----------
echo "[1/3] 修改 gossip 读取器（5 处）..."

repl "$O" \
  'uint16 menuId           = fields[0].GetUInt16();' \
  'uint32 menuId           = fields[0].GetUInt32();' \
  "ObjectMgr.cpp:305 locale menuId" || FAIL=1

repl "$O" \
  'uint16 optionId         = fields[1].GetUInt16();' \
  'uint32 optionId         = fields[1].GetUInt32();' \
  "ObjectMgr.cpp:306 locale optionId" || FAIL=1

repl "$O" \
  'gMenu.MenuID = fields[0].GetUInt16();' \
  'gMenu.MenuID = fields[0].GetUInt32();' \
  "ObjectMgr.cpp:9546 gossip_menu.MenuID" || FAIL=1

repl "$O" \
  'gMenuItem.MenuID                = fields[0].GetUInt16();' \
  'gMenuItem.MenuID                = fields[0].GetUInt32();' \
  "ObjectMgr.cpp:9584 gossip_menu_option.MenuID" || FAIL=1

repl "$O" \
  'gMenuItem.OptionID              = fields[1].GetUInt16();' \
  'gMenuItem.OptionID              = fields[1].GetUInt32();' \
  "ObjectMgr.cpp:9585 gossip_menu_option.OptionID" || FAIL=1

echo

# ---------- 2. game_tele.map ----------
echo "[2/3] 修改 game_tele 读取器（1 处）..."

repl "$O" \
  'gt.mapId          = fields[5].GetUInt16();' \
  'gt.mapId          = fields[5].GetUInt32();' \
  "ObjectMgr.cpp:9064 game_tele.map" || FAIL=1

echo

# ---------- 3. 验证 ----------
echo "[3/3] 验证结果..."

check(){
    local pat="$1" name="$2"
    if grep -q -F "$pat" "$O"; then
        ok "$name"
    else
        err "$name —— 未生效"
        FAIL=1
    fi
}

check 'uint32 menuId           = fields[0].GetUInt32();' "locale menuId 已扩容"
check 'uint32 optionId         = fields[1].GetUInt32();' "locale optionId 已扩容"
check 'gMenu.MenuID = fields[0].GetUInt32();'            "gossip_menu.MenuID 已扩容"
check 'gMenuItem.MenuID                = fields[0].GetUInt32();' "gossip_menu_option.MenuID 已扩容"
check 'gMenuItem.OptionID              = fields[1].GetUInt32();' "gossip_menu_option.OptionID 已扩容"
check 'gt.mapId          = fields[5].GetUInt32();'       "game_tele.map 已扩容"

# 反向检查：确认没有残留的窄读取
echo
echo "  反向检查（确认无残留）..."
LEFT=$(grep -n 'gMenu.MenuID = fields\[0\].GetUInt16()\|gMenuItem.MenuID                = fields\[0\].GetUInt16()\|gMenuItem.OptionID              = fields\[1\].GetUInt16()\|gt.mapId          = fields\[5\].GetUInt16()' "$O" | wc -l | tr -d ' ')
if [ "$LEFT" = "0" ]; then
    ok "无残留的 GetUInt16"
else
    err "还有 $LEFT 处残留"
    FAIL=1
fi

echo
echo "================================================"
if [ "$FAIL" = "0" ]; then
    echo "${GRN} 全部完成！${NC}"
    echo
    echo " 下一步："
    echo "   1. VS2022 重新编译（只改了 ObjectMgr.cpp，不用重跑 CMake）"
    echo "   2. 编译成功后，再执行 sql/39_widen_columns.sql"
    echo "   3. ${YLW}顺序不能反${NC} —— 先库后码会崩服"
else
    echo "${RED} 有 $FAIL 处失败，请检查上面的错误${NC}"
    exit 1
fi
echo "================================================"
