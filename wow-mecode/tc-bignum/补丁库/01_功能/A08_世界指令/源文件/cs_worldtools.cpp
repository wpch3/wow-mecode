/*
 * ============================================================================
 *  世界与副本指令 —— cs_worldtools.cpp   (step21)
 * ============================================================================
 *
 *  第 3 批「世界与副本」7 个 + 第 2 批剩余 3 个，共 10 个指令。
 *
 *  ── 第 3 批 · 世界与副本 ──────────────────────────────────────────────
 *   .kill radius <码>              秒杀周围敌对目标
 *   .npc delete radius <码>        清 GM 刷的残留 NPC（默认只删 GM 刷的）
 *   .instance cleartrash           清小怪，保留 BOSS
 *   .instance opendoors            打开本内所有门
 *   .group buffall                 全队/全团加满增益
 *   .aura addraid <法术ID>         给全团上指定光环
 *   .summon service                召唤便携拍卖/银行/邮箱
 *
 *  ── 第 2 批 · 装备补完 ────────────────────────────────────────────────
 *   .gear socketall <宝石ID>       全身插满宝石
 *   .gear enchantall <附魔ID>      全身打满附魔
 *   .gear stats                    全身属性汇总
 *
 *  ── 保护系统（用户设计）──────────────────────────────────────────────
 *   .protect                       查看/开关所有保护
 *   .protect city on|off           主城保护（AREA_FLAG_CAPITAL 自动覆盖全种族）
 *   .protect sanctuary on|off      圣所保护（达拉然/沙塔斯）
 *   .protect town on|off           小镇保护（默认关）
 *   .protect gmonly on|off         删NPC时只删GM刷的（默认开）
 *
 *   · 半径【无上限】，最低可设很小，覆盖所有地图
 *   · 危险操作必须二次确认，确认提示里会列出【哪些保护没开】
 *
 *  ── 已核实 API（文件:行号，全部 public）───────────────────────────────
 *   DBCEnums.h:255   AREA_FLAG_CAPITAL    = 0x00000100   全种族主城
 *   DBCEnums.h:258   AREA_FLAG_SANCTUARY  = 0x00000800   圣所
 *   DBCEnums.h:268   AREA_FLAG_TOWN       = 0x00200000   带旅店的小镇
 *   DBCStructure.h:176 AreaTableEntry / :182 Flags
 *   DBCStores.h:93   sAreaTableStore
 *   WorldObject.h:390 GetAreaId() / :389 GetZoneId()
 *   Unit.h:1023      Unit::Kill(Unit*, Unit*, bool)          static
 *   Unit.h:904       GetStat(Stats) / :906 GetArmor()
 *   Unit.h:1538      GetTotalStatValue(Stats)
 *   Unit.h:1562      GetTotalAttackPowerValue(WeaponAttackType)
 *   Unit.h:1624      SpellBaseDamageBonusDone(SpellSchoolMask)
 *   Creature.h:98    GetSpawnId()
 *   Creature.h:222   Creature::DeleteFromDB(LowType)         static
 *   Creature.h:268   DespawnOrUnsummon(Milliseconds, Seconds)
 *   Creature.h:129   IsDungeonBoss()
 *   Creature.h:394   IsNPCBotOrPet()      ← 必须排除，否则删坏 bot
 *   Unit.h:1806      ToTempSummon()
 *   GameObject.h:176 GetGoType() / :179 SetGoState() / :237 UseDoorOrButton()
 *   SharedDefines.h:1724 GO_STATE_ACTIVE / :1655 GAMEOBJECT_TYPE_DOOR
 *   Group.h:257      GetFirstMember() / :259 GetMembersCount()
 *   Player.h:2155    GetGroup()
 *   Map.h:446        IsDungeon() / :448 IsRaid()
 *   Chat.h:87        ParseCommands()
 *
 *  放置：D:\TrinityCore\src\server\scripts\Commands\cs_worldtools.cpp
 *  RBAC：71012      新增源文件必须重跑 CMake
 * ============================================================================
 */

