# -*- coding: utf-8 -*-
"""
check_encoding.py  ——  中文源码编码体检

用法:
    python check_encoding.py <文件1> [文件2 ...]
    python check_encoding.py D:/TrinityCore/src/server/scripts/Commands/*.cpp

背景（step54 踩的坑）:
    本仓库 cmake/compiler/msvc/settings.cmake:30 有 /utf-8
    -> MSVC 【强制】按 UTF-8 解析源文件，不看 Windows 系统代码页。

    但中文 Windows 上的编辑器，遇到【UTF-8 无 BOM】的文件时，
    保存会猜成 ANSI(GBK)。一旦存成 GBK:
        GBK 汉字首字节 81-FE，大量落在 UTF-8 的三字节序列头区间 E0-EF
        -> 被多吞一个字节 -> 行尾的换行符 0x0A 被吞掉
        -> 下一行被拖进注释 -> 函数头消失
        -> 报 "C2447 { 缺少函数标题"，而且报的是【下下行】的行号

    典型症状: 报错行是一个孤零零的 {，而它上面明明有函数头。

检查项:
    1  能否按 UTF-8 解码（不能 = 已经被存成 GBK 了，最严重）
    2  有没有 BOM（没有 = 随时可能被编辑器转成 GBK，隐患）
    3  按 UTF-8 解析时有没有行被"吃掉"（C2447 的直接原因）
    4  是否全部可用 GBK 编码（用户文档要求 GBK 兼容，不能有 emoji）
    5  行尾有没有落单的反斜杠（行继续符陷阱，同样会吃行）
    6  大括号 / 小括号是否平衡，块注释是否闭合

退出码: 0 = 全部通过, 1 = 有问题
"""

import sys
import os


def utf8_overrun(bs):
    """按 UTF-8 规则扫这行字节，返回越界的字节数（>0 表示会吞掉换行符）"""
    bs = bs.rstrip(b'\r')
    i = 0
    while i < len(bs):
        c = bs[i]
        if c < 0x80:
            i += 1
        elif c < 0xC0:
            i += 1          # 孤立 continuation 字节
        elif c < 0xE0:
            i += 2
        elif c < 0xF0:
            i += 3
        else:
            i += 4
    return i - len(bs)


def brace_balance(txt):
    """剥掉注释和字符串后统计括号，返回 (大括号差, 小括号差, 结束状态)"""
    i, n = 0, len(txt)
    st = 'code'
    d = p = 0
    while i < n:
        c = txt[i]
        if st == 'code':
            if txt.startswith('//', i):
                st = 'line'; i += 2; continue
            if txt.startswith('/*', i):
                st = 'blk';  i += 2; continue
            if c == '"':
                st = 'str';  i += 1; continue
            if c == "'":
                st = 'chr';  i += 1; continue
            if   c == '{': d += 1
            elif c == '}': d -= 1
            elif c == '(': p += 1
            elif c == ')': p -= 1
        elif st == 'line':
            if c == '\n':
                st = 'code'
        elif st == 'blk':
            if txt.startswith('*/', i):
                st = 'code'; i += 2; continue
        elif st == 'str':
            if c == '\\':
                i += 2; continue
            if c == '"':
                st = 'code'
        elif st == 'chr':
            if c == '\\':
                i += 2; continue
            if c == "'":
                st = 'code'
        i += 1
    return d, p, st


def check(path):
    print("=" * 68)
    print(" " + path)
    print("=" * 68)

    if not os.path.isfile(path):
        print("  [FAIL] 文件不存在")
        return False

    raw = open(path, 'rb').read()
    ok = True

    # --- 1  UTF-8 可解码性 ---
    try:
        txt = raw.decode('utf-8-sig')
        print("  [PASS] 1. UTF-8 解码正常")
    except UnicodeDecodeError as e:
        print("  [FAIL] 1. UTF-8 解码失败，位置 %d: %s" % (e.start, e.reason))
        print("         这个文件已经被存成 GBK 了。")
        print("         修法: 用带 BOM 的 UTF-8 版本【整个覆盖】，不要在编辑器里粘贴。")
        return False

    # --- 2  BOM ---
    has_bom = raw[:3] == b'\xef\xbb\xbf'
    if has_bom:
        print("  [PASS] 2. 有 UTF-8 BOM（编辑器不会猜错编码）")
    else:
        print("  [WARN] 2. 【没有 BOM】")
        print("         中文 Windows 的编辑器保存时可能把它转成 GBK。")
        print("         建议存成 UTF-8 带 BOM。")
        ok = False

    # --- 3  吃行检测 ---
    lines = raw.split(b'\n')
    eaten = [(k, utf8_overrun(b)) for k, b in enumerate(lines, 1)
             if utf8_overrun(b) > 0]
    if not eaten:
        print("  [PASS] 3. 无「吞掉换行」的行")
    else:
        print("  [FAIL] 3. 有 %d 行会吞掉换行符 -> 下一行被拖进注释" % len(eaten))
        for k, o in eaten[:10]:
            nxt = lines[k].decode('utf-8', 'replace').strip()[:44] if k < len(lines) else ''
            print("         行%-5d 越界%d字节 -> 吃掉行%d: %s" % (k, o, k + 1, nxt))
        ok = False

    # --- 4  GBK 兼容 ---
    bad = []
    for k, l in enumerate(txt.split('\n'), 1):
        try:
            l.encode('gbk')
        except UnicodeEncodeError:
            for ch in l:
                try:
                    ch.encode('gbk')
                except UnicodeEncodeError:
                    bad.append((k, ch, hex(ord(ch))))
    if not bad:
        print("  [PASS] 4. 全文可 GBK 编码（无 emoji / 特殊符号）")
    else:
        print("  [FAIL] 4. 有 %d 处不可 GBK 编码的字符" % len(bad))
        for k, ch, code in bad[:10]:
            print("         行%-5d 字符 %r (%s)" % (k, ch, code))
        ok = False

    # --- 5  行尾反斜杠 ---
    bs = [k for k, l in enumerate(txt.replace('\r\n', '\n').split('\n'), 1)
          if l.rstrip().endswith('\\')]
    if not bs:
        print("  [PASS] 5. 行尾无落单反斜杠")
    else:
        print("  [WARN] 5. 行尾有反斜杠（若非宏定义则是行继续符陷阱）: 行 %s" % bs[:10])

    # --- 6  括号平衡 ---
    d, p, st = brace_balance(txt)
    if d == 0 and p == 0 and st == 'code':
        print("  [PASS] 6. 括号平衡，注释闭合")
    else:
        print("  [FAIL] 6. 大括号差=%d 小括号差=%d 结束状态=%s" % (d, p, st))
        if st == 'blk':
            print("         有未闭合的 /* 块注释")
        ok = False

    print()
    print("  结论: " + ("全部通过，可以编译" if ok else "有问题，见上面 FAIL/WARN"))
    print()
    return ok


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    results = [check(p) for p in sys.argv[1:]]
    return 0 if all(results) else 1


if __name__ == '__main__':
    sys.exit(main())
