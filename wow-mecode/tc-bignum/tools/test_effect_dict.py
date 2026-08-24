#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""效果词典正则的回归测试 —— 改了 CATEGORIES 之后跑一下，防止引入误判"""
import re, sys, importlib.util

spec = importlib.util.spec_from_file_location("bed", "build_effect_dict.py")
bed = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bed)

def cats_of(name):
    return [c for c, a, p, d in bed.CATEGORIES if re.search(p, name, re.IGNORECASE)]

# (物品名, 必须命中, 绝对不能命中)
CASES = [
    # --- 真实误判案例（曾经踩过）---
    ("Hand of Justice",          [],         ["冰霜"]),   # Justice 含 ice
    ("Warp Slicer",              [],         ["冰霜"]),   # Slicer 含 ice
    ("Ancient Voidcaller",       [],         []),          # Void 在词中
    ("Lightweight Boots",        [],         ["神圣"]),   # Lightweight 含 Light
    ("Manacles of Torment",      [],         ["奥术"]),   # Manacles 含 Mana
    ("Warden's Blade",           [],         ["护盾"]),   # Warden 含 Ward
    ("Speedster Gloves",         [],         []),          # Speedster 含 Speed
    ("Mighty Boots",             [],         ["力量"]),   # Mighty 含 Might
    ("Firebrand Sword",          ["火焰"],   []),          # Fire 开头应命中
    ("Sunburn Amulet",           [],         []),          # Sunburn 中的 burn
    # --- 正常应命中 ---
    ("Vampiric Leech Blade",     ["吸血"],   []),
    ("Flamestrike Ring",         ["火焰"],   []),
    ("Frostbite Dagger",         ["冰霜"],   []),
    ("Icy Touch Blade",          ["冰霜"],   []),
    ("Thunderfury",              ["闪电"],   []),
    ("Shadowmourne",             ["暗影"],   []),
    ("Holy Avenger",             ["神圣"],   []),
    ("Divine Protector",         ["神圣"],   []),
    ("Poison Fang",              ["自然"],   []),
    ("Arcane Torrent",           ["奥术"],   []),
    ("Healing Touch Staff",      ["治疗"],   []),
    ("Shield of Valor",          ["护盾"],   []),
    ("Hasty Blade",              [],         []),
    ("Swift Boots",              ["急速"],   []),
    ("Critical Strike Gem",      ["暴击"],   []),
    ("Stunning Blow Mace",       ["眩晕"],   []),
    ("Slowing Poison",           ["减速","自然"], []),
    ("Silence Ring",             ["沉默"],   []),
    ("Summon Felguard",          ["召唤"],   []),
    ("Blink Staff",              ["传送"],   []),
    ("Stealth Cloak",            ["隐身"],   []),
    # --- 中文名 ---
    ("霜之哀伤",                  ["冰霜"],   []),
    ("灰烬使者",                  [],         []),
    ("雷霆之怒",                  ["闪电"],   []),
    ("吸血鬼之牙",                ["吸血"],   []),
    ("神圣壁垒",                  ["神圣","护盾"], []),
]

p = f = 0
fails = []
for name, must, never in CASES:
    got = cats_of(name)
    ok = True
    for m in must:
        if m not in got:
            ok = False; fails.append("  [!!] %-26s 应命中「%s」实际 %s" % (name, m, got or "无"))
    for n in never:
        if n in got:
            ok = False; fails.append("  [!!] %-26s 不应命中「%s」实际 %s" % (name, n, got))
    if ok: p += 1
    else:  f += 1

print("=" * 62)
print(" 效果词典正则回归测试")
print("=" * 62)
for line in fails: print(line)
if not fails: print("  全部通过")
print("-" * 62)
print(" 通过 %d / 失败 %d （共 %d 例）" % (p, f, len(CASES)))
print("=" * 62)
sys.exit(1 if f else 0)
