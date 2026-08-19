/*
 * CustomStatPersist.cpp - GM 属性修改持久化实现
 * 详见 CustomStatPersist.h 头部说明
 */

#include "CustomStatPersist.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Player.h"
#include "Timer.h"
#include "Unit.h"
#include <algorithm>

CustomStatPersistMgr* CustomStatPersistMgr::instance()
{
    static CustomStatPersistMgr inst;
    return &inst;
}

// ============================================================================
//  名字表
// ============================================================================

char const* CustomStatPersistMgr::UnitModName(uint8 idx)
{
    // 对应 Unit.h:157 的 UnitMods 枚举顺序
    static char const* names[] =
    {
        "力量", "敏捷", "耐力", "智力", "精神",
        "生命", "法力", "怒气", "集中值", "能量", "快乐", "符文", "符文能量",
        "护甲", "神圣抗性", "火焰抗性", "自然抗性", "冰霜抗性", "暗影抗性", "奥术抗性",
        "攻击强度", "远程攻击强度",
        "主手伤害", "副手伤害", "远程伤害",
    };
    if (idx < sizeof(names) / sizeof(names[0]))
        return names[idx];
    return "未知";
}

char const* CustomStatPersistMgr::RatingName(uint8 idx)
{
    // 对应 Unit.h:322 的 CombatRating 枚举顺序
    static char const* names[] =
    {
        "武器技能", "防御", "躲闪", "招架", "格挡",
        "近战命中", "远程命中", "法术命中",
        "近战暴击", "远程暴击", "法术暴击",
        "近战受击", "远程受击", "法术受击",
        "近战受暴", "远程受暴", "法术受暴",
        "近战急速", "远程急速", "法术急速",
        "主手武器技能", "副手武器技能", "远程武器技能",
        "精准", "护甲穿透",
    };
    if (idx < sizeof(names) / sizeof(names[0]))
        return names[idx];
    return "未知";
}

// ============================================================================
//  生命周期
// ============================================================================

void CustomStatPersistMgr::LoadConfig()
{
    _enabled = sConfigMgr->GetBoolDefault("PlayerStat.Persist", true);
}

void CustomStatPersistMgr::LoadFromDB()
{
    LoadConfig();
    _data.clear();

    if (!_enabled)
    {
        TC_LOG_INFO("server.loading", ">> GM属性持久化：已关闭（PlayerStat.Persist = 0）");
        return;
    }

    uint32 oldMSTime = getMSTime();

    QueryResult result = CharacterDatabase.Query(
        "SELECT guid, statType, statIndex, amount FROM custom_playerstat");

    uint32 count = 0;
    if (result)
    {
        do
        {
            Field* f = result->Fetch();
            ObjectGuid::LowType guidLow = f[0].GetUInt32();
            uint8  type  = f[1].GetUInt8();
            uint8  index = f[2].GetUInt8();
            float  amt   = f[3].GetFloat();

            if (type > TYPE_RATING)
            {
                TC_LOG_ERROR("server.loading",
                    "custom_playerstat: guid {} 的 statType {} 无效，已跳过", guidLow, type);
                continue;
            }

            if (amt == 0.0f)
                continue;

            _data[guidLow][MakeKey(StatType(type), index)] = amt;
            ++count;
        }
        while (result->NextRow());
    }

    TC_LOG_INFO("server.loading",
        ">> 载入 {} 条 GM 属性修改（{} 名角色），耗时 {} ms",
        count, uint32(_data.size()), GetMSTimeDiffToNow(oldMSTime));
}

// ============================================================================
//  玩家事件
// ============================================================================

