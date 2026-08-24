#!/bin/bash
# =============================================================================
#  第 11 步：战斗节奏优化 —— 一键打补丁脚本
#
#  用法（Git Bash 里执行）：
#      cd /d/TrinityCore
#      bash /path/to/step11_apply.sh
#
#  特点：
#    - 用 python 精确字符串替换，不受 CRLF/LF 影响（不像 patch 命令那样挑剔）
#    - 自动备份原文件为 .bak
#    - 幂等：重复执行不会重复插入
#    - 每一步都校验，失败立即停止并说明原因
# =============================================================================

set -e

SRC="src/server/game/Spells"

if [ ! -f "$SRC/Spell.cpp" ]; then
    echo "[错误] 找不到 $SRC/Spell.cpp"
    echo "       请确认你在 D:\\TrinityCore 目录下执行本脚本"
    exit 1
fi

echo "=========================================="
echo "  战斗节奏优化 —— 打补丁"
echo "=========================================="
echo ""

python3 - "$SRC" <<'PYEOF'
# -*- coding: utf-8 -*-
import io, os, sys, shutil

SRC = sys.argv[1]
ok_count = 0
skip_count = 0

def patch(path, name, old, new, idem_key=None):
    """idem_key: 用于判断"是否已打过"的唯一标记串

    关键：Windows 检出是 CRLF，Linux/补丁里写的是 LF。
    做法是先统一成 LF 匹配替换，写回时再还原成原文件的换行风格。
    """
    global ok_count, skip_count
    full = os.path.join(SRC, path)
    raw = io.open(full, encoding='utf-8', newline='').read()

    # 记住原文件用的是什么换行符
    crlf = ('\r\n' in raw)
    s = raw.replace('\r\n', '\n')          # 统一成 LF 再处理

    # 幂等检查
    if idem_key and idem_key.replace('\r\n', '\n') in s:
        print("  [跳过] %-34s (已打过)" % name)
        skip_count += 1
        return

    cnt = s.count(old)
    if cnt == 0:
        print("  [失败] %-34s 找不到锚点" % name)
        print("         你的源码可能与预期版本不同。")
        print("         请手工按 step11_核心补丁.md 修改，或把该文件发给助手核对。")
        sys.exit(1)
    if cnt > 1:
        print("  [失败] %-34s 锚点出现 %d 次，不唯一" % (name, cnt))
        sys.exit(1)

    # 备份（保留原始字节，回滚时原样还原）
    bak = full + '.bak'
    if not os.path.exists(bak):
        shutil.copy2(full, bak)

    s = s.replace(old, new, 1)

    # 还原换行风格
    if crlf:
        s = s.replace('\n', '\r\n')
    io.open(full, 'w', encoding='utf-8', newline='').write(s)
    print("  [完成] %s" % name)
    ok_count += 1


# ---------- Spell.cpp : include ----------
patch('Spell.cpp', 'Spell.cpp  include',
'#include "Containers.h"\n#include "DatabaseEnv.h"',
'#include "Containers.h"\n#include "CustomSpeed.h"\n#include "DatabaseEnv.h"',
idem_key='#include "Containers.h"\n#include "CustomSpeed.h"')

# ---------- Spell.cpp : 读条缩放 ----------
patch('Spell.cpp', 'Spell.cpp  读条缩放',
"""            // calculate cast time (calculated after first CheckCast check to prevent charge counting for first CheckCast fail)
            m_casttime = m_spellInfo->CalcCastTime(this);
        }
        else
            m_casttime = 0; // Set cast time to 0 if .cheat casttime is enabled.""",
"""            // calculate cast time (calculated after first CheckCast check to prevent charge counting for first CheckCast fail)
            m_casttime = m_spellInfo->CalcCastTime(this);
            // ===== [CUSTOM SPEED] 读条缩放 begin =====
            m_casttime = sCustomSpeed->ScaleCastTime(player, m_spellInfo, m_casttime);
            // ===== [CUSTOM SPEED] end =====
        }
        else
            m_casttime = 0; // Set cast time to 0 if .cheat casttime is enabled.""",
idem_key='[CUSTOM SPEED] 读条缩放 begin')

