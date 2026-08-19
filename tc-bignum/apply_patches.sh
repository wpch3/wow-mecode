#!/bin/bash
# =====================================================================
# TrinityCore 3.3.5 大数值改造 —— 一键应用脚本（Git Bash 专用）
#
# 用法：在 D:\TrinityCore 根目录开 Git Bash，执行：
#     bash apply_patches.sh
#
# 特点：
#   - 不需要 .patch 文件，直接原地改代码
#   - 不受 CRLF/LF 行尾影响（实测保持 CRLF 不破坏）
#   - 可重复执行（已改过的自动跳过）
#   - 逐处唯一性校验 + 改完自动验证 9 项
# =====================================================================

set -u

O="src/server/game/Globals/ObjectMgr.cpp"
I="src/server/game/Entities/Item/Item.cpp"
E="src/server/game/LuaEngine/LuaEngine.cpp"

RED=$'\033[0;31m'; GRN=$'\033[0;32m'; YLW=$'\033[1;33m'; NC=$'\033[0m'
ok(){   echo "${GRN}  [OK]${NC} $1"; }
skip(){ echo "${YLW}  [跳过]${NC} $1"; }
err(){  echo "${RED}  [错误]${NC} $1"; }

echo "================================================"
echo " TrinityCore 大数值改造 - 一键应用"
echo "================================================"
echo

# ---------- 0. 环境检查 ----------
echo "[0/4] 检查环境..."
FAIL=0
for f in "$O" "$I"; do
    [ -f "$f" ] || { err "找不到 $f"; FAIL=1; }
done
if [ ! -f "$E" ]; then
    err "找不到 $E"
    err "Eluna 子模块未初始化：git submodule update --init --recursive"
    FAIL=1
fi
if [ "$FAIL" = "1" ]; then
    echo; err "请在 D:\\TrinityCore 根目录下运行。当前：$(pwd)"; exit 1
fi
ok "三个目标文件都存在"
echo

# ---------- 通用替换函数（全部用固定字符串，不用正则）----------
# $1=文件 $2=原文 $3=新文 $4=名称
repl(){
    local file="$1" from="$2" to="$3" name="$4"
    local n
    n=$(grep -o -F "$from" "$file" | wc -l | tr -d ' ')
    if [ "$n" = "1" ]; then
        # 用 python 做固定字符串替换，避免 sed 的正则转义问题
        FROM="$from" TO="$to" python -c "
import os,io,sys
p=sys.argv[1]
d=open(p,'rb').read()
f=os.environ['FROM'].encode(); t=os.environ['TO'].encode()
assert d.count(f)==1
open(p,'wb').write(d.replace(f,t,1))
" "$file" 2>/dev/null || {
            # 没有 python 就退回 sed（转义特殊字符）
            local ef et
            ef=$(printf '%s' "$from" | sed 's/[[\.*^$()+?{|/]/\\&/g')
            et=$(printf '%s' "$to"   | sed 's/[&/]/\\&/g')
            sed -i "s/$ef/$et/" "$file"
        }
        ok "$name"
        return 0
    elif [ "$n" = "0" ]; then
        if grep -q -F "$to" "$file"; then
            skip "$name（已改过）"
        else
            err "$name 找不到目标代码"
        fi
        return 1
    else
        err "$name 匹配到 $n 处（预期1处），已跳过"
        return 1
    fi
}

# ---------- 1. ObjectMgr.cpp ----------
echo "[1/4] 修改 ObjectMgr.cpp（物品读取器 10 处）..."

repl "$O" "itemTemplate.ItemLevel                 = uint32(fields[15].GetUInt16());" \
          "itemTemplate.ItemLevel                 = fields[15].GetUInt32();" "物品等级 ItemLevel"

repl "$O" "itemTemplate.ItemStat[i].ItemStatValue = int32(fields[29 + i*2].GetInt16());" \
          "itemTemplate.ItemStat[i].ItemStatValue = fields[29 + i*2].GetInt32();" "★属性值 stat_value（核心）"

repl "$O" "itemTemplate.Armor          = uint32(fields[56].GetUInt16());" \
          "itemTemplate.Armor          = fields[56].GetUInt32();" "护甲 Armor"

repl "$O" "itemTemplate.HolyRes        = uint32(fields[57].GetUInt8());" \
          "itemTemplate.HolyRes        = fields[57].GetUInt32();" "神圣抗性"

