# -*- coding: utf-8 -*-
"""
================================================================================
  客户端补丁内容预检
================================================================================

  用途：打包 MPQ 之前先检查内容有没有问题，避免打完包才发现要重来。

  检查项：
    1. 文件总数（MPQ 建包时的 max files 要够）
    2. 图标格式与尺寸（必须 BLP，技能图标 64x64）
    3. 文件名合法性（中文/空格会导致客户端读不到）
    4. 目录结构是否正确
    5. DBC 文件列表（提醒同步服务端）

  用法：
      python check_patch_content.py <补丁内容目录>

  例：
      python check_patch_content.py "D:/MyPatch"

  目录结构应该是：
      MyPatch/
        DBFilesClient/      <- DBC 放这
        Interface/
          Icons/            <- 图标放这
          GlueXML/
          FrameXML/
        World/              <- 地图/模型
================================================================================
"""
import sys, os, struct, re

def blp_size(path):
    """读 BLP 头拿尺寸"""
    try:
        with open(path, 'rb') as f:
            d = f.read(20)
        if len(d) < 20 or d[:4] not in (b'BLP2', b'BLP1'):
            return None
        w, h = struct.unpack('<2I', d[12:20])
        return (w, h)
    except Exception:
        return None

def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    root = sys.argv[1]
    if not os.path.isdir(root):
        print("[错误] 目录不存在: %s" % root)
        return 1

    print("=" * 78)
    print("  客户端补丁内容预检")
    print("=" * 78)
    print("  目录: %s" % root)
    print()

    total = 0
    by_ext = {}
    bad_name = []
    icons = []
    dbcs = []
    big_files = []

    for dp, dns, fns in os.walk(root):
        for fn in fns:
            full = os.path.join(dp, fn)
            rel = os.path.relpath(full, root).replace('\\', '/')
            total += 1
            ext = os.path.splitext(fn)[1].lower()
            by_ext[ext] = by_ext.get(ext, 0) + 1

            # 文件名合法性：只允许 ASCII 字母数字 . _ - 和路径分隔
            if not re.match(r'^[A-Za-z0-9_\-./\\]+$', rel):
                bad_name.append(rel)

            if ext == '.blp' and '/icons/' in rel.lower():
                icons.append((rel, full))
            if ext == '.dbc':
                dbcs.append(rel)

            try:
                sz = os.path.getsize(full)
                if sz > 50 * 1024 * 1024:
                    big_files.append((rel, sz))
            except Exception:
                pass

    # ---- 1 文件总数 ----
    print("  [1] 文件总数")
    print("      共 %d 个文件" % total)
    if total > 60000:
        print("      [!!] 超过 60000，建 MPQ 时 max files 必须设更大，或拆分多个 patch")
    elif total > 4000:
        print("      [警告] 超过 4000 —— MPQ Editor 默认上限就是 4000！")
        print("             建包时务必手动把 max files 设到 65536")
    else:
        print("      [OK] 在默认上限内")
    print()

    # ---- 2 按类型 ----
    print("  [2] 文件类型分布")
    for e, n in sorted(by_ext.items(), key=lambda x: -x[1])[:12]:
        print("      %-10s %6d" % (e if e else '(无扩展名)', n))
    print()

    # ---- 3 图标检查 ----
    print("  [3] 图标检查 (Interface/Icons)")
    if not icons:
        print("      (没有图标文件)")
    else:
        wrong = []
        for rel, full in icons:
            s = blp_size(full)
            if s is None:
                wrong.append((rel, "不是有效 BLP"))
            elif s != (64, 64):
                wrong.append((rel, "尺寸 %dx%d，应为 64x64" % s))
        print("      共 %d 个图标" % len(icons))
        if wrong:
            print("      [!!] %d 个有问题:" % len(wrong))
            for rel, why in wrong[:10]:
                print("           %-52s %s" % (rel, why))
            if len(wrong) > 10:
                print("           ... 还有 %d 个" % (len(wrong) - 10))
        else:
            print("      [OK] 全部是 64x64 的 BLP")
    print()

    # ---- 4 文件名 ----
    print("  [4] 文件名合法性")
    if bad_name:
        print("      [!!] %d 个文件名含中文/空格/特殊字符，客户端可能读不到:" % len(bad_name))
        for f in bad_name[:10]:
            print("           %s" % f)
        if len(bad_name) > 10:
            print("           ... 还有 %d 个" % (len(bad_name) - 10))
    else:
        print("      [OK] 全部合法（纯 ASCII）")
    print()

    # ---- 5 DBC ----
    print("  [5] DBC 文件")
    if dbcs:
        print("      共 %d 个:" % len(dbcs))
        for f in dbcs[:12]:
            print("           %s" % f)
        if len(dbcs) > 12:
            print("           ... 还有 %d 个" % (len(dbcs) - 12))
        print()
        print("      [提醒] 这些 DBC 必须同步到服务端的 dbc 目录！")
        print("             用 check_dbc.py 校验双端一致性")
    else:
        print("      (没有 DBC)")
    print()

    # ---- 6 大文件 ----
    if big_files:
        print("  [6] 超大文件 (>50MB)")
        for rel, sz in big_files[:8]:
            print("      %-56s %.1f MB" % (rel, sz / 1048576.0))
        print("      [提示] MPQ 对单文件没硬限制，但打包会很慢")
        print()

    # ---- 目录结构建议 ----
    print("  [7] 目录结构")
    expect = ['DBFilesClient', 'Interface', 'World', 'Sound', 'Character', 'Item']
    tops = [d for d in os.listdir(root) if os.path.isdir(os.path.join(root, d))]
    print("      顶层目录: %s" % (', '.join(tops) if tops else '(无)'))
    unknown = [d for d in tops if d not in expect]
    if unknown:
        print("      [警告] 这些目录不是标准 WoW 结构，客户端可能忽略:")
        for d in unknown:
            print("             %s" % d)
        print("      标准顶层目录: %s" % ', '.join(expect))
    print()

    print("=" * 78)
    issues = len(bad_name) + (1 if total > 4000 else 0)
    if issues == 0:
        print("  [OK] 预检通过，可以打包")
    else:
        print("  发现 %d 类问题，建议修完再打包" % issues)
    print("=" * 78)
    return 0

if __name__ == '__main__':
    sys.exit(main())
