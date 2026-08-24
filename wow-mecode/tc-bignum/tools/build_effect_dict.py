#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
效果词典生成器 —— 把「吸血/火球/闪电」这类中文词映射到实际 SpellID

【为什么需要这个】
WDE 里给武器加技能要手填 ItemEffect 的 7 个字段 x 5 组 = 35 个格子：
    SpellID / TriggerType / Charges / PPMRate
    CoolDownMSec / SpellCategoryID / CategoryCoolDownMSec
而且不告诉你哪个该填、填错会怎样。本脚本把这件事变成「查表」。

【两级安全性】
  安全区（safe）  —— 从 item_template 已有的 spellid_1~5 提取
                     这些是暴雪实际用在装备上的，在正式服跑了十几年，铁定不崩
  实验区（exp）   —— 从 spell_template / Spell.dbc 全量筛
                     能找到更多花样，但可能是 BOSS 专用或未完成的，行为可能异常

【用法】
    # 1. 先导出数据（在你的 MySQL 客户端跑，或用 --sql 生成语句）
    python3 build_effect_dict.py --sql          # 打印要执行的 SQL
    # 2. 把查询结果存成 CSV，再喂给本脚本
    python3 build_effect_dict.py --parse safe.csv --out 效果词典.md

【设计原则】
  · 只依赖标准库，不需要装 mysql 驱动（你的环境是 Windows + Git Bash）
  · 输出 GBK 兼容（你的 SQL 客户端以 GBK 读文件）
  · 生成的词典同时给「人看的 md」和「程序读的 tsv」
