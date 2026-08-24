/*
 * ============================================================================
 *  step34  伙伴关怀系统 —— 数据层实现
 * ============================================================================
 *
 *  铁律遵守：
 *    - SQL 占位符用 {} 不是 %u（本仓库 DirectPExecute 走 fmt 库，用错崩服）
 *    - 不硬编码任何内容ID，启动时校验 item_template
 *    - GBK 兼容，不使用特殊符号
 * ============================================================================
 */

#include "bot_companion.h"

#include "DatabaseEnv.h"
#include "GameTime.h"       // GameTime::GetGameTime()  (game/Time/GameTime.h:32, 返回 time_t)
#include "Log.h"
#include "ObjectMgr.h"
#include "Random.h"         // urand()  (common/Utilities/Random.h:29)
#include "Timer.h"          // getMSTime / GetMSTimeDiffToNow
#include <algorithm>
#include <random>

BotCompanionMgr* BotCompanionMgr::instance()
{
    static BotCompanionMgr _instance;
    return &_instance;
}

// ---------------------------------------------------------------------------
//  加载台词
// ---------------------------------------------------------------------------
void BotCompanionMgr::LoadCareTexts()
{
    uint32 oldMSTime = getMSTime();
    _texts.clear();

    //                                                0   1          2          3     4      5
    QueryResult result = WorldDatabase.Query("SELECT id, care_type, bot_class, text, emote, weight FROM npcbot_care_text");
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 npcbot care texts. Table `npcbot_care_text` is empty.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        CompanionText t;
        t.Id       = fields[0].GetUInt32();
        t.CareType = fields[1].GetUInt8();
        t.BotClass = fields[2].GetUInt8();
        t.Text     = fields[3].GetString();
        t.Emote    = fields[4].GetUInt32();
        t.Weight   = fields[5].GetUInt8();

        if (t.CareType == CARE_TYPE_NONE || t.CareType >= CARE_TYPE_MAX)
        {
            TC_LOG_ERROR("sql.sql", "npcbot_care_text id {} has invalid care_type {}, skipped.", t.Id, t.CareType);
            continue;
        }
        if (t.Text.empty())
        {
            TC_LOG_ERROR("sql.sql", "npcbot_care_text id {} has empty text, skipped.", t.Id);
            continue;
        }
        if (t.Weight == 0)
            t.Weight = 1;

        _texts[t.CareType].push_back(std::move(t));
        ++count;
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded {} npcbot care texts in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
}

// ---------------------------------------------------------------------------
//  加载可给予物品
//
//  【关键】这里校验 item_template。
//  历史教训：硬编码的 itemid 在换了整合包之后可能不存在，
//  服务端不校验的话客户端会静默失败。
// ---------------------------------------------------------------------------
void BotCompanionMgr::LoadCareItems()
{
    uint32 oldMSTime = getMSTime();
    _items.clear();

    //                                                0   1          2        3          4          5
    QueryResult result = WorldDatabase.Query("SELECT id, care_type, item_id, min_level, max_level, source_text FROM npcbot_care_item");
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 npcbot care items. Table `npcbot_care_item` is empty.");
        return;
    }

    uint32 count = 0;
    uint32 skipped = 0;
    do
    {
        Field* fields = result->Fetch();

        CompanionItem it;
        it.Id         = fields[0].GetUInt32();
        it.CareType   = fields[1].GetUInt8();
        it.ItemId     = fields[2].GetUInt32();
        it.MinLevel   = fields[3].GetUInt8();
        it.MaxLevel   = fields[4].GetUInt8();
        it.SourceText = fields[5].GetString();

        // 校验物品真的存在
        if (!sObjectMgr->GetItemTemplate(it.ItemId))
        {
            TC_LOG_ERROR("sql.sql", "npcbot_care_item id {} references non-existent item {}, skipped.", it.Id, it.ItemId);
            ++skipped;
            continue;
        }

        if (it.MinLevel > it.MaxLevel)
        {
            TC_LOG_ERROR("sql.sql", "npcbot_care_item id {} has min_level {} > max_level {}, skipped.", it.Id, it.MinLevel, it.MaxLevel);
            ++skipped;
            continue;
        }

        _items[it.CareType].push_back(std::move(it));
        ++count;
    }
    while (result->NextRow());

    if (skipped)
        TC_LOG_WARN("server.loading", ">> Skipped {} invalid npcbot care items (see errors above).", skipped);

    TC_LOG_INFO("server.loading", ">> Loaded {} npcbot care items in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
}

