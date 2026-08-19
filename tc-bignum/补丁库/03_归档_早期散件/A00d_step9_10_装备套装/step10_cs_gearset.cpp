/*
 * ============================================================================
 *  套装系统 v3 —— cs_gearset.cpp
 * ============================================================================
 *
 *  相对 v2 的改动（本次全部新增）：
 *    1. 真 ItemSet 套装      —— 读 ItemSet.dbc，发的是成套的，不再是散件拼凑
 *    2. 刷本解锁             —— 击杀副本末王计 1 次，攒够次数必掉全套
 *    3. 职业套装开关         —— 默认关闭，.gearset tier on 打开
 *    4. 自动补足前置         —— 声望刷满、专业点满，装备本身不改
 *    5. 装备预览与对比       —— 发之前先看，含装等/属性差值
 *    6. 一键穿戴             —— .gearset ... equip
 *    7. 武器套装             —— .gearset weapon
 *    8. 饰品套装             —— .gearset trinket
 *    9. NPCBot 配装          —— .gearset bot
 *   10. 进度查询             —— .gearset progress
 *   11. 自动宝石附魔         —— 发装备时自动打孔镶嵌
 *   12. 套装收藏册           —— .gearset book 看已解锁的
 *
 *  ── 指令总览 ──────────────────────────────────────────────────────────
 *   .gearset                        打开主菜单（可点击）
 *   .gearset <职业> <装等>          按职业+装等生成整套散件
 *   .gearset <职业> <装等> <定位>   定位：tank/dps/heal
 *   .gearset <职业> <装等> equip    生成并直接穿上
 *   .gearset tier                   弹出已解锁的职业套装菜单
 *   .gearset tier on|off            职业套装开关（默认关）
 *   .gearset tier <套装ID>          直接发指定套装
 *   .gearset progress               看所有副本的刷本进度
 *   .gearset book                   套装收藏册（已解锁清单）
 *   .gearset preview <职业> <装等>  只预览不发放
 *   .gearset weapon <职业> <装等>   武器套装
 *   .gearset trinket <职业> <装等>  饰品套装
 *   .gearset bot <职业> <装等>      给选中的 NPCBot 配装
 *   .gearset save/load/list/del     方案存取（v2 已有）
 *   .gearset strip                  卸下全身装备
 *   .gearset config                 查看/修改个人开关
 *
 *  放置：D:\TrinityCore\src\server\scripts\Commands\cs_gearset.cpp
 *  需要先执行 SQL：05 06 07 08 09
 *  新增源文件必须重跑 CMake
 * ============================================================================
 */

#include "ScriptMgr.h"
#include "Chat.h"
#include "Config.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "RBAC.h"
#include "World.h"
#include "WorldSession.h"
#include "Language.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "GossipDef.h"
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "DBCStructure.h"
#include "Map.h"
#include "Creature.h"
#include "Group.h"
#include "ReputationMgr.h"
#include "SpellMgr.h"
#include "Util.h"
#include <vector>
#include <string>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <set>

namespace GearSet
{
    // ======================================================================
    //  常量
    // ======================================================================
    static constexpr uint32 GOSSIP_HARD_LIMIT = 32;   // GossipDef.cpp:42 的 ASSERT 上限
    static constexpr uint32 NAV_SLOTS         = 3;
    static constexpr uint32 PER_PAGE          = GOSSIP_HARD_LIMIT - NAV_SLOTS;  // 29
    static constexpr uint32 MENU_ID           = 60100;

    // sender 分类
    static constexpr uint32 SENDER_MAIN    = 1;
    static constexpr uint32 SENDER_CLASS   = 2;
    static constexpr uint32 SENDER_ROLE    = 3;
    static constexpr uint32 SENDER_ILVL    = 4;
    static constexpr uint32 SENDER_LOAD    = 5;
    static constexpr uint32 SENDER_TIER    = 6;   // 职业套装列表
    static constexpr uint32 SENDER_PROG    = 7;   // 刷本进度
    static constexpr uint32 SENDER_PREVIEW = 8;   // 预览确认
    static constexpr uint32 SENDER_NAV     = 9;
    static constexpr uint32 SENDER_CONFIG  = 10;  // 配置开关
    static constexpr uint32 SENDER_BOOK    = 11;  // 收藏册

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

    enum GearMode : uint8
    {
        MODE_PIECES  = 0,   // 散件
        MODE_TIER    = 1,   // 职业套装
        MODE_WEAPON  = 2,   // 武器
        MODE_TRINKET = 3,   // 饰品
    };

    // ======================================================================
    //  会话
    // ======================================================================
    struct GearSession
    {
        uint8       step      = 0;
        uint8       cls       = 0;
        GearRole    role      = ROLE_ANY;
        uint32      ilvl      = 0;
        bool        autoEquip = false;
        GearMode    mode      = MODE_PIECES;
        uint32      page      = 0;
        std::vector<std::string> saveNames;
        std::vector<uint32>      tierIds;      // tier 菜单用
        std::vector<uint32>      pendingItems; // 预览待确认的物品
        void Clear() { *this = GearSession(); }
    };

    static std::unordered_map<uint32, GearSession> s_sess;

    inline GearSession& Sess(Player* p) { return s_sess[p->GetGUID().GetCounter()]; }

    // ======================================================================
    //  角色配置（缓存，避免每次查库）
    // ======================================================================
    struct GearConfig
    {
        bool tierEnabled = false;   // 职业套装开关，默认关
        bool autoEquip   = false;
        bool autoGem     = true;
        bool grantReq    = true;    // 自动补足声望/专业
        bool loaded      = false;
    };

    static std::unordered_map<uint32, GearConfig> s_cfg;

    inline GearConfig& GetConfig(Player* player)
    {
        uint32 guid = player->GetGUID().GetCounter();
        GearConfig& c = s_cfg[guid];
        if (c.loaded)
            return c;

        // fmt 风格占位符 {} —— 绝不能用 %u，会抛 fmt::format_error 崩服
        if (QueryResult r = CharacterDatabase.PQuery(
                "SELECT tier_enabled, auto_equip, auto_gem, grant_req "
                "FROM custom_gearset_config WHERE owner_guid = {}", guid))
        {
            Field* f = r->Fetch();
            c.tierEnabled = f[0].GetUInt8() != 0;
            c.autoEquip   = f[1].GetUInt8() != 0;
            c.autoGem     = f[2].GetUInt8() != 0;
            c.grantReq    = f[3].GetUInt8() != 0;
        }
        c.loaded = true;
        return c;
    }

    inline void SaveConfig(Player* player)
    {
        uint32 guid = player->GetGUID().GetCounter();
        GearConfig& c = s_cfg[guid];
        CharacterDatabase.PExecute(
            "REPLACE INTO custom_gearset_config "
            "(owner_guid, tier_enabled, auto_equip, auto_gem, grant_req) "
            "VALUES ({}, {}, {}, {}, {})",
            guid, c.tierEnabled ? 1 : 0, c.autoEquip ? 1 : 0,
            c.autoGem ? 1 : 0, c.grantReq ? 1 : 0);
    }

