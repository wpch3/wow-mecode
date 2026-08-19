/*
 * ============================================================================
 *  step49  .pin —— 游荡 bot 永久化开关
 * ============================================================================
 *
 *  用户需求：「给游荡bot做个是否永久存在的开关，
 *              可以让npcbot重启就消失或者永久存在。
 *              这个功能不用做gm专用，只不过除去gm2-3其他的都最多只能留4个」
 *
 *  ==========================================================================
 *  【为什么游荡bot重启就没了】—— 实查根因
 *  ==========================================================================
 *
 *    botdatamgr.cpp:210  SpawnWandererBot()
 *        bot->LoadBotCreatureFromDB(0, map, true, true, bot_id, &spawnPos);
 *                                   ^          ^^^^
 *                                   |          generated = true
 *                                   spawnId = 0
 *
 *    Creature.cpp:3531
 *        CreatureData const* data = generated ? nullptr : sObjectMgr->GetCreatureData(spawnId);
 *        -> generated=true 时【完全不读 creature 表】，纯内存对象
 *
 *    botdatamgr.cpp:460  注释写得很清楚：
 *        //We do not create CreatureData for generated bots
 *
 *    => 游荡bot从来没进过数据库，重启当然没了。
 *
 *  ==========================================================================
 *  【怎么让它永久】—— 照抄官方 .npcbot spawn 的落库四步
 *  ==========================================================================
 *
 *    botcommands.cpp:3860-3873  HandleNpcBotSpawnCommand 的核心：
 *
 *      1. BotDataMgr::AddNpcBotData(...)     -> 写 characters_npcbot
 *      2. creature->SaveToDB(...)             -> 写 world.creature
 *      3. creature->LoadBotCreatureFromDB(db_guid, map)  -> 用真 spawnId 重载
 *      4. sObjectMgr->AddCreatureToGrid(...)  -> 挂进网格
 *
 *    【两张表必须同时写】这是硬约束，实查证据：
 *
 *    botdatamgr.cpp:1168-1176  服务端启动时交叉校验
 *      for (auto const& [_, cdata] : sObjectMgr->GetAllCreatureData())
 *          if (cdata.id >= BOT_ENTRY_BEGIN && ...IsNPCBot() && 不在entryList里)
 *              invalid_ids.push_back(cdata.id);
 *      ...
 *      ABORT_MSG("Invalid NPCBot spawns found in `creature` table
 *                 having no data in `characters_npcbot` table!");
 *
 *    -> 只写 creature 不写 characters_npcbot = 【下次启动直接崩服】
 *
 *  ==========================================================================
 *  【权限设计】按用户要求
 *  ==========================================================================
 *    SEC_GAMEMASTER(2) / SEC_ADMINISTRATOR(3)  -> 不限数量
 *    其它（SEC_PLAYER 0 / SEC_MODERATOR 1）    -> 最多 4 个
 *
 *    Common.h:40-43  SEC_PLAYER=0 / MODERATOR=1 / GAMEMASTER=2 / ADMINISTRATOR=3
 *    WorldSession.h:534  GetSecurity()  public(494段)
 *
 *  权限：rbac::RBAC_PERM_COMMAND_WORLDTOOLS（step21 自建 = 71012）
 *  【注意】这个 rbac 权限普通玩家默认没有。如果要让普通玩家也能用，
 *          需要在 rbac 表里给默认角色加这个权限，见安装说明第六章。
 * ============================================================================
 */

#include "ScriptMgr.h"
#include "Chat.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "Map.h"
#include "MapManager.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "RBAC.h"
#include "World.h"
#include "WorldSession.h"

#include "botdatamgr.h"
#include "botcommon.h"
#include "bot_ai.h"

#include <cstdio>
#include <string>
#include <vector>

// ============================================================================
//  非GM玩家的永久bot数量上限
// ============================================================================
//  用户要求：「除去gm2-3其他的都最多只能留4个」
static constexpr uint32 PIN_LIMIT_NORMAL = 4;

// ============================================================================
//  小工具
// ============================================================================
static std::vector<std::string> PinTok(char const* args)
{
    std::vector<std::string> out;
    if (!args)
        return out;
    std::string s(args);
    size_t i = 0;
    while (i < s.size())
    {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
            ++i;
        if (i >= s.size())
            break;
        size_t start = i;
        while (i < s.size() && s[i] != ' ' && s[i] != '\t')
            ++i;
        out.push_back(s.substr(start, i - start));
    }
    return out;
}

