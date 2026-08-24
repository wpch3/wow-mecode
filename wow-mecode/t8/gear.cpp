/*
 * ============================================================================
 *  套装系统 —— cs_gearset.cpp
 * ============================================================================
 *
 *  复用 cs_smartadd.cpp 的批量地基（BatchSession + Gossip 分页）
 *
 *  ── 指令 ──────────────────────────────────────────────────────────────
 *   .gearset                        打开主菜单（可点击）
 *   .gearset <职业> <装等>          按职业+装等生成整套
 *   .gearset <职业> <装等> <定位>   定位：tank/dps/heal
 *   .gearset save <方案名>          保存当前全身装备为方案
 *   .gearset load                   弹菜单选择已保存方案
 *   .gearset load <方案名>          直接加载指定方案
 *   .gearset list                   列出所有已保存方案
 *   .gearset del <方案名>           删除方案
 *   .gearset strip                  卸下全身装备（存入背包）
 *
 *  ── 设计要点 ──────────────────────────────────────────────────────────
 *   1. 只发放到背包，不自动穿戴 —— 避免覆盖你手动配好的装备
 *      （想自动穿用 .gearset ... equip）
 *   2. 按职业自动选护甲类型（布/皮/锁/板）
 *   3. 按定位过滤属性（坦克要耐力护甲，治疗要智力精神）
 *   4. 方案存数据库表 custom_gearset，跨角色可用
 *
 *  放置：D:\TrinityCore\src\server\scripts\Commands\cs_gearset.cpp
 *  需要先执行配套 SQL 建表
 * ============================================================================
 */

#include "mock.h"
#include <vector>
#include <string>
#include <algorithm>
#include <unordered_map>

namespace GearSet
{
    static constexpr uint32 GOSSIP_HARD_LIMIT = 32;
    static constexpr uint32 NAV_SLOTS         = 3;
    static constexpr uint32 PER_PAGE          = GOSSIP_HARD_LIMIT - NAV_SLOTS;  // 29
    static constexpr uint32 MENU_ID           = 60100;   // 避开 smartadd 的 60000

    static constexpr uint32 SENDER_MAIN   = 1;   // 主菜单
    static constexpr uint32 SENDER_CLASS  = 2;   // 选职业
    static constexpr uint32 SENDER_ROLE   = 3;   // 选定位
    static constexpr uint32 SENDER_ILVL   = 4;   // 选装等
    static constexpr uint32 SENDER_LOAD   = 5;   // 加载方案
    static constexpr uint32 SENDER_NAV    = 9;

    static constexpr uint32 NAV_PREV   = 1;
    static constexpr uint32 NAV_NEXT   = 2;
    static constexpr uint32 NAV_CANCEL = 3;
    static constexpr uint32 NAV_BACK   = 4;

    enum GearRole : uint8
    {
        ROLE_ANY  = 0,
        ROLE_TANK = 1,
        ROLE_DPS  = 2,
        ROLE_HEAL = 3,
    };

    // 会话：记录用户在多级菜单里的选择
    struct GearSession
    {
        uint8               step      = 0;      // 0=主菜单 1=选职业 2=选定位 3=选装等
        uint8               cls       = 0;      // 选中的职业
        GearRole            role      = ROLE_ANY;
        uint32              ilvl      = 0;
        bool                autoEquip = false;
        uint32              page      = 0;
        std::vector<std::string> saveNames;     // load 菜单用
        void Clear() { *this = GearSession(); }
    };

    static std::unordered_map<uint32, GearSession> s_sess;

    // ------------------------------------------------------------------
    //  职业 -> 护甲类型
    // ------------------------------------------------------------------
    inline uint32 GetArmorSubClassForClass(uint8 cls)
    {
        switch (cls)
        {
            case CLASS_WARRIOR:
            case CLASS_PALADIN:
            case CLASS_DEATH_KNIGHT:
                return ITEM_SUBCLASS_ARMOR_PLATE;
            case CLASS_HUNTER:
            case CLASS_SHAMAN:
                return ITEM_SUBCLASS_ARMOR_MAIL;
            case CLASS_ROGUE:
            case CLASS_DRUID:
                return ITEM_SUBCLASS_ARMOR_LEATHER;
            case CLASS_PRIEST:
            case CLASS_MAGE:
            case CLASS_WARLOCK:
            default:
                return ITEM_SUBCLASS_ARMOR_CLOTH;
        }
    }

    inline char const* GetClassName(uint8 cls)
    {
        switch (cls)
        {
            case CLASS_WARRIOR:      return "战士";
            case CLASS_PALADIN:      return "圣骑士";
            case CLASS_HUNTER:       return "猎人";
            case CLASS_ROGUE:        return "潜行者";
            case CLASS_PRIEST:       return "牧师";
            case CLASS_DEATH_KNIGHT: return "死亡骑士";
            case CLASS_SHAMAN:       return "萨满祭司";
            case CLASS_MAGE:         return "法师";
            case CLASS_WARLOCK:      return "术士";
            case CLASS_DRUID:        return "德鲁伊";
            default:                 return "未知";
        }
    }

