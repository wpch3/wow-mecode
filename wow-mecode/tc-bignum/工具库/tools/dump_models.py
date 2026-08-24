# -*- coding: utf-8 -*-
r"""
dump_models.py —— 从你自己的 DBC 导出【模型ID总表】

用途
    .model 要 displayid，网上的号对不上你打了 HD 补丁的客户端。
    真正准确的表就在你硬盘里：
        <运行目录>\dbc\CreatureDisplayInfo.dbc
        <运行目录>\dbc\CreatureModelData.dbc
    本脚本把这两张表 JOIN 起来，导出成一个能用 Excel 打开、能 Ctrl+F 搜的 CSV。

用法（Git Bash 或 CMD 都行）
    python dump_models.py "D:\\TC-Build\\bin\\RelWithDebInfo\\dbc"
    python dump_models.py "D:\\TC-Build\\bin\\RelWithDebInfo\\dbc" -o 模型总表.csv
    python dump_models.py "D:\\TC-Build\\bin\\RelWithDebInfo\\dbc" -k wolf

输出
    模型总表.csv    列：displayid, modelid, 模型短名, 模型完整路径, 缩放, 玩家型
    直接 Excel 打开筛选，或用 -k 在命令行里搜。

DBC 格式（wowdev.wiki/DBC，3.3.5 通用）
    偏移0  uint32 magic       'WDBC'
    偏移4  uint32 recordCount
    偏移8  uint32 fieldCount
    偏移12 uint32 recordSize
    偏移16 uint32 stringSize
    之后   recordCount * recordSize 字节的记录区
    再之后 stringSize 字节的字符串池（字段里存的是池内偏移）

本脚本按【列号】读，不依赖任何外部定义文件。
列号来自 TrinityCore 源码实查：
    src/server/shared/DataStores/DBCStructure.h:442  CreatureDisplayInfoEntry
        列0 ID / 列1 ModelID / 列3 ExtendedDisplayInfoID / 列4 CreatureModelScale
    src/server/shared/DataStores/DBCStructure.h:496  CreatureModelDataEntry
        列0 ID / 列2 ModelName(字符串池偏移)
"""

import os
import sys
import csv
import struct
import argparse


def read_dbc(path):
    """读一个 3.3.5 DBC，返回 (records, stringblock)。
    records 是 list[tuple[int,...]]，每个字段按 uint32 原样读出。"""
    with open(path, 'rb') as f:
        data = f.read()

    if len(data) < 20:
        raise ValueError("文件太小，不是 DBC: %s" % path)

    magic = data[0:4]
    if magic != b'WDBC':
        raise ValueError("magic 不是 WDBC（可能是 DB2/加密文件）: %s" % path)

    rec_count, field_count, rec_size, str_size = struct.unpack('<4I', data[4:20])

    header = 20
    rec_end = header + rec_count * rec_size
    if rec_end + str_size > len(data):
        raise ValueError("文件被截断: %s" % path)

    if rec_size != field_count * 4:
        # 3.3.5 的 DBC 全部是 4 字节字段，对不上说明版本不对
        sys.stderr.write(
            "  [warn] %s recordSize=%d != fieldCount*4=%d，可能不是3.3.5\n"
            % (os.path.basename(path), rec_size, field_count * 4))

    strings = data[rec_end:rec_end + str_size]

    records = []
    fmt = '<%dI' % field_count
    for i in range(rec_count):
        off = header + i * rec_size
        records.append(struct.unpack_from(fmt, data, off))

    return records, strings, field_count


def get_string(strings, offset):
    """从字符串池按偏移取 C 字符串。"""
    if offset == 0 or offset >= len(strings):
        return ''
    end = strings.find(b'\x00', offset)
    if end < 0:
        end = len(strings)
    raw = strings[offset:end]
    # DBC 里是路径，基本纯 ASCII；用 latin-1 兜底保证不抛异常
    try:
        return raw.decode('utf-8')
    except UnicodeDecodeError:
        return raw.decode('latin-1')


