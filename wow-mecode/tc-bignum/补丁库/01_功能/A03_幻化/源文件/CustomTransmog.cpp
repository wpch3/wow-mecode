/*
 * CustomTransmog.cpp - 幻化系统实现
 * 详见 CustomTransmog.h 头部说明
 */

#include "CustomTransmog.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Timer.h"
#include "World.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace
{
    // 槽位中文名（索引 = EQUIPMENT_SLOT_*，见 Player.h:556-576）
    char const* const g_slotNames[TRANSMOG_MAX_SLOT] =
    {
        "头部",     //  0 HEAD
        "颈部",     //  1 NECK      (不可见)
        "肩部",     //  2 SHOULDERS
        "衬衣",     //  3 BODY
        "胸甲",     //  4 CHEST
        "腰带",     //  5 WAIST
        "腿部",     //  6 LEGS
        "靴子",     //  7 FEET
        "护腕",     //  8 WRISTS
        "手套",     //  9 HANDS
        "戒指1",    // 10 FINGER1   (不可见)
        "戒指2",    // 11 FINGER2   (不可见)
        "饰品1",    // 12 TRINKET1  (不可见)
        "饰品2",    // 13 TRINKET2  (不可见)
        "披风",     // 14 BACK
        "主手",     // 15 MAINHAND
        "副手",     // 16 OFFHAND
        "远程",     // 17 RANGED
        "战袍"      // 18 TABARD
    };

    // 别名表：让 .transmog slot 头 / 头盔 / head 都能用
    struct SlotAlias { char const* alias; uint8 slot; };
    SlotAlias const g_slotAlias[] =
    {
        { "头",     0 }, { "头部",   0 }, { "头盔",   0 }, { "head",      0 },
        { "颈",     1 }, { "颈部",   1 }, { "项链",   1 }, { "neck",      1 },
        { "肩",     2 }, { "肩部",   2 }, { "护肩",   2 }, { "shoulders", 2 },
        { "衬衣",   3 }, { "衬衫",   3 }, { "body",   3 }, { "shirt",     3 },
        { "胸",     4 }, { "胸甲",   4 }, { "上衣",   4 }, { "chest",     4 },
        { "腰",     5 }, { "腰带",   5 }, { "waist",  5 }, { "belt",      5 },
        { "腿",     6 }, { "腿部",   6 }, { "护腿",   6 }, { "legs",      6 },
        { "脚",     7 }, { "靴子",   7 }, { "鞋",     7 }, { "feet",      7 },
        { "腕",     8 }, { "护腕",   8 }, { "wrists", 8 }, { "bracers",   8 },
        { "手",     9 }, { "手套",   9 }, { "hands",  9 }, { "gloves",    9 },
        { "戒指1", 10 }, { "戒指",  10 }, { "finger1", 10 },
        { "戒指2", 11 }, { "finger2", 11 },
        { "饰品1", 12 }, { "饰品",  12 }, { "trinket1", 12 },
        { "饰品2", 13 }, { "trinket2", 13 },
        { "背",    14 }, { "披风",  14 }, { "斗篷",  14 }, { "back",     14 }, { "cloak", 14 },
        { "主手",  15 }, { "武器",  15 }, { "mainhand", 15 }, { "mh",     15 },
        { "副手",  16 }, { "盾",    16 }, { "盾牌",  16 }, { "offhand",  16 }, { "oh",    16 },
        { "远程",  17 }, { "弓",    17 }, { "枪",    17 }, { "ranged",   17 },
        { "战袍",  18 }, { "徽章",  18 }, { "tabard",  18 }
    };

    // 不可见槽位：项链/戒指/饰品在 3.3.5 客户端上没有模型
    inline bool IsInvisibleSlot(uint8 slot)
    {
        return slot == 1 || slot == 10 || slot == 11 || slot == 12 || slot == 13;
    }

    std::string ToLower(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    }

    /*
     * 方案名合法性校验 —— 防 SQL 注入的第一道闸。
     * 拒绝引号、反斜杠、分号、注释符等危险字符。
     * 写库时还会再走一次 CharacterDatabase.EscapeString()（第二道闸）。
     */
    bool IsValidSetName(std::string const& name)
    {
        if (name.empty() || name.length() > 32)
            return false;

        for (char c : name)
        {
            if (c == '\'' || c == '"' || c == '`' || c == '\\' ||
                c == ';'  || c == '-'  || c == '#' || c == '\n' || c == '\r')
                return false;
        }
        return true;
    }
}

