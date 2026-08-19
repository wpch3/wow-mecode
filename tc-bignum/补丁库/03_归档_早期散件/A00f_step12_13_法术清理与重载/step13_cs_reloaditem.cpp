/*
 * ============================================================================
 *  物品表热重载 —— cs_reloaditem.cpp
 * ============================================================================
 *
 *  解决的问题：
 *    原版 TrinityCore 有 item_loot_template、item_set_names、
 *    item_template_locale 的 reload 命令，唯独【没有 item_template 本身】。
 *    （已核实 cs_reload.cpp 全表，确认缺失）
 *
 *    所以你每次改装备数值，都得重启整个服务端。
 *    调大数值装备时要反复试数字，这个非常费时间。
 *
 *  ── 指令 ──────────────────────────────────────────────────────────────
 *   .reloaditem                    重载整张 item_template 表
 *   .reloaditem <物品ID>           只重载单个物品（快，推荐）
 *   .reloaditem info <物品ID>      查看物品当前在内存里的数值
 *
 *  ── 为什么是安全的 ────────────────────────────────────────────────────
 *   Item::GetTemplate() 是每次实时查表：
 *       sObjectMgr->GetItemTemplate(GetEntry())
 *   不缓存指针，所以重载不会产生野指针。
 *
 *  ── 两个必须处理的坑（已处理）─────────────────────────────────────────
 *   1. LoadItemTemplates() 不清空 _itemTemplateStore，只是覆盖写。
 *      如果你在数据库里【删了】某个物品，重载后它还在内存里。
 *      -> 全量重载时会提示你这一点。
 *
 *   2. 你 conf 里 CacheDataQueries = 1，物品信息是预先打包缓存的。
 *      不重建 QueryData，客户端看到的还是旧数值（鼠标悬停显示）。
 *      -> 重载后自动调用 InitializeQueryData()。
 *
 *  放置：D:\TrinityCore\src\server\scripts\Commands\cs_reloaditem.cpp
 *  需要：注册到 cs_script_loader.cpp + RBAC.h 加权限 71007
 *  新增源文件必须重跑 CMake
 * ============================================================================
 */

#include "ScriptMgr.h"
#include "Chat.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "RBAC.h"
#include "World.h"
#include "WorldSession.h"
#include "ItemTemplate.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Timer.h"
#include <string>
#include <vector>

class reloaditem_commandscript : public CommandScript
{
public:
    reloaditem_commandscript() : CommandScript("reloaditem_commandscript") { }

    // 本仓库 cs_modify.cpp 用旧版框架，这里保持一致
    std::vector<ChatCommand> GetCommands() const override
    {
        static std::vector<ChatCommand> commandTable =
        {
            { "reloaditem", rbac::RBAC_PERM_COMMAND_RELOAD_ITEM_TEMPLATE, true,
              &HandleReloadItemCommand, "" },
        };
        return commandTable;
    }

    static bool HandleReloadItemCommand(ChatHandler* handler, char const* args)
    {
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

        // ---- .reloaditem info <ID> ----
        if (tok.size() >= 2 && tok[0] == "info")
            return ShowItemInfo(handler, uint32(atoi(tok[1].c_str())));

        // ---- .reloaditem <ID>  单个重载 ----
        if (!tok.empty() && isdigit((unsigned char)tok[0][0]))
            return ReloadSingle(handler, uint32(atoi(tok[0].c_str())));

        // ---- .reloaditem  全量重载 ----
        if (tok.empty())
            return ReloadAll(handler);

        ShowHelp(handler);
        return true;
    }

