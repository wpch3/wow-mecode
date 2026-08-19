/*
 * ============================================================================
 *  智能添加系统 v3 —— cs_smartadd.cpp
 * ============================================================================
 *
 *  v3 新增：Gossip 可点击菜单 + 无限分页 + 可复用批量框架
 *
 *  ── 指令 ──────────────────────────────────────────────────────────────
 *   .add   <名称|ID> [数量]     智能添加物品（多结果时弹出可点击菜单）
 *   .add   A, B, C              逗号批量
 *   .add!  <名称>               强制：所有匹配项全给
 *   .add   last                 重复上次
 *   .spawn <名称|ID> [xN]       智能召唤生物（同样支持点击菜单）
 *   .spawn A, B, C              逗号批量
 *   .spawn! <名称>              强制：所有匹配项各刷一只
 *   .clean [半径]               清理自己召唤的生物
 *
 *  ── 架构设计（为后续套装系统预留）────────────────────────────────────
 *   BatchSession   通用批量会话：可承载任意「候选列表 + 分页 + 点击回调」
 *   PickerType     业务类型枚举，新增功能只需加一个枚举 + 一个执行函数
 *   目前已接入：物品(ITEM)、生物(CREATURE)
 *   后续套装只需加 PICKER_GEARSET，复用全部分页与菜单逻辑
 *
 *  ── 关键技术点 ────────────────────────────────────────────────────────
 *   1. Gossip 硬上限 32 项/页，GossipDef.cpp:42 有 ASSERT，超过直接崩服
 *      -> 每页固定 29 条结果 + 3 个导航按钮 = 32，严格卡线
 *   2. 用玩家自身 GUID 调 SendGossipMenu，无需 NPC（MiscHandler.cpp 有
 *      guid.IsPlayer() 分支官方支持）
 *   3. 点击回调走 PlayerScript::OnGossipSelect(player, menuId, sender, action)
 *
 *  放置：D:\TrinityCore\src\server\scripts\Commands\cs_smartadd.cpp
 * ============================================================================
 */

#include "mock.h"
#include <vector>
#include <string>
#include <algorithm>
#include <unordered_map>
#include <cmath>

// ============================================================================
//  常量
// ============================================================================
namespace SmartAdd
{
    // Gossip 单页硬上限 32（超过 ASSERT 崩服），预留 3 个导航位
    static constexpr uint32 GOSSIP_HARD_LIMIT   = 32;
    static constexpr uint32 NAV_SLOTS           = 3;
    static constexpr uint32 ITEMS_PER_PAGE      = GOSSIP_HARD_LIMIT - NAV_SLOTS;  // 29

    // 搜索结果上限（无限分页，此值仅防止极端宽泛搜索拖慢服务器）
    static constexpr uint32 MAX_SEARCH_RESULTS  = 3000;   // 约 103 页

    // 自定义菜单 ID（避开官方 menu_id 范围）
    static constexpr uint32 MENU_ID             = 60000;

    // sender 编码：区分「选中条目」和「导航动作」
    static constexpr uint32 SENDER_PICK         = 1;
    static constexpr uint32 SENDER_NAV          = 2;

    // 导航 action
    static constexpr uint32 NAV_PREV            = 1;
    static constexpr uint32 NAV_NEXT            = 2;
    static constexpr uint32 NAV_CANCEL          = 3;
    static constexpr uint32 NAV_ALL             = 4;

    // 业务类型：新增功能在此扩展
    enum PickerType : uint8
    {
        PICKER_NONE     = 0,
        PICKER_ITEM     = 1,
        PICKER_CREATURE = 2,
        // PICKER_GEARSET = 3,   // 预留：套装系统
    };

    // ------------------------------------------------------------------
    //  批量会话 —— 通用结构，任何「多选一 + 分页」功能都可复用
    // ------------------------------------------------------------------
    struct BatchSession
    {
        PickerType          type    = PICKER_NONE;
        std::vector<uint32> results;        // 候选 ID 列表（已按推荐度排序）
        uint32              page    = 0;    // 当前页（0 基）
        uint32              amount  = 1;    // 数量 / 召唤只数
        std::string         keyword;        // 原始搜索词，用于标题

