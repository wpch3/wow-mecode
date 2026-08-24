/*
 * ============================================================================
 *  step49  .pin —— 游荡 bot 永久化开关
 *
 *  【step62 修订 —— 重要】修复两个致命错误
 *    错误1: .pin 从 step49 起【从未成功过】
 *      Creature.cpp:1430  SaveToDB 第一行:
 *        if (IsNPCBot() && GetBotAI() && GetBotAI()->IsWanderer()) return;
 *      我把 UnsetWanderer() 放在 SaveToDB 后面 -> 永远被拦 -> spawnId=0
 *      -> 只写成 characters_npcbot，creature 表空 -> "半永久"死锁
 *      修法: UnsetWanderer 提到最前面（第1步）
 *
 *    错误2: .pin fix 会造成野指针崩服（用户实测"招募会闪退"）
 *      bot_ai.cpp:164  构造时缓存 _botData = SelectNpcBotData(entry)
 *      botdatamgr.cpp:3118  NPCBOT_UPDATE_ERASE 会 _botsData.erase()
 *      -> _botData 野指针 -> bot_ai.cpp 里 73 处解引用一碰就崩
 *      修法: 只删数据库，绝不调 UpdateNpcBotData(NPCBOT_UPDATE_ERASE)
 *
 *  【step60 修订】修复"状态自相矛盾"
 *    原来 .pin 用 SelectNpcBotData 判断（查 characters_npcbot），
 *    .pin off 用 GetSpawnId() 判断（查 world.creature）。
 *    两套标准 -> 两张表不一致时两个指令互相矛盾，用户被卡死：
 *        .pin      说"这个bot已经有持久化数据了"
 *        .pin off  说"这个bot本来就不是永久的"
 *    现在统一为 GetPinState()：固定 = 两张表【都有】。
 *    并新增失败回滚、.pin status（诊断）、.pin fix（自助修复）。
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
        handler->SendSysMessage("  .pin status          看这个bot的真实状态（两张表）");
        handler->SendSysMessage("  .pin fix             修复损坏状态（两张表不一致时）");
        handler->SendSysMessage("|cffffff00--- 中文别名 ---|r");
        handler->SendSysMessage("  .pin 固定 / 取消 / 列表 / 数量 / 状态 / 修复");

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
    // ==================================================================
    //  step60  统一的"固定状态"判定
    // ==================================================================
    //  【为什么要有这个】
    //  step49 的 .pin 用 SelectNpcBotData 判断（查 characters_npcbot），
    //  .pin off 用 GetSpawnId() 判断（查 world.creature 的内存映射）。
    //  两套标准 -> 当两张表不一致时，两个指令互相矛盾：
    //      .pin      说"已经有持久化数据了"
    //      .pin off  说"本来就不是永久的"
    //  用户被卡死，两个指令都进不去。
    //
    //  现在统一：固定 = 两张表【都有】。其余情况分别处理。
    enum PinState
    {
        PIN_STATE_FREE      = 0,    // 两张表都没有 -> 纯游荡bot，可以 .pin
        PIN_STATE_PINNED    = 1,    // 两张表都有   -> 已固定，可以 .pin off
        PIN_STATE_BROKEN_DATA_ONLY = 2,   // 只有 characters_npcbot
        PIN_STATE_BROKEN_SPAWN_ONLY = 3   // 只有 world.creature（启动会崩服）
    };

    //  hasData  : characters_npcbot 里有没有（botdatamgr.h:216 public 203段）
    //  hasSpawn : world.creature 里有没有
    static PinState GetPinState(uint32 entry, bool& hasData, bool& hasSpawn)
    {
        hasData = (BotDataMgr::SelectNpcBotData(entry) != nullptr);

        QueryResult res = WorldDatabase.PQuery(
            "SELECT `guid` FROM `creature` WHERE `id` = {} LIMIT 1", entry);
        hasSpawn = (res != nullptr);

        if (hasData && hasSpawn)   return PIN_STATE_PINNED;
        if (hasData && !hasSpawn)  return PIN_STATE_BROKEN_DATA_ONLY;
        if (!hasData && hasSpawn)  return PIN_STATE_BROKEN_SPAWN_ONLY;
        return PIN_STATE_FREE;
    }

    //  把损坏状态清干净，回到"纯游荡bot"
    //
    //  ==================================================================
    //  step62 【重大修正】绝对不能调 UpdateNpcBotData(NPCBOT_UPDATE_ERASE)
    //  ==================================================================
    //  用户实测：用了 .pin fix 之后招募 bot 会闪退。
    //
    //  根因（实查）：
    //    bot_ai.cpp:164  构造函数初始化列表里：
    //      _botData(const_cast<NpcBotData*>(BotDataMgr::SelectNpcBotData(...)))
    //    -> 每个 bot 在创建时就把 NpcBotData* 缓存进成员变量
    //
    //    botdatamgr.cpp:3114-3118  NPCBOT_UPDATE_ERASE:
    //      _botsData.erase(bitr);
    //    -> _botsData 是 unordered_map<uint32, NpcBotData>（存值不存指针）
    //    -> erase 之后那个 NpcBotData 对象【被析构】
    //    -> bot_ai::_botData 变成【野指针】
    //
    //    而 bot_ai.cpp 里有 73 处 `_botData->` 解引用，几乎没有判空
    //    -> 招募时一碰就崩服
    //
    //  【正确做法】只删数据库，不动内存。
    //  内存里的 _botsData 保持原样 -> _botData 指针依然有效 -> 不崩。
    //  重启后 LoadNpcBots 从库里读，自然就干净了。
    //
    //  代价：要重启才完全生效。但这比崩服强得多。
    static void CleanupBroken(uint32 entry, bool hasData, bool hasSpawn)
    {
        if (hasSpawn)
        {
            // creature 表这边可以安全删 —— 没有任何 bot_ai 成员缓存它
            QueryResult res = WorldDatabase.PQuery(
                "SELECT `guid` FROM `creature` WHERE `id` = {}", entry);
            if (res)
            {
                do
                {
                    uint32 sid = (*res)[0].GetUInt32();
                    // Creature.h:222  static bool DeleteFromDB(spawnId)  public(78段)
                    // Creature.cpp:1922 开头有 if (!data) return false; 判空，安全
                    Creature::DeleteFromDB(sid);
                } while (res->NextRow());
            }
        }

        if (hasData)
        {
            // 【只发 SQL，绕过 UpdateNpcBotData】
            // 不能用 BotDataMgr::UpdateNpcBotData(entry, NPCBOT_UPDATE_ERASE, nullptr)
            // —— 它会 _botsData.erase() 造成 bot_ai::_botData 野指针（见上面大段说明）
            CharacterDatabase.PExecute(
                "DELETE FROM `characters_npcbot` WHERE `entry` = {}", entry);
            CharacterDatabase.PExecute(
                "DELETE FROM `characters_npcbot_group_member` WHERE `entry` = {}", entry);
        }
    }

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

        // ---- step60: 用统一状态判定，不再用两套标准 ----
        bool hasData = false, hasSpawn = false;
        PinState st = GetPinState(entry, hasData, hasSpawn);

        if (st == PIN_STATE_PINNED)
        {
            handler->SendSysMessage("|cffff0000 这个bot已经是永久的了|r");
            handler->SendSysMessage("|cffffff00 想取消：.pin off|r");
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (st != PIN_STATE_FREE)
        {
            // 损坏状态：两张表只有一张有。这是 step49 没做回滚留下的。
            handler->SendSysMessage("|cffff0000 这个bot的固定数据【已损坏】|r");
            snprintf(buf, sizeof(buf), "|cffffff00   characters_npcbot: %s|r",
                     hasData ? "有" : "无");
            handler->SendSysMessage(buf);
            snprintf(buf, sizeof(buf), "|cffffff00   world.creature   : %s|r",
                     hasSpawn ? "有" : "无");
            handler->SendSysMessage(buf);
            handler->SendSysMessage("|cffffff00 用 .pin fix 修复它，然后再 .pin|r");
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

        // ====================================================================
        //  step62 【顺序修正】必须先退出游荡状态，否则 SaveToDB 会被拦
        // ====================================================================
        //  实查 Creature.cpp:1427-1432：
        //      void Creature::SaveToDB(uint32 mapid, uint8 spawnMask, uint32 phaseMask)
        //      {
        //          //npcbot: disallow saving generated bots
        //          if (IsNPCBot() && GetBotAI() && (GetBotAI()->IsWanderer() || IsSummon()))
        //              return;              <-- 【第一行就 return，什么都不做】
        //
        //  step49 我把 UnsetWanderer() 放在 SaveToDB 【后面】，
        //  导致 SaveToDB 永远被这道门拦下 -> m_spawnId 永远是 0
        //  -> 只写成了 characters_npcbot，creature 表是空的
        //  -> 就是用户遇到的"半永久"死锁状态。
        //
        //  【.pin 从 step49 交付起就没成功过一次】，这是我的失职。
        bot_ai* botAI = bot->GetBotAI();
        if (!botAI)
        {
            handler->SendSysMessage("|cffff0000 拿不到 bot AI，无法固定|r");
            handler->SetSentErrorMessage(true);
            return false;
        }

        // --- 第1步：退出游荡状态（必须最先做）---
        //  bot_ai.h:196  UnsetWanderer()  public(53段)  <- step44 新增
        botAI->UnsetWanderer();

        // --- 第2步：写 characters_npcbot ---
        uint8 bot_spec = BotDataMgr::SelectSpecForClass(extras->bclass);
        BotDataMgr::AddNpcBotData(entry,
            BotDataMgr::DefaultRolesForClass(extras->bclass, bot_spec),
            bot_spec,
            bot->GetCreatureTemplate()->faction);

        // --- 第3步：写 world.creature（现在不会被拦了）---
        //  Creature.h:221  SaveToDB(mapid, spawnMask, phaseMask)  public(78段)
        bot->SaveToDB(map->GetId(), (uint8(1) << map->GetSpawnMode()), player->GetPhaseMaskForSpawn());

        uint32 db_guid = bot->GetSpawnId();     // Creature.h:98 public(78段)

        // --- step60: 失败回滚 ---
        //  SaveToDB 没成功（db_guid 仍为0）时，第1步写进 characters_npcbot
        //  的记录必须撤掉，否则就留下"只有持久化数据没有实体"的损坏状态
        //  —— 这正是用户遇到的那个 .pin/.pin off 互相矛盾的死锁。
        if (!db_guid)
        {
            // step62: 回滚要做【两件事】
            //  1. 恢复游荡状态（第1步已经 UnsetWanderer 了，不恢复的话
            //     bot 会变成"不游荡也没落库"的孤儿，站着不动直到重启）
            //  2. 删掉第2步写进 characters_npcbot 的记录
            //
            //  bot_ai.cpp:20414  SetWanderer() 内部有 if (IAmFree())，
            //  无主bot 的 IAmFree() 返回 true（bot_ai.cpp:15381），所以能恢复。
            botAI->SetWanderer();

            // 【不能用 UpdateNpcBotData(NPCBOT_UPDATE_ERASE)】
            // 它会 _botsData.erase() -> bot_ai::_botData(bot_ai.cpp:164) 变野指针
            // -> 73处解引用一碰就崩。只删库不动内存。
            CharacterDatabase.PExecute(
                "DELETE FROM `characters_npcbot` WHERE `entry` = {}", entry);

            handler->SendSysMessage("|cffff0000 固定失败：SaveToDB 没能生成 spawnId|r");
            handler->SendSysMessage("|cffffff00 已回滚，bot 恢复游荡状态|r");
            handler->SetSentErrorMessage(true);
            return false;
        }

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
        //  step62: UnsetWanderer() 已经移到【最前面】（第1步）执行了，
        //  因为 SaveToDB 会拦 IsWanderer() 为真的 bot。这里不再重复调用。

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

        // ---- step60: 和 .pin 用【同一个】判定，不再看 spawnId ----
        //  原来这里用 GetSpawnId()==0 判断"不是永久的"，
        //  而 .pin 那边用 SelectNpcBotData 判断"已经是永久的"。
        //  两张表不一致时两个指令互相矛盾，用户被卡死。
        bool hasData = false, hasSpawn = false;
        PinState st = GetPinState(entry, hasData, hasSpawn);

        if (st == PIN_STATE_FREE)
        {
            handler->SendSysMessage("|cffff0000 这个bot本来就不是永久的|r");
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (st != PIN_STATE_PINNED)
        {
            handler->SendSysMessage("|cffff0000 这个bot的固定数据【已损坏】|r");
            snprintf(buf, sizeof(buf), "|cffffff00   characters_npcbot: %s|r",
                     hasData ? "有" : "无");
            handler->SendSysMessage(buf);
            snprintf(buf, sizeof(buf), "|cffffff00   world.creature   : %s|r",
                     hasSpawn ? "有" : "无");
            handler->SendSysMessage(buf);
            handler->SendSysMessage("|cffffff00 用 .pin fix 清理它|r");
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint32 const spawnId = bot->GetSpawnId();

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
        // step62: 【不能用 UpdateNpcBotData(NPCBOT_UPDATE_ERASE)】
        //  它会 _botsData.erase()，而 bot_ai.cpp:164 在构造时缓存了
        //  NpcBotData* 到 _botData 成员，erase 后变野指针，
        //  bot_ai.cpp 里 73 处 `_botData->` 解引用一碰就崩服。
        //  只删数据库，内存留给重启时自然清理。
        if (bdata)
            CharacterDatabase.PExecute(
                "DELETE FROM `characters_npcbot` WHERE `entry` = {}", entry);

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
    //  step60  .pin status —— 看真实状态（两张表分别列出来）
    // ------------------------------------------------------------------
    static bool DoStatus(ChatHandler* handler, Player* /*player*/)
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
        bool hasData = false, hasSpawn = false;
        PinState st = GetPinState(entry, hasData, hasSpawn);

        snprintf(buf, sizeof(buf), "|cff00ff00[%s] entry=%u|r", bot->GetName().c_str(), entry);
        handler->SendSysMessage(buf);

        snprintf(buf, sizeof(buf), "  characters_npcbot : %s",
                 hasData ? "|cff00ff00有|r" : "|cffff0000无|r");
        handler->SendSysMessage(buf);

        snprintf(buf, sizeof(buf), "  world.creature    : %s",
                 hasSpawn ? "|cff00ff00有|r" : "|cffff0000无|r");
        handler->SendSysMessage(buf);

        snprintf(buf, sizeof(buf), "  spawnId(内存)     : %u", bot->GetSpawnId());
        handler->SendSysMessage(buf);

        snprintf(buf, sizeof(buf), "  IsWandererBot()   : %s",
                 bot->IsWandererBot() ? "是" : "否");
        handler->SendSysMessage(buf);

        switch (st)
        {
            case PIN_STATE_FREE:
                handler->SendSysMessage("  判定: |cffffff00游荡bot（未固定）|r  可以用 .pin");
                handler->SendSysMessage("  |cff888888提示: 游荡bot每次重启都会重新洗牌，|r");
                handler->SendSysMessage("  |cff888888entry和位置都会变。.pin 成功后才固定。|r");
                break;
            case PIN_STATE_PINNED:
                handler->SendSysMessage("  判定: |cff00ff00已固定|r  可以用 .pin off");
                break;
            case PIN_STATE_BROKEN_DATA_ONLY:
                handler->SendSysMessage("  判定: |cffff0000损坏|r 只有持久化数据，没有实体");
                handler->SendSysMessage("        用 .pin fix 清理");
                break;
            case PIN_STATE_BROKEN_SPAWN_ONLY:
                handler->SendSysMessage("  判定: |cffff0000损坏|r 只有实体，没有持久化数据");
                handler->SendSysMessage("        |cffff0000这种状态下次重启会崩服|r，请立刻 .pin fix");
                break;
        }
        return true;
    }

    // ------------------------------------------------------------------
    //  step60  .pin fix —— 修复损坏状态，回到纯游荡bot
    // ------------------------------------------------------------------
    static bool DoFix(ChatHandler* handler, Player* /*player*/)
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
        bool hasData = false, hasSpawn = false;
        PinState st = GetPinState(entry, hasData, hasSpawn);

        if (st == PIN_STATE_FREE)
        {
            handler->SendSysMessage("|cff00ff00 这个bot状态正常（纯游荡bot），无需修复|r");
            return true;
        }

        if (st == PIN_STATE_PINNED)
        {
            handler->SendSysMessage("|cff00ff00 这个bot状态正常（已固定），无需修复|r");
            handler->SendSysMessage("|cffffff00 想取消固定用 .pin off|r");
            return true;
        }

        // ---- 损坏，清理 ----
        snprintf(buf, sizeof(buf), "|cffffff00 正在修复 entry=%u ...|r", entry);
        handler->SendSysMessage(buf);

        CleanupBroken(entry, hasData, hasSpawn);

        handler->SendSysMessage("|cff00ff00 已清理数据库里的孤儿记录|r");
        handler->SendSysMessage("|cffffff00 内存缓存【故意】不动 —— 动了会让bot_ai的|r");
        handler->SendSysMessage("|cffffff00 _botData变野指针，招募时崩服（step62实证）|r");
        handler->SendSysMessage("|cffff8800 所以：现在【不要招募这个bot】，先重启服务端|r");
        handler->SendSysMessage("|cffffff00 另外：游荡bot本来每次重启就重新洗牌，|r");
        handler->SendSysMessage("|cffffff00 这个entry重启后可能就不存在了，属正常现象|r");
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

        // step60: 新增两个诊断/修复子命令
        if (s0 == "status" || s0 == "stat" || tok[0] == "状态")
            return DoStatus(handler, player);

        if (s0 == "fix" || s0 == "repair" || tok[0] == "修复")
            return DoFix(handler, player);

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
