# -*- coding: utf-8 -*-
"""
patch_scan.py -- 模型补丁体检（只读，不改任何文件）

用途：拿到一个别人做的模型/皮肤补丁，判断能不能在 3.3.5a 上用。
      直接读 M2 文件头里的魔数和版本号，不靠猜。

用法（Git Bash 或 CMD）：
    python patch_scan.py "D:/下载/BfA全种族模型"
    python patch_scan.py "D:/下载/patch-5.MPQ的解压目录"

依赖：只用 Python 标准库，不装任何东西。
      Python 3.6+。Windows 上装了 Python 就能跑。

判定依据（wowdev.wiki/M2 权威版本表）：
    版本 256-257  Classic
    版本 260-263  TBC
    版本 264      WotLK 3.3.5   <-- 我们要的
    版本 265-272  Cataclysm
    版本 272      MoP / WoD
    版本 272-274  Legion / BfA / Shadowlands
    魔数 MD20 = 传统格式（3.3.5 认）
    魔数 MD21 = Legion+ 分块格式（3.3.5 完全不认）
"""

import os
import sys
import struct
import io

# ---------------------------------------------------------------------
# 版本号 -> 资料片
# ---------------------------------------------------------------------
def ver_name(v):
    if v <= 257:
        return "Classic (1.x)"
    if 260 <= v <= 263:
        return "TBC (2.x)"
    if v == 264:
        return "WotLK 3.3.5"
    if 265 <= v <= 271:
        return "Cataclysm (4.x)"
    if v == 272:
        return "MoP/WoD/Legion+ (5.x-7.x)"
    if v == 273:
        return "Legion (7.x)"
    if v == 274:
        return "Legion/BfA/SL (7.x-9.x)"
    return "未知版本 %d" % v


def read_m2(path):
    """读 M2 头部，返回 (魔数, 版本号, 分块列表, 错误)"""
    try:
        with open(path, "rb") as f:
            head = f.read(8)
            if len(head) < 8:
                return None, None, [], "文件太小"
            magic = head[0:4].decode("ascii", "replace")

            if magic == "MD20":
                ver = struct.unpack("<I", head[4:8])[0]
                return "MD20", ver, [], None

            # Legion+ 分块格式：遍历全部分块名
            # 注意 MD21 内容很大，不能只读 4K，要按 size 跳过
            f.seek(0, os.SEEK_END)
            fsize = f.tell()
            f.seek(0)
            chunks = []
            md21_ver = None
            pos = 0
            guard = 0
            while pos + 8 <= fsize and guard < 64:
                guard += 1
                f.seek(pos)
                hdr = f.read(8)
                if len(hdr) < 8:
                    break
                name = hdr[0:4].decode("ascii", "replace")
                size = struct.unpack("<I", hdr[4:8])[0]
                # 分块名必须是 4 个可打印字符
                if not all(33 <= b <= 126 for b in hdr[0:4]):
                    break
                chunks.append(name)
                if name == "MD21":
                    inner = f.read(8)
                    if len(inner) >= 8 and inner[0:4] == b"MD20":
                        md21_ver = struct.unpack("<I", inner[4:8])[0]
                pos += 8 + size          # 按真实长度跳过，才能读到后面的块
                if size == 0:
                    break
            return magic, md21_ver, chunks, None
    except Exception as e:
        return None, None, [], str(e)


def read_blp(path):
    """读 BLP 头，返回 (编码, 像素格式, 宽, 高, 错误)"""
    try:
        with open(path, "rb") as f:
            head = f.read(0x14)
            if len(head) < 0x14:
                return None, None, 0, 0, "文件太小"
            if head[0:4] != b"BLP2":
                return head[0:4].decode("ascii", "replace"), None, 0, 0, "不是BLP2"
            enc = head[8]              # 0x08 ColorEncoding
            alpha_depth = head[9]      # 0x09 AlphaBitDepth
            pix = head[10]             # 0x0A preferredFormat
            w = struct.unpack("<I", head[0x0C:0x10])[0]
            h = struct.unpack("<I", head[0x10:0x14])[0]
            return enc, pix, w, h, None
    except Exception as e:
        return None, None, 0, 0, str(e)


