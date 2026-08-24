# -*- coding: utf-8 -*-
r"""
patch_slots.py —— 扫描客户端补丁槽位占用情况

用途
    回答"我的 patch 字母还剩几个能用"这个问题。
    客户端只认【单个字符】的补丁名：patch-0 到 patch-9、patch-A 到 patch-Z。
    Data\ 和 Data\<locale>\ 各是一套独立的 36 个槽位。

用法
    python patch_slots.py "D:\WOW\Data"

输出
    每个目录的占用/空闲槽位，加载优先级排序，以及不合规的文件名警告。

命名规则（客户端硬性要求）
    Data\             patch-<X>.MPQ          X = 0-9 或 A-Z，单个字符
    Data\zhCN\        patch-zhCN-<X>.MPQ     同上

    大小写不敏感（Windows 文件系统本身也不区分）。
    patch.MPQ（不带后缀字符）是最早的一个，优先级最低。

加载优先级（后加载的覆盖先加载的）
    同目录内：  patch < patch-0 < ... < patch-9 < patch-A < ... < patch-Z
    跨目录：    Data\  <  Data\<locale>\      locale 整体压过 Data
    所以全客户端优先级最高的位置是 Data\<locale>\patch-<locale>-Z.MPQ
"""

import os
import re
import sys
import argparse

# 客户端认的槽位字符，按加载顺序排列
SLOT_CHARS = list('0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ')

# Data\ 下：patch-X.MPQ
RE_PLAIN = re.compile(r'^patch-([0-9A-Za-z])\.mpq$', re.IGNORECASE)
# locale 下：patch-<locale>-X.MPQ
RE_LOCALE = re.compile(r'^patch-([A-Za-z]{4})-([0-9A-Za-z])\.mpq$', re.IGNORECASE)
# 不带槽位字符的基础补丁
RE_BASE = re.compile(r'^patch(-[A-Za-z]{4})?\.mpq$', re.IGNORECASE)

# 客户端自带的核心档案，不是补丁，不占槽位，正常加载。
# 不能把它们报成"不合规"，否则是假警报。
RE_CORE = re.compile(
    r'^(common|common-2|expansion|lichking|base|misc|model|interface|'
    r'sound|speech|texture|wmo|backup|dbc|fonts|itemtexture|terrain|'
    r'locale-[A-Za-z]{4}|speech-[A-Za-z]{4}|expansion-locale-[A-Za-z]{4}|'
    r'lichking-locale-[A-Za-z]{4}|expansion-speech-[A-Za-z]{4}|'
    r'lichking-speech-[A-Za-z]{4}|wow-update(-[A-Za-z]{4})?-\d+)\.mpq$',
    re.IGNORECASE)


def scan_dir(path, is_locale, locale_name=None):
    """扫描一个目录，返回 (占用dict, 基础补丁list, 不合规list, 文件夹list)"""
    used = {}          # 槽位字符(大写) -> 文件名
    base = []          # patch.MPQ / patch-zhCN.MPQ
    odd = []           # 名字不合规的 .mpq（想当补丁但客户端不认）
    folders = []       # 同名文件夹（打补丁的 exe 支持文件夹当 MPQ）
    core = []          # 客户端自带核心档案，正常加载，仅统计

    if not os.path.isdir(path):
        return None

    for name in sorted(os.listdir(path)):
        full = os.path.join(path, name)

        if os.path.isdir(full):
            # 文件夹形式的补丁（需要打过补丁的 exe）
            if name.lower().startswith('patch'):
                folders.append(name)
            continue

        if not name.lower().endswith('.mpq'):
            continue

        # 核心档案先排除，它们不是补丁也不占槽位
        if RE_CORE.match(name):
            core.append(name)
            continue

        if RE_BASE.match(name):
            base.append(name)
            continue

        m = RE_LOCALE.match(name) if is_locale else None
        if m:
            ch = m.group(2).upper()
            used[ch] = name
            continue

        m = RE_PLAIN.match(name)
        if m and not is_locale:
            ch = m.group(1).upper()
            used[ch] = name
            continue

        # locale 目录里出现了 patch-X.MPQ（少了 locale 段），或者其它怪名字
        odd.append(name)

    return used, base, odd, folders, core