repl "$O" "itemTemplate.FireRes        = uint32(fields[58].GetUInt8());" \
          "itemTemplate.FireRes        = fields[58].GetUInt32();" "火焰抗性"

repl "$O" "itemTemplate.NatureRes      = uint32(fields[59].GetUInt8());" \
          "itemTemplate.NatureRes      = fields[59].GetUInt32();" "自然抗性"

repl "$O" "itemTemplate.FrostRes       = uint32(fields[60].GetUInt8());" \
          "itemTemplate.FrostRes       = fields[60].GetUInt32();" "冰霜抗性"

repl "$O" "itemTemplate.ShadowRes      = uint32(fields[61].GetUInt8());" \
          "itemTemplate.ShadowRes      = fields[61].GetUInt32();" "暗影抗性"

repl "$O" "itemTemplate.ArcaneRes      = uint32(fields[62].GetUInt8());" \
          "itemTemplate.ArcaneRes      = fields[62].GetUInt32();" "奥术抗性"

repl "$O" "itemTemplate.MaxDurability  = uint32(fields[114].GetUInt16());" \
          "itemTemplate.MaxDurability  = fields[114].GetUInt32();" "最大耐久"
echo

# ---------- 2. Item.cpp ----------
echo "[2/4] 修改 Item.cpp（耐久字段 2 处）..."
repl "$I" "stmt->setUInt16(++index, GetUInt32Value(ITEM_FIELD_DURABILITY));" \
          "stmt->setUInt32(++index, GetUInt32Value(ITEM_FIELD_DURABILITY));" "耐久存盘 setUInt32"
repl "$I" "uint32 durability = fields[8].GetUInt16();" \
          "uint32 durability = fields[8].GetUInt32();" "耐久读盘 GetUInt32"
echo

# ---------- 3. LuaEngine.cpp ----------
echo "[3/4] 修改 LuaEngine.cpp（Eluna uint64 截断 bug）..."
repl "$E" "return static_cast<unsigned long long>(CHECKVAL<uint32>(narg));" \
          "return static_cast<unsigned long long>(CHECKVAL<double>(narg));" "Eluna uint64 修复"
echo

# ---------- 4. 验证 ----------
echo "[4/4] 验证结果..."
echo
PASS=0; TOTAL=0
chk(){
    TOTAL=$((TOTAL+1))
    if grep -q -F "$2" "$1"; then ok "$3"; PASS=$((PASS+1));
    else err "$3  <- 未通过！"; fi
}

chk "$O" "itemTemplate.ItemStat[i].ItemStatValue = fields[29 + i*2].GetInt32();" "★属性值读取器 = GetInt32（最关键）"
chk "$O" "itemTemplate.Armor          = fields[56].GetUInt32();"                 "护甲 = GetUInt32"
chk "$O" "itemTemplate.HolyRes        = fields[57].GetUInt32();"                 "神圣抗性 = GetUInt32"
chk "$O" "itemTemplate.ArcaneRes      = fields[62].GetUInt32();"                 "奥术抗性 = GetUInt32"
chk "$O" "itemTemplate.ItemLevel                 = fields[15].GetUInt32();"      "物品等级 = GetUInt32"
chk "$O" "itemTemplate.MaxDurability  = fields[114].GetUInt32();"                "最大耐久 = GetUInt32"
chk "$I" "stmt->setUInt32(++index, GetUInt32Value(ITEM_FIELD_DURABILITY));"      "耐久存盘 = setUInt32"
chk "$I" "uint32 durability = fields[8].GetUInt32();"                            "耐久读盘 = GetUInt32"
chk "$E" "return static_cast<unsigned long long>(CHECKVAL<double>(narg));"       "Eluna uint64 修复"

echo
echo "================================================"
if [ "$PASS" = "$TOTAL" ]; then
    echo "${GRN} 全部 $TOTAL 项验证通过！${NC}"
    echo "================================================"
    echo
    echo "下一步："
    echo "  1. 查看改动：  git diff --stat"
    echo "  2. 提交存档：  git add -A && git commit -m \"bignum: item loader 32bit\""
    echo "  3. 告诉我完成，我给第 2 步（VS 编译）"
else
    echo "${RED} $PASS/$TOTAL 通过，有失败项！${NC}"
    echo "================================================"
    echo
    echo "请把上面完整输出发我排查。回滚： git checkout -- ."
fi
echo