void CustomStatPersistMgr::OnPlayerLogin(Player* player)
{
    if (!_enabled || !player)
        return;

    auto itr = _data.find(player->GetGUID().GetCounter());
    if (itr == _data.end())
        return;

    uint32 applied = 0;

    for (auto const& kv : itr->second)
    {
        StatType type = StatType(kv.first >> 8);
        uint8    idx  = uint8(kv.first & 0xFF);
        float    amt  = kv.second;

        if (amt == 0.0f)
            continue;

        if (type == TYPE_UNITMOD)
        {
            if (idx >= UNIT_MOD_END)
                continue;
            /*
             * 用 SetStatFlatModifier 设 TOTAL_VALUE 槽。
             * 注意是 Set 不是 Handle —— Set 是直接赋值，
             * 重复登录不会累加。
             */
            player->SetStatFlatModifier(UnitMods(idx), TOTAL_VALUE, amt);
            ++applied;
        }
        else // TYPE_RATING
        {
            if (idx >= MAX_COMBAT_RATING)
                continue;
            player->ApplyRatingMod(CombatRating(idx), int32(amt), true);
            ++applied;
        }
    }

    if (applied)
    {
        // 一次性重算，比每项都算便宜
        player->UpdateAllStats();

        TC_LOG_DEBUG("entities.player",
            "CustomStatPersist: 为玩家 {} 恢复了 {} 项属性修改",
            player->GetName(), applied);
    }
}

void CustomStatPersistMgr::OnCharacterDeleted(ObjectGuid::LowType guidLow)
{
    _data.erase(guidLow);
    CharacterDatabase.PExecute("DELETE FROM custom_playerstat WHERE guid = {}", guidLow);
}

// ============================================================================
//  记录 / 查询
// ============================================================================

void CustomStatPersistMgr::Record(ObjectGuid::LowType guidLow, StatType type,
                                  uint8 index, float amount)
{
    if (!_enabled)
        return;

    uint16 key = MakeKey(type, index);

    if (amount == 0.0f)
    {
        auto itr = _data.find(guidLow);
        if (itr != _data.end())
        {
            itr->second.erase(key);
            if (itr->second.empty())
                _data.erase(itr);
        }
        CharacterDatabase.PExecute(
            "DELETE FROM custom_playerstat WHERE guid = {} AND statType = {} AND statIndex = {}",
            guidLow, uint32(type), uint32(index));
        return;
    }

    _data[guidLow][key] = amount;

    CharacterDatabase.PExecute(
        "REPLACE INTO custom_playerstat (guid, statType, statIndex, amount) "
        "VALUES ({}, {}, {}, {})",
        guidLow, uint32(type), uint32(index), amount);
}

float CustomStatPersistMgr::Get(ObjectGuid::LowType guidLow, StatType type, uint8 index) const
{
    auto itr = _data.find(guidLow);
    if (itr == _data.end())
        return 0.0f;

    auto k = itr->second.find(MakeKey(type, index));
    return (k == itr->second.end()) ? 0.0f : k->second;
}

bool CustomStatPersistMgr::HasAny(ObjectGuid::LowType guidLow) const
{
    return _data.find(guidLow) != _data.end();
}

void CustomStatPersistMgr::ClearAll(Player* player)
{
    if (!player)
        return;

    ObjectGuid::LowType guidLow = player->GetGUID().GetCounter();

    auto itr = _data.find(guidLow);
    if (itr != _data.end())
    {
        // 先从玩家身上撤销，再删记录
        for (auto const& kv : itr->second)
        {
            StatType type = StatType(kv.first >> 8);
            uint8    idx  = uint8(kv.first & 0xFF);

            if (type == TYPE_UNITMOD)
            {
                if (idx < UNIT_MOD_END)
                    player->SetStatFlatModifier(UnitMods(idx), TOTAL_VALUE, 0.0f);
            }
            else
            {
                if (idx < MAX_COMBAT_RATING)
                    player->ApplyRatingMod(CombatRating(idx), -int32(kv.second), true);
            }
        }
        _data.erase(itr);
        player->UpdateAllStats();
    }

    CharacterDatabase.PExecute("DELETE FROM custom_playerstat WHERE guid = {}", guidLow);
}

std::vector<CustomStatPersistMgr::Entry>
CustomStatPersistMgr::List(ObjectGuid::LowType guidLow) const
{
    std::vector<Entry> out;

    auto itr = _data.find(guidLow);
    if (itr == _data.end())
        return out;

    out.reserve(itr->second.size());
    for (auto const& kv : itr->second)
    {
        Entry e;
        e.type   = StatType(kv.first >> 8);
        e.index  = uint8(kv.first & 0xFF);
        e.amount = kv.second;
        out.push_back(e);
    }

    std::sort(out.begin(), out.end(), [](Entry const& a, Entry const& b)
    {
        if (a.type != b.type)
            return a.type < b.type;
        return a.index < b.index;
    });

    return out;
}
