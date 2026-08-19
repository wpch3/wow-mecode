#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Item.dbc 导出工具 —— 让自造装备被客户端认识

【为什么需要这个】
3.3.5 的武器类型判定在【客户端】做，读的是客户端 MPQ 里的 Item.dbc。
服务端 item_template 改得再对，客户端 Item.dbc 里没有这个 entry，
就不知道它是单手剑还是双手剑 —— 于是「十字军打击」这类需要特定武器的
技能全部放不出来（普通攻击不受影响，因为普攻是纯服务端判定）。

已验证：WDE 生成的新 entry 装备同样有这个问题，所以不是工具的锅，
是 3.3.5 的硬机制。

【Item.dbc 结构】（已核实）
    DBCStructure.h:862  struct ItemEntry
    DBCfmt.h:76         char constexpr Itemfmt[] = "niiiiiii";

    列  字段                      类型     说明
    0   ID                        n(索引)  物品 entry
    1   ClassID                   int32    2=武器 4=护甲
    2   SubclassID                int32    7=单手剑 4=单手锤 ... 关键！技能判定看这个
    3   SoundOverrideSubclassID   int32    -1 = 用 SubclassID 的默认音效
    4   Material                  int32    1=金属 2=木头 3=液体 ... -1=无
    5   DisplayInfoID             int32    模型 ID
    6   InventoryType             int32    21=主手 13=单手 17=双手
    7   SheatheType               int32    1=双手背后 3=单手腰侧 0=不显示

【用法】
    # 第 1 步：在 SQL 客户端跑这条，导出 CSV
    python3 export_item_dbc.py --sql

    # 第 2 步：把结果存成 CSV，生成 WDBX 可导入的文件
    python3 export_item_dbc.py --parse 你的文件.csv --out item_add.csv

    # 第 3 步：用 WDBX Editor 打开客户端 Item.dbc，导入 item_add.csv，
    #          保存后打包成 MPQ 补丁

【设计原则】
  · 只依赖标准库（你的环境是 Windows + Git Bash）
  · 输出 GBK 兼容
  · 自动校验字段合法性，不合法的直接标出来而不是默默写错
"""

import argparse
import csv
import io
import sys

# ---------------------------------------------------------------------------
# 参考表
# ---------------------------------------------------------------------------
CLASS_NAME = {2: "武器", 4: "护甲", 0: "消耗品", 1: "容器", 7: "材料", 15: "杂项"}

WEAPON_SUB = {
    0: "单手斧", 1: "双手斧", 2: "弓", 3: "枪", 4: "单手锤", 5: "双手锤",
    6: "长柄", 7: "单手剑", 8: "双手剑", 10: "法杖", 13: "拳套",
    14: "杂项", 15: "匕首", 16: "投掷", 18: "弩", 19: "魔杖", 20: "鱼竿",
}

ARMOR_SUB = {
    0: "杂项", 1: "布甲", 2: "皮甲", 3: "锁甲", 4: "板甲",
    5: "小盾", 6: "盾牌", 7: "圣契", 8: "神像", 9: "图腾", 10: "魔印",
}

INV_NAME = {
    0: "非装备", 1: "头", 2: "颈", 3: "肩", 4: "衬衣", 5: "胸", 6: "腰",
    7: "腿", 8: "脚", 9: "腕", 10: "手", 11: "戒指", 12: "饰品",
    13: "单手", 14: "盾", 15: "弓", 16: "披风", 17: "双手", 19: "战袍",
    20: "长袍", 21: "主手", 22: "副手", 23: "副手物品", 25: "投掷", 26: "远程",
}

# 双手武器的 subclass
TWOHAND_SUB = {1, 5, 6, 8, 10}
# 远程武器的 subclass
RANGED_SUB = {2, 3, 16, 18, 19}


def print_sql():
    print("=" * 78)
    print(" 第 1 步：在 SQL 客户端执行下面这条，结果导出为 CSV")
    print("=" * 78)
    print()
    print("""