    inline char const* GetRoleName(GearRole r)
    {
        switch (r)
        {
            case ROLE_TANK: return "坦克";
            case ROLE_DPS:  return "输出";
            case ROLE_HEAL: return "治疗";
            default:        return "通用";
        }
    }

    // 按职业名（中文/英文/数字）解析
    inline uint8 ParseClass(std::string const& s)
    {
        struct { char const* key; uint8 cls; } const map[] =
        {
            { "战士",     CLASS_WARRIOR      }, { "warrior", CLASS_WARRIOR      },
            { "圣骑士",   CLASS_PALADIN      }, { "paladin", CLASS_PALADIN      },
            { "骑士",     CLASS_PALADIN      },
            { "猎人",     CLASS_HUNTER       }, { "hunter",  CLASS_HUNTER       },
            { "潜行者",   CLASS_ROGUE        }, { "rogue",   CLASS_ROGUE        },
            { "盗贼",     CLASS_ROGUE        },
            { "牧师",     CLASS_PRIEST       }, { "priest",  CLASS_PRIEST       },
            { "死亡骑士", CLASS_DEATH_KNIGHT }, { "dk",      CLASS_DEATH_KNIGHT },
            { "萨满",     CLASS_SHAMAN       }, { "shaman",  CLASS_SHAMAN       },
            { "法师",     CLASS_MAGE         }, { "mage",    CLASS_MAGE         },
            { "术士",     CLASS_WARLOCK      }, { "warlock", CLASS_WARLOCK      },
            { "德鲁伊",   CLASS_DRUID        }, { "druid",   CLASS_DRUID        },
            { "小德",     CLASS_DRUID        },
        };
        std::string low = s;
        std::transform(low.begin(), low.end(), low.begin(), ::tolower);
        for (auto const& m : map)
            if (low == m.key || s == m.key)
                return m.cls;
        return 0;
    }

    inline GearRole ParseRole(std::string const& s)
    {
        std::string low = s;
        std::transform(low.begin(), low.end(), low.begin(), ::tolower);
        if (low == "tank" || s == "坦克" || s == "防御") return ROLE_TANK;
        if (low == "heal" || low == "healer" || s == "治疗" || s == "奶") return ROLE_HEAL;
        if (low == "dps"  || s == "输出") return ROLE_DPS;
        return ROLE_ANY;
    }

