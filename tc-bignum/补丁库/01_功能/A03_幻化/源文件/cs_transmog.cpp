/*
 * ============================================================================
 *  幻化系统指令 —— cs_transmog.cpp
 * ============================================================================
 *
 *  底层能力在 src/server/game/Custom/CustomTransmog.h/.cpp
 *  本文件只做「指令解析 + 玩家交互」。
 *
 *  ── 指令总览 ──────────────────────────────────────────────────────────
 *   .transmog                          显示帮助
 *   .transmog slot <槽位> <物品ID>     单槽幻化（槽位支持中文名）
 *   .transmog copy <物品ID>            按物品自身部位自动幻化到对应槽
 *   .transmog remove <槽位>            移除单槽
 *   .transmog clear                    清空全部幻化
 *   .transmog list                     查看当前幻化
 *   .transmog slots                    查看所有可幻化槽位名
 *   .transmog save <名字>              保存当前为方案
 *   .transmog load <名字>              载入方案
 *   .transmog del <名字>               删除方案
 *   .transmog sets                     列出所有方案
 *
 *  放置：D:\TrinityCore\src\server\scripts\Commands\cs_transmog.cpp
 *  RBAC：71009
 *  新增源文件必须重跑 CMake
 * ============================================================================
 */

#include "ScriptMgr.h"
#include "Chat.h"
#include "CustomTransmog.h"
#include "DBCStores.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "RBAC.h"
#include "World.h"
#include "WorldSession.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    // 品质对应的颜色码（客户端标准色）
    char const* QualityColor(uint32 quality)
    {
        switch (quality)
        {
            case 0:  return "ff9d9d9d";   // 灰
            case 1:  return "ffffffff";   // 白
            case 2:  return "ff1eff00";   // 绿
            case 3:  return "ff0070dd";   // 蓝
            case 4:  return "ffa335ee";   // 紫
            case 5:  return "ffff8000";   // 橙
            case 6:  return "ffe6cc80";   // 神器
            default: return "ffffffff";
        }
    }

    // 生成可点击的物品链接
    std::string ItemLink(ItemTemplate const* proto, LocaleConstant locale)
    {
        if (!proto)
            return "|cffff0000[未知物品]|r";

        std::string name = proto->Name1;

        // 有本地化就用本地化名字
        if (ItemLocale const* il = sObjectMgr->GetItemLocale(proto->ItemId))
            ObjectMgr::GetLocaleString(il->Name, locale, name);

        std::ostringstream ss;
        ss << "|c" << QualityColor(proto->Quality)
           << "|Hitem:" << proto->ItemId << ":0:0:0:0:0:0:0:0|h["
           << name << "]|h|r";
        return ss.str();
    }

    // InventoryType -> 装备槽位。返回 TRANSMOG_MAX_SLOT 表示不可幻化
    // 参考 ItemTemplate.h:270-300 的 INVTYPE_* 枚举
    uint8 SlotFromInventoryType(uint32 invType)
    {
        switch (invType)
        {
            case INVTYPE_HEAD:            return 0;
            case INVTYPE_SHOULDERS:       return 2;
            case INVTYPE_BODY:            return 3;
            case INVTYPE_CHEST:           return 4;
            case INVTYPE_ROBE:            return 4;
            case INVTYPE_WAIST:           return 5;
            case INVTYPE_LEGS:            return 6;
            case INVTYPE_FEET:            return 7;
            case INVTYPE_WRISTS:          return 8;
            case INVTYPE_HANDS:           return 9;
            case INVTYPE_CLOAK:           return 14;
            case INVTYPE_WEAPON:          return 15;
            case INVTYPE_2HWEAPON:        return 15;
            case INVTYPE_WEAPONMAINHAND:  return 15;
            case INVTYPE_WEAPONOFFHAND:   return 16;
            case INVTYPE_SHIELD:          return 16;
            case INVTYPE_HOLDABLE:        return 16;
            case INVTYPE_RANGED:          return 17;
            case INVTYPE_RANGEDRIGHT:     return 17;   // 3.3.5 的魔杖是这个，没有 INVTYPE_WAND
            case INVTYPE_THROWN:          return 17;
            case INVTYPE_TABARD:          return 18;
            default:                      return TRANSMOG_MAX_SLOT;
        }
    }

    // 拆词
    std::vector<std::string> Tokenize(char const* args)
    {
        std::vector<std::string> tok;
        if (!args)
            return tok;

        std::string a = args;
        size_t pos = 0;
        while (pos < a.size())
        {
            size_t sp = a.find(' ', pos);
            if (sp == std::string::npos)
                sp = a.size();
            if (sp > pos)
                tok.push_back(a.substr(pos, sp - pos));
            pos = sp + 1;
        }
        return tok;
    }

    // 把 tok[from] 之后的词拼回一个字符串（方案名可能带空格）
    std::string JoinFrom(std::vector<std::string> const& tok, size_t from)
    {
        std::string out;
        for (size_t i = from; i < tok.size(); ++i)
        {
            if (!out.empty())
                out += " ";
            out += tok[i];
        }
        return out;
    }

    bool IsNumber(std::string const& s)
    {
        return !s.empty() && s.find_first_not_of("0123456789") == std::string::npos;
    }

    // ---- 以下为「外观浏览器」用到的辅助 ----

    std::string ToLowerStr(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    }

    // 不可见槽位：颈/戒指/饰品在 3.3.5 客户端没有模型
    bool IsInvisibleSlotLocal(uint8 slot)
    {
        return slot == 1 || slot == 10 || slot == 11 || slot == 12 || slot == 13;
    }

    // 中文/英文品质名 -> ItemQualities（SharedDefines.h:375）
    // 返回 -1 表示不是品质词
    int32 QualityFromName(std::string const& s)
    {
        std::string t = ToLowerStr(s);
        if (t == "灰" || t == "垃圾" || t == "poor" || t == "grey" || t == "gray") return 0;
        if (t == "白" || t == "普通" || t == "common" || t == "white")             return 1;
        if (t == "绿" || t == "优秀" || t == "uncommon" || t == "green")           return 2;
        if (t == "蓝" || t == "精良" || t == "rare" || t == "blue")                return 3;
        if (t == "紫" || t == "史诗" || t == "epic" || t == "purple")              return 4;
        if (t == "橙" || t == "传说" || t == "legendary" || t == "orange")         return 5;
        if (t == "神器" || t == "artifact")                                        return 6;
        return -1;
    }

    char const* QualityName(uint32 q)
    {
        switch (q)
        {
            case 0:  return "灰";
            case 1:  return "白";
            case 2:  return "绿";
            case 3:  return "蓝";
            case 4:  return "紫";
            case 5:  return "橙";
            case 6:  return "神器";
            default: return "";
        }
    }
}

