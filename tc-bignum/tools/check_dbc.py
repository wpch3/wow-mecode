# -*- coding: utf-8 -*-
"""
================================================================================
  DBC 双端一致性校验
================================================================================

  用途：客户端和服务端的 DBC 必须完全一致，否则会出现：
          · 能进地图但坐标错乱
          · 服务端认为你会某技能，客户端不显示
          · 登录后直接掉线
        这是自定义内容最常见的翻车点。

  用法：
      python check_dbc.py <服务端dbc目录> <客户端dbc目录>

  例：
      python check_dbc.py "D:/TC-Build/bin/RelWithDebInfo/dbc" "D:/WoW335/dbc_extracted"

  说明：客户端 DBC 在 MPQ 里，需要先用 MPQ Editor 解出来到一个临时目录。
================================================================================
"""
import sys, os, hashlib, struct

def md5(path, block=1 << 20):
    h = hashlib.md5()
    with open(path, 'rb') as f:
        while True:
            b = f.read(block)
            if not b:
                break
            h.update(b)
    return h.hexdigest()

def dbc_header(path):
    """读 DBC 头：magic(4) + 记录数 + 字段数 + 记录大小 + 字符串块大小"""
    try:
        with open(path, 'rb') as f:
            d = f.read(20)
        if len(d) < 20 or d[:4] != b'WDBC':
            return None
        rec, fld, size, sblk = struct.unpack('<4I', d[4:20])
        return dict(records=rec, fields=fld, recsize=size, strblock=sblk)
    except Exception:
        return None

def scan(d):
    out = {}
    if not os.path.isdir(d):
        return out
    for fn in os.listdir(d):
        if fn.lower().endswith('.dbc'):
            out[fn.lower()] = os.path.join(d, fn)
    return out

def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 1

    sdir, cdir = sys.argv[1], sys.argv[2]
    S, C = scan(sdir), scan(cdir)

    if not S:
        print("[错误] 服务端目录里没找到 .dbc 文件: %s" % sdir)
        return 1
    if not C:
        print("[错误] 客户端目录里没找到 .dbc 文件: %s" % cdir)
        print("       客户端 DBC 在 MPQ 里，需要先用 MPQ Editor 解压出来")
        return 1

    print("=" * 78)
    print("  DBC 双端一致性校验")
    print("=" * 78)
    print("  服务端: %s  (%d 个)" % (sdir, len(S)))
    print("  客户端: %s  (%d 个)" % (cdir, len(C)))
    print()

    only_s = sorted(set(S) - set(C))
    only_c = sorted(set(C) - set(S))
    both = sorted(set(S) & set(C))

    problems = 0

    if only_s:
        print("  [警告] 只在服务端有 (%d 个):" % len(only_s))
        for f in only_s[:15]:
            print("         %s" % f)
        if len(only_s) > 15:
            print("         ... 还有 %d 个" % (len(only_s) - 15))
        print()

    if only_c:
        print("  [警告] 只在客户端有 (%d 个) —— 服务端缺这些可能导致异常:" % len(only_c))
        for f in only_c[:15]:
            print("         %s" % f)
        if len(only_c) > 15:
            print("         ... 还有 %d 个" % (len(only_c) - 15))
        print()

    diff = []
    for f in both:
        ms, mc = md5(S[f]), md5(C[f])
        if ms != mc:
            hs, hc = dbc_header(S[f]), dbc_header(C[f])
            diff.append((f, hs, hc))

    if diff:
        problems += len(diff)
        print("  [不一致] %d 个文件内容不同  <<< 这是最危险的" % len(diff))
        print()
        print("  %-34s %-22s %s" % ("文件", "服务端(记录/字段)", "客户端(记录/字段)"))
        print("  " + "-" * 74)
        for f, hs, hc in diff:
            a = "%d / %d" % (hs['records'], hs['fields']) if hs else "读取失败"
            b = "%d / %d" % (hc['records'], hc['fields']) if hc else "读取失败"
            flag = ""
            if hs and hc:
                if hs['fields'] != hc['fields']:
                    flag = "  <<< 字段数不同！结构不兼容"
                elif hs['records'] != hc['records']:
                    flag = "  <<< 记录数不同"
            print("  %-34s %-22s %-22s%s" % (f, a, b, flag))
        print()

    print("=" * 78)
    if problems == 0 and not only_s and not only_c:
        print("  [OK] 双端完全一致，共 %d 个 DBC" % len(both))
    else:
        print("  发现 %d 个内容不一致" % len(diff))
        if diff:
            print()
            print("  【怎么修】")
            print("    1. 确定哪一边是最新的（通常是你刚编辑过的那边）")
            print("    2. 把它复制到另一边，覆盖")
            print("    3. 重新运行本脚本确认")
            print()
            print("  【字段数不同的特别注意】")
            print("    说明两边的 DBC 结构版本不一样，直接复制可能不够，")
            print("    要确认是不是用了不同版本的 DBC 编辑器。")
    print("=" * 78)
    return 1 if diff else 0

if __name__ == '__main__':
    sys.exit(main())
