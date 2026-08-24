# -*- coding: utf-8 -*-
"""
find_mojibake.py —— 找出被 VS 存成问号(?)的损坏中文，并分类

【背景】2026-08-09
  从网页复制代码会带进 NBSP(U+00A0) 等 GBK 编码不了的字符。
  VS 保存时弹「某些 Unicode 字符未能保存到当前代码页」，
  如果点了「否」，那些字符会被替换成 ? —— 中文全毁。

【这个工具解决什么】
  一个 .cpp 里可能有 2000+ 个问号，但绝大多数是【注释】(不影响编译)，
  只有极少数在【代码】里(致命)。手工分辨太累，所以自动分类：

    [代码]   问号出现在可执行代码里            -> 致命，必须修
    [字符串] 问号在 "..." 里                   -> 玩家能看到，要修
    [注释]   问号在 // 或 /* */ 里             -> 不影响编译，可缓
    [正常]   三元运算符 ? : 、条件、宏          -> 不是损坏，自动排除

【用法】
  python3 tools/find_mojibake.py <文件...>              扫描并分类
  python3 tools/find_mojibake.py --code-only <文件...>  只看致命的
  python3 tools/find_mojibake.py --dir <目录>           扫整个目录的 .cpp/.h

  【推荐】和 GitHub 干净源码对比，只报你真正改坏的：
  python3 tools/find_mojibake.py --dir /d/TrinityCore/src --baseline /tmp/tcsrc/src

  没有基线时会误报上游自带的问号，例如：
    bot_Events.h:8                    "%Complete: ???"    作者写的占位符
    BoundingIntervalHierarchy.h:242   "// stack is empty?" 英文疑问句
  实测：无基线报 286 个文件，有基线只报 1 个（真正损坏的那个）。

【判定规则】
  只有【连续2个及以上问号】才算被吃掉的中文。
  单个问号可能是三元运算符、英文疑问句，一律不算。
"""
import sys, io, os, re

# 正常的问号用法（不算损坏）
NORMAL_PATTERNS = [
    re.compile(r'\?\s*[\w\(\)\[\]&\*\->\.:"\']+\s*:'),   # 三元 a ? b : c
    re.compile(r'^\s*#\s*(if|elif|define|include)'),      # 预处理
    re.compile(r'\?\?='),                                 # 三字符组
    re.compile(r'"\s*\?\s*"'),                            # 单独的"?"字符串
    re.compile(r"'\?'"),                                  # '?' 字符
    re.compile(r'\bQ\w*\?'),                              # 变量名带?（罕见）
    # 上游自带的占位符写法（不是损坏）
    re.compile(r'%Complete\s*:\s*\?+'),                   # bot_Events.h:8 "%Complete: ???"
    re.compile(r'^\s*\*?\s*\?+\s*$'),                     # 整行只有问号的分隔符
    re.compile(r'(TODO|FIXME|XXX|NOTE|Complete|Unknown|unk)\s*[:：]?\s*\?+', re.I),
]


def looks_normal(line):
    """判断这行的问号是不是正常用法"""
    for p in NORMAL_PATTERNS:
        if p.search(line):
            return True
    return False


def is_damaged_run(line):
    """
    真正的损坏特征：连续多个问号，或问号夹在中文语境里。
    单个孤立问号更可能是三元运算符。
    """
    # 连续2个及以上问号 = 几乎肯定是损坏的中文
    if re.search(r'\?{2,}', line):
        return True
    return False


def classify(path):
    with io.open(path, 'r', encoding='utf-8', errors='replace') as f:
        text = f.read()

    lines = text.split('\n')
    in_block = False

    code, strings, comments, normal = [], [], [], []

    for i, raw in enumerate(lines, 1):
        line = raw

        # ---- 维护块注释状态（无论有没有问号都要跟踪）----
        was_in_block = in_block
        if in_block:
            if '*/' in line:
                in_block = False
        else:
            # 只在没有配对结束时才进入块注释
            if '/*' in line and '*/' not in line.split('/*', 1)[1]:
                in_block = True

        if '?' not in line:
            continue

        stripped = line.strip()

        # ---- 1. 块注释内 ----
        if was_in_block:
            # 【关键】英文注释里的正常疑问句(如 "// stack is empty?")不算损坏。
            # 只有【连续2个及以上问号】才是被吃掉的中文。
            if is_damaged_run(line):
                comments.append((i, line))
            else:
                normal.append((i, line))
            continue

        # ---- 2. 整行注释 ----
        if stripped.startswith('//') or stripped.startswith('*') or stripped.startswith('/*'):
            if is_damaged_run(line):
                comments.append((i, line))
            else:
                normal.append((i, line))
            continue

        # ---- 3. 切掉行尾注释，只看代码部分 ----
        code_part = line
        if '//' in line:
            # 避免切到 "http://" 这种
            idx = line.find('//')
            before = line[:idx]
            if before.count('"') % 2 == 0:      # 引号成对 = 真注释
                code_part = before
                # 注释部分单独记（同样只认连续问号）
                if '?' in line[idx:] and is_damaged_run(line[idx:]):
                    comments.append((i, line))

        if '?' not in code_part:
            continue

        # ---- 4. 正常的三元运算符等 ----
        if looks_normal(code_part) and not is_damaged_run(code_part):
            normal.append((i, line))
            continue

        # ---- 5. 字符串里的问号 ----
        in_str = False
        for m in re.finditer(r'"(?:[^"\\]|\\.)*"', code_part):
            if '?' in m.group():
                in_str = True
                break

        if in_str:
            # 检查引号外还有没有问号
            outside = re.sub(r'"(?:[^"\\]|\\.)*"', '', code_part)
            if '?' in outside and is_damaged_run(outside):
                code.append((i, line))
            elif is_damaged_run(code_part):
                # 只有连续问号才是被吃掉的中文；
                # 英文字符串里的 "Really?" 之类是正常的
                strings.append((i, line))
            else:
                normal.append((i, line))
        else:
            if is_damaged_run(code_part):
                code.append((i, line))
            else:
                normal.append((i, line))

    return code, strings, comments, normal