    // ------------------------------------------------------------------
    //  定位评分：坦克重耐力护甲，治疗重智力精神，输出重主属性
    // ------------------------------------------------------------------
    inline double ScoreForRole(ItemTemplate const* p, GearRole role)
    {
        if (!p)
            return -1.0;

        double s = double(p->ItemLevel) * 10.0 + double(p->Quality) * 50.0;

        for (uint32 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
        {
            uint32 type = p->ItemStat[i].ItemStatType;
            double val  = double(p->ItemStat[i].ItemStatValue);
            if (val <= 0.0)
                continue;

            switch (role)
            {
                case ROLE_TANK:
                    if (type == ITEM_MOD_STAMINA)             s += val * 3.0;
                    else if (type == ITEM_MOD_DEFENSE_SKILL_RATING
                          || type == ITEM_MOD_DODGE_RATING
                          || type == ITEM_MOD_PARRY_RATING
                          || type == ITEM_MOD_BLOCK_RATING)   s += val * 2.5;
                    else if (type == ITEM_MOD_STRENGTH)       s += val * 1.0;
                    else                                       s += val * 0.3;
                    break;
                case ROLE_HEAL:
                    if (type == ITEM_MOD_INTELLECT)           s += val * 3.0;
                    else if (type == ITEM_MOD_SPIRIT)         s += val * 2.5;
                    else if (type == ITEM_MOD_SPELL_POWER)    s += val * 2.0;
                    else if (type == ITEM_MOD_MANA_REGENERATION) s += val * 2.0;
                    else                                       s += val * 0.3;
                    break;
                case ROLE_DPS:
                    if (type == ITEM_MOD_STRENGTH || type == ITEM_MOD_AGILITY
                     || type == ITEM_MOD_INTELLECT)           s += val * 2.5;
                    else if (type == ITEM_MOD_ATTACK_POWER
                          || type == ITEM_MOD_SPELL_POWER)    s += val * 1.5;
                    else if (type == ITEM_MOD_CRIT_RATING
                          || type == ITEM_MOD_HASTE_RATING)   s += val * 2.0;
                    else                                       s += val * 0.5;
                    break;
                default:
                    s += val;
                    break;
            }
        }

        if (role == ROLE_TANK)
            s += double(p->Armor) * 0.5;

        return s;
    }

    // ------------------------------------------------------------------
    //  InventoryType -> 需要几件
    //  返回 0 表示该部位不需要
    // ------------------------------------------------------------------
    struct SlotNeed { uint32 invType; uint32 count; char const* name; };

    inline std::vector<SlotNeed> GetSlotNeeds()
    {
        return {
            { INVTYPE_HEAD,       1, "头部" },
            { INVTYPE_NECK,       1, "颈部" },
            { INVTYPE_SHOULDERS,  1, "肩部" },
            { INVTYPE_CLOAK,      1, "背部" },
            { INVTYPE_CHEST,      1, "胸甲" },
            { INVTYPE_WRISTS,     1, "护腕" },
            { INVTYPE_HANDS,      1, "手套" },
            { INVTYPE_WAIST,      1, "腰带" },
            { INVTYPE_LEGS,       1, "腿部" },
            { INVTYPE_FEET,       1, "靴子" },
            { INVTYPE_FINGER,     2, "戒指" },
            { INVTYPE_TRINKET,    2, "饰品" },
        };
    }

    // ------------------------------------------------------------------
    //  为某个部位挑选最佳装备
    // ------------------------------------------------------------------
    inline uint32 PickBestForSlot(uint8 cls, GearRole role, uint32 invType,
                                  uint32 maxIlvl, std::vector<uint32> const& exclude)
    {
        uint32 armorSub = GetArmorSubClassForClass(cls);
        uint32 bestId = 0;
        double bestScore = -1.0;

        for (auto const& pair : sObjectMgr->GetItemTemplateStore())
        {
            ItemTemplate const* p = &pair.second;

            if (p->InventoryType != invType)
                continue;
            if (maxIlvl && p->ItemLevel > maxIlvl)
                continue;
            if (p->Quality < ITEM_QUALITY_UNCOMMON)   // 至少绿装
                continue;
            if (p->StatsCount == 0)                   // 必须有属性
                continue;

            // 护甲类：必须匹配职业可穿的类型
            if (p->Class == ITEM_CLASS_ARMOR)
            {
                bool isJewelry = (invType == INVTYPE_NECK || invType == INVTYPE_FINGER
                               || invType == INVTYPE_TRINKET || invType == INVTYPE_CLOAK);
                if (!isJewelry && p->SubClass != armorSub)
                    continue;
            }

            // 职业限制检查
            if (p->AllowableClass != -1 && !(p->AllowableClass & (1 << (cls - 1))))
                continue;

            if (std::find(exclude.begin(), exclude.end(), pair.first) != exclude.end())
                continue;

            double sc = ScoreForRole(p, role);
            if (sc > bestScore)
            {
                bestScore = sc;
                bestId = pair.first;
            }
        }
        return bestId;
    }


    // ------------------------------------------------------------------
    //  根据玩家当前所在副本 / 等级，推荐合适的装等
    //  用户需求：进副本时能一键拿到对应档次的装备
    // ------------------------------------------------------------------
    inline uint32 SuggestIlvlForPlayer(Player* player)
    {
        uint8 lvl = player->GetLevel();
        Map* map = player->GetMap();

        // 优先按副本判断
        if (map && (map->IsDungeon() || map->IsRaid()))
        {
            uint32 mapId = map->GetId();
            bool heroic = map->IsHeroic();

            switch (mapId)
            {
                // ---- WLK 团本 ----
                case 631: return heroic ? 264u : 251u;  // 冰冠堡垒
                case 649: return heroic ? 258u : 232u;  // 十字军的试炼
                case 603: return heroic ? 232u : 219u;  // 奥杜尔
                case 615: case 616: return 213u;        // 黑曜石/永恒之眼
                case 533: return 213u;                  // 纳克萨玛斯
                case 249: return 226u;                  // 奥妮克希亚
                case 724: return 277u;                  // 红玉圣殿
                // ---- WLK 5人本 ----
                case 601: case 602: case 604: case 599:
                case 600: case 608: case 619: case 632:
                case 658: case 668: return heroic ? 200u : 175u;
                // ---- TBC 团本 ----
                case 564: return 146u;                  // 黑暗神殿
                case 580: return 164u;                  // 太阳井
                case 548: case 550: return 128u;
                // ---- 经典团本 ----
                case 409: return 71u;                   // 熔火之心
                case 469: return 78u;                   // 黑翼之巢
                case 531: return 90u;                   // AQ40
                default: break;
            }
            // 未列出的副本：按等级推算
        }

        // 按等级推算（向下兼容，低级角色拿低级装）
        if (lvl <= 20)  return 25u;
        if (lvl <= 40)  return 45u;
        if (lvl <= 58)  return 65u;
        if (lvl <= 60)  return 92u;
        if (lvl <= 68)  return 115u;
        if (lvl <= 70)  return 146u;
        if (lvl <= 79)  return 175u;
        if (lvl <= 80)  return 232u;
        // 80 级以上（等级255改造后）：按等级线性放大
        return uint32(232 + (lvl - 80) * 20);
    }

    inline char const* GetLocationDesc(Player* player)
    {
        Map* map = player->GetMap();
        if (!map)
            return "未知";
        if (map->IsRaid())
            return map->IsHeroic() ? "团本(英雄)" : "团本(普通)";
        if (map->IsDungeon())
            return map->IsHeroic() ? "5人本(英雄)" : "5人本(普通)";
        return "野外";
    }

    // ------------------------------------------------------------------
    //  发放物品到背包
    // ------------------------------------------------------------------
    inline bool GiveToBag(ChatHandler* handler, Player* target, uint32 itemId)
    {
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
        if (!proto)
            return false;

        ItemPosCountVec dest;
        uint32 noSpace = 0;
        InventoryResult msg = target->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemId, 1, &noSpace);
        if (msg != EQUIP_ERR_OK || dest.empty())
            return false;

        Item* item = target->StoreNewItem(dest, itemId, true, GenerateItemRandomPropertyId(itemId));
        if (item)
            target->SendNewItem(item, 1, false, true);
        return item != nullptr;
    }