def short_name(full):
    """Creature\\Wolf\\Wolf.mdx -> Wolf"""
    if not full:
        return ''
    s = full.replace('/', '\\')
    if '\\' in s:
        s = s.rsplit('\\', 1)[1]
    if '.' in s:
        s = s.rsplit('.', 1)[0]
    return s


def main():
    ap = argparse.ArgumentParser(
        description='从 CreatureDisplayInfo.dbc + CreatureModelData.dbc 导出模型总表')
    ap.add_argument('dbcdir', help='dbc 目录，例如 D:\\TC-Build\\bin\\RelWithDebInfo\\dbc')
    ap.add_argument('-o', '--out', default='模型总表.csv', help='输出 CSV 文件名')
    ap.add_argument('-k', '--keyword', default=None,
                    help='只列出模型路径含该关键字的行（不区分大小写），并打印到屏幕')
    args = ap.parse_args()

    dbcdir = args.dbcdir
    p_disp = os.path.join(dbcdir, 'CreatureDisplayInfo.dbc')
    p_model = os.path.join(dbcdir, 'CreatureModelData.dbc')

    for p in (p_disp, p_model):
        if not os.path.isfile(p):
            sys.stderr.write("找不到 %s\n" % p)
            sys.stderr.write("确认路径是【服务端运行目录下的 dbc 文件夹】，\n")
            sys.stderr.write("通常是 D:\\TC-Build\\bin\\RelWithDebInfo\\dbc\n")
            return 2

    print("读取 CreatureModelData.dbc ...")
    md_recs, md_str, md_fields = read_dbc(p_model)
    print("  %d 行, %d 列" % (len(md_recs), md_fields))
    if md_fields < 3:
        sys.stderr.write("  列数不足3，无法取 ModelName\n")
        return 3

    # ModelData: 列0=ID, 列2=ModelName(池偏移)
    model_name = {}
    for r in md_recs:
        model_name[r[0]] = get_string(md_str, r[2])

    print("读取 CreatureDisplayInfo.dbc ...")
    di_recs, di_str, di_fields = read_dbc(p_disp)
    print("  %d 行, %d 列" % (len(di_recs), di_fields))
    if di_fields < 5:
        sys.stderr.write("  列数不足5，无法取 Scale\n")
        return 3

    kw = args.keyword.lower() if args.keyword else None

    rows = []
    missing = 0
    for r in di_recs:
        did = r[0]
        mid = r[1]
        extended = r[3]
        scale = struct.unpack('<f', struct.pack('<I', r[4]))[0]

        full = model_name.get(mid)
        if full is None:
            missing += 1
            continue
        if not full:
            continue

        if kw and kw not in full.lower():
            continue

        rows.append({
            'displayid': did,
            'modelid': mid,
            '模型短名': short_name(full),
            '模型完整路径': full,
            '缩放': '%.2f' % scale,
            '玩家型': '是' if extended else '',
        })

    rows.sort(key=lambda x: x['displayid'])

    # 写 CSV。用 utf-8-sig，Excel 双击打开中文不乱码。
    fields = ['displayid', 'modelid', '模型短名', '模型完整路径', '缩放', '玩家型']
    with open(args.out, 'w', newline='', encoding='utf-8-sig') as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        w.writerows(rows)

    print("")
    print("导出完成: %s" % os.path.abspath(args.out))
    print("  有效行 %d" % len(rows))
    if missing:
        print("  跳过坏行 %d（ModelID 在 CreatureModelData 里不存在）" % missing)

    if kw:
        print("")
        print("含 \"%s\" 的模型：" % args.keyword)
        print("  %-10s %-10s %s" % ('displayid', 'modelid', '模型'))
        for x in rows[:80]:
            print("  %-10s %-10s %s" % (x['displayid'], x['modelid'], x['模型短名']))
        if len(rows) > 80:
            print("  ... 还有 %d 条，看 CSV" % (len(rows) - 80))
        print("")
        print("用法: .model id <displayid>")

    return 0


if __name__ == '__main__':
    sys.exit(main())
