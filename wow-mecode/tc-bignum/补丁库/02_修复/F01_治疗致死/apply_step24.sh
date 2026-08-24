#!/bin/bash
# =====================================================================
# step24 治疗致死修复 —— 一键应用（Git Bash 专用）
#
# 用法：在 D:\TrinityCore 根目录开 Git Bash，执行：
#     bash apply_step24.sh
#
# ---------------------------------------------------------------------
# 修的是什么
#
#   血量超过 21.47 亿(INT32_MAX) 后：
#     · 任何治疗 -> 角色暴毙
#     · 任何掉血 -> 角色暴毙
#     · 复活后 maxHealth 仍溢出 -> 再治疗又死
#     · 小额治疗会把血瞬间顶满到 42.9 亿 -> 下次必死
#     · 只有把血量降回上限才能恢复正常
#
# 根因（Unit.cpp  Unit::ModifyHealth / Unit::GetHealthGain）：
#     int32 curHealth = (int32)GetHealth();      <- GetHealth() 是 uint32
#     int32 val = dVal + curHealth;
#     if (val <= 0) { SetHealth(0); ... }        <- 强转成负数就走这里 = 死
#     int32 maxHealth = (int32)GetMaxHealth();   <- 同样强转，判断全乱
#
#   实测：(int32)4100000000 = -194967296
#
# 修法：签名【不动】，只把函数内部计算提升到 int64。
#   为什么不改签名：DealHeal 里有
#       sScriptMgr->OnHeal(healer, victim, (uint32&)gain);
#   改返回类型会让这个引用强转出错。
#   内部用 int64 就足够解决问题，对 28 个调用点【零影响】。
#
# 效果：血量安全线  21.47亿 -> 42.9亿
#       耐力安全线  2.147亿 -> 4.29亿
#
# 42.9亿 是 UNIT_FIELD_HEALTH(uint32) 的物理极限，
# 再往上要改客户端字段布局，留到客户端改造时一起做。
# =====================================================================

set -u

U="src/server/game/Entities/Unit/Unit.cpp"
PYFIX="$(dirname "$0")/fix_modifyhealth.py"

RED=$'\033[0;31m'; GRN=$'\033[0;32m'; YLW=$'\033[1;33m'; NC=$'\033[0m'
ok(){   echo "${GRN}  [OK]${NC} $1"; }
err(){  echo "${RED}  [错误]${NC} $1"; }

echo "================================================"
echo " step24 治疗致死修复"
echo "================================================"
echo

# ---------- 0. 环境检查 ----------
echo "[0/2] 检查环境..."
if [ ! -f "$U" ]; then
    err "找不到 $U"
    err "请在 D:\\TrinityCore 根目录下运行。当前：$(pwd)"
    exit 1
fi
ok "目标文件存在：$U"

if [ ! -f "$PYFIX" ]; then
    err "找不到 $PYFIX"
    err "请确保 fix_modifyhealth.py 和本脚本在同一目录"
    exit 1
fi
ok "修复脚本存在"
echo

# ---------- 1. 应用修复 ----------
echo "[1/2] 修改 Unit.cpp（两个函数）..."
python "$PYFIX" "$U"
if [ $? -ne 0 ]; then
    err "修复失败，见上面的错误"
    exit 1
fi
echo

# ---------- 2. 验证 ----------
echo "[2/2] 验证..."
FAIL=0

check(){
    if grep -q -F "$1" "$U"; then
        ok "$2"
    else
        err "$2 —— 未生效"
        FAIL=1
    fi
}

check 'int64 curHealth = (int64)GetHealth();'    "curHealth 已是 int64"
check 'int64 val = (int64)dVal + curHealth;'     "val 已是 int64"
check 'int64 maxHealth = (int64)GetMaxHealth();' "maxHealth 已是 int64"
check 'SetHealth((uint32)val);'                  "SetHealth 转换正确"

echo
echo "  反向检查（确认无残留）..."
LEFT=$(grep -c 'int32 curHealth = (int32)GetHealth();' "$U" 2>/dev/null || true)
LEFT=${LEFT:-0}
if [ "$LEFT" = "0" ]; then
    ok "无残留的 int32 强转（ModifyHealth + GetHealthGain 都已修）"
else
    err "还有 $LEFT 处残留 int32 强转"
    grep -n 'int32 curHealth = (int32)GetHealth();' "$U"
    FAIL=1
fi

echo
echo "================================================"
if [ "$FAIL" = "0" ]; then
    echo "${GRN} 修复完成！${NC}"
    echo
    echo " 效果："
    echo "   血量安全线  21.47亿 -> ${GRN}42.9亿${NC}"
    echo "   耐力安全线  2.147亿 -> ${GRN}4.29亿${NC}"
    echo
    echo " 下一步：VS2022 重新编译（只改了 Unit.cpp，不用重跑 CMake）"
    echo
    echo " ${YLW}注意${NC}：42.9亿 是 UNIT_FIELD_HEALTH(uint32) 的物理极限，"
    echo "        再往上要改客户端字段布局，留到客户端改造时一起做。"
else
    echo "${RED} 有问题，请检查上面的错误${NC}"
    exit 1
fi
echo "================================================"