        uint32 TotalPages() const
        {
            if (results.empty())
                return 1;
            return uint32((results.size() + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE);
        }
        bool HasPrev() const { return page > 0; }
        bool HasNext() const { return page + 1 < TotalPages(); }
        void Clear() { type = PICKER_NONE; results.clear(); page = 0; amount = 1; keyword.clear(); }
    };

    // 每个账号一个会话
    static std::unordered_map<uint32, BatchSession> s_sessions;
    // 上次操作记录
    static std::unordered_map<uint32, uint32> s_lastItem;
    static std::unordered_map<uint32, uint32> s_lastCreature;
    // 自己召唤的生物
    static std::unordered_map<uint32, std::vector<ObjectGuid>> s_mySummons;

    // ------------------------------------------------------------------
    //  基础工具
    // ------------------------------------------------------------------
    inline bool IsAllDigits(std::string const& s)
    {
        return !s.empty() && std::all_of(s.begin(), s.end(),
            [](char c) { return c >= '0' && c <= '9'; });
    }

    inline std::vector<std::string> SplitByComma(std::string const& input)
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

    // ------------------------------------------------------------------
    //  物品：身份判定 / 评分 / 搜索 / 发放
    // ------------------------------------------------------------------
    inline bool SameItemIdentity(ChatHandler* handler, uint32 a_, uint32 b_)
    {
        ItemTemplate const* a = sObjectMgr->GetItemTemplate(a_);
        ItemTemplate const* b = sObjectMgr->GetItemTemplate(b_);
        if (!a || !b)
            return false;

        LocaleConstant loc = handler->GetSessionDbLocaleIndex();
        if (a->GetName(loc)  != b->GetName(loc))  return false;
        if (a->Class         != b->Class)         return false;
        if (a->SubClass      != b->SubClass)      return false;
        if (a->InventoryType != b->InventoryType) return false;   // 主手/副手关键
        if (a->Quality       != b->Quality)       return false;
        if (a->ItemLevel     != b->ItemLevel)     return false;
        if (a->RequiredLevel != b->RequiredLevel) return false;
        if (a->Armor         != b->Armor)         return false;
        if (a->Description   != b->Description)   return false;
        if (a->StatsCount    != b->StatsCount)    return false;

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

    inline bool AllItemsIdentical(ChatHandler* handler, std::vector<uint32> const& ids)
    {
        for (size_t i = 1; i < ids.size(); ++i)
            if (!SameItemIdentity(handler, ids[0], ids[i]))
                return false;
        return true;
    }

    inline double ScoreItem(uint32 id)
    {
        ItemTemplate const* p = sObjectMgr->GetItemTemplate(id);
        if (!p)
            return 0.0;
        double s = double(p->ItemLevel) * 100.0 + double(p->Quality) * 500.0 + double(p->Armor);
        for (uint32 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
            s += double(p->ItemStat[i].ItemStatValue);
        for (uint32 i = 0; i < MAX_ITEM_PROTO_DAMAGES; ++i)
            s += double(p->Damage[i].DamageMin + p->Damage[i].DamageMax) / 2.0 * 10.0;
        return s;
    }

    inline void SearchItems(ChatHandler* handler, std::string const& part, std::vector<uint32>& out)
    {
        std::wstring w;
        if (!Utf8toWStr(part, w))
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
                out.push_back(pair.first);
                if (out.size() >= MAX_SEARCH_RESULTS)
                    return;
            }
        }
    }

    inline bool GiveItem(ChatHandler* handler, Player* target, uint32 itemId, uint32 count)
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

    // ------------------------------------------------------------------
    //  生物：身份判定 / 评分 / 搜索 / 召唤
    // ------------------------------------------------------------------
    inline std::string GetCreatureName(ChatHandler* handler, uint32 id)
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

    inline bool SameCreatureIdentity(ChatHandler* handler, uint32 a_, uint32 b_)
    {
        CreatureTemplate const* a = sObjectMgr->GetCreatureTemplate(a_);
        CreatureTemplate const* b = sObjectMgr->GetCreatureTemplate(b_);
        if (!a || !b)
            return false;
        if (GetCreatureName(handler, a_) != GetCreatureName(handler, b_)) return false;
        if (a->Title      != b->Title)      return false;
        if (a->minlevel   != b->minlevel)   return false;
        if (a->maxlevel   != b->maxlevel)   return false;
        if (a->faction    != b->faction)    return false;
        if (a->rank       != b->rank)       return false;
        if (a->type       != b->type)       return false;
        if (a->unit_class != b->unit_class) return false;
        if (a->Modelid1   != b->Modelid1)   return false;
        if (std::fabs(a->ModHealth - b->ModHealth) > 0.01f) return false;
        if (std::fabs(a->ModDamage - b->ModDamage) > 0.01f) return false;
        return true;
    }

