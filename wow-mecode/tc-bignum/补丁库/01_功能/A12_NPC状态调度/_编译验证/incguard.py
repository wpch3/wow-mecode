#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
incguard.py —— include 完整性检查

起因：cs_dummy.cpp 报 C2027「使用了未定义类型 Spell」。
根因：Unit.h:83 里 Spell 只是前向声明(class Spell;)，
      调 cur->GetSpellInfo() 需要完整类型，必须 include Spell.h。
      而 mock.h 里我自己定义了 Spell 类 -> 本地编译通过 -> 成了盲区。

本脚本按【真实源码】核对：凡是对某类型做了 -> 成员访问的，
其完整定义所在头文件必须被 include。
"""
import re, sys, os

SRC = sys.argv[1] if len(sys.argv) > 1 else '../源文件/cs_dummy.cpp'
TC  = '/tmp/tcsrc/src'

# 类型 -> (完整定义所在头, 该头的 include 名)
NEEDS = {
    'Spell':          ('server/game/Spells/Spell.h',            'Spell.h'),
    'SpellInfo':      ('server/game/Spells/SpellInfo.h',        'SpellInfo.h'),
    'Creature':       ('server/game/Entities/Creature/Creature.h','Creature.h'),
    'Player':         ('server/game/Entities/Player/Player.h',  'Player.h'),
    'Unit':           ('server/game/Entities/Unit/Unit.h',      'Unit.h'),
    'TempSummon':     ('server/game/Entities/Creature/TemporarySummon.h','TemporarySummon.h'),
}
# 命名空间函数 -> 所需头
NS_NEEDS = {
    'ObjectAccessor::': 'ObjectAccessor.h',
    'GameTime::':       'GameTime.h',
    'sObjectMgr':       'ObjectMgr.h',
    'sSpellMgr':        'SpellMgr.h',
    'rbac::':           'RBAC.h',
}

src = open(SRC, encoding='utf8').read()
# 去注释，避免注释里的字样误报
body = re.sub(r'/\*.*?\*/', '', src, flags=re.S)
body = re.sub(r'//[^\n]*', '', body)

includes = set(re.findall(r'#include\s+"([^"]+)"', src))

print(f"检查 {os.path.basename(SRC)}")
print(f"  已 include {len(includes)} 个项目头\n")

fail = 0

# 1. 对变量做 -> 成员访问的类型
print("-- 成员访问需要完整类型 --")
for typ, (path, hdr) in NEEDS.items():
    # 形如  Type* x = ... ; x->  或  if (Type* x = ...)
    used = re.search(rf'\b{typ}\s*(?:const\s*)?\*\s*(\w+)\s*=', body)
    if not used:
        continue
    var = used.group(1)
    deref = re.search(rf'\b{var}\s*->', body)
    if not deref:
        continue
    ok = hdr in includes
    print(f"  {typ:<12} 变量 {var:<8} 有 -> 访问   include {hdr:<22} {'OK' if ok else '缺失!'}")
    if not ok:
        fail += 1
        print(f"       完整定义在 {path}")

# 2. 命名空间/全局符号
print("\n-- 命名空间/全局符号 --")
for sym, hdr in NS_NEEDS.items():
    if sym in body:
        ok = hdr in includes
        print(f"  {sym:<20} include {hdr:<22} {'OK' if ok else '缺失!'}")
        if not ok:
            fail += 1

# 3. 反向：确认前向声明陷阱
print("\n-- 前向声明陷阱核对（真实源码）--")
for typ, (path, hdr) in NEEDS.items():
    full = os.path.join(TC, path)
    if not os.path.exists(full):
        continue
    # 在 Unit.h 里是否只是前向声明
    unit_h = os.path.join(TC, 'server/game/Entities/Unit/Unit.h')
    if os.path.exists(unit_h):
        uh = open(unit_h, encoding='utf8', errors='ignore').read()
        if re.search(rf'^class {typ};', uh, re.M):
            print(f"  {typ:<12} 在 Unit.h 里【只是前向声明】-> 必须单独 include {hdr}")

print()
if fail:
    print(f"发现 {fail} 处缺失 include")
    sys.exit(1)
print("include 完整性检查通过")