def report(title, path, used, base, odd, folders, core, prefix_fmt):
    print("")
    print("=" * 66)
    print("  %s" % title)
    print("  %s" % path)
    print("=" * 66)

    if core:
        print("  客户端核心档案 %d 个（正常加载，不占补丁槽位）" % len(core))

    if base:
        for b in base:
            print("  [基础] %s   （优先级最低）" % b)

    if used:
        print("")
        print("  已占用 %d 个槽位（按加载顺序，靠后的覆盖靠前的）：" % len(used))
        for ch in SLOT_CHARS:
            if ch in used:
                print("      %s   %s" % (ch, used[ch]))
    else:
        print("")
        print("  没有占用任何槽位")

    free = [c for c in SLOT_CHARS if c not in used]
    print("")
    print("  空闲 %d 个槽位：" % len(free))
    # 每行 18 个，方便看
    for i in range(0, len(free), 18):
        print("      " + ' '.join(free[i:i + 18]))

    if free:
        print("")
        print("  下一个建议用： %s" % (prefix_fmt % free[0]))
        print("  想压过本目录所有补丁： %s" % (prefix_fmt % free[-1]))

    if folders:
        print("")
        print("  文件夹形式的补丁（需要打过补丁的 exe 才认）：")
        for f in folders:
            print("      %s\\" % f)

    if odd:
        print("")
        print("  [警告] 下面这些 .mpq 名字不合规，客户端【不会加载】：")
        for o in odd:
            print("      %s" % o)
        print("  合规格式： %s" % (prefix_fmt % 'X'))
        print("  X 必须是【单个】字符（0-9 或 A-Z）。")
        print("  patch-AA.MPQ / patch-ZZ.MPQ / patch-10.MPQ 这类【都不认】。")

    return len(free)


def main():
    ap = argparse.ArgumentParser(description='扫描 WoW 客户端补丁槽位占用')
    ap.add_argument('datadir', help='客户端 Data 目录，例如 D:\\WOW\\Data')
    args = ap.parse_args()

    datadir = args.datadir
    if not os.path.isdir(datadir):
        sys.stderr.write("找不到目录: %s\n" % datadir)
        sys.stderr.write("应该指向客户端的 Data 文件夹，例如 D:\\WOW\\Data\n")
        return 2

    print("")
    print("WoW 3.3.5 补丁槽位扫描")
    print("客户端只认单字符槽位：0-9 和 A-Z，每个目录各 36 个")

    total_free = 0

    # Data\
    r = scan_dir(datadir, is_locale=False)
    if r:
        used, base, odd, folders, core = r
        total_free += report("Data 目录（所有语言共用）", datadir,
                             used, base, odd, folders, core, "patch-%s.MPQ")

    # locale 子目录
    known_locales = ['zhCN', 'zhTW', 'enUS', 'enGB', 'deDE', 'frFR',
                     'esES', 'esMX', 'ruRU', 'koKR', 'ptBR', 'itIT']
    for name in sorted(os.listdir(datadir)):
        full = os.path.join(datadir, name)
        if not os.path.isdir(full):
            continue
        if name not in known_locales:
            continue

        r = scan_dir(full, is_locale=True, locale_name=name)
        if r:
            used, base, odd, folders, core = r
            total_free += report("%s 目录（整体压过 Data）" % name, full,
                                 used, base, odd, folders, core,
                                 "patch-" + name + "-%s.MPQ")

    print("")
    print("=" * 66)
    print("  合计还有 %d 个空闲槽位" % total_free)
    print("=" * 66)
    print("")
    print("  槽位真不够时的三条出路（按推荐顺序）：")
    print("    1. 合并 —— 一个 MPQ 能装无限个文件，把几个补丁合成一个")
    print("    2. 用 locale 目录 —— 那是另外独立的 36 个槽位")
    print("    3. 文件夹形式 —— 你的 exe 打过补丁，支持文件夹当 MPQ")
    print("")
    print("  patch-AA / patch-ZZ / patch-10 这类【多字符名客户端不认】，")
    print("  放进去不报错，就是静默不加载。")
    print("")
    return 0


if __name__ == '__main__':
    sys.exit(main())
