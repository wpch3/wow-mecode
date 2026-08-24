# -*- coding: utf-8 -*-
"""
texplan.py -- 贴图类补丁安装方案分析（只读）

用途：补丁以 BLP 贴图为主时，判断：
      1. 贴图是给【原版模型】还是【HD(WoD/Legion)模型】做的  <- 决定成败
      2. 覆盖了哪些种族
      3. 哪些文件是垃圾该删（不存在于 3.3.5 的种族/职业）

用法：
    python texplan.py "D:/r18/ralia-nude-patch"

只读，不改任何文件。
"""

import os
import sys
import struct

# 3.3.5 不存在的种族/职业关键词。含这些的文件在 3.3.5 里永远不会被请求。
NOT_IN_335 = {
    "pandaren":     "熊猫人 (MoP 5.0 才有)",
    "worgen":       "狼人 (Cata 4.0 才有)",
    "goblin":       "哥布林玩家种族 (Cata 4.0 才有)",
    "demonhunter":  "恶魔猎手 (Legion 7.0 才有)",
    "nightborne":   "夜之子 (Legion 7.0 才有)",
    "highmountain": "至高岭牛头人 (Legion 7.0 才有)",
    "lightforged":  "光铸德莱尼 (Legion 7.3 才有)",
    "voidelf":      "虚空精灵 (BfA 8.0 才有)",
    "darkiron":     "黑铁矮人 (BfA 8.0 才有)",
    "maghar":       "玛格汉兽人 (BfA 8.0 才有)",
    "zandalari":    "赞达拉巨魔 (BfA 8.0 才有)",
    "kultiran":     "库尔提拉斯人 (BfA 8.0 才有)",
    "mechagnome":   "机械侏儒 (BfA 8.3 才有)",
    "vulpera":      "沃姆斯 (BfA 8.3 才有)",
    "dracthyr":     "龙希尔 (DF 10.0 才有)",
}

# 3.3.5 的十个种族
RACES_335 = ["human", "orc", "dwarf", "nightelf", "scourge", "undead",
             "tauren", "gnome", "troll", "bloodelf", "draenei", "goblin"]


def why_not_335(name):
    low = name.lower().replace("_", "").replace("-", "")
    for k, v in NOT_IN_335.items():
        if k in low:
            return v
    return None


def read_m2_magic(path):
    try:
        with open(path, "rb") as f:
            h = f.read(8)
            if len(h) < 8:
                return "?", 0
            m = h[0:4].decode("ascii", "replace")
            if m == "MD20":
                return "MD20", struct.unpack("<I", h[4:8])[0]
            return m, 0
    except Exception:
        return "?", 0


