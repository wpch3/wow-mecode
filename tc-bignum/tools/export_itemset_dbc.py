#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ItemSet.dbc 导出工具 —— 自定义套装效果

【什么时候需要】
用 `.item set bind` 借壳（复用暴雪现成套装）时【不需要】这个工具。
只有用 `.item set new` 造了全新套装效果，才需要打这个补丁。

【ItemSet.dbc 结构】（已核实）
    DBCStructure.h:960  struct ItemSetEntry
    DBCfmt.h:84  ItemSetEntryfmt[] =
        "dssssssssssssssssxiiiiiiiiiixxxxxxxiiiiiiiiiiiiiiiiii"

    列       内容                     说明
    0        ID                       套装ID（d=索引）
    1-16     Name[16]                 16 个语言的名字
    17       Name_lang_mask           跳过(x)
    18-27    ItemID[10]               组成部件（可留空，服务端不校验）
    28-34    UnusedItemID[7]          跳过(x)
    35-42    SetSpellID[8]            套装效果法术
    43-50    SetThreshold[8]          需要几件触发
    51       RequiredSkill
    52       RequiredSkillRank

    共 53 列。

【重要发现】
Item.cpp:85-93 的逻辑：
    ++eff->item_count;
    if (set->SetThreshold[x] > eff->item_count) continue;
它【只数穿了几件带这个 setid 的装备】，**不校验 ItemID[] 列表**。
所以 ItemID 那 10 列可以全填 0，照样能触发效果。

【用法】
    python3 export_itemset_dbc.py --sql
    python3 export_itemset_dbc.py --parse 导出.csv --out itemset_add.csv
"""

import argparse
import csv
import io
import sys
from collections import defaultdict

# 3.3.5 客户端语言列顺序（Name[16]）
# 实际只需要填 enUS(0) 和 zhCN(4)，其余留空
LOCALE_ENUS = 0
LOCALE_ZHCN = 4
NUM_LOCALES = 16

MAX_SET_ITEMS = 10
MAX_SET_SPELLS = 8


def print_sql():
    print("=" * 78)
    print(" 第 1 步：在 SQL 客户端执行，结果导出为 CSV")
    print("=" * 78)
    print()
    print("""