    inline bool AllCreaturesIdentical(ChatHandler* handler, std::vector<uint32> const& ids)
    {
        for (size_t i = 1; i < ids.size(); ++i)
            if (!SameCreatureIdentity(handler, ids[0], ids[i]))
                return false;
        return true;
    }

    inline double ScoreCreature(uint32 id)
    {
        CreatureTemplate const* ct = sObjectMgr->GetCreatureTemplate(id);
        if (!ct)
            return 0.0;
        return double(ct->maxlevel) * 100.0 + double(ct->rank) * 50.0
             + double(ct->ModHealth) * 10.0 + double(ct->ModDamage) * 10.0;
    }

    inline void SearchCreatures(ChatHandler* handler, std::string const& part, std::vector<uint32>& out)
    {
        std::wstring w;
        if (!Utf8toWStr(part, w))
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
                out.push_back(id);
                if (out.size() >= MAX_SEARCH_RESULTS)
                    return;
            }
        }
    }

    inline void SpawnOne(Player* player, uint32 entry, uint32 amount)
    {
        if (!sObjectMgr->GetCreatureTemplate(entry))
            return;
        uint32 accId = player->GetSession()->GetAccountId();
        for (uint32 i = 0; i < amount; ++i)
        {
            float angle = frand(0.0f, 2.0f * float(M_PI));
            float dist  = (amount > 1) ? frand(2.0f, 5.0f) : 2.0f;
            float x = player->GetPositionX() + dist * std::cos(angle);
            float y = player->GetPositionY() + dist * std::sin(angle);
            if (TempSummon* s = player->SummonCreature(entry, x, y, player->GetPositionZ(),
                    player->GetOrientation(), TEMPSUMMON_MANUAL_DESPAWN, 0))
                s_mySummons[accId].push_back(s->GetGUID());
        }
    }

    // ------------------------------------------------------------------
    //  Gossip 菜单渲染 —— 通用分页，任何 PickerType 都走这里
    // ------------------------------------------------------------------
    inline void SendPickerMenu(Player* player, ChatHandler* handler)
    {
        uint32 accId = player->GetSession()->GetAccountId();
        auto it = s_sessions.find(accId);
        if (it == s_sessions.end() || it->second.results.empty())
            return;

        BatchSession& ss = it->second;
        player->PlayerTalkClass->ClearMenus();
        player->PlayerTalkClass->GetGossipMenu().SetMenuId(MENU_ID);

        uint32 total      = uint32(ss.results.size());
        uint32 totalPages = ss.TotalPages();
        uint32 start      = ss.page * ITEMS_PER_PAGE;
        uint32 end        = std::min(start + ITEMS_PER_PAGE, total);

        uint32 gossipIdx = 0;

        // —— 结果条目 ——
        for (uint32 i = start; i < end; ++i)
        {
            uint32 id = ss.results[i];
            char label[256];

            if (ss.type == PICKER_ITEM)
            {
                ItemTemplate const* p = sObjectMgr->GetItemTemplate(id);
                if (!p)
                    continue;
                snprintf(label, sizeof(label), "%s%s  [装等%u 品质%u]",
                    (i == 0 ? "[荐] " : ""),
                    p->GetName(handler->GetSessionDbLocaleIndex()).c_str(),
                    p->ItemLevel, p->Quality);
            }
            else if (ss.type == PICKER_CREATURE)
            {
                CreatureTemplate const* ct = sObjectMgr->GetCreatureTemplate(id);
                if (!ct)
                    continue;
                snprintf(label, sizeof(label), "%s%s  [等级%u-%u]",
                    (i == 0 ? "[荐] " : ""),
                    GetCreatureName(handler, id).c_str(),
                    ct->minlevel, ct->maxlevel);
            }
            else
                continue;

            player->PlayerTalkClass->GetGossipMenu().AddMenuItem(
                int32(gossipIdx++), GOSSIP_ICON_CHAT, label, SENDER_PICK, id, "", 0, false);
        }

        // —— 导航按钮（最多 3 个，确保总数不超 32）——
        char nav[64];
        if (ss.HasPrev())
        {
            snprintf(nav, sizeof(nav), "<< 上一页  (第 %u/%u 页)", ss.page, totalPages);
            player->PlayerTalkClass->GetGossipMenu().AddMenuItem(
                int32(gossipIdx++), GOSSIP_ICON_CHAT, nav, SENDER_NAV, NAV_PREV, "", 0, false);
        }
        if (ss.HasNext())
        {
            snprintf(nav, sizeof(nav), ">> 下一页  (第 %u/%u 页)", ss.page + 2, totalPages);
            player->PlayerTalkClass->GetGossipMenu().AddMenuItem(
                int32(gossipIdx++), GOSSIP_ICON_CHAT, nav, SENDER_NAV, NAV_NEXT, "", 0, false);
        }
        player->PlayerTalkClass->GetGossipMenu().AddMenuItem(
            int32(gossipIdx++), GOSSIP_ICON_CHAT, "[ 取消 ]", SENDER_NAV, NAV_CANCEL, "", 0, false);

        // 安全兜底：绝不允许超过硬上限（超过 ASSERT 崩服）
        ASSERT(player->PlayerTalkClass->GetGossipMenu().GetMenuItemCount() <= GOSSIP_HARD_LIMIT);

        player->PlayerTalkClass->SendGossipMenu(DEFAULT_GOSSIP_MESSAGE, player->GetGUID());

        // 标题信息用聊天框补充（Gossip 标题需要 DB 文本，用聊天框更灵活）
        handler->PSendSysMessage("|cffffcc00「%s」共 %u 项  —  第 %u/%u 页|r",
            ss.keyword.c_str(), total, ss.page + 1, totalPages);
    }

    // 开启一个选择会话
    inline void StartPicker(Player* player, ChatHandler* handler, PickerType type,
                            std::vector<uint32> results, uint32 amount,
                            std::string const& keyword)
    {
        uint32 accId = player->GetSession()->GetAccountId();
        BatchSession& ss = s_sessions[accId];
        ss.type    = type;
        ss.results = std::move(results);
        ss.page    = 0;
        ss.amount  = amount;
        ss.keyword = keyword;

        // 按推荐度排序
        if (type == PICKER_ITEM)
            std::sort(ss.results.begin(), ss.results.end(),
                [](uint32 a, uint32 b) { return ScoreItem(a) > ScoreItem(b); });
        else if (type == PICKER_CREATURE)
            std::sort(ss.results.begin(), ss.results.end(),
                [](uint32 a, uint32 b) { return ScoreCreature(a) > ScoreCreature(b); });

        SendPickerMenu(player, handler);
    }

} // namespace SmartAdd

