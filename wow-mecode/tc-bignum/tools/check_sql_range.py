# -*- coding: utf-8 -*-
"""
check_sql_range.py —— SQL 数值是否超出列类型范围

起因：38_service_npc_gossip.sql 用 MenuID=96001，
      但 gossip_menu.MenuID 是 smallint unsigned（上限 65535），
      直接报 1264 Data truncation。

这类错误肉眼很难发现 —— 同一个概念在不同表里类型可能不一样：
    creature_template.gossip_menu_id  int unsigned    上限 42 亿
    gossip_menu.MenuID                smallint unsigned 上限 65535
所以必须按【每张表的实际列类型】逐列校验。

用法：
    python3 tools/check_sql_range.py sql/38_service_npc_gossip.sql
    python3 tools/check_sql_range.py sql/*.sql
"""
import re, sys, os, urllib.request

LIMITS = {
    'tinyint':   (0, 255,        -128,       127),
    'smallint':  (0, 65535,      -32768,     32767),
    'mediumint': (0, 16777215,   -8388608,   8388607),
    'int':       (0, 4294967295, -2147483648, 2147483647),
    'bigint':    (0, 18446744073709551615, -9223372036854775808, 9223372036854775807),
}

SCHEMA_CACHE = '/tmp/wdb_schema.sql'
SCHEMA_URL = ('https://raw.githubusercontent.com/328950225/'
              'TrinityCore-NPCBOT-Eluna-zhCN/NPCBOT-Eluna-zhCN-2026/'
              'sql/base/dev/world_database.sql')


def load_schema():
    if not os.path.exists(SCHEMA_CACHE):
        try:
            urllib.request.urlretrieve(SCHEMA_URL, SCHEMA_CACHE)
        except Exception as e:
            print("无法下载表结构:", e)
            return {}
    txt = open(SCHEMA_CACHE, encoding='utf-8', errors='replace').read()
    tables = {}
    for m in re.finditer(r'CREATE TABLE `(\w+)` \((.*?)\n\) ENGINE', txt, re.S):
        name, body = m.group(1), m.group(2)
        cols = {}
        for cm in re.finditer(r'^\s+`(\w+)`\s+(\w+)(?:\(\d+\))?\s*(unsigned)?', body, re.M):
            col, typ, uns = cm.group(1), cm.group(2).lower(), bool(cm.group(3))
            if typ in LIMITS:
                lo, hi = (LIMITS[typ][0], LIMITS[typ][1]) if uns else (LIMITS[typ][2], LIMITS[typ][3])
                cols[col] = (typ + (' unsigned' if uns else ''), lo, hi)
        tables[name] = cols
    return tables


def split_values(row):
    out, depth, cur, q = [], 0, '', False
    for ch in row:
        if ch == "'" and (not cur or cur[-1] != '\\'):
            q = not q
        if not q:
            if ch == '(':
                depth += 1
            elif ch == ')':
                depth -= 1
            elif ch == ',' and depth == 0:
                out.append(cur.strip()); cur = ''; continue
        cur += ch
    if cur.strip():
        out.append(cur.strip())
    return out


def check_file(path, tables):
    src = open(path, encoding='utf-8').read()
    body = '\n'.join(l for l in src.split('\n') if not l.strip().startswith('--'))
    problems = []
    for m in re.finditer(
            r'(?:REPLACE|INSERT)\s+(?:IGNORE\s+)?INTO\s+`?\w+`?\.?`?(\w+)`?\s*\((.*?)\)\s*VALUES(.*?)(?=;|\Z)',
            body, re.S | re.I):
        table, colstr, valstr = m.group(1), m.group(2), m.group(3)
        cols = [c.strip().strip('`') for c in colstr.replace('\n', ' ').split(',') if c.strip()]
        schema = tables.get(table)
        if not schema:
            continue
        for rn, rm in enumerate(re.finditer(r'\(([^()]*(?:\([^()]*\)[^()]*)*)\)', valstr), 1):
            vals = split_values(rm.group(1))
            if len(vals) != len(cols):
                continue
            for col, val in zip(cols, vals):
                info = schema.get(col)
                if not info:
                    continue
                typ, lo, hi = info
                if not re.fullmatch(r'-?\d+', val):
                    continue
                n = int(val)
                if n < lo or n > hi:
                    problems.append((table, rn, col, n, typ, lo, hi))
    return problems


def main():
    tables = load_schema()
    if not tables:
        sys.exit(2)
    bad = 0
    for path in sys.argv[1:]:
        name = os.path.basename(path)
        probs = check_file(path, tables)
        if probs:
            bad += 1
            print("[FAIL] %s" % name)
            for t, rn, col, n, typ, lo, hi in probs:
                print("        %s.%s 第%d行 值 %d 超出 %s 范围 [%d, %d]"
                      % (t, col, rn, n, typ, lo, hi))
        else:
            print("[ OK ] %s  所有数值都在列类型范围内" % name)
    sys.exit(1 if bad else 0)


main()