SELECT s.setId, s.name, p.idx, p.spellId, p.threshold
FROM world.custom_itemset s
LEFT JOIN world.custom_itemset_spell p ON p.setId = s.setId
ORDER BY s.setId, p.idx;
""".strip())
    print()
    print("  这两张表由 `.item set new` 自动写入。")
    print()
    print("=" * 78)
    print(" 第 2 步：")
    print("   python3 export_itemset_dbc.py --parse 导出.csv --out itemset_add.csv")
    print("=" * 78)


def sniff(header):
    idx = {}
    for i, c in enumerate(header):
        k = c.strip().lower().replace("_", "").replace(" ", "")
        if k == "setid":      idx["setid"] = i
        elif k == "name":     idx["name"] = i
        elif k == "idx":      idx["idx"] = i
        elif k == "spellid":  idx["spell"] = i
        elif k == "threshold":idx["th"] = i
    return idx


def parse_csv(path):
    for enc in ("utf-8-sig", "utf-8", "gbk", "latin-1"):
        try:
            with io.open(path, "r", encoding=enc, newline="") as f:
                sample = f.read(4096)
                f.seek(0)
                try:
                    dialect = csv.Sniffer().sniff(sample, delimiters=",;\t|")
                except Exception:
                    dialect = csv.excel
                data = list(csv.reader(f, dialect))
            if not data:
                continue

            idx = sniff(data[0])
            if "setid" not in idx:
                print("  [x] 没识别出 setId 列，表头: %s" % data[0], file=sys.stderr)
                return {}

            sets = {}
            for r in data[1:]:
                if len(r) <= idx["setid"]:
                    continue
                try:
                    sid = int(str(r[idx["setid"]]).strip())
                except ValueError:
                    continue

                if sid not in sets:
                    nm = ""
                    if "name" in idx and len(r) > idx["name"]:
                        nm = str(r[idx["name"]]).strip()
                    sets[sid] = {"name": nm, "effs": []}

                def gi(k):
                    if k not in idx or len(r) <= idx[k]:
                        return 0
                    v = str(r[idx[k]]).strip()
                    if v == "" or v.upper() == "NULL":
                        return 0
                    try:
                        return int(float(v))
                    except ValueError:
                        return 0

                sp = gi("spell")
                th = gi("th")
                if sp > 0:
                    sets[sid]["effs"].append((gi("idx"), sp, th))

            print("  [i] 用编码 %s 读取，%d 个套装" % (enc, len(sets)))
            return sets
        except UnicodeDecodeError:
            continue
        except FileNotFoundError:
            print("  [x] 找不到文件: %s" % path, file=sys.stderr)
            return {}
    print("  [x] 所有编码都读不了", file=sys.stderr)
    return {}


def validate(sets):
    warns = []
    for sid, d in sets.items():
        if not d["name"]:
            warns.append("setId %d: 没有名字" % sid)
        if not d["effs"]:
            warns.append("setId %d: 没有任何效果，做出来也没用" % sid)
        if len(d["effs"]) > MAX_SET_SPELLS:
            warns.append("setId %d: 效果 %d 个，超过上限 %d，多余的会被丢弃"
                         % (sid, len(d["effs"]), MAX_SET_SPELLS))
            d["effs"] = d["effs"][:MAX_SET_SPELLS]

        # 门槛检查
        for (i, sp, th) in d["effs"]:
            if th == 0:
                warns.append("setId %d 效果槽%d: threshold=0，穿0件就触发，通常是填错" % (sid, i))
            if th > MAX_SET_ITEMS:
                warns.append("setId %d 效果槽%d: threshold=%d 超过 %d 件，永远触发不了"
                             % (sid, i, th, MAX_SET_ITEMS))

        # 门槛应递增
        ths = [th for (_, _, th) in sorted(d["effs"])]
        if ths != sorted(ths):
            warns.append("setId %d: 效果门槛没有按件数递增，建议 2/4/6 这样排" % sid)
    return warns, sets


def write_csv(sets, path):
    """输出 53 列的 WDBX 导入文件"""
    header = ["ID"]
    for i in range(NUM_LOCALES):
        header.append("Name_Lang_%d" % i)
    header.append("Name_Lang_Mask")
    for i in range(MAX_SET_ITEMS):
        header.append("ItemID_%d" % (i + 1))
    for i in range(7):
        header.append("Unused_%d" % (i + 1))
    for i in range(MAX_SET_SPELLS):
        header.append("SetSpellID_%d" % (i + 1))
    for i in range(MAX_SET_SPELLS):
        header.append("SetThreshold_%d" % (i + 1))
    header.append("RequiredSkill")
    header.append("RequiredSkillRank")

    assert len(header) == 53, "列数应为 53，实际 %d" % len(header)

    with io.open(path, "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f)
        w.writerow(header)

        for sid in sorted(sets):
            d = sets[sid]
            row = [sid]

            # 名字：enUS 和 zhCN 都填，其余留空
            for i in range(NUM_LOCALES):
                if i in (LOCALE_ENUS, LOCALE_ZHCN):
                    row.append(d["name"])
                else:
                    row.append("")
            row.append(16712190)     # Name_lang_mask，用官方常见值

            # ItemID[10] 全 0
            # 已核实 Item.cpp:85 只数件数，不校验这个列表
            row.extend([0] * MAX_SET_ITEMS)
            row.extend([0] * 7)      # Unused

            effs = sorted(d["effs"])
            spells = [0] * MAX_SET_SPELLS
            thres  = [0] * MAX_SET_SPELLS
            for n, (_, sp, th) in enumerate(effs[:MAX_SET_SPELLS]):
                spells[n] = sp
                thres[n]  = th
            row.extend(spells)
            row.extend(thres)

            row.append(0)            # RequiredSkill
            row.append(0)            # RequiredSkillRank

            assert len(row) == 53
            w.writerow(row)

    print("  [√] 已写出 %s（%d 个套装，53 列）" % (path, len(sets)))


def write_report(sets, warns, path):
    L = []
    L.append("# ItemSet.dbc 补丁清单")
    L.append("")
    L.append("> 自动生成，共 %d 个自定义套装" % len(sets))
    L.append("")
    L.append("## 什么时候需要这个补丁")
    L.append("")
    L.append("| 做法 | 要补丁吗 |")
    L.append("|---|---|")
    L.append("| `.item set bind` **借壳**（用暴雪现成套装） | 不用，立刻生效 |")
    L.append("| `.item set new` **新建**（自定义效果） | 要，就是本清单 |")
    L.append("")
    L.append("---")
    L.append("")
    L.append("## 套装清单")
    L.append("")
    for sid in sorted(sets):
        d = sets[sid]
        L.append("### %s（setId %d）" % (d["name"] or "(无名)", sid))
        L.append("")
        if d["effs"]:
            L.append("| 件数 | 法术ID |")
            L.append("|---|---|")
            for (_, sp, th) in sorted(d["effs"], key=lambda x: x[2]):
                L.append("| %d 件 | %d |" % (th, sp))
        else:
            L.append("*（无效果）*")
        L.append("")

    if warns:
        L.append("## 警告（%d 条）" % len(warns))
        L.append("")
        for w in warns:
            L.append("- %s" % w)
        L.append("")

    L.append("---")
    L.append("")
    L.append("## 怎么用")
    L.append("")
    L.append("### 1. 提取原始 ItemSet.dbc")
    L.append("")
    L.append("用 MPQ Editor 从客户端提取 `DBFilesClient\\ItemSet.dbc`")
    L.append("")
    L.append("### 2. 导入")
    L.append("")
    L.append("WDBX Editor 打开 → `Import CSV` → 选 `itemset_add.csv` → **选「追加」不要「覆盖」**")
    L.append("")
    L.append("### 3. 打包")
    L.append("")
    L.append("```")
    L.append("patch-4.MPQ")
    L.append("  └─ DBFilesClient\\")
    L.append("       ├─ Item.dbc         ← 自造装备（export_item_dbc.py 生成）")
    L.append("       └─ ItemSet.dbc      ← 自定义套装（本工具生成）")
    L.append("```")
    L.append("")
    L.append("**两个 DBC 可以打进同一个补丁包**，一次发完。")
    L.append("")
    L.append("### 4. 服务端也要同一份")
    L.append("")
    L.append("复制到服务端 `dbc/` 目录，重启 worldserver。")
    L.append("**两边必须完全一致**，否则会错乱。")
    L.append("")
    L.append("---")
    L.append("")
    L.append("## 字段说明")
    L.append("")
    L.append("`DBCfmt.h:84` → 53 列")
    L.append("")
    L.append("| 列 | 内容 | 本工具填什么 |")
    L.append("|---|---|---|")
    L.append("| 0 | ID | 套装ID |")
    L.append("| 1-16 | Name[16] | enUS 和 zhCN 填名字，其余留空 |")
    L.append("| 17 | Name_lang_mask | 16712190（官方常见值） |")
    L.append("| 18-27 | ItemID[10] | **全 0** |")
    L.append("| 28-34 | Unused[7] | 全 0 |")
    L.append("| 35-42 | SetSpellID[8] | 你定义的效果法术 |")
    L.append("| 43-50 | SetThreshold[8] | 需要几件触发 |")
    L.append("| 51-52 | RequiredSkill/Rank | 0 |")
    L.append("")
    L.append("### 为什么 ItemID 可以全填 0")
    L.append("")
    L.append("已核实 `Item.cpp:85-93`：")
    L.append("")
    L.append("```cpp")
    L.append("++eff->item_count;")
    L.append("for (uint32 x = 0; x < MAX_ITEM_SET_SPELLS; ++x) {")
    L.append("    if (!set->SetSpellID[x]) continue;")
    L.append("    if (set->SetThreshold[x] > eff->item_count) continue;   // 只比件数")
    L.append("```")
    L.append("")
    L.append("它**只数「穿了几件带这个 setid 的装备」**，从不读 `ItemID[]` 列表。")
    L.append("所以哪些装备属于这个套装，完全由 `item_template.itemset` 决定。")
    L.append("")

    with io.open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(L))
    print("  [√] 已写出 %s" % path)


def main():
    ap = argparse.ArgumentParser(description="ItemSet.dbc 导出工具")
    ap.add_argument("--sql", action="store_true")
    ap.add_argument("--parse", metavar="CSV")
    ap.add_argument("--out", default="itemset_add.csv")
    ap.add_argument("--report", default="ItemSet.dbc补丁清单.md")
    args = ap.parse_args()

    if args.sql or not args.parse:
        print_sql()
        return 0

    print("正在解析 %s ..." % args.parse)
    sets = parse_csv(args.parse)
    if not sets:
        return 1

    warns, sets = validate(sets)
    if warns:
        print()
        print("  发现 %d 条需要注意的：" % len(warns))
        for w in warns[:15]:
            print("    · %s" % w)
        print()

    write_csv(sets, args.out)
    write_report(sets, warns, args.report)

    print()
    print("下一步：WDBX 打开 ItemSet.dbc，导入 %s（选【追加】）" % args.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