    // ------------------------------------------------------------------
    //  生成整套装备
    // ------------------------------------------------------------------
    inline void GenerateSet(ChatHandler* handler, Player* target,
                            uint8 cls, GearRole role, uint32 ilvl, bool autoEquip)
    {
        handler->PSendSysMessage("|cffffcc00正在生成 %s / %s / 装等<=%u 的套装...|r",
            GetClassName(cls), GetRoleName(role), ilvl ? ilvl : 999u);

        std::vector<uint32> picked;
        uint32 ok = 0, fail = 0;

        for (SlotNeed const& need : GetSlotNeeds())
        {
            for (uint32 n = 0; n < need.count; ++n)
            {
                uint32 id = PickBestForSlot(cls, role, need.invType, ilvl, picked);
                if (!id)
                {
                    handler->PSendSysMessage("  |cff888888[跳过]|r %s（无合适装备）", need.name);
                    ++fail;
                    continue;
                }
                picked.push_back(id);

                if (GiveToBag(handler, target, id))
                {
                    ItemTemplate const* p = sObjectMgr->GetItemTemplate(id);
                    handler->PSendSysMessage("  |cff00ff00[%s]|r %s |cff888888(装等%u)|r",
                        need.name,
                        p->GetName(handler->GetSessionDbLocaleIndex()).c_str(),
                        p->ItemLevel);
                    ++ok;
                }
                else
                {
                    handler->PSendSysMessage("  |cffff0000[背包满]|r %s", need.name);
                    ++fail;
                }
            }
        }

        handler->PSendSysMessage("|cffffcc00完成：获得 %u 件，跳过 %u 件|r", ok, fail);
        if (!autoEquip)
            handler->SendSysMessage("|cff00ccff装备已放入背包，请手动穿戴（避免覆盖你现有装备）|r");
    }

    // ------------------------------------------------------------------
    //  方案保存 / 读取
    // ------------------------------------------------------------------
    inline void SaveCurrentGear(ChatHandler* handler, Player* player, std::string const& name)
    {
        std::string safeName = name;
        CharacterDatabase.EscapeString(safeName);

        // 先删同名
        CharacterDatabase.PExecute("DELETE FROM custom_gearset WHERE owner_guid = {} AND set_name = '{}'",
            player->GetGUID().GetCounter(), safeName);

        uint32 saved = 0;
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (!item)
                continue;
            CharacterDatabase.PExecute(
                "INSERT INTO custom_gearset (owner_guid, set_name, slot, item_entry) VALUES ({}, '{}', {}, {})",
                player->GetGUID().GetCounter(), safeName, uint32(slot), item->GetEntry());
            ++saved;
        }