static std::string PinLower(std::string s)
{
    for (char& c : s)
        if (c >= 'A' && c <= 'Z')
            c = char(c - 'A' + 'a');
    return s;
}

// ============================================================================
//  .pin 实现
// ============================================================================
class botpersist_commandscript : public CommandScript
{
public:
    botpersist_commandscript() : CommandScript("botpersist_commandscript") { }

    std::vector<ChatCommand> GetCommands() const override
    {
        static std::vector<ChatCommand> commandTable =
        {
            { "pin", rbac::RBAC_PERM_COMMAND_WORLDTOOLS, false, &HandlePin, "" },
        };
        return commandTable;
    }

    static void SendHelp(ChatHandler* handler)
    {
        char buf[512];
        handler->SendSysMessage("|cff00ff00[.pin 游荡bot永久化]|r");
        handler->SendSysMessage("  .pin                 把【选中的】游荡bot设为永久");
        handler->SendSysMessage("  .pin off             取消永久（重启后消失）");
        handler->SendSysMessage("  .pin list            列出你固定的bot");
        handler->SendSysMessage("  .pin count           看还能固定几个");
        handler->SendSysMessage("|cffffff00--- 中文别名 ---|r");
        handler->SendSysMessage("  .pin 固定 / 取消 / 列表 / 数量");

        AccountTypes sec = handler->GetSession() ? handler->GetSession()->GetSecurity() : SEC_PLAYER;
        if (sec >= SEC_GAMEMASTER)
            handler->SendSysMessage("|cff00ff00 你是GM，不限数量|r");
        else
        {
            snprintf(buf, sizeof(buf), "|cffffff00 你最多能固定 %u 个|r", PIN_LIMIT_NORMAL);
            handler->SendSysMessage(buf);
        }
        handler->SendSysMessage("|cffff0000 注意：固定后它就不再游荡，会站在原地|r");
    }

    // ------------------------------------------------------------------
    //  数一个玩家固定了几个 bot
    // ------------------------------------------------------------------
    //  用 world.creature.ScriptName 存"谁固定的"（这是个不占用业务字段的做法）。
    //  格式： pin:<玩家guid低位>
    static uint32 CountPinned(uint32 ownerGuidLow)
    {
        std::string tag = "pin:" + std::to_string(ownerGuidLow);
        // 铁律5：PQuery 走 fmt 库，占位符是 {} 不是 %u
        QueryResult res = WorldDatabase.PQuery(
            "SELECT COUNT(*) FROM `creature` WHERE `ScriptName` = '{}'", tag);
        if (!res)
            return 0;
        return (*res)[0].GetUInt32();
    }