CustomTransmogMgr* CustomTransmogMgr::instance()
{
    static CustomTransmogMgr inst;
    return &inst;
}

char const* CustomTransmogMgr::SlotName(uint8 slot)
{
    if (slot >= TRANSMOG_MAX_SLOT)
        return "未知";
    return g_slotNames[slot];
}

uint8 CustomTransmogMgr::SlotFromName(std::string const& name)
{
    // 先试纯数字
    if (!name.empty() && name.find_first_not_of("0123456789") == std::string::npos)
    {
        uint32 n = uint32(atoi(name.c_str()));
        if (n < TRANSMOG_MAX_SLOT)
            return uint8(n);
        return TRANSMOG_MAX_SLOT;
    }

    std::string lower = ToLower(name);
    for (auto const& a : g_slotAlias)
    {
        if (lower == ToLower(a.alias))
            return a.slot;
    }
    return TRANSMOG_MAX_SLOT;
}

void CustomTransmogMgr::LoadConfig()
{
    _enabled          = sConfigMgr->GetBoolDefault("Transmog.Enable", true);
    _requireItem      = sConfigMgr->GetBoolDefault("Transmog.RequireItem", false);
    _costPerSlot      = uint32(std::max(0, sConfigMgr->GetIntDefault("Transmog.CostPerSlot", 0)));
    _allowWeaponCross = sConfigMgr->GetBoolDefault("Transmog.AllowWeaponCross", true);
    _maxSets          = uint32(std::clamp(sConfigMgr->GetIntDefault("Transmog.MaxSets", 10), 1, 50));
    _previewSeconds   = uint32(std::clamp(sConfigMgr->GetIntDefault("Transmog.PreviewSeconds", 15), 3, 120));
}

void CustomTransmogMgr::LoadFromDB()
{
    LoadConfig();

    _data.clear();
    _sets.clear();

    uint32 oldMSTime = getMSTime();

    // ---------- 当前幻化 ----------
    {
        QueryResult result = CharacterDatabase.Query(
            "SELECT guid, slot, fakeEntry FROM custom_transmog");

        uint32 count = 0;
        if (result)
        {
            do
            {
                Field* fields = result->Fetch();
                ObjectGuid::LowType guidLow = fields[0].GetUInt32();
                uint8  slot      = fields[1].GetUInt8();
                uint32 fakeEntry = fields[2].GetUInt32();

                if (slot >= TRANSMOG_MAX_SLOT)
                {
                    TC_LOG_ERROR("server.loading",
                        "custom_transmog: guid {} 的槽位 {} 越界，已跳过", guidLow, slot);
                    continue;
                }

                if (!fakeEntry)
                    continue;

                if (!sObjectMgr->GetItemTemplate(fakeEntry))
                {
                    TC_LOG_ERROR("server.loading",
                        "custom_transmog: guid {} 槽位 {} 的外观物品 {} 不存在，已跳过",
                        guidLow, slot, fakeEntry);
                    continue;
                }

                auto itr = _data.find(guidLow);
                if (itr == _data.end())
                {
                    SlotArray arr{};
                    arr.fill(0);
                    arr[slot] = fakeEntry;
                    _data[guidLow] = arr;
                }
                else
                    itr->second[slot] = fakeEntry;

                ++count;
            }
            while (result->NextRow());
        }

        TC_LOG_INFO("server.loading",
            ">> 载入 {} 条幻化记录（{} 名角色），耗时 {} ms",
            count, uint32(_data.size()), GetMSTimeDiffToNow(oldMSTime));
    }

    // ---------- 外观方案 ----------
    {
        QueryResult result = CharacterDatabase.Query(
            "SELECT guid, setName, slot, fakeEntry FROM custom_transmog_sets");

        uint32 count = 0;
        if (result)
        {
            do
            {
                Field* fields = result->Fetch();
                ObjectGuid::LowType guidLow = fields[0].GetUInt32();
                std::string setName = fields[1].GetString();
                uint8  slot      = fields[2].GetUInt8();
                uint32 fakeEntry = fields[3].GetUInt32();

                if (slot >= TRANSMOG_MAX_SLOT || setName.empty())
                    continue;

                auto& setMap = _sets[guidLow];
                auto itr = setMap.find(setName);
                if (itr == setMap.end())
                {
                    SlotArray arr{};
                    arr.fill(0);
                    arr[slot] = fakeEntry;
                    setMap[setName] = arr;
                }
                else
                    itr->second[slot] = fakeEntry;

                ++count;
            }
            while (result->NextRow());
        }

        if (count)
            TC_LOG_INFO("server.loading", ">> 载入 {} 条幻化方案记录", count);
    }
}