"""

import argparse
import csv
import io
import os
import re
import sys
from collections import defaultdict

# ---------------------------------------------------------------------------
# 中文关键词 -> 匹配规则
# 用法术名里的特征词来归类。一个法术可以命中多个类别。
# ---------------------------------------------------------------------------
CATEGORIES = [
    # (中文类别, 别名列表, 匹配用的正则(法术名), 说明)
    ("吸血",   ["吸血", "生命偷取", "leech", "drain", "vampiric"],
     r"(吸取生命|生命偷取|吸血|Vampiric|Leech|Drain Life|Lifesteal)",
     "命中时回复生命"),

    ("火焰",   ["火", "火球", "燃烧", "fire", "flame", "burn"],
     r"(火焰|火球|烈焰|燃烧|灼烧|\bFire|Flame|\bBurn|Scorch|Immolat)",
     "火焰系伤害"),

    ("冰霜",   ["冰", "冰霜", "冻结", "frost", "ice", "freeze"],
     r"(冰霜|冰冻|冻结|寒冰|霜|冰|Frost|\bIce\b|Icy|Freez|Chill)",
     "冰霜伤害与减速"),

    ("闪电",   ["电", "闪电", "雷", "lightning", "shock", "thunder"],
     r"(闪电|雷击|雷霆|电击|Lightning|Shock|Thunder|Electr)",
     "自然/闪电伤害"),

    ("暗影",   ["暗影", "暗", "shadow", "void"],
     r"(暗影|黑暗|虚空|Shadow|\bVoid\b|\bDark)",
     "暗影伤害"),

    ("神圣",   ["神圣", "圣光", "holy", "light"],
     r"(神圣|圣光|裁决|Holy|\bLight\b|Divine)",
     "神圣伤害与治疗"),

    ("自然",   ["自然", "毒", "nature", "poison"],
     r"(自然|中毒|剧毒|毒液|Nature|Poison|Venom)",
     "自然伤害与中毒"),

    ("奥术",   ["奥术", "秘法", "arcane"],
     r"(奥术|秘法|Arcane|\bMana\b)",
     "奥术伤害与法力"),

    ("治疗",   ["治疗", "回血", "heal", "restore"],
     r"(治疗|回复|痊愈|\bHeal|Restor|Renew|\bMend)",
     "治疗效果"),

    ("护盾",   ["护盾", "吸收", "shield", "absorb", "barrier"],
     r"(护盾|吸收|壁垒|Shield|Absorb|Barrier|\bWard\b)",
     "伤害吸收"),

    ("急速",   ["急速", "加速", "haste", "speed"],
     r"(急速|加速|迅捷|Haste|\bSpeed\b|Quick|Swift)",
     "攻击/施法速度"),

    ("暴击",   ["暴击", "致命", "crit"],
     r"(暴击|致命一击|Crit|Deadly)",
     "暴击率与暴击伤害"),

    ("力量",   ["力量", "strength"],
     r"(力量|Strength|\bMight\b)",
     "力量属性"),

    ("敏捷",   ["敏捷", "agility"],
     r"(敏捷|Agility|Dexterity)",
     "敏捷属性"),

    ("智力",   ["智力", "intellect"],
     r"(智力|Intellect|Intelligence)",
     "智力属性"),

    ("耐力",   ["耐力", "stamina"],
     r"(耐力|Stamina|Fortitude|Endurance)",
     "耐力属性"),

    ("护甲",   ["护甲", "armor"],
     r"(护甲|Armor|Protection)",
     "护甲值"),

    ("反伤",   ["反伤", "荆棘", "thorns", "retribution"],
     r"(荆棘|反伤|Thorns|Retribution|Reflect)",
     "受击时反弹伤害"),

    ("眩晕",   ["眩晕", "昏迷", "stun"],
     r"(眩晕|昏迷|击晕|\bStun|\bDaze)",
     "控制效果"),

    ("减速",   ["减速", "缓慢", "slow"],
     r"(减速|缓慢|迟缓|\bSlow|Cripple|Hamstring)",
     "移动速度削弱"),

    ("沉默",   ["沉默", "silence"],
     r"(沉默|Silence)",
     "禁止施法"),

    ("召唤",   ["召唤", "summon"],
     r"(召唤|唤出|Summon|Call)",
     "召唤生物"),

    ("传送",   ["传送", "闪现", "teleport", "blink"],
     r"(传送|闪现|瞬移|Teleport|Blink|Port)",
     "位移"),

    ("变形",   ["变形", "变身", "polymorph", "shapeshift"],
     r"(变形|变身|形态|Polymorph|Shapeshift|Transform)",
     "形态变化"),

    ("隐身",   ["隐身", "潜行", "invisible", "stealth"],
     r"(隐身|潜行|隐匿|Invisib|Stealth|Camouflag)",
     "隐蔽效果"),

    ("免疫",   ["免疫", "无敌", "immune", "invulnerab"],
     r"(免疫|无敌|保护|Immun|Invulnerab|Divine Shield)",
     "免疫效果"),
]

# TriggerType 说明 —— 这是 WDE 里最容易填错的字段
TRIGGER_DESC = {
    0: "使用时触发（有装备冷却）—— 主动点击，最常见",
    1: "装备时触发 —— 穿上就一直生效，被动光环用这个",
    2: "命中时几率触发 —— 需要配 PPMRate，武器特效用这个",
    4: "灵魂石专用 —— 别用",
    5: "使用时触发（无装备冷却）—— 消耗品用",
    6: "教学法术 —— 配合 spell_1 的 SPELL_GENERIC_LEARN",
}


def print_sql():
    """打印用户需要在 MySQL 客户端执行的 SQL"""
    print("=" * 78)
    print(" 第 1 步：在你的 SQL 客户端执行下面的查询，结果导出为 CSV")
    print("=" * 78)
    print()
    print("【安全区】—— 暴雪实际用在装备上的法术（推荐）")
    print("-" * 78)
    print("""
SELECT entry, name, spellid_1, spelltrigger_1, spellppmRate_1, spellcooldown_1
FROM world.item_template
WHERE spellid_1 > 0
ORDER BY Quality DESC, ItemLevel DESC
LIMIT 5000;
""".strip())
    print()
    print("  说明：ORDER BY Quality DESC 让橙装紫装排前面，")
    print("        这些是暴雪最用心做的，特效也最好用。")
    print()
    print("【实验区】—— 全量法术（可能有 BOSS 专用/未完成的）")
    print("-" * 78)
    print("""