SELECT entry, class, subclass, SoundOverrideSubclass, Material,
       displayid, InventoryType, sheath, name
FROM world.item_template
WHERE entry BETWEEN 800000 AND 899999
ORDER BY entry;
""".strip())
    print()
    print("  说明：只导出自造装备段（800000-899999）。")
    print("        最后的 name 列只是给你看的，不会写进 DBC。")
    print()
    print("=" * 78)
    print(" 第 2 步：")
    print("   python3 export_item_dbc.py --parse 你的文件.csv --out item_add.csv")
    print("=" * 78)


def sniff_columns(header):
    """智能识别列名"""
    idx = {}
    for i, col in enumerate(header):
        c = col.strip().lower().replace("_", "").replace(" ", "")
        if c == "entry":                       idx["entry"] = i
        elif c == "class":                     idx["cls"] = i
        elif c == "subclass":                  idx["sub"] = i
        elif c in ("soundoverridesubclass",):  idx["sound"] = i
        elif c == "material":                  idx["mat"] = i
        elif c in ("displayid", "displayinfoid"): idx["disp"] = i
        elif c == "inventorytype":             idx["inv"] = i
        elif c in ("sheath", "sheathtype"):    idx["sheath"] = i
        elif c == "name":                      idx["name"] = i
    return idx


def parse_csv(path):
    """读 CSV，多编码尝试"""
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

            idx = sniff_columns(data[0])
            need = ["entry", "cls", "sub", "inv"]
            missing = [n for n in need if n not in idx]
            if missing:
                print("  [x] CSV 缺少必要列: %s" % missing, file=sys.stderr)
                print("      表头是: %s" % data[0], file=sys.stderr)
                return []

            rows = []
            for r in data[1:]:
                if len(r) <= idx["entry"]:
                    continue
                def get(k, default=0):
                    if k not in idx or len(r) <= idx[k]:
                        return default
                    v = str(r[idx[k]]).strip()
                    if v == "" or v.upper() == "NULL":
                        return default
                    try:
                        return int(float(v))
                    except ValueError:
                        return default

                try:
                    entry = int(str(r[idx["entry"]]).strip())
                except ValueError:
                    continue

                rows.append({
                    "entry":  entry,
                    "cls":    get("cls"),
                    "sub":    get("sub"),
                    "sound":  get("sound", -1),
                    "mat":    get("mat", -1),
                    "disp":   get("disp"),
                    "inv":    get("inv"),
                    "sheath": get("sheath"),
                    "name":   (r[idx["name"]].strip() if "name" in idx and len(r) > idx["name"] else ""),
                })

            print("  [i] 用编码 %s 读取，%d 条记录" % (enc, len(rows)))
            return rows
        except UnicodeDecodeError:
            continue
        except FileNotFoundError:
            print("  [x] 找不到文件: %s" % path, file=sys.stderr)
            return []
    print("  [x] 所有编码都读不了", file=sys.stderr)
    return []


def validate(rows):
    """校验字段合法性，返回 (警告列表, 修正后的行)"""
    warns = []
    for r in rows:
        e = r["entry"]

        # DisplayInfoID 为 0 = 隐形
        if r["disp"] == 0:
            warns.append("entry %d: displayid=0，游戏里会看不见模型" % e)

        # 武器类
        if r["cls"] == 2:
            if r["sub"] not in WEAPON_SUB:
                warns.append("entry %d: subclass=%d 不是合法武器类型" % (e, r["sub"]))

            two = r["sub"] in TWOHAND_SUB
            rng = r["sub"] in RANGED_SUB

            # InventoryType 与 subclass 是否协调
            if not rng:
                if two and r["inv"] not in (17,):
                    warns.append("entry %d: %s 是双手武器，InventoryType 应为 17，当前 %d"
                                 % (e, WEAPON_SUB.get(r["sub"], "?"), r["inv"]))
                if not two and r["inv"] not in (13, 21, 22):
                    warns.append("entry %d: %s 是单手武器，InventoryType 应为 13/21/22，当前 %d"
                                 % (e, WEAPON_SUB.get(r["sub"], "?"), r["inv"]))

                # Sheath 校正
                want_sheath = 1 if two else 3
                if r["sheath"] != want_sheath:
                    warns.append("entry %d: sheath 应为 %d（%s），当前 %d，已自动修正"
                                 % (e, want_sheath, "双手背后" if two else "单手腰侧", r["sheath"]))
                    r["sheath"] = want_sheath

        # 护甲类
        elif r["cls"] == 4:
            if r["sub"] not in ARMOR_SUB:
                warns.append("entry %d: subclass=%d 不是合法护甲类型" % (e, r["sub"]))

    return warns, rows


def write_csv(rows, path):
    """输出 WDBX 可导入的 CSV（严格 8 列，顺序按 Itemfmt）"""
    with io.open(path, "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f)
        w.writerow(["ID", "ClassID", "SubclassID", "SoundOverrideSubclassID",
                    "Material", "DisplayInfoID", "InventoryType", "SheatheType"])
        for r in sorted(rows, key=lambda x: x["entry"]):
            w.writerow([r["entry"], r["cls"], r["sub"], r["sound"],
                        r["mat"], r["disp"], r["inv"], r["sheath"]])
    print("  [√] 已写出 %s（%d 行）" % (path, len(rows)))


def write_report(rows, warns, path):
    """人看的报告"""
    L = []
    L.append("# Item.dbc 补丁清单")
    L.append("")
    L.append("> 自动生成，共 %d 件自造装备" % len(rows))
    L.append("")
    L.append("## 为什么需要这个补丁")
    L.append("")
    L.append("3.3.5 的**武器类型判定在客户端**，读客户端 MPQ 里的 `Item.dbc`。")
    L.append("服务端 `item_template` 改得再对，客户端 `Item.dbc` 里没有这个 entry，")
    L.append("就不知道它是单手剑还是双手剑 —— 需要特定武器的技能全部放不出来。")
    L.append("")
    L.append("**已验证**：WDE 生成的新 entry 同样有此问题，不是工具的锅，是 3.3.5 硬机制。")
    L.append("")
    L.append("| 现象 | 原因 |")
    L.append("|---|---|")
    L.append("| 名字/模型/属性都对 | 走服务端的 `SMSG_ITEM_QUERY_SINGLE_RESPONSE` |")
    L.append("| 普通攻击正常 | 纯服务端判定 |")
    L.append("| **需要武器的技能不行** | **客户端 `Item.dbc` 判定，查不到** |")
    L.append("")
    L.append("---")
    L.append("")
    L.append("## 装备清单")
    L.append("")
    L.append("| entry | 名称 | 类型 | 部位 | 模型 | Sheath |")
    L.append("|---|---|---|---|---|---|")
    for r in sorted(rows, key=lambda x: x["entry"]):
        if r["cls"] == 2:
            tname = WEAPON_SUB.get(r["sub"], "武器?")
        elif r["cls"] == 4:
            tname = ARMOR_SUB.get(r["sub"], "护甲?")
        else:
            tname = CLASS_NAME.get(r["cls"], "class%d" % r["cls"])
        L.append("| %d | %s | %s | %s | %d | %d |" % (
            r["entry"], r["name"][:24] or "-", tname,
            INV_NAME.get(r["inv"], str(r["inv"])), r["disp"], r["sheath"]))
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
    L.append("### 1. 准备工具")
    L.append("")
    L.append("下载 **WDBX Editor**（支持 3.3.5 DBC 读写）")
    L.append("")
    L.append("### 2. 提取原始 Item.dbc")
    L.append("")
    L.append("用 MPQ Editor 从客户端 `Data\\enGB\\patch-enGB-2.MPQ`（或 zhCN 对应文件）")
    L.append("里提取 `DBFilesClient\\Item.dbc`")
    L.append("")
    L.append("### 3. 导入新行")
    L.append("")
    L.append("WDBX 打开 `Item.dbc` → 菜单 `Import CSV` → 选 `item_add.csv` → 选择「追加」")
    L.append("")
    L.append("**注意**：一定选追加/合并，不要选覆盖，否则原版几万件物品会没了。")
    L.append("")
    L.append("### 4. 打包成 MPQ")
    L.append("")
    L.append("```")
    L.append("patch-4.MPQ")
    L.append("  └─ DBFilesClient\\")
    L.append("       └─ Item.dbc        ← 改好的")
    L.append("```")
    L.append("")
    L.append("放进客户端 `Data\\` 目录。")
    L.append("")
    L.append("### 5. 服务端也要同一份")
    L.append("")
    L.append("**服务端和客户端的 DBC 必须完全一致**，否则会出现")
    L.append("「服务端认为是单手剑，客户端认为不存在」的错乱。")
    L.append("")
    L.append("把改好的 `Item.dbc` 复制到服务端的 `dbc/` 目录，重启 worldserver。")
    L.append("")
    L.append("### 6. 验证")
    L.append("")
    L.append("```")
    L.append(".additem <entry>      重新拿一件（旧的先删掉）")
    L.append("穿上，放需要武器的技能")
    L.append("```")
    L.append("")
    L.append("---")
    L.append("")
    L.append("## 字段参考")
    L.append("")
    L.append("`DBCfmt.h:76` → `Itemfmt[] = \"niiiiiii\"`（8 列，首列索引，其余 int32）")
    L.append("")
    L.append("| 列 | 字段 | 说明 |")
    L.append("|---|---|---|")
    L.append("| 0 | ID | 物品 entry |")
    L.append("| 1 | ClassID | 2=武器 4=护甲 |")
    L.append("| 2 | **SubclassID** | **技能判定就看这个**。7=单手剑 4=单手锤 8=双手剑 |")
    L.append("| 3 | SoundOverrideSubclassID | -1 = 用默认音效 |")
    L.append("| 4 | Material | 1=金属 2=木头 3=液体 4=石头 5=肉 6=布 7=皮革 -1=无 |")
    L.append("| 5 | DisplayInfoID | 模型 ID，0 = 看不见 |")
    L.append("| 6 | InventoryType | 21=主手 13=单手 17=双手 |")
    L.append("| 7 | SheatheType | 1=双手背后 3=单手腰侧 0=不显示 |")
    L.append("")

    with io.open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(L))
    print("  [√] 已写出 %s" % path)


def main():
    ap = argparse.ArgumentParser(description="Item.dbc 导出工具")
    ap.add_argument("--sql", action="store_true", help="打印要执行的 SQL")
    ap.add_argument("--parse", metavar="CSV", help="解析导出的 CSV")
    ap.add_argument("--out", default="item_add.csv", help="输出 CSV（给 WDBX 导入）")
    ap.add_argument("--report", default="Item.dbc补丁清单.md", help="输出报告")
    args = ap.parse_args()

    if args.sql or not args.parse:
        print_sql()
        return 0

    print("正在解析 %s ..." % args.parse)
    rows = parse_csv(args.parse)
    if not rows:
        return 1

    warns, rows = validate(rows)
    if warns:
        print()
        print("  发现 %d 条需要注意的地方：" % len(warns))
        for w in warns[:15]:
            print("    · %s" % w)
        if len(warns) > 15:
            print("    ... 还有 %d 条，详见报告" % (len(warns) - 15))
        print()

    write_csv(rows, args.out)
    write_report(rows, warns, args.report)

    print()
    print("下一步：用 WDBX Editor 打开客户端 Item.dbc，导入 %s（选【追加】）" % args.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