def main():
    if len(sys.argv) < 2:
        print("用法: python texplan.py <补丁解压目录>")
        sys.exit(1)
    root = sys.argv[1]
    if not os.path.isdir(root):
        print("[错误] 目录不存在: %s" % root)
        sys.exit(1)

    print("=" * 69)
    print(" 贴图补丁安装方案")
    print(" 目标: %s" % root)
    print("=" * 69)

    m2s, skins, blps = [], [], []
    for dp, _, fs in os.walk(root):
        for fn in fs:
            p = os.path.join(dp, fn)
            low = fn.lower()
            if low.endswith(".m2"):
                m2s.append(p)
            elif low.endswith(".skin"):
                skins.append(p)
            elif low.endswith(".blp"):
                blps.append(p)

    # 相对路径（模拟 MPQ 内部路径）
    def rel(p):
        return os.path.relpath(p, root).replace("/", "\\")

    # -----------------------------------------------------------------
    print("")
    print("---------------------------------------------------------------------")
    print(" [1/4] 模型文件处置建议")
    print("---------------------------------------------------------------------")

    if not m2s:
        print("  没有 M2，是纯贴图补丁")
    else:
        junk, keep = [], []
        for p in m2s:
            magic, ver = read_m2_magic(p)
            reason = why_not_335(os.path.basename(p))
            if reason:
                junk.append((p, magic, ver, reason))
            elif magic != "MD20" or ver != 264:
                keep.append((p, magic, ver, "格式不兼容但属于3.3.5有的种族"))
            else:
                keep.append((p, magic, ver, "可用"))

        if junk:
            print("")
            print("  [可直接删除] 这些是 3.3.5 【根本不存在】的种族/职业，")
            print("               客户端永远不会请求它们，删了毫无影响：")
            print("")
            for p, magic, ver, reason in junk:
                print("    %s" % rel(p))
                print("        %s   格式 %s v%s" % (reason, magic, ver))

        if keep:
            print("")
            print("  [需要处理] 这些属于 3.3.5 有的种族：")
            for p, magic, ver, note in keep:
                flag = "[OK]" if note == "可用" else "[!!]"
                print("    %s %s   %s v%s  %s" % (flag, rel(p), magic, ver, note))

        # 关联的 skin
        if junk:
            junk_bases = [os.path.splitext(os.path.basename(p))[0].lower()
                          for p, _, _, _ in junk]
            rel_skins = []
            for s in skins:
                sb = os.path.basename(s).lower()
                for jb in junk_bases:
                    if sb.startswith(jb):
                        rel_skins.append(s)
                        break
            if rel_skins:
                print("")
                print("  这 %d 个 .skin 属于上面要删的模型，一起删：" % len(rel_skins))
                for s in rel_skins[:20]:
                    print("    %s" % rel(s))
                if len(rel_skins) > 20:
                    print("    ... 还有 %d 个" % (len(rel_skins) - 20))

            orphan = len(skins) - len(rel_skins)
            if orphan > 0:
                print("")
                print("  剩下 %d 个 .skin 不属于要删的模型，保留。" % orphan)

    # -----------------------------------------------------------------
    print("")
    print("---------------------------------------------------------------------")
    print(" [2/4] 【关键】贴图是给原版模型还是 HD 模型做的")
    print("---------------------------------------------------------------------")

    hd = [p for p in blps if "_hd" in os.path.basename(p).lower()]
    plain = [p for p in blps if "_hd" not in os.path.basename(p).lower()]

    print("")
    print("  含 _HD 后缀 : %d 个" % len(hd))
    print("  不含 _HD    : %d 个" % len(plain))
    print("")

    if len(blps) == 0:
        print("  没有 BLP")
    elif len(hd) > len(plain):
        print("  >> 这是给【HD 模型】(WoD/Legion 移植版) 做的贴图")
        print("     必须【先装 HD 人物模型包】，否则贴图无处可贴，看不到效果。")
    elif len(hd) == 0:
        print("  >> 这是给【3.3.5 原版模型】做的贴图")
        print("     不需要额外装模型包，直接覆盖原版贴图即可。这是最省事的情况。")
    else:
        print("  >> 混合。两套都覆盖了，装了 HD 模型包也能用，不装也能用。")

    # -----------------------------------------------------------------
    print("")
    print("---------------------------------------------------------------------")
    print(" [3/4] 覆盖范围")
    print("---------------------------------------------------------------------")

    race_hit = {}
    other_top = {}
    for p in blps:
        r = rel(p)
        parts = r.split("\\")
        if len(parts) >= 2 and parts[0].lower() == "character":
            race = parts[1]
            race_hit[race] = race_hit.get(race, 0) + 1
        else:
            top = parts[0] if parts else "?"
            other_top[top] = other_top.get(top, 0) + 1

    if race_hit:
        print("")
        print("  Character 下覆盖的种族：")
        for r, c in sorted(race_hit.items(), key=lambda x: -x[1]):
            mark = ""
            if why_not_335(r):
                mark = "   <- %s，可删" % why_not_335(r)
            print("    %-16s %4d 个贴图%s" % (r, c, mark))

    if other_top:
        print("")
        print("  其他顶层目录：")
        for t, c in sorted(other_top.items(), key=lambda x: -x[1])[:10]:
            print("    %-24s %4d 个文件" % (t, c))

    # -----------------------------------------------------------------
    print("")
    print("---------------------------------------------------------------------")
    print(" [4/4] 安装方案")
    print("---------------------------------------------------------------------")
    print("")

    junk_n = len([p for p in m2s if why_not_335(os.path.basename(p))])
    bad_m2 = 0
    for p in m2s:
        if why_not_335(os.path.basename(p)):
            continue
        magic, ver = read_m2_magic(p)
        if magic != "MD20" or ver != 264:
            bad_m2 += 1

    if bad_m2 == 0 and junk_n == len(m2s):
        print("  >> 【好消息：删掉那几个没用的模型就能装】")
        print("")
        print("     所有格式不兼容的 M2 都属于 3.3.5 不存在的种族/职业，")
        print("     删掉它们（和对应的 .skin），剩下的纯贴图部分可以正常使用。")
        print("")
        print("     步骤：")
        print("       1. 删掉上面 [可直接删除] 列出的 .m2 和 .skin")
        print("       2. 剩下的 Character 目录打包成 patch-Y.MPQ")
        print("       3. 放进 Data\\，删 Cache\\ 文件夹")
        if len(hd) > len(plain):
            print("       4. 【前提】必须已装 HD 人物模型包")
    elif bad_m2 > 0:
        print("  >> 【有 %d 个 3.3.5 种族的模型格式不兼容】" % bad_m2)
        print("     这些不能简单删掉（删了那个种族会缺模型）。")
        print("     要么转换，要么放弃这部分。")
    else:
        print("  >> 模型部分没问题，按常规流程装。")

    print("")
    print("=" * 69)


if __name__ == "__main__":
    main()