using namespace SmartAdd;

// ============================================================================
//  Gossip 点击回调
// ============================================================================
class smartadd_gossip : public PlayerScript
{
public:
    smartadd_gossip() : PlayerScript("smartadd_gossip") { }

    void OnGossipSelect(Player* player, uint32 menuId, uint32 sender, uint32 action) override
    {
        if (menuId != MENU_ID)
            return;

        ChatHandler handler(player->GetSession());
        uint32 accId = player->GetSession()->GetAccountId();
        auto it = s_sessions.find(accId);
        if (it == s_sessions.end())
        {
            player->PlayerTalkClass->SendCloseGossip();
            return;
        }
        BatchSession& ss = it->second;

        // —— 导航 ——
        if (sender == SENDER_NAV)
        {
            switch (action)
            {
                case NAV_PREV:
                    if (ss.HasPrev())
                        --ss.page;
                    SendPickerMenu(player, &handler);
                    return;
                case NAV_NEXT:
                    if (ss.HasNext())
                        ++ss.page;
                    SendPickerMenu(player, &handler);
                    return;
                case NAV_CANCEL:
                default:
                    ss.Clear();
                    player->PlayerTalkClass->SendCloseGossip();
                    handler.SendSysMessage("|cff888888已取消。|r");
                    return;
            }
        }

        // —— 选中条目 ——
        if (sender == SENDER_PICK)
        {
            uint32 id = action;
            player->PlayerTalkClass->SendCloseGossip();

            if (ss.type == PICKER_ITEM)
            {
                if (GiveItem(&handler, player, id, ss.amount))
                {
                    ItemTemplate const* p = sObjectMgr->GetItemTemplate(id);
                    handler.PSendSysMessage("|cff00ff00已添加|r %s x%u (ID:%u)",
                        p ? p->GetName(handler.GetSessionDbLocaleIndex()).c_str() : "",
                        ss.amount, id);
                    s_lastItem[accId] = id;
                }
            }
            else if (ss.type == PICKER_CREATURE)
            {
                SpawnOne(player, id, ss.amount);
                handler.PSendSysMessage("|cff00ff00已召唤|r %s x%u (ID:%u)",
                    GetCreatureName(&handler, id).c_str(), ss.amount, id);
                s_lastCreature[accId] = id;
            }

            ss.Clear();
        }
    }
};

