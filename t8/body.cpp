/*
 * 智能添加指令 v2 —— cs_smartadd.cpp
 *
 * v2 相对 v1 的改动：
 *   1. 纯数字输入 -> 直接按 ID 处理（v1 会当名字搜，报"未找到"）
 *   2. 严格身份判定 -> 只有【所有关键字段完全一致】才视为同一件物品
 *      （v1 只比名字，会把埃辛诺斯战刃主手/副手混为一谈）
 *   3. 有差异时按【推荐度评分排序】列出，★标注最优项
 *   4. 修正提示文字（v1 错误地说"点击链接可获取"，实际链接只能看属性）
 *
 * 指令：
 *   .add <名称|ID> [数量]        智能添加物品
 *   .add A, B, C                 逗号分隔批量
 *   .add! <名称>                 强制：所有匹配项全给
 *   .add last                    重复上次
 *   .spawn <名称|ID> [xN]        智能召唤生物
 *   .spawn A, B, C               批量
 *   .spawn! <名称>               强制：所有匹配项各刷一只
 *   .spawn last                  重复上次
 *   .clean [半径]                清理自己召唤的生物（默认30码）
 *
 * 放置：D:\TrinityCore\src\server\scripts\Commands\cs_smartadd.cpp
 */

#include "mock.h"
#include <vector>
#include <string>
#include <algorithm>
#include <unordered_map>
#include <cmath>

static std::unordered_map<uint32 /*accountId*/, uint32 /*itemId*/>     s_lastItem;
static std::unordered_map<uint32 /*accountId*/, uint32 /*creatureId*/> s_lastCreature;
static std::unordered_map<uint32 /*accountId*/, std::vector<ObjectGuid>> s_mySummons;

class smartadd_commandscript : public CommandScript
{
public:
    smartadd_commandscript() : CommandScript("smartadd_commandscript") { }

    std::vector<ChatCommand> GetCommands() const override
    {
        static std::vector<ChatCommand> commandTable =
        {
            { "add",    rbac::RBAC_PERM_COMMAND_SMART_ADD,   false, &HandleSmartAddCommand,        "" },
            { "add!",   rbac::RBAC_PERM_COMMAND_SMART_ADD,   false, &HandleSmartAddForceCommand,   "" },
            { "spawn",  rbac::RBAC_PERM_COMMAND_SMART_SPAWN, false, &HandleSmartSpawnCommand,      "" },
            { "spawn!", rbac::RBAC_PERM_COMMAND_SMART_SPAWN, false, &HandleSmartSpawnForceCommand, "" },
            { "clean",  rbac::RBAC_PERM_COMMAND_SMART_SPAWN, false, &HandleSmartCleanCommand,      "" },
        };
        return commandTable;
    }

    // ==============================================================
    // 基础工具
    // ==============================================================
    static bool IsAllDigits(std::string const& s)
    {
        return !s.empty() && std::all_of(s.begin(), s.end(),
            [](char c) { return c >= '0' && c <= '9'; });
    }