    // ------------------------------------------------------------------
    //  把选中的游荡bot固定下来
    // ------------------------------------------------------------------
    static bool DoPin(ChatHandler* handler, Player* player)
    {
        char buf[512];

        Unit* sel = handler->getSelectedUnit();
        if (!sel || sel->GetTypeId() != TYPEID_UNIT)
        {
            handler->SendSysMessage("|cffff0000 请先选中一个游荡bot|r");
            handler->SetSentErrorMessage(true);
            return false;
        }

        Creature* bot = sel->ToCreature();
        if (!bot || !bot->IsNPCBot())
        {
            handler->SendSysMessage("|cffff0000 这不是一个NPCBot|r");
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!bot->IsWandererBot())
        {
            handler->SendSysMessage("|cffff0000 这不是游荡bot|r");
            handler->SendSysMessage("|cffffff00 已经固定/已被招募的bot本来就是永久的|r");
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint32 const entry = bot->GetEntry();

        // ---- 数量限制（GM 2-3 不限）----
        AccountTypes sec = handler->GetSession()->GetSecurity();   // WorldSession.h:534 public
        if (sec < SEC_GAMEMASTER)                                  // Common.h:42
        {
            uint32 have = CountPinned(player->GetGUID().GetCounter());
            if (have >= PIN_LIMIT_NORMAL)
            {
                snprintf(buf, sizeof(buf),
                         "|cffff0000 你已经固定了 %u 个，上限 %u 个|r", have, PIN_LIMIT_NORMAL);
                handler->SendSysMessage(buf);
                handler->SendSysMessage("|cffffff00 用 .pin list 看看，先 .pin off 掉一个|r");
                handler->SetSentErrorMessage(true);
                return false;
            }
        }

        // ---- 已经在 characters_npcbot 里就不能重复固定 ----
        //  botcommands.cpp:3800 官方同款检查
        if (BotDataMgr::SelectNpcBotData(entry))
        {
            handler->SendSysMessage("|cffff0000 这个bot已经有持久化数据了|r");
            handler->SetSentErrorMessage(true);
            return false;
        }

        // ---- creature 表里也不能已存在 ----
        QueryResult res = WorldDatabase.PQuery(
            "SELECT `guid` FROM `creature` WHERE `id` = {}", entry);
        if (res)
        {
            handler->SendSysMessage("|cffff0000 这个bot在 creature 表里已存在|r");
            handler->SetSentErrorMessage(true);
            return false;
        }

        Map* map = bot->GetMap();
        if (map->Instanceable())
        {
            handler->SendSysMessage("|cffff0000 不能在副本里固定bot|r");
            handler->SetSentErrorMessage(true);
            return false;
        }

        NpcBotExtras const* extras = BotDataMgr::SelectNpcBotExtras(entry);
        if (!extras)
        {
            snprintf(buf, sizeof(buf), "|cffff0000 找不到bot %u 的职业/种族数据|r", entry);
            handler->SendSysMessage(buf);
            handler->SetSentErrorMessage(true);
            return false;
        }

        // ====================================================================
        //  照抄官方 botcommands.cpp:3860-3873 的落库四步
        // ====================================================================
        //  【顺序不能变】两张表必须同时写，否则下次启动
        //  botdatamgr.cpp:1174 的交叉校验会 ABORT_MSG 崩服。

        // --- 第1步：写 characters_npcbot ---
        uint8 bot_spec = BotDataMgr::SelectSpecForClass(extras->bclass);
        BotDataMgr::AddNpcBotData(entry,
            BotDataMgr::DefaultRolesForClass(extras->bclass, bot_spec),
            bot_spec,
            bot->GetCreatureTemplate()->faction);

        // --- 第2步：写 world.creature ---
        //  Creature.h:221  SaveToDB(mapid, spawnMask, phaseMask)  public(78段)
        bot->SaveToDB(map->GetId(), (uint8(1) << map->GetSpawnMode()), player->GetPhaseMaskForSpawn());

        uint32 db_guid = bot->GetSpawnId();     // Creature.h:98 public(78段)

        // --- 第3步：打上"谁固定的"标记 ---
        //  用 ScriptName 存 tag，不占用任何业务字段。
        //  ScriptName 对 NPCBot 无实际作用（bot 走 bot_ai 不走 ScriptName）。
        std::string tag = "pin:" + std::to_string(player->GetGUID().GetCounter());
        WorldDatabase.PExecute(
            "UPDATE `creature` SET `ScriptName` = '{}' WHERE `guid` = {}", tag, db_guid);

        // --- 第4步：挂进网格 ---
        //  ObjectMgr.h:1458  AddCreatureToGrid  public(947段)
        sObjectMgr->AddCreatureToGrid(db_guid, sObjectMgr->GetCreatureData(db_guid));

        // --- 第5步【关键】退出游荡状态 ---
        //  Creature.cpp:3617  IsWandererBot() 走 bot_AI->IsWanderer()
        //  不清掉的话：
        //    1. 它会继续按路点游走，"固定"就没意义了
        //    2. bot_ai.cpp:15873/15926/15933 会解引用 _travel_node_cur -> 崩服
        //       （这正是 step44 修的那个 bug）
        //
        //  UnsetWanderer() 是 step44 新增的方法（bot_ai.h:196 下面）。
        //  【依赖】必须先装 step44 的补丁，否则这里编译不过。
        if (bot_ai* ai = bot->GetBotAI())
            ai->UnsetWanderer();

        snprintf(buf, sizeof(buf), "|cff00ff00 %s 已固定（重启后依然存在）|r", bot->GetName().c_str());
        handler->SendSysMessage(buf);
        snprintf(buf, sizeof(buf), "|cffffff00 entry=%u  spawnId=%u|r", entry, db_guid);
        handler->SendSysMessage(buf);

        if (sec < SEC_GAMEMASTER)
        {
            uint32 have = CountPinned(player->GetGUID().GetCounter());
            snprintf(buf, sizeof(buf), "|cffffff00 你已固定 %u / %u 个|r", have, PIN_LIMIT_NORMAL);
            handler->SendSysMessage(buf);
        }

        handler->SendSysMessage("|cffffff00 提示：它现在不再游荡，会留在这个位置|r");
        handler->SendSysMessage("|cffffff00 想让它消失：.pin off（选中它）|r");
        return true;
    }

    // ------------------------------------------------------------------
    //  取消永久
    // ------------------------------------------------------------------
    static bool DoUnpin(ChatHandler* handler, Player* player)
    {
        char buf[512];

        Unit* sel = handler->getSelectedUnit();
        if (!sel || sel->GetTypeId() != TYPEID_UNIT)
        {
            handler->SendSysMessage("|cffff0000 请先选中一个bot|r");
            handler->SetSentErrorMessage(true);
            return false;
        }

        Creature* bot = sel->ToCreature();
        if (!bot || !bot->IsNPCBot())
        {
            handler->SendSysMessage("|cffff0000 这不是一个NPCBot|r");
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint32 const entry = bot->GetEntry();
        uint32 const spawnId = bot->GetSpawnId();

        if (!spawnId)
        {
            handler->SendSysMessage("|cffff0000 这个bot本来就不是永久的|r");
            handler->SetSentErrorMessage(true);
            return false;
        }

        // ---- 非GM只能取消自己固定的 ----
        AccountTypes sec = handler->GetSession()->GetSecurity();
        if (sec < SEC_GAMEMASTER)
        {
            std::string tag = "pin:" + std::to_string(player->GetGUID().GetCounter());
            QueryResult res = WorldDatabase.PQuery(
                "SELECT `guid` FROM `creature` WHERE `guid` = {} AND `ScriptName` = '{}'",
                spawnId, tag);
            if (!res)
            {
                handler->SendSysMessage("|cffff0000 这不是你固定的bot|r");
                handler->SendSysMessage("|cffffff00 只有GM能取消别人固定的|r");
                handler->SetSentErrorMessage(true);
                return false;
            }
        }

        // ---- 有主人的bot不能这样删（那是招募来的，要用 .npcbot remove）----
        NpcBotData const* bdata = BotDataMgr::SelectNpcBotData(entry);
        if (bdata && bdata->owner != 0)
        {
            handler->SendSysMessage("|cffff0000 这个bot已被招募，请先 .npcbot remove|r");
            handler->SetSentErrorMessage(true);
            return false;
        }

        // ====================================================================
        //  反向四步：先删网格/世界，再删两张表
        // ====================================================================

        // --- 从世界移除 ---
        bot->CombatStop();
        bot->DeleteFromDB(spawnId);      // Creature.h:222  static，删 creature 表 + 网格
        bot->AddObjectToRemoveList();

        // --- 删 characters_npcbot ---
        //  botdatamgr.h:81  NPCBOT_UPDATE_ERASE
        //  botdatamgr.cpp:3114  会 _botsData.erase + DELETE FROM characters_npcbot
        if (bdata)
            BotDataMgr::UpdateNpcBotData(entry, NPCBOT_UPDATE_ERASE, nullptr);

        snprintf(buf, sizeof(buf), "|cff00ff00 已取消固定（entry=%u）|r", entry);
        handler->SendSysMessage(buf);
        handler->SendSysMessage("|cffffff00 它会在下次服务端重启时重新变回游荡bot|r");
        return true;
    }

    // ------------------------------------------------------------------
    //  列出固定的bot
    // ------------------------------------------------------------------
    static bool DoList(ChatHandler* handler, Player* player)
    {
        char buf[512];

        AccountTypes sec = handler->GetSession()->GetSecurity();
        std::string tag = "pin:" + std::to_string(player->GetGUID().GetCounter());

        QueryResult res;
        if (sec >= SEC_GAMEMASTER)
        {
            // GM 看全部
            res = WorldDatabase.Query(
                "SELECT c.`guid`, c.`id`, ct.`name`, c.`map`, c.`ScriptName` "
                "FROM `creature` c JOIN `creature_template` ct ON ct.`entry` = c.`id` "
                "WHERE c.`ScriptName` LIKE 'pin:%' ORDER BY c.`guid`");
            handler->SendSysMessage("|cff00ff00[全服固定的bot]|r");
        }
        else
        {
            res = WorldDatabase.PQuery(
                "SELECT c.`guid`, c.`id`, ct.`name`, c.`map`, c.`ScriptName` "
                "FROM `creature` c JOIN `creature_template` ct ON ct.`entry` = c.`id` "
                "WHERE c.`ScriptName` = '{}' ORDER BY c.`guid`", tag);
            handler->SendSysMessage("|cff00ff00[你固定的bot]|r");
        }

        if (!res)
        {
            handler->SendSysMessage("|cffffff00 一个都没有|r");
            return true;
        }

        uint32 n = 0;
        do
        {
            Field* f = res->Fetch();
            snprintf(buf, sizeof(buf), "  |cff00ff00%s|r  entry=%u spawnId=%u 地图%u  %s",
                     f[2].GetCString(), f[1].GetUInt32(), f[0].GetUInt32(), f[3].GetUInt32(),
                     (sec >= SEC_GAMEMASTER) ? f[4].GetCString() : "");
            handler->SendSysMessage(buf);
            ++n;
        } while (res->NextRow() && n < 50);

        snprintf(buf, sizeof(buf), "|cffffff00 共 %u 个|r", n);
        handler->SendSysMessage(buf);
        return true;
    }

    // ------------------------------------------------------------------
    static bool HandlePin(ChatHandler* handler, char const* args)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        char buf[512];
        std::vector<std::string> tok = PinTok(args);

        // ---------- 无参数 = 固定选中的 ----------
        if (tok.empty())
            return DoPin(handler, player);

        std::string s0 = PinLower(tok[0]);

        if (s0 == "off" || s0 == "remove" || tok[0] == "取消" || tok[0] == "解除")
            return DoUnpin(handler, player);

        if (s0 == "list" || tok[0] == "列表")
            return DoList(handler, player);

        if (s0 == "count" || tok[0] == "数量")
        {
            AccountTypes sec = handler->GetSession()->GetSecurity();
            uint32 have = CountPinned(player->GetGUID().GetCounter());
            if (sec >= SEC_GAMEMASTER)
                snprintf(buf, sizeof(buf), "|cff00ff00 你固定了 %u 个（GM不限量）|r", have);
            else
                snprintf(buf, sizeof(buf), "|cff00ff00 你固定了 %u / %u 个|r", have, PIN_LIMIT_NORMAL);
            handler->SendSysMessage(buf);
            return true;
        }

        if (s0 == "on" || tok[0] == "固定")
            return DoPin(handler, player);

        SendHelp(handler);
        return true;
    }
};

void AddSC_botpersist_commandscript()
{
    new botpersist_commandscript();
}

/* ============================================================================
 *  API 核实记录（全部含访问段，逐条 grep 实查）
 * ============================================================================
 *  【根因】
 *  botdatamgr.cpp:210   LoadBotCreatureFromDB(0, map, true, true, ...)
 *                       spawnId=0 + generated=true -> 纯内存，不落库
 *  botdatamgr.cpp:460   //We do not create CreatureData for generated bots
 *  Creature.cpp:3531    generated ? nullptr : GetCreatureData(spawnId)
 *
 *  【落库四步 —— 照抄官方 .npcbot spawn】
 *  botcommands.cpp:3860 AddNpcBotData(...)
 *  botcommands.cpp:3862 creature->SaveToDB(...)
 *  botcommands.cpp:3865 creature->LoadBotCreatureFromDB(db_guid, map)
 *  botcommands.cpp:3873 sObjectMgr->AddCreatureToGrid(...)
 *
 *  【硬约束：两张表必须同时写】
 *  botdatamgr.cpp:1168-1176  启动时交叉校验
 *      creature 表有 bot 但 characters_npcbot 没有 -> ABORT_MSG 崩服
 *
 *  【API】
 *  botdatamgr.h:215  AddNpcBotData()          public(203段)
 *  botdatamgr.h:216  SelectNpcBotData()       public(203段)
 *  botdatamgr.h:232  FindBot(uint32)          public(203段)
 *  botdatamgr.h:81   NPCBOT_UPDATE_ERASE
 *  botdatamgr.cpp:3114  ERASE 分支：_botsData.erase + DELETE FROM characters_npcbot
 *  Creature.h:98     GetSpawnId()             public(78段)
 *  Creature.h:221    SaveToDB(mapid,mask,phase) public(78段)
 *  Creature.h:222    DeleteFromDB(spawnId) static public(78段)
 *  ObjectMgr.h:1458  AddCreatureToGrid()      public(947段)
 *  WorldSession.h:534  GetSecurity()          public(494段)
 *  Common.h:40-43    SEC_PLAYER=0/MODERATOR=1/GAMEMASTER=2/ADMINISTRATOR=3
 *
 *  【铁律遵守】
 *  - PQuery/PExecute 用 {} 不是 %u（DatabaseWorkerPool.h:99/133）
 *  - 所有 API 都确认了访问段
 *  - 不硬编码 entry/guid，全部运行时取
 * ============================================================================
 */