# ---------- Spell.cpp : GCD 缩放 + 急速修复 ----------
patch('Spell.cpp', 'Spell.cpp  GCD缩放+急速修复',
"""        gcd = int32(float(gcd) * m_caster->GetFloatValue(UNIT_MOD_CAST_SPEED));
        RoundToInterval<int32>(gcd, MIN_GCD, MAX_GCD);
    }

    if (gcd)
        m_caster->ToUnit()->GetSpellHistory()->AddGlobalCooldown(m_spellInfo, gcd);""",
"""        gcd = int32(float(gcd) * m_caster->GetFloatValue(UNIT_MOD_CAST_SPEED));
        RoundToInterval<int32>(gcd, MIN_GCD, MAX_GCD);
    }
    // ===== [CUSTOM SPEED] 急速影响近战 GCD（修复原版缺陷）begin =====
    else if (sCustomSpeed->HasteAffectsMelee() && m_spellInfo->StartRecoveryTime == 1500)
    {
        gcd = int32(float(gcd) * m_caster->GetFloatValue(UNIT_MOD_CAST_SPEED));
        RoundToInterval<int32>(gcd, MIN_GCD, MAX_GCD);
    }
    // ===== [CUSTOM SPEED] end =====

    // ===== [CUSTOM SPEED] GCD 缩放 begin =====
    gcd = sCustomSpeed->ScaleGcd(m_caster->ToPlayer(), gcd);
    // ===== [CUSTOM SPEED] end =====

    if (gcd)
        m_caster->ToUnit()->GetSpellHistory()->AddGlobalCooldown(m_spellInfo, gcd);""",
idem_key='[CUSTOM SPEED] GCD 缩放 begin')

# ---------- SpellHistory.cpp : include ----------
patch('SpellHistory.cpp', 'SpellHistory.cpp  include',
'#include "SpellHistory.h"\n#include "DatabaseEnv.h"',
'#include "SpellHistory.h"\n#include "CustomSpeed.h"\n#include "DatabaseEnv.h"',
idem_key='#include "SpellHistory.h"\n#include "CustomSpeed.h"')

# ---------- SpellHistory.cpp : CD 缩放 ----------
patch('SpellHistory.cpp', 'SpellHistory.cpp  CD缩放',
"""        // replace negative cooldowns by 0
        if (cooldown < 0)
            cooldown = 0;""",
"""        // ===== [CUSTOM SPEED] 单技能CD 缩放 begin =====
        sCustomSpeed->ScaleCooldown(GetPlayerOwner(), spellInfo, cooldown, categoryCooldown);
        // ===== [CUSTOM SPEED] end =====

        // replace negative cooldowns by 0
        if (cooldown < 0)
            cooldown = 0;""",
idem_key='[CUSTOM SPEED] 单技能CD 缩放 begin')

print("")
print("  新打 %d 处，跳过 %d 处（已存在）" % (ok_count, skip_count))
PYEOF

echo ""
echo "=========================================="
echo "  补丁完成"
echo "=========================================="
echo ""
echo "  原文件已备份为 *.bak"
echo ""
echo "  还需要手工做 2 件事："
echo ""
echo "  1. 把 CustomSpeed.h 和 CustomSpeed.cpp 复制到："
echo "     $SRC/"
echo ""
echo "  2. （可选）World.cpp 的 LoadConfigSettings() 末尾加一行："
echo "     sCustomSpeed->LoadConfig();"
echo "     并在 World.cpp 顶部加 #include \"CustomSpeed.h\""
echo "     不加也能用，只是改 conf 后要重启而不能 .reload config"
echo ""
echo "  然后：重跑 CMake -> 编译"
echo ""
