#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
交付 SQL 前的强制自检 —— 每一条都对应一个踩过的坑

用法:
    python3 tools/check_sql.py 你的.sql [更多.sql ...]

检查项:
  1. 存储过程/DELIMITER      -> DBeaver 不识别，会在第一个分号切断
  2. 表引用未写库名           -> No database selected (1046)
  3. 逐条 sqlglot 解析        -> 整体解析会漏掉切分问题
  4. GBK 兼容                 -> 用户环境是 GBK
"""
import re
import sys

try:
    import sqlglot
except ImportError:
    sqlglot = None


def strip_comments(text):
    """去掉 -- 行注释，保留 SQL 主体"""
    return '\n'.join(
        l for l in text.split('\n') if not l.strip().startswith('--')
    )


def check_procedure(sql_body):
    """坑1: DBeaver 默认不识别 DELIMITER"""
    hits = [k for k in ('DELIMITER', 'CREATE PROCEDURE', 'CREATE FUNCTION',
                        'CREATE TRIGGER', 'DECLARE CURSOR')
            if k in sql_body.upper()]
    return hits


def check_unqualified(text):
    """坑2: 表引用必须写 `库`.`表`，临时表也一样"""
    pat = re.compile(
        r'\b(FROM|INTO|UPDATE|TABLE|JOIN)\s+'
        r'(?:IF\s+(?:NOT\s+)?EXISTS\s+)?'
        r'`([A-Za-z_][A-Za-z0-9_]*)`(?!\s*\.)',
        re.I)
    bad = []
    for i, l in enumerate(text.split('\n'), 1):
        if l.strip().startswith('--'):
            continue
        for m in pat.finditer(l):
            bad.append((i, m.group(1), m.group(2)))
    return bad


def check_parse(text):
    """坑3: 按分号切分后【逐条】解析（模拟客户端行为）"""
    if sqlglot is None:
        return None, ['(未安装 sqlglot，跳过)']
    errs = []
    ok = 0
    for i, st in enumerate(re.split(r';\s*\n', text), 1):
        body = strip_comments(st).strip()
        if not body:
            continue
        try:
            sqlglot.parse_one(body, read='mysql')
            ok += 1
        except Exception as e:
            errs.append(f"#{i}: {body[:60]}... | {str(e)[:80]}")
    return ok, errs


def check_like_pk(text):
    """坑5: CREATE TEMPORARY TABLE ... LIKE 会连主键一起复制
    如果之后要插入多行相同主键值，必须先 DROP PRIMARY KEY"""
    body = strip_comments(text)
    likes = re.findall(r'CREATE\s+TEMPORARY\s+TABLE\s+`?(?:\w+`?\.`?)?(\w+)`?\s+LIKE',
                       body, re.I)
    warns = []
    for t in likes:
        # 该临时表后面有没有 DROP PRIMARY KEY
        if not re.search(r'ALTER\s+TABLE\s+`?(?:\w+`?\.`?)?' + t + r'`?\s+DROP\s+PRIMARY\s+KEY',
                         body, re.I):
            # 只有当它被 INSERT 多行时才危险
            if re.search(r'INSERT\s+INTO\s+`?(?:\w+`?\.`?)?' + t + r'`?', body, re.I):
                warns.append(t)
    return warns


def check_gbk(text):
    """坑4: 用户环境 GBK，emoji/特殊符号会乱码"""
    bad = set()
    for c in text:
        try:
            c.encode('gbk')
        except UnicodeEncodeError:
            bad.add(c)
    return sorted(bad)


def check_commented_sql(path):
    """
    【2026-08-05 新增】检查【被注释掉】的 SQL 块还原后是否语法正确。

    背景：交付的 SQL 常把"会改数据"的部分注释掉防误操作。
    但注释掉的代码不会被任何检查覆盖 —— 用户取消注释执行时才发现有错。
    A35 就差点栽在这：2.3 段语法虽对，但用 (@i := @i+1) 生成行号，
    有4个逻辑缺陷（变量不重置/MySQL8求值顺序无保证/派生表ORDER BY被忽略/
    两个子查询LIMIT不对称）。已改用 ROW_NUMBER()。
    """
    try:
        import sqlglot
    except ImportError:
        return None, "(未安装 sqlglot，跳过)"

    import re
    txt = open(path, encoding='utf-8').read()
    restored = []
    for l in txt.split('\n'):
        t = l.rstrip()
        if t.startswith('-- '):
            b = t[3:]
            if set(b.strip()) <= set('-') and len(b.strip()) > 5:
                continue
            restored.append(b)
        elif t == '--':
            restored.append('')
    blob = '\n'.join(restored)

    stmts = re.findall(
        r'(?ms)^\s*(INSERT\s+INTO.*?;|DELETE\s+\w+.*?;|UPDATE\s+`?\w.*?;|'
        r'SET\s+@\w+\s*:=.*?;|SELECT\s+ROW_COUNT.*?;)', blob)
    if not stmts:
        return True, "无被注释的SQL块"

    bad = []
    for st in stmts:
        try:
            sqlglot.parse_one(st, dialect='mysql')
        except Exception as e:
            bad.append((' '.join(st.split())[:50], str(e)[:80]))
    if bad:
        return False, "被注释的SQL有 %d 条解析失败: %s" % (len(bad), bad[:3])
    return True, "被注释的SQL %d 条，还原后语法全部正确" % len(stmts)



def check_multitable_delete(text):
    """
    【2026-08-05 新增】检查 MySQL 多表 DELETE 的别名陷阱。

    背景：用户执行 `DELETE c FROM \`world\`.\`creature\` c JOIN ...`
    报 ERROR 1046 No database selected。

    根因：DELETE 后面的 `c` 是【裸标识符】，MySQL 会当成
          `当前库`.`c` 去解析。DBeaver 没 USE 过库 -> 1046。
          就算 FROM 里写了全限定名也没用 —— 那是两个独立的解析位置。

    修法：改成 `DELETE FROM \`库\`.\`表\` WHERE ... IN (子查询)`，
          不用别名，也就没有裸标识符。
    """
    import re
    bad = []
    for i, line in enumerate(text.split('\n'), 1):
        s = line.strip()
        # 跳过纯注释行以外的（注释里的也要查，因为用户会取消注释）
        if s.startswith('--'):
            s = s.lstrip('-').strip()
        # DELETE <别名> FROM   或   DELETE <别名>.* FROM
        m = re.match(r'^DELETE\s+([A-Za-z_]\w*)(\.\*)?\s+FROM\s', s, re.I)
        if m and m.group(1).upper() != 'FROM':
            bad.append((i, s[:60]))
    return bad


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1

    all_ok = True
    for path in sys.argv[1:]:
        print(f"\n{'=' * 66}")
        print(f" {path}")
        print('=' * 66)
        try:
            text = open(path, encoding='utf-8').read()
        except Exception as e:
            print(f"  [错误] 无法读取: {e}")
            all_ok = False
            continue

        body = strip_comments(text)
        failed = False

        # 1
        hits = check_procedure(body)
        if hits:
            print(f"  [FAIL] 含存储过程语法: {hits}")
            print("         DBeaver 不识别 DELIMITER，会在第一个分号切断")
            print("         -> 改用【数字辅助表 + INSERT SELECT】")
            failed = True
        else:
            print("  [OK]   无存储过程/DELIMITER")

        # 2
        bad = check_unqualified(text)
        if bad:
            print(f"  [FAIL] {len(bad)} 处表引用未写库名 -> 会报 1046:")
            for ln, kw, tbl in bad[:8]:
                print(f"           {ln:>4}: {kw} `{tbl}`  应为 `库`.`{tbl}`")
            if len(bad) > 8:
                print(f"           ... 还有 {len(bad) - 8} 处")
            print("         注意：临时表也要写库名！")
            failed = True
        else:
            print("  [OK]   所有表引用都是完全限定名")

        # 3
        ok, errs = check_parse(text)
        if errs and ok is not None:
            print(f"  [FAIL] 逐条解析: {ok} 通过, {len(errs)} 失败")
            for e in errs[:5]:
                print(f"           {e}")
            failed = True
        elif ok is not None:
            print(f"  [OK]   逐条解析全部通过（{ok} 条）")
        else:
            print(f"  [跳过] {errs[0]}")

        # 5
        pkw = check_like_pk(text)
        if pkw:
            print(f"  [警告] 这些临时表用 LIKE 建表且会被 INSERT，")
            print(f"         但没有 DROP PRIMARY KEY: {pkw}")
            print("         若插入多行相同主键值会报 1062 Duplicate entry")
        else:
            print("  [OK]   LIKE建表的主键处理正确")

        # 4
        bad_ch = check_gbk(text)
        if bad_ch:
            print(f"  [FAIL] {len(bad_ch)} 个字符 GBK 不兼容: {bad_ch[:10]}")
            failed = True
        else:
            print("  [OK]   GBK 兼容")

        # 4.5 【2026-08-05 新增】多表DELETE的别名陷阱（ERROR 1046）
        mtd = check_multitable_delete(text)
        if mtd:
            print(f"  [FAIL] {len(mtd)} 处多表DELETE用了别名 -> 会报 1046:")
            for ln, snip in mtd[:5]:
                print(f"           {ln:>4}: {snip}")
            print("         DELETE 后的裸别名会被当成 `当前库`.`别名` 解析")
            print("         -> 改成 DELETE FROM `库`.`表` WHERE ... IN (子查询)")
            failed = True
        else:
            print("  [OK]   无多表DELETE别名陷阱")

        # 5 【2026-08-05 新增】被注释掉的 SQL 块也要检查
        ok5, msg5 = check_commented_sql(path)
        if ok5 is None:
            print(f"  [跳过] {msg5}")
        elif ok5:
            print(f"  [OK]   {msg5}")
        else:
            print(f"  [FAIL] {msg5}")
            print("         注释掉的SQL用户取消注释就会执行，必须同样正确")
            failed = True

        if failed:
            all_ok = False

    print(f"\n{'=' * 66}")
    print(" 全部通过，可以交付" if all_ok else " 有问题，修完再给用户")
    print('=' * 66)
    return 0 if all_ok else 1


if __name__ == '__main__':
    sys.exit(main())