        if (saved)
            handler->PSendSysMessage("|cff00ff00已保存方案|r「%s」|cff888888（%u 件装备）|r",
                name.c_str(), saved);
        else
        {
            handler->SendSysMessage("|cffff0000当前没有穿戴任何装备，未保存。|r");
            handler->SetSentErrorMessage(true);
        }
    }

    inline void LoadGearSet(ChatHandler* handler, Player* player, std::string const& name)
    {
        std::string safeName = name;
        CharacterDatabase.EscapeString(safeName);

        QueryResult res = CharacterDatabase.PQuery(
            "SELECT slot, item_entry FROM custom_gearset WHERE owner_guid = {} AND set_name = '{}' ORDER BY slot",
            player->GetGUID().GetCounter(), safeName);

        if (!res)
        {
            handler->PSendSysMessage("|cffff0000找不到方案|r「%s」", name.c_str());
            handler->SetSentErrorMessage(true);
            return;
        }

        uint32 ok = 0, fail = 0;
        do
        {
            Field* f = res->Fetch();
            uint32 itemId = f[1].GetUInt32();
            if (GiveToBag(handler, player, itemId))
                ++ok;
            else
                ++fail;
        } while (res->NextRow());

        handler->PSendSysMessage("|cff00ff00方案|r「%s」|cff00ff00已发放：%u 件|r%s",
            name.c_str(), ok,
            fail ? Trinity::StringFormat("  |cffff0000（{} 件失败，背包可能已满）|r", fail).c_str() : "");
    }

    inline std::vector<std::string> GetSaveList(Player* player)
    {
        std::vector<std::string> out;
        QueryResult res = CharacterDatabase.PQuery(
            "SELECT DISTINCT set_name FROM custom_gearset WHERE owner_guid = {} ORDER BY set_name",
            player->GetGUID().GetCounter());
        if (!res)
            return out;
        do { out.push_back(res->Fetch()[0].GetString()); } while (res->NextRow());
        return out;
    }

    // ------------------------------------------------------------------
    //  菜单渲染
    // ------------------------------------------------------------------
    inline void SendMainMenu(Player* player, ChatHandler* handler)
    {
        player->PlayerTalkClass->ClearMenus();
        player->PlayerTalkClass->GetGossipMenu().SetMenuId(MENU_ID);
        uint32 idx = 0;
        auto& m = player->PlayerTalkClass->GetGossipMenu();

        char autoBuf[128];
        snprintf(autoBuf, sizeof(autoBuf), "[推荐] 按当前位置自动配装  (%s / 装等%u)",
            GetLocationDesc(player), SuggestIlvlForPlayer(player));
        m.AddMenuItem(int32(idx++), GOSSIP_ICON_BATTLE, autoBuf, SENDER_MAIN, 4, "", 0, false);
        m.AddMenuItem(int32(idx++), GOSSIP_ICON_BATTLE, "生成套装（选职业）", SENDER_MAIN, 1, "", 0, false);
        m.AddMenuItem(int32(idx++), GOSSIP_ICON_MONEY_BAG, "保存当前装备为方案", SENDER_MAIN, 2, "", 0, false);
        m.AddMenuItem(int32(idx++), GOSSIP_ICON_TALK, "加载已保存方案", SENDER_MAIN, 3, "", 0, false);
        m.AddMenuItem(int32(idx++), GOSSIP_ICON_CHAT, "[ 关闭 ]", SENDER_NAV, NAV_CANCEL, "", 0, false);

        player->PlayerTalkClass->SendGossipMenu(DEFAULT_GOSSIP_MESSAGE, player->GetGUID());
        handler->SendSysMessage("|cffffcc00=== 套装系统 ===|r");
    }

    inline void SendClassMenu(Player* player, ChatHandler* handler)
    {
        player->PlayerTalkClass->ClearMenus();
        player->PlayerTalkClass->GetGossipMenu().SetMenuId(MENU_ID);
        uint32 idx = 0;
        auto& m = player->PlayerTalkClass->GetGossipMenu();

        uint8 const classes[] = { CLASS_WARRIOR, CLASS_PALADIN, CLASS_HUNTER, CLASS_ROGUE,
                                  CLASS_PRIEST, CLASS_DEATH_KNIGHT, CLASS_SHAMAN,
                                  CLASS_MAGE, CLASS_WARLOCK, CLASS_DRUID };
        for (uint8 c : classes)
            m.AddMenuItem(int32(idx++), GOSSIP_ICON_BATTLE, GetClassName(c), SENDER_CLASS, c, "", 0, false);

        m.AddMenuItem(int32(idx++), GOSSIP_ICON_CHAT, "<< 返回", SENDER_NAV, NAV_BACK, "", 0, false);
        m.AddMenuItem(int32(idx++), GOSSIP_ICON_CHAT, "[ 关闭 ]", SENDER_NAV, NAV_CANCEL, "", 0, false);

        player->PlayerTalkClass->SendGossipMenu(DEFAULT_GOSSIP_MESSAGE, player->GetGUID());
        handler->SendSysMessage("|cffffcc00选择职业：|r");
    }

    inline void SendRoleMenu(Player* player, ChatHandler* handler)
    {
        player->PlayerTalkClass->ClearMenus();
        player->PlayerTalkClass->GetGossipMenu().SetMenuId(MENU_ID);
        uint32 idx = 0;
        auto& m = player->PlayerTalkClass->GetGossipMenu();

        m.AddMenuItem(int32(idx++), GOSSIP_ICON_BATTLE, "坦克（耐力/护甲/闪避）", SENDER_ROLE, ROLE_TANK, "", 0, false);
        m.AddMenuItem(int32(idx++), GOSSIP_ICON_BATTLE, "输出（主属性/爆击/急速）", SENDER_ROLE, ROLE_DPS, "", 0, false);
        m.AddMenuItem(int32(idx++), GOSSIP_ICON_BATTLE, "治疗（智力/精神/法强）", SENDER_ROLE, ROLE_HEAL, "", 0, false);
        m.AddMenuItem(int32(idx++), GOSSIP_ICON_CHAT,   "通用（不限定）",         SENDER_ROLE, ROLE_ANY,  "", 0, false);
        m.AddMenuItem(int32(idx++), GOSSIP_ICON_CHAT,   "<< 返回", SENDER_NAV, NAV_BACK, "", 0, false);
        m.AddMenuItem(int32(idx++), GOSSIP_ICON_CHAT,   "[ 关闭 ]", SENDER_NAV, NAV_CANCEL, "", 0, false);

        player->PlayerTalkClass->SendGossipMenu(DEFAULT_GOSSIP_MESSAGE, player->GetGUID());
        handler->SendSysMessage("|cffffcc00选择定位：|r");
    }

    inline void SendIlvlMenu(Player* player, ChatHandler* handler)
    {
        player->PlayerTalkClass->ClearMenus();
        player->PlayerTalkClass->GetGossipMenu().SetMenuId(MENU_ID);
        uint32 idx = 0;
        auto& m = player->PlayerTalkClass->GetGossipMenu();

        // 装等档位：覆盖 1~60 低级到 999+ 大数值，向下兼容
        struct IlvlOpt { uint32 v; char const* label; };
        IlvlOpt const levels[] = {
            {  20, "装等 <= 20    (1-20级新手)"    },
            {  40, "装等 <= 40    (20-40级)"       },
            {  60, "装等 <= 60    (40-60级)"       },
            {  90, "装等 <= 90    (60级 T1-T2)"    },
            { 120, "装等 <= 120   (60级 T3)"       },
            { 140, "装等 <= 140   (TBC 前期)"      },
            { 164, "装等 <= 164   (TBC T5)"        },
            { 200, "装等 <= 200   (WLK 入门)"      },
            { 219, "装等 <= 219   (纳克萨玛斯)"    },
            { 232, "装等 <= 232   (奥杜尔)"        },
            { 245, "装等 <= 245   (十字军)"        },
            { 264, "装等 <= 264   (ICC 25H)"       },
            { 284, "装等 <= 284   (ICC 传说)"      },
            { 500, "装等 <= 500   (自定义内容)"    },
            { 999, "装等 <= 999   (自定义高级)"    },
            {9999, "装等 <= 9999  (超凡内容)"      },
            {   0, "不限装等      (含大数值装备)"  },
        };
        char buf[96];
        for (auto const& lv : levels)
        {
            snprintf(buf, sizeof(buf), "%s", lv.label);
            m.AddMenuItem(int32(idx++), GOSSIP_ICON_MONEY_BAG, buf, SENDER_ILVL, lv.v, "", 0, false);
        }
        m.AddMenuItem(int32(idx++), GOSSIP_ICON_CHAT, "<< 返回", SENDER_NAV, NAV_BACK, "", 0, false);
        m.AddMenuItem(int32(idx++), GOSSIP_ICON_CHAT, "[ 关闭 ]", SENDER_NAV, NAV_CANCEL, "", 0, false);

        player->PlayerTalkClass->SendGossipMenu(DEFAULT_GOSSIP_MESSAGE, player->GetGUID());
        handler->SendSysMessage("|cffffcc00选择装等上限：|r");
    }

    inline void SendLoadMenu(Player* player, ChatHandler* handler)
    {
        uint32 accId = player->GetSession()->GetAccountId();
        GearSession& ss = s_sess[accId];
        ss.saveNames = GetSaveList(player);

        if (ss.saveNames.empty())
        {
            handler->SendSysMessage("|cffff0000还没有保存过任何方案。|r");
            handler->SendSysMessage("|cff00ccff用 .gearset save <名称> 保存当前装备|r");
            player->PlayerTalkClass->SendCloseGossip();
            return;
        }

        player->PlayerTalkClass->ClearMenus();
        player->PlayerTalkClass->GetGossipMenu().SetMenuId(MENU_ID);
        auto& m = player->PlayerTalkClass->GetGossipMenu();

        uint32 total = uint32(ss.saveNames.size());
        uint32 pages = (total + PER_PAGE - 1) / PER_PAGE;
        if (!pages) pages = 1;
        uint32 start = ss.page * PER_PAGE;
        uint32 end   = std::min(start + PER_PAGE, total);

        uint32 idx = 0;
        for (uint32 i = start; i < end; ++i)
            m.AddMenuItem(int32(idx++), GOSSIP_ICON_MONEY_BAG, ss.saveNames[i], SENDER_LOAD, i, "", 0, false);

        char nav[64];
        if (ss.page > 0)
        {
            snprintf(nav, sizeof(nav), "<< 上一页 (%u/%u)", ss.page, pages);
            m.AddMenuItem(int32(idx++), GOSSIP_ICON_CHAT, nav, SENDER_NAV, NAV_PREV, "", 0, false);
        }
        if (ss.page + 1 < pages)
        {
            snprintf(nav, sizeof(nav), ">> 下一页 (%u/%u)", ss.page + 2, pages);
            m.AddMenuItem(int32(idx++), GOSSIP_ICON_CHAT, nav, SENDER_NAV, NAV_NEXT, "", 0, false);
        }
        m.AddMenuItem(int32(idx++), GOSSIP_ICON_CHAT, "[ 关闭 ]", SENDER_NAV, NAV_CANCEL, "", 0, false);

        ASSERT(m.GetMenuItemCount() <= GOSSIP_HARD_LIMIT);

        player->PlayerTalkClass->SendGossipMenu(DEFAULT_GOSSIP_MESSAGE, player->GetGUID());
        handler->PSendSysMessage("|cffffcc00已保存方案 %u 个 — 第 %u/%u 页|r", total, ss.page + 1, pages);
    }

} // namespace GearSet

