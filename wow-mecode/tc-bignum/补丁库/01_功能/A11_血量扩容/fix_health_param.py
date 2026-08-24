# -*- coding: utf-8 -*-
"""
step25 血量参数扩容 —— 承接 step24

step24 修好了 ModifyHealth 的【内部计算】，但【参数】还是 int32：
    int32 ModifyHealth(int32 val);
                       ^^^^^ 治疗量/回血量本身超 21.47亿 就被截断

用户实测症状：
  · 耐力 4亿（血上限 40亿）时，血量只能【缓慢上升】
  · 涨到 21亿 就停住，再用治疗就死
  · 根本测不到 40亿 满血

根因链（三处）：
  1. Unit.h:929   int32 ModifyHealth(int32 val)      参数 int32
  2. Unit.h:930   int32 GetHealthGain(int32 dVal)    同上
  3. Player.cpp:2109  ModifyHealth(int32(addValue))  回血量强转 int32
     Unit.cpp:6419    ModifyHealth(int32(addhealth)) 治疗量强转 int32
     Unit.cpp:967     ModifyHealth(-(int32)damage)   伤害量强转 int32

修法：
  · 参数与返回值都改 int64
  · 三个调用点去掉 int32 强转
  · DealHeal 里 gain 改 int64，OnHeal 那行用临时 uint32 变量转接
    （不能直接 (uint32&)int64，会读错 4 字节）

用法：
    python fix_health_param.py <TrinityCore根目录>

自动识别 LF/CRLF，保留原行尾，可重复执行。
"""
import sys
import os