def report(path, code_only=False):
    name = os.path.basename(path)
    code, strings, comments, normal = classify(path)

    total = len(code) + len(strings) + len(comments)
    if total == 0:
        print("[ OK ] %s  没有损坏的中文" % name)
        return 0

    print("=" * 66)
    print(" %s" % name)
    print("=" * 66)
    print("  [致命] 代码里的问号    %4d 行   <- 必须修，否则编译/逻辑错" % len(code))
    print("  [重要] 字符串里的问号  %4d 行   <- 玩家能看到乱码" % len(strings))
    print("  [可缓] 注释里的问号    %4d 行   <- 不影响编译" % len(comments))
    print("  [正常] 三元运算符等    %4d 行   <- 不是损坏，已排除" % len(normal))

    if code:
        print("\n  --- [致命] 代码损坏 ---")
        for i, l in code[:40]:
            print("  %5d: %s" % (i, l.strip()[:100]))
        if len(code) > 40:
            print("  ... 还有 %d 行" % (len(code) - 40))

    if not code_only and strings:
        print("\n  --- [重要] 字符串损坏（前25行）---")
        for i, l in strings[:25]:
            print("  %5d: %s" % (i, l.strip()[:100]))
        if len(strings) > 25:
            print("  ... 还有 %d 行" % (len(strings) - 25))

    if not code_only and comments:
        print("\n  --- [可缓] 注释损坏：共 %d 行（不逐条列出）---" % len(comments))
        rng = [i for i, _ in comments]
        print("        行号范围 %d ~ %d" % (min(rng), max(rng)))

    # 给结论
    print("\n  【结论】", end='')
    if code:
        print("代码逻辑受损，必须修复后才能用")
    elif strings:
        print("代码逻辑完好，但中文文本全毁 —— 能编译，游戏里显示乱码")
    else:
        print("只有注释受损，能正常编译运行，建议有空再补")

    return 1 if code else 2


def count_damage(path):
    """返回 (代码, 字符串, 注释) 三类的损坏行数"""
    try:
        c, s, m, _ = classify(path)
        return len(c), len(s), len(m)
    except Exception:
        return (0, 0, 0)


def main():
    args = sys.argv[1:]
    code_only = '--code-only' in args
    use_dir = '--dir' in args

    # --baseline <干净源码目录>：只报比基线【更严重】的文件，
    # 用来排除上游自带的问号（如 bot_Events.h 的 "%Complete: ???"）
    baseline = None
    if '--baseline' in args:
        bi = args.index('--baseline')
        if bi + 1 < len(args):
            baseline = args[bi + 1]

    files = [a for a in args if not a.startswith('--')]
    if baseline and baseline in files:
        files.remove(baseline)

    if not files:
        print(__doc__)
        return 1

    targets = []
    if use_dir:
        for d in files:
            for root, _, fs in os.walk(d):
                for fn in fs:
                    if fn.endswith(('.cpp', '.h', '.hpp', '.cc')):
                        targets.append(os.path.join(root, fn))
    else:
        targets = files

    worst = 0
    damaged = []
    skipped_same = 0

    for p in targets:
        if not os.path.isfile(p):
            print("[跳过] 不存在: %s" % p)
            continue

        # 基线对比：找对应的干净文件
        if baseline and use_dir:
            rel = os.path.relpath(p, files[0]).replace('\\', '/')
            bpath = os.path.join(baseline, rel)
            if os.path.isfile(bpath):
                mine = count_damage(p)
                base = count_damage(bpath)
                if mine <= base:        # 不比上游更差 -> 不是我们弄坏的
                    skipped_same += 1
                    continue

        rc = report(p, code_only)
        if rc:
            damaged.append((p, rc))
        worst = max(worst, rc)

    if use_dir:
        print("\n" + "=" * 66)
        print(" 汇总：扫描 %d 个文件，%d 个有损坏" % (len(targets), len(damaged)))
        if baseline:
            print(" （已用基线排除 %d 个上游自带问号的文件）" % skipped_same)
        print("=" * 66)
        for p, rc in damaged:
            tag = "【致命】" if rc == 1 else "【文本】"
            print("  %s %s" % (tag, p))
        if not damaged:
            print("  没有发现你改坏的文件")

    return worst


if __name__ == '__main__':
    sys.exit(main())