using namespace GearSet;

// ============================================================================
//  Gossip 回调
// ============================================================================
class gearset_gossip : public PlayerScript
{
public:
    gearset_gossip() : PlayerScript("gearset_gossip") { }

    void OnGossipSelect(Player* player, uint32 menuId, uint32 sender, uint32 action) override
    {
        if (menuId != MENU_ID)
            return;

        ChatHandler handler(player->GetSession());
        uint32 accId = player->GetSession()->GetAccountId();
        GearSession& ss = s_sess[accId];

        switch (sender)
        {
            case SENDER_NAV:
                if (action == NAV_CANCEL)
                {
                    ss.Clear();
                    player->PlayerTalkClass->SendCloseGossip();
                    return;
                }
                if (action == NAV_BACK)
                {
                    if (ss.step > 0) --ss.step;
                    if (ss.step == 0)      SendMainMenu(player, &handler);
                    else if (ss.step == 1) SendClassMenu(player, &handler);
                    else if (ss.step == 2) SendRoleMenu(player, &handler);
                    return;
                }
                if (action == NAV_PREV) { if (ss.page) --ss.page; SendLoadMenu(player, &handler); return; }
                if (action == NAV_NEXT) { ++ss.page; SendLoadMenu(player, &handler); return; }
                return;

            case SENDER_MAIN:
                if (action == 1) { ss.step = 1; SendClassMenu(player, &handler); }
                else if (action == 2)
                {
                    player->PlayerTalkClass->SendCloseGossip();
                    handler.SendSysMessage("|cff00ccff请用指令保存：.gearset save <方案名>|r");
                }
                else if (action == 3) { ss.page = 0; SendLoadMenu(player, &handler); }
                else if (action == 4)
                {
                    // 按当前位置自动配装：用玩家自己的职业
                    player->PlayerTalkClass->SendCloseGossip();
                    uint32 sugg = SuggestIlvlForPlayer(player);
                    handler.PSendSysMessage("|cffffcc00当前位置：%s  推荐装等：%u|r",
                        GetLocationDesc(player), sugg);
                    GenerateSet(&handler, player, player->GetClass(), ROLE_ANY, sugg, false);
                    ss.Clear();
                }
                return;

            case SENDER_CLASS:
                ss.cls  = uint8(action);
                ss.step = 2;
                SendRoleMenu(player, &handler);
                return;

            case SENDER_ROLE:
                ss.role = GearRole(action);
                ss.step = 3;
                SendIlvlMenu(player, &handler);
                return;

            case SENDER_ILVL:
                ss.ilvl = action;
                player->PlayerTalkClass->SendCloseGossip();
                GenerateSet(&handler, player, ss.cls, ss.role, ss.ilvl, false);
                ss.Clear();
                return;

            case SENDER_LOAD:
                if (action < ss.saveNames.size())
                {
                    std::string name = ss.saveNames[action];
                    player->PlayerTalkClass->SendCloseGossip();
                    LoadGearSet(&handler, player, name);
                }
                ss.Clear();
                return;

            default:
                return;
        }
    }
};

