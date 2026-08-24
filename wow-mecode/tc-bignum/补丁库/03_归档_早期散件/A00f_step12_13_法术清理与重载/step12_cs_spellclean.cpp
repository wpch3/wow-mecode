/*
 * ============================================================================
 *  技能书清理 —— cs_spellclean.cpp
 * ============================================================================
 *
 *  解决的问题：
 *    技能书里堆满了低等级技能。比如学了「火球术 Rank16」，
 *    前面 15 个 Rank 全都还在书里，翻起来眼花缭乱。
 *
 *    正常情况下 TrinityCore 学高阶会自动顶掉低阶，但以下情况会残留：
 *      · 用 .learn all / PlayerStart.AllSpells = 1 批量学的
 *      · 数据库直接塞进去的
 *      · 跨版本升级留下的
 *      · 天赋重置后的残留
 *
 *  ── 指令 ──────────────────────────────────────────────────────────────
 *   .spell clean              预览会清理哪些（不实际删除）
 *   .spell clean confirm      执行清理
 *   .spell count              统计当前技能数量
 *   .spell find <关键词>      按名称搜索自己会的技能
 *
 *  ── 安全设计 ──────────────────────────────────────────────────────────
 *   1. 默认只预览不删除，必须加 confirm 才真删
 *   2. 只删「同一条技能链上、且有更高等级」的技能
 *   3. 被动技能、天赋、专业配方一律不动
 *   4. 保留最高 Rank，只删低于它的
 *
 *  放置：D:\TrinityCore\src\server\scripts\Commands\cs_spellclean.cpp
 *  需要：注册到 cs_script_loader.cpp + RBAC.h 加权限 71006
 *  新增源文件必须重跑 CMake
 * ============================================================================
 */

#include "ScriptMgr.h"
#include "CustomSpellTweak.h"
#include "Chat.h"
#include "Player.h"
#include "RBAC.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "DBCStores.h"
#include "WorldSession.h"
#include <vector>
#include <string>
#include <algorithm>

namespace SpellClean
{
    struct CleanTarget
    {
        uint32      spellId;
        uint32      keepId;      // 保留的那个（最高 Rank）
        uint8       rank;
        uint8       keepRank;
        std::string name;
    };

    // ------------------------------------------------------------------
    //  取技能名（按客户端语言）
    // ------------------------------------------------------------------
    inline std::string GetSpellName(uint32 spellId, uint32 locale)
    {
        SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
        if (!info)
            return "";
        char const* n = info->SpellName[locale];
        return n ? std::string(n) : "";
    }

    // ------------------------------------------------------------------
    //  判断一个技能能不能删
    // ------------------------------------------------------------------
    inline bool IsSafeToRemove(Player* player, uint32 spellId, uint32& outKeepId,
                               uint8& outRank, uint8& outKeepRank)
    {
        SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
        if (!info)
            return false;

        // 被动技能不动 —— 很多是种族天赋、职业精通，删了会掉属性
        if (info->IsPassive())
            return false;

        // 天赋不动
        if (GetTalentSpellCost(spellId) > 0)
            return false;

        // 没有技能链的（唯一等级）不动
        uint32 first = sSpellMgr->GetFirstSpellInChain(spellId);
        if (!first)
            return false;

        uint8 myRank = sSpellMgr->GetSpellRank(spellId);
        if (myRank == 0)
            return false;   // 不在等级链里

        // 沿着链往上找，看玩家有没有学更高等级的
        uint32 higher = 0;
        uint8  higherRank = myRank;
        uint32 cur = spellId;
        while (uint32 next = sSpellMgr->GetNextSpellInChain(cur))
        {
            if (player->HasSpell(next))
            {
                higher = next;
                higherRank = sSpellMgr->GetSpellRank(next);
            }
            cur = next;
        }

        if (!higher)
            return false;   // 这已经是玩家会的最高等级，保留

        outKeepId   = higher;
        outRank     = myRank;
        outKeepRank = higherRank;
        return true;
    }

    // ------------------------------------------------------------------
    //  扫描出所有可清理的技能
    // ------------------------------------------------------------------
    inline std::vector<CleanTarget> Scan(Player* player, uint32 locale)
    {
        std::vector<CleanTarget> out;

        // 先复制一份 ID 列表 —— 不能边遍历边删
        std::vector<uint32> ids;
        ids.reserve(player->GetSpellMap().size());
        for (auto const& pair : player->GetSpellMap())
        {
            if (pair.second.state == PLAYERSPELL_REMOVED)
                continue;
            if (!pair.second.active || pair.second.disabled)
                continue;
            ids.push_back(pair.first);
        }

        for (uint32 id : ids)
        {
            uint32 keepId = 0;
            uint8  rank = 0, keepRank = 0;
            if (!IsSafeToRemove(player, id, keepId, rank, keepRank))
                continue;

            CleanTarget t;
            t.spellId  = id;
            t.keepId   = keepId;
            t.rank     = rank;
            t.keepRank = keepRank;
            t.name     = GetSpellName(id, locale);
            out.push_back(std::move(t));
        }

        // 按名称排序，方便看
        std::sort(out.begin(), out.end(),
                  [](CleanTarget const& a, CleanTarget const& b)
                  {
                      if (a.name != b.name)
                          return a.name < b.name;
                      return a.rank < b.rank;
                  });

        return out;
    }
}