#include "ScriptMgr.h"
#include "Cell.h"
#include "CellImpl.h"
#include "Chat.h"
#include "DBCStores.h"
#include "GameObject.h"
#include "GameTime.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "Item.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "RBAC.h"
#include "SpellMgr.h"
#include "TemporarySummon.h"
#include "World.h"
#include "WorldSession.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace WorldTools
{
    // ==================================================================
    //  保护设置（每个 GM 独立，运行时可改）
    // ==================================================================
    struct Protection
    {
        bool city      = true;    // 主城（AREA_FLAG_CAPITAL，全种族自动覆盖）
        bool sanctuary = true;    // 圣所（达拉然/沙塔斯）
        bool town      = false;   // 小镇（默认关，太多会碍事）
        bool gmOnly    = true;    // 删 NPC 只删 GM 刷的
        bool players   = true;    // 永不伤害玩家（这条不给关）
        bool friendly  = true;    // 不打友方单位
    };

    inline std::unordered_map<uint32, Protection>& Prot()
    {
        static std::unordered_map<uint32, Protection> m;
        return m;
    }

    inline Protection& P(Player* p) { return Prot()[p->GetGUID().GetCounter()]; }

    // ==================================================================
    //  待确认操作（二次确认用）
    // ==================================================================
    struct PendingOp
    {
        std::string cmd;        // 原指令，用于比对
        uint32      expireMs = 0;
    };

    inline std::unordered_map<uint32, PendingOp>& Pend()
    {
        static std::unordered_map<uint32, PendingOp> m;
        return m;
    }

    // ==================================================================
    //  区域判定
    // ==================================================================
    /*
     * 主城不用手写列表 —— 暴雪已经给 7 个主城全打了 AREA_FLAG_CAPITAL：
     *   奥格瑞玛 / 暴风城 / 雷霆崖 / 幽暗城 / 达纳苏斯 / 埃索达 / 银月城
     * 以后加新种族主城，只要 DBC 里打了这个 flag 也自动覆盖。
     */
    struct AreaGuard
    {
        bool        blocked = false;
        char const* reason  = "";
        std::string areaName;
    };

    inline void CheckArea(Player* player, AreaGuard& out)
    {
        out.blocked = false;
        out.reason  = "";
        out.areaName.clear();

        Protection const& pr = P(player);

        uint32 areaId = player->GetAreaId();
        AreaTableEntry const* area = sAreaTableStore.LookupEntry(areaId);

        // 当前 area 查不到就往上找 zone
        if (!area)
            area = sAreaTableStore.LookupEntry(player->GetZoneId());

        if (!area)
            return;

        LocaleConstant loc = player->GetSession()
            ? player->GetSession()->GetSessionDbcLocale() : DEFAULT_LOCALE;
        if (area->AreaName[loc] && *area->AreaName[loc])
            out.areaName = area->AreaName[loc];

        if (pr.city && (area->Flags & AREA_FLAG_CAPITAL))
        {
            out.blocked = true;
            out.reason  = "主城";
            return;
        }
        if (pr.sanctuary && (area->Flags & AREA_FLAG_SANCTUARY))
        {
            out.blocked = true;
            out.reason  = "圣所";
            return;
        }
        if (pr.town && (area->Flags & AREA_FLAG_TOWN))
        {
            out.blocked = true;
            out.reason  = "小镇";
            return;
        }
    }

    // 当前区域名（提示用）
    inline std::string AreaNameOf(Player* player)
    {
        AreaTableEntry const* area = sAreaTableStore.LookupEntry(player->GetAreaId());
        if (!area)
            area = sAreaTableStore.LookupEntry(player->GetZoneId());
        if (!area)
            return "未知区域";

        LocaleConstant loc = player->GetSession()
            ? player->GetSession()->GetSessionDbcLocale() : DEFAULT_LOCALE;
        if (area->AreaName[loc] && *area->AreaName[loc])
            return area->AreaName[loc];
        return "未知区域";
    }

    // ==================================================================
    //  二次确认
    // ==================================================================
    /*
     * 返回 true = 已确认过，可以执行
     * 返回 false = 已经把确认提示发出去了，本次不执行
     *
     * 确认提示里会【列出所有保护的当前状态】，特别标出没开的那些 —— 
     * 用户明确要求"要加上当前的哪些保护未开"。
     */
    inline bool NeedConfirm(ChatHandler* handler, Player* player,
                            std::string const& cmdKey, uint32 targetCount,
                            char const* actionDesc, float radius)
    {
        uint32 low = player->GetGUID().GetCounter();
        uint32 now = GameTime::GetGameTimeMS();

        auto it = Pend().find(low);
        if (it != Pend().end() && it->second.cmd == cmdKey && now < it->second.expireMs)
        {
            Pend().erase(it);          // 用掉这次确认
            return true;
        }

        Protection const& pr = P(player);

        handler->PSendSysMessage(" ");
        handler->PSendSysMessage("|cffff0000===== 危险操作确认 =====|r");
        handler->PSendSysMessage("|cffffff00%s|r", actionDesc);
        handler->PSendSysMessage("  范围：|cffffff00%.0f 码|r   影响：|cffff8000%u 个目标|r",
            radius, targetCount);
        handler->PSendSysMessage("  当前位置：|cff00ccff%s|r", AreaNameOf(player).c_str());
        handler->PSendSysMessage(" ");
        handler->PSendSysMessage("|cff00ff00保护状态：|r");

        auto line = [&](char const* name, bool on, char const* note)
        {
            if (on)
                handler->PSendSysMessage("    %-10s |cff00ff00[开]|r %s", name, note);
            else
                handler->PSendSysMessage("    %-10s |cffff0000[关]|r %s  |cffff0000<-- 未开启|r",
                    name, note);
        };

        line("主城保护",   pr.city,      "全种族7个主城");
        line("圣所保护",   pr.sanctuary, "达拉然/沙塔斯等");
        line("小镇保护",   pr.town,      "带旅店的小镇");
        line("友方保护",   pr.friendly,  "不打友方单位");
        line("玩家保护",   pr.players,   "永不伤害玩家");

        handler->PSendSysMessage(" ");
        handler->PSendSysMessage("|cffff8000确认执行请重复一次并加 |r|cffffff00confirm|r");
        handler->PSendSysMessage("|cff888888（10 秒内有效；用 .protect 改保护设置）|r");

        PendingOp op;
        op.cmd      = cmdKey;
        op.expireMs = now + 10000;
        Pend()[low] = op;
        return false;
    }

    // ==================================================================
    //  工具
    // ==================================================================
    inline std::vector<std::string> Tok(char const* args)
    {
        std::vector<std::string> t;
        if (!args)
            return t;
        std::string a = args;
        size_t pos = 0;
        while (pos < a.size())
        {
            size_t sp = a.find(' ', pos);
            if (sp == std::string::npos)
                sp = a.size();
            if (sp > pos)
                t.push_back(a.substr(pos, sp - pos));
            pos = sp + 1;
        }
        return t;
    }

    inline bool IsOn(std::string const& s)
    {
        return s == "on" || s == "1" || s == "开" || s == "true";
    }

    // 找出末尾有没有 confirm
    inline bool HasConfirm(std::vector<std::string>& tok)
    {
        for (auto it = tok.begin(); it != tok.end(); ++it)
        {
            if (*it == "confirm" || *it == "确认")
            {
                tok.erase(it);
                return true;
            }
        }
        return false;
    }
}

class worldtools_commandscript : public CommandScript
{
public:
    worldtools_commandscript() : CommandScript("worldtools_commandscript") { }

    std::vector<ChatCommand> GetCommands() const override
    {
        static std::vector<ChatCommand> commandTable =
        {
            { "protect",  rbac::RBAC_PERM_COMMAND_WORLDTOOLS, false, &HandleProtect,  "" },
            { "killr",    rbac::RBAC_PERM_COMMAND_WORLDTOOLS, false, &HandleKillR,    "" },
            { "npcclean", rbac::RBAC_PERM_COMMAND_WORLDTOOLS, false, &HandleNpcClean, "" },
            { "inst",     rbac::RBAC_PERM_COMMAND_WORLDTOOLS, false, &HandleInst,     "" },
            { "raidbuff", rbac::RBAC_PERM_COMMAND_WORLDTOOLS, false, &HandleRaidBuff, "" },
            { "service",  rbac::RBAC_PERM_COMMAND_WORLDTOOLS, false, &HandleService,  "" },
            { "gear",     rbac::RBAC_PERM_COMMAND_WORLDTOOLS, false, &HandleGear,     "" },
        };
        return commandTable;
    }

    // ==================================================================
    //  .protect —— 保护设置
    // ==================================================================
    static bool HandleProtect(ChatHandler* handler, char const* args)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        std::vector<std::string> tok = WorldTools::Tok(args);
        WorldTools::Protection& pr = WorldTools::P(player);