// ============================================================================
//  指令
// ============================================================================
class gearset_commandscript : public CommandScript
{
public:
    gearset_commandscript() : CommandScript("gearset_commandscript") { }

    std::vector<ChatCommand> GetCommands() const override
    {
        static std::vector<ChatCommand> table =
        {
            { "gearset", rbac::RBAC_PERM_COMMAND_GEARSET, false, &HandleGearSet, "" },
        };
        return table;
    }

    static bool HandleGearSet(ChatHandler* handler, char const* args)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
        {
            handler->SendSysMessage("此命令只能在游戏内使用。");
            handler->SetSentErrorMessage(true);
            return false;
        }

        // 无参数 -> 打开主菜单
        if (!*args)
        {
            s_sess[player->GetSession()->GetAccountId()].Clear();
            SendMainMenu(player, handler);
            return true;
        }

        std::string input = args;
        std::vector<std::string> tok;
        {
            std::string cur;
            for (char c : input)
            {
                if (c == ' ') { if (!cur.empty()) tok.push_back(cur); cur.clear(); }
                else cur += c;
            }
            if (!cur.empty()) tok.push_back(cur);
        }
        if (tok.empty())
        {
            SendMainMenu(player, handler);
            return true;
        }

        std::string cmd = tok[0];
        std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

