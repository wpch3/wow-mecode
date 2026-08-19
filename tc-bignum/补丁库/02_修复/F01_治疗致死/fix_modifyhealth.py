# -*- coding: utf-8 -*-
"""
step24 治疗致死修复 —— 实际改文件的脚本

修 Unit.cpp 里两个函数的 int32 溢出：
    Unit::ModifyHealth   血量超 21.47亿 时任何治疗/掉血 -> 角色暴毙
    Unit::GetHealthGain  同样问题，影响生命偷取（SpellEffects.cpp:1536）

根因：GetHealth()/GetMaxHealth() 返回 uint32（上限 42.9亿），
      但函数内部用 int32（上限 21.47亿）计算。
      实测 (int32)4100000000 = -194967296

修法：签名不动，只把内部计算提升到 int64。
      不改签名是因为 Unit.cpp DealHeal 里有
          sScriptMgr->OnHeal(healer, victim, (uint32&)gain);
      改返回类型会让这个引用强转出错。

用法：
    python fix_modifyhealth.py <Unit.cpp路径>

自动识别 LF / CRLF，保留原行尾。可重复执行。
"""
import sys
import os

MODIFY_HEALTH_OLD = '''int32 Unit::ModifyHealth(int32 dVal)
{
    int32 gain = 0;

    if (dVal == 0)
        return 0;

    int32 curHealth = (int32)GetHealth();

    int32 val = dVal + curHealth;
    if (val <= 0)
    {
        SetHealth(0);
        return -curHealth;
    }

    int32 maxHealth = (int32)GetMaxHealth();

    if (val < maxHealth)
    {
        SetHealth(val);
        gain = val - curHealth;
    }
    else if (curHealth != maxHealth)
    {
        SetHealth(maxHealth);
        gain = maxHealth - curHealth;
    }

    return gain;
}'''

MODIFY_HEALTH_NEW = '''int32 Unit::ModifyHealth(int32 dVal)
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
    // 改返回类型会让那个引用强转出错。内部 int64 已足够。
    int64 gain = 0;

    if (dVal == 0)
        return 0;

    int64 curHealth = (int64)GetHealth();

    int64 val = (int64)dVal + curHealth;
    if (val <= 0)
    {
        SetHealth(0);
        return (int32)(-curHealth);
    }

    int64 maxHealth = (int64)GetMaxHealth();

    if (val < maxHealth)
    {
        SetHealth((uint32)val);
        gain = val - curHealth;
    }
    else if (curHealth != maxHealth)
    {
        SetHealth((uint32)maxHealth);
        gain = maxHealth - curHealth;
    }

    return (int32)gain;
}'''

HEALTH_GAIN_OLD = '''int32 Unit::GetHealthGain(int32 dVal)
{
    int32 gain = 0;

    if (dVal == 0)
        return 0;

    int32 curHealth = (int32)GetHealth();

    int32 val = dVal + curHealth;
    if (val <= 0)
    {
        return -curHealth;
    }

    int32 maxHealth = (int32)GetMaxHealth();

    if (val < maxHealth)
        gain = dVal;
    else if (curHealth != maxHealth)
        gain = maxHealth - curHealth;

    return gain;
}'''

HEALTH_GAIN_NEW = '''int32 Unit::GetHealthGain(int32 dVal)
{
    // step24 大数值修复：和 ModifyHealth 同样的 int32 溢出问题。
    // 本函数算"这次治疗实际能回多少"，用于生命偷取
    // （SpellEffects.cpp:1536 吸血效果）。
    // 血量超 21.47亿 时 (int32)GetHealth() 变负 -> 偷取量算错。
    int64 gain = 0;

    if (dVal == 0)
        return 0;

    int64 curHealth = (int64)GetHealth();

    int64 val = (int64)dVal + curHealth;
    if (val <= 0)
    {
        return (int32)(-curHealth);
    }

    int64 maxHealth = (int64)GetMaxHealth();

    if (val < maxHealth)
        gain = dVal;
    else if (curHealth != maxHealth)
        gain = maxHealth - curHealth;

    return (int32)gain;
}'''

PATCHES = [
    ("Unit::ModifyHealth",  MODIFY_HEALTH_OLD, MODIFY_HEALTH_NEW,
     "int64 curHealth = (int64)GetHealth();\n\n    int64 val = (int64)dVal + curHealth;\n    if (val <= 0)\n    {\n        SetHealth(0);"),
    ("Unit::GetHealthGain", HEALTH_GAIN_OLD,   HEALTH_GAIN_NEW,
     "int64 curHealth = (int64)GetHealth();\n\n    int64 val = (int64)dVal + curHealth;\n    if (val <= 0)\n    {\n        return (int32)(-curHealth);"),
]


def main():
    if len(sys.argv) < 2:
        print("用法: python fix_modifyhealth.py <Unit.cpp路径>")
        return 1

    path = sys.argv[1]
    if not os.path.isfile(path):
        print("[错误] 找不到文件: %s" % path)
        return 1

    data = open(path, 'rb').read()
    changed = 0
    skipped = 0

    for name, old_s, new_s, marker_s in PATCHES:
        old_lf = old_s.encode('utf-8')
        new_lf = new_s.encode('utf-8')
        marker = marker_s.encode('utf-8')

        old_crlf = old_lf.replace(b'\n', b'\r\n')
        new_crlf = new_lf.replace(b'\n', b'\r\n')
        marker_crlf = marker.replace(b'\n', b'\r\n')

        # 已经改过？
        if marker in data or marker_crlf in data:
            print("  [跳过] %s（已是目标状态）" % name)
            skipped += 1
            continue

        if data.count(old_crlf) == 1:
            data = data.replace(old_crlf, new_crlf)
            print("  [OK] %s 已修复（CRLF）" % name)
            changed += 1
        elif data.count(old_lf) == 1:
            data = data.replace(old_lf, new_lf)
            print("  [OK] %s 已修复（LF）" % name)
            changed += 1
        else:
            n1 = data.count(old_crlf)
            n2 = data.count(old_lf)
            print("  [错误] %s 找不到原文（crlf=%d lf=%d）" % (name, n1, n2))
            print("         可能版本不同，请手动检查")
            return 1

    if changed:
        open(path, 'wb').write(data)

    print()
    print("  修改 %d 处，跳过 %d 处" % (changed, skipped))
    return 0


if __name__ == '__main__':
    sys.exit(main())
