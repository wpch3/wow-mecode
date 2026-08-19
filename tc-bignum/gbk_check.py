# -*- coding: utf-8 -*-
import sys, io, os
bad_total = 0
for path in sys.argv[1:]:
    with io.open(path, 'r', encoding='utf-8') as f:
        txt = f.read()
    bad = {}
    for i, line in enumerate(txt.split('\n'), 1):
        for ch in line:
            try:
                ch.encode('gbk')
            except UnicodeEncodeError:
                bad.setdefault(ch, []).append(i)
    name = os.path.basename(path)
    if bad:
        bad_total += 1
        print("[FAIL] %s  不可 GBK 编码字符 %d 种:" % (name, len(bad)))
        for ch, lines in bad.items():
            print("        U+%04X %r  行: %s" % (ord(ch), ch, lines[:8]))
    else:
        print("[ OK ] %s  全部字符 GBK 兼容 (%d 行 / %d 字符)" % (name, txt.count('\n')+1, len(txt)))
sys.exit(1 if bad_total else 0)