class spellclean_commandscript : public CommandScript
{
public:
    spellclean_commandscript() : CommandScript("spellclean_commandscript") { }

    // 本仓库 cs_modify.cpp 用旧版框架，这里保持一致
    std::vector<ChatCommand> GetCommands() const override
    {
        static std::vector<ChatCommand> commandTable =
        {
            { "spell", rbac::RBAC_PERM_COMMAND_SPELLCLEAN, false, &HandleSpellCommand, "" },
        };
        return commandTable;
    }

    static bool HandleSpellCommand(ChatHandler* handler, char const* args)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        std::string a = args ? args : "";
        // 拆词
        std::vector<std::string> tok;
        {
            size_t pos = 0;
            while (pos < a.size())
            {
                size_t sp = a.find(' ', pos);
                if (sp == std::string::npos) sp = a.size();
                if (sp > pos) tok.push_back(a.substr(pos, sp - pos));
                pos = sp + 1;
            }
        }

        if (tok.empty())
        {
            ShowHelp(handler);
            return true;
        }

        std::string const& sub = tok[0];

        if (sub == "clean")
            return HandleClean(handler, player, tok.size() > 1 && tok[1] == "confirm");
        if (sub == "count")
            return HandleCount(handler, player);
        if (sub == "find")
        {
            if (tok.size() < 2)
            {
                handler->PSendSysMessage("用法：|cffffff00.spell find <关键词>|r");
                return true;
            }
            return HandleFind(handler, player, tok[1]);
        }
        if (sub == "tweak")
            return HandleTweak(handler, tok);
        if (sub == "dump")
        {
            if (tok.size() < 2)
            {
                handler->PSendSysMessage("用法：|cffffff00.spell dump <法术ID>|r");
                handler->PSendSysMessage("打印法术的真实结构，用于诊断为什么强化没生效。");
                return true;
            }
            return HandleDump(handler, uint32(atoi(tok[1].c_str())));
        }