uint32 CustomTransmogMgr::GetFakeEntry(ObjectGuid::LowType guidLow, uint8 slot) const
{
    if (!_enabled || slot >= TRANSMOG_MAX_SLOT)
        return 0;

    auto itr = _data.find(guidLow);
    if (itr == _data.end())
        return 0;

    return itr->second[slot];
}

bool CustomTransmogMgr::HasAny(ObjectGuid::LowType guidLow) const
{
    return _data.find(guidLow) != _data.end();
}

void CustomTransmogMgr::SetFakeEntry(ObjectGuid::LowType guidLow, uint8 slot, uint32 fakeEntry)
{
    if (slot >= TRANSMOG_MAX_SLOT)
        return;

    if (!fakeEntry)
    {
        RemoveSlot(guidLow, slot);
        return;
    }

    auto itr = _data.find(guidLow);
    if (itr == _data.end())
    {
        SlotArray arr{};
        arr.fill(0);
        arr[slot] = fakeEntry;
        _data[guidLow] = arr;
    }
    else
        itr->second[slot] = fakeEntry;

    CharacterDatabase.PExecute(
        "REPLACE INTO custom_transmog (guid, slot, fakeEntry) VALUES ({}, {}, {})",
        guidLow, uint32(slot), fakeEntry);
}

void CustomTransmogMgr::RemoveSlot(ObjectGuid::LowType guidLow, uint8 slot)
{
    if (slot >= TRANSMOG_MAX_SLOT)
        return;

    auto itr = _data.find(guidLow);
    if (itr != _data.end())
    {
        itr->second[slot] = 0;

        // 全空则整条移除，省内存
        bool anyLeft = false;
        for (uint32 e : itr->second)
        {
            if (e)
            {
                anyLeft = true;
                break;
            }
        }
        if (!anyLeft)
            _data.erase(itr);
    }

    CharacterDatabase.PExecute(
        "DELETE FROM custom_transmog WHERE guid = {} AND slot = {}",
        guidLow, uint32(slot));
}

void CustomTransmogMgr::ClearAll(ObjectGuid::LowType guidLow)
{
    _data.erase(guidLow);
    CharacterDatabase.PExecute("DELETE FROM custom_transmog WHERE guid = {}", guidLow);
}

void CustomTransmogMgr::OnCharacterDeleted(ObjectGuid::LowType guidLow)
{
    _data.erase(guidLow);
    _sets.erase(guidLow);
    CharacterDatabase.PExecute("DELETE FROM custom_transmog WHERE guid = {}", guidLow);
    CharacterDatabase.PExecute("DELETE FROM custom_transmog_sets WHERE guid = {}", guidLow);
}

bool CustomTransmogMgr::SaveSet(ObjectGuid::LowType guidLow, std::string const& name)
{
    if (!IsValidSetName(name))
        return false;

    auto dataItr = _data.find(guidLow);
    if (dataItr == _data.end())
        return false;   // 当前没有任何幻化，没什么可存

    auto& setMap = _sets[guidLow];

    // 新方案才检查数量上限，覆盖已有方案不受限
    if (setMap.find(name) == setMap.end() && setMap.size() >= _maxSets)
        return false;

    setMap[name] = dataItr->second;

    // 写库前再转义一次（第二道闸）
    std::string safeName = name;
    CharacterDatabase.EscapeString(safeName);

    // 覆盖式写库
    CharacterDatabase.PExecute(
        "DELETE FROM custom_transmog_sets WHERE guid = {} AND setName = '{}'",
        guidLow, safeName);

    for (uint8 slot = 0; slot < TRANSMOG_MAX_SLOT; ++slot)
    {
        uint32 entry = dataItr->second[slot];
        if (!entry)
            continue;

        CharacterDatabase.PExecute(
            "INSERT INTO custom_transmog_sets (guid, setName, slot, fakeEntry) VALUES ({}, '{}', {}, {})",
            guidLow, safeName, uint32(slot), entry);
    }

    return true;
}