ENC_NAME = {0: "JPEG", 1: "调色板RAW1", 2: "DXT", 3: "未压缩BGRA"}
PIX_NAME = {0: "DXT1", 1: "DXT3", 2: "ARGB8888", 3: "ARGB1555",
            4: "ARGB4444", 5: "RGB565", 6: "A8", 7: "DXT5",
            8: "未指定", 9: "ARGB2565", 11: "BC5"}


def is_pow2(n):
    return n > 0 and (n & (n - 1)) == 0


# 3.3.5a 关键 DBC 的正确列数。列数对不上客户端直接报 Error #121 拒绝启动。
# 来源：3.3.5.12340 客户端实际结构 + wowdev.wiki/DB/*
DBC_FIELDS_335 = {
    "chrraces.dbc":         69,
    "charsections.dbc":     10,
    "creaturedisplayinfo.dbc": 16,
    "creaturemodeldata.dbc":   30,
    "item.dbc":              8,
    "itemdisplayinfo.dbc":  25,
    "chrclasses.dbc":       60,
    "charstartoutfit.dbc":  77,
    "charhairgeosets.dbc":   9,
    "creaturesounddata.dbc": 38,
    "helmetgeosetvisdata.dbc": 8,
}


def read_dbc(path):
    """读 WDBC 头，返回 (记录数, 列数, 每条字节数, 错误)"""
    try:
        with open(path, "rb") as f:
            h = f.read(20)
            if len(h) < 20:
                return 0, 0, 0, "文件太小"
            if h[0:4] != b"WDBC":
                return 0, 0, 0, "不是WDBC(%s)" % h[0:4].decode("ascii", "replace")
            rec, fld, size = struct.unpack("<III", h[4:16])
            return rec, fld, size, None
    except Exception as e:
        return 0, 0, 0, str(e)