# ---------------------------------------------------------------
# 每项 = (文件相对路径, 说明, 旧文本, 新文本, 幂等标记)
# ---------------------------------------------------------------
PATCHES = [
    # ---- 1. Unit.h 两个声明 ----
    (
        "src/server/game/Entities/Unit/Unit.h",
        "Unit.h 声明 ModifyHealth/GetHealthGain 改 int64",
        """        int32 ModifyHealth(int32 val);
        int32 GetHealthGain(int32 dVal);""",
        """        // step25 大数值：参数与返回值提升到 int64。
        // 原为 int32，治疗量/回血量超 21.47亿 会被截断，
        // 导致大血量角色只能缓慢回血、卡在 21亿 上不去。
        int64 ModifyHealth(int64 val);
        int64 GetHealthGain(int64 dVal);""",
        "int64 ModifyHealth(int64 val);",
    ),

    # ---- 2. Unit.cpp ModifyHealth 定义 ----
    (
        "src/server/game/Entities/Unit/Unit.cpp",
        "Unit.cpp ModifyHealth 签名改 int64 + 更新过时注释",
        """int32 Unit::ModifyHealth(int32 dVal)
{
    // step24 大数值修复：内部计算全部提升到 int64
    //
    // 原来是 (int32)GetHealth()，但 GetHealth() 返回 uint32。
    // 血量超 INT32_MAX(21.47亿) 时强转会变【负数】：
    //     (int32)4100000000 = -194967296
    // 于是 val <= 0 成立 -> SetHealth(0) -> 角色直接暴毙。
    //
    // maxHealth 同理：(int32)42亿 = -94967296，导致
    // val < maxHealth 判断失效，小额治疗会把血瞬间顶满，
    // 下一次治疗必死 —— 这就是"打着打着突然死"的来源。
    //
    // 签名保持 int32 不变，因为 DealHeal 里有
    //     sScriptMgr->OnHeal(healer, victim, (uint32&)gain);
    // 改返回类型会让那个引用强转出错。内部 int64 已足够。""",
        """int64 Unit::ModifyHealth(int64 dVal)
{
    // step24 + step25 大数值修复
    //
    // step24：内部计算提升到 int64。
    //   原为 (int32)GetHealth()，而 GetHealth() 返回 uint32。
    //   血量超 INT32_MAX(21.47亿) 时强转变负：
    //       (int32)4100000000 = -194967296
    //   -> val <= 0 -> SetHealth(0) -> 角色暴毙。
    //   maxHealth 同理，导致 val < maxHealth 判断失效，
    //   小额治疗把血瞬间顶满，下次治疗必死。
    //
    // step25：【参数与返回值】也提升到 int64。
    //   只改内部不够 —— 参数 int32 会让治疗量/回血量本身被截断，
    //   表现为大血量角色只能缓慢回血、卡在 21亿 上不去。
    //   DealHeal 里 OnHeal 的 (uint32&)gain 强转已改为临时变量转接。""",
        "int64 Unit::ModifyHealth(int64 dVal)",
    ),
    (
        "src/server/game/Entities/Unit/Unit.cpp",
        "ModifyHealth 内部去掉 int32 回转",
        """    int64 val = (int64)dVal + curHealth;
    if (val <= 0)
    {
        SetHealth(0);
        return (int32)(-curHealth);
    }""",
        """    int64 val = dVal + curHealth;
    if (val <= 0)
    {
        SetHealth(0);
        return -curHealth;
    }""",
        "        return -curHealth;\n    }\n\n    int64 maxHealth = (int64)GetMaxHealth();\n\n    if (val < maxHealth)\n    {\n        SetHealth((uint32)val);",
    ),
    (
        "src/server/game/Entities/Unit/Unit.cpp",
        "ModifyHealth 返回值去掉 int32 回转",
        """        gain = maxHealth - curHealth;
    }

    return (int32)gain;
}

int32 Unit::GetHealthGain(int32 dVal)""",
        """        gain = maxHealth - curHealth;
    }

    return gain;
}

int64 Unit::GetHealthGain(int64 dVal)""",
        "int64 Unit::GetHealthGain(int64 dVal)",
    ),

    # ---- 3. GetHealthGain 内部 ----
    (
        "src/server/game/Entities/Unit/Unit.cpp",
        "GetHealthGain 内部去掉 int32 回转",
        """    int64 val = (int64)dVal + curHealth;
    if (val <= 0)
    {
        return (int32)(-curHealth);
    }""",
        """    int64 val = dVal + curHealth;
    if (val <= 0)
    {
        return -curHealth;
    }""",
        "        return -curHealth;\n    }\n\n    int64 maxHealth = (int64)GetMaxHealth();\n\n    if (val < maxHealth)\n        gain = dVal;",
    ),
    (
        "src/server/game/Entities/Unit/Unit.cpp",
        "GetHealthGain 返回值去掉 int32 回转",
        """    else if (curHealth != maxHealth)
        gain = maxHealth - curHealth;

    return (int32)gain;
}""",
        """    else if (curHealth != maxHealth)
        gain = maxHealth - curHealth;

    return gain;
}""",
        "        gain = maxHealth - curHealth;\n\n    return gain;\n}",
    ),

    # ---- 4. DealHeal：gain 改 int64 + OnHeal 转接 ----
    (
        "src/server/game/Entities/Unit/Unit.cpp",
        "DealHeal gain 改 int64，OnHeal 用临时变量转接",
        """    int32 gain = 0;
    Unit* healer = healInfo.GetHealer();
    Unit* victim = healInfo.GetTarget();
    uint32 addhealth = healInfo.GetHeal();""",
        """    // step25: gain 提升到 int64，治疗量不再被 int32 截断
    int64 gain = 0;
    Unit* healer = healInfo.GetHealer();
    Unit* victim = healInfo.GetTarget();
    uint32 addhealth = healInfo.GetHeal();""",
        "    // step25: gain 提升到 int64",
    ),
    (
        "src/server/game/Entities/Unit/Unit.cpp",
        "DealHeal 去掉治疗量 int32 强转",
        "        gain = victim->ModifyHealth(int32(addhealth));\n\n    // Hook for OnHeal Event\n    sScriptMgr->OnHeal(healer, victim, (uint32&)gain);",
        """        gain = victim->ModifyHealth(int64(addhealth));

    // Hook for OnHeal Event
    // step25: gain 现在是 int64，不能直接 (uint32&) 强转引用
    //         （会按小端只读低 4 字节，大数值下读出错误值）
    //         用临时 uint32 转接，回写时钳到 uint32 范围。
    uint32 gainForHook = (gain > 0)
        ? uint32(std::min<int64>(gain, std::numeric_limits<uint32>::max()))
        : 0u;
    sScriptMgr->OnHeal(healer, victim, gainForHook);
    gain = int64(gainForHook);""",
        "uint32 gainForHook = (gain > 0)",
    ),

    # ---- 5. 调用点：去掉 int32 截断 ----
    (
        "src/server/game/Entities/Player/Player.cpp",
        "Player.cpp:2109 生命回复去掉 int32 截断",
        "    ModifyHealth(int32(addValue));",
        "    ModifyHealth(int64(addValue));   // step25: 大血量下 int32 会截断",
        "    ModifyHealth(int64(addValue));",
    ),
    (
        "src/server/game/Entities/Unit/Unit.cpp",
        "Unit.cpp:967 伤害去掉 int32 截断",
        "        victim->ModifyHealth(-(int32)damage);",
        "        victim->ModifyHealth(-(int64)damage);   // step25",
        "        victim->ModifyHealth(-(int64)damage);",
    ),
]