class transmog_commandscript : public CommandScript
{
public:
    transmog_commandscript() : CommandScript("transmog_commandscript") { }

    // 本仓库用旧版 ChatCommand 框架，与 cs_gearset / cs_spellclean 保持一致
    std::vector<ChatCommand> GetCommands() const override
    {
        static std::vector<ChatCommand> commandTable =
        {
            { "transmog", rbac::RBAC_PERM_COMMAND_TRANSMOG, false, &HandleTransmogCommand, "" },
        };
        return commandTable;
    }

    static bool HandleTransmogCommand(ChatHandler* handler, char const* args)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        if (!sCustomTransmog->Enabled())
        {
            handler->PSendSysMessage("|cffff0000幻化系统已关闭。|r（conf: Transmog.Enable = 0）");
            return true;
        }

        std::vector<std::string> tok = Tokenize(args);

        if (tok.empty())
        {
            ShowHelp(handler);
            return true;
        }

        std::string const& sub = tok[0];

        if (sub == "slot")   return HandleSlot(handler, player, tok);
        if (sub == "copy")   return HandleCopy(handler, player, tok);
        if (sub == "remove" || sub == "rm")
                             return HandleRemove(handler, player, tok);
        if (sub == "clear")  return HandleClear(handler, player);
        if (sub == "list")   return HandleList(handler, player);
        if (sub == "slots")  return HandleSlots(handler);
        if (sub == "save")   return HandleSave(handler, player, tok);
        if (sub == "load")   return HandleLoad(handler, player, tok);
        if (sub == "del" || sub == "delete")
                             return HandleDel(handler, player, tok);
        if (sub == "sets")   return HandleSets(handler, player);
        if (sub == "find")   return HandleFind(handler, player, tok);
        if (sub == "preview" || sub == "pv")
                             return HandlePreview(handler, player, tok);
        if (sub == "restore")return HandleRestore(handler, player);

