#!/usr/bin/env python3
"""G17VisualDB - 3.3.5.12340 Spell.dbc 法术视觉对照库（铁律 §0.1 的工具化落地）

数据源：Kaev/AzerothcoreDBCToSQL 的 Spell.sql（完整 3.3.5.12340 Spell.dbc 转储，
49839 条），列位用五个已验证的龙类法术校准（42950→7860, 68867→3879,
11426→4302, 31475→4961, 30616→7776 全部命中后视觉列=129）。

用途：为御龙术技能选特效时，禁止凭名字猜——必须用本库核对候选法术的
真实 SpellVisualID，并用反查确认该视觉的其它来源法术（判断视觉强弱）。

用法：
  python g17visualdb.py 42950            # 查法术 → 视觉
  python g17visualdb.py 7860             # 反查视觉 → 使用它的法术（前10个）
  python g17visualdb.py --name 火箭      # 按名字模糊搜索（含视觉ID）
"""
import json
import sys
from pathlib import Path

DB = json.loads((Path(__file__).parent / "data" / "spell_visual_db.json").read_text(encoding="utf-8"))


def lookup_spell(spell_id):
    e = DB.get(str(spell_id))
    if not e:
        return None
    return {"spell": spell_id, "visual": e[0], "name": e[1]}


def lookup_visual(visual_id, limit=10):
    out = [(int(s), e[0], e[1]) for s, e in DB.items() if e[0] == visual_id]
    out.sort()
    return out[:limit]


def search_name(sub, limit=20):
    out = [(int(s), e[0], e[1]) for s, e in DB.items() if sub.lower() in e[1].lower()]
    out.sort()
    return out[:limit]


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if not args:
        print(__doc__)
        return 0
    a = args[0]
    if a.isdigit():
        n = int(a)
        r = lookup_spell(n)
        if r:
            print(f"法术 {n}: {r['name']}  ->  SpellVisualID = {r['visual']}")
            users = lookup_visual(r["visual"])
            print(f"该视觉({r['visual']})的其它来源法术（前10，共{len(users)}）:")
            for s, v, name in users:
                print(f"  {s:6d} v={v:6d} {name}")
        else:
            users = lookup_visual(n)
            if users:
                print(f"visual {n} 被 {len(users)} 个法术使用（前10）:")
                for s, v, name in users:
                    print(f"  {s:6d} v={v:6d} {name}")
            else:
                print(f"{n} 既不是法术ID也不是视觉ID")
    elif len(sys.argv) >= 3 and sys.argv[1] == "--name":
        for s, v, name in search_name(args[1]):
            print(f"  {s:6d} v={v:6d} {name}")
    else:
        print(__doc__)
    return 0


if __name__ == "__main__":
    sys.exit(main())