# 需要确保 include 的头（DealHeal 用到 std::min / numeric_limits）
INCLUDE_FILE = "src/server/game/Entities/Unit/Unit.cpp"
INCLUDE_NEEDED = [
    ("#include <limits>", "#include <limits>"),
]


def apply_one(root, relpath, desc, old_s, new_s, marker_s):
    path = os.path.join(root, relpath)
    if not os.path.isfile(path):
        print("  [错误] 找不到 %s" % relpath)
        return None

    data = open(path, 'rb').read()

    old_lf = old_s.encode('utf-8')
    new_lf = new_s.encode('utf-8')
    marker = marker_s.encode('utf-8')

    old_crlf = old_lf.replace(b'\n', b'\r\n')
    new_crlf = new_lf.replace(b'\n', b'\r\n')
    marker_crlf = marker.replace(b'\n', b'\r\n')

    if marker in data or marker_crlf in data:
        print("  [跳过] %s" % desc)
        return 'skip'

    if data.count(old_crlf) == 1:
        data = data.replace(old_crlf, new_crlf)
        open(path, 'wb').write(data)
        print("  [OK] %s（CRLF）" % desc)
        return 'ok'
    if data.count(old_lf) == 1:
        data = data.replace(old_lf, new_lf)
        open(path, 'wb').write(data)
        print("  [OK] %s（LF）" % desc)
        return 'ok'

    print("  [错误] %s —— 找不到原文（crlf=%d lf=%d）"
          % (desc, data.count(old_crlf), data.count(old_lf)))
    return None


def ensure_include(root):
    path = os.path.join(root, INCLUDE_FILE)
    data = open(path, 'rb').read()
    if b'#include <limits>' in data:
        print("  [跳过] <limits> 已 include")
        return 'skip'
    # 插在第一个 #include <algorithm> 之后，没有就插在 #include "Unit.h" 后
    for anchor in (b'#include <algorithm>', b'#include "Unit.h"'):
        for a in (anchor + b'\r\n', anchor + b'\n'):
            if a in data:
                nl = b'\r\n' if a.endswith(b'\r\n') else b'\n'
                data = data.replace(a, a + b'#include <limits>' + nl, 1)
                open(path, 'wb').write(data)
                print("  [OK] 已加 #include <limits>")
                return 'ok'
    print("  [错误] 找不到插入点，请手动在 Unit.cpp 顶部加 #include <limits>")
    return None


def main():
    if len(sys.argv) < 2:
        print("用法: python fix_health_param.py <TrinityCore根目录>")
        return 1

    root = sys.argv[1]
    if not os.path.isdir(root):
        print("[错误] 目录不存在: %s" % root)
        return 1

    ok = skip = 0

    print("[1/2] 应用补丁...")
    for relpath, desc, old_s, new_s, marker_s in PATCHES:
        r = apply_one(root, relpath, desc, old_s, new_s, marker_s)
        if r is None:
            print()
            print("失败，已中止。请检查上面的错误。")
            return 1
        if r == 'ok':
            ok += 1
        else:
            skip += 1

    print()
    print("[2/2] 检查 include...")
    r = ensure_include(root)
    if r is None:
        return 1
    if r == 'ok':
        ok += 1
    else:
        skip += 1

    print()
    print("  修改 %d 处，跳过 %d 处" % (ok, skip))
    return 0


if __name__ == '__main__':
    sys.exit(main())