        ShowHelp(handler);
        return true;
    }

    // ==================================================================
    //  打印法术真实结构（诊断用）
    //  .spell dump <法术ID>
    // ==================================================================
    static bool HandleDump(ChatHandler* handler, uint32 spellId)
    {
        SpellInfo const* si = sSpellMgr->GetSpellInfo(spellId);
        if (!si)
        {
            handler->PSendSysMessage("|cffff0000法术 %u 不存在。|r", spellId);
            return true;
        }

        LocaleConstant loc = handler->GetSessionDbcLocale();
        handler->PSendSysMessage("|cff00ff00===== 法术 %u =====|r", spellId);
        handler->PSendSysMessage("名称：%s", si->SpellName[loc] ? si->SpellName[loc] : "(无)");
        handler->PSendSysMessage("冷却：RecoveryTime=|cffffff00%u|r  CategoryRecovery=|cffffff00%u|r",
            si->RecoveryTime, si->CategoryRecoveryTime);
        handler->PSendSysMessage("Category=%u  StartRecovery=%u  StackAmount=%u  MaxTargets=%u",
            si->CategoryEntry ? si->CategoryEntry->ID : 0,
            si->StartRecoveryTime, si->StackAmount, si->MaxAffectedTargets);

        handler->PSendSysMessage("|cff00ff00--- 效果 ---|r");
        uint32 idx = 0;
        for (SpellEffectInfo const& eff : si->GetEffects())
        {
            if (!eff.Effect)
            {
                ++idx;
                continue;
            }
            handler->PSendSysMessage("|cff00ccff[效果%u]|r Effect=|cffffff00%u|r  Aura=%u",
                idx, uint32(eff.Effect), uint32(eff.ApplyAuraName));
            handler->PSendSysMessage("   BasePoints=|cffffff00%d|r  DieSides=%d  RealPPL=%.1f",
                eff.BasePoints, eff.DieSides, double(eff.RealPointsPerLevel));
            handler->PSendSysMessage("   ChainTargets=|cffffff00%u|r  MiscValue=%d  MiscValueB=%d",
                eff.ChainTargets, eff.MiscValue, eff.MiscValueB);
            if (eff.TriggerSpell)
                handler->PSendSysMessage("   |cffff8000TriggerSpell=%u|r  <- 实际效果在这个法术里",
                    eff.TriggerSpell);
            ++idx;
        }

        handler->PSendSysMessage(" ");
        handler->PSendSysMessage("|cff888888Effect 常见值：2=直伤 6=光环 10=治疗 28=召唤 64/151=触发其他法术|r");
        return true;
    }

    // ==================================================================
    //  查看老职业技能强化的应用情况
    //  .spell tweak [职业名|failed]
    // ==================================================================
    static bool HandleTweak(ChatHandler* handler, std::vector<std::string> const& tok)
    {
        auto const& log = sCustomSpellTweak->GetLog();

        if (log.empty())
        {
            handler->PSendSysMessage("|cffff8000没有任何强化记录。|r");
            handler->PSendSysMessage("可能是 |cffffff00CustomSpell.Enable = 0|r，或模块没装。");
            return true;
        }

        // 过滤条件
        std::string filter;
        bool onlyFailed = false;
        if (tok.size() > 1)
        {
            if (tok[1] == "failed" || tok[1] == "失败" || tok[1] == "跳过")
                onlyFailed = true;
            else
                filter = tok[1];
        }

        uint32 shown = 0, okCount = 0, failCount = 0;

        for (auto const& e : log)
        {
            if (e.applied) ++okCount; else ++failCount;
        }

        handler->PSendSysMessage("|cff00ff00===== 老职业技能强化 =====|r");
        handler->PSendSysMessage("成功 |cff00ff00%u|r 条，跳过 |cffff8000%u|r 条",
            okCount, failCount);
        handler->PSendSysMessage(" ");

        for (auto const& e : log)
        {
            if (onlyFailed && e.applied)
                continue;
            if (!filter.empty() && e.cls.find(filter) == std::string::npos)
                continue;

            if (e.applied)
            {
                handler->PSendSysMessage("|cff00ff00[OK]|r |cff00ccff%s|r %s |cff888888(%u %s)|r",
                    e.cls.c_str(), e.what.c_str(), e.spellId, e.spellName.c_str());
            }
            else
            {
                handler->PSendSysMessage("|cffff0000[跳过]|r |cff00ccff%s|r %s |cff888888(%u)|r",
                    e.cls.c_str(), e.what.c_str(), e.spellId);
                handler->PSendSysMessage("        |cffff8000原因：%s|r", e.reason.c_str());
            }
            ++shown;

            if (shown >= 40)
            {
                handler->PSendSysMessage("|cff888888... 还有更多，用参数过滤：|r");
                handler->PSendSysMessage("|cffffff00.spell tweak 法师|r  |cffffff00.spell tweak failed|r");
                break;
            }
        }

        if (!shown)
            handler->PSendSysMessage("|cffff8000没有匹配的记录。|r");

        handler->PSendSysMessage(" ");
        handler->PSendSysMessage("|cff888888用法：.spell tweak [职业名]  只看某职业|r");
        handler->PSendSysMessage("|cff888888      .spell tweak failed    只看跳过的|r");
        return true;
    }

    // ==================================================================
    //  清理
    // ==================================================================
    static bool HandleClean(ChatHandler* handler, Player* player, bool confirm)
    {
        uint32 locale = uint32(handler->GetSessionDbcLocale());
        std::vector<SpellClean::CleanTarget> targets = SpellClean::Scan(player, locale);

        if (targets.empty())
        {
            handler->PSendSysMessage("|cff00ff00技能书很干净|r，没有需要清理的低阶技能");
            return true;
        }

        uint32 total = uint32(player->GetSpellMap().size());

        handler->PSendSysMessage("|cff00ccff========== 技能书清理 ==========|r");

        if (!confirm)
        {
            // 预览模式 —— 最多列 30 条，避免刷屏
            uint32 shown = 0;
            for (SpellClean::CleanTarget const& t : targets)
            {
                if (shown++ >= 30)
                    break;
                handler->PSendSysMessage("  |cff888888%s (等级%u)|r  ->  保留等级%u",
                                         t.name.empty() ? "?" : t.name.c_str(),
                                         t.rank, t.keepRank);
            }
            if (targets.size() > 30)
                handler->PSendSysMessage("  |cff888888... 还有 %u 条|r",
                                         uint32(targets.size()) - 30);

            handler->PSendSysMessage("|cff00ccff================================|r");
            handler->PSendSysMessage("当前技能总数：|cffffff00%u|r", total);
            handler->PSendSysMessage("可清理：|cffff8800%u|r 条  "
                                     "清理后剩余：|cff00ff00%u|r 条",
                                     uint32(targets.size()), total - uint32(targets.size()));
            handler->PSendSysMessage("");
            handler->PSendSysMessage("这只是|cffffff00预览|r，没有实际删除。");
            handler->PSendSysMessage("确认清理请输入：|cffffff00.spell clean confirm|r");
            return true;
        }

        // 真删
        uint32 removed = 0;
        for (SpellClean::CleanTarget const& t : targets)
        {
            // learn_low_rank = false，防止删了又自动补回低阶
            player->RemoveSpell(t.spellId, false, false);
            ++removed;
        }

        handler->PSendSysMessage("|cff00ff00清理完成|r");
        handler->PSendSysMessage("  已移除：|cffff8800%u|r 条低阶技能", removed);
        handler->PSendSysMessage("  技能总数：%u -> |cff00ff00%u|r", total, total - removed);
        handler->PSendSysMessage("|cff888888提示：被动技能、天赋、专业配方均未改动|r");
        return true;
    }

    // ==================================================================
    //  统计
    // ==================================================================
    static bool HandleCount(ChatHandler* handler, Player* player)
    {
        uint32 total = 0, passive = 0, talent = 0, active = 0, ranked = 0;

        for (auto const& pair : player->GetSpellMap())
        {
            if (pair.second.state == PLAYERSPELL_REMOVED)
                continue;
            ++total;

            SpellInfo const* info = sSpellMgr->GetSpellInfo(pair.first);
            if (!info)
                continue;

            if (info->IsPassive())
                ++passive;
            else
                ++active;

            if (GetTalentSpellCost(pair.first) > 0)
                ++talent;
            if (sSpellMgr->GetSpellRank(pair.first) > 0)
                ++ranked;
        }

        uint32 locale = uint32(handler->GetSessionDbcLocale());
        uint32 cleanable = uint32(SpellClean::Scan(player, locale).size());

        handler->PSendSysMessage("|cff00ccff========== 技能统计 ==========|r");
        handler->PSendSysMessage("  技能总数    ：|cffffff00%u|r", total);
        handler->PSendSysMessage("  主动技能    ：%u", active);
        handler->PSendSysMessage("  被动技能    ：%u", passive);
        handler->PSendSysMessage("  天赋技能    ：%u", talent);
        handler->PSendSysMessage("  带等级的技能：%u", ranked);
        handler->PSendSysMessage("|cff00ccff------------------------------|r");
        if (cleanable)
            handler->PSendSysMessage("  |cffff8800可清理低阶：%u 条|r  "
                                     "（用 .spell clean 查看）", cleanable);
        else
            handler->PSendSysMessage("  |cff00ff00无可清理项，技能书很干净|r");
        return true;
    }

    // ==================================================================
    //  搜索
    // ==================================================================
    static bool HandleFind(ChatHandler* handler, Player* player, std::string const& kw)
    {
        uint32 locale = uint32(handler->GetSessionDbcLocale());
        uint32 found = 0;

        handler->PSendSysMessage("|cff00ccff搜索：%s|r", kw.c_str());

        for (auto const& pair : player->GetSpellMap())
        {
            if (pair.second.state == PLAYERSPELL_REMOVED)
                continue;
            if (!pair.second.active)
                continue;

            std::string name = SpellClean::GetSpellName(pair.first, locale);
            if (name.empty() || name.find(kw) == std::string::npos)
                continue;

            if (found++ >= 25)
            {
                handler->PSendSysMessage("  |cff888888... 结果过多，请用更精确的关键词|r");
                break;
            }

            uint8 rank = sSpellMgr->GetSpellRank(pair.first);
            if (rank)
                handler->PSendSysMessage("  |cffffff00%u|r  %s |cff888888(等级%u)|r",
                                         pair.first, name.c_str(), rank);
            else
                handler->PSendSysMessage("  |cffffff00%u|r  %s",
                                         pair.first, name.c_str());
        }

        if (!found)
            handler->PSendSysMessage("  |cff888888没有找到|r");
        return true;
    }

    // ==================================================================
    static void ShowHelp(ChatHandler* handler)
    {
        handler->PSendSysMessage("|cff00ccff========== 技能书管理 ==========|r");
        handler->PSendSysMessage("  |cffffff00.spell clean|r          预览可清理的低阶技能");
        handler->PSendSysMessage("  |cffffff00.spell clean confirm|r  执行清理");
        handler->PSendSysMessage("  |cffffff00.spell count|r          技能数量统计");
        handler->PSendSysMessage("  |cffffff00.spell find <词>|r      搜索自己会的技能");
        handler->PSendSysMessage("|cff888888被动技能、天赋、专业配方不会被清理|r");
    }
};

void AddSC_spellclean_commandscript()
{
    new spellclean_commandscript();
}