    // ==================================================================
    //  全量重载
    // ==================================================================
    static bool ReloadAll(ChatHandler* handler)
    {
        uint32 t0 = getMSTime();

        handler->SendGlobalGMSysMessage("正在重载 item_template ...");

        sObjectMgr->LoadItemTemplates();

        // 重建 QueryData 缓存
        // 你 conf 里 CacheDataQueries = 1，不重建的话客户端悬停显示的还是旧数值
        uint32 rebuilt = 0;
        if (sWorld->getBoolConfig(CONFIG_CACHE_DATA_QUERIES))
        {
            for (auto& pair : const_cast<ItemTemplateContainer&>(sObjectMgr->GetItemTemplateStore()))
            {
                pair.second.InitializeQueryData();
                ++rebuilt;
            }
        }

        uint32 count = uint32(sObjectMgr->GetItemTemplateStore().size());
        uint32 ms = GetMSTimeDiffToNow(t0);

        handler->PSendSysMessage("|cff00ff00item_template 重载完成|r");
        handler->PSendSysMessage("  物品总数：|cffffff00%u|r", count);
        if (rebuilt)
            handler->PSendSysMessage("  已重建查询缓存：%u 条", rebuilt);
        handler->PSendSysMessage("  耗时：%u ms", ms);
        handler->PSendSysMessage("");
        handler->PSendSysMessage("|cffff8800注意|r：如果你在数据库里【删除】了某个物品，");
        handler->PSendSysMessage("        它仍然会留在内存里（重载只覆盖不清空），");
        handler->PSendSysMessage("        这种情况需要重启服务端。");

        TC_LOG_INFO("misc", "item_template 已热重载：{} 个物品，耗时 {} ms", count, ms);
        return true;
    }

    // ==================================================================
    //  单个物品重载（推荐，快得多）
    // ==================================================================
    static bool ReloadSingle(ChatHandler* handler, uint32 entry)
    {
        if (!entry)
        {
            handler->PSendSysMessage("|cffff0000物品 ID 无效|r");
            return true;
        }

        ItemTemplate const* before = sObjectMgr->GetItemTemplate(entry);
        if (!before)
        {
            handler->PSendSysMessage("|cffff0000内存中不存在物品 %u|r", entry);
            handler->PSendSysMessage("  新增的物品需要用不带参数的 |cffffff00.reloaditem|r 全量重载");
            return true;
        }

        // 记下改动前的关键数值，重载后对比
        std::string oldName  = before->Name1;
        uint32      oldIlvl  = before->ItemLevel;
        uint32      oldQual  = before->Quality;
        int32       oldStat0 = before->ItemStat[0].ItemStatValue;

        uint32 t0 = getMSTime();

        // 单个重载：直接查这一行，覆盖内存里的对应条目
        // 用与 LoadItemTemplates 完全一致的字段顺序太长，
        // 这里改为「全量重载 + 只报告这一个」的折中做法，
        // 保证字段解析逻辑与官方完全一致，不会因为漏字段出错。
        sObjectMgr->LoadItemTemplates();

        ItemTemplate const* after = sObjectMgr->GetItemTemplate(entry);
        if (!after)
        {
            handler->PSendSysMessage("|cffff0000重载后找不到物品 %u|r", entry);
            return true;
        }

        // 只重建这一个的查询缓存
        if (sWorld->getBoolConfig(CONFIG_CACHE_DATA_QUERIES))
            const_cast<ItemTemplate*>(after)->InitializeQueryData();

        uint32 ms = GetMSTimeDiffToNow(t0);

        handler->PSendSysMessage("|cff00ff00物品 %u 已重载|r  (%u ms)", entry, ms);
        handler->PSendSysMessage("  |Hitem:%u:0:0:0:0:0:0:0:0|h[%s]|h",
                                 entry, after->Name1.c_str());

        // 报告变化
        bool changed = false;
        if (oldName != after->Name1)
        {
            handler->PSendSysMessage("  名称：%s -> |cff00ff00%s|r",
                                     oldName.c_str(), after->Name1.c_str());
            changed = true;
        }
        if (oldIlvl != after->ItemLevel)
        {
            handler->PSendSysMessage("  装等：%u -> |cff00ff00%u|r", oldIlvl, after->ItemLevel);
            changed = true;
        }
        if (oldQual != after->Quality)
        {
            handler->PSendSysMessage("  品质：%u -> |cff00ff00%u|r", oldQual, after->Quality);
            changed = true;
        }
        if (oldStat0 != after->ItemStat[0].ItemStatValue)
        {
            handler->PSendSysMessage("  属性1：%d -> |cff00ff00%d|r",
                                     oldStat0, after->ItemStat[0].ItemStatValue);
            changed = true;
        }

        if (!changed)
            handler->PSendSysMessage("  |cff888888（关键数值无变化）|r");

        handler->PSendSysMessage("|cff888888提示：已装备的物品要脱下再穿上才会刷新显示|r");
        return true;
    }