// ============================================================================
//  指令
// ============================================================================
class smartadd_commandscript : public CommandScript
{
public:
    smartadd_commandscript() : CommandScript("smartadd_commandscript") { }

    std::vector<ChatCommand> GetCommands() const override
    {
        static std::vector<ChatCommand> commandTable =
        {
            { "add",    rbac::RBAC_PERM_COMMAND_SMART_ADD,   false, &HandleAdd,        "" },
            { "add!",   rbac::RBAC_PERM_COMMAND_SMART_ADD,   false, &HandleAddForce,   "" },
            { "spawn",  rbac::RBAC_PERM_COMMAND_SMART_SPAWN, false, &HandleSpawn,      "" },
            { "spawn!", rbac::RBAC_PERM_COMMAND_SMART_SPAWN, false, &HandleSpawnForce, "" },
            { "clean",  rbac::RBAC_PERM_COMMAND_SMART_SPAWN, false, &HandleClean,      "" },
        };
        return commandTable;
    }

    // ---------------- .add ----------------
    static bool DoAdd(ChatHandler* handler, char const* args, bool forceAll)
    {
        if (!*args)
        {
            handler->SendSysMessage("用法: .add <物品名或ID> [数量]");
            handler->SendSysMessage("  .add 32837          按ID直接给");
            handler->SendSysMessage("  .add 战刃 5         多个结果时弹出可点击菜单");
            handler->SendSysMessage("  .add 火焰, 霜之     逗号批量");
            handler->SendSysMessage("  .add! 战刃          全部匹配项都给");
            handler->SendSysMessage("  .add last           重复上次");
            handler->SetSentErrorMessage(true);
            return false;
        }

        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        Player* target = handler->getSelectedPlayerOrSelf();
        if (!target)
        {
            handler->SendSysMessage(LANG_NO_CHAR_SELECTED);
            handler->SetSentErrorMessage(true);
            return false;
        }
        uint32 accId = handler->GetSession() ? handler->GetSession()->GetAccountId() : 0;
        std::string input = args;

        // last
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

        // 批量
        if (input.find(',') != std::string::npos)
        {
            std::vector<std::string> names = SplitByComma(input);
            uint32 ok = 0, skip = 0;
            handler->PSendSysMessage("|cffffcc00批量添加 %u 项...|r", uint32(names.size()));

            for (std::string const& nm : names)
            {
                if (IsAllDigits(nm))
                {
                    uint32 id = uint32(atoi(nm.c_str()));
                    ItemTemplate const* p = sObjectMgr->GetItemTemplate(id);
                    if (p && GiveItem(handler, target, id, 1))
                    {
                        handler->PSendSysMessage("  |cff00ff00[OK]|r %s (ID:%u)",
                            p->GetName(handler->GetSessionDbLocaleIndex()).c_str(), id);
                        ++ok;
                        s_lastItem[accId] = id;
                    }
                    else { handler->PSendSysMessage("  |cffff0000[ID无效]|r %s", nm.c_str()); ++skip; }
                    continue;
                }

                std::vector<uint32> found;
                SearchItems(handler, nm, found);
                if (found.empty())
                {
                    handler->PSendSysMessage("  |cffff0000[未找到]|r %s", nm.c_str());
                    ++skip;
                    continue;
                }
                if (found.size() == 1 || AllItemsIdentical(handler, found) || forceAll)
                {
                    uint32 loops = forceAll ? uint32(found.size()) : 1u;
                    for (uint32 k = 0; k < loops; ++k)
                        if (GiveItem(handler, target, found[k], 1))
                        {
                            ItemTemplate const* p = sObjectMgr->GetItemTemplate(found[k]);
                            handler->PSendSysMessage("  |cff00ff00[OK]|r %s (ID:%u)",
                                p ? p->GetName(handler->GetSessionDbLocaleIndex()).c_str() : "", found[k]);
                            ++ok;
                            s_lastItem[accId] = found[k];
                        }
                }
                else
                {
                    std::sort(found.begin(), found.end(),
                        [](uint32 a, uint32 b) { return ScoreItem(a) > ScoreItem(b); });
                    if (GiveItem(handler, target, found[0], 1))
                    {
                        ItemTemplate const* p = sObjectMgr->GetItemTemplate(found[0]);
                        handler->PSendSysMessage("  |cffffd700[荐]|r %s (ID:%u) |cff888888另有%u个相似|r",
                            p ? p->GetName(handler->GetSessionDbLocaleIndex()).c_str() : "",
                            found[0], uint32(found.size() - 1));
                        ++ok;
                        s_lastItem[accId] = found[0];
                    }
                }
            }
            handler->PSendSysMessage("|cffffcc00完成：成功 %u，跳过 %u|r", ok, skip);
            return true;
        }

        // 解析数量
        std::string namePart = input;
        uint32 count = 1;
        size_t sp = input.find_last_of(' ');
        if (sp != std::string::npos)
        {
            std::string tail = input.substr(sp + 1);
            if (IsAllDigits(tail))
            {
                count = uint32(atoi(tail.c_str()));
                if (!count) count = 1;
                namePart = input.substr(0, sp);
            }
        }

        // 纯数字 = ID
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

        // 唯一 或 完全等价 -> 直接给
        if (found.size() == 1 || AllItemsIdentical(handler, found))
        {
            uint32 id = found[0];
            if (GiveItem(handler, target, id, count))
            {
                ItemTemplate const* p = sObjectMgr->GetItemTemplate(id);
                if (found.size() > 1)
                    handler->PSendSysMessage("|cff00ff00已添加|r %s x%u (ID:%u) |cff888888[%u个完全相同]|r",
                        p->GetName(handler->GetSessionDbLocaleIndex()).c_str(), count, id, uint32(found.size()));
                else
                    handler->PSendSysMessage("|cff00ff00已添加|r %s x%u (ID:%u)",
                        p->GetName(handler->GetSessionDbLocaleIndex()).c_str(), count, id);
                s_lastItem[accId] = id;
            }
            return true;
        }

        // 强制全给
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

        // 多个有差异 -> 弹出可点击菜单
        if (!player)
        {
            handler->SendSysMessage("控制台无法使用交互菜单，请用 .add <ID>");
            handler->SetSentErrorMessage(true);
            return false;
        }
        StartPicker(player, handler, PICKER_ITEM, found, count, namePart);
        return true;
    }

