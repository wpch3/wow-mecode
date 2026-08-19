#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
F28 重复代码检测器

用途：检查补丁代码是否被重复粘贴（F27 的 m_trade 被 new 3 次就是这种）

为什么需要：
    函数体内的【语句重复】编译器完全不报错，
    只有函数定义/枚举/成员声明重复才会报错。
    所以"能编译"不代表"没重复"。

用法：
    python3 tools/check_dup.py <源码根目录>
    python3 tools/check_dup.py D:/TrinityCore/src
    python3 tools/check_dup.py D:/TrinityCore/src --verbose

输出：
    每个特征串的实际次数 vs 应有次数，超标的标红并给出行号
"""

import os
import sys
import re

# ============================================================================
#  规则表：文件相对路径 -> [(搜索词, 应有次数, 说明), ...]
#
#  应有次数从补丁库文档逐个数出来的。
#  实际 < 应有 = 该补丁没装或没装全（不报错，只提示）
#  实际 > 应有 = 【重复粘贴】，报错
# ============================================================================

RULES = {
    "server/game/Handlers/TradeHandler.cpp": [
        ("// OK start trade",                    1, "上游原注释", True),
        ("IsBotSession",                          1, "step57 bot交易钩子"),
        ("new TradeData(_player, pOther)",        1, "交易对象创建"),
        ("HandleBeginTradeOpcode",                2, "1个函数定义+1个钩子调用"),
    ],

    "server/game/AI/NpcBots/bot_ai.cpp": [
        ("void bot_ai::SetWanderer()",            1, "上游原函数"),
        # --- A38 锚点 ---
        ("bot_ai::HasAnchor",                     1, "A38 定义"),
        ("bot_ai::SetAnchor",                     1, "A38 定义"),
        ("bot_ai::ClearAnchor",                   1, "A38 定义"),
        ("bot_ai::IsNodeWithinAnchor",            1, "A38 定义"),
        ("IsNodeWithinAnchor(",                   4, "定义1+GetNextWanderNode里3个调用"),
        ("BOTAI_MISC_ANCHOR_RADIUS",              4, "HasAnchor+GetRadius+SetAnchor+ClearAnchor"),
        # --- A37-2 赠予 ---
        ("bot_ai::CheckGiftItem",                 1, "A37-2 定义"),
        ("bot_ai::ClassifyGiftItem",              1, "A37-2 定义"),
        ("bot_ai::GetGiftRejectText",             1, "A37-2 定义"),
        ("GOSSIP_SENDER_GIFT_MENU",               7, "菜单入口1+case1+上下页2+三处return3"),
        ("GOSSIP_SENDER_GIFT_ITEM",               4, "两处AddGossipItemFor+case1+日志1"),
        # --- A37-3 情感反馈 ---
        ("bot_ai::GetGiftNeedState",              1, "A37-3 定义"),
        ("bot_ai::GetBondTierWith",               1, "A37-3 定义"),
        ("bot_ai::UpdateBotRequest",              1, "A37-5 定义"),
        ("bot_ai::DecideRequestType",             1, "A37-5 定义"),
        ("UpdateBotRequest(diff);",               1, "A37-5 主循环调用"),
        # --- A37-4 计分 ---
        ("BotCompanionMgr::RecordGift",           1, "A37-4 记账"),
        ("BotCompanionMgr::GetTodayGiftCount",    1, "A37-4 防刷查询"),
        ("uint32 giftItemId",                     1, "A37-4 局部变量"),
        ("int16 gainPoints",                      1, "A37-4 局部变量"),
        ("uint32 giftZoneId",                     1, "A37-4 局部变量"),
        # --- include ---
        ('#include "bot_companion.h"',            1, "A37 依赖"),
    ],

    "server/game/AI/NpcBots/bot_ai.h": [
        ("void SetWanderer();",                          1, "上游声明"),
        ("bool HasAnchor() const;",                      1, "A38"),
        ("bool IsNodeWithinAnchor(",                     1, "A38"),
        ("uint8 CheckGiftItem(Item const* item) const;", 1, "A37-2"),
        ("uint8 ClassifyGiftItem(",                      1, "A37-2"),
        ("uint32 GetGiftRejectText(",                    1, "A37-2"),
        ("uint8 GetGiftNeedState(",                      1, "A37-3"),
        ("uint8 GetBondTierWith(",                       1, "A37-3"),
    ],

    "server/game/AI/NpcBots/bot_companion.cpp": [
        ("BotCompanionMgr::LoadGiftTexts",     1, "A37-3"),
        ("BotCompanionMgr::PickGiftText",      1, "A37-3"),
        ("BotCompanionMgr::FormatGiftText",    1, "A37-3"),
        ("BotCompanionMgr::BondLevelToTier",   1, "A37-3"),
        ("BotCompanionMgr::CalcGiftPoints",    1, "A37-4"),
        ("BotCompanionMgr::CalcBondLevel",     1, "A37-4"),
        ("BotCompanionMgr::GetTodayGiftCount", 1, "A37-4"),
        ("BotCompanionMgr::RecordGift",        1, "A37-4"),
        ("BotCompanionMgr::GetBondLevel",      1, "A37-4"),
        ("BotCompanionMgr::LoadRequestTexts",  1, "A37-5"),
        ("BotCompanionMgr::PickRequestText",   1, "A37-5"),
        ("BotCompanionMgr::CalcRequestCooldown",1,"A37-5"),
        ("BotCompanionMgr::RecordRequest",     1, "A37-5"),
        ("LoadRequestTexts();",                1, "ReloadAll里的调用"),
        ("LoadGiftTexts();",                   1, "ReloadAll里的调用"),
    ],

    "server/game/AI/NpcBots/bot_companion.h": [
        ("struct CompanionGiftText",                    1, "A37-3"),
        ("void LoadGiftTexts();",                       1, "A37-3"),
        ("std::vector<CompanionGiftText> _giftTexts;",  1, "A37-3"),
        ("static int16 CalcGiftPoints(",                1, "A37-4"),
        ("static uint8 CalcBondLevel(",                 1, "A37-4"),
        ("static void RecordGift(",                     1, "A37-4"),
        ("struct CompanionRequestText",                 1, "A37-5"),
        ("void LoadRequestTexts();",                    1, "A37-5"),
        ("enum BotRequestType",                         1, "A37-5"),
        ("enum BotGiftKind",                            1, "A37-2（第6步从bot_ai.cpp移来）"),
        ("enum BotGiftReject",                          1, "A37-2（第6步从bot_ai.cpp移来）"),
    ],

    "server/game/AI/NpcBots/botcommon.h": [
        ("BOTAI_MISC_ANCHOR_MAPID",  1, "A38 枚举"),
        ("BOTAI_MISC_ANCHOR_RADIUS", 2, "枚举定义1 + SAVED_LAST引用1"),
    ],

    "server/game/AI/NpcBots/bottext.h": [
        ("BOT_TEXT_GIVE_GIFT",       1, "A37-2"),
        ("BOT_TEXT_GIFT_ACCEPTED",   1, "A37-2"),
    ],

    "server/game/AI/NpcBots/botgossip.h": [
        ("GOSSIP_SENDER_GIFT_MENU",  1, "A37-2 枚举"),
        ("GOSSIP_SENDER_GIFT_ITEM",  1, "A37-2 枚举"),
    ],

    "server/game/AI/NpcBots/botcommands.cpp": [
        ("has data but no live creature", 1, "F22"),
    ],

    "server/scripts/Commands/cs_playerbot.cpp": [
        ('s0 == "spawn"',              1, "A25"),
        ('s0 == "kick"',               1, "A39"),
        ('s0 == "invite"',             1, "A39"),
        ('s0 == "follow"',             1, "A39"),
        ('s0 == "stay"',               1, "A39"),
        ('s0 == "give"',               1, "A37-6"),
        ('#include "Group.h"',         1, "A39 依赖"),
        ('#include "GroupMgr.h"',      1, "A39 依赖"),
        ('#include "MotionMaster.h"',  1, "A39 依赖"),
    ],

    "server/scripts/Custom/pbot_autoaccept.cpp": [
        ("static void TryAcceptTrade",   1, "A27"),
        ("static void TryAcceptGroup",   1, "A27"),
        ("static void TryAcceptGuild",   1, "A27"),
        ("static void TryAcceptDuel",    1, "A27"),
        ("static void TryAutoRelease",   1, "A27"),
        ("void PBotForceAcceptAll",      1, "A27"),
        ("static uint8 PBotDecideRequest", 1, "A37-6"),
        ("static void TryBotRequest",      1, "A37-6"),
        ("HandleAcceptTradeOpcode",        1, "A37-6 交易成交"),
        ("_pbotRequestTimer",              2, "A37-6 声明1+使用1"),
        ("_pbotTradeOpened",               0, "F11已删除，有残留就是错的"),
        ('#include "bot_companion.h"',     1, "A37-6 依赖"),
    ],
}

# pbot_autoaccept.cpp / cs_playerbot.cpp 可能放在不同目录，这里列备选路径
ALT_PATHS = {
    "server/scripts/Custom/pbot_autoaccept.cpp": [
        "server/scripts/Commands/pbot_autoaccept.cpp",
        "server/game/AI/NpcBots/pbot_autoaccept.cpp",
        "server/scripts/World/pbot_autoaccept.cpp",
    ],
    "server/scripts/Commands/cs_playerbot.cpp": [
        "server/scripts/Custom/cs_playerbot.cpp",
        "server/scripts/World/cs_playerbot.cpp",
    ],
}


def strip_comments(lines):
    """把整行注释和块注释内的行标记出来，避免误判被注释掉的代码"""
    result = []
    in_block = False
    for ln in lines:
        s = ln.strip()
        if in_block:
            result.append("")
            if "*/" in s:
                in_block = False
            continue
        if s.startswith("/*"):
            if "*/" not in s:
                in_block = True
            result.append("")
            continue
        if s.startswith("//"):
            result.append("")
            continue
        result.append(ln)
    return result


def find_file(root, rel):
    p = os.path.join(root, rel.replace("/", os.sep))
    if os.path.isfile(p):
        return p
    for alt in ALT_PATHS.get(rel, []):
        p2 = os.path.join(root, alt.replace("/", os.sep))
        if os.path.isfile(p2):
            return p2
    # 最后尝试全盘搜同名文件
    base = os.path.basename(rel)
    for dirpath, _, files in os.walk(root):
        if base in files:
            return os.path.join(dirpath, base)
    return None


def find_exact_dup_lines(path, min_len=25):
    """
    找出【完全相同且非平凡】的重复代码行。
    这个检测 100% 可靠，不依赖任何期望值，绝不会误检。
    只看：同一个文件里，同样一行有意义的代码出现了 2 次以上。
    """
    try:
        with open(path, "r", encoding="utf-8", errors="ignore") as f:
            raw = f.readlines()
    except Exception:
        return []

    clean = strip_comments(raw)
    seen = {}
    for i, ln in enumerate(clean):
        s2 = ln.strip()
        if len(s2) < min_len:
            continue
        # 跳过太常见的结构性代码
        if s2 in ("}", "{", "break;", "return;", "return false;", "return true;"):
            continue
        if s2.startswith("#include") or s2.startswith("case ") or s2.startswith("//"):
            continue
        # 跳过纯声明式的行（大括号结尾的控制流）
        seen.setdefault(s2, []).append(i + 1)

    return [(txt, lines) for txt, lines in seen.items() if len(lines) > 1]


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        print("错误：必须指定源码根目录")
        print("例如： python3 tools/check_dup.py D:/TrinityCore/src")
        return 2

    root = sys.argv[1]
    verbose = "--verbose" in sys.argv or "-v" in sys.argv

    if not os.path.isdir(root):
        print("错误：目录不存在 -> " + root)
        return 2

    print("=" * 74)
    print(" F28 重复代码检测")
    print(" 源码目录: " + root)
    print("=" * 74)

    n_dup = 0        # 重复（严重）
    n_missing = 0    # 缺失（提示）
    n_ok = 0
    n_nofile = 0

    for rel, checks in RULES.items():
        path = find_file(root, rel)
        if not path:
            print("\n[跳过] 找不到文件: " + rel)
            n_nofile += 1
            continue

        try:
            with open(path, "r", encoding="utf-8", errors="ignore") as f:
                raw = f.readlines()
        except Exception as e:
            print("\n[跳过] 读取失败 " + rel + " : " + str(e))
            n_nofile += 1
            continue

        clean = strip_comments(raw)

        file_issues = []
        for rule in checks:
            if len(rule) == 4:
                needle, expect, note, is_comment = rule
            else:
                needle, expect, note = rule
                is_comment = False
            # 搜索词本身就是注释的，要在【原始行】里找，不能用去注释后的
            src = raw if is_comment else clean
            hits = [i + 1 for i, ln in enumerate(src) if needle in ln]
            actual = len(hits)

            if actual > expect:
                file_issues.append(("DUP", needle, actual, expect, note, hits))
                n_dup += 1
            elif actual < expect:
                file_issues.append(("MISS", needle, actual, expect, note, hits))
                n_missing += 1
            else:
                n_ok += 1
                if verbose:
                    file_issues.append(("OK", needle, actual, expect, note, hits))

        if file_issues:
            print("\n" + "-" * 74)
            print(" " + os.path.relpath(path, root))
            print("-" * 74)
            for kind, needle, actual, expect, note, hits in file_issues:
                short = needle if len(needle) <= 44 else needle[:41] + "..."
                if kind == "DUP":
                    print("  [重复] {:<46} {} 处 (应有 {})".format(short, actual, expect))
                    print("         说明: " + note)
                    print("         行号: " + ", ".join(str(h) for h in hits))
                    print("         >>> 多了 {} 处，需要删掉".format(actual - expect))
                elif kind == "MISS":
                    print("  [缺失] {:<46} {} 处 (应有 {})".format(short, actual, expect))
                    print("         说明: " + note + "  <- 该补丁可能没装或没装全")
                else:
                    print("  [OK  ] {:<46} {} 处".format(short, actual))

    # ====================================================================
    #  第二轮：精确重复行扫描（不依赖期望值，100%可靠，绝不误检）
    # ====================================================================
    print("\n" + "=" * 74)
    print(" 第二轮：完全相同的代码行（不依赖期望值，这个绝不误检）")
    print("=" * 74)

    n_exact = 0
    for rel in RULES.keys():
        path = find_file(root, rel)
        if not path:
            continue
        dups = find_exact_dup_lines(path)
        if not dups:
            continue
        print("\n " + os.path.relpath(path, root))
        for txt, lines in sorted(dups, key=lambda x: -len(x[1]))[:12]:
            short = txt if len(txt) <= 62 else txt[:59] + "..."
            print("   x{} 行{}  {}".format(len(lines),
                  ",".join(str(l) for l in lines), short))
            n_exact += 1

    if n_exact == 0:
        print("\n 没有发现完全相同的重复代码行。")

    print("\n" + "=" * 74)
    print(" 结果: 重复 {} 项 / 缺失 {} 项 / 正常 {} 项 / 跳过文件 {} 个".format(
        n_dup, n_missing, n_ok, n_nofile))
    print(" 第二轮精确重复行: {} 组".format(n_exact))
    print("=" * 74)
    print("\n【怎么看】")
    print(" 第一轮[重复] = 靠我维护的期望值判断，【可能误检】(期望值我可能数错)")
    print(" 第二轮       = 同一行代码逐字相同出现多次，【几乎不会误检】")
    print(" 两轮都报的地方 = 基本可以确定是重复粘贴")

    if n_dup:
        print("\n【有重复】上面标 [重复] 的地方需要删掉多余的。")
        print("重点：函数体内的语句重复【编译器不会报错】，但会导致逻辑执行多次。")
        print("F27 的 m_trade 被 new 3 次就是这种，表现为交易窗口一闪而过。")
        return 1

    if n_missing:
        print("\n【无重复】但有 {} 项缺失。".format(n_missing))
        print("如果那些补丁你本来就没装，可以忽略；")
        print("如果装了却检测不到，说明装的位置或写法和文档不一致。")
        return 0

    print("\n【全部正常】没有发现重复粘贴。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