        // ---- save ----
        if (cmd == "save")
        {
            if (tok.size() < 2)
            {
                handler->SendSysMessage("用法: .gearset save <方案名>");
                handler->SetSentErrorMessage(true);
                return false;
            }
            std::string name = input.substr(input.find(tok[1]));
            SaveCurrentGear(handler, player, name);
            return true;
        }

        // ---- load ----
        if (cmd == "load")
        {
            if (tok.size() < 2)
            {
                s_sess[player->GetSession()->GetAccountId()].page = 0;
                SendLoadMenu(player, handler);
                return true;
            }
            std::string name = input.substr(input.find(tok[1]));
            LoadGearSet(handler, player, name);
            return true;
        }

        // ---- list ----
        if (cmd == "list")
        {
            auto names = GetSaveList(player);
            if (names.empty())
            {
                handler->SendSysMessage("还没有保存过任何方案。");
                return true;
            }
            handler->PSendSysMessage("|cffffcc00已保存 %u 个方案：|r", uint32(names.size()));
            for (auto const& n : names)
                handler->PSendSysMessage("  |cff00ff00%s|r", n.c_str());
            return true;
        }

        // ---- del ----
        if (cmd == "del" || cmd == "delete")
        {
            if (tok.size() < 2)
            {
                handler->SendSysMessage("用法: .gearset del <方案名>");
                handler->SetSentErrorMessage(true);
                return false;
            }
            std::string name = input.substr(input.find(tok[1]));
            std::string safe = name;
            CharacterDatabase.EscapeString(safe);
            CharacterDatabase.PExecute("DELETE FROM custom_gearset WHERE owner_guid = {} AND set_name = '{}'",
                player->GetGUID().GetCounter(), safe);
            handler->PSendSysMessage("|cff00ff00已删除方案|r「%s」", name.c_str());
            return true;
        }

        // ---- strip ----
        if (cmd == "strip")
        {
            uint32 n = 0;
            for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
            {
                if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
                {
                    ItemPosCountVec dest;
                    if (player->CanStoreItem(NULL_BAG, NULL_SLOT, dest, item, false) == EQUIP_ERR_OK)
                    {
                        player->RemoveItem(INVENTORY_SLOT_BAG_0, slot, true);
                        player->StoreItem(dest, item, true);
                        ++n;
                    }
                }
            }
            handler->PSendSysMessage("|cff00ff00已卸下 %u 件装备到背包|r", n);
            return true;
        }

        // ---- auto / here : 按当前副本自动配装 ----
        if (cmd == "auto" || cmd == "here")
        {
            uint32 sugg = SuggestIlvlForPlayer(player);
            GearRole role = ROLE_ANY;
            if (tok.size() >= 2)
                role = ParseRole(tok[1]);
            handler->PSendSysMessage("|cffffcc00当前位置：%s   推荐装等：%u|r",
                GetLocationDesc(player), sugg);
            GenerateSet(handler, player, player->GetClass(), role, sugg, false);
            return true;
        }

        // ---- <职业> <装等> [定位] ----
        uint8 cls = ParseClass(tok[0]);
        if (!cls)
        {
            handler->SendSysMessage("用法：");
            handler->SendSysMessage("  .gearset                     打开菜单");
            handler->SendSysMessage("  .gearset auto                按当前副本自动配装");
            handler->SendSysMessage("  .gearset auto tank           同上，指定定位");
            handler->SendSysMessage("  .gearset 战士 264            指定职业+装等");
            handler->SendSysMessage("  .gearset 圣骑士 264 tank     指定定位");
            handler->SendSysMessage("  .gearset save <名称>         保存当前装备");
            handler->SendSysMessage("  .gearset load [名称]         加载方案");
            handler->SendSysMessage("  .gearset list                列出方案");
            handler->SendSysMessage("  .gearset del <名称>          删除方案");
            handler->SendSysMessage("  .gearset strip               卸下全身装备");
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint32 ilvl = 0;
        GearRole role = ROLE_ANY;
        if (tok.size() >= 2)
            ilvl = uint32(atoi(tok[1].c_str()));
        if (tok.size() >= 3)
            role = ParseRole(tok[2]);

        GenerateSet(handler, player, cls, role, ilvl, false);
        return true;
    }
};

void AddSC_gearset_commandscript()
{
    new gearset_commandscript();
    new gearset_gossip();
}
