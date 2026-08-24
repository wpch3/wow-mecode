/*
 * 智能添加指令 —— cs_smartadd.cpp
 *
 * 新增指令：
 *   .add <中文名>                 模糊搜索物品，唯一匹配直接给，多个列候选
 *   .add <中文名> <数量>          带数量
 *   .add A, B, C                  逗号分隔，一次给多件
 *   .add! <中文名>                强制模式：把所有匹配项全给
 *   .add last                     重复上次添加
 *
 *   .spawn <中文名>               模糊搜索生物并召唤
 *   .spawn <中文名> x5            一次刷5只
 *   .spawn A, B, C                一次刷多种
 *   .spawn! <中文名>              强制模式：所有匹配项各刷一只
 *   .spawn last                   重复上次召唤
 *   .clean [半径]                 清理自己刷的生物（默认30码）
 *
 * 放置位置：D:\TrinityCore\src\server\scripts\Commands\cs_smartadd.cpp
 * 需要在 cs_script_loader.cpp 注册（见配套文档）
 */

#include "ScriptMgr.h"
#include "Chat.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "RBAC.h"
#include "World.h"
#include "WorldSession.h"
#include "Language.h"
#include "Item.h"
#include "Creature.h"
#include "TemporarySummon.h"
#include "Util.h"
#include <vector>
#include <string>
#include <algorithm>

// 每个玩家记住上次操作的目标，供 .add last / .spawn last 使用
static std::unordered_map<uint32 /*accountId*/, uint32 /*itemId*/>     s_lastItem;
static std::unordered_map<uint32 /*accountId*/, uint32 /*creatureId*/> s_lastCreature;
// 记录本次会话中自己召唤的生物，供 .clean 使用
static std::unordered_map<uint32 /*accountId*/, std::vector<ObjectGuid>> s_mySummons;

class smartadd_commandscript : public CommandScript
{
public:
    smartadd_commandscript() : CommandScript("smartadd_commandscript") { }

    std::vector<ChatCommand> GetCommands() const override
    {
        static std::vector<ChatCommand> commandTable =
        {
            { "add",    rbac::RBAC_PERM_COMMAND_SMART_ADD,   false, &HandleSmartAddCommand,      "" },
            { "add!",   rbac::RBAC_PERM_COMMAND_SMART_ADD,   false, &HandleSmartAddForceCommand, "" },
            { "spawn",  rbac::RBAC_PERM_COMMAND_SMART_SPAWN, false, &HandleSmartSpawnCommand,      "" },
            { "spawn!", rbac::RBAC_PERM_COMMAND_SMART_SPAWN, false, &HandleSmartSpawnForceCommand, "" },
            { "clean",  rbac::RBAC_PERM_COMMAND_SMART_SPAWN, false, &HandleSmartCleanCommand,      "" },
        };
        return commandTable;
    }

    // ==============================================================
    // 工具：按名字模糊搜索物品，返回所有匹配的 entry
    // ==============================================================
    static void SearchItems(ChatHandler* handler, std::string const& namePart,
                            std::vector<uint32>& results, uint32 maxResults = 30)
    {
        std::wstring wNamePart;
        if (!Utf8toWStr(namePart, wNamePart))
            return;
        wstrToLower(wNamePart);

        LocaleConstant loc = handler->GetSessionDbLocaleIndex();
        ItemTemplateContainer const& its = sObjectMgr->GetItemTemplateStore();

        for (auto const& pair : its)
        {
            std::string name = pair.second.GetName(loc);
            if (name.empty())
                continue;

            if (Utf8FitTo(name, wNamePart))
            {
                results.push_back(pair.first);
                if (results.size() >= maxResults)
                    return;
            }
        }
    }

    // ==============================================================
    // 工具：按名字模糊搜索生物
    // ==============================================================
    static void SearchCreatures(ChatHandler* handler, std::string const& namePart,
                                std::vector<uint32>& results, uint32 maxResults = 30)
    {
        std::wstring wNamePart;
        if (!Utf8toWStr(namePart, wNamePart))
            return;
        wstrToLower(wNamePart);

        uint8 localeIndex = handler->GetSessionDbLocaleIndex();
        CreatureTemplateContainer const& ctc = sObjectMgr->GetCreatureTemplates();

        for (auto const& pair : ctc)
        {
            uint32 id = pair.first;
            bool matched = false;

            // 先查本地化名（中文）
            if (CreatureLocale const* cl = sObjectMgr->GetCreatureLocale(id))
            {
                if (cl->Name.size() > localeIndex && !cl->Name[localeIndex].empty())
                {
                    if (Utf8FitTo(cl->Name[localeIndex], wNamePart))
                        matched = true;
                }
            }

            // 再查英文原名
            if (!matched && !pair.second.Name.empty())
            {
                if (Utf8FitTo(pair.second.Name, wNamePart))
                    matched = true;
            }

            if (matched)
            {
                results.push_back(id);
                if (results.size() >= maxResults)
                    return;
            }
        }
    }