    // ======================================================================
    //  职业 / 护甲 / 定位 辅助
    // ======================================================================
    /*
     * 职业【满级时】能穿的最高护甲类型。
     * 注意：低等级不能直接用这个，见下面的 GetArmorSubClassForLevel()。
     */
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
                return ITEM_SUBCLASS_ARMOR_CLOTH;
            default:
                return ITEM_SUBCLASS_ARMOR_CLOTH;
        }
    }

    /*
     * ========================================================================
     *  按【实际等级】决定护甲类型（用户实测提出的问题）
     * ========================================================================
     *
     *  3.3.5 的护甲精通是【40 级才学】的：
     *      战士/圣骑    1-39 级只能穿锁甲 -> 40 级学板甲精通(SKILL_PLATE_MAIL=293)
     *      猎人/萨满    1-39 级只能穿皮甲 -> 40 级学锁甲精通(SKILL_MAIL=413)
     *      死骑         出生即 55 级，直接板甲
     *      盗贼/德鲁伊  全程皮甲
     *      牧师/法师/术士 全程布甲
     *
     *  旧版写死"战士=板甲"，导致 20 级战士拿到一身穿不上的板甲
     *  （等级过滤放行了，但护甲精通没学，装备栏里是灰的）。
     *
     *  这里优先【实际查玩家学没学】，查不到再按等级推断 —— 
     *  这样自定义服改过技能学习等级也能自动适配。
     *
     *  API 已核实：
     *    Player.h:1785  HasSkill(uint32)         public
     *    SharedDefines.h:3056  SKILL_PLATE_MAIL = 293
     *    SharedDefines.h:3067  SKILL_MAIL       = 413
     */
    inline uint32 GetArmorSubClassForLevel(Player const* player, uint8 cls)
    {
        uint32 top = GetArmorSubClassForClass(cls);

        if (!player)
            return top;

        // 布甲和皮甲职业没有等级门槛，直接返回
        if (top == ITEM_SUBCLASS_ARMOR_CLOTH || top == ITEM_SUBCLASS_ARMOR_LEATHER)
            return top;

        Player* p = const_cast<Player*>(player);

        if (top == ITEM_SUBCLASS_ARMOR_PLATE)
        {
            // 学了板甲精通就发板甲，没学就退回锁甲
            if (p->HasSkill(SKILL_PLATE_MAIL))
                return ITEM_SUBCLASS_ARMOR_PLATE;
            return ITEM_SUBCLASS_ARMOR_MAIL;
        }

        if (top == ITEM_SUBCLASS_ARMOR_MAIL)
        {
            // 猎人/萨满：学了锁甲精通才发锁甲，否则皮甲
            if (p->HasSkill(SKILL_MAIL))
                return ITEM_SUBCLASS_ARMOR_MAIL;
            return ITEM_SUBCLASS_ARMOR_LEATHER;
        }

        return top;
    }

    // 护甲类型的中文名（提示用）
    inline char const* GetArmorSubName(uint32 sub)
    {
        switch (sub)
        {
            case ITEM_SUBCLASS_ARMOR_PLATE:   return "板甲";
            case ITEM_SUBCLASS_ARMOR_MAIL:    return "锁甲";
            case ITEM_SUBCLASS_ARMOR_LEATHER: return "皮甲";
            case ITEM_SUBCLASS_ARMOR_CLOTH:   return "布甲";
            default:                          return "护甲";
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

    inline uint8 ParseClassName(std::string const& s)
    {
        if (s == "战士"   || s == "warrior") return CLASS_WARRIOR;
        if (s == "圣骑士" || s == "圣骑" || s == "paladin") return CLASS_PALADIN;
        if (s == "猎人"   || s == "hunter")  return CLASS_HUNTER;
        if (s == "潜行者" || s == "盗贼" || s == "rogue") return CLASS_ROGUE;
        if (s == "牧师"   || s == "priest")  return CLASS_PRIEST;
        if (s == "死亡骑士" || s == "死骑" || s == "dk" || s == "deathknight") return CLASS_DEATH_KNIGHT;
        if (s == "萨满"   || s == "萨满祭司" || s == "shaman") return CLASS_SHAMAN;
        if (s == "法师"   || s == "mage")    return CLASS_MAGE;
        if (s == "术士"   || s == "warlock") return CLASS_WARLOCK;
        if (s == "德鲁伊" || s == "小德" || s == "druid") return CLASS_DRUID;
        return 0;
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

    inline GearRole ParseRole(std::string const& s)
    {
        if (s == "tank" || s == "坦克" || s == "t")  return ROLE_TANK;
        if (s == "heal" || s == "治疗" || s == "h")  return ROLE_HEAL;
        if (s == "dps"  || s == "输出" || s == "d")  return ROLE_DPS;
        return ROLE_ANY;
    }

    // ======================================================================
    //  定位评分
    // ======================================================================
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

    // ======================================================================
    //  【新增】自动补足前置条件
    //  —— 用户明确要求：拿到套装时不要被声望/专业卡住
    //  做法：把需要的声望刷满、专业技能点到位。装备本身不改，
    //        所以其他玩家、其他途径拿到同款装备行为完全一致。
    // ======================================================================
    inline void GrantRequirements(Player* player, ItemTemplate const* proto, ChatHandler* handler)
    {
        if (!proto || !player)
            return;

        // ---- 1. 声望 ----
        // Player.cpp:11617  CanUseItem() 里检查 GetReputationRank() < RequiredReputationRank
        if (proto->RequiredReputationFaction && proto->RequiredReputationRank)
        {
            FactionEntry const* fe = sFactionStore.LookupEntry(proto->RequiredReputationFaction);
            // ReputationIndex < 0 的阵营不参与声望系统，跳过（参考 cs_modify.cpp:717）
            if (fe && fe->ReputationIndex >= 0)
            {
                uint32 cur = uint32(player->GetReputationRank(proto->RequiredReputationFaction));
                if (cur < proto->RequiredReputationRank)
                {
                    // 崇拜 = 42000，直接给满，一劳永逸
                    // Player::SetReputation(uint32 factionentry, uint32 value)  Player.h:1462
                    player->SetReputation(proto->RequiredReputationFaction, 42000);
                    if (handler)
                        handler->PSendSysMessage("  |cff00ff00[前置]|r 声望已补足：%s -> 崇拜",
                                                 fe->Name[handler->GetSessionDbcLocale()]);
                }
            }
        }

        // ---- 2. 专业/武器技能 ----
        // Player.cpp:11642  CanUseItem() 里检查 GetSkillValue() < RequiredSkillRank
        if (proto->RequiredSkill)
        {
            uint16 need = uint16(proto->RequiredSkillRank ? proto->RequiredSkillRank : 1);
            uint16 cur  = player->GetSkillValue(proto->RequiredSkill);
            if (cur < need)
            {
                uint16 target = std::max<uint16>(need, 450);   // 直接顶到 450
                player->SetSkill(proto->RequiredSkill, 0, target, target);
                if (handler)
                    handler->PSendSysMessage("  |cff00ff00[前置]|r 技能已补足：ID %u -> %u",
                                             proto->RequiredSkill, target);
            }
        }

        // ---- 3. 需要的法术（如某些装备要求学会特定技能）----
        if (proto->RequiredSpell && !player->HasSpell(proto->RequiredSpell))
        {
            player->LearnSpell(proto->RequiredSpell, false);
            if (handler)
                handler->PSendSysMessage("  |cff00ff00[前置]|r 已学会法术：%u", proto->RequiredSpell);
        }
    }

    // ======================================================================
    //  【新增】ItemSet 真套装 —— 读 ItemSet.dbc
    //  用户反馈："散件大多有点乱，希望有对应的套装"
    //  ItemSetEntry 结构（DBCStructure.h:960）：
    //      char const* Name[16];
    //      uint32 ItemID[MAX_ITEM_SET_ITEMS];      // 10 件
    //      uint32 SetSpellID[MAX_ITEM_SET_SPELLS]; // 8 个套装效果
    //      uint32 SetThreshold[MAX_ITEM_SET_SPELLS];
    //      uint32 RequiredSkill / RequiredSkillRank;
    // ======================================================================
    struct TierSetInfo
    {
        uint32      setId    = 0;
        std::string name;
        uint32      classMask = 0;   // 该套装适用的职业位掩码
        uint32      avgIlvl   = 0;
        uint32      pieceCount = 0;
        std::vector<uint32> items;
    };

    // 全局缓存：启动后第一次访问时构建
    static std::vector<TierSetInfo> s_tierCache;
    static bool s_tierCacheBuilt = false;

    inline void BuildTierCache(uint32 dbcLocale)
    {
        if (s_tierCacheBuilt)
            return;

        s_tierCache.clear();

        for (uint32 i = 0; i < sItemSetStore.GetNumRows(); ++i)
        {
            ItemSetEntry const* set = sItemSetStore.LookupEntry(i);
            if (!set)
                continue;

            TierSetInfo info;
            info.setId = i;
            info.name  = set->Name[dbcLocale] ? set->Name[dbcLocale] : "";
            if (info.name.empty())
                continue;

            uint64 ilvlSum = 0;
            uint32 clsMaskAcc = 0;

            for (uint32 k = 0; k < MAX_ITEM_SET_ITEMS; ++k)
            {
                uint32 itemId = set->ItemID[k];
                if (!itemId)
                    continue;

                ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
                if (!proto)
                    continue;

                info.items.push_back(itemId);
                ilvlSum += proto->ItemLevel;
                clsMaskAcc |= proto->AllowableClass;
            }

            info.pieceCount = uint32(info.items.size());
            if (info.pieceCount == 0)
                continue;

            info.avgIlvl   = uint32(ilvlSum / info.pieceCount);
            info.classMask = clsMaskAcc;
            s_tierCache.push_back(std::move(info));
        }

        // 按装等排序，方便按档位查找
        std::sort(s_tierCache.begin(), s_tierCache.end(),
                  [](TierSetInfo const& a, TierSetInfo const& b) { return a.avgIlvl < b.avgIlvl; });

        s_tierCacheBuilt = true;
    }

    // 找出适合某职业+装等区间的套装
    inline std::vector<TierSetInfo const*> FindTierSets(uint8 cls, uint32 ilvlMin, uint32 ilvlMax, uint32 dbcLocale)
    {
        BuildTierCache(dbcLocale);

        std::vector<TierSetInfo const*> out;
        uint32 clsMask = 1u << (cls - 1);

        for (TierSetInfo const& t : s_tierCache)
        {
            // classMask 为 -1 (全职业) 的套装不算职业套装
            if (t.classMask != uint32(-1) && !(t.classMask & clsMask))
                continue;
            if (ilvlMin && t.avgIlvl < ilvlMin)
                continue;
            if (ilvlMax && t.avgIlvl > ilvlMax)
                continue;
            if (t.pieceCount < 3)   // 少于 3 件的不当套装
                continue;

            out.push_back(&t);
        }
        return out;
    }

    // ======================================================================
    //  【新增】刷本进度
    // ======================================================================
    struct DungeonReq
    {
        uint32      mapId      = 0;
        uint8       difficulty = 0;
        uint32      needKills  = 10;
        std::string setIds;
        uint32      ilvlMin    = 0;
        uint32      ilvlMax    = 0;
        std::string name;
    };

    static std::unordered_map<uint64, DungeonReq> s_dungeonReq;   // key = mapId<<8 | diff
    static bool s_dungeonReqLoaded = false;

    inline uint64 DungeonKey(uint32 mapId, uint8 diff) { return (uint64(mapId) << 8) | diff; }

    inline void LoadDungeonReq()
    {
        if (s_dungeonReqLoaded)
            return;

        s_dungeonReq.clear();

        if (QueryResult r = WorldDatabase.Query(
                "SELECT map_id, difficulty, need_kills, set_ids, ilvl_min, ilvl_max, dungeon_name "
                "FROM custom_tierset_dungeon"))
        {
            do
            {
                Field* f = r->Fetch();
                DungeonReq d;
                d.mapId      = f[0].GetUInt32();
                d.difficulty = f[1].GetUInt8();
                d.needKills  = f[2].GetUInt32();
                d.setIds     = f[3].GetString();
                d.ilvlMin    = f[4].GetUInt32();
                d.ilvlMax    = f[5].GetUInt32();
                d.name       = f[6].GetString();
                s_dungeonReq[DungeonKey(d.mapId, d.difficulty)] = std::move(d);
            } while (r->NextRow());
        }

        s_dungeonReqLoaded = true;
    }

    inline uint32 GetKillCount(uint32 guid, uint32 mapId, uint8 diff)
    {
        if (QueryResult r = CharacterDatabase.PQuery(
                "SELECT kill_count FROM custom_raid_progress "
                "WHERE owner_guid = {} AND map_id = {} AND difficulty = {}",
                guid, mapId, uint32(diff)))
            return r->Fetch()[0].GetUInt32();
        return 0;
    }

    inline void AddKillCount(uint32 guid, uint32 mapId, uint8 diff)
    {
        CharacterDatabase.PExecute(
            "INSERT INTO custom_raid_progress (owner_guid, map_id, difficulty, kill_count, last_kill) "
            "VALUES ({}, {}, {}, 1, UNIX_TIMESTAMP()) "
            "ON DUPLICATE KEY UPDATE kill_count = kill_count + 1, last_kill = UNIX_TIMESTAMP()",
            guid, mapId, uint32(diff));
    }

    inline bool IsSetUnlocked(uint32 guid, uint32 setId)
    {
        if (QueryResult r = CharacterDatabase.PQuery(
                "SELECT 1 FROM custom_tierset_unlocked WHERE owner_guid = {} AND set_id = {}",
                guid, setId))
            return true;
        return false;
    }

    inline void UnlockSet(uint32 guid, uint32 setId)
    {
        CharacterDatabase.PExecute(
            "INSERT IGNORE INTO custom_tierset_unlocked (owner_guid, set_id, unlock_time) "
            "VALUES ({}, {}, UNIX_TIMESTAMP())",
            guid, setId);
    }

    // ======================================================================
    //  【新增】自动镶嵌宝石 —— 把装备上的插槽填满
    // ======================================================================
    // 常用满级宝石（3.3.5 强化类，全都是无绑定的普通宝石）
    static constexpr uint32 GEM_RED    = 39996;  // 强效正义之石 +20力量
    static constexpr uint32 GEM_BLUE   = 40008;  // 坚固恒金 +30耐力
    static constexpr uint32 GEM_YELLOW = 40014;  // 精巧翡翠 +20敏捷
    static constexpr uint32 GEM_META   = 41398;  // relentless earthsiege

    inline uint32 PickGemForSocket(uint32 socketColor, GearRole role)
    {
        switch (socketColor)
        {
            case SOCKET_COLOR_META:   return GEM_META;
            case SOCKET_COLOR_RED:    return role == ROLE_HEAL ? GEM_BLUE : GEM_RED;
            case SOCKET_COLOR_BLUE:   return GEM_BLUE;
            case SOCKET_COLOR_YELLOW: return role == ROLE_TANK ? GEM_BLUE : GEM_YELLOW;
            default:                  return GEM_BLUE;
        }
    }

    // ======================================================================
    //  发放一件物品（核心函数，所有发放路径都走这里）
    // ======================================================================
    struct GrantResult
    {
        uint32 ok      = 0;
        uint32 failed  = 0;
        uint32 equipped = 0;
    };

    inline bool GrantOneItem(Player* player, uint32 itemId, bool autoEquip,
                             ChatHandler* handler, GearRole /*role*/, GrantResult& res)
    {
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
        if (!proto)
        {
            ++res.failed;
            return false;
        }

        GearConfig& cfg = GetConfig(player);

        // 先补前置，否则 CanEquipNewItem 会被声望/专业挡下
        if (cfg.grantReq)
            GrantRequirements(player, proto, nullptr);

        // ---- 尝试穿戴 ----
        if (autoEquip)
        {
            uint16 eDest = 0;
            InventoryResult eMsg = player->CanEquipNewItem(NULL_SLOT, eDest, itemId, false);
            if (eMsg == EQUIP_ERR_OK)
            {
                if (Item* it = player->EquipNewItem(eDest, itemId, true))
                {
                    it->SetBinding(true);
                    player->SendNewItem(it, 1, true, false);
                    ++res.ok;
                    ++res.equipped;
                    return true;
                }
            }
            // 穿不上就退回背包，不报错
        }

        // ---- 放背包 ----
        ItemPosCountVec dest;
        InventoryResult msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemId, 1);
        if (msg != EQUIP_ERR_OK)
        {
            if (handler)
                handler->PSendSysMessage("  |cffff0000背包已满|r，未能发放：%s", proto->Name1.c_str());
            ++res.failed;
            return false;
        }

        if (Item* it = player->StoreNewItem(dest, itemId, true, GenerateItemRandomPropertyId(itemId)))
        {
            player->SendNewItem(it, 1, true, false);
            ++res.ok;
            return true;
        }

        ++res.failed;
        return false;
    }

    // ======================================================================
    //  【新增】发放一整套 ItemSet 真套装
    // ======================================================================
    inline void GrantTierSet(ChatHandler* handler, Player* player,
                             TierSetInfo const* tier, bool autoEquip, GearRole role)
    {
        if (!tier)
            return;

        handler->PSendSysMessage("|cff00ccff========== 发放套装 ==========|r");
        handler->PSendSysMessage("|cffffcc00%s|r  (%u 件 / 平均装等 %u)",
                                 tier->name.c_str(), tier->pieceCount, tier->avgIlvl);

        GrantResult res;
        for (uint32 itemId : tier->items)
            GrantOneItem(player, itemId, autoEquip, handler, role, res);

        handler->PSendSysMessage("|cff00ff00完成|r：发放 %u 件，穿戴 %u 件，失败 %u 件",
                                 res.ok, res.equipped, res.failed);

        // 列出套装效果，让玩家知道凑齐几件有什么用
        if (ItemSetEntry const* raw = sItemSetStore.LookupEntry(tier->setId))
        {
            bool hasBonus = false;
            for (uint32 i = 0; i < MAX_ITEM_SET_SPELLS; ++i)
            {
                if (!raw->SetSpellID[i])
                    continue;
                if (!hasBonus)
                {
                    handler->PSendSysMessage("|cff00ccff--- 套装效果 ---|r");
                    hasBonus = true;
                }
                handler->PSendSysMessage("  %u 件套：法术 |cffffff00%u|r",
                                         raw->SetThreshold[i], raw->SetSpellID[i]);
            }
        }
    }

    // ======================================================================
    //  【新增】预览：只看不发
    // ======================================================================
    inline void PreviewItems(ChatHandler* handler, Player* /*player*/,
                             std::vector<uint32> const& items, char const* title)
    {
        handler->PSendSysMessage("|cff00ccff===== 预览：%s =====|r", title);

        uint64 totalIlvl = 0;
        uint32 count = 0;

        for (uint32 itemId : items)
        {
            ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
            if (!proto)
                continue;

            ++count;
            totalIlvl += proto->ItemLevel;

            // 品质颜色
            char const* color = "|cffffffff";
            switch (proto->Quality)
            {
                case ITEM_QUALITY_UNCOMMON:  color = "|cff1eff00"; break;
                case ITEM_QUALITY_RARE:      color = "|cff0070dd"; break;
                case ITEM_QUALITY_EPIC:      color = "|cffa335ee"; break;
                case ITEM_QUALITY_LEGENDARY: color = "|cffff8000"; break;
                default: break;
            }

            // 标出会被自动补足的前置
            std::string req;
            if (proto->RequiredReputationFaction && proto->RequiredReputationRank)
                req += " |cffff8800[需声望-将自动补足]|r";
            if (proto->RequiredSkill)
                req += " |cffff8800[需专业-将自动补足]|r";

            handler->PSendSysMessage("  %s|Hitem:%u:0:0:0:0:0:0:0:0|h[%s]|h|r  装等%u%s",
                                     color, itemId, proto->Name1.c_str(),
                                     proto->ItemLevel, req.c_str());
        }

        if (count)
            handler->PSendSysMessage("|cff00ff00共 %u 件，平均装等 %u|r",
                                     count, uint32(totalIlvl / count));
        handler->PSendSysMessage("确认发放请用同样指令加 |cffffff00confirm|r");
    }

    // ======================================================================
    //  InventoryType -> 需要几件
    // ======================================================================
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

    // 【新增】武器需求：按职业给对应武器类型
    struct WeaponNeed { uint32 invType; uint32 subClass; uint32 count; char const* name; };

    inline std::vector<WeaponNeed> GetWeaponNeeds(uint8 cls, GearRole role)
    {
        switch (cls)
        {
            case CLASS_WARRIOR:
                if (role == ROLE_TANK)
                    return { { INVTYPE_WEAPONMAINHAND, ITEM_SUBCLASS_WEAPON_SWORD, 1, "单手剑" },
                             { INVTYPE_SHIELD,         ITEM_SUBCLASS_ARMOR_SHIELD, 1, "盾牌" } };
                return { { INVTYPE_2HWEAPON, ITEM_SUBCLASS_WEAPON_AXE2, 1, "双手斧" } };
            case CLASS_PALADIN:
                if (role == ROLE_TANK || role == ROLE_HEAL)
                    return { { INVTYPE_WEAPONMAINHAND, ITEM_SUBCLASS_WEAPON_MACE, 1, "单手锤" },
                             { INVTYPE_SHIELD,         ITEM_SUBCLASS_ARMOR_SHIELD, 1, "盾牌" } };
                return { { INVTYPE_2HWEAPON, ITEM_SUBCLASS_WEAPON_MACE2, 1, "双手锤" } };
            case CLASS_DEATH_KNIGHT:
                return { { INVTYPE_2HWEAPON, ITEM_SUBCLASS_WEAPON_AXE2, 1, "双手斧" } };
            case CLASS_HUNTER:
                return { { INVTYPE_RANGED,  ITEM_SUBCLASS_WEAPON_BOW,  1, "弓" },
                         { INVTYPE_2HWEAPON, ITEM_SUBCLASS_WEAPON_AXE2, 1, "双手斧" } };
            case CLASS_ROGUE:
                return { { INVTYPE_WEAPONMAINHAND, ITEM_SUBCLASS_WEAPON_DAGGER, 1, "主手匕首" },
                         { INVTYPE_WEAPONOFFHAND,  ITEM_SUBCLASS_WEAPON_DAGGER, 1, "副手匕首" } };
            case CLASS_SHAMAN:
                if (role == ROLE_HEAL)
                    return { { INVTYPE_WEAPONMAINHAND, ITEM_SUBCLASS_WEAPON_MACE, 1, "单手锤" },
                             { INVTYPE_SHIELD,         ITEM_SUBCLASS_ARMOR_SHIELD, 1, "盾牌" } };
                return { { INVTYPE_WEAPONMAINHAND, ITEM_SUBCLASS_WEAPON_AXE, 1, "主手斧" },
                         { INVTYPE_WEAPONOFFHAND,  ITEM_SUBCLASS_WEAPON_AXE, 1, "副手斧" } };
            case CLASS_DRUID:
                return { { INVTYPE_2HWEAPON, ITEM_SUBCLASS_WEAPON_STAFF, 1, "法杖" } };
            case CLASS_PRIEST:
            case CLASS_MAGE:
            case CLASS_WARLOCK:
                return { { INVTYPE_2HWEAPON, ITEM_SUBCLASS_WEAPON_STAFF, 1, "法杖" },
                         { INVTYPE_RANGEDRIGHT,     ITEM_SUBCLASS_WEAPON_WAND,  1, "魔杖" } };
            default:
                return { { INVTYPE_2HWEAPON, ITEM_SUBCLASS_WEAPON_STAFF, 1, "法杖" } };
        }
    }

    // ======================================================================
    //  挑选最合适的散件
    // ======================================================================

    /*
     * 按等级过滤的开关与上限计算。
     *
     * 问题：原来只筛 ItemLevel（装等），不看 RequiredLevel（需求等级），
     * 结果 20 级角色会拿到一堆 80 级才能穿的装备。
     *
     * GearSet.MatchLevel = 1（默认）时，只发当前等级穿得上的。
     */
    inline bool MatchLevelEnabled()
    {
        return sConfigMgr->GetBoolDefault("GearSet.MatchLevel", true);
    }

    // 返回可用的最高需求等级；0 = 不限制
    inline uint32 LevelCapFor(Player const* player)
    {
        if (!player || !MatchLevelEnabled())
            return 0;
        return player->GetLevel();
    }

    inline uint32 PickBestItem(uint8 cls, GearRole role, uint32 ilvlCap,
                               uint32 invType, uint32 armorSub,
                               std::unordered_set<uint32> const& exclude,
                               uint32 playerLevel = 0)
    {
        uint32 clsMask = 1u << (cls - 1);
        uint32 best = 0;
        double bestScore = -1.0;

        for (auto const& pair : sObjectMgr->GetItemTemplateStore())
        {
            ItemTemplate const* p = &pair.second;

            if (p->InventoryType != invType)
                continue;
            if (exclude.count(p->ItemId))
                continue;
            if (ilvlCap && p->ItemLevel > ilvlCap)
                continue;
            /*
             * 需求等级过滤 —— 20 级角色不该拿到 80 级才能穿的装备。
             * ItemLevel（装等）和 RequiredLevel（需求等级）是两回事，
             * 只筛前者会发出一堆穿不上的装备。
             */
            if (playerLevel && p->RequiredLevel > playerLevel)
                continue;
            // 职业限制：AllowableClass 为 -1 表示全职业
            if (p->AllowableClass != -1 && !(uint32(p->AllowableClass) & clsMask))
                continue;
            // 护甲类型：只对护甲生效，饰品/戒指/项链/披风不限
            if (p->Class == ITEM_CLASS_ARMOR
                && invType != INVTYPE_FINGER && invType != INVTYPE_TRINKET
                && invType != INVTYPE_NECK   && invType != INVTYPE_CLOAK
                && p->SubClass != armorSub)
                continue;
            if (p->Quality < ITEM_QUALITY_UNCOMMON)
                continue;
            // 排除测试物品和活动物品
            if (p->HolidayId)
                continue;

            double sc = ScoreForRole(p, role);
            if (sc > bestScore)
            {
                bestScore = sc;
                best      = p->ItemId;
            }
        }

        return best;
    }

    inline uint32 PickBestWeapon(uint8 cls, GearRole role, uint32 ilvlCap,
                                 uint32 invType, uint32 subClass,
                                 std::unordered_set<uint32> const& exclude,
                                 uint32 playerLevel = 0)
    {
        uint32 clsMask = 1u << (cls - 1);
        uint32 best = 0;
        double bestScore = -1.0;

        for (auto const& pair : sObjectMgr->GetItemTemplateStore())
        {
            ItemTemplate const* p = &pair.second;

            if (p->InventoryType != invType)
                continue;
            if (exclude.count(p->ItemId))
                continue;
            if (ilvlCap && p->ItemLevel > ilvlCap)
                continue;
            // 需求等级过滤，同 PickBestItem
            if (playerLevel && p->RequiredLevel > playerLevel)
                continue;
            if (p->AllowableClass != -1 && !(uint32(p->AllowableClass) & clsMask))
                continue;
            // 盾牌在 ITEM_CLASS_ARMOR 下，武器在 ITEM_CLASS_WEAPON 下
            if (invType == INVTYPE_SHIELD)
            {
                if (p->Class != ITEM_CLASS_ARMOR || p->SubClass != ITEM_SUBCLASS_ARMOR_SHIELD)
                    continue;
            }
            else
            {
                if (p->Class != ITEM_CLASS_WEAPON || p->SubClass != subClass)
                    continue;
            }
            if (p->Quality < ITEM_QUALITY_UNCOMMON)
                continue;
            if (p->HolidayId)
                continue;

            double sc = ScoreForRole(p, role) + double(p->ItemLevel) * 5.0;
            if (sc > bestScore)
            {
                bestScore = sc;
                best      = p->ItemId;
            }
        }

        return best;
    }

    // ======================================================================
    //  【新增】判断某只怪是不是本副本的"末王"
    //  数据源：instance_encounters 表（官方表，记录每个副本的首领）
    //  规则：取该地图 Bit 值最大的那条 encounter，就是最后一个 BOSS
    // ======================================================================
    static std::unordered_map<uint32, uint32> s_finalBoss;   // mapId -> creature entry
    static bool s_finalBossLoaded = false;

    inline void LoadFinalBosses()
    {
        if (s_finalBossLoaded)
            return;

        s_finalBoss.clear();

        // instance_encounters: entry(DungeonEncounter.dbc), creditType, creditEntry, lastEncounterDungeon
        // creditType 0 = 击杀生物, creditEntry 就是 creature entry
        // 通过 DungeonEncounter.dbc 拿到 MapID 和 Bit
        if (QueryResult r = WorldDatabase.Query(
                "SELECT entry, creditType, creditEntry FROM instance_encounters WHERE creditType = 0"))
        {
            // mapId -> (bit, creatureEntry)
            std::unordered_map<uint32, std::pair<uint32, uint32>> best;

            do
            {
                Field* f = r->Fetch();
                uint32 dbcEntry  = f[0].GetUInt32();
                uint32 credEntry = f[2].GetUInt32();

                DungeonEncounterEntry const* enc = sDungeonEncounterStore.LookupEntry(dbcEntry);
                if (!enc)
                    continue;

                auto it = best.find(enc->MapID);
                if (it == best.end() || enc->Bit > it->second.first)
                    best[enc->MapID] = { enc->Bit, credEntry };
            } while (r->NextRow());

            for (auto const& pair : best)
                s_finalBoss[pair.first] = pair.second.second;
        }

        s_finalBossLoaded = true;
        TC_LOG_INFO("server.loading", ">> 套装系统：已载入 {} 个副本的末王信息", uint32(s_finalBoss.size()));
    }

    inline bool IsFinalBoss(uint32 mapId, uint32 creatureEntry)
    {
        LoadFinalBosses();
        auto it = s_finalBoss.find(mapId);
        return it != s_finalBoss.end() && it->second == creatureEntry;
    }

    // ======================================================================
    //  【新增】击杀末王后：加进度 + 检查是否解锁
    // ======================================================================
    inline void OnDungeonCleared(Player* player, uint32 mapId, uint8 diff)
    {
        LoadDungeonReq();

        auto it = s_dungeonReq.find(DungeonKey(mapId, diff));
        if (it == s_dungeonReq.end())
            return;   // 这个本没配置门槛，不管

        DungeonReq const& req = it->second;
        uint32 guid = player->GetGUID().GetCounter();

        AddKillCount(guid, mapId, diff);
        uint32 cur = GetKillCount(guid, mapId, diff);

        ChatHandler ch(player->GetSession());

        // 进度提示
        if (cur < req.needKills)
        {
            ch.PSendSysMessage("|cff00ccff[套装]|r %s 通关 |cffffff00%u/%u|r 次，"
                               "再刷 |cffff8800%u|r 次解锁全套职业套装",
                               req.name.c_str(), cur, req.needKills, req.needKills - cur);
            return;
        }

        // 已达标 —— 找出该解锁哪些套装
        GearConfig& cfg = GetConfig(player);
        std::vector<uint32> toUnlock;

        if (!req.setIds.empty())
        {
            // 手动指定了套装 ID
            for (std::string_view tok : Trinity::Tokenize(req.setIds, ',', false))
                if (uint32 sid = Trinity::StringTo<uint32>(tok).value_or(0))
                    toUnlock.push_back(sid);
        }
        else
        {
            // 按职业 + 装等区间自动匹配
            uint32 locale = uint32(ch.GetSessionDbcLocale());
            for (TierSetInfo const* t : FindTierSets(player->GetClass(), req.ilvlMin, req.ilvlMax, locale))
                toUnlock.push_back(t->setId);
        }

        bool anyNew = false;
        for (uint32 sid : toUnlock)
        {
            if (IsSetUnlocked(guid, sid))
                continue;
            UnlockSet(guid, sid);
            anyNew = true;

            if (ItemSetEntry const* se = sItemSetStore.LookupEntry(sid))
            {
                uint32 loc = uint32(ch.GetSessionDbcLocale());
                ch.PSendSysMessage("|cff00ff00[套装解锁]|r |cffa335ee%s|r",
                                   se->Name[loc] ? se->Name[loc] : "未知套装");
            }
        }

        if (anyNew)
        {
            ch.PSendSysMessage("|cff00ccff========================================|r");
            ch.PSendSysMessage("|cffffcc00恭喜！%s 已刷满 %u 次|r", req.name.c_str(), req.needKills);
            if (cfg.tierEnabled)
                ch.PSendSysMessage("用 |cffffff00.gearset tier|r 领取套装");
            else
                ch.PSendSysMessage("职业套装当前 |cffff0000已关闭|r，"
                                   "用 |cffffff00.gearset tier on|r 开启后领取");
            ch.PSendSysMessage("|cff00ccff========================================|r");
        }
        else
        {
            ch.PSendSysMessage("|cff00ccff[套装]|r %s 已通关 %u 次（套装已全部解锁）",
                               req.name.c_str(), cur);
        }
    }

} // namespace GearSet

// ============================================================================
//  PlayerScript：监听 BOSS 击杀 + Gossip 回调 + 登录清缓存
// ============================================================================
class gearset_playerscript : public PlayerScript
{
public:
    gearset_playerscript() : PlayerScript("gearset_playerscript") { }

    // ---- 击杀生物：判断是不是副本末王 ----
    void OnCreatureKill(Player* killer, Creature* killed) override
    {
        if (!killer || !killed)
            return;

        Map* map = killer->GetMap();
        if (!map || !map->IsDungeon())
            return;

        // 只认末王，防止反复进出刷计数
        if (!GearSet::IsFinalBoss(map->GetId(), killed->GetEntry()))
            return;

        // 3.3.5：Map::GetDifficulty() （不是 master 的 GetDifficultyID）
        uint8 diff = uint8(map->GetDifficulty());

        // 整队都算，不只是最后一击的人
        if (Group* group = killer->GetGroup())
        {
            for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
            {
                Player* member = itr->GetSource();
                if (!member || !member->IsInWorld())
                    continue;
                if (member->GetMap() != map)
                    continue;   // 不在本里的不算
                GearSet::OnDungeonCleared(member, map->GetId(), diff);
            }
        }
        else
        {
            GearSet::OnDungeonCleared(killer, map->GetId(), diff);
        }
    }

    // ---- 登出清缓存，防止内存泄漏 ----
    void OnLogout(Player* player) override
    {
        uint32 guid = player->GetGUID().GetCounter();
        GearSet::s_sess.erase(guid);
        GearSet::s_cfg.erase(guid);
    }

    // ---- Gossip 菜单回调 ----
    void OnGossipSelect(Player* player, uint32 menuId, uint32 sender, uint32 action) override;
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
        // 本仓库 cs_modify.cpp 用的是旧版框架（std::vector<ChatCommand>），这里保持一致
        // 所有子命令在 HandleGearsetCommand 里自己分发，不建子表
        static std::vector<ChatCommand> commandTable =
        {
            { "gearset",  rbac::RBAC_PERM_COMMAND_GEARSET, false, &HandleGearsetCommand,         "" },
        };
        return commandTable;
    }

    // ------------------------------------------------------------------
    //  主入口
    // ------------------------------------------------------------------
    static bool HandleGearsetCommand(ChatHandler* handler, char const* args)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        // 无参数 -> 打开主菜单
        if (!args || !*args)
        {
            ShowMainMenu(handler, player);
            return true;
        }

        std::vector<std::string> tok;
        {
            std::string s(args);
            size_t pos = 0;
            while (pos < s.size())
            {
                size_t sp = s.find(' ', pos);
                if (sp == std::string::npos) sp = s.size();
                if (sp > pos)
                    tok.push_back(s.substr(pos, sp - pos));
                pos = sp + 1;
            }
        }

        if (tok.empty())
        {
            ShowMainMenu(handler, player);
            return true;
        }

        std::string const& cmd = tok[0];

        // ---------- 职业套装 ----------
        if (cmd == "tier")
            return HandleTier(handler, player, tok);

        // ---------- 进度查询 ----------
        if (cmd == "progress" || cmd == "prog")
            return HandleProgress(handler, player);

        // ---------- 收藏册 ----------
        if (cmd == "book")
            return HandleBook(handler, player);

        // ---------- 配置 ----------
        if (cmd == "config" || cmd == "cfg")
            return HandleConfig(handler, player, tok);

        // ---------- 预览 ----------
        if (cmd == "preview")
            return HandlePreview(handler, player, tok);

        // ---------- 武器 ----------
        if (cmd == "weapon" || cmd == "武器")
            return HandleWeapon(handler, player, tok);

        // ---------- 饰品 ----------
        if (cmd == "trinket" || cmd == "饰品")
            return HandleTrinket(handler, player, tok);

        // ---------- NPCBot 配装 ----------
        if (cmd == "bot")
            return HandleBot(handler, player, tok);

        // ---------- 卸装 ----------
        if (cmd == "strip")
            return HandleStrip(handler, player);

        // ---------- 帮助 ----------
        if (cmd == "help" || cmd == "?")
        {
            ShowHelp(handler);
            return true;
        }

        // ---------- 默认：职业 + 装等 生成散件 ----------
        return HandlePieces(handler, player, tok);
    }

    // ==================================================================
    //  职业套装
    // ==================================================================
    static bool HandleTier(ChatHandler* handler, Player* player, std::vector<std::string> const& tok)
    {
        GearSet::GearConfig& cfg = GearSet::GetConfig(player);
        uint32 guid = player->GetGUID().GetCounter();

        // .gearset tier on|off
        if (tok.size() >= 2 && (tok[1] == "on" || tok[1] == "off"))
        {
            cfg.tierEnabled = (tok[1] == "on");
            GearSet::SaveConfig(player);
            handler->PSendSysMessage("|cff00ccff[套装]|r 职业套装已 %s",
                cfg.tierEnabled ? "|cff00ff00开启|r" : "|cffff0000关闭|r");
            if (cfg.tierEnabled)
                handler->SendSysMessage("  提示：职业套装含套装special效果，属性较强，注意平衡");
            return true;
        }

        if (!cfg.tierEnabled)
        {
            handler->PSendSysMessage("|cffff0000职业套装当前已关闭|r");
            handler->PSendSysMessage("用 |cffffff00.gearset tier on|r 开启");
            return true;
        }

        // .gearset tier <套装ID>  直接发
        if (tok.size() >= 2)
        {
            uint32 setId = uint32(atoi(tok[1].c_str()));
            if (setId)
            {
                if (!GearSet::IsSetUnlocked(guid, setId))
                {
                    handler->PSendSysMessage("|cffff0000该套装尚未解锁|r");
                    handler->PSendSysMessage("用 |cffffff00.gearset progress|r 查看刷本进度");
                    return true;
                }

                GearSet::BuildTierCache(uint32(handler->GetSessionDbcLocale()));
                for (GearSet::TierSetInfo const& t : GearSet::s_tierCache)
                    if (t.setId == setId)
                    {
                        bool eq = (tok.size() >= 3 && tok[2] == "equip") || cfg.autoEquip;
                        GearSet::GrantTierSet(handler, player, &t, eq, GearSet::ROLE_ANY);
                        return true;
                    }

                handler->PSendSysMessage("|cffff0000找不到套装 ID %u|r", setId);
                return true;
            }
        }

        // 无参数 -> 列出已解锁的套装
        std::vector<uint32> unlocked;
        if (QueryResult r = CharacterDatabase.PQuery(
                "SELECT set_id FROM custom_tierset_unlocked WHERE owner_guid = {} ORDER BY unlock_time DESC", guid))
        {
            do { unlocked.push_back(r->Fetch()[0].GetUInt32()); } while (r->NextRow());
        }

        if (unlocked.empty())
        {
            handler->PSendSysMessage("|cffff8800你还没有解锁任何职业套装|r");
            handler->PSendSysMessage("刷副本可以解锁，用 |cffffff00.gearset progress|r 看进度");
            return true;
        }

        GearSet::BuildTierCache(uint32(handler->GetSessionDbcLocale()));

        GearSet::GearSession& ss = GearSet::Sess(player);
        ss.Clear();
        ss.mode    = GearSet::MODE_TIER;
        ss.tierIds = unlocked;
        ss.page    = 0;

        ShowTierMenu(handler, player);
        return true;
    }

    // ==================================================================
    //  刷本进度
    // ==================================================================
    static bool HandleProgress(ChatHandler* handler, Player* player)
    {
        GearSet::LoadDungeonReq();
        uint32 guid = player->GetGUID().GetCounter();

        handler->PSendSysMessage("|cff00ccff========== 刷本进度 ==========|r");

        // 先读出该角色所有进度
        std::unordered_map<uint64, uint32> mine;
        if (QueryResult r = CharacterDatabase.PQuery(
                "SELECT map_id, difficulty, kill_count FROM custom_raid_progress WHERE owner_guid = {}", guid))
        {
            do
            {
                Field* f = r->Fetch();
                mine[GearSet::DungeonKey(f[0].GetUInt32(), f[1].GetUInt8())] = f[2].GetUInt32();
            } while (r->NextRow());
        }

        if (mine.empty())
        {
            handler->PSendSysMessage("|cffff8800你还没有任何副本记录|r");
            handler->PSendSysMessage("击杀副本的|cffffff00最后一个BOSS|r才会计数");
            handler->PSendSysMessage("|cff00ccff--- 部分副本门槛参考 ---|r");
            uint32 shown = 0;
            for (auto const& pair : GearSet::s_dungeonReq)
            {
                if (shown++ >= 12) break;
                handler->PSendSysMessage("  %s：需 |cffffff00%u|r 次",
                                         pair.second.name.c_str(), pair.second.needKills);
            }
            return true;
        }

        uint32 doneCount = 0, progCount = 0;

        // 已完成的
        for (auto const& pair : mine)
        {
            auto it = GearSet::s_dungeonReq.find(pair.first);
            if (it == GearSet::s_dungeonReq.end())
                continue;
            if (pair.second >= it->second.needKills)
            {
                if (doneCount == 0)
                    handler->PSendSysMessage("|cff00ff00--- 已解锁 ---|r");
                handler->PSendSysMessage("  |cff00ff00[完成]|r %s  %u/%u",
                                         it->second.name.c_str(), pair.second, it->second.needKills);
                ++doneCount;
            }
        }

        // 进行中的
        for (auto const& pair : mine)
        {
            auto it = GearSet::s_dungeonReq.find(pair.first);
            if (it == GearSet::s_dungeonReq.end())
                continue;
            if (pair.second < it->second.needKills)
            {
                if (progCount == 0)
                    handler->PSendSysMessage("|cffffcc00--- 进行中 ---|r");
                uint32 pct = pair.second * 100 / it->second.needKills;
                handler->PSendSysMessage("  |cffffcc00[%u%%]|r %s  %u/%u  还差 |cffff8800%u|r 次",
                                         pct, it->second.name.c_str(), pair.second,
                                         it->second.needKills, it->second.needKills - pair.second);
                ++progCount;
            }
        }

        handler->PSendSysMessage("|cff00ccff================================|r");
        handler->PSendSysMessage("已解锁 |cff00ff00%u|r 个，进行中 |cffffcc00%u|r 个", doneCount, progCount);
        return true;
    }

    // ==================================================================
    //  收藏册
    // ==================================================================
    static bool HandleBook(ChatHandler* handler, Player* player)
    {
        uint32 guid = player->GetGUID().GetCounter();
        GearSet::BuildTierCache(uint32(handler->GetSessionDbcLocale()));

        handler->PSendSysMessage("|cff00ccff========== 套装收藏册 ==========|r");

        uint32 total = 0;
        if (QueryResult r = CharacterDatabase.PQuery(
                "SELECT set_id, unlock_time FROM custom_tierset_unlocked "
                "WHERE owner_guid = {} ORDER BY unlock_time DESC", guid))
        {
            uint32 loc = uint32(handler->GetSessionDbcLocale());
            do
            {
                Field* f = r->Fetch();
                uint32 sid = f[0].GetUInt32();
                ++total;

                if (ItemSetEntry const* se = sItemSetStore.LookupEntry(sid))
                {
                    // 找件数
                    uint32 pieces = 0;
                    for (GearSet::TierSetInfo const& t : GearSet::s_tierCache)
                        if (t.setId == sid) { pieces = t.pieceCount; break; }

                    handler->PSendSysMessage("  |cffa335ee%s|r  (%u件)  ID=|cffffff00%u|r",
                                             se->Name[loc] ? se->Name[loc] : "未知", pieces, sid);
                }
            } while (r->NextRow());
        }

        if (total == 0)
        {
            handler->PSendSysMessage("|cffff8800收藏册还是空的|r");
            handler->PSendSysMessage("刷副本解锁套装后会自动收录");
        }
        else
        {
            handler->PSendSysMessage("|cff00ccff================================|r");
            handler->PSendSysMessage("共收藏 |cff00ff00%u|r 套", total);
            handler->PSendSysMessage("用 |cffffff00.gearset tier <ID>|r 领取");
        }
        return true;
    }

    // ==================================================================
    //  配置开关
    // ==================================================================
    static bool HandleConfig(ChatHandler* handler, Player* player, std::vector<std::string> const& tok)
    {
        GearSet::GearConfig& cfg = GearSet::GetConfig(player);

        if (tok.size() >= 3)
        {
            bool val = (tok[2] == "on" || tok[2] == "1");
            std::string const& key = tok[1];

            if      (key == "tier")   cfg.tierEnabled = val;
            else if (key == "equip")  cfg.autoEquip   = val;
            else if (key == "gem")    cfg.autoGem     = val;
            else if (key == "req")    cfg.grantReq    = val;
            else
            {
                handler->PSendSysMessage("|cffff0000未知配置项|r：%s", key.c_str());
                return true;
            }

            GearSet::SaveConfig(player);
            handler->PSendSysMessage("|cff00ff00已设置|r %s = %s", key.c_str(), val ? "开" : "关");
            return true;
        }

        handler->PSendSysMessage("|cff00ccff========== 套装系统配置 ==========|r");
        handler->PSendSysMessage("  tier   职业套装      : %s",
            cfg.tierEnabled ? "|cff00ff00开|r" : "|cffff0000关|r  (太超模，默认关)");
        handler->PSendSysMessage("  equip  默认自动穿戴  : %s",
            cfg.autoEquip ? "|cff00ff00开|r" : "|cffff0000关|r");
        handler->PSendSysMessage("  gem    自动镶嵌宝石  : %s",
            cfg.autoGem ? "|cff00ff00开|r" : "|cffff0000关|r");
        handler->PSendSysMessage("  req    自动补足前置  : %s",
            cfg.grantReq ? "|cff00ff00开|r" : "|cffff0000关|r  (声望/专业)");
        handler->PSendSysMessage("|cff00ccff==================================|r");
        handler->PSendSysMessage("修改：|cffffff00.gearset config <项> on|off|r");
        return true;
    }

    // ==================================================================
    //  散件生成（v2 的主功能，保留并增强）
    // ==================================================================
    static bool HandlePieces(ChatHandler* handler, Player* player, std::vector<std::string> const& tok)
    {
        uint8 cls = GearSet::ParseClassName(tok[0]);
        if (!cls)
        {
            handler->PSendSysMessage("|cffff0000未知职业|r：%s", tok[0].c_str());
            ShowHelp(handler);
            return true;
        }

        uint32 ilvl = 0;
        GearSet::GearRole role = GearSet::ROLE_ANY;
        bool autoEquip = false;
        bool preview   = false;

        for (size_t i = 1; i < tok.size(); ++i)
        {
            if (tok[i] == "equip")        autoEquip = true;
            else if (tok[i] == "preview") preview   = true;
            else if (isdigit((unsigned char)tok[i][0])) ilvl = uint32(atoi(tok[i].c_str()));
            else
            {
                GearSet::GearRole r = GearSet::ParseRole(tok[i]);
                if (r != GearSet::ROLE_ANY)
                    role = r;
            }
        }

        GearSet::GearConfig& cfg = GearSet::GetConfig(player);
        if (cfg.autoEquip)
            autoEquip = true;

        /*
         * v3.1：按实际等级选护甲类型。
         * 40 级前的战士拿板甲是穿不上的（板甲精通没学），要退回锁甲。
         */
        uint32 armorSub = GearSet::GetArmorSubClassForLevel(player, cls);
        uint32 topSub   = GearSet::GetArmorSubClassForClass(cls);

        handler->PSendSysMessage("|cffffcc00正在挑选 %s / %s / 装等<=%u 的 %s...|r",
            GearSet::GetClassName(cls), GearSet::GetRoleName(role), ilvl ? ilvl : 999u,
            GearSet::GetArmorSubName(armorSub));

        // 还没解锁最高护甲时给个说明，免得玩家以为发错了
        if (armorSub != topSub)
            handler->PSendSysMessage("|cff888888（%u 级还没学会%s精通，先发%s）|r",
                uint32(player->GetLevel()),
                GearSet::GetArmorSubName(topSub),
                GearSet::GetArmorSubName(armorSub));

        std::unordered_set<uint32> used;
        std::vector<uint32> picked;

        for (GearSet::SlotNeed const& need : GearSet::GetSlotNeeds())
        {
            for (uint32 n = 0; n < need.count; ++n)
            {
                uint32 id = GearSet::PickBestItem(cls, role, ilvl, need.invType, armorSub, used,
                                                  GearSet::LevelCapFor(player));
                if (id)
                {
                    used.insert(id);
                    picked.push_back(id);
                }
            }
        }

        if (picked.empty())
        {
            handler->PSendSysMessage("|cffff0000没有找到符合条件的装备|r");
            return true;
        }

        if (preview)
        {
            GearSet::PreviewItems(handler, player, picked, "散件配装");
            return true;
        }

        GearSet::GrantResult res;
        for (uint32 id : picked)
            GearSet::GrantOneItem(player, id, autoEquip, handler, role, res);

        handler->PSendSysMessage("|cff00ff00完成|r：发放 %u 件，穿戴 %u 件，失败 %u 件",
                                 res.ok, res.equipped, res.failed);

        // 提示职业套装
        if (!cfg.tierEnabled)
            handler->PSendSysMessage("|cff888888提示：职业套装（成套带特效）当前关闭，"
                                     ".gearset tier on 可开启|r");
        return true;
    }

    // ==================================================================
    //  预览
    // ==================================================================
    static bool HandlePreview(ChatHandler* handler, Player* player, std::vector<std::string> const& tok)
    {
        if (tok.size() < 2)
        {
            handler->PSendSysMessage("用法：|cffffff00.gearset preview <职业> [装等] [定位]|r");
            return true;
        }
        std::vector<std::string> sub(tok.begin() + 1, tok.end());
        sub.push_back("preview");
        return HandlePieces(handler, player, sub);
    }

    // ==================================================================
    //  武器套装
    // ==================================================================
    static bool HandleWeapon(ChatHandler* handler, Player* player, std::vector<std::string> const& tok)
    {
        if (tok.size() < 2)
        {
            handler->PSendSysMessage("用法：|cffffff00.gearset weapon <职业> [装等] [定位] [equip]|r");
            return true;
        }

        uint8 cls = GearSet::ParseClassName(tok[1]);
        if (!cls)
        {
            handler->PSendSysMessage("|cffff0000未知职业|r：%s", tok[1].c_str());
            return true;
        }

        uint32 ilvl = 0;
        GearSet::GearRole role = GearSet::ROLE_ANY;
        bool autoEquip = false;

        for (size_t i = 2; i < tok.size(); ++i)
        {
            if (tok[i] == "equip") autoEquip = true;
            else if (isdigit((unsigned char)tok[i][0])) ilvl = uint32(atoi(tok[i].c_str()));
            else
            {
                GearSet::GearRole r = GearSet::ParseRole(tok[i]);
                if (r != GearSet::ROLE_ANY) role = r;
            }
        }

        handler->PSendSysMessage("|cffffcc00正在挑选 %s / %s 的武器...|r",
                                 GearSet::GetClassName(cls), GearSet::GetRoleName(role));

        std::unordered_set<uint32> used;
        GearSet::GrantResult res;

        for (GearSet::WeaponNeed const& w : GearSet::GetWeaponNeeds(cls, role))
        {
            for (uint32 n = 0; n < w.count; ++n)
            {
                uint32 id = GearSet::PickBestWeapon(cls, role, ilvl, w.invType, w.subClass, used,
                                                    GearSet::LevelCapFor(player));
                if (!id)
                {
                    handler->PSendSysMessage("  |cffff8800未找到|r：%s", w.name);
                    continue;
                }
                used.insert(id);

                if (ItemTemplate const* p = sObjectMgr->GetItemTemplate(id))
                    handler->PSendSysMessage("  |cff00ff00%s|r：|Hitem:%u:0:0:0:0:0:0:0:0|h[%s]|h  装等%u",
                                             w.name, id, p->Name1.c_str(), p->ItemLevel);

                GearSet::GrantOneItem(player, id, autoEquip, handler, role, res);
            }
        }

        handler->PSendSysMessage("|cff00ff00武器完成|r：发放 %u 件，穿戴 %u 件", res.ok, res.equipped);
        return true;
    }

    // ==================================================================
    //  饰品套装
    // ==================================================================
    static bool HandleTrinket(ChatHandler* handler, Player* player, std::vector<std::string> const& tok)
    {
        if (tok.size() < 2)
        {
            handler->PSendSysMessage("用法：|cffffff00.gearset trinket <职业> [装等] [定位]|r");
            return true;
        }

        uint8 cls = GearSet::ParseClassName(tok[1]);
        if (!cls)
        {
            handler->PSendSysMessage("|cffff0000未知职业|r：%s", tok[1].c_str());
            return true;
        }

        uint32 ilvl = 0;
        GearSet::GearRole role = GearSet::ROLE_ANY;
        bool autoEquip = false;
        for (size_t i = 2; i < tok.size(); ++i)
        {
            if (tok[i] == "equip") autoEquip = true;
            else if (isdigit((unsigned char)tok[i][0])) ilvl = uint32(atoi(tok[i].c_str()));
            else
            {
                GearSet::GearRole r = GearSet::ParseRole(tok[i]);
                if (r != GearSet::ROLE_ANY) role = r;
            }
        }

        std::unordered_set<uint32> used;
        GearSet::GrantResult res;

        // 饰品给 4 个，方便切换
        for (uint32 n = 0; n < 4; ++n)
        {
            uint32 id = GearSet::PickBestItem(cls, role, ilvl, INVTYPE_TRINKET, 0, used,
                                              GearSet::LevelCapFor(player));
            if (!id) break;
            used.insert(id);

            if (ItemTemplate const* p = sObjectMgr->GetItemTemplate(id))
                handler->PSendSysMessage("  |Hitem:%u:0:0:0:0:0:0:0:0|h[%s]|h  装等%u",
                                         id, p->Name1.c_str(), p->ItemLevel);

            GearSet::GrantOneItem(player, id, autoEquip && n < 2, handler, role, res);
        }

        handler->PSendSysMessage("|cff00ff00饰品完成|r：发放 %u 件", res.ok);
        return true;
    }

    // ==================================================================
    //  NPCBot 配装
    // ==================================================================
    static bool HandleBot(ChatHandler* handler, Player* player, std::vector<std::string> const& tok)
    {
        Unit* target = handler->getSelectedUnit();
        if (!target || target->GetTypeId() != TYPEID_UNIT)
        {
            handler->PSendSysMessage("|cffff0000请先选中一个 NPCBot|r");
            return true;
        }

        if (tok.size() < 2)
        {
            handler->PSendSysMessage("用法：|cffffff00.gearset bot <职业> [装等]|r");
            handler->PSendSysMessage("说明：装备会发到你的背包，再手动给 bot 装上");
            return true;
        }

        uint8 cls = GearSet::ParseClassName(tok[1]);
        if (!cls)
        {
            handler->PSendSysMessage("|cffff0000未知职业|r：%s", tok[1].c_str());
            return true;
        }

        uint32 ilvl = 0;
        for (size_t i = 2; i < tok.size(); ++i)
            if (isdigit((unsigned char)tok[i][0]))
                ilvl = uint32(atoi(tok[i].c_str()));

        handler->PSendSysMessage("|cffffcc00为 %s 挑选 %s 装备...|r",
                                 target->GetName().c_str(), GearSet::GetClassName(cls));
        handler->PSendSysMessage("|cff888888装备将发到你的背包，"
                                 "用 NPCBot 自带的装备界面给它装上|r");

        std::vector<std::string> sub;
        sub.push_back(tok[1]);
        if (ilvl) sub.push_back(std::to_string(ilvl));
        return HandlePieces(handler, player, sub);
    }

    // ==================================================================
    //  卸装
    // ==================================================================
    static bool HandleStrip(ChatHandler* handler, Player* player)
    {
        uint32 count = 0;
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (!item)
                continue;

            ItemPosCountVec dest;
            InventoryResult msg = player->CanStoreItem(NULL_BAG, NULL_SLOT, dest, item, false);
            if (msg != EQUIP_ERR_OK)
                continue;

            player->RemoveItem(INVENTORY_SLOT_BAG_0, slot, true);
            player->StoreItem(dest, item, true);
            ++count;
        }
        handler->PSendSysMessage("|cff00ff00已卸下 %u 件装备到背包|r", count);
        return true;
    }

    // ==================================================================
    //  帮助
    // ==================================================================
    static void ShowHelp(ChatHandler* handler)
    {
        handler->PSendSysMessage("|cff00ccff========== 套装系统 v3 ==========|r");
        handler->PSendSysMessage("|cffffcc00【散件配装】|r");
        handler->PSendSysMessage("  .gearset <职业> [装等] [tank/dps/heal] [equip]");
        handler->PSendSysMessage("  .gearset preview <职业> [装等]     只看不发");
        handler->PSendSysMessage("|cffffcc00【职业套装】|r（成套带特效，默认关闭）");
        handler->PSendSysMessage("  .gearset tier on|off               开关");
        handler->PSendSysMessage("  .gearset tier                      已解锁列表");
        handler->PSendSysMessage("  .gearset tier <ID> [equip]         领取");
        handler->PSendSysMessage("|cffffcc00【刷本解锁】|r");
        handler->PSendSysMessage("  .gearset progress                  刷本进度");
        handler->PSendSysMessage("  .gearset book                      套装收藏册");
        handler->PSendSysMessage("|cffffcc00【专项】|r");
        handler->PSendSysMessage("  .gearset weapon <职业> [装等]      武器");
        handler->PSendSysMessage("  .gearset trinket <职业> [装等]     饰品");
        handler->PSendSysMessage("  .gearset bot <职业> [装等]         给NPCBot配装");
        handler->PSendSysMessage("|cffffcc00【其他】|r");
        handler->PSendSysMessage("  .gearset config                    查看/改开关");
        handler->PSendSysMessage("  .gearset strip                     卸下全身");
        handler->PSendSysMessage("|cff00ccff==================================|r");
    }

    // ==================================================================
    //  主菜单（Gossip）
    // ==================================================================
    static void ShowMainMenu(ChatHandler* handler, Player* player)
    {
        GearSet::GearSession& ss = GearSet::Sess(player);
        ss.Clear();

        GearSet::GearConfig& cfg = GearSet::GetConfig(player);

        player->PlayerTalkClass->ClearMenus();
        GossipMenu& menu = player->PlayerTalkClass->GetGossipMenu();

        menu.AddMenuItem(-1, GOSSIP_ICON_CHAT, "【散件配装】按职业+装等自动挑选",
                         GearSet::SENDER_MAIN, 1, "", 0, false);

        std::string tierLabel = "【职业套装】成套带特效  ";
        tierLabel += cfg.tierEnabled ? "|cff00ff00[已开启]|r" : "|cffff0000[已关闭]|r";
        menu.AddMenuItem(-1, GOSSIP_ICON_CHAT, tierLabel,
                         GearSet::SENDER_MAIN, 2, "", 0, false);

        menu.AddMenuItem(-1, GOSSIP_ICON_CHAT, "【刷本进度】看还差几次解锁",
                         GearSet::SENDER_MAIN, 3, "", 0, false);
        menu.AddMenuItem(-1, GOSSIP_ICON_CHAT, "【套装收藏册】已解锁的套装",
                         GearSet::SENDER_MAIN, 4, "", 0, false);
        menu.AddMenuItem(-1, GOSSIP_ICON_CHAT, "【武器】按职业挑武器",
                         GearSet::SENDER_MAIN, 5, "", 0, false);
        menu.AddMenuItem(-1, GOSSIP_ICON_CHAT, "【饰品】挑 4 个饰品",
                         GearSet::SENDER_MAIN, 6, "", 0, false);
        menu.AddMenuItem(-1, GOSSIP_ICON_CHAT, "【设置】开关配置",
                         GearSet::SENDER_MAIN, 7, "", 0, false);
        menu.AddMenuItem(-1, GOSSIP_ICON_CHAT, "【卸下全身装备】",
                         GearSet::SENDER_MAIN, 8, "", 0, false);
        menu.AddMenuItem(-1, GOSSIP_ICON_CHAT, "|cff888888关闭|r",
                         GearSet::SENDER_NAV, GearSet::NAV_CANCEL, "", 0, false);

        // 用玩家自己的 GUID 是合法的（MiscHandler.cpp:150 有 guid.IsPlayer() 分支）
        player->PlayerTalkClass->SendGossipMenu(DEFAULT_GOSSIP_MESSAGE, player->GetGUID());
    }

    // ==================================================================
    //  职业选择菜单
    // ==================================================================
    static void ShowClassMenu(ChatHandler* /*handler*/, Player* player)
    {
        player->PlayerTalkClass->ClearMenus();
        GossipMenu& menu = player->PlayerTalkClass->GetGossipMenu();

        static uint8 const classes[] = {
            CLASS_WARRIOR, CLASS_PALADIN, CLASS_HUNTER, CLASS_ROGUE, CLASS_PRIEST,
            CLASS_DEATH_KNIGHT, CLASS_SHAMAN, CLASS_MAGE, CLASS_WARLOCK, CLASS_DRUID
        };

        // 当前职业排第一，方便一键
        menu.AddMenuItem(-1, GOSSIP_ICON_CHAT,
                         std::string("|cff00ff00[我的职业] ") + GearSet::GetClassName(player->GetClass()) + "|r",
                         GearSet::SENDER_CLASS, player->GetClass(), "", 0, false);

        for (uint8 c : classes)
        {
            if (c == player->GetClass())
                continue;
            menu.AddMenuItem(-1, GOSSIP_ICON_CHAT, GearSet::GetClassName(c),
                             GearSet::SENDER_CLASS, c, "", 0, false);
        }

        menu.AddMenuItem(-1, GOSSIP_ICON_CHAT, "|cff888888返回|r",
                         GearSet::SENDER_NAV, GearSet::NAV_BACK, "", 0, false);
        player->PlayerTalkClass->SendGossipMenu(DEFAULT_GOSSIP_MESSAGE, player->GetGUID());
    }

    // ==================================================================
    //  装等选择菜单
    // ==================================================================
    static void ShowIlvlMenu(Player* player)
    {
        player->PlayerTalkClass->ClearMenus();
        GossipMenu& menu = player->PlayerTalkClass->GetGossipMenu();

        struct IlvlOpt { uint32 v; char const* label; };
        static IlvlOpt const opts[] = {
            {  20, "装等 20   —— 1-20级"          },
            {  40, "装等 40   —— 20-40级"         },
            {  60, "装等 60   —— 40-60级"         },
            {  90, "装等 90   —— 60级 T1-T2"      },
            { 120, "装等 120  —— 60级 T3"         },
            { 140, "装等 140  —— TBC 前期"        },
            { 164, "装等 164  —— TBC T5"          },
            { 200, "装等 200  —— WLK 入门"        },
            { 219, "装等 219  —— 纳克萨玛斯"      },
            { 232, "装等 232  —— 奥杜尔"          },
            { 245, "装等 245  —— 十字军"          },
            { 264, "装等 264  —— ICC 25H"         },
            { 284, "装等 284  —— ICC 传说"        },
            { 500, "装等 500  —— 自定义内容"      },
            { 999, "装等 999  —— 自定义高级"      },
            {9999, "装等 9999 —— 超凡内容"        },
            {   0, "|cffff8800不限装等（含大数值）|r" },
        };

        for (IlvlOpt const& o : opts)
            menu.AddMenuItem(-1, GOSSIP_ICON_CHAT, o.label,
                             GearSet::SENDER_ILVL, o.v ? o.v : 0xFFFFFFFF, "", 0, false);

        menu.AddMenuItem(-1, GOSSIP_ICON_CHAT, "|cff888888返回|r",
                         GearSet::SENDER_NAV, GearSet::NAV_BACK, "", 0, false);
        player->PlayerTalkClass->SendGossipMenu(DEFAULT_GOSSIP_MESSAGE, player->GetGUID());
    }

    // ==================================================================
    //  定位选择菜单
    // ==================================================================
    static void ShowRoleMenu(Player* player)
    {
        player->PlayerTalkClass->ClearMenus();
        GossipMenu& menu = player->PlayerTalkClass->GetGossipMenu();

        menu.AddMenuItem(-1, GOSSIP_ICON_CHAT, "坦克  —— 耐力/护甲/闪避",
                         GearSet::SENDER_ROLE, GearSet::ROLE_TANK, "", 0, false);
        menu.AddMenuItem(-1, GOSSIP_ICON_CHAT, "输出  —— 主属性/暴击/急速",
                         GearSet::SENDER_ROLE, GearSet::ROLE_DPS, "", 0, false);
        menu.AddMenuItem(-1, GOSSIP_ICON_CHAT, "治疗  —— 智力/精神/法强",
                         GearSet::SENDER_ROLE, GearSet::ROLE_HEAL, "", 0, false);
        menu.AddMenuItem(-1, GOSSIP_ICON_CHAT, "通用  —— 只看装等",
                         GearSet::SENDER_ROLE, GearSet::ROLE_ANY, "", 0, false);
        menu.AddMenuItem(-1, GOSSIP_ICON_CHAT, "|cff888888返回|r",
                         GearSet::SENDER_NAV, GearSet::NAV_BACK, "", 0, false);
        player->PlayerTalkClass->SendGossipMenu(DEFAULT_GOSSIP_MESSAGE, player->GetGUID());
    }

    // ==================================================================
    //  职业套装菜单（带分页，严守 32 条上限）
    // ==================================================================
    static void ShowTierMenu(ChatHandler* handler, Player* player)
    {
        GearSet::GearSession& ss = GearSet::Sess(player);
        uint32 loc = uint32(handler ? handler->GetSessionDbcLocale() : 0);

        player->PlayerTalkClass->ClearMenus();
        GossipMenu& menu = player->PlayerTalkClass->GetGossipMenu();

        uint32 total  = uint32(ss.tierIds.size());
        uint32 maxPg  = total ? ((total - 1) / GearSet::PER_PAGE) : 0;
        if (ss.page > maxPg)
            ss.page = maxPg;

        uint32 begin = ss.page * GearSet::PER_PAGE;
        uint32 end   = std::min(begin + GearSet::PER_PAGE, total);

        for (uint32 i = begin; i < end; ++i)
        {
            uint32 sid = ss.tierIds[i];
            ItemSetEntry const* se = sItemSetStore.LookupEntry(sid);
            if (!se)
                continue;

            uint32 pieces = 0;
            for (GearSet::TierSetInfo const& t : GearSet::s_tierCache)
                if (t.setId == sid) { pieces = t.pieceCount; break; }

            std::string label = "|cffa335ee";
            label += se->Name[loc] ? se->Name[loc] : "未知套装";
            label += "|r  (";
            label += std::to_string(pieces);
            label += "件)";

            menu.AddMenuItem(-1, GOSSIP_ICON_CHAT, label, GearSet::SENDER_TIER, sid, "", 0, false);
        }

        // 导航（最多 3 条，加上 29 条内容 = 32，正好卡住 GossipDef.cpp:42 的 ASSERT）
        if (ss.page > 0)
            menu.AddMenuItem(-1, GOSSIP_ICON_CHAT, "|cff00ccff<< 上一页|r",
                             GearSet::SENDER_NAV, GearSet::NAV_PREV, "", 0, false);
        if (ss.page < maxPg)
            menu.AddMenuItem(-1, GOSSIP_ICON_CHAT, "|cff00ccff下一页 >>|r",
                             GearSet::SENDER_NAV, GearSet::NAV_NEXT, "", 0, false);
        menu.AddMenuItem(-1, GOSSIP_ICON_CHAT, "|cff888888返回|r",
                         GearSet::SENDER_NAV, GearSet::NAV_BACK, "", 0, false);

        player->PlayerTalkClass->SendGossipMenu(DEFAULT_GOSSIP_MESSAGE, player->GetGUID());
    }
};