    static bool HandleAdd(ChatHandler* h, char const* a)      { return DoAdd(h, a, false); }
    static bool HandleAddForce(ChatHandler* h, char const* a) { return DoAdd(h, a, true); }

    // ---------------- .spawn ----------------
    static bool DoSpawn(ChatHandler* handler, char const* args, bool forceAll)
    {
        if (!*args)
        {
            handler->SendSysMessage("用法: .spawn <生物名或ID> [x数量]");
            handler->SendSysMessage("  .spawn 299          按ID召唤");
            handler->SendSysMessage("  .spawn 豺狼人 x5    多个结果时弹出可点击菜单");
            handler->SendSysMessage("  .spawn 熊, 狼       逗号批量");
            handler->SendSysMessage("  .spawn! 豺狼人      全部匹配项各刷一只");
            handler->SendSysMessage("  .clean [半径]       清理自己刷的");
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

        if (input == "last")
        {
            auto it = s_lastCreature.find(accId);
            if (it == s_lastCreature.end())
            {
                handler->SendSysMessage("没有上次记录。");
                handler->SetSentErrorMessage(true);
                return false;
            }
            SpawnOne(player, it->second, 1);
            handler->PSendSysMessage("|cff00ff00已重复召唤|r %s (ID:%u)",
                GetCreatureName(handler, it->second).c_str(), it->second);
            return true;
        }

        if (input.find(',') != std::string::npos)
        {
            std::vector<std::string> names = SplitByComma(input);
            uint32 ok = 0, skip = 0;
            handler->PSendSysMessage("|cffffcc00批量召唤 %u 项...|r", uint32(names.size()));

            for (std::string const& nm : names)
            {
                if (IsAllDigits(nm))
                {
                    uint32 id = uint32(atoi(nm.c_str()));
                    if (sObjectMgr->GetCreatureTemplate(id))
                    {
                        SpawnOne(player, id, 1);
                        handler->PSendSysMessage("  |cff00ff00[OK]|r %s (ID:%u)",
                            GetCreatureName(handler, id).c_str(), id);
                        ++ok;
                        s_lastCreature[accId] = id;
                    }
                    else { handler->PSendSysMessage("  |cffff0000[ID无效]|r %s", nm.c_str()); ++skip; }
                    continue;
                }

                std::vector<uint32> found;
                SearchCreatures(handler, nm, found);
                if (found.empty())
                {
                    handler->PSendSysMessage("  |cffff0000[未找到]|r %s", nm.c_str());
                    ++skip;
                    continue;
                }
                if (found.size() == 1 || AllCreaturesIdentical(handler, found) || forceAll)
                {
                    uint32 loops = forceAll ? uint32(found.size()) : 1u;
                    for (uint32 k = 0; k < loops; ++k)
                    {
                        SpawnOne(player, found[k], 1);
                        handler->PSendSysMessage("  |cff00ff00[OK]|r %s (ID:%u)",
                            GetCreatureName(handler, found[k]).c_str(), found[k]);
                        ++ok;
                        s_lastCreature[accId] = found[k];
                    }
                }
                else
                {
                    std::sort(found.begin(), found.end(),
                        [](uint32 a, uint32 b) { return ScoreCreature(a) > ScoreCreature(b); });
                    SpawnOne(player, found[0], 1);
                    handler->PSendSysMessage("  |cffffd700[荐]|r %s (ID:%u) |cff888888另有%u个相似|r",
                        GetCreatureName(handler, found[0]).c_str(), found[0], uint32(found.size() - 1));
                    ++ok;
                    s_lastCreature[accId] = found[0];
                }
            }
            handler->PSendSysMessage("|cffffcc00完成：成功 %u，跳过 %u|r", ok, skip);
            return true;
        }

        std::string namePart = input;
        uint32 amount = 1;
        size_t sp = input.find_last_of(' ');
        if (sp != std::string::npos)
        {
            std::string tail = input.substr(sp + 1);
            if (tail.size() >= 2 && (tail[0] == 'x' || tail[0] == 'X'))
            {
                std::string num = tail.substr(1);
                if (IsAllDigits(num))
                {
                    amount = uint32(atoi(num.c_str()));
                    if (!amount)     amount = 1;
                    if (amount > 50) amount = 50;
                    namePart = input.substr(0, sp);
                }
            }
        }

        if (IsAllDigits(namePart))
        {
            uint32 id = uint32(atoi(namePart.c_str()));
            if (!sObjectMgr->GetCreatureTemplate(id))
            {
                handler->PSendSysMessage("|cffff0000生物 ID %u 不存在。|r", id);
                handler->SetSentErrorMessage(true);
                return false;
            }
            SpawnOne(player, id, amount);
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

        if (found.size() == 1 || AllCreaturesIdentical(handler, found))
        {
            uint32 id = found[0];
            SpawnOne(player, id, amount);
            if (found.size() > 1)
                handler->PSendSysMessage("|cff00ff00已召唤|r %s x%u (ID:%u) |cff888888[%u个完全相同]|r",
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
                SpawnOne(player, id, amount);
                s_lastCreature[accId] = id;
            }
            handler->SendSysMessage("|cff00ff00完成|r");
            return true;
        }

        StartPicker(player, handler, PICKER_CREATURE, found, amount, namePart);
        return true;
    }

    static bool HandleSpawn(ChatHandler* h, char const* a)      { return DoSpawn(h, a, false); }
    static bool HandleSpawnForce(ChatHandler* h, char const* a) { return DoSpawn(h, a, true); }

    // ---------------- .clean ----------------
    static bool HandleClean(ChatHandler* handler, char const* args)
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
    new smartadd_gossip();
}