    static std::vector<std::string> SplitByComma(std::string const& input)
    {
        std::vector<std::string> parts;
        std::string cur;
        for (char c : input)
        {
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

        for (auto& s : parts)
        {
            size_t b = s.find_first_not_of(" \t");
            size_t e = s.find_last_not_of(" \t");
            s = (b == std::string::npos) ? "" : s.substr(b, e - b + 1);
        }
        parts.erase(std::remove_if(parts.begin(), parts.end(),
            [](std::string const& s) { return s.empty(); }), parts.end());
        return parts;
    }

    // ==============================================================
    // 物品：严格身份判定
    // 只有【全部关键字段一致】才算同一件物品的重复条目
    // 任一不同（尤其 InventoryType 主手/副手）都视为不同物品
    // ==============================================================
    static bool SameItemIdentity(ChatHandler* handler, uint32 idA, uint32 idB)
    {
        ItemTemplate const* a = sObjectMgr->GetItemTemplate(idA);
        ItemTemplate const* b = sObjectMgr->GetItemTemplate(idB);
        if (!a || !b)
            return false;

        LocaleConstant loc = handler->GetSessionDbLocaleIndex();
        if (a->GetName(loc)   != b->GetName(loc))   return false;
        if (a->Class          != b->Class)          return false;  // 武器/护甲
        if (a->SubClass       != b->SubClass)       return false;  // 剑/斧/板甲
        if (a->InventoryType  != b->InventoryType)  return false;  // ★主手/副手关键
        if (a->Quality        != b->Quality)        return false;
        if (a->ItemLevel      != b->ItemLevel)      return false;
        if (a->RequiredLevel  != b->RequiredLevel)  return false;
        if (a->Armor          != b->Armor)          return false;
        if (a->Description    != b->Description)    return false;
        if (a->StatsCount     != b->StatsCount)     return false;

        for (uint32 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
        {
            if (a->ItemStat[i].ItemStatType  != b->ItemStat[i].ItemStatType)  return false;
            if (a->ItemStat[i].ItemStatValue != b->ItemStat[i].ItemStatValue) return false;
        }
        for (uint32 i = 0; i < MAX_ITEM_PROTO_DAMAGES; ++i)
        {
            if (std::fabs(a->Damage[i].DamageMin - b->Damage[i].DamageMin) > 0.01f) return false;
            if (std::fabs(a->Damage[i].DamageMax - b->Damage[i].DamageMax) > 0.01f) return false;
        }
        return true;
    }

    static bool AllItemsIdentical(ChatHandler* handler, std::vector<uint32> const& ids)
    {
        if (ids.size() <= 1)
            return true;
        for (size_t i = 1; i < ids.size(); ++i)
            if (!SameItemIdentity(handler, ids[0], ids[i]))
                return false;
        return true;
    }

    // 推荐度评分：装等/品质权重最高，其次属性总和
    static double ScoreItem(uint32 id)
    {
        ItemTemplate const* p = sObjectMgr->GetItemTemplate(id);
        if (!p)
            return 0.0;

        double s = 0.0;
        s += double(p->ItemLevel) * 100.0;
        s += double(p->Quality)   * 500.0;
        s += double(p->Armor);
        for (uint32 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
            s += double(p->ItemStat[i].ItemStatValue);
        for (uint32 i = 0; i < MAX_ITEM_PROTO_DAMAGES; ++i)
            s += double(p->Damage[i].DamageMin + p->Damage[i].DamageMax) / 2.0 * 10.0;
        return s;
    }

    // ==============================================================
    // 生物：严格身份判定
    // ==============================================================
    static std::string GetCreatureName(ChatHandler* handler, uint32 id)
    {
        CreatureTemplate const* ct = sObjectMgr->GetCreatureTemplate(id);
        if (!ct)
            return "";
        uint8 loc = handler->GetSessionDbLocaleIndex();
        if (CreatureLocale const* cl = sObjectMgr->GetCreatureLocale(id))
            if (cl->Name.size() > loc && !cl->Name[loc].empty())
                return cl->Name[loc];
        return ct->Name;
    }

    static bool SameCreatureIdentity(ChatHandler* handler, uint32 idA, uint32 idB)
    {
        CreatureTemplate const* a = sObjectMgr->GetCreatureTemplate(idA);
        CreatureTemplate const* b = sObjectMgr->GetCreatureTemplate(idB);
        if (!a || !b)
            return false;

        if (GetCreatureName(handler, idA) != GetCreatureName(handler, idB)) return false;
        if (a->Title      != b->Title)      return false;   // 副标题（如"部落守卫"）
        if (a->minlevel   != b->minlevel)   return false;
        if (a->maxlevel   != b->maxlevel)   return false;
        if (a->faction    != b->faction)    return false;   // 阵营不同=不同怪
        if (a->rank       != b->rank)       return false;   // 精英/BOSS
        if (a->type       != b->type)       return false;   // 野兽/人形
        if (a->unit_class != b->unit_class) return false;
        if (a->Modelid1   != b->Modelid1)   return false;   // 模型不同=外观不同
        if (std::fabs(a->ModHealth - b->ModHealth) > 0.01f) return false;
        if (std::fabs(a->ModDamage - b->ModDamage) > 0.01f) return false;
        return true;
    }

    static bool AllCreaturesIdentical(ChatHandler* handler, std::vector<uint32> const& ids)
    {
        if (ids.size() <= 1)
            return true;
        for (size_t i = 1; i < ids.size(); ++i)
            if (!SameCreatureIdentity(handler, ids[0], ids[i]))
                return false;
        return true;
    }

    static double ScoreCreature(uint32 id)
    {
        CreatureTemplate const* ct = sObjectMgr->GetCreatureTemplate(id);
        if (!ct)
            return 0.0;
        double s = 0.0;
        s += double(ct->maxlevel) * 100.0;
        s += double(ct->rank)     * 50.0;
        s += double(ct->ModHealth) * 10.0;
        s += double(ct->ModDamage) * 10.0;
        return s;
    }

    // ==============================================================
    // 搜索
    // ==============================================================
    static void SearchItems(ChatHandler* handler, std::string const& namePart,
                            std::vector<uint32>& results, uint32 maxResults = 30)
    {
        std::wstring w;
        if (!Utf8toWStr(namePart, w))
            return;
        wstrToLower(w);

        LocaleConstant loc = handler->GetSessionDbLocaleIndex();
        for (auto const& pair : sObjectMgr->GetItemTemplateStore())
        {
            std::string name = pair.second.GetName(loc);
            if (name.empty())
                continue;
            if (Utf8FitTo(name, w))
            {
                results.push_back(pair.first);
                if (results.size() >= maxResults)
                    return;
            }
        }
    }

    static void SearchCreatures(ChatHandler* handler, std::string const& namePart,
                                std::vector<uint32>& results, uint32 maxResults = 30)
    {
        std::wstring w;
        if (!Utf8toWStr(namePart, w))
            return;
        wstrToLower(w);

        uint8 loc = handler->GetSessionDbLocaleIndex();
        for (auto const& pair : sObjectMgr->GetCreatureTemplates())
        {
            uint32 id = pair.first;
            bool matched = false;

            if (CreatureLocale const* cl = sObjectMgr->GetCreatureLocale(id))
                if (cl->Name.size() > loc && !cl->Name[loc].empty())
                    if (Utf8FitTo(cl->Name[loc], w))
                        matched = true;

            if (!matched && !pair.second.Name.empty())
                if (Utf8FitTo(pair.second.Name, w))
                    matched = true;

            if (matched)
            {
                results.push_back(id);
                if (results.size() >= maxResults)
                    return;
            }
        }
    }

    // ==============================================================
    // 发放物品
    // ==============================================================
    static bool GiveItem(ChatHandler* handler, Player* target, uint32 itemId, uint32 count)
    {
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
        if (!proto)
            return false;

        if (count > 1000)
            count = 1000;

        uint32 noSpace = 0;
        ItemPosCountVec dest;
        InventoryResult msg = target->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemId, count, &noSpace);
        if (msg != EQUIP_ERR_OK)
            count -= noSpace;

        if (count == 0 || dest.empty())
        {
            handler->PSendSysMessage("|cffff0000背包空间不足|r，无法放入 %s",
                proto->GetName(handler->GetSessionDbLocaleIndex()).c_str());
            return false;
        }

        Item* item = target->StoreNewItem(dest, itemId, true, GenerateItemRandomPropertyId(itemId));
        if (item)
            target->SendNewItem(item, count, false, true);
        return item != nullptr;
    }

    // ==============================================================
    // 列候选（按推荐度排序，★标注最优）
    // ==============================================================
    static void ListItemCandidates(ChatHandler* handler, std::vector<uint32> ids,
                                   std::string const& keyword)
    {
        std::sort(ids.begin(), ids.end(),
            [](uint32 a, uint32 b) { return ScoreItem(a) > ScoreItem(b); });

        LocaleConstant loc = handler->GetSessionDbLocaleIndex();
        handler->PSendSysMessage("|cffffcc00「%s」匹配到 %u 件物品（按推荐度排序）：|r",
            keyword.c_str(), uint32(ids.size()));

        for (size_t i = 0; i < ids.size(); ++i)
        {
            ItemTemplate const* p = sObjectMgr->GetItemTemplate(ids[i]);
            if (!p)
                continue;

            char const* mark = (i == 0) ? "|cffffd700[推荐]|r " : "         ";
            handler->PSendSysMessage("%s|cffffffff%s|r  ID:|cff00ff00%u|r  装等:%u 品质:%u",
                mark, p->GetName(loc).c_str(), ids[i], p->ItemLevel, p->Quality);
        }

        handler->PSendSysMessage("|cff00ccff用 .add %u 获取推荐项，或 .add <其他ID> 指定|r", ids[0]);
        handler->PSendSysMessage("|cff00ccff用 .add! %s 一次性全部获取|r", keyword.c_str());
    }

    static void ListCreatureCandidates(ChatHandler* handler, std::vector<uint32> ids,
                                       std::string const& keyword)
    {
        std::sort(ids.begin(), ids.end(),
            [](uint32 a, uint32 b) { return ScoreCreature(a) > ScoreCreature(b); });

        handler->PSendSysMessage("|cffffcc00「%s」匹配到 %u 种生物（按推荐度排序）：|r",
            keyword.c_str(), uint32(ids.size()));

        for (size_t i = 0; i < ids.size(); ++i)
        {
            CreatureTemplate const* ct = sObjectMgr->GetCreatureTemplate(ids[i]);
            if (!ct)
                continue;

            char const* mark = (i == 0) ? "|cffffd700[推荐]|r " : "         ";
            handler->PSendSysMessage("%s|cffffffff%s|r  ID:|cff00ff00%u|r  等级:%u-%u",
                mark, GetCreatureName(handler, ids[i]).c_str(), ids[i],
                ct->minlevel, ct->maxlevel);
        }

        handler->PSendSysMessage("|cff00ccff用 .spawn %u 召唤推荐项，或 .spawn <其他ID> 指定|r", ids[0]);
        handler->PSendSysMessage("|cff00ccff用 .spawn! %s 全部各刷一只|r", keyword.c_str());
    }

    // ==============================================================
    // .add 主逻辑
    // ==============================================================
    static bool DoSmartAdd(ChatHandler* handler, char const* args, bool forceAll)
    {
        if (!*args)
        {
            handler->SendSysMessage("用法: .add <物品名或ID> [数量]");
            handler->SendSysMessage("      .add 32837              按ID直接给");
            handler->SendSysMessage("      .add 战刃 5             按名字，带数量");
            handler->SendSysMessage("      .add 火焰, 霜之哀伤     逗号批量");
            handler->SendSysMessage("      .add! 战刃              全部匹配项都给");
            handler->SendSysMessage("      .add last               重复上次");
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

        // ---- 批量模式 ----
        if (input.find(',') != std::string::npos)
        {
            std::vector<std::string> names = SplitByComma(input);
            uint32 okCount = 0, skipCount = 0;
            handler->PSendSysMessage("|cffffcc00批量添加 %u 项...|r", uint32(names.size()));

            for (std::string const& nm : names)
            {
                // 纯数字 -> 按ID
                if (IsAllDigits(nm))
                {
                    uint32 id = uint32(atoi(nm.c_str()));
                    ItemTemplate const* p = sObjectMgr->GetItemTemplate(id);
                    if (p && GiveItem(handler, target, id, 1))
                    {
                        handler->PSendSysMessage("  |cff00ff00[OK]|r %s (ID:%u)",
                            p->GetName(handler->GetSessionDbLocaleIndex()).c_str(), id);
                        ++okCount;
                        s_lastItem[accId] = id;
                    }
                    else
                    {
                        handler->PSendSysMessage("  |cffff0000[ID无效]|r %s", nm.c_str());
                        ++skipCount;
                    }
                    continue;
                }

                std::vector<uint32> found;
                SearchItems(handler, nm, found, 10);

                if (found.empty())
                {
                    handler->PSendSysMessage("  |cffff0000[未找到]|r %s", nm.c_str());
                    ++skipCount;
                }
                else if (found.size() == 1 || AllItemsIdentical(handler, found) || forceAll)
                {
                    uint32 loops = forceAll ? uint32(found.size()) : 1u;
                    for (uint32 k = 0; k < loops; ++k)
                    {
                        if (GiveItem(handler, target, found[k], 1))
                        {
                            ItemTemplate const* p = sObjectMgr->GetItemTemplate(found[k]);
                            handler->PSendSysMessage("  |cff00ff00[OK]|r %s (ID:%u)",
                                p ? p->GetName(handler->GetSessionDbLocaleIndex()).c_str() : "", found[k]);
                            ++okCount;
                            s_lastItem[accId] = found[k];
                        }
                    }
                }
                else
                {
                    // 有差异：给推荐项（评分最高），并提示
                    std::sort(found.begin(), found.end(),
                        [](uint32 a, uint32 b) { return ScoreItem(a) > ScoreItem(b); });
                    if (GiveItem(handler, target, found[0], 1))
                    {
                        ItemTemplate const* p = sObjectMgr->GetItemTemplate(found[0]);
                        handler->PSendSysMessage("  |cffffd700[推荐]|r %s (ID:%u) |cff888888还有%u个相似项|r",
                            p ? p->GetName(handler->GetSessionDbLocaleIndex()).c_str() : "",
                            found[0], uint32(found.size() - 1));
                        ++okCount;
                        s_lastItem[accId] = found[0];
                    }
                }
            }

            handler->PSendSysMessage("|cffffcc00完成：成功 %u，跳过 %u|r", okCount, skipCount);
            return true;
        }

        // ---- 解析尾部数量 ----
        std::string namePart = input;
        uint32 count = 1;
        size_t lastSpace = input.find_last_of(' ');
        if (lastSpace != std::string::npos)
        {
            std::string tail = input.substr(lastSpace + 1);
            if (IsAllDigits(tail))
            {
                count = uint32(atoi(tail.c_str()));
                if (count == 0)
                    count = 1;
                namePart = input.substr(0, lastSpace);
            }
        }

        // ---- 规则1：纯数字 = ID 直取 ----
        if (IsAllDigits(namePart))
        {
            uint32 id = uint32(atoi(namePart.c_str()));
            ItemTemplate const* p = sObjectMgr->GetItemTemplate(id);
            if (!p)
            {
                handler->PSendSysMessage("|cffff0000物品 ID %u 不存在。|r", id);
                handler->SetSentErrorMessage(true);
                return false;
            }
            if (GiveItem(handler, target, id, count))
            {
                handler->PSendSysMessage("|cff00ff00已添加|r %s x%u (ID:%u)",
                    p->GetName(handler->GetSessionDbLocaleIndex()).c_str(), count, id);
                s_lastItem[accId] = id;
            }
            return true;
        }

        std::vector<uint32> found;
        SearchItems(handler, namePart, found);

        if (found.empty())
        {
            handler->PSendSysMessage("|cffff0000未找到|r 名称含「%s」的物品。", namePart.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        // ---- 规则2：唯一，或全部完全等价 -> 直接给 ----
        if (found.size() == 1 || AllItemsIdentical(handler, found))
        {
            uint32 id = found[0];
            if (GiveItem(handler, target, id, count))
            {
                ItemTemplate const* p = sObjectMgr->GetItemTemplate(id);
                if (found.size() > 1)
                    handler->PSendSysMessage("|cff00ff00已添加|r %s x%u (ID:%u) |cff888888[%u个完全相同条目]|r",
                        p->GetName(handler->GetSessionDbLocaleIndex()).c_str(), count, id, uint32(found.size()));
                else
                    handler->PSendSysMessage("|cff00ff00已添加|r %s x%u (ID:%u)",
                        p->GetName(handler->GetSessionDbLocaleIndex()).c_str(), count, id);
                s_lastItem[accId] = id;
            }
            return true;
        }

        // ---- 强制模式 ----
        if (forceAll)
        {
            handler->PSendSysMessage("|cffffcc00强制模式：添加全部 %u 项|r", uint32(found.size()));
            uint32 ok = 0;
            for (uint32 id : found)
                if (GiveItem(handler, target, id, count))
                {
                    ++ok;
                    s_lastItem[accId] = id;
                }
            handler->PSendSysMessage("|cff00ff00已添加 %u 件|r", ok);
            return true;
        }

        // ---- 规则3：有差异 -> 列候选 ----
        ListItemCandidates(handler, found, namePart);
        return true;
    }

    static bool HandleSmartAddCommand(ChatHandler* handler, char const* args)
    { return DoSmartAdd(handler, args, false); }

    static bool HandleSmartAddForceCommand(ChatHandler* handler, char const* args)
    { return DoSmartAdd(handler, args, true); }

    // ==============================================================
    // .spawn 主逻辑
    // ==============================================================
    static bool SpawnOne(ChatHandler* handler, Player* player, uint32 entry, uint32 amount)
    {
        if (!sObjectMgr->GetCreatureTemplate(entry))
            return false;

        uint32 accId = handler->GetSession() ? handler->GetSession()->GetAccountId() : 0;
        for (uint32 i = 0; i < amount; ++i)
        {
            float angle = frand(0.0f, 2.0f * float(M_PI));
            float dist  = (amount > 1) ? frand(2.0f, 5.0f) : 2.0f;
            float x = player->GetPositionX() + dist * std::cos(angle);
            float y = player->GetPositionY() + dist * std::sin(angle);
            float z = player->GetPositionZ();

            if (TempSummon* s = player->SummonCreature(entry, x, y, z,
                    player->GetOrientation(), TEMPSUMMON_MANUAL_DESPAWN, 0))
                s_mySummons[accId].push_back(s->GetGUID());
        }
        return true;
    }

    static bool DoSmartSpawn(ChatHandler* handler, char const* args, bool forceAll)
    {
        if (!*args)
        {
            handler->SendSysMessage("用法: .spawn <生物名或ID> [x数量]");
            handler->SendSysMessage("      .spawn 299              按ID召唤");
            handler->SendSysMessage("      .spawn 豺狼人 x5        按名字，刷5只");
            handler->SendSysMessage("      .spawn 熊, 狼           逗号批量");
            handler->SendSysMessage("      .spawn! 豺狼人          全部匹配项各刷一只");
            handler->SendSysMessage("      .clean [半径]           清理自己刷的");
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

        // ---- last ----
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
            handler->PSendSysMessage("|cff00ff00已重复召唤|r %s (ID:%u)",
                GetCreatureName(handler, it->second).c_str(), it->second);
            return true;
        }

        // ---- 批量 ----
        if (input.find(',') != std::string::npos)
        {
            std::vector<std::string> names = SplitByComma(input);
            uint32 okCount = 0, skipCount = 0;
            handler->PSendSysMessage("|cffffcc00批量召唤 %u 项...|r", uint32(names.size()));

            for (std::string const& nm : names)
            {
                if (IsAllDigits(nm))
                {
                    uint32 id = uint32(atoi(nm.c_str()));
                    if (sObjectMgr->GetCreatureTemplate(id))
                    {
                        SpawnOne(handler, player, id, 1);
                        handler->PSendSysMessage("  |cff00ff00[OK]|r %s (ID:%u)",
                            GetCreatureName(handler, id).c_str(), id);
                        ++okCount;
                        s_lastCreature[accId] = id;
                    }
                    else
                    {
                        handler->PSendSysMessage("  |cffff0000[ID无效]|r %s", nm.c_str());
                        ++skipCount;
                    }
                    continue;
                }

                std::vector<uint32> found;
                SearchCreatures(handler, nm, found, 10);

                if (found.empty())
                {
                    handler->PSendSysMessage("  |cffff0000[未找到]|r %s", nm.c_str());
                    ++skipCount;
                }
                else if (found.size() == 1 || AllCreaturesIdentical(handler, found) || forceAll)
                {
                    uint32 loops = forceAll ? uint32(found.size()) : 1u;
                    for (uint32 k = 0; k < loops; ++k)
                    {
                        SpawnOne(handler, player, found[k], 1);
                        handler->PSendSysMessage("  |cff00ff00[OK]|r %s (ID:%u)",
                            GetCreatureName(handler, found[k]).c_str(), found[k]);
                        ++okCount;
                        s_lastCreature[accId] = found[k];
                    }
                }
                else
                {
                    std::sort(found.begin(), found.end(),
                        [](uint32 a, uint32 b) { return ScoreCreature(a) > ScoreCreature(b); });
                    SpawnOne(handler, player, found[0], 1);
                    handler->PSendSysMessage("  |cffffd700[推荐]|r %s (ID:%u) |cff888888还有%u个相似项|r",
                        GetCreatureName(handler, found[0]).c_str(), found[0], uint32(found.size() - 1));
                    ++okCount;
                    s_lastCreature[accId] = found[0];
                }
            }

            handler->PSendSysMessage("|cffffcc00完成：成功 %u，跳过 %u|r", okCount, skipCount);
            return true;
        }

        // ---- 解析 xN ----
        std::string namePart = input;
        uint32 amount = 1;
        size_t lastSpace = input.find_last_of(' ');
        if (lastSpace != std::string::npos)
        {
            std::string tail = input.substr(lastSpace + 1);
            if (tail.size() >= 2 && (tail[0] == 'x' || tail[0] == 'X'))
            {
                std::string num = tail.substr(1);
                if (IsAllDigits(num))
                {
                    amount = uint32(atoi(num.c_str()));
                    if (amount == 0) amount = 1;
                    if (amount > 50) amount = 50;
                    namePart = input.substr(0, lastSpace);
                }
            }
        }

        // ---- 规则1：纯数字 = ID ----
        if (IsAllDigits(namePart))
        {
            uint32 id = uint32(atoi(namePart.c_str()));
            if (!sObjectMgr->GetCreatureTemplate(id))
            {
                handler->PSendSysMessage("|cffff0000生物 ID %u 不存在。|r", id);
                handler->SetSentErrorMessage(true);
                return false;
            }
            SpawnOne(handler, player, id, amount);
            handler->PSendSysMessage("|cff00ff00已召唤|r %s x%u (ID:%u)",
                GetCreatureName(handler, id).c_str(), amount, id);
            s_lastCreature[accId] = id;
            return true;
        }

        std::vector<uint32> found;
        SearchCreatures(handler, namePart, found);

        if (found.empty())
        {
            handler->PSendSysMessage("|cffff0000未找到|r 名称含「%s」的生物。", namePart.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        // ---- 规则2：唯一或完全等价 ----
        if (found.size() == 1 || AllCreaturesIdentical(handler, found))
        {
            uint32 id = found[0];
            SpawnOne(handler, player, id, amount);
            if (found.size() > 1)
                handler->PSendSysMessage("|cff00ff00已召唤|r %s x%u (ID:%u) |cff888888[%u个完全相同条目]|r",
                    GetCreatureName(handler, id).c_str(), amount, id, uint32(found.size()));
            else
                handler->PSendSysMessage("|cff00ff00已召唤|r %s x%u (ID:%u)",
                    GetCreatureName(handler, id).c_str(), amount, id);
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
            handler->SendSysMessage("|cff00ff00完成|r");
            return true;
        }

        ListCreatureCandidates(handler, found, namePart);
        return true;
    }

    static bool HandleSmartSpawnCommand(ChatHandler* handler, char const* args)
    { return DoSmartSpawn(handler, args, false); }

    static bool HandleSmartSpawnForceCommand(ChatHandler* handler, char const* args)
    { return DoSmartSpawn(handler, args, true); }

    // ==============================================================
    // .clean
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
                continue;
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