SELECT entry, spellName FROM world.spell_dbc LIMIT 5000;
""".strip())
    print()
    print("  注意：3.3.5 官方 TrinityCore 通常没有 spell_dbc 表，")
    print("        法术名在客户端 Spell.dbc 里。若没有此表，只用安全区即可。")
    print()
    print("=" * 78)
    print(" 第 2 步：把结果存成 CSV，然后跑：")
    print("   python3 build_effect_dict.py --parse 你的文件.csv --out 效果词典.md")
    print("=" * 78)


def sniff_columns(header):
    """智能识别列名，兼容不同导出格式"""
    idx = {}
    for i, col in enumerate(header):
        c = col.strip().lower().replace("_", "").replace(" ", "")
        if c in ("entry", "itementry", "id"):
            idx.setdefault("entry", i)
        elif c in ("name", "itemname", "spellname"):
            idx.setdefault("name", i)
        elif c in ("spellid1", "spellid", "spell1"):
            idx.setdefault("spellid", i)
        elif c in ("spelltrigger1", "spelltrigger", "trig"):
            idx.setdefault("trigger", i)
        elif c in ("spellppmrate1", "spellppmrate", "ppm"):
            idx.setdefault("ppm", i)
        elif c in ("spellcooldown1", "spellcooldown", "cd"):
            idx.setdefault("cd", i)
        elif c in ("quality",):
            idx.setdefault("quality", i)
        elif c in ("itemlevel", "ilvl"):
            idx.setdefault("ilvl", i)
    return idx


def parse_csv(path):
    """读 CSV，返回记录列表"""
    rows = []
    # 依次尝试几种编码，Windows 导出常见 gbk
    for enc in ("utf-8-sig", "utf-8", "gbk", "latin-1"):
        try:
            with io.open(path, "r", encoding=enc, newline="") as f:
                sample = f.read(4096)
                f.seek(0)
                try:
                    dialect = csv.Sniffer().sniff(sample, delimiters=",;\t|")
                except Exception:
                    dialect = csv.excel
                reader = csv.reader(f, dialect)
                data = list(reader)
            if not data:
                continue
            header = data[0]
            idx = sniff_columns(header)
            if "spellid" not in idx:
                print("  [!] 没识别出 spellid 列，表头是：%s" % header, file=sys.stderr)
                return []
            for r in data[1:]:
                if len(r) <= idx["spellid"]:
                    continue
                try:
                    sid = int(str(r[idx["spellid"]]).strip() or 0)
                except ValueError:
                    continue
                if sid <= 0:
                    continue
                rec = {"spellid": sid}
                for k in ("entry", "name", "trigger", "ppm", "cd", "quality", "ilvl"):
                    if k in idx and len(r) > idx[k]:
                        rec[k] = str(r[idx[k]]).strip()
                rows.append(rec)
            print("  [i] 用编码 %s 读取成功，%d 行有效数据" % (enc, len(rows)))
            return rows
        except UnicodeDecodeError:
            continue
        except FileNotFoundError:
            print("  [x] 找不到文件：%s" % path, file=sys.stderr)
            return []
    print("  [x] 所有编码都读不了：%s" % path, file=sys.stderr)
    return []


def classify(rows):
    """按中文类别归类"""
    buckets = defaultdict(list)
    unmatched = []
    for rec in rows:
        name = rec.get("name", "")
        hit = False
        for cat, aliases, pattern, desc in CATEGORIES:
            if re.search(pattern, name, re.IGNORECASE):
                buckets[cat].append(rec)
                hit = True
        if not hit:
            unmatched.append(rec)
    return buckets, unmatched


def write_md(buckets, unmatched, out_path, level="安全区"):
    """输出人看的 Markdown"""
    lines = []
    lines.append("# 装备效果词典（%s）" % level)
    lines.append("")
    lines.append("> 自动生成，别手改。重新生成：`python3 tools/build_effect_dict.py`")
    lines.append("")
    lines.append("## 怎么用")
    lines.append("")
    lines.append("造装备时不用记 SpellID，查这张表就行：")
    lines.append("")
    lines.append("```")
    lines.append(".item make 武器 单手剑 装等300 力量 暴击 吸血")
    lines.append("                                        ^^^^ 从本表查到对应 SpellID")
    lines.append("```")
    lines.append("")
    lines.append("## TriggerType 速查（最容易填错的字段）")
    lines.append("")
    lines.append("| 值 | 含义 |")
    lines.append("|---|---|")
    for k in sorted(TRIGGER_DESC):
        lines.append("| %d | %s |" % (k, TRIGGER_DESC[k]))
    lines.append("")
    lines.append("**填错的后果**：装备穿上毫无反应，而且不报错，最难查。")
    lines.append("")
    lines.append("---")
    lines.append("")

    total = 0
    for cat, aliases, pattern, desc in CATEGORIES:
        recs = buckets.get(cat, [])
        if not recs:
            continue
        # 去重
        seen = set()
        uniq = []
        for r in recs:
            if r["spellid"] in seen:
                continue
            seen.add(r["spellid"])
            uniq.append(r)
        total += len(uniq)

        lines.append("## %s" % cat)
        lines.append("")
        lines.append("**说明**：%s" % desc)
        lines.append("")
        lines.append("**可用别名**：`%s`" % "` `".join(aliases))
        lines.append("")
        lines.append("| SpellID | 来源物品 | Trigger | PPM | CD(ms) |")
        lines.append("|---|---|---|---|---|")
        for r in uniq[:40]:
            lines.append("| %d | %s | %s | %s | %s |" % (
                r["spellid"],
                r.get("name", "?")[:28],
                r.get("trigger", "-"),
                r.get("ppm", "-"),
                r.get("cd", "-"),
            ))
        if len(uniq) > 40:
            lines.append("")
            lines.append("> 还有 %d 条未列出" % (len(uniq) - 40))
        lines.append("")

    lines.append("---")
    lines.append("")
    lines.append("## 统计")
    lines.append("")
    lines.append("- 已归类：**%d** 条" % total)
    lines.append("- 未归类：%d 条（名字里没有特征词）" % len(unmatched))
    lines.append("")

    with io.open(out_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
    print("  [√] 已写出 %s（%d 条已归类）" % (out_path, total))


def write_tsv(buckets, out_path):
    """输出程序读的 TSV，给 .item make 用"""
    lines = ["# 类别\t别名(逗号分隔)\tSpellID\tTrigger\tPPM\tCD"]
    for cat, aliases, pattern, desc in CATEGORIES:
        recs = buckets.get(cat, [])
        seen = set()
        for r in recs:
            if r["spellid"] in seen:
                continue
            seen.add(r["spellid"])
            lines.append("%s\t%s\t%d\t%s\t%s\t%s" % (
                cat, ",".join(aliases), r["spellid"],
                r.get("trigger", "1"), r.get("ppm", "0"), r.get("cd", "0")))
    with io.open(out_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
    print("  [√] 已写出 %s" % out_path)


def main():
    ap = argparse.ArgumentParser(description="效果词典生成器")
    ap.add_argument("--sql", action="store_true", help="打印要执行的 SQL")
    ap.add_argument("--parse", metavar="CSV", help="解析导出的 CSV")
    ap.add_argument("--out", default="效果词典.md", help="输出 md 路径")
    ap.add_argument("--tsv", default="", help="同时输出 tsv（给程序读）")
    ap.add_argument("--level", default="安全区", help="安全区 / 实验区")
    args = ap.parse_args()

    if args.sql or not args.parse:
        print_sql()
        return 0

    print("正在解析 %s ..." % args.parse)
    rows = parse_csv(args.parse)
    if not rows:
        return 1

    buckets, unmatched = classify(rows)
    write_md(buckets, unmatched, args.out, args.level)
    if args.tsv:
        write_tsv(buckets, args.tsv)
    return 0


if __name__ == "__main__":
    sys.exit(main())
