# -*- coding: utf-8 -*-
"""
fix_nbsp.py —— 清理源码里的隐形危险字符

【为什么需要】2026-08-09 A41 踩的坑：
  从网页/文档复制代码时，会带进【不间断空格 U+00A0】。
  它看起来和普通空格一模一样，但：
    1. GBK 编码不了它 -> VS 保存时弹
       「此文件中的某些 Unicode 字符未能保存到当前代码页中」
    2. 编译器不认 -> MSVC 报 C2018/C2059，
       且报错行看起来完全正常，极难排查

【用法】
  python3 tools/fix_nbsp.py <文件...>          只检查，不改
  python3 tools/fix_nbsp.py --fix <文件...>    就地修复（自动备份 .bak）

【处理的字符】
  U+00A0  不间断空格   -> 普通空格      （最常见，网页复制来的）
  U+3000  全角空格     -> 普通空格      （中文输入法误触）
  U+200B  零宽空格     -> 删除
  U+200C/D 零宽连接符  -> 删除
  U+FEFF  BOM(非行首)  -> 删除
  U+2018/19 弯引号     -> '             （Word 自动替换造成）
  U+201C/1D 弯双引号   -> "
  U+2013/14 长破折号   -> -
"""
import sys, io, os

# 字符 -> 替换成什么
REPL = {
    '\u00a0': ' ',    # 不间断空格
    '\u3000': ' ',    # 全角空格
    '\u200b': '',     # 零宽空格
    '\u200c': '',     # 零宽非连接符
    '\u200d': '',     # 零宽连接符
    '\ufeff': '',     # BOM（行首的会被单独处理）
    '\u2018': "'",    # 左弯单引号
    '\u2019': "'",    # 右弯单引号
    '\u201c': '"',    # 左弯双引号
    '\u201d': '"',    # 右弯双引号
    '\u2013': '-',    # en dash
    '\u2014': '-',    # em dash
}

NAMES = {
    '\u00a0': 'NBSP 不间断空格',
    '\u3000': '全角空格',
    '\u200b': '零宽空格',
    '\u200c': '零宽非连接符',
    '\u200d': '零宽连接符',
    '\ufeff': 'BOM/零宽不换行空格',
    '\u2018': '左弯单引号',
    '\u2019': '右弯单引号',
    '\u201c': '左弯双引号',
    '\u201d': '右弯双引号',
    '\u2013': 'en dash',
    '\u2014': 'em dash',
}


def scan(text):
    """返回 {字符: [行号...]}"""
    found = {}
    for lineno, line in enumerate(text.split('\n'), 1):
        # 跳过文件最开头的 BOM
        if lineno == 1 and line.startswith('\ufeff'):
            line = line[1:]
        for ch in line:
            if ch in REPL:
                found.setdefault(ch, []).append(lineno)
    return found


def check_gbk(text):
    """找出 GBK 编码不了的字符（VS 弹框的真正原因）"""
    bad = {}
    for lineno, line in enumerate(text.split('\n'), 1):
        for ch in line:
            try:
                ch.encode('gbk')
            except UnicodeEncodeError:
                bad.setdefault(ch, []).append(lineno)
    return bad


def process(path, do_fix):
    if not os.path.isfile(path):
        print("[跳过] 文件不存在: %s" % path)
        return 0

    with io.open(path, 'r', encoding='utf-8-sig') as f:
        text = f.read()

    name = os.path.basename(path)
    found = scan(text)
    bad_gbk = check_gbk(text)

    if not found and not bad_gbk:
        print("[ OK ] %s  没有隐形危险字符" % name)
        return 0

    print("=" * 62)
    print(" %s" % name)
    print("=" * 62)

    total = 0
    for ch, lines in sorted(found.items(), key=lambda x: -len(x[1])):
        uniq = sorted(set(lines))
        total += len(lines)
        show = ', '.join(str(x) for x in uniq[:12])
        more = ' ...' if len(uniq) > 12 else ''
        print("  U+%04X %-16s %4d 处   行: %s%s"
              % (ord(ch), NAMES.get(ch, '?'), len(lines), show, more))

    # GBK 不兼容的单独提示（这些会让 VS 弹框）
    gbk_only = {c: l for c, l in bad_gbk.items() if c not in REPL}
    if gbk_only:
        print("  --- 以下字符 GBK 编码不了，但不在自动替换表里 ---")
        for ch, lines in gbk_only.items():
            print("  U+%04X %r  %d 处   行: %s"
                  % (ord(ch), ch, len(lines), sorted(set(lines))[:8]))

    if not do_fix:
        print("\n  这些字符会导致：")
        print("    1. VS 保存时弹「某些 Unicode 字符未能保存到当前代码页」")
        print("    2. 编译报 C2018/C2059，且报错行看起来完全正常")
        print("\n  修复： python3 tools/fix_nbsp.py --fix %s" % path)
        return 1

    # 备份
    bak = path + '.bak'
    if not os.path.exists(bak):
        with io.open(bak, 'w', encoding='utf-8') as f:
            f.write(text)
        print("\n  已备份 -> %s" % os.path.basename(bak))

    fixed = text
    for ch, rep in REPL.items():
        fixed = fixed.replace(ch, rep)

    # 写回：UTF-8 带 BOM（坑表铁律：中文源码必须带BOM，
    # 否则中文Windows下会被当成GBK，注释吃掉下一行报 C2447）
    with io.open(path, 'w', encoding='utf-8-sig', newline='\r\n') as f:
        f.write(fixed)

    print("  已修复 %d 处，写回 UTF-8 with BOM (CRLF)" % total)

    # 复查
    with io.open(path, 'r', encoding='utf-8-sig') as f:
        again = scan(f.read())
    if again:
        print("  [警告] 复查仍有残留:", {hex(ord(c)): len(v) for c, v in again.items()})
        return 1
    print("  [ OK ] 复查通过")
    return 0


def main():
    args = sys.argv[1:]
    do_fix = '--fix' in args
    files = [a for a in args if a != '--fix']

    if not files:
        print(__doc__)
        return 1

    rc = 0
    for p in files:
        rc |= process(p, do_fix)

    if not do_fix and rc:
        print("\n（当前是检查模式，加 --fix 才会真的改）")
    return rc


if __name__ == '__main__':
    sys.exit(main())
