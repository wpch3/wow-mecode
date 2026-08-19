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
    inline uint32 PickBestItem(uint8 cls, GearRole role, uint32 ilvlCap,
                               uint32 invType, uint32 armorSub,
                               std::unordered_set<uint32> const& exclude)
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
                                 std::unordered_set<uint32> const& exclude)
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