        ShowHelp(handler);
        return true;
    }

private:

    // ==================================================================
    //  帮助
    // ==================================================================
    static void ShowHelp(ChatHandler* handler)
    {
        handler->PSendSysMessage("|cff00ff00===== 幻化系统 =====|r");
        handler->PSendSysMessage("|cffffff00.transmog slot <槽位> <物品ID>|r  幻化指定槽位");
        handler->PSendSysMessage("|cffffff00.transmog copy <物品ID>|r         自动识别部位并幻化");
        handler->PSendSysMessage("|cffffff00.transmog remove <槽位>|r         移除单个槽位");
        handler->PSendSysMessage("|cffffff00.transmog clear|r                 清空全部幻化");
        handler->PSendSysMessage("|cffffff00.transmog list|r                  查看当前幻化");
        handler->PSendSysMessage("|cffffff00.transmog slots|r                 查看可用槽位名");
        handler->PSendSysMessage(" ");
        handler->PSendSysMessage("|cff00ff00--- 外观方案 ---|r");
        handler->PSendSysMessage("|cffffff00.transmog save <名字>|r           保存当前为方案");
        handler->PSendSysMessage("|cffffff00.transmog load <名字>|r           载入方案");
        handler->PSendSysMessage("|cffffff00.transmog del <名字>|r            删除方案");
        handler->PSendSysMessage("|cffffff00.transmog sets|r                  列出所有方案");
        handler->PSendSysMessage(" ");
        handler->PSendSysMessage(" ");
        handler->PSendSysMessage("|cff00ff00--- 外观浏览器 ---|r");
        handler->PSendSysMessage("|cffffff00.transmog find <关键词> [品质] [页]|r  搜外观");
        handler->PSendSysMessage("|cffffff00.transmog preview <物品ID>|r        试穿，到时自动恢复");
        handler->PSendSysMessage("|cffffff00.transmog restore|r                 立即结束试穿");
        handler->PSendSysMessage(" ");
        handler->PSendSysMessage("槽位可用中文：|cff00ccff头盔 胸甲 主手 披风|r 等，也可用数字 0-18");
        handler->PSendSysMessage("例：|cffffff00.transmog slot 头盔 12640|r");

        if (sCustomTransmog->RequireItem())
            handler->PSendSysMessage("|cffff8000当前为收集模式：外观物品必须在你的背包里。|r");

        if (uint32 cost = sCustomTransmog->CostPerSlot())
            handler->PSendSysMessage("|cffff8000每次幻化需要 %u 铜。|r", cost);
    }

    // ==================================================================
    //  槽位列表
    // ==================================================================
    static bool HandleSlots(ChatHandler* handler)
    {
        handler->PSendSysMessage("|cff00ff00===== 可幻化槽位 =====|r");
        for (uint8 slot = 0; slot < TRANSMOG_MAX_SLOT; ++slot)
        {
            // 不可见的跳过
            std::string err;
            if (!sCustomTransmog->ValidateSlot(slot, 1, &err) && err.find("没有模型") != std::string::npos)
                continue;

            handler->PSendSysMessage("  |cff00ccff%-8s|r  编号 %u",
                CustomTransmogMgr::SlotName(slot), uint32(slot));
        }
        handler->PSendSysMessage("|cff888888（项链/戒指/饰品在 3.3.5 客户端不显示外观，已隐藏）|r");
        return true;
    }

    // ==================================================================
    //  单槽幻化
    // ==================================================================
    static bool HandleSlot(ChatHandler* handler, Player* player, std::vector<std::string> const& tok)
    {
        if (tok.size() < 3)
        {
            handler->PSendSysMessage("用法：|cffffff00.transmog slot <槽位> <物品ID>|r");
            handler->PSendSysMessage("例：  |cffffff00.transmog slot 头盔 12640|r");
            handler->PSendSysMessage("槽位名用 |cffffff00.transmog slots|r 查看");
            return true;
        }

        uint8 slot = CustomTransmogMgr::SlotFromName(tok[1]);
        if (slot >= TRANSMOG_MAX_SLOT)
        {
            handler->PSendSysMessage("|cffff0000槽位「%s」无效。|r用 |cffffff00.transmog slots|r 查看可用槽位。",
                tok[1].c_str());
            return true;
        }

        if (!IsNumber(tok[2]))
        {
            handler->PSendSysMessage("|cffff0000物品ID 必须是数字。|r");
            return true;
        }

        uint32 fakeEntry = uint32(atoi(tok[2].c_str()));

        return ApplyTransmog(handler, player, slot, fakeEntry);
    }

    // ==================================================================
    //  自动识别部位
    // ==================================================================
    static bool HandleCopy(ChatHandler* handler, Player* player, std::vector<std::string> const& tok)
    {
        if (tok.size() < 2 || !IsNumber(tok[1]))
        {
            handler->PSendSysMessage("用法：|cffffff00.transmog copy <物品ID>|r");
            handler->PSendSysMessage("自动识别该物品的部位，幻化到对应槽位。");
            return true;
        }

        uint32 fakeEntry = uint32(atoi(tok[1].c_str()));

        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(fakeEntry);
        if (!proto)
        {
            handler->PSendSysMessage("|cffff0000物品 %u 不存在。|r", fakeEntry);
            return true;
        }

        uint8 slot = SlotFromInventoryType(proto->InventoryType);
        if (slot >= TRANSMOG_MAX_SLOT)
        {
            handler->PSendSysMessage("|cffff0000该物品不是可穿戴装备，无法幻化。|r（InventoryType = %u）",
                proto->InventoryType);
            return true;
        }

        handler->PSendSysMessage("识别为：|cff00ccff%s|r", CustomTransmogMgr::SlotName(slot));
        return ApplyTransmog(handler, player, slot, fakeEntry);
    }

    // ==================================================================
    //  幻化核心（slot / copy 共用）
    // ==================================================================
    static bool ApplyTransmog(ChatHandler* handler, Player* player, uint8 slot, uint32 fakeEntry)
    {
        LocaleConstant locale = handler->GetSessionDbcLocale();

        // 校验外观合法性（模块层：物品存在 + 有模型 + 槽位可见）
        std::string err;
        if (!sCustomTransmog->ValidateSlot(slot, fakeEntry, &err))
        {
            handler->PSendSysMessage("|cffff0000无法幻化：%s|r", err.c_str());
            return true;
        }

        // 该槽位必须真的穿着装备 —— 空槽幻化没有意义（客户端不显示）
        Item* equipped = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!equipped)
        {
            handler->PSendSysMessage("|cffff0000你的「%s」槽位没有装备，先穿上再幻化。|r",
                CustomTransmogMgr::SlotName(slot));
            return true;
        }

        ItemTemplate const* fakeProto = sObjectMgr->GetItemTemplate(fakeEntry);

        // 收集模式：外观物品必须在背包里
        if (sCustomTransmog->RequireItem())
        {
            if (!player->HasItemCount(fakeEntry, 1, true))
            {
                handler->PSendSysMessage("|cffff0000收集模式：你的背包里没有 %s|r",
                    ItemLink(fakeProto, locale).c_str());
                return true;
            }
        }

        // 扣费
        uint32 cost = sCustomTransmog->CostPerSlot();
        if (cost)
        {
            if (!player->HasEnoughMoney(cost))
            {
                handler->PSendSysMessage("|cffff0000金钱不足，需要 %u 铜。|r", cost);
                return true;
            }
            player->ModifyMoney(-int32(cost));
        }

        // 写入
        sCustomTransmog->SetFakeEntry(player->GetGUID().GetCounter(), slot, fakeEntry);
        CustomTransmogMgr::RefreshPlayer(player);

        handler->PSendSysMessage("|cff00ff00[幻化]|r %s -> %s",
            CustomTransmogMgr::SlotName(slot),
            ItemLink(fakeProto, locale).c_str());

        if (cost)
            handler->PSendSysMessage("|cff888888花费 %u 铜。|r", cost);

        return true;
    }

    // ==================================================================
    //  移除单槽
    // ==================================================================
    static bool HandleRemove(ChatHandler* handler, Player* player, std::vector<std::string> const& tok)
    {
        if (tok.size() < 2)
        {
            handler->PSendSysMessage("用法：|cffffff00.transmog remove <槽位>|r");
            return true;
        }

        uint8 slot = CustomTransmogMgr::SlotFromName(tok[1]);
        if (slot >= TRANSMOG_MAX_SLOT)
        {
            handler->PSendSysMessage("|cffff0000槽位「%s」无效。|r", tok[1].c_str());
            return true;
        }

        if (!sCustomTransmog->GetFakeEntry(player->GetGUID().GetCounter(), slot))
        {
            handler->PSendSysMessage("|cffff8000「%s」本来就没有幻化。|r",
                CustomTransmogMgr::SlotName(slot));
            return true;
        }

        sCustomTransmog->RemoveSlot(player->GetGUID().GetCounter(), slot);
        CustomTransmogMgr::RefreshPlayer(player);

        handler->PSendSysMessage("|cff00ff00[幻化]|r 已移除「%s」的幻化。",
            CustomTransmogMgr::SlotName(slot));
        return true;
    }

    // ==================================================================
    //  清空
    // ==================================================================
    static bool HandleClear(ChatHandler* handler, Player* player)
    {
        if (!sCustomTransmog->HasAny(player->GetGUID().GetCounter()))
        {
            handler->PSendSysMessage("|cffff8000你当前没有任何幻化。|r");
            return true;
        }

        sCustomTransmog->ClearAll(player->GetGUID().GetCounter());
        CustomTransmogMgr::RefreshPlayer(player);

        handler->PSendSysMessage("|cff00ff00[幻化]|r 已清空全部幻化，恢复真实装备外观。");
        return true;
    }

    // ==================================================================
    //  当前幻化列表
    // ==================================================================
    static bool HandleList(ChatHandler* handler, Player* player)
    {
        LocaleConstant locale = handler->GetSessionDbcLocale();
        ObjectGuid::LowType guidLow = player->GetGUID().GetCounter();

        if (!sCustomTransmog->HasAny(guidLow))
        {
            handler->PSendSysMessage("|cffff8000你当前没有任何幻化。|r");
            handler->PSendSysMessage("用 |cffffff00.transmog slot <槽位> <物品ID>|r 开始。");
            return true;
        }

        handler->PSendSysMessage("|cff00ff00===== 当前幻化 =====|r");

        uint32 count = 0;
        for (uint8 slot = 0; slot < TRANSMOG_MAX_SLOT; ++slot)
        {
            uint32 fakeEntry = sCustomTransmog->GetFakeEntry(guidLow, slot);
            if (!fakeEntry)
                continue;

            ItemTemplate const* fakeProto = sObjectMgr->GetItemTemplate(fakeEntry);
            Item* equipped = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            ItemTemplate const* realProto = equipped ? equipped->GetTemplate() : nullptr;

            handler->PSendSysMessage("|cff00ccff%-8s|r %s",
                CustomTransmogMgr::SlotName(slot),
                ItemLink(fakeProto, locale).c_str());

            if (realProto)
                handler->PSendSysMessage("           |cff888888实际：%s|r",
                    ItemLink(realProto, locale).c_str());
            else
                handler->PSendSysMessage("           |cffff8000该槽位当前没穿装备，幻化不显示|r");

            ++count;
        }

        handler->PSendSysMessage("|cff888888共 %u 个槽位。|r", count);
        return true;
    }

    // ==================================================================
    //  方案：保存
    // ==================================================================
    static bool HandleSave(ChatHandler* handler, Player* player, std::vector<std::string> const& tok)
    {
        if (tok.size() < 2)
        {
            handler->PSendSysMessage("用法：|cffffff00.transmog save <名字>|r");
            handler->PSendSysMessage("例：  |cffffff00.transmog save 战斗套|r");
            return true;
        }

        std::string name = JoinFrom(tok, 1);

        if (!sCustomTransmog->HasAny(player->GetGUID().GetCounter()))
        {
            handler->PSendSysMessage("|cffff0000你当前没有任何幻化，没什么可保存的。|r");
            return true;
        }

        if (!sCustomTransmog->SaveSet(player->GetGUID().GetCounter(), name))
        {
            handler->PSendSysMessage("|cffff0000保存失败。|r可能原因：");
            handler->PSendSysMessage("  · 名字含非法字符（引号、分号、反斜杠等）");
            handler->PSendSysMessage("  · 名字超过 32 字符");
            handler->PSendSysMessage("  · 方案数量已达上限");
            return true;
        }

        handler->PSendSysMessage("|cff00ff00[幻化]|r 已保存方案「|cffffff00%s|r」。", name.c_str());
        handler->PSendSysMessage("用 |cffffff00.transmog load %s|r 随时切回。", name.c_str());
        return true;
    }

    // ==================================================================
    //  方案：载入
    // ==================================================================
    static bool HandleLoad(ChatHandler* handler, Player* player, std::vector<std::string> const& tok)
    {
        if (tok.size() < 2)
        {
            handler->PSendSysMessage("用法：|cffffff00.transmog load <名字>|r");
            return true;
        }

        std::string name = JoinFrom(tok, 1);

        if (!sCustomTransmog->LoadSet(player->GetGUID().GetCounter(), name))
        {
            handler->PSendSysMessage("|cffff0000找不到方案「%s」。|r", name.c_str());
            handler->PSendSysMessage("用 |cffffff00.transmog sets|r 查看已保存的方案。");
            return true;
        }

        CustomTransmogMgr::RefreshPlayer(player);

        handler->PSendSysMessage("|cff00ff00[幻化]|r 已载入方案「|cffffff00%s|r」。", name.c_str());
        return true;
    }

    // ==================================================================
    //  方案：删除
    // ==================================================================
    static bool HandleDel(ChatHandler* handler, Player* player, std::vector<std::string> const& tok)
    {
        if (tok.size() < 2)
        {
            handler->PSendSysMessage("用法：|cffffff00.transmog del <名字>|r");
            return true;
        }

        std::string name = JoinFrom(tok, 1);

        if (!sCustomTransmog->DeleteSet(player->GetGUID().GetCounter(), name))
        {
            handler->PSendSysMessage("|cffff0000找不到方案「%s」。|r", name.c_str());
            return true;
        }

        handler->PSendSysMessage("|cff00ff00[幻化]|r 已删除方案「%s」。", name.c_str());
        return true;
    }

    // ==================================================================
    //  方案：列表
    // ==================================================================
    static bool HandleSets(ChatHandler* handler, Player* player)
    {
        std::vector<std::string> sets = sCustomTransmog->ListSets(player->GetGUID().GetCounter());

        if (sets.empty())
        {
            handler->PSendSysMessage("|cffff8000你还没有保存任何方案。|r");
            handler->PSendSysMessage("用 |cffffff00.transmog save <名字>|r 保存当前幻化。");
            return true;
        }

        handler->PSendSysMessage("|cff00ff00===== 我的外观方案 =====|r");
        for (std::string const& name : sets)
            handler->PSendSysMessage("  |cffffff00%s|r", name.c_str());

        handler->PSendSysMessage("|cff888888共 %u 套。用 .transmog load <名字> 切换。|r",
            uint32(sets.size()));
        return true;
    }

    // ==================================================================
    //  搜索外观
    //  .transmog find <关键词> [品质] [页码]
    // ==================================================================
    static bool HandleFind(ChatHandler* handler, Player* /*player*/, std::vector<std::string> const& tok)
    {
        if (tok.size() < 2)
        {
            handler->PSendSysMessage("用法：|cffffff00.transmog find <关键词> [品质] [页码]|r");
            handler->PSendSysMessage(" ");
            handler->PSendSysMessage("例：|cffffff00.transmog find 头盔|r          搜所有名字含「头盔」的");
            handler->PSendSysMessage("例：|cffffff00.transmog find 剑 橙|r         只看橙色的剑");
            handler->PSendSysMessage("例：|cffffff00.transmog find 长袍 紫 2|r     紫色长袍第 2 页");
            handler->PSendSysMessage(" ");
            handler->PSendSysMessage("品质可填：|cff9d9d9d灰|r |cffffffff白|r |cff1eff00绿|r |cff0070dd蓝|r |cffa335ee紫|r |cffff8000橙|r");
            return true;
        }

        std::string keyword = tok[1];

        // 解析可选的品质与页码
        int32 wantQuality = -1;     // -1 = 不限
        uint32 page = 0;

        for (size_t i = 2; i < tok.size(); ++i)
        {
            std::string const& t = tok[i];
            if (IsNumber(t))
            {
                uint32 n = uint32(atoi(t.c_str()));
                if (n >= 1)
                    page = n - 1;   // 用户从 1 开始数
                continue;
            }
            int32 q = QualityFromName(t);
            if (q >= 0)
                wantQuality = q;
        }

        LocaleConstant locale = handler->GetSessionDbcLocale();

        // ---- 扫描 item_template ----
        struct Hit
        {
            uint32 entry;
            uint32 quality;
            uint32 ilvl;
            uint8  slot;
        };
        std::vector<Hit> hits;

        std::string kwLower = ToLowerStr(keyword);

        ItemTemplateContainer const& store = sObjectMgr->GetItemTemplateStore();
        for (auto const& kv : store)
        {
            ItemTemplate const& proto = kv.second;

            // 只要能穿戴、有模型的
            if (!proto.DisplayInfoID)
                continue;

            uint8 slot = SlotFromInventoryType(proto.InventoryType);
            if (slot >= TRANSMOG_MAX_SLOT)
                continue;

            // 不可见槽位（项链/戒指/饰品）没意义
            if (IsInvisibleSlotLocal(slot))
                continue;

            if (wantQuality >= 0 && int32(proto.Quality) != wantQuality)
                continue;

            // 名字匹配：本地化名优先，回退英文名
            std::string name = proto.Name1;
            if (ItemLocale const* il = sObjectMgr->GetItemLocale(proto.ItemId))
                ObjectMgr::GetLocaleString(il->Name, locale, name);

            if (ToLowerStr(name).find(kwLower) == std::string::npos)
                continue;

            Hit h;
            h.entry   = proto.ItemId;
            h.quality = proto.Quality;
            h.ilvl    = proto.ItemLevel;
            h.slot    = slot;
            hits.push_back(h);
        }

        if (hits.empty())
        {
            handler->PSendSysMessage("|cffff8000没找到含「%s」的可幻化外观。|r", keyword.c_str());
            handler->PSendSysMessage("提示：试试更短的词，或换个说法（如「头盔」->「盔」）");
            return true;
        }

        // 品质高的、装等高的排前面 —— 通常也是模型最好看的
        std::sort(hits.begin(), hits.end(), [](Hit const& a, Hit const& b)
        {
            if (a.quality != b.quality)
                return a.quality > b.quality;
            return a.ilvl > b.ilvl;
        });

        uint32 const perPage = 15;
        uint32 total  = uint32(hits.size());
        uint32 maxPg  = total ? ((total - 1) / perPage) : 0;
        if (page > maxPg)
            page = maxPg;

        uint32 begin = page * perPage;
        uint32 end   = std::min(begin + perPage, total);

        handler->PSendSysMessage("|cff00ff00===== 搜索「%s」 第 %u/%u 页，共 %u 件 =====|r",
            keyword.c_str(), page + 1, maxPg + 1, total);

        for (uint32 i = begin; i < end; ++i)
        {
            Hit const& h = hits[i];
            ItemTemplate const* proto = sObjectMgr->GetItemTemplate(h.entry);
            if (!proto)
                continue;

            handler->PSendSysMessage("%s |cff888888[%s]|r  ID:|cffffff00%u|r  装等%u",
                ItemLink(proto, locale).c_str(),
                CustomTransmogMgr::SlotName(h.slot),
                h.entry,
                h.ilvl);
        }

        handler->PSendSysMessage(" ");
        if (maxPg > 0)
            handler->PSendSysMessage("|cff888888翻页：.transmog find %s %s %u|r",
                keyword.c_str(),
                wantQuality >= 0 ? QualityName(uint32(wantQuality)) : "",
                page + 2 > maxPg + 1 ? 1 : page + 2);

        handler->PSendSysMessage("|cff888888试穿：|r|cffffff00.transmog preview <ID>|r"
                                 "   |cff888888确定：|r|cffffff00.transmog copy <ID>|r");
        return true;
    }

    // ==================================================================
    //  试穿预览
    //  .transmog preview <物品ID> [槽位]
    // ==================================================================
    static bool HandlePreview(ChatHandler* handler, Player* player, std::vector<std::string> const& tok)
    {
        if (tok.size() < 2 || !IsNumber(tok[1]))
        {
            handler->PSendSysMessage("用法：|cffffff00.transmog preview <物品ID> [槽位]|r");
            handler->PSendSysMessage("临时穿上看效果，%u 秒后自动恢复。",
                sCustomTransmog->PreviewSeconds());
            handler->PSendSysMessage("不想等可以用 |cffffff00.transmog restore|r 立即恢复。");
            return true;
        }

        uint32 fakeEntry = uint32(atoi(tok[1].c_str()));

        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(fakeEntry);
        if (!proto)
        {
            handler->PSendSysMessage("|cffff0000物品 %u 不存在。|r", fakeEntry);
            return true;
        }

        // 槽位：显式指定优先，否则按物品自身部位推断
        uint8 slot;
        if (tok.size() >= 3)
        {
            slot = CustomTransmogMgr::SlotFromName(tok[2]);
            if (slot >= TRANSMOG_MAX_SLOT)
            {
                handler->PSendSysMessage("|cffff0000槽位「%s」无效。|r", tok[2].c_str());
                return true;
            }
        }
        else
        {
            slot = SlotFromInventoryType(proto->InventoryType);
            if (slot >= TRANSMOG_MAX_SLOT)
            {
                handler->PSendSysMessage("|cffff0000该物品不是可穿戴装备。|r（InventoryType = %u）",
                    proto->InventoryType);
                return true;
            }
        }

        if (IsInvisibleSlotLocal(slot))
        {
            handler->PSendSysMessage("|cffff0000「%s」在 3.3.5 客户端不显示外观。|r",
                CustomTransmogMgr::SlotName(slot));
            return true;
        }

        // 该槽位得有装备，否则客户端不画
        Item* equipped = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!equipped)
        {
            handler->PSendSysMessage("|cffff0000你的「%s」槽位没有装备，先穿上再试穿外观。|r",
                CustomTransmogMgr::SlotName(slot));
            return true;
        }

        LocaleConstant locale = handler->GetSessionDbcLocale();

        /*
         * 直接改客户端可见字段，不碰幻化数据库。
         * Object.h:123 SetUInt32Value 是 public。
         * 字段布局见 Player::SetVisibleItemSlot（Player.cpp:12170）：
         *     PLAYER_VISIBLE_ITEM_1_ENTRYID + slot * 2
         *
         * 注意：这是纯视觉的，不写库、不进缓存。
         * 任何一次装备变动或重新登录都会被 SetVisibleItemSlot 覆盖回去，
         * 所以即使定时器没跑到，也不会留下脏状态。
         */
        player->SetUInt32Value(PLAYER_VISIBLE_ITEM_1_ENTRYID + (slot * 2), fakeEntry);

        uint32 seconds = sCustomTransmog->PreviewSeconds();

        handler->PSendSysMessage("|cff00ff00[试穿]|r %s -> %s",
            CustomTransmogMgr::SlotName(slot),
            ItemLink(proto, locale).c_str());
        handler->PSendSysMessage("|cff888888%u 秒后自动恢复。满意就用 |r|cffffff00.transmog slot %s %u|r",
            seconds, CustomTransmogMgr::SlotName(slot), fakeEntry);

        // 定时恢复：Object.h:620 的 m_Events 是 public
        ObjectGuid guid = player->GetGUID();
        uint8 slotCopy = slot;
        player->m_Events.AddEventAtOffset([guid, slotCopy]()
        {
            Player* p = ObjectAccessor::FindPlayer(guid);
            if (!p)
                return;
            // 用真实装备（或幻化）重刷这一格
            Item* it = p->GetItemByPos(INVENTORY_SLOT_BAG_0, slotCopy);
            p->SetVisibleItemSlot(slotCopy, it);
        }, Seconds(seconds));

        return true;
    }

    // ==================================================================
    //  立即结束试穿
    // ==================================================================
    static bool HandleRestore(ChatHandler* handler, Player* player)
    {
        CustomTransmogMgr::RefreshPlayer(player);
        handler->PSendSysMessage("|cff00ff00[试穿]|r 已恢复。");
        return true;
    }
};

/*
 * ============================================================================
 *  PlayerScript —— 角色删除时清理幻化数据
 * ============================================================================
 *  不清理的话，characters.custom_transmog 会残留孤儿记录；
 *  更麻烦的是 GUID 会被新角色复用，导致新号一登录就顶着别人的幻化。
 */
class transmog_playerscript : public PlayerScript
{
public:
    transmog_playerscript() : PlayerScript("transmog_playerscript") { }

    void OnDelete(ObjectGuid guid, uint32 /*accountId*/) override
    {
        sCustomTransmog->OnCharacterDeleted(guid.GetCounter());
    }
};

void AddSC_transmog_commandscript()
{
    new transmog_commandscript();
    new transmog_playerscript();
}