    // ==============================================================
    // 工具：把逗号分隔的字符串拆成多段，并去掉首尾空格
    // ==============================================================
    static std::vector<std::string> SplitByComma(std::string const& input)
    {
        std::vector<std::string> parts;
        std::string cur;

        for (char c : input)
        {
            // 支持中英文逗号
            if (c == ',')
            {
                if (!cur.empty())
                    parts.push_back(cur);
                cur.clear();
            }
            else
                cur += c;
        }
        if (!cur.empty())
            parts.push_back(cur);

        // 去首尾空格
        for (auto& s : parts)
        {
            size_t b = s.find_first_not_of(" \t");
            size_t e = s.find_last_not_of(" \t");
            s = (b == std::string::npos) ? "" : s.substr(b, e - b + 1);
        }

        // 移除空串
        parts.erase(std::remove_if(parts.begin(), parts.end(),
            [](std::string const& s) { return s.empty(); }), parts.end());

        return parts;
    }

    // ==============================================================
    // 工具：实际发放物品
    // ==============================================================
    static bool GiveItem(ChatHandler* handler, Player* target, uint32 itemId, uint32 count)
    {
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
        if (!proto)
            return false;

        // 限制单次数量，避免误操作
        if (count > 1000)
            count = 1000;

        uint32 noSpaceForCount = 0;
        ItemPosCountVec dest;
        InventoryResult msg = target->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemId, count, &noSpaceForCount);

        if (msg != EQUIP_ERR_OK)
            count -= noSpaceForCount;

        if (count == 0 || dest.empty())
        {
            handler->PSendSysMessage("|cffff0000背包空间不足|r，无法放入 %s", proto->Name1.c_str());
            return false;
        }

        Item* item = target->StoreNewItem(dest, itemId, true,
                                          GenerateItemRandomPropertyId(itemId));
        if (item)
            target->SendNewItem(item, count, false, true);

