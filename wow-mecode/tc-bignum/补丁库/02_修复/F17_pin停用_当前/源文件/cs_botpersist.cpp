/*
 * ============================================================================
 *  step49  .pin —— 游荡 bot 永久化开关
 *
 *  【step63 修订 —— 判断模型重做】
 *    前两版把「有内存数据 + 无creature记录」判成"损坏"，
 *    但实证 botdatamgr.cpp:452/460 —— 那【恰恰是游荡bot的正常状态】：
 *      _botsData.emplace(...);                             <- 内存塞一条
 *      //We do not create CreatureData for generated bots  <- 故意不写creature表
 *    且 botdatamgr.cpp:199 ASSERT(bot_data) —— 游荡bot必须有内存数据。
 *
 *    更糟：SelectNpcBotData 读【内存】(botdatamgr.cpp:2929)，
 *    而 step62 的 fix 只发 DELETE SQL（为避开野指针）-> 内存永不变
 *    -> 判定永远"损坏" -> 用户 fix 完再 pin 还是叫他 fix，死循环。
 *
 *    新模型以 Creature.cpp:3617 IsWandererBot() 为权威判据。
 *    真正的"损坏"只有一种：creature表有 + 内存无（启动会崩服）。
 *
 *  【step62 修订】修复两个致命错误
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
        handler->SendSysMessage("|cffff0000  .pin  【已停用】游荡bot无法持久化，会崩服|r");
        handler->SendSysMessage("|cffffff00        原因: 它的模板是内存临时对象|r");
        handler->SendSysMessage("|cff00ff00        替代: 用官方 .npcbot spawn 创建永久bot|r");
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
        PIN_STATE_WANDERING = 0,    // 游荡bot -> 可以 .pin
        PIN_STATE_PINNED    = 1,    // 已固定  -> 可以 .pin off
        PIN_STATE_RECRUITED = 2,    // 已被玩家招募 -> 要用 .npcbot remove
        PIN_STATE_BROKEN    = 3     // 真损坏：creature表有记录但内存无数据（会崩服）
    };

    // ==================================================================
    //  step63 【判断模型重做】—— 前两版的模型是错的
    // ==================================================================
    //  【step60/62 的错误】我把「characters_npcbot有 + world.creature无」
    //  判定成"损坏"。**那恰恰是游荡bot的正常状态。**
    //
    //  实证 botdatamgr.cpp:452（生成游荡bot时）：
    //      _botsData.emplace(...);                       <- 往内存塞一条
    //      ...
    //      //We do not create CreatureData for generated bots   <- :460 官方注释
    //
    //  实证 botdatamgr.cpp:199（生成前的断言）：
    //      ASSERT(bot_data);      <- 游荡bot【必须】有 _botsData，没有就崩服
    //
    //  所以「有内存数据、无creature记录」= 游荡bot应有的样子，不是损坏。
    //
    //  【还有一个致命点】SelectNpcBotData 读的是【内存】不是数据库：
    //      botdatamgr.cpp:2929  _botsData.find(entry)
    //  而 step62 的 fix 只发 DELETE SQL（为了避开野指针崩服）
    //  -> 内存永远不变 -> 判定永远是"损坏" -> 用户陷入死循环。
    //
    //  【新模型】以 IsWandererBot() 为权威判据：
    //      Creature.cpp:3617  bot_AI ? bot_AI->IsWanderer() : ...
    //  它直接反映 bot 的真实身份，不受两张表状态影响。
    static PinState GetPinState(Creature* bot, bool& hasData, bool& hasSpawn, uint32& owner)
    {
        uint32 const entry = bot->GetEntry();

        // botdatamgr.h:216  SelectNpcBotData  public(203段)  —— 读内存 _botsData
        NpcBotData const* bdata = BotDataMgr::SelectNpcBotData(entry);
        hasData = (bdata != nullptr);
        owner   = bdata ? bdata->owner : 0;      // botdatamgr.h:92  uint32 owner

        QueryResult res = WorldDatabase.PQuery(
            "SELECT `guid` FROM `creature` WHERE `id` = {} LIMIT 1", entry);
        hasSpawn = (res != nullptr);

        // ---- 判断顺序很重要，从最特殊到最一般 ----

        // 1) 被玩家招募了 -> 归 .npcbot remove 管，我们不插手
        if (owner != 0)
            return PIN_STATE_RECRUITED;

        // 2) 真损坏：creature 表有记录，但内存里没数据
        //    这种下次启动 botdatamgr.cpp:1174 会 ABORT_MSG 崩服，必须修
        if (hasSpawn && !hasData)
            return PIN_STATE_BROKEN;

        // 3) 还在游荡 -> 可以固定
        //    Creature.h:396  IsWandererBot()  public(78段)
        if (bot->IsWandererBot())
            return PIN_STATE_WANDERING;

        // 4) 不游荡了 + creature表有记录 = 已经固定成功
        if (hasSpawn)
            return PIN_STATE_PINNED;

        // 5) 不游荡、无creature记录、无主
        //    多半是 .pin 中途失败留下的中间态，当作可以重新固定
        return PIN_STATE_WANDERING;
    }

    //  只清理【真损坏】—— 删 world.creature 里的孤儿记录
    //
    //  【绝对不碰 _botsData】：bot_ai.cpp:164 构造时缓存了 NpcBotData*，
    //  erase 会让那 73 处 `_botData->` 解引用变成野指针（step62 实证崩服）。
    //  而且游荡bot的 _botsData 是运行时生成的，删了下次 ASSERT 直接崩。
    static void CleanupBroken(uint32 entry)
    {
        QueryResult res = WorldDatabase.PQuery(
            "SELECT `guid` FROM `creature` WHERE `id` = {}", entry);
        if (!res)
            return;

        do
        {
            uint32 sid = (*res)[0].GetUInt32();
            // Creature.h:222  static bool DeleteFromDB(spawnId)  public(78段)
            // Creature.cpp:1922 开头 if (!data) return false; 判空安全
            Creature::DeleteFromDB(sid);
        } while (res->NextRow());
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

        // step64: entry 已不再使用（.pin 停用），移除避免 C4189 警告

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

        // ==================================================================
        //  step64 【功能停用】游荡bot根本不可能被持久化
        // ==================================================================
        //  用户实测 step63 版本：.pin 之后 worldserver.exe 直接闪退。
        //
        //  【根因】游荡bot的 creature_template 是【内存临时对象】：
        //    botdatamgr.cpp:63   static CreatureTemplateContainer _botsExtraCreatureTemplates;
        //    botdatamgr.cpp:344  CreatureTemplate& t = _botsExtraCreatureTemplates[next_bot_id];
        //
        //  ObjectMgr.cpp:10274-10279 对这类 entry 有硬断言：
        //    if (entry >= BOT_ENTRY_CREATE_BEGIN)
        //        if (CreatureTemplate const* t = BotDataMgr::GetBotExtraCreatureTemplate(entry))
        //            //custom creature template should only exist in custom container
        //            ASSERT_NODEBUGINFO(_creatureTemplateStore.find(entry) == _creatureTemplateStore.end());
        //
        //  -> 把游荡bot写进 world.creature 表，等于让一个"临时模板"的 entry
        //     出现在持久化数据里 -> 下次 GetCreatureTemplate 就踩断言 -> 崩服。
        //
        //  而且 botdatamgr.cpp:787 CleanExtraBotData 在 despawn 时会
        //  _botsExtraCreatureTemplates.erase() 并把 entry 回收复用，
        //  就算不崩，creature 表里那条记录下次也会对应完全不同的bot。
        //
        //  【最关键】Creature.cpp:1429 那道拦截不是bug，是保护：
        //    //npcbot: disallow saving generated bots
        //    if (IsNPCBot() && GetBotAI() && (GetBotAI()->IsWanderer() || IsSummon()))
        //        return;
        //  step63 我把 UnsetWanderer() 提前来绕过它，等于拆了安全气囊。
        //
        //  【正确的路】不是"固定游荡bot"，而是"用它的属性【新建】一个真bot"
        //  （复制种族/职业/等级/外观/装备到一个专用entry段，写4张表）。
        //  那是另一个功能，要单独设计。见 step64 根因文档第三章。
        handler->SendSysMessage("|cffff0000[.pin 已停用]|r");
        handler->SendSysMessage("|cffffff00 游荡bot的模板是【内存临时对象】|r");
        handler->SendSysMessage("|cffffff00 (botdatamgr.cpp:63 _botsExtraCreatureTemplates)|r");
        handler->SendSysMessage("|cffffff00 写进数据库会让 ObjectMgr.cpp:10279 断言失败 -> 崩服|r");
        handler->SendSysMessage("|cff00ff00 想要永久bot请用官方指令: .npcbot spawn|r");
        handler->SendSysMessage("|cff888888 诊断用 .pin status 仍可用（只读，安全）|r");
        handler->SetSentErrorMessage(true);
        return false;
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

        // ---- step63: 新判断模型 ----
        bool hasData = false, hasSpawn = false;
        uint32 owner = 0;
        PinState st = GetPinState(bot, hasData, hasSpawn, owner);

        if (st == PIN_STATE_WANDERING)
        {
            handler->SendSysMessage("|cffff0000 这个bot本来就不是永久的（还在游荡）|r");
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (st == PIN_STATE_RECRUITED)
        {
            handler->SendSysMessage("|cffff0000 这个bot已被招募，请用 .npcbot remove|r");
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (st == PIN_STATE_BROKEN)
        {
            handler->SendSysMessage("|cffff0000 数据损坏，请先 .pin fix|r");
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
        uint32 owner = 0;
        PinState st = GetPinState(bot, hasData, hasSpawn, owner);

        snprintf(buf, sizeof(buf), "|cff00ff00[%s] entry=%u|r", bot->GetName().c_str(), entry);
        handler->SendSysMessage(buf);

        snprintf(buf, sizeof(buf), "  内存 _botsData    : %s",
                 hasData ? "|cff00ff00有|r" : "|cffff0000无|r");
        handler->SendSysMessage(buf);

        snprintf(buf, sizeof(buf), "  world.creature    : %s",
                 hasSpawn ? "|cff00ff00有|r" : "|cffff0000无|r");
        handler->SendSysMessage(buf);

        snprintf(buf, sizeof(buf), "  owner(主人)       : %u%s", owner,
                 owner ? "" : "  (无主)");
        handler->SendSysMessage(buf);

        snprintf(buf, sizeof(buf), "  spawnId(内存)     : %u", bot->GetSpawnId());
        handler->SendSysMessage(buf);

        snprintf(buf, sizeof(buf), "  IsWandererBot()   : %s",
                 bot->IsWandererBot() ? "是" : "否");
        handler->SendSysMessage(buf);

        switch (st)
        {
            case PIN_STATE_WANDERING:
                handler->SendSysMessage("  判定: |cffffff00游荡bot（未固定）|r  可以用 .pin");
                handler->SendSysMessage("  |cff888888注: 游荡bot【本来就】只有内存数据、|r");
                handler->SendSysMessage("  |cff888888没有creature记录，这是正常的不是损坏。|r");
                handler->SendSysMessage("  |cff888888每次重启会重新洗牌，entry和位置都会变。|r");
                break;
            case PIN_STATE_PINNED:
                handler->SendSysMessage("  判定: |cff00ff00已固定|r  重启后依然存在");
                handler->SendSysMessage("  |cff888888想取消用 .pin off|r");
                break;
            case PIN_STATE_RECRUITED:
                handler->SendSysMessage("  判定: |cff00ffff已被玩家招募|r");
                handler->SendSysMessage("  |cff888888招募的bot本来就是永久的，用 .npcbot remove 解雇|r");
                break;
            case PIN_STATE_BROKEN:
                handler->SendSysMessage("  判定: |cffff0000损坏|r creature表有记录但内存无数据");
                handler->SendSysMessage("  |cffff0000下次重启会 ABORT_MSG 崩服，请立刻 .pin fix|r");
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
        uint32 owner = 0;
        PinState st = GetPinState(bot, hasData, hasSpawn, owner);

        if (st != PIN_STATE_BROKEN)
        {
            handler->SendSysMessage("|cff00ff00 这个bot状态正常，不需要修复|r");
            switch (st)
            {
                case PIN_STATE_WANDERING:
                    handler->SendSysMessage("|cffffff00 当前: 游荡bot。直接用 .pin 就能固定。|r");
                    handler->SendSysMessage("|cff888888 提示: 游荡bot只有内存数据没有creature记录，|r");
                    handler->SendSysMessage("|cff888888 这是【正常的】，不是损坏。|r");
                    break;
                case PIN_STATE_PINNED:
                    handler->SendSysMessage("|cffffff00 当前: 已固定。想取消用 .pin off|r");
                    break;
                case PIN_STATE_RECRUITED:
                    handler->SendSysMessage("|cffffff00 当前: 已被招募。用 .npcbot remove 解雇|r");
                    break;
                default:
                    break;
            }
            return true;
        }

        // ---- 只有真损坏才走到这 ----
        snprintf(buf, sizeof(buf), "|cffffff00 正在修复 entry=%u ...|r", entry);
        handler->SendSysMessage(buf);
        handler->SendSysMessage("|cffffff00 症状: creature表有记录但内存无数据|r");

        CleanupBroken(entry);

        handler->SendSysMessage("|cff00ff00 已删除 world.creature 里的孤儿记录|r");
        handler->SendSysMessage("|cffffff00 这样下次启动就不会 ABORT_MSG 崩服了|r");
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