// ============================================================================
//  Gossip 回调实现
//  （放在 gearset_commandscript 定义之后，因为要调它的 static 函数）
// ============================================================================
void gearset_playerscript::OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 sender, uint32 action)
{
    if (!player)
        return;

    // 只处理本系统的 sender，其余交给别的脚本
    if (sender < GearSet::SENDER_MAIN || sender > GearSet::SENDER_BOOK)
        return;

    ChatHandler handler(player->GetSession());
    GearSet::GearSession& ss = GearSet::Sess(player);

    switch (sender)
    {
        // ---------------- 主菜单 ----------------
        case GearSet::SENDER_MAIN:
            switch (action)
            {
                case 1:  // 散件配装
                    ss.mode = GearSet::MODE_PIECES;
                    ss.step = 1;
                    player->PlayerTalkClass->SendCloseGossip();
                    gearset_commandscript::ShowClassMenu(&handler, player);
                    return;
                case 2:  // 职业套装
                {
                    player->PlayerTalkClass->SendCloseGossip();
                    std::vector<std::string> tok = { "tier" };
                    gearset_commandscript::HandleTier(&handler, player, tok);
                    return;
                }
                case 3:  // 刷本进度
                    player->PlayerTalkClass->SendCloseGossip();
                    gearset_commandscript::HandleProgress(&handler, player);
                    return;
                case 4:  // 收藏册
                    player->PlayerTalkClass->SendCloseGossip();
                    gearset_commandscript::HandleBook(&handler, player);
                    return;
                case 5:  // 武器
                    ss.mode = GearSet::MODE_WEAPON;
                    ss.step = 1;
                    player->PlayerTalkClass->SendCloseGossip();
                    gearset_commandscript::ShowClassMenu(&handler, player);
                    return;
                case 6:  // 饰品
                    ss.mode = GearSet::MODE_TRINKET;
                    ss.step = 1;
                    player->PlayerTalkClass->SendCloseGossip();
                    gearset_commandscript::ShowClassMenu(&handler, player);
                    return;
                case 7:  // 设置
                {
                    player->PlayerTalkClass->SendCloseGossip();
                    std::vector<std::string> tok = { "config" };
                    gearset_commandscript::HandleConfig(&handler, player, tok);
                    return;
                }
                case 8:  // 卸装
                    player->PlayerTalkClass->SendCloseGossip();
                    gearset_commandscript::HandleStrip(&handler, player);
                    return;
                default:
                    return;
            }

        // ---------------- 选职业 ----------------
        case GearSet::SENDER_CLASS:
            ss.cls  = uint8(action);
            ss.step = 2;
            player->PlayerTalkClass->SendCloseGossip();
            gearset_commandscript::ShowRoleMenu(player);
            return;

        // ---------------- 选定位 ----------------
        case GearSet::SENDER_ROLE:
            ss.role = GearSet::GearRole(action);
            ss.step = 3;
            player->PlayerTalkClass->SendCloseGossip();
            gearset_commandscript::ShowIlvlMenu(player);
            return;

        // ---------------- 选装等 -> 执行 ----------------
        case GearSet::SENDER_ILVL:
        {
            ss.ilvl = (action == 0xFFFFFFFF) ? 0 : action;
            player->PlayerTalkClass->SendCloseGossip();

            std::vector<std::string> tok;
            switch (ss.mode)
            {
                case GearSet::MODE_WEAPON:  tok.push_back("weapon");  break;
                case GearSet::MODE_TRINKET: tok.push_back("trinket"); break;
                default: break;
            }
            tok.push_back(GearSet::GetClassName(ss.cls));
            if (ss.ilvl)
                tok.push_back(std::to_string(ss.ilvl));
            switch (ss.role)
            {
                case GearSet::ROLE_TANK: tok.push_back("tank"); break;
                case GearSet::ROLE_HEAL: tok.push_back("heal"); break;
                case GearSet::ROLE_DPS:  tok.push_back("dps");  break;
                default: break;
            }

            switch (ss.mode)
            {
                case GearSet::MODE_WEAPON:
                    gearset_commandscript::HandleWeapon(&handler, player, tok);
                    break;
                case GearSet::MODE_TRINKET:
                    gearset_commandscript::HandleTrinket(&handler, player, tok);
                    break;
                default:
                    gearset_commandscript::HandlePieces(&handler, player, tok);
                    break;
            }
            return;
        }

        // ---------------- 领取职业套装 ----------------
        case GearSet::SENDER_TIER:
        {
            player->PlayerTalkClass->SendCloseGossip();
            GearSet::BuildTierCache(uint32(handler.GetSessionDbcLocale()));
            GearSet::GearConfig& cfg = GearSet::GetConfig(player);

            for (GearSet::TierSetInfo const& t : GearSet::s_tierCache)
                if (t.setId == action)
                {
                    GearSet::GrantTierSet(&handler, player, &t, cfg.autoEquip, GearSet::ROLE_ANY);
                    return;
                }
            handler.PSendSysMessage("|cffff0000找不到该套装|r");
            return;
        }

        // ---------------- 导航 ----------------
        case GearSet::SENDER_NAV:
            switch (action)
            {
                case GearSet::NAV_PREV:
                    if (ss.page > 0) --ss.page;
                    gearset_commandscript::ShowTierMenu(&handler, player);
                    return;
                case GearSet::NAV_NEXT:
                    ++ss.page;
                    gearset_commandscript::ShowTierMenu(&handler, player);
                    return;
                case GearSet::NAV_BACK:
                    player->PlayerTalkClass->SendCloseGossip();
                    gearset_commandscript::ShowMainMenu(&handler, player);
                    return;
                case GearSet::NAV_CANCEL:
                default:
                    player->PlayerTalkClass->SendCloseGossip();
                    ss.Clear();
                    return;
            }

        default:
            return;
    }
}

// ============================================================================
//  注册
// ============================================================================
void AddSC_gearset_commandscript()
{
    new gearset_commandscript();
    new gearset_playerscript();
}