bool CustomTransmogMgr::LoadSet(ObjectGuid::LowType guidLow, std::string const& name)
{
    auto setItr = _sets.find(guidLow);
    if (setItr == _sets.end())
        return false;

    auto nameItr = setItr->second.find(name);
    if (nameItr == setItr->second.end())
        return false;

    SlotArray const& arr = nameItr->second;

    // 先清空当前，再逐槽写入
    ClearAll(guidLow);

    bool anySet = false;
    for (uint8 slot = 0; slot < TRANSMOG_MAX_SLOT; ++slot)
    {
        if (arr[slot])
        {
            SetFakeEntry(guidLow, slot, arr[slot]);
            anySet = true;
        }
    }

    return anySet;
}

bool CustomTransmogMgr::DeleteSet(ObjectGuid::LowType guidLow, std::string const& name)
{
    if (!IsValidSetName(name))
        return false;

    auto setItr = _sets.find(guidLow);
    if (setItr == _sets.end())
        return false;

    if (!setItr->second.erase(name))
        return false;

    if (setItr->second.empty())
        _sets.erase(setItr);

    std::string safeName = name;
    CharacterDatabase.EscapeString(safeName);

    CharacterDatabase.PExecute(
        "DELETE FROM custom_transmog_sets WHERE guid = {} AND setName = '{}'",
        guidLow, safeName);

    return true;
}

std::vector<std::string> CustomTransmogMgr::ListSets(ObjectGuid::LowType guidLow) const
{
    std::vector<std::string> out;

    auto itr = _sets.find(guidLow);
    if (itr == _sets.end())
        return out;

    out.reserve(itr->second.size());
    for (auto const& kv : itr->second)
        out.push_back(kv.first);

    std::sort(out.begin(), out.end());
    return out;
}

bool CustomTransmogMgr::ValidateEntry(uint32 fakeEntry, std::string* err) const
{
    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(fakeEntry);
    if (!proto)
    {
        if (err)
            *err = "物品不存在";
        return false;
    }

    // 没有模型的物品幻化上去等于隐形
    if (!proto->DisplayInfoID)
    {
        if (err)
            *err = "该物品没有模型（DisplayInfoID = 0）";
        return false;
    }

    return true;
}

bool CustomTransmogMgr::ValidateSlot(uint8 slot, uint32 fakeEntry, std::string* err) const
{
    if (slot >= TRANSMOG_MAX_SLOT)
    {
        if (err)
            *err = "槽位无效";
        return false;
    }

    if (IsInvisibleSlot(slot))
    {
        if (err)
            *err = "该槽位在 3.3.5 客户端没有模型（项链/戒指/饰品不显示外观）";
        return false;
    }

    if (!ValidateEntry(fakeEntry, err))
        return false;

    /*
     * 完全自由模式（用户拍板）：
     * 不检查甲类、不检查职业、不检查等级 ——
     * 布甲可以幻化成板甲外观，法杖可以变大剑。
     *
     * 唯一保留的软限制是「武器 <-> 护甲」不互换，
     * 因为客户端对武器槽和护甲槽用的是不同的挂载点，
     * 强行互换会导致模型飞到脚下或不显示。
     * 这个限制可用 Transmog.AllowWeaponCross = 1 关掉（默认关掉限制=允许）。
     */
    if (!_allowWeaponCross)
    {
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(fakeEntry);
        if (!proto)
            return false;

        bool slotIsWeapon = (slot == 15 || slot == 16 || slot == 17);
        bool itemIsWeapon = (proto->Class == ITEM_CLASS_WEAPON);

        if (slotIsWeapon != itemIsWeapon)
        {
            if (err)
                *err = "武器与护甲不能互相幻化（可在 conf 里开 Transmog.AllowWeaponCross = 1）";
            return false;
        }
    }

    return true;
}

void CustomTransmogMgr::RefreshPlayer(Player* player)
{
    if (!player)
        return;

    /*
     * 重刷 19 个槽位的外观。
     * SetVisibleItemSlot 内部已经会查幻化缓存，
     * 所以这里只要用当前真实装备再调一次即可。
     */
    for (uint8 slot = 0; slot < TRANSMOG_MAX_SLOT; ++slot)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        player->SetVisibleItemSlot(slot, item);
    }
}