def main():
    if len(sys.argv) < 2:
        print("用法: python patch_scan.py <补丁解压目录>")
        sys.exit(1)

    root = sys.argv[1]
    if not os.path.isdir(root):
        print("[错误] 目录不存在: %s" % root)
        sys.exit(1)

    print("=" * 69)
    print(" 模型补丁体检")
    print(" 目标: %s" % root)
    print("=" * 69)

    m2s, blps, skins, anims, others = [], [], [], [], []
    dbcs = []
    for dirpath, _, files in os.walk(root):
        for fn in files:
            p = os.path.join(dirpath, fn)
            low = fn.lower()
            if low.endswith(".m2"):
                m2s.append(p)
            elif low.endswith(".blp"):
                blps.append(p)
            elif low.endswith(".skin"):
                skins.append(p)
            elif low.endswith(".anim"):
                anims.append(p)
            elif low.endswith(".dbc"):
                dbcs.append(p)
            else:
                others.append(p)

    print("")
    print("---------------------------------------------------------------------")
    print(" [1/4] 文件清点")
    print("---------------------------------------------------------------------")
    print("  .m2   模型   : %d" % len(m2s))
    print("  .skin 网格   : %d" % len(skins))
    print("  .blp  贴图   : %d" % len(blps))
    print("  .anim 动画   : %d" % len(anims))
    print("  .dbc  数据表 : %d" % len(dbcs))
    print("  其他         : %d" % len(others))

    dbc_bad = []      # 列数对不上 3.3.5 的
    dbc_ok = []
    dbc_unknown = []

    if dbcs:
        print("")
        print("  [重要] 补丁自带 DBC。DBC 列数错了客户端会【拒绝启动】，逐个查：")
        print("")
        for p in dbcs:
            name = os.path.basename(p)
            rec, fld, size, err = read_dbc(p)
            if err:
                print("    [??] %-28s %s" % (name, err))
                dbc_unknown.append((p, name))
                continue
            expect = DBC_FIELDS_335.get(name.lower())
            if fld == 0 or rec == 0:
                print("    [??] %-28s 空文件或已损坏 (%d 列 / %d 条)"
                      % (name, fld, rec))
                dbc_unknown.append((p, name))
            elif expect is None:
                print("    [??] %-28s %d 列 / %d 条  (无对照数据)"
                      % (name, fld, rec))
                dbc_unknown.append((p, name))
            elif fld == expect:
                print("    [OK] %-28s %d 列 / %d 条  符合 3.3.5"
                      % (name, fld, rec))
                dbc_ok.append((p, name))
            else:
                print("    [!!] %-28s %d 列 / %d 条  <<< 3.3.5 要求 %d 列"
                      % (name, fld, rec, expect))
                dbc_bad.append((p, name, fld, expect))

        if dbc_bad:
            print("")
            print("    [严重] %d 个 DBC 列数不符！" % len(dbc_bad))
            print("           客户端会报 Error #121 Version Mismatch 并【无法启动】")
            print("           说明这些 DBC 是给别的资料片做的，不能直接用")

    # -----------------------------------------------------------------
    print("")
    print("---------------------------------------------------------------------")
    print(" [2/4] M2 模型版本判定（关键）")
    print("---------------------------------------------------------------------")

    if not m2s:
        print("  没有 .m2 文件 -- 这是纯贴图补丁")
        print("  纯贴图补丁替换人物皮肤有 UV 错位风险，见第 4 段")
    else:
        stats = {}
        bad_chunk = []
        samples = []
        for p in m2s:
            magic, ver, chunks, err = read_m2(p)
            if err:
                continue
            key = (magic, ver)
            stats[key] = stats.get(key, 0) + 1
            if magic != "MD20":
                bad_chunk.append((p, magic, chunks))
            if len(samples) < 5:
                samples.append((os.path.basename(p), magic, ver))

        print("  版本分布：")
        for (magic, ver), cnt in sorted(stats.items(), key=lambda x: -x[1]):
            if magic == "MD20":
                vn = ver_name(ver) if ver else "?"
                mark = "[可用]" if ver == 264 else "[!!需转换]"
                print("    %s 魔数=%s 版本=%s (%s)  共 %d 个  %s"
                      % (mark, magic, ver, vn, cnt, ""))
            else:
                print("    [!!不可用] 魔数=%s (Legion+分块格式)  共 %d 个"
                      % (magic, cnt))

        print("")
        print("  样例：")
        for name, magic, ver in samples:
            print("    %-42s %s v%s" % (name[:42], magic, ver))

        if bad_chunk:
            print("")
            print("  [严重] 发现 %d 个分块格式(MD21)模型" % len(bad_chunk))
            print("         3.3.5 客户端【完全不认】这种格式，会直接崩溃或不显示")
            _, _, ch = bad_chunk[0]
            if ch:
                print("         首个模型的分块: %s" % " ".join(ch[:12]))
                if "TXID" in ch:
                    print("         含 TXID 块 = BfA 8.0.1+ ，贴图引用是数字ID不是路径")
                if "SKID" in ch:
                    print("         含 SKID 块 = 用 .skel 骨骼，多数转换器【不支持】")

    # -----------------------------------------------------------------
    print("")
    print("---------------------------------------------------------------------")
    print(" [3/4] BLP 贴图检查")
    print("---------------------------------------------------------------------")

    if not blps:
        print("  没有 BLP")
    else:
        enc_stat = {}
        bc5 = []
        nonpow2 = []
        big = []
        for p in blps:
            enc, pix, w, h, err = read_blp(p)
            if err and enc is None:
                continue
            if err == "不是BLP2":
                enc_stat["非BLP2"] = enc_stat.get("非BLP2", 0) + 1
                continue
            key = "%s / %s" % (ENC_NAME.get(enc, "?%s" % enc),
                               PIX_NAME.get(pix, "?%s" % pix))
            enc_stat[key] = enc_stat.get(key, 0) + 1
            if pix == 11:
                bc5.append(p)
            if not (is_pow2(w) and is_pow2(h)):
                nonpow2.append((p, w, h))
            if w > 1024 or h > 1024:
                big.append((p, w, h))

        print("  编码分布：")
        for k, v in sorted(enc_stat.items(), key=lambda x: -x[1]):
            print("    %-28s %d 个" % (k, v))

        if bc5:
            print("")
            print("  [!!] %d 个 BC5 编码 -- DX10 格式，3.3.5(DX9) 读不了，必须转" % len(bc5))
        if nonpow2:
            print("")
            print("  [!!] %d 个尺寸不是2的幂，游戏里会异常：" % len(nonpow2))
            for p, w, h in nonpow2[:5]:
                print("       %s  %dx%d" % (os.path.basename(p), w, h))
        if big:
            print("")
            print("  [注意] %d 个大于 1024x1024 -- 3.3.5 能读，但显存吃紧时可能掉贴图"
                  % len(big))

    # -----------------------------------------------------------------
    print("")
    print("---------------------------------------------------------------------")
    print(" [4/4] 结论")
    print("---------------------------------------------------------------------")

    md20_264 = 0
    md20_other = 0
    md21 = 0
    for p in m2s:
        magic, ver, chunks, err = read_m2(p)
        if err:
            continue
        if magic == "MD20":
            if ver == 264:
                md20_264 += 1
            else:
                md20_other += 1
        else:
            md21 += 1

    print("")
    if not m2s:
        print("  >> 纯贴图补丁。")
        print("     人物皮肤类风险高（UV 可能对不上），建议先小范围试一个种族。")
    elif md21 > 0:
        print("  >> 【不能直接用】")
        print("     补丁里有 %d 个 Legion+ 分块格式(MD21)模型。" % md21)
        print("     3.3.5 只认 MD20，这些文件放进去会崩溃或不显示。")
        print("")
        print("     这说明补丁是【从正式服直接扒的原始文件】，不是移植成品。")
        print("     要用必须先转换，工具见文档 13 篇。")
    elif md20_other > 0 and md20_264 == 0:
        print("  >> 【需要转换】")
        print("     模型是 MD20 格式，但版本不是 264(WotLK)。")
        print("     已经过一轮处理，但没转到位，还要再降一级。")
    elif md20_264 > 0 and md20_other == 0:
        if dbc_bad:
            print("  >> 【模型能用，但 DBC 有问题 -- 不要整包直接装】")
            print("     模型全部是 MD20 v264，没问题。")
            print("     但有 %d 个 DBC 列数不符合 3.3.5，装上去客户端起不来。" % len(dbc_bad))
            print("")
            print("     做法：打包时【只放模型和贴图，不要放这些 DBC】：")
            for _, n, f, e in dbc_bad:
                print("            %s  (%d列，应为%d列)" % (n, f, e))
            print("")
            print("     然后用 WDBX Editor 打开你【客户端原版】的同名 DBC，")
            print("     手动改需要的行，再放进包里。见文档 13 篇第五节。")
        else:
            print("  >> 【可以直接用】")
            print("     全部 %d 个模型都是 MD20 v264 = 原生 WotLK 3.3.5 格式。" % md20_264)
            print("     作者已经移植好了，打包成 MPQ 丢进 Data 就行。")
            if dbc_ok:
                print("     %d 个 DBC 列数也都对，一起放进 DBFilesClient\\ 目录。"
                      % len(dbc_ok))
            elif dbcs:
                print("     DBC 列数无法核对，装完如果客户端起不来就先把 DBC 拿掉试试。")
            else:
                print("")
                print("     [注意] 补丁【没带 DBC】。人物模型替换通常需要改")
                print("            CharSections.dbc / ChrRaces.dbc，")
                print("            如果进游戏人物不变，就是缺这一步。")
    else:
        print("  >> 【混合状态】")
        print("     MD20 v264: %d 个（可用）" % md20_264)
        print("     MD20 其他版本: %d 个（要转）" % md20_other)
        print("     MD21 分块: %d 个（要转）" % md21)

    # skin 配套检查
    if m2s and skins:
        print("")
        ratio = len(skins) / float(len(m2s))
        if ratio < 1:
            print("  [注意] skin 数(%d) 少于 m2 数(%d)，可能有模型缺网格文件"
                  % (len(skins), len(m2s)))
        else:
            print("  skin/m2 = %.1f，配套正常（一个模型通常 1-4 个 skin）" % ratio)

    print("")
    print("=" * 69)
    print(" 体检结束")
    print("=" * 69)


if __name__ == "__main__":
    main()