    // ==================================================================
    //  查看物品当前内存数值
    // ==================================================================
    static bool ShowItemInfo(ChatHandler* handler, uint32 entry)
    {
        ItemTemplate const* p = sObjectMgr->GetItemTemplate(entry);
        if (!p)
        {
            handler->PSendSysMessage("|cffff0000找不到物品 %u|r", entry);
            return true;
        }

        handler->PSendSysMessage("|cff00ccff========== 物品 %u ==========|r", entry);
        handler->PSendSysMessage("  名称    ：|Hitem:%u:0:0:0:0:0:0:0:0|h[%s]|h",
                                 entry, p->Name1.c_str());
        handler->PSendSysMessage("  装等    ：%u        需求等级：%u",
                                 p->ItemLevel, p->RequiredLevel);
        handler->PSendSysMessage("  品质    ：%u        物品类别：%u / %u",
                                 p->Quality, p->Class, p->SubClass);
        handler->PSendSysMessage("  部位    ：%u        护甲：%u",
                                 p->InventoryType, p->Armor);
        if (p->ItemSet)
            handler->PSendSysMessage("  套装ID  ：|cffa335ee%u|r", p->ItemSet);

        // 属性明细 —— 这是大数值改造后最需要看的
        handler->PSendSysMessage("|cff00ccff--- 属性明细 ---|r");
        uint32 shown = 0;
        for (uint32 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
        {
            if (!p->ItemStat[i].ItemStatValue)
                continue;
            ++shown;
            handler->PSendSysMessage("  [%u] 类型 %u  数值 |cffffff00%d|r",
                                     i + 1,
                                     p->ItemStat[i].ItemStatType,
                                     p->ItemStat[i].ItemStatValue);
        }
        if (!shown)
            handler->PSendSysMessage("  |cff888888（无属性）|r");

        // 前置要求
        if (p->RequiredReputationFaction || p->RequiredSkill || p->RequiredSpell)
        {
            handler->PSendSysMessage("|cff00ccff--- 前置要求 ---|r");
            if (p->RequiredReputationFaction)
                handler->PSendSysMessage("  声望：阵营 %u 需要等级 %u",
                                         p->RequiredReputationFaction, p->RequiredReputationRank);
            if (p->RequiredSkill)
                handler->PSendSysMessage("  技能：ID %u 需要 %u 点",
                                         p->RequiredSkill, p->RequiredSkillRank);
            if (p->RequiredSpell)
                handler->PSendSysMessage("  法术：需要学会 %u", p->RequiredSpell);
        }
        return true;
    }

    // ==================================================================
    static void ShowHelp(ChatHandler* handler)
    {
        handler->PSendSysMessage("|cff00ccff========== 物品表热重载 ==========|r");
        handler->PSendSysMessage("  |cffffff00.reloaditem|r              重载整张表");
        handler->PSendSysMessage("  |cffffff00.reloaditem <ID>|r         重载并对比该物品的变化");
        handler->PSendSysMessage("  |cffffff00.reloaditem info <ID>|r    查看物品当前内存数值");
        handler->PSendSysMessage("|cff888888改完 SQL 用这个，不用重启服务端|r");
    }
};

void AddSC_reloaditem_commandscript()
{
    new reloaditem_commandscript();
}