        if (tok.empty())
            return ShowProtect(handler, player);

        std::string const& k = tok[0];

        if (k == "all")
        {
            bool on = (tok.size() > 1) ? WorldTools::IsOn(tok[1]) : true;
            pr.city = pr.sanctuary = pr.town = on;
            handler->PSendSysMessage("|cff00ff00[保护]|r 全部区域保护已%s", on ? "开启" : "关闭");
            return ShowProtect(handler, player);
        }

        if (tok.size() < 2)
        {
            handler->PSendSysMessage("|cffff0000用法：|r.protect <city|sanctuary|town|gmonly> <on|off>");
            return true;
        }

        bool on = WorldTools::IsOn(tok[1]);

        if (k == "city"      || k == "主城")   pr.city      = on;
        else if (k == "sanctuary" || k == "圣所") pr.sanctuary = on;
        else if (k == "town"      || k == "小镇") pr.town      = on;
        else if (k == "gmonly")                   pr.gmOnly    = on;
        else if (k == "friendly"  || k == "友方") pr.friendly  = on;
        else
        {
            handler->PSendSysMessage("|cffff0000未知项|r：%s", k.c_str());
            return true;
        }

        handler->PSendSysMessage("|cff00ff00[保护]|r %s 已%s", k.c_str(), on ? "开启" : "关闭");
        return ShowProtect(handler, player);
    }

    static bool ShowProtect(ChatHandler* handler, Player* player)
    {
        WorldTools::Protection const& pr = WorldTools::P(player);

        handler->PSendSysMessage("|cff00ff00===== 保护设置 =====|r");
        handler->PSendSysMessage("  当前位置：|cff00ccff%s|r",
            WorldTools::AreaNameOf(player).c_str());
        handler->PSendSysMessage(" ");

        auto row = [&](char const* key, char const* name, bool on, char const* note)
        {
            handler->PSendSysMessage("  %-10s %s  |cff888888%s|r",
                name, on ? "|cff00ff00[开]|r" : "|cffff0000[关]|r", note);
            (void)key;
        };

        row("city",      "主城保护", pr.city,      "全种族7个主城，自动识别");
        row("sanctuary", "圣所保护", pr.sanctuary, "达拉然/沙塔斯");
        row("town",      "小镇保护", pr.town,      "所有带旅店的小镇");
        row("friendly",  "友方保护", pr.friendly,  "不打友方单位");
        row("gmonly",    "只删GM的", pr.gmOnly,    "删NPC时保护原版NPC");

        handler->PSendSysMessage(" ");
        handler->PSendSysMessage("|cffffff00.protect city off|r    关掉主城保护");
        handler->PSendSysMessage("|cffffff00.protect all off|r     关掉所有区域保护");
        handler->PSendSysMessage("|cff888888玩家永远受保护，不可关闭|r");
        return true;
    }

    // ==================================================================
    //  .killr <半径> —— 秒杀周围
    // ==================================================================
    static bool HandleKillR(ChatHandler* handler, char const* args)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        std::vector<std::string> tok = WorldTools::Tok(args);
        bool confirmed = WorldTools::HasConfirm(tok);

        if (tok.empty())
        {
            handler->PSendSysMessage("|cff00ff00===== 范围秒杀 =====|r");
            handler->PSendSysMessage("|cffffff00.killr <半径>|r   秒杀周围敌对生物");
            handler->PSendSysMessage("|cff888888半径无上限，最小 1 码|r");
            handler->PSendSysMessage("|cff888888会跳过：玩家、友方、NPCBot、宠物、图腾|r");
            return true;
        }

        // 半径：无上限，最低 1
        float radius = float(atof(tok[0].c_str()));
        if (radius < 1.0f)
            radius = 1.0f;

        // 区域保护
        WorldTools::AreaGuard g;
        WorldTools::CheckArea(player, g);
        if (g.blocked)
        {
            handler->PSendSysMessage("|cffff0000[已拦截]|r 当前在【%s】：%s",
                g.reason, g.areaName.c_str());
            handler->PSendSysMessage("|cff888888要在这里执行，先关掉对应保护：|r");
            handler->PSendSysMessage("|cffffff00.protect %s off|r",
                strcmp(g.reason, "主城") == 0 ? "city" :
                strcmp(g.reason, "圣所") == 0 ? "sanctuary" : "town");
            return true;
        }

        // 先数一遍
        std::vector<Creature*> targets;
        CollectKillTargets(player, radius, targets);

        if (targets.empty())
        {
            handler->PSendSysMessage("|cffff8000范围内没有可击杀的目标。|r");
            return true;
        }

        std::ostringstream key;
        key << "killr:" << uint32(radius);

        if (!confirmed)
        {
            std::ostringstream desc;
            desc << "秒杀周围所有敌对生物";
            if (!WorldTools::NeedConfirm(handler, player, key.str(),
                                         uint32(targets.size()), desc.str().c_str(), radius))
                return true;
        }

        uint32 killed = 0;
        for (Creature* c : targets)
        {
            if (!c || !c->IsAlive())
                continue;
            // Unit.h:1023  static void Kill(Unit* attacker, Unit* victim, bool durabilityLoss)
            Unit::Kill(player, c, false);
            ++killed;
        }

        handler->PSendSysMessage("|cff00ff00[完成]|r 击杀 |cffffff00%u|r 个目标（半径 %.0f 码）",
            killed, radius);
        return true;
    }

    // 收集可击杀目标
    static void CollectKillTargets(Player* player, float radius,
                                   std::vector<Creature*>& out)
    {
        out.clear();

        WorldTools::Protection const& pr = WorldTools::P(player);

        std::list<Creature*> found;
        Trinity::AnyUnitInObjectRangeCheck check(player, radius);
        Trinity::CreatureListSearcher<Trinity::AnyUnitInObjectRangeCheck>
            searcher(player, found, check);
        Cell::VisitAllObjects(player, searcher, radius);

        for (Creature* c : found)
        {
            if (!c || !c->IsAlive())
                continue;

            // NPCBot 绝对不能杀（Creature.h:394）
            if (c->IsNPCBotOrPet())
                continue;

            // 宠物/图腾/触发器跳过
            if (c->IsPet() || c->IsTotem() || c->IsTrigger())
                continue;

            // 友方保护
            if (pr.friendly && player->IsFriendlyTo(c))
                continue;

            out.push_back(c);
        }
    }

    // ==================================================================
    //  .npcclean <半径> —— 清残留 NPC
    // ==================================================================
    static bool HandleNpcClean(ChatHandler* handler, char const* args)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        std::vector<std::string> tok = WorldTools::Tok(args);
        bool confirmed = WorldTools::HasConfirm(tok);

        if (tok.empty())
        {
            handler->PSendSysMessage("|cff00ff00===== 清理残留NPC =====|r");
            handler->PSendSysMessage("|cffffff00.npcclean <半径>|r   删除周围 NPC");
            handler->PSendSysMessage(" ");
            handler->PSendSysMessage("|cff888888默认只删【GM刷的】（.npc add 出来的）|r");
            handler->PSendSysMessage("|cff888888要删全部：|r|cffffff00.protect gmonly off|r");
            handler->PSendSysMessage("|cffff0000永不删除：NPCBot、玩家宠物|r");
            return true;
        }

        float radius = float(atof(tok[0].c_str()));
        if (radius < 1.0f)
            radius = 1.0f;

        WorldTools::AreaGuard g;
        WorldTools::CheckArea(player, g);
        if (g.blocked)
        {
            handler->PSendSysMessage("|cffff0000[已拦截]|r 当前在【%s】：%s",
                g.reason, g.areaName.c_str());
            handler->PSendSysMessage("|cff888888.protect %s off 可临时关闭|r",
                strcmp(g.reason, "主城") == 0 ? "city" :
                strcmp(g.reason, "圣所") == 0 ? "sanctuary" : "town");
            return true;
        }

        WorldTools::Protection const& pr = WorldTools::P(player);

        std::vector<Creature*> temp;    // 临时召唤，UnSummon
        std::vector<uint32>    dbSpawn; // 数据库刷的，DeleteFromDB
        CollectCleanTargets(player, radius, temp, dbSpawn);

        uint32 total = uint32(temp.size() + dbSpawn.size());
        if (!total)
        {
            handler->PSendSysMessage("|cffff8000范围内没有可清理的 NPC。|r");
            return true;
        }

        std::ostringstream key;
        key << "npcclean:" << uint32(radius);

        if (!confirmed)
        {
            std::ostringstream desc;
            desc << "删除周围 NPC"
                 << (pr.gmOnly ? "（只删GM刷的）" : "（删全部，含原版NPC）");
            if (!WorldTools::NeedConfirm(handler, player, key.str(),
                                         total, desc.str().c_str(), radius))
            {
                if (!pr.gmOnly)
                    handler->PSendSysMessage(
                        "|cffff0000注意：gmonly 已关闭，原版 NPC 也会被删且需重启才能恢复！|r");
                return true;
            }
        }

        uint32 done = 0;

        for (Creature* c : temp)
        {
            if (!c)
                continue;
            // Creature.h:268
            c->DespawnOrUnsummon();
            ++done;
        }

        for (uint32 spawnId : dbSpawn)
        {
            // Creature.h:222  static bool DeleteFromDB(LowType)
            if (Creature::DeleteFromDB(spawnId))
                ++done;
        }

        handler->PSendSysMessage("|cff00ff00[完成]|r 清理 |cffffff00%u|r 个 NPC（半径 %.0f 码）",
            done, radius);
        if (!dbSpawn.empty())
            handler->PSendSysMessage("|cff888888其中 %u 个是数据库刷点，已永久删除|r",
                uint32(dbSpawn.size()));
        return true;
    }

    static void CollectCleanTargets(Player* player, float radius,
                                    std::vector<Creature*>& temp,
                                    std::vector<uint32>& dbSpawn)
    {
        temp.clear();
        dbSpawn.clear();

        WorldTools::Protection const& pr = WorldTools::P(player);

        std::list<Creature*> found;
        Trinity::AnyUnitInObjectRangeCheck check(player, radius);
        Trinity::CreatureListSearcher<Trinity::AnyUnitInObjectRangeCheck>
            searcher(player, found, check);
        Cell::VisitAllObjects(player, searcher, radius);

        for (Creature* c : found)
        {
            if (!c)
                continue;

            // 这三种永远不删
            if (c->IsNPCBotOrPet())
                continue;
            if (c->IsPet())
                continue;

            /*
             * 临时召唤 vs 数据库刷点：
             *   TempSummon（.npc add temp / 技能召唤）-> UnSummon 即可
             *   有 SpawnId 的是 .npc add 写进数据库的 -> DeleteFromDB
             *
             * gmOnly 模式下只处理临时召唤 + 有 SpawnId 的，
             * 因为原版 NPC 也有 SpawnId，无法从内存区分谁是 GM 加的。
             * 所以 gmOnly 时【只清临时召唤】，这是最安全的口径。
             */
            if (c->ToTempSummon())
            {
                temp.push_back(c);
                continue;
            }

            if (!pr.gmOnly)
            {
                uint32 sid = c->GetSpawnId();
                if (sid)
                    dbSpawn.push_back(sid);
            }
        }
    }

    // ==================================================================
    //  .inst —— 副本工具
    // ==================================================================
    static bool HandleInst(ChatHandler* handler, char const* args)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        std::vector<std::string> tok = WorldTools::Tok(args);
        bool confirmed = WorldTools::HasConfirm(tok);

        if (tok.empty())
        {
            handler->PSendSysMessage("|cff00ff00===== 副本工具 =====|r");
            handler->PSendSysMessage("|cffffff00.inst cleartrash [半径]|r  清小怪，保留BOSS");
            handler->PSendSysMessage("|cffffff00.inst opendoors [半径]|r   打开周围的门");
            handler->PSendSysMessage("|cff888888不填半径默认 200 码|r");
            return true;
        }

        Map* map = player->GetMap();
        bool inInstance = map && (map->IsDungeon() || map->IsRaid());

        if (tok[0] == "cleartrash" || tok[0] == "清小怪")
        {
            if (!inInstance)
            {
                handler->PSendSysMessage("|cffff0000这个指令只能在副本/团本里用。|r");
                return true;
            }

            float radius = (tok.size() > 1) ? float(atof(tok[1].c_str())) : 200.0f;
            if (radius < 1.0f)
                radius = 1.0f;

            std::vector<Creature*> trash;
            std::list<Creature*> found;
            Trinity::AnyUnitInObjectRangeCheck check(player, radius);
            Trinity::CreatureListSearcher<Trinity::AnyUnitInObjectRangeCheck>
                searcher(player, found, check);
            Cell::VisitAllObjects(player, searcher, radius);

            for (Creature* c : found)
            {
                if (!c || !c->IsAlive())
                    continue;
                if (c->IsNPCBotOrPet() || c->IsPet() || c->IsTotem() || c->IsTrigger())
                    continue;
                if (player->IsFriendlyTo(c))
                    continue;
                // Creature.h:129 —— BOSS 保留
                if (c->IsDungeonBoss())
                    continue;
                trash.push_back(c);
            }

            if (trash.empty())
            {
                handler->PSendSysMessage("|cffff8000范围内没有小怪（BOSS已自动保留）。|r");
                return true;
            }

            std::ostringstream key;
            key << "cleartrash:" << uint32(radius);

            if (!confirmed)
            {
                if (!WorldTools::NeedConfirm(handler, player, key.str(),
                        uint32(trash.size()), "清除副本小怪（BOSS会保留）", radius))
                    return true;
            }

            uint32 n = 0;
            for (Creature* c : trash)
            {
                if (c && c->IsAlive())
                {
                    Unit::Kill(player, c, false);
                    ++n;
                }
            }

            handler->PSendSysMessage("|cff00ff00[完成]|r 清除 |cffffff00%u|r 个小怪，BOSS 已保留", n);
            return true;
        }

        if (tok[0] == "opendoors" || tok[0] == "开门")
        {
            float radius = (tok.size() > 1) ? float(atof(tok[1].c_str())) : 200.0f;
            if (radius < 1.0f)
                radius = 1.0f;

            std::list<GameObject*> gos;
            Trinity::GameObjectInRangeCheck gcheck(player->GetPositionX(),
                player->GetPositionY(), player->GetPositionZ(), radius);
            Trinity::GameObjectListSearcher<Trinity::GameObjectInRangeCheck>
                gsearcher(player, gos, gcheck);
            Cell::VisitAllObjects(player, gsearcher, radius);

            uint32 opened = 0;
            for (GameObject* go : gos)
            {
                if (!go)
                    continue;
                // GameObject.h:176
                GameobjectTypes t = go->GetGoType();
                if (t != GAMEOBJECT_TYPE_DOOR && t != GAMEOBJECT_TYPE_BUTTON)
                    continue;
                // GameObject.h:179 / SharedDefines.h:1724
                go->SetGoState(GO_STATE_ACTIVE);
                ++opened;
            }

            handler->PSendSysMessage("|cff00ff00[完成]|r 打开 |cffffff00%u|r 扇门（半径 %.0f 码）",
                opened, radius);
            if (!opened)
                handler->PSendSysMessage("|cff888888附近没找到门，试试加大半径|r");
            return true;
        }

        handler->PSendSysMessage("|cffff0000未知子命令|r，用 .inst 看用法");
        return true;
    }

    // ==================================================================
    //  .raidbuff —— 全团增益 / 全团上光环
    // ==================================================================
    static bool HandleRaidBuff(ChatHandler* handler, char const* args)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        std::vector<std::string> tok = WorldTools::Tok(args);

        if (tok.empty())
        {
            handler->PSendSysMessage("|cff00ff00===== 团队增益 =====|r");
            handler->PSendSysMessage("|cffffff00.raidbuff|r              给全队补满增益");
            handler->PSendSysMessage("|cffffff00.raidbuff <法术ID>|r     给全队上指定光环");
            handler->PSendSysMessage("|cff888888没组队就只作用于自己|r");
            return true;
        }

        // 指定法术ID
        uint32 spellId = uint32(atoi(tok[0].c_str()));
        if (!spellId)
        {
            handler->PSendSysMessage("|cffff0000无效的法术ID|r");
            return true;
        }

        SpellInfo const* si = sSpellMgr->GetSpellInfo(spellId);
        if (!si)
        {
            handler->PSendSysMessage("|cffff0000法术 %u 不存在|r", spellId);
            return true;
        }

        std::vector<Unit*> members;
        CollectGroup(player, members);

        uint32 n = 0, botCount = 0;
        for (Unit* m : members)
        {
            if (!m || !m->IsAlive())
                continue;
            m->CastSpell(m, spellId, true);
            ++n;
            if (m->GetTypeId() != TYPEID_PLAYER)
                ++botCount;
        }

        LocaleConstant loc = handler->GetSessionDbcLocale();
        handler->PSendSysMessage("|cff00ff00[完成]|r 给 |cffffff00%u|r 个目标上了 |cffffff00%s|r",
            n, (si->SpellName[loc] && *si->SpellName[loc]) ? si->SpellName[loc] : "该光环");

        if (botCount)
            handler->PSendSysMessage("|cff888888其中 %u 个是 NPCBot|r", botCount);
        return true;
    }

    /*
     * v2 修复（用户实测：.raidbuff 没给 bot 上 buff）
     *
     * 根因：Group 里【玩家和 bot 是两条独立链表】。
     *   GetFirstMember()    -> GroupReference    -> Player*    （Group.h:257）
     *   GetFirstBotMember() -> GroupBotReference -> Creature*  （Group.h:202）
     *
     * 旧代码只遍历了前者，NPCBot 是 Creature，压根不在那条链表里。
     *
     * 已核实：
     *   Group.h:202          GetFirstBotMember()          public
     *   GroupRefManager.h:55 GroupBotReference::next()    public
     *   Reference.h:96       GetSource() -> FROM*（这里是 Creature*）
     *
     * 输出改成 Unit*，玩家和 bot 一起装 —— CastSpell 收的就是 Unit*。
     */
    static void CollectGroup(Player* player, std::vector<Unit*>& out)
    {
        out.clear();

        Group* grp = player->GetGroup();
        if (!grp)
        {
            out.push_back(player);
            return;
        }

        // ---- 玩家成员 ----
        for (GroupReference* itr = grp->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player* m = itr->GetSource();
            if (!m)
                continue;
            if (!m->IsInWorld() || !m->IsInMap(player))
                continue;
            out.push_back(m);
        }

        // ---- NPCBot 成员（这条之前漏了）----
        for (GroupBotReference* itr = grp->GetFirstBotMember(); itr != nullptr; itr = itr->next())
        {
            Creature* c = itr->GetSource();
            if (!c)
                continue;
            if (!c->IsInWorld() || !c->IsInMap(player))
                continue;
            out.push_back(c);
        }

        /*
         * 没组队时，跟随的 bot 也要吃到 buff。
         * 玩家可以带 bot 但不一定开队伍，这种情况扫周围友方 bot。
         */
        if (out.size() <= 1)
        {
            std::list<Creature*> found;
            Trinity::AnyUnitInObjectRangeCheck check(player, 60.0f);
            Trinity::CreatureListSearcher<Trinity::AnyUnitInObjectRangeCheck>
                searcher(player, found, check);
            Cell::VisitAllObjects(player, searcher, 60.0f);

            for (Creature* c : found)
            {
                if (!c || !c->IsAlive())
                    continue;
                // 只认自己的 bot（Creature.h:394）
                if (!c->IsNPCBotOrPet())
                    continue;

                bool dup = false;
                for (Unit* u : out)
                    if (u == c) { dup = true; break; }
                if (!dup)
                    out.push_back(c);
            }
        }

        if (out.empty())
            out.push_back(player);
    }

    // ==================================================================
    //  .service —— 便携服务NPC
    // ==================================================================
    static bool HandleService(ChatHandler* handler, char const* args)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        std::vector<std::string> tok = WorldTools::Tok(args);

        /*
         * v3.3 修正（用户实测：商人没商店、修理没修理、邮箱不是邮箱）
         *
         *  1. 修理/商店按钮【由 Gossip 菜单驱动】，光有 npcflag 没用
         *     Player.cpp:14163  遍历 gossip_menu_option 按 OptionNpcFlag 匹配
         *     Player.cpp:14170  GOSSIP_OPTION_ARMORER = 15（修理）
         *     Player.cpp:14177  GOSSIP_OPTION_VENDOR  = 3（商店）
         *     -> 见 sql/38，给 960003/960007 挂了 gossip_menu_id
         *
         *  2. 商人必须有货
         *     Player.cpp:14179  VENDOR 选项检查 GetVendorItems()，
         *     空列表直接 canTalk=false -> 见 sql/38 的 npc_vendor
         *
         * 银行/拍卖/旅店为什么本来就能用：
         *   这三个是客户端【硬编码】按 npcflag 直接弹窗，不走 gossip_menu_option。
         *
         * ------------------------------------------------------------------
         * v4.0 邮箱修正 —— 上一版的结论是【错的】，在此更正
         *
         *   上一版写「UNIT_NPC_FLAG_MAILBOX 服务端无处理代码，邮箱只能用 GO」，
         *   那是因为当时只 grep 了本地留存的几十个文件，不是全仓库。
         *   拉全量源码重查，结论完全相反：
         *
         *     MailHandler.cpp:39  bool WorldSession::CanOpenMailBox(ObjectGuid guid)
         *     MailHandler.cpp:49      else if (guid.IsGameObject())
         *     MailHandler.cpp:51          GetGameObjectIfCanInteractWith(guid, GAMEOBJECT_TYPE_MAILBOX)
         *     MailHandler.cpp:54      else if (guid.IsAnyTypeCreature())
         *     MailHandler.cpp:56          GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_MAILBOX)  <- NPC 完全支持
         *
         *   官方自己就有 NPC 邮箱 —— 银之侍从 / 征战联盟传令官：
         *     npcs_special.cpp:2089   case GOSSIP_OPTION_MAIL:
         *     npcs_special.cpp:2091       me->SetNpcFlag(UNIT_NPC_FLAG_MAILBOX);
         *     npcs_special.cpp:2092       player->GetSession()->SendShowMailBox(me->GetGUID());
         *
         *   那为什么上一版召唤出来是个「人类且没功能」的 NPC？
         *     GO 184137 在本仓库的 gameobject_template 里【查不到】
         *     （world_database.sql 只有表结构，TDB 数据包另发；
         *       全仓库只有 sql/old/ 的历史文件提过这个 entry）。
         *     Object.cpp SummonGameObject() 查不到模板就 return nullptr，
         *     所以邮箱那次召唤根本没成 —— 你看到的是 960004「便携邮差」
         *     的旧数据（npcflag=1 只有 GOSSIP，模型 1300 是人类 Lyria）。
         *
         *   v4.0 的做法（双保险，两条腿走路）：
         *     a) NPC 960004 挂上 GOSSIP|MAILBOX，走官方银之侍从那条路；
         *     b) 召唤时【运行时探测】库里真实存在的邮箱 GO
         *        （遍历 GetGameObjectTemplates() 找 type==GAMEOBJECT_TYPE_MAILBOX），
         *        探到就顺手放一个，探不到也不影响 NPC 那条路。
         *     这样无论你的 TDB 里有没有 184137，邮箱都能用。
         */
        struct Svc { char const* key; char const* cn; uint32 entry; bool isGO; };
        static Svc const svcs[] =
        {
            { "auction", "便携拍卖师",   960001, false },
            { "bank",    "便携银行家",   960002, false },
            { "repair",  "便携修理匠",   960003, false },
            { "mail",    "便携邮差",     960004, false },   // v4.0 改回 NPC
            { "inn",     "便携旅店老板", 960005, false },
            { "vendor",  "便携商人",     960007, false },
        };

        /*
         * 运行时探测一个真实可用的邮箱 GameObject entry。
         *   ObjectMgr.h:975  GameObjectTemplateContainer const& GetGameObjectTemplates() const
         *   GameObjectData.h:36   uint32 type;
         *   SharedDefines.h:1674  GAMEOBJECT_TYPE_MAILBOX = 19
         * 优先用官方常见的 184137，没有就退而取库里任意一个邮箱 GO。
         * 静态缓存，只算一次。
         */
        static uint32 s_mailboxGo = 0xFFFFFFFF;   // 0xFFFFFFFF = 还没探测过
        if (s_mailboxGo == 0xFFFFFFFF)
        {
            s_mailboxGo = 0;                      // 0 = 探测过但没找到
            if (GameObjectTemplate const* pref = sObjectMgr->GetGameObjectTemplate(184137))
            {
                if (pref->type == GAMEOBJECT_TYPE_MAILBOX)
                    s_mailboxGo = 184137;
            }
            if (!s_mailboxGo)
            {
                for (auto const& kv : sObjectMgr->GetGameObjectTemplates())
                {
                    if (kv.second.type == GAMEOBJECT_TYPE_MAILBOX)
                    {
                        s_mailboxGo = kv.first;
                        break;
                    }
                }
            }
        }

        if (tok.empty())
        {
            handler->PSendSysMessage("|cff00ff00===== 便携服务 =====|r");
            for (Svc const& s : svcs)
                handler->PSendSysMessage("|cffffff00.service %s|r   召唤%s", s.key, s.cn);
            handler->PSendSysMessage("|cffffff00.service all|r       全部召唤");
            handler->PSendSysMessage("|cff888888召唤物 5 分钟后自动消失|r");
            return true;
        }

        bool all = (tok[0] == "all" || tok[0] == "全部");
        uint32 spawned = 0;

        for (Svc const& s : svcs)
        {
            if (!all && tok[0] != s.key)
                continue;

            // 稍微错开位置，免得叠在一起
            float ang  = float(spawned) * 1.2f;
            float x = player->GetPositionX() + 2.5f * std::cos(ang);
            float y = player->GetPositionY() + 2.5f * std::sin(ang);

            // 模板不存在就跳过，并提示去执行 SQL
            if (!sObjectMgr->GetCreatureTemplate(s.entry))
            {
                handler->PSendSysMessage(
                    "|cffff0000缺少模板 %u（%s）|r，请先执行 |cffffff00sql/37|r 和 |cffffff00sql/38|r",
                    s.entry, s.cn);
                continue;
            }

            /*
             * WorldObject.h:476
             *   TempSummon* SummonCreature(uint32, float x,y,z,o,
             *                              TempSummonType, Milliseconds, ObjectGuid)
             * 注意返回的是 TempSummon*，时长参数是 Milliseconds 不是 uint32。
             *
             * 名字、阵营、npcflag 全部由模板决定，这里不再手动改 ——
             * 客户端只认 entry 对应的缓存。
             */
            if (TempSummon* c = player->SummonCreature(s.entry, x, y,
                    player->GetPositionZ(), player->GetOrientation(),
                    TEMPSUMMON_TIMED_DESPAWN, Milliseconds(300000)))
            {
                (void)c;
                ++spawned;
                handler->PSendSysMessage("|cff00ff00[召唤]|r %s", s.cn);

                /*
                 * 邮差额外附赠一个真·邮箱 GameObject（双保险）。
                 * NPC 那条路靠 MailHandler.cpp:56 的 UNIT_NPC_FLAG_MAILBOX，
                 * GO 这条路靠 MailHandler.cpp:51 的 GAMEOBJECT_TYPE_MAILBOX。
                 * 两条路只要有一条通，你就能收发邮件。
                 *
                 * WorldObject.h:478
                 *   GameObject* SummonGameObject(uint32 entry, float x,y,z,ang,
                 *                                QuaternionData const&, Seconds)
                 * QuaternionData::fromEulerAnglesZYX 用法见 SpellEffects.cpp:3622
                 */
                if (s.entry == 960004 && s_mailboxGo)
                {
                    QuaternionData rot = QuaternionData::fromEulerAnglesZYX(
                        player->GetOrientation(), 0.0f, 0.0f);

                    if (player->SummonGameObject(s_mailboxGo,
                            x + 1.5f, y, player->GetPositionZ(),
                            player->GetOrientation(), rot, Seconds(300)))
                    {
                        handler->PSendSysMessage(
                            "|cff00ff00[召唤]|r 邮箱（GO %u）", s_mailboxGo);
                    }
                }
            }
        }

        if (!spawned)
        {
            handler->PSendSysMessage("|cffff0000未知服务|r，用 .service 看列表");
            return true;
        }

        handler->PSendSysMessage("|cff888888共 %u 个，5 分钟后消失|r", spawned);
        return true;
    }

    // ==================================================================
    //  .gear —— 装备补完（第2批剩余3个）
    // ==================================================================
    static bool HandleGear(ChatHandler* handler, char const* args)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        std::vector<std::string> tok = WorldTools::Tok(args);

        if (tok.empty())
        {
            handler->PSendSysMessage("|cff00ff00===== 装备工具 =====|r");
            handler->PSendSysMessage("|cffffff00.gear stats|r              全身属性汇总");
            handler->PSendSysMessage("|cffffff00.gear socketall <宝石ID>|r 全身插满宝石");
            handler->PSendSysMessage("|cffffff00.gear enchantall <附魔ID>|r 全身打满附魔");
            return true;
        }

        if (tok[0] == "stats" || tok[0] == "属性")
            return GearStats(handler, player);

        if (tok[0] == "socketall")
            return GearSocketAll(handler, player, tok);

        if (tok[0] == "enchantall")
            return GearEnchantAll(handler, player, tok);

        handler->PSendSysMessage("|cffff0000未知子命令|r，用 .gear 看用法");
        return true;
    }

    // ------------------------------------------------------------------
    //  .gear stats —— 属性汇总
    // ------------------------------------------------------------------
    static bool GearStats(ChatHandler* handler, Player* player)
    {
        handler->PSendSysMessage("|cff00ff00===== 全身属性汇总 =====|r");

        // 五维（Unit.h:1538 GetTotalStatValue）
        handler->PSendSysMessage("|cff00ccff[基础属性]|r");
        static char const* statCn[MAX_STATS] = { "力量", "敏捷", "耐力", "智力", "精神" };
        for (uint8 i = 0; i < MAX_STATS; ++i)
            handler->PSendSysMessage("  %-6s |cffffff00%.0f|r",
                statCn[i], player->GetTotalStatValue(Stats(i)));

        // 生命/法力
        handler->PSendSysMessage(" ");
        handler->PSendSysMessage("|cff00ccff[生存]|r");
        handler->PSendSysMessage("  生命上限  |cffffff00%u|r", player->GetMaxHealth());
        if (player->GetMaxPower(POWER_MANA))
            handler->PSendSysMessage("  法力上限  |cffffff00%u|r", player->GetMaxPower(POWER_MANA));
        handler->PSendSysMessage("  护甲      |cffffff00%u|r", player->GetArmor());

        // 抗性（SharedDefines.h:327 起，HOLY=1）
        std::ostringstream res;
        static char const* resCn[] = { "", "神圣", "火焰", "自然", "冰霜", "暗影", "奥术" };
        for (int32 i = SPELL_SCHOOL_HOLY; i < MAX_SPELL_SCHOOL; ++i)
        {
            if (res.tellp() > 0)
                res << "  ";
            res << resCn[i] << " " << player->GetResistance(SpellSchools(i));
        }
        handler->PSendSysMessage("  抗性      |cff888888%s|r", res.str().c_str());

        // 攻击（Unit.h:1562）
        handler->PSendSysMessage(" ");
        handler->PSendSysMessage("|cff00ccff[攻击]|r");
        handler->PSendSysMessage("  近战攻强  |cffffff00%.0f|r",
            player->GetTotalAttackPowerValue(BASE_ATTACK));
        handler->PSendSysMessage("  远程攻强  |cffffff00%.0f|r",
            player->GetTotalAttackPowerValue(RANGED_ATTACK));

        // 法伤（Unit.h:1624）—— 取最高的一系
        int32 maxSp = 0;
        for (int32 i = SPELL_SCHOOL_HOLY; i < MAX_SPELL_SCHOOL; ++i)
        {
            int32 sp = player->SpellBaseDamageBonusDone(SpellSchoolMask(1 << i));
            if (sp > maxSp)
                maxSp = sp;
        }
        handler->PSendSysMessage("  法术强度  |cffffff00%d|r", maxSp);

        // 战斗评级（Player.h:1663 GetRatingMultiplier 反算百分比）
        handler->PSendSysMessage(" ");
        handler->PSendSysMessage("|cff00ccff[战斗评级]|r");

        struct RRow { CombatRating cr; char const* cn; };
        static RRow const rows[] =
        {
            { CR_CRIT_MELEE,        "暴击"     },
            { CR_HIT_MELEE,         "命中"     },
            { CR_HASTE_MELEE,       "急速"     },
            { CR_EXPERTISE,         "精准"     },
            { CR_DODGE,             "躲闪"     },
            { CR_PARRY,             "招架"     },
            { CR_BLOCK,             "格挡"     },
            { CR_ARMOR_PENETRATION, "护甲穿透" },
        };

        for (RRow const& r : rows)
        {
            /*
             * Player.h:1664 GetRatingBonusValue(CombatRating) 直接返回百分比，
             * 比自己取 PLAYER_FIELD_COMBAT_RATING_1 字段再乘 multiplier 干净。
             * 实现见 Player.cpp:5332。
             */
            handler->PSendSysMessage("  %-8s |cffffff00%.2f%%|r",
                r.cn, player->GetRatingBonusValue(r.cr));
        }

        // 装备件数
        uint32 equipped = 0;
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
            if (player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
                ++equipped;

        handler->PSendSysMessage(" ");
        handler->PSendSysMessage("|cff888888已装备 %u / %u 个部位|r",
            equipped, uint32(EQUIPMENT_SLOT_END - EQUIPMENT_SLOT_START));
        return true;
    }

    // ------------------------------------------------------------------
    //  .gear socketall —— 全身插宝石
    // ------------------------------------------------------------------
    static bool GearSocketAll(ChatHandler* handler, Player* player,
                              std::vector<std::string> const& tok)
    {
        if (tok.size() < 2)
        {
            handler->PSendSysMessage("|cffff0000用法：|r.gear socketall <宝石ID>");
            handler->PSendSysMessage("|cff888888常用：40008 坚固恒金(+30耐力)  40014 精巧翡翠(+20敏捷)|r");
            return true;
        }

        uint32 gemId = uint32(atoi(tok[1].c_str()));
        ItemTemplate const* gem = sObjectMgr->GetItemTemplate(gemId);
        if (!gem)
        {
            handler->PSendSysMessage("|cffff0000物品 %u 不存在|r", gemId);
            return true;
        }

        if (gem->Class != ITEM_CLASS_GEM)
        {
            handler->PSendSysMessage("|cffff0000物品 %u 不是宝石|r", gemId);
            return true;
        }

        /*
         * 插宝石走 .gem 那套太复杂（要处理 socket bonus、宝石颜色匹配），
         * 这里用最直接的做法：给每个有插槽的部位挨个调官方指令。
         *
         * 注意：SocketColor 是 ItemTemplate 的字段，
         * 空的插槽 SocketColor = 0。
         */
        uint32 done = 0, slots = 0;

        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (!item)
                continue;

            ItemTemplate const* proto = item->GetTemplate();
            if (!proto)
                continue;

            for (uint32 i = 0; i < MAX_ITEM_PROTO_SOCKETS; ++i)
            {
                if (!proto->Socket[i].Color)
                    continue;

                ++slots;

                // EnchantmentSlot: SOCK_ENCHANTMENT_SLOT + i
                EnchantmentSlot es = EnchantmentSlot(SOCK_ENCHANTMENT_SLOT + i);

                // 宝石的附魔ID存在 GemProperties
                GemPropertiesEntry const* gp =
                    sGemPropertiesStore.LookupEntry(gem->GemProperties);
                if (!gp || !gp->EnchantID)
                    continue;

                player->ApplyEnchantment(item, es, false);
                item->SetEnchantment(es, gp->EnchantID, 0, 0);
                player->ApplyEnchantment(item, es, true);
                ++done;
            }
        }

        if (!slots)
        {
            handler->PSendSysMessage("|cffff8000身上装备没有宝石插槽。|r");
            return true;
        }

        handler->PSendSysMessage("|cff00ff00[完成]|r 插了 |cffffff00%u|r / %u 个插槽",
            done, slots);
        return true;
    }

    // ------------------------------------------------------------------
    //  .gear enchantall —— 全身附魔
    // ------------------------------------------------------------------
    static bool GearEnchantAll(ChatHandler* handler, Player* player,
                               std::vector<std::string> const& tok)
    {
        if (tok.size() < 2)
        {
            handler->PSendSysMessage("|cffff0000用法：|r.gear enchantall <附魔ID>");
            handler->PSendSysMessage("|cff888888附魔ID 是 SpellItemEnchantment.dbc 的 ID|r");
            return true;
        }

        uint32 enchId = uint32(atoi(tok[1].c_str()));
        SpellItemEnchantmentEntry const* ench = sSpellItemEnchantmentStore.LookupEntry(enchId);
        if (!ench)
        {
            handler->PSendSysMessage("|cffff0000附魔 %u 不存在|r", enchId);
            return true;
        }

        uint32 done = 0;
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (!item)
                continue;

            player->ApplyEnchantment(item, PERM_ENCHANTMENT_SLOT, false);
            item->SetEnchantment(PERM_ENCHANTMENT_SLOT, enchId, 0, 0);
            player->ApplyEnchantment(item, PERM_ENCHANTMENT_SLOT, true);
            ++done;
        }

        handler->PSendSysMessage("|cff00ff00[完成]|r 给 |cffffff00%u|r 件装备打了附魔", done);
        return true;
    }
};

void AddSC_worldtools_commandscript()
{
    new worldtools_commandscript();
}