// ---------------------------------------------------------------------------
//  加载 bot 背包
// ---------------------------------------------------------------------------
void BotCompanionMgr::LoadInventories()
{
    uint32 oldMSTime = getMSTime();
    _inventories.clear();

    //                                                     0         1        2      3
    QueryResult result = CharacterDatabase.Query("SELECT bot_guid, item_id, count, acquired_from FROM npcbot_inventory");
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 npcbot inventory entries.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        uint32 guid = fields[0].GetUInt32();

        CompanionInvEntry e;
        e.ItemId       = fields[1].GetUInt32();
        e.Count        = fields[2].GetUInt32();
        e.AcquiredFrom = fields[3].GetString();

        if (!e.Count)
            continue;

        _inventories[guid].push_back(std::move(e));
        ++count;
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded {} npcbot inventory entries in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void BotCompanionMgr::ReloadAll()
{
    LoadCareTexts();
    LoadCareItems();
    LoadInventories();
}

// ---------------------------------------------------------------------------
//  按权重随机挑一条台词
// ---------------------------------------------------------------------------
std::string BotCompanionMgr::PickText(uint8 careType, uint8 botClass) const
{
    auto itr = _texts.find(careType);
    if (itr == _texts.end() || itr->second.empty())
        return std::string();

    // 先收集匹配的（职业专属优先，没有就用通用）
    std::vector<CompanionText const*> pool;
    for (CompanionText const& t : itr->second)
        if (t.BotClass == botClass)
            pool.push_back(&t);

    if (pool.empty())
        for (CompanionText const& t : itr->second)
            if (t.BotClass == 0)
                pool.push_back(&t);

    if (pool.empty())
        return std::string();

    // 权重随机
    uint32 total = 0;
    for (CompanionText const* t : pool)
        total += t->Weight;

    if (!total)
        return pool[0]->Text;

    uint32 roll = urand(0, total - 1);
    for (CompanionText const* t : pool)
    {
        if (roll < t->Weight)
            return t->Text;
        roll -= t->Weight;
    }

    return pool.back()->Text;
}

// ---------------------------------------------------------------------------
//  按等级挑物品。挑等级段最匹配的（尽量给好的）。
// ---------------------------------------------------------------------------
CompanionItem const* BotCompanionMgr::PickItem(uint8 careType, uint8 level) const
{
    auto itr = _items.find(careType);
    if (itr == _items.end() || itr->second.empty())
        return nullptr;

    CompanionItem const* best = nullptr;
    for (CompanionItem const& it : itr->second)
    {
        if (level < it.MinLevel || level > it.MaxLevel)
            continue;
        // 同样符合条件时，取 min_level 更高的（更好的东西）
        if (!best || it.MinLevel > best->MinLevel)
            best = &it;
    }

    return best;
}

// ---------------------------------------------------------------------------
//  虚拟背包操作
// ---------------------------------------------------------------------------
void BotCompanionMgr::AddToInventory(uint32 botGuid, uint32 itemId, uint32 count, std::string const& from)
{
    if (!count || !itemId)
        return;

    auto& inv = _inventories[botGuid];
    for (CompanionInvEntry& e : inv)
    {
        if (e.ItemId == itemId)
        {
            e.Count += count;
            // 铁律5：占位符是 {} 不是 %u
            CharacterDatabase.DirectPExecute(
                "UPDATE `npcbot_inventory` SET `count` = {} WHERE `bot_guid` = {} AND `item_id` = {}",
                e.Count, botGuid, itemId);
            return;
        }
    }

    CompanionInvEntry e;
    e.ItemId       = itemId;
    e.Count        = count;
    e.AcquiredFrom = from;
    inv.push_back(e);

    // 来源字符串来自数据库，可能含引号。不转义会拼坏 SQL（甚至注入）。
    // 官方同款做法：botdump.cpp:804  CharacterDatabase.EscapeString(s);
    std::string safeFrom = from;
    CharacterDatabase.EscapeString(safeFrom);

    CharacterDatabase.DirectPExecute(
        "REPLACE INTO `npcbot_inventory` (`bot_guid`, `item_id`, `count`, `acquired_from`, `acquired_time`) "
        "VALUES ({}, {}, {}, '{}', {})",
        botGuid, itemId, count, safeFrom, uint32(GameTime::GetGameTime()));
}

bool BotCompanionMgr::TakeFromInventory(uint32 botGuid, uint32 itemId, uint32 count)
{
    auto itr = _inventories.find(botGuid);
    if (itr == _inventories.end())
        return false;

    auto& inv = itr->second;
    for (auto e = inv.begin(); e != inv.end(); ++e)
    {
        if (e->ItemId != itemId)
            continue;
        if (e->Count < count)
            return false;

        e->Count -= count;
        if (e->Count == 0)
        {
            inv.erase(e);
            CharacterDatabase.DirectPExecute(
                "DELETE FROM `npcbot_inventory` WHERE `bot_guid` = {} AND `item_id` = {}",
                botGuid, itemId);
        }
        else
        {
            CharacterDatabase.DirectPExecute(
                "UPDATE `npcbot_inventory` SET `count` = {} WHERE `bot_guid` = {} AND `item_id` = {}",
                e->Count, botGuid, itemId);
        }
        return true;
    }

    return false;
}

bool BotCompanionMgr::HasInInventory(uint32 botGuid, uint32 itemId, uint32 count) const
{
    auto itr = _inventories.find(botGuid);
    if (itr == _inventories.end())
        return false;

    for (CompanionInvEntry const& e : itr->second)
        if (e.ItemId == itemId && e.Count >= count)
            return true;

    return false;
}

uint32 BotCompanionMgr::FindInInventory(uint32 botGuid, uint8 careType, uint8 level, std::string& outFrom) const
{
    auto invItr = _inventories.find(botGuid);
    if (invItr == _inventories.end())
        return 0;

    auto poolItr = _items.find(careType);
    if (poolItr == _items.end())
        return 0;

    // 在背包里找一个属于该 careType 且等级合适的
    uint32 bestItem = 0;
    uint8  bestLvl  = 0;
    for (CompanionInvEntry const& e : invItr->second)
    {
        if (!e.Count)
            continue;
        for (CompanionItem const& it : poolItr->second)
        {
            if (it.ItemId != e.ItemId)
                continue;
            if (level < it.MinLevel || level > it.MaxLevel)
                continue;
            if (it.MinLevel >= bestLvl)
            {
                bestLvl  = it.MinLevel;
                bestItem = e.ItemId;
                outFrom  = e.AcquiredFrom;
            }
        }
    }

    return bestItem;
}

std::vector<CompanionInvEntry> const* BotCompanionMgr::GetInventory(uint32 botGuid) const
{
    auto itr = _inventories.find(botGuid);
    return itr == _inventories.end() ? nullptr : &itr->second;
}

// ---------------------------------------------------------------------------
//  给 bot 补货
//
//  设计：bot 不是无限供应。补货有上限，用完要等下次。
//  这样"给你东西"才有分量 —— 它是真的从自己的储备里拿。
// ---------------------------------------------------------------------------
void BotCompanionMgr::RestockBot(uint32 botGuid, uint8 level)
{
    static uint32 const RESTOCK_COUNT = 5;

    for (uint8 type : { uint8(CARE_TYPE_FOOD), uint8(CARE_TYPE_DRINK) })
    {
        CompanionItem const* it = PickItem(type, level);
        if (!it)
            continue;

        // 已经有了就不重复补
        if (HasInInventory(botGuid, it->ItemId, 1))
            continue;

        AddToInventory(botGuid, it->ItemId, RESTOCK_COUNT, it->SourceText);
    }
}