        return item != nullptr;
    }

    // ==============================================================
    // 工具：列出候选项（带可点击链接）
    // ==============================================================
    static void ListItemCandidates(ChatHandler* handler, std::vector<uint32> const& ids,
                                   std::string const& keyword)
    {
        LocaleConstant loc = handler->GetSessionDbLocaleIndex();
        handler->PSendSysMessage("|cffffcc00「%s」匹配到 %u 件物品：|r", keyword.c_str(), uint32(ids.size()));

        for (uint32 id : ids)
        {
            ItemTemplate const* proto = sObjectMgr->GetItemTemplate(id);
            if (!proto)
                continue;
            std::string name = proto->GetName(loc);
            handler->PSendSysMessage(LANG_ITEM_LIST_CHAT, id, id, name.c_str());
        }

        handler->SendSysMessage("|cff00ccff点击链接或用 .additem <ID> 获取；|r");
        handler->PSendSysMessage("|cff00ccff用 .add! %s 可一次性全部获取。|r", keyword.c_str());
    }

    static void ListCreatureCandidates(ChatHandler* handler, std::vector<uint32> const& ids,
                                       std::string const& keyword)
    {
        uint8 localeIndex = handler->GetSessionDbLocaleIndex();
        handler->PSendSysMessage("|cffffcc00「%s」匹配到 %u 种生物：|r", keyword.c_str(), uint32(ids.size()));

        for (uint32 id : ids)
        {
            CreatureTemplate const* ct = sObjectMgr->GetCreatureTemplate(id);
            if (!ct)
                continue;

            std::string name = ct->Name;
            if (CreatureLocale const* cl = sObjectMgr->GetCreatureLocale(id))
                if (cl->Name.size() > localeIndex && !cl->Name[localeIndex].empty())
                    name = cl->Name[localeIndex];

            handler->PSendSysMessage(LANG_CREATURE_ENTRY_LIST_CHAT, id, id, name.c_str());
        }

        handler->SendSysMessage("|cff00ccff用 .spawn <ID> 召唤指定的；|r");
        handler->PSendSysMessage("|cff00ccff用 .spawn! %s 可全部各刷一只。|r", keyword.c_str());
    }

    // ==============================================================
    // .add —— 主逻辑
    // ==============================================================
    static bool DoSmartAdd(ChatHandler* handler, char const* args, bool forceAll)
    {
        if (!*args)
        {
            handler->SendSysMessage("用法: .add <物品名> [数量]");
            handler->SendSysMessage("      .add 火焰之击, 霜之哀伤     (逗号分隔批量)");
            handler->SendSysMessage("      .add! <物品名>              (全部匹配项都给)");
            handler->SendSysMessage("      .add last                   (重复上次)");
            handler->SetSentErrorMessage(true);
            return false;
        }

        Player* target = handler->getSelectedPlayerOrSelf();
        if (!target)
        {
            handler->SendSysMessage(LANG_NO_CHAR_SELECTED);
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint32 accId = handler->GetSession() ? handler->GetSession()->GetAccountId() : 0;
        std::string input = args;

        // ---- .add last ----
        if (input == "last")
        {
            auto it = s_lastItem.find(accId);
            if (it == s_lastItem.end())
            {
                handler->SendSysMessage("没有上次记录。");
                handler->SetSentErrorMessage(true);
                return false;
            }
            if (GiveItem(handler, target, it->second, 1))
            {
                ItemTemplate const* p = sObjectMgr->GetItemTemplate(it->second);
                handler->PSendSysMessage("|cff00ff00已重复添加|r %s",
                    p ? p->GetName(handler->GetSessionDbLocaleIndex()).c_str() : "");
            }
            return true;
        }

        // ---- 批量模式：含逗号 ----
        if (input.find(',') != std::string::npos)
        {
            std::vector<std::string> names = SplitByComma(input);
            uint32 okCount = 0, failCount = 0;

            handler->PSendSysMessage("|cffffcc00批量添加 %u 项...|r", uint32(names.size()));

            for (std::string const& nm : names)
            {
                std::vector<uint32> found;
                SearchItems(handler, nm, found, 5);

                if (found.empty())
                {
                    handler->PSendSysMessage("  |cffff0000[未找到]|r %s", nm.c_str());
                    ++failCount;
                }
                else if (found.size() == 1 || forceAll)
                {
                    for (uint32 id : found)
                    {
                        if (GiveItem(handler, target, id, 1))
                        {
                            ItemTemplate const* p = sObjectMgr->GetItemTemplate(id);
                            handler->PSendSysMessage("  |cff00ff00[OK]|r %s",
                                p ? p->GetName(handler->GetSessionDbLocaleIndex()).c_str() : "");
                            ++okCount;
                            s_lastItem[accId] = id;
                        }
                        if (!forceAll)
                            break;
                    }
                }
                else
                {
                    handler->PSendSysMessage("  |cffffff00[多个匹配]|r %s (%u个)，已跳过",
                        nm.c_str(), uint32(found.size()));
                    ++failCount;
                }
            }

            handler->PSendSysMessage("|cffffcc00完成：成功 %u，跳过 %u|r", okCount, failCount);
            return true;
        }

        // ---- 单项模式：解析可选数量 ----
        std::string namePart = input;
        uint32 count = 1;

        size_t lastSpace = input.find_last_of(' ');
        if (lastSpace != std::string::npos)
        {
            std::string tail = input.substr(lastSpace + 1);
            bool allDigit = !tail.empty() &&
                std::all_of(tail.begin(), tail.end(), [](char c) { return c >= '0' && c <= '9'; });
            if (allDigit)
            {
                count = uint32(atoi(tail.c_str()));
                if (count == 0)
                    count = 1;
                namePart = input.substr(0, lastSpace);
            }
        }

        std::vector<uint32> found;
        SearchItems(handler, namePart, found);

        if (found.empty())
        {
            handler->PSendSysMessage("|cffff0000未找到|r 名称含「%s」的物品。", namePart.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        // 唯一匹配 -> 直接给
        if (found.size() == 1)
        {
            uint32 id = found[0];
            if (GiveItem(handler, target, id, count))
            {
                ItemTemplate const* p = sObjectMgr->GetItemTemplate(id);
                handler->PSendSysMessage("|cff00ff00已添加|r %s x%u (ID:%u)",
                    p ? p->GetName(handler->GetSessionDbLocaleIndex()).c_str() : "", count, id);
                s_lastItem[accId] = id;
            }
            return true;
        }

        // 多个匹配
        if (forceAll)
        {
            handler->PSendSysMessage("|cffffcc00强制模式：添加全部 %u 项|r", uint32(found.size()));
            uint32 ok = 0;
            for (uint32 id : found)
            {
                if (GiveItem(handler, target, id, count))
                {
                    ++ok;
                    s_lastItem[accId] = id;
                }
            }
            handler->PSendSysMessage("|cff00ff00已添加 %u 件|r", ok);
            return true;
        }

        ListItemCandidates(handler, found, namePart);
        return true;
    }

    static bool HandleSmartAddCommand(ChatHandler* handler, char const* args)
    {
        return DoSmartAdd(handler, args, false);
    }

    static bool HandleSmartAddForceCommand(ChatHandler* handler, char const* args)
    {
        return DoSmartAdd(handler, args, true);
    }

    // ==============================================================
    // .spawn —— 主逻辑
    // ==============================================================
    static bool SpawnOne(ChatHandler* handler, Player* player, uint32 entry, uint32 amount)
    {
        CreatureTemplate const* ct = sObjectMgr->GetCreatureTemplate(entry);
        if (!ct)
            return false;

        uint32 accId = handler->GetSession() ? handler->GetSession()->GetAccountId() : 0;

        for (uint32 i = 0; i < amount; ++i)
        {
            float angle = frand(0.0f, 2.0f * float(M_PI));
            float dist  = (amount > 1) ? frand(2.0f, 5.0f) : 2.0f;
            float x = player->GetPositionX() + dist * std::cos(angle);
            float y = player->GetPositionY() + dist * std::sin(angle);
            float z = player->GetPositionZ();

            if (TempSummon* summon = player->SummonCreature(entry, x, y, z,
                    player->GetOrientation(), TEMPSUMMON_MANUAL_DESPAWN, 0s))
            {
                s_mySummons[accId].push_back(summon->GetGUID());
            }
        }
        return true;
    }

    static bool DoSmartSpawn(ChatHandler* handler, char const* args, bool forceAll)
    {
        if (!*args)
        {
            handler->SendSysMessage("用法: .spawn <生物名> [x数量]");
            handler->SendSysMessage("      .spawn 石爪豺狼人 x5      (刷5只)");
            handler->SendSysMessage("      .spawn 熊, 狼, 豺狼人     (逗号分隔批量)");
            handler->SendSysMessage("      .spawn! <生物名>          (全部匹配项各刷一只)");
            handler->SendSysMessage("      .spawn last               (重复上次)");
            handler->SendSysMessage("      .clean [半径]             (清理自己刷的)");
            handler->SetSentErrorMessage(true);
            return false;
        }

        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
        {
            handler->SendSysMessage("此命令只能在游戏内使用。");
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint32 accId = handler->GetSession()->GetAccountId();
        std::string input = args;

        // ---- .spawn last ----
        if (input == "last")
        {
            auto it = s_lastCreature.find(accId);
            if (it == s_lastCreature.end())
            {
                handler->SendSysMessage("没有上次记录。");
                handler->SetSentErrorMessage(true);
                return false;
            }
            SpawnOne(handler, player, it->second, 1);
            handler->PSendSysMessage("|cff00ff00已重复召唤|r (ID:%u)", it->second);
            return true;
        }

        // ---- 批量模式 ----
        if (input.find(',') != std::string::npos)
        {
            std::vector<std::string> names = SplitByComma(input);
            uint32 okCount = 0, failCount = 0;

            handler->PSendSysMessage("|cffffcc00批量召唤 %u 项...|r", uint32(names.size()));

            for (std::string const& nm : names)
            {
                std::vector<uint32> found;
                SearchCreatures(handler, nm, found, 5);

                if (found.empty())
                {
                    handler->PSendSysMessage("  |cffff0000[未找到]|r %s", nm.c_str());
                    ++failCount;
                }
                else if (found.size() == 1 || forceAll)
                {
                    for (uint32 id : found)
                    {
                        SpawnOne(handler, player, id, 1);
                        handler->PSendSysMessage("  |cff00ff00[OK]|r %s (ID:%u)", nm.c_str(), id);
                        ++okCount;
                        s_lastCreature[accId] = id;
                        if (!forceAll)
                            break;
                    }
                }
                else
                {
                    handler->PSendSysMessage("  |cffffff00[多个匹配]|r %s (%u个)，已跳过",
                        nm.c_str(), uint32(found.size()));
                    ++failCount;
                }
            }

            handler->PSendSysMessage("|cffffcc00完成：成功 %u，跳过 %u|r", okCount, failCount);
            return true;
        }

        // ---- 解析 xN 数量后缀 ----
        std::string namePart = input;
        uint32 amount = 1;

        size_t lastSpace = input.find_last_of(' ');
        if (lastSpace != std::string::npos)
        {
            std::string tail = input.substr(lastSpace + 1);
            if (tail.size() >= 2 && (tail[0] == 'x' || tail[0] == 'X'))
            {
                std::string numStr = tail.substr(1);
                bool allDigit = std::all_of(numStr.begin(), numStr.end(),
                    [](char c) { return c >= '0' && c <= '9'; });
                if (allDigit)
                {
                    amount = uint32(atoi(numStr.c_str()));
                    if (amount == 0)  amount = 1;
                    if (amount > 50)  amount = 50;   // 上限保护
                    namePart = input.substr(0, lastSpace);
                }
            }
        }

        std::vector<uint32> found;
        SearchCreatures(handler, namePart, found);

        if (found.empty())
        {
            handler->PSendSysMessage("|cffff0000未找到|r 名称含「%s」的生物。", namePart.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (found.size() == 1)
        {
            uint32 id = found[0];
            SpawnOne(handler, player, id, amount);
            CreatureTemplate const* ct = sObjectMgr->GetCreatureTemplate(id);
            handler->PSendSysMessage("|cff00ff00已召唤|r %s x%u (ID:%u)",
                ct ? ct->Name.c_str() : "", amount, id);
            s_lastCreature[accId] = id;
            return true;
        }

        if (forceAll)
        {
            handler->PSendSysMessage("|cffffcc00强制模式：召唤全部 %u 种|r", uint32(found.size()));
            for (uint32 id : found)
            {
                SpawnOne(handler, player, id, amount);
                s_lastCreature[accId] = id;
            }
            handler->PSendSysMessage("|cff00ff00完成|r");
            return true;
        }

        ListCreatureCandidates(handler, found, namePart);
        return true;
    }

    static bool HandleSmartSpawnCommand(ChatHandler* handler, char const* args)
    {
        return DoSmartSpawn(handler, args, false);
    }

    static bool HandleSmartSpawnForceCommand(ChatHandler* handler, char const* args)
    {
        return DoSmartSpawn(handler, args, true);
    }

    // ==============================================================
    // .clean —— 只清理自己召唤的生物
    // ==============================================================
    static bool HandleSmartCleanCommand(ChatHandler* handler, char const* args)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
        {
            handler->SendSysMessage("此命令只能在游戏内使用。");
            handler->SetSentErrorMessage(true);
            return false;
        }

        float radius = 30.0f;
        if (*args)
        {
            float r = float(atof(args));
            if (r > 0.0f && r <= 500.0f)
                radius = r;
        }

        uint32 accId = handler->GetSession()->GetAccountId();
        auto it = s_mySummons.find(accId);
        if (it == s_mySummons.end() || it->second.empty())
        {
            handler->SendSysMessage("没有记录到你召唤的生物。");
            return true;
        }

        uint32 removed = 0;
        std::vector<ObjectGuid> remaining;

        for (ObjectGuid guid : it->second)
        {
            Creature* c = ObjectAccessor::GetCreature(*player, guid);
            if (!c || !c->IsInWorld())
                continue;   // 已经不存在，直接丢弃记录

            if (player->GetDistance(c) <= radius)
            {
                c->DespawnOrUnsummon();
                ++removed;
            }
            else
                remaining.push_back(guid);
        }

        it->second = remaining;

        handler->PSendSysMessage("|cff00ff00已清理 %u 只|r（半径 %.0f 码内，仅限你召唤的）",
            removed, radius);
        if (!remaining.empty())
            handler->PSendSysMessage("|cff888888还有 %u 只在范围外|r", uint32(remaining.size()));

        return true;
    }
};

void AddSC_smartadd_commandscript()
{
    new smartadd_commandscript();
}